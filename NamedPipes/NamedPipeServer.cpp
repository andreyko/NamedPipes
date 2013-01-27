#include "StdAfx.h"
#include "NamedPipeServer.h"

#include <strsafe.h>	// StringCchCopy в GetAnswerToRequest


NamedPipeServer::NamedPipeServer(t_string pipe_name)
{
	PipeName_ = pipe_name;
}


NamedPipeServer::~NamedPipeServer(VOID)
{
	Stop_Helper();
}


DWORD NamedPipeServer::Start()
{
	if (eS_State_ == STATE::started)
	{
		return ERROR_ALREADY_EXISTS;
	}

	// Сервер может запускается в одном экземпляре.
	DWORD error = Start_Helper();
	if (error == ERROR_SUCCESS)
	{		
		StartMainThread();
		StartReadThread();
	}
	else
	{
		Stop_Helper();
		return error;
	}
	
	eS_State_ = STATE::started;

	return ERROR_SUCCESS;	
}


VOID NamedPipeServer::StartMainThread()
{
	hS_MainThread_ = CreateThread  (NULL, 0, (LPTHREAD_START_ROUTINE)MainThread, this, 0, &nS_MainThreadId_);
	// MainThread -> MainThread_Helper
}


VOID NamedPipeServer::StopMainThread()
{
}


DWORD NamedPipeServer::Start_Helper()
{
	// Предотвращение повторного запуска.
	HANDLE hS_ServerIsUp_Mutex = CreateMutexA(NULL, FALSE,  SERVER_IS_WORKING_MUTEX_GUID);
	if ((hS_ServerIsUp_Mutex == NULL) || (GetLastError() == ERROR_ALREADY_EXISTS))
	{
		CloseHandle(hS_ServerIsUp_Mutex);
		hS_ServerIsUp_Mutex = NULL;
		return ERROR_ALREADY_EXISTS;
	}

	DWORD i;

	// The initial loop creates several S_PIPES_INSTANCES of a named pipe 
	// along with an event object for each instance.  An 
	// overlapped ConnectNamedPipe operation is started for 
	// each instance. 

	for (i = 0; i < S_PIPES_INSTANCES; i++) 
	{ 

		// Create an event object for this instance. 

		hS_Pipes_Events_[i] = CreateEvent( 
														NULL,									// default security attribute 
														TRUE,									// manual-reset event 
														TRUE,									// initial state = signaled 
														NULL									// unnamed event object 
													);

		if (hS_Pipes_Events_[i] == NULL) 
		{					
			return GetLastError();			
		}

		Pipe_[i].oOverlap.hEvent = hS_Pipes_Events_[i]; 

		Pipe_[i].hPipeInst = CreateNamedPipe( 												
												PipeName_.c_str(),				// pipe name 
												PIPE_ACCESS_DUPLEX |			// read/write access 
												FILE_FLAG_OVERLAPPED,			// overlapped mode 
												PIPE_TYPE_MESSAGE |				// message-type pipe 
												PIPE_READMODE_MESSAGE |			// message-read mode 
												PIPE_WAIT,						// blocking mode 
												S_PIPES_INSTANCES,				// number of S_PIPES_INSTANCES 
												S_BUFSIZE*sizeof(TCHAR),		// output buffer size 
												S_BUFSIZE*sizeof(TCHAR),		// input buffer size 
												S_PIPE_TIMEOUT,					// client time-out 
												NULL							// default security attributes 
											);

		if (Pipe_[i].hPipeInst == INVALID_HANDLE_VALUE) 
		{
			return GetLastError();
		}

		// Call the subroutine to connect to the new client
		DWORD connect_to_new_client_result = ConnectToNewClient(i);

		if (connect_to_new_client_result != ERROR_SUCCESS) 
		{
			connect_to_new_client_result;
		}		
	}

	hS_Pipes_Events_[S_PIPES_EVENTS -1] = CreateEvent( 
														NULL,									// default security attribute 
														TRUE,									// manual-reset event 
														FALSE,									// initial state = signaled 
														NULL									// unnamed event object 
													);

	for (i = 0; i < S_MANAGING_EVENTS; i++) 
	{	
		if (i != S_EVENT_STOP_SERVER)
		{
			hS_Managing_Events_[i] = CreateEvent	( 
														NULL,							// default security attribute 
														TRUE,							// manual-reset event 
														FALSE,							// initial state = unsignaled 
														NULL							// unnamed event object 
													);
		}
		else
		{
			hS_Managing_Events_[i] = CreateEvent	( 
														NULL,							// default security attribute 
														TRUE,							// manual-reset event 
														FALSE,							// initial state = unsignaled 
														TEXT(STOP_SERVER_EVENT_GUID)	// named event object 
													);
		}
	}	

	for (DWORD counter = 0; counter < S_EVENTS_; counter++ )
	{

		hS_Events_[counter] = CreateEvent	( 
												NULL,							// default security attribute 
												TRUE,							// manual-reset event 
												FALSE,							// initial state = unsignaled 
												NULL							// unnamed event object 
											);
	}

	//TODO_URGENT: инициализировать S_EVENTS_

	InitializeCriticalSection(&csPipe_);
	
	return ERROR_SUCCESS;
}


VOID NamedPipeServer::Stop(VOID)
{
	
	SetEvent(hS_Managing_Events_[S_EVENT_STOP]);
	SetEvent(hS_Pipes_Events_[S_PIPES_EVENTS - 1]);
	
	WaitForSingleObject(hS_MainThread_,  INFINITE);
	WaitForSingleObject(hS_ReadThread_,  INFINITE);
	

	Stop_Helper();
}


VOID NamedPipeServer::Stop_Helper()
{
	for (DWORD counter = 0; counter < S_PIPES_INSTANCES; counter++ )	
	{
		if (Pipe_[counter].hPipeInst != NULL)
		{
			CloseHandle(Pipe_[counter].hPipeInst);	
			Pipe_[counter].hPipeInst = NULL;
		}
	}

	for (DWORD counter = 0; counter < S_PIPES_EVENTS; counter++ )
	{
		if (hS_Pipes_Events_[counter] != NULL)
		{
			CloseHandle(hS_Pipes_Events_[counter]);	
			hS_Pipes_Events_[counter] = NULL;
		}
	}

	for (DWORD counter = 0; counter < S_MANAGING_EVENTS; counter++ )
	{
		if (hS_Managing_Events_[counter] != NULL)
		{
			CloseHandle(hS_Managing_Events_[counter]);	
			hS_Managing_Events_[counter] = NULL;
		}
	}

	for (DWORD counter = 0; counter < S_EVENTS_; counter++ )
	{
		if (hS_Events_[counter] != NULL)
		{
			CloseHandle(hS_Events_[counter]);	
			hS_Events_[counter] = NULL;
		}
	}

	if (hS_MainThread_ != NULL)
	{
		CloseHandle(hS_MainThread_);
		hS_MainThread_ = NULL;
	}

	if (hS_ReadThread_ != NULL)
	{
		CloseHandle(hS_ReadThread_);
		hS_ReadThread_ = NULL;
	}

	if (hS_ServerIsUp_Mutex != NULL)
	{
		CloseHandle(hS_ServerIsUp_Mutex);
		hS_ServerIsUp_Mutex = NULL;
	}

	DeleteCriticalSection(&csPipe_);
}


DWORD NamedPipeServer::Read(DWORD & pipe_number, t_string & read_buffer)
{
	if (eS_State_ != STATE::started)
	{
		return ERROR_NOT_FOUND;
	}

	EnterCriticalSection(&csPipe_);

		// Если буфер пустой, будем ждать чтение пайпа.
		if (S_GlobalReadBuffer_.empty())
		{	
			ResetEvent(hS_Events_[S_EVENT_READING_FINISHED]);			
		}

	LeaveCriticalSection(&csPipe_);
	
	// Ждем окончание чтения пайпа.
	WaitForSingleObject(hS_Events_[S_EVENT_READING_FINISHED], INFINITE);		
	
	EnterCriticalSection(&csPipe_);

		if (S_GlobalReadBuffer_.size() > 1)
		{
			Sleep(1);
		}

		std::pair<DWORD, t_string> pipe_number_and_buffer = S_GlobalReadBuffer_.front();
		S_GlobalReadBuffer_.pop();

		pipe_number = pipe_number_and_buffer.first;
		read_buffer = pipe_number_and_buffer.second;

	LeaveCriticalSection(&csPipe_);	
		
	return ERROR_SUCCESS; // TODO: Вернуть корректное значение.
}


DWORD NamedPipeServer::Read_Helper(VOID)
{
	_tprintf(TEXT("Read_Helper\n"));
	
	return ERROR_SUCCESS;
}


VOID NamedPipeServer::StartReadThread()
{
	hS_ReadThread_ = CreateThread  (NULL, 0, (LPTHREAD_START_ROUTINE)ReadThread, this, 0, &nS_ReadThreadId_);
	// ReadThread -> ReadThread_Helper
}

					  
VOID NamedPipeServer::ReadThread_Helper()
{
	DWORD	dwPipeIndex;
	DWORD	dwWait;
	DWORD	dwErr;
	DWORD	dwRet;
	BOOL	bSuccess;

	while (1) 
	{ 
		// Wait for the event object to be signaled, indicating 
		// completion of an overlapped read, write, or 
		// connect operation. 

		_tprintf(TEXT("ReadThread_Helper: Before WaitForMultipleObjects\n"));		

		dwWait = WaitForMultipleObjects	( 
											S_PIPES_EVENTS,									// number of event objects 
											hS_Pipes_Events_,								// array of event objects 
											FALSE,											// does not wait for all 
											INFINITE										// waits indefinitely 
										); 			

		// TODO: Убрать (заменить) логирование.
		_tprintf(TEXT("ReadThread_Helper: After WaitForMultipleObjects\n"));		

		dwPipeIndex = dwWait - WAIT_OBJECT_0;  // determines which pipe 
		if (dwPipeIndex < 0 || dwPipeIndex > (S_PIPES_EVENTS - 2))			// (S_PIPES_EVENTS - 2)б т.к. последний евент - указание завершить рботу.
		{
			// TODO: Убрать (заменить) логирование.
			_tprintf(TEXT("ReadThread_Helper: Index out of range.\n")); 

			//TODO_URGENT: возвращать значение. Кому???
			//return ERROR_INVALID_INDEX;

			return;
		}		
		
		ResetEvent(hS_Pipes_Events_[dwPipeIndex]);

		if (Pipe_[dwPipeIndex].dwState == S_STATE_CONNECTED)
		{
			bSuccess = GetOverlappedResult	( 
												Pipe_[dwPipeIndex].hPipeInst,	// handle to pipe 
												&Pipe_[dwPipeIndex].oOverlap,	// OVERLAPPED structure 
												&dwRet,							// bytes transferred 
												FALSE							// do not wait 
											);
			if (bSuccess) 
			{
				Pipe_[dwPipeIndex].cbRead = dwRet;

				// TODO: Если размер S_GlobalReadBuffer_ достиг максимума,
				// ждем операцию чтения, в буфер не читаем.

				// Сохранить прочитанное (если оно есть) и продолжить чтение.
				if (Pipe_[dwPipeIndex].cbRead != 0)
				{					
					EnterCriticalSection(&csPipe_);

						S_GlobalReadBuffer_.push(std::pair<DWORD, t_string>(dwPipeIndex, Pipe_[dwPipeIndex].chRequest));

						//_tprintf(TEXT("\nAfter PUSH:  Buffer: #%5d %s\n"), S_GlobalReadBuffer_.front().first, S_GlobalReadBuffer_.front().second.c_str());
						//_tprintf(TEXT(  "After PUSH:  Pipe:   #%5d %s\n"), dwPipeIndex, ((t_string)Pipe_[dwPipeIndex].chRequest).c_str());

						if (S_GlobalReadBuffer_.front().second != Pipe_[dwPipeIndex].chRequest)
						{
							Sleep(1);
						}

						// Сигнализируем о том, что роизошло чтение.
						SetEvent(hS_Events_[S_EVENT_READING_FINISHED]);

					LeaveCriticalSection(&csPipe_);


				}
				else
				{
					// TODO: Обработать случай, если cbRead == 0;
					printf("cbRead = 0\n");
				}
			}
			else
			{
				//// TODO: Убрать (заменить) логирование.				
				//_tprintf(TEXT("ReadThread_Helper: Error %d.\n"), GetLastError()); 
				//return;
				
				// TODO: Убрать (заменить) логирование.
				_tprintf(TEXT("ReadThread_Helper: Error %d.\n"), GetLastError()); 
				DisconnectAndReconnect(dwPipeIndex);

				continue;
			}
		}
		else
		{
			Pipe_[dwPipeIndex].dwState = S_STATE_CONNECTED;
		}

		bSuccess = ReadFile	(
								Pipe_[dwPipeIndex].hPipeInst,						// pipe handle 
								Pipe_[dwPipeIndex].chRequest,						// buffer to receive reply
								S_BUFSIZE*sizeof(TCHAR),							// size of buffer 
								&Pipe_[dwPipeIndex].cbRead,							// number of bytes read 
								&Pipe_[dwPipeIndex].oOverlap						// overlapped
							);		
	}
}

DWORD NamedPipeServer::Write(DWORD write_to_pipe, t_string write_buffer)
{
	if (eS_State_ != STATE::started)
	{
		return ERROR_NOT_FOUND;
	}

	if (Pipe_[write_to_pipe].dwState != S_STATE_CONNECTED)
	{
		return ERROR_PIPE_NOT_CONNECTED;
	}

	// Ожидание окончания операции, если она происходит в параллельном потоке.
	WaitForSingleObject(hS_Events_[S_EVENT_WRITING_FINISHED],  INFINITE);	
	
	// Блокировка повторных вызовов "Write".
	ResetEvent(hS_Events_[S_EVENT_WRITING_FINISHED]);
		
	// Копировние номера и буфера.
	nWriteToPipe_ = write_to_pipe;
	S_WriteBuffer_ = write_buffer;

	// Старт операции для MainThread_Helper (сброс event-а происходит в MainThread_Helper).
	SetEvent(hS_Managing_Events_[S_EVENT_START_WRITING]);

	// Значение nWriteResult_ определяется в MainThread_Helper.
	DWORD write_result = nWriteResult_;

	// Ожидание окончания текущей операции записи (блокирующий режим).
	WaitForSingleObject(hS_Events_[S_EVENT_WRITING_FINISHED],  INFINITE);

	return write_result;
}


DWORD NamedPipeServer::Write_Helper(VOID)
{
	_tprintf(TEXT("Write_Helper\n"));

	DWORD cbWritten;

	BOOL bSuccess = WriteFile	( 
									Pipe_[nWriteToPipe_].hPipeInst,							// pipe handle 
									S_WriteBuffer_.c_str(),									// message 
									S_WriteBuffer_.length() * 
												sizeof(t_string::traits_type::char_type),	// message length 
									&cbWritten,												// bytes written 
									NULL													// not overlapped 
								);

   if (!bSuccess) 
   {      
      return GetLastError();
   }
   else
   {
	   return ERROR_SUCCESS;
   }
}


VOID NamedPipeServer::MainThread_Helper()
{
	DWORD	dwPipeIndex;
	DWORD	dwWait;
	DWORD	dwErr;
	DWORD	dwRet;
	BOOL	bSuccess;

	while (1) 
	{ 
		// Wait for the event object to be signaled, indicating 
		// completion of an overlapped read, write, or 
		// connect operation. 

		_tprintf(TEXT("MainThread_Helper: Before WaitForMultipleObjects\n"));

		dwWait = WaitForMultipleObjects	( 
											S_MANAGING_EVENTS,		// number of event objects 
											hS_Managing_Events_,	// array of event objects 
											FALSE,					// does not wait for all 
											INFINITE				// waits indefinitely 
										); 			

		// TODO: Убрать (заменить) логирование.
		_tprintf(TEXT("MainThread_Helper: After WaitForMultipleObjects\n"));		

		dwPipeIndex = dwWait - WAIT_OBJECT_0;  // determines which pipe 
		if (dwPipeIndex < 0 || dwPipeIndex > (S_MANAGING_EVENTS - 1)) 
		{
			// TODO: Убрать (заменить) логирование.
			_tprintf(TEXT("Index out of range.\n")); 

			//TODO_URGENT: возвращать значение
			//return ERROR_INVALID_INDEX;
		}
		
		ResetEvent(hS_Managing_Events_[dwPipeIndex]);
				
		switch (dwPipeIndex)
		{
			case S_EVENT_START_READING:
				ResetEvent(hS_Managing_Events_[S_EVENT_START_READING]);
				_tprintf(TEXT("S_EVENT_START_READING\n"));
				continue;
				break;

			case S_EVENT_START_WRITING:
				ResetEvent(hS_Managing_Events_[S_EVENT_START_WRITING]);
				_tprintf(TEXT("S_EVENT_START_WRITING\n"));
				continue;
				break;

			case S_EVENT_STOP:
				ResetEvent(hS_Managing_Events_[S_EVENT_STOP]);
				_tprintf(TEXT("S_EVENT_STOP\n"));
				return;				
		}		

		continue;

		//////////////////////////////////////////////////////////////////////////////////////////////////////////////
		//
		//// Get the result if the operation was pending.
		//if (Pipe_[dwPipeIndex].fPendingIO) 
		//{ 
		//	bSuccess = GetOverlappedResult	( 
		//										Pipe_[dwPipeIndex].hPipeInst,	// handle to pipe 
		//										&Pipe_[dwPipeIndex].oOverlap,	// OVERLAPPED structure 
		//										&dwRet,							// bytes transferred 
		//										FALSE							// do not wait 
		//									);

		//	switch (Pipe_[dwPipeIndex].dwState) 
		//	{ 
		//		// Pending connect operation 
		//		case S_CONNECTING_STATE: 

		//			_tprintf(TEXT("S_CONNECTING_STATE (1)\n"));

		//			if (! bSuccess) 
		//			{
		//				// TODO: Убрать (заменить) логирование.
		//				_tprintf(TEXT("Error %d.\n"), GetLastError()); 
		//				//return 0;
		//			}
		//			Pipe_[dwPipeIndex].dwState = S_READING_STATE; 
		//			break; 

		//		// Pending read operation 
		//		case S_READING_STATE: 

		//			_tprintf(TEXT("S_READING_STATE (1)\n"));

		//			if (! bSuccess || dwRet == 0) 
		//			{ 
		//				DisconnectAndReconnect(dwPipeIndex); 
		//				continue; 
		//			}
		//			Pipe_[dwPipeIndex].cbRead = dwRet;
		//			Pipe_[dwPipeIndex].dwState = S_WRITING_STATE; 
		//			break; 

		//		// Pending write operation 
		//		case S_WRITING_STATE: 

		//			_tprintf(TEXT("S_WRITING_STATE (1)\n"));

		//			if (! bSuccess || dwRet != Pipe_[dwPipeIndex].cbToWrite) 
		//			{ 
		//				DisconnectAndReconnect(dwPipeIndex); 
		//				continue; 
		//			} 
		//			Pipe_[dwPipeIndex].dwState = S_READING_STATE; 
		//			break; 

		//		default: 
		//			{
		//				// TODO: Убрать (заменить) логирование.
		//				_tprintf(TEXT("Invalid pipe state.\n")); 
		//				//return 0;
		//			}
		//	}  
		//} 

		//// The pipe state determines which operation to do next.
		//switch (Pipe_[dwPipeIndex].dwState) 
		//{ 
		//	// S_READING_STATE: 
		//	// The pipe instance is connected to the client 
		//	// and is ready to read a request from the client. 

		//	case S_READING_STATE: 

		//		_tprintf(TEXT("S_READING_STATE (2)\n"));

		//		bSuccess = ReadFile	(
		//								Pipe_[dwPipeIndex].hPipeInst, 
		//								Pipe_[dwPipeIndex].chRequest, 
		//								S_BUFSIZE*sizeof(TCHAR), 
		//								&Pipe_[dwPipeIndex].cbRead, 
		//								&Pipe_[dwPipeIndex].oOverlap
		//							); 

		//		// The read operation completed successfully. 

		//		if (bSuccess && Pipe_[dwPipeIndex].cbRead != 0) 
		//		{ 
		//			Pipe_[dwPipeIndex].fPendingIO = FALSE; 
		//			Pipe_[dwPipeIndex].dwState = S_WRITING_STATE; 
		//			continue; 
		//		} 

		//		// The read operation is still pending. 

		//		dwErr = GetLastError(); 
		//		if (! bSuccess && (dwErr == ERROR_IO_PENDING)) 
		//		{ 
		//			Pipe_[dwPipeIndex].fPendingIO = TRUE;
		//			// TODO_URGENT: при отключении клиента уходит в бесконечный цикл.
		//			ResetEvent(hS_Events_WMPW_[dwPipeIndex]);
		//			continue; 
		//		} 

		//		// An error occurred; disconnect from the client. 

		//		DisconnectAndReconnect(dwPipeIndex); 
		//		break; 

		//	// S_WRITING_STATE: 
		//	// The request was successfully read from the client. 
		//	// Get the reply data and write it to the client. 

		//	case S_WRITING_STATE: 

		//		_tprintf(TEXT("S_WRITING_STATE (2)\n"));

		//		GetAnswerToRequest(&Pipe_[dwPipeIndex]); 

		//		bSuccess = WriteFile( 
		//								Pipe_[dwPipeIndex].hPipeInst, 
		//								Pipe_[dwPipeIndex].chReply, 
		//								Pipe_[dwPipeIndex].cbToWrite, 
		//								&dwRet, 
		//								&Pipe_[dwPipeIndex].oOverlap
		//							); 

		//		// The write operation completed successfully. 

		//		if (bSuccess && dwRet == Pipe_[dwPipeIndex].cbToWrite) 
		//		{ 
		//			Pipe_[dwPipeIndex].fPendingIO = FALSE; 
		//			Pipe_[dwPipeIndex].dwState = S_READING_STATE; 
		//			continue; 
		//		} 

		//		// The write operation is still pending. 

		//		dwErr = GetLastError(); 
		//		if (! bSuccess && (dwErr == ERROR_IO_PENDING)) 
		//		{ 
		//			Pipe_[dwPipeIndex].fPendingIO = TRUE; 
		//			continue; 
		//		} 

		//		// An error occurred; disconnect from the client. 

		//		DisconnectAndReconnect(dwPipeIndex); 
		//		break; 

		//	default: 
		//		{
		//			// TODO: Убрать (заменить) логирование.
		//			_tprintf(TEXT("Invalid pipe state.\n")); 
		//			//return 0;
		//		}
		//} 
	}
}


// ConnectToNewClient(HANDLE, LPOVERLAPPED) 
// This function is called to start an overlapped connect operation. 

DWORD NamedPipeServer::ConnectToNewClient(DWORD pipe_number) 
{
	// Start an overlapped connection for this pipe instance. 
	BOOL fConnected = ConnectNamedPipe(Pipe_[pipe_number].hPipeInst, &Pipe_[pipe_number].oOverlap); 
	
	// Overlapped ConnectNamedPipe should return zero. 
	if (fConnected) 
	{		
		return GetLastError();
	}

	switch (GetLastError()) 
	{ 
		// The overlapped connection in progress. 
		case ERROR_IO_PENDING: 
			Pipe_[pipe_number].dwState = S_STATE_CONNECTING; 
			break; 

		// Client is already connected, so signal an event. 
		case ERROR_PIPE_CONNECTED: 
			Pipe_[pipe_number].dwState = S_STATE_CONNECTED; 
			if (SetEvent(&Pipe_[pipe_number].oOverlap.hEvent)) 
			break; 

		// If an error occurs during the connect operation... 
		default: 
			{
				return GetLastError();
			}
	} 	

	return ERROR_SUCCESS;
}


VOID NamedPipeServer::DisconnectAndReconnect(DWORD dwPipeIndex)
{ 
	// Disconnect the pipe instance. 

	if (! DisconnectNamedPipe(Pipe_[dwPipeIndex].hPipeInst) ) 
	{
		// TODO: Убрать (заменить) логирование.
		printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
	}
	
	// Call a subroutine to connect to the new client. 
	
	ConnectToNewClient(dwPipeIndex);

	//Pipe_[dwPipeIndex].fPendingIO = ConnectToNewClient	( 
	//														Pipe_[dwPipeIndex].hPipeInst,
	//														&Pipe_[dwPipeIndex].oOverlap
	//													); 

	//Pipe_[dwPipeIndex].dwState = Pipe_[dwPipeIndex].fPendingIO ? S_CONNECTING_STATE : // still connecting 
	//															 S_CONNECTED_STATE;   // ready to read/write
} 


VOID NamedPipeServer::GetAnswerToRequest(LPPIPEINST pipe)
{
   _tprintf( TEXT("[%d] %s\n"), pipe->hPipeInst, pipe->chRequest);
   StringCchCopy( pipe->chReply, S_BUFSIZE, TEXT("Default answer from server") );
   pipe->cbToWrite = (lstrlen(pipe->chReply)+1)*sizeof(TCHAR);
}