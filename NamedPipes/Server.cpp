#include "StdAfx.h"
#include "Server.h"

namespace NamedPipe
{
    Server::Server(t_string pipe_name) : Base(pipe_name)
    {   
        printf("Server\n");
    }


    Server::~Server(VOID)
    {
        printf("~Server\n");

        Stop_Helper();
    }


    DWORD Server::Start_Helper(VOID)
    {
        // Предотвращение повторного запуска.
        hServerIsUp_Mutex = CreateMutexA(NULL, FALSE,  SERVER_IS_WORKING_MUTEX_GUID);
        if ((hServerIsUp_Mutex == NULL) || (GetLastError() == ERROR_ALREADY_EXISTS))
        {
            CLOSE_HANDLE(hServerIsUp_Mutex);

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
            CREATE_EVENT__SIGNALED(hPipes_Events_[i], t_string(TEXT("!!!!!_") + t_to_string(i)).c_str());

            if (hPipes_Events_[i] == NULL) 
            {     
                return GetLastError();   
            }

            Pipe_[i].oOverlap.hEvent = hPipes_Events_[i]; 

            Pipe_[i].hPipeInst = CreateNamedPipe(             
                                                    PipeName_.c_str(),                              // pipe name 
                                                    PIPE_ACCESS_DUPLEX |                            // read/write access 
                                                    FILE_FLAG_OVERLAPPED,                           // overlapped mode 
                                                    PIPE_TYPE_MESSAGE |                             // message-type pipe 
                                                    PIPE_READMODE_MESSAGE |                         // message-read mode 
                                                    PIPE_WAIT,                                      // blocking mode 
                                                    S_PIPES_INSTANCES,                              // number of S_PIPES_INSTANCES 
                                                    BUFSIZE*sizeof(TCHAR),                          // output buffer size 
                                                    BUFSIZE*sizeof(TCHAR),                          // input buffer size 
                                                    S_PIPE_TIMEOUT,                                 // client time-out 
                                                    NULL                                            // default security attributes 
                                                );

            if (Pipe_[i].hPipeInst == INVALID_HANDLE_VALUE) 
            {
                return GetLastError();
            }

            // Call the subroutine to connect to the new client            
            if (DWORD connect_to_new_client_result = ConnectToNewClient(i) != ERROR_SUCCESS) 
            {
                return connect_to_new_client_result;
            }  
        }

        // stop-event
        CREATE_EVENT__UNSIGNALED(hPipes_Events_[S_EVENT_STOP], TEXT(""));
        // stop-server-event
        CREATE_EVENT__UNSIGNALED(hPipes_Events_[S_EVENT_STOP_SERVER], TEXT(STOP_SERVER_EVENT_GUID));

        for (DWORD counter = 0; counter < S_EVENTS_; counter++ )
        {
            CREATE_EVENT__UNSIGNALED(hS_Events_[counter], TEXT(""));
        }

        //TODO_URGENT: инициализировать S_EVENTS_

        InitializeCriticalSection(&csReadPipe_);
        for (DWORD counter = 0; counter < S_PIPES_INSTANCES; counter++)
        {        
            InitializeCriticalSection(&Pipe_[counter].csWritePipe);
        }    

        StartMainThread();

        eState_ = STATE::started;

        return ERROR_SUCCESS;
    }


    DWORD Server::ConnectToNewClient(DWORD pipe_number) 
    {
        // Start an overlapped connection for this pipe instance. 
        BOOL fConnected = ConnectNamedPipe(Pipe_[pipe_number].hPipeInst, &Pipe_[pipe_number].oOverlap); 

        // Overlapped ConnectNamedPipe should return zero. 
        if (fConnected) 
        {  
            Pipe_[pipe_number].dwState = STATE::errored;

            return GetLastError();
        }

        switch (GetLastError()) 
        { 
                // The overlapped connection in progress. 
            case ERROR_IO_PENDING: 
                Pipe_[pipe_number].dwState = STATE::connecting; 
                break; 

                // Client is already connected, so signal an event. 
            case ERROR_PIPE_CONNECTED: 
                Pipe_[pipe_number].dwState = STATE::connected; 
                if (SetEvent(&Pipe_[pipe_number].oOverlap.hEvent)) 
                    break; 

                // If an error occurs during the connect operation... 
            default: 
                {
                    Pipe_[pipe_number].dwState = STATE::errored;
                    return GetLastError();
                }
        }  

        return ERROR_SUCCESS;
    }


    VOID Server::DisconnectAndReconnect(DWORD pipe_number)
    { 
        // Disconnect the pipe instance.
        if (! DisconnectNamedPipe(Pipe_[pipe_number].hPipeInst)) 
        {
            // TODO: Убрать (заменить) логирование.
            printf("DisconnectNamedPipe failed with %d.\n", GetLastError());

            Pipe_[pipe_number].dwState = STATE::errored;
        }

        Pipe_[pipe_number].dwState = STATE::undefined;

        // Call a subroutine to connect to the new client. 
        ConnectToNewClient(pipe_number);
    }


    VOID Server::MainThread_Helper(VOID)
    {
        DWORD dwPipeIndex;
        DWORD dwWait;
        DWORD dwRet;
        BOOL bSuccess;

        while (1) 
        { 
            // Wait for the event object to be signaled, indicating 
            // completion of an overlapped read, write, or 
            // connect operation. 

            _tprintf(TEXT("ReadThread_Helper: Before WaitForMultipleObjects\n"));  

            dwWait = WaitForMultipleObjects ( 
                                                S_PIPES_EVENTS,                                     // number of event objects 
                                                hPipes_Events_,                                     // array of event objects 
                                                FALSE,                                              // does not wait for all 
                                                INFINITE                                            // waits indefinitely 
                                            );    

            // TODO: Убрать (заменить) логирование.
            _tprintf(TEXT("ReadThread_Helper: After WaitForMultipleObjects\n"));  

            dwPipeIndex = dwWait - WAIT_OBJECT_0;                                                   // determines which pipe 
            if (dwPipeIndex < 0 || dwPipeIndex > (S_PIPES_EVENTS - 2))                              // (S_PIPES_EVENTS - 2)б т.к. последний евент - указание завершить рботу.
            {
                // TODO: Убрать (заменить) логирование.
                _tprintf(TEXT("ReadThread_Helper: Index out of range.\n")); 

                //TODO_URGENT: возвращать значение. Кому???
                //return ERROR_INVALID_INDEX;

                return;
            }             

            ResetEvent(hPipes_Events_[dwPipeIndex]);

            if (Pipe_[dwPipeIndex].dwState == STATE::connected)
            {
                bSuccess = GetOverlappedResult  ( 
                                                    Pipe_[dwPipeIndex].hPipeInst,                   // handle to pipe 
                                                    &Pipe_[dwPipeIndex].oOverlap,                   // OVERLAPPED structure 
                                                    &dwRet,                                         // bytes transferred 
                                                    FALSE                                           // do not wait 
                                                );
                if (bSuccess) 
                {
                    Pipe_[dwPipeIndex].cbRead = dwRet;

                    // TODO: Если размер S_GlobalReadBuffer_ достиг максимума,
                    // ждем операцию чтения, в буфер не читаем.

                    // Сохранить прочитанное (если оно есть) и продолжить чтение.
                    if (Pipe_[dwPipeIndex].cbRead != 0)
                    {     
                        EnterCriticalSection(&csReadPipe_);

                        GlobalReadBuffer_.push(std::pair<DWORD, t_string>(dwPipeIndex, Pipe_[dwPipeIndex].chRequest));

                        _tprintf(TEXT("\nAfter PUSH:  Buffer: #%5d %s\n"), GlobalReadBuffer_.front().first, GlobalReadBuffer_.front().second.c_str());
                        _tprintf(TEXT(  "After PUSH:  Pipe:   #%5d %s\n"), dwPipeIndex, ((t_string)Pipe_[dwPipeIndex].chRequest).c_str());

                        //if (S_GlobalReadBuffer_.front().second != Pipe_[dwPipeIndex].chRequest)
                        //{
                        //    Sleep(1);
                        //}

                        // Сигнализируем о том, что произошло чтение.
                        SetEvent(hS_Events_[S_EVENT_READING_FINISHED]);

                        LeaveCriticalSection(&csReadPipe_);
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
                bSuccess = GetOverlappedResult  ( 
                                                    Pipe_[dwPipeIndex].hPipeInst,                   // handle to pipe 
                                                    &Pipe_[dwPipeIndex].oOverlap,                   // OVERLAPPED structure 
                                                    &dwRet,                                         // bytes transferred 
                                                    FALSE                                           // do not wait 
                                                );
                if (!bSuccess)
                {
                    Sleep(1);
                }


                Pipe_[dwPipeIndex].dwState = STATE::connected;
            }

            bSuccess = ReadFile (
                                    Pipe_[dwPipeIndex].hPipeInst,                                   // pipe handle 
                                    Pipe_[dwPipeIndex].chRequest,                                   // buffer to receive reply
                                    BUFSIZE*sizeof(TCHAR),                                          // size of buffer 
                                    &Pipe_[dwPipeIndex].cbRead,                                     // number of bytes read 
                                    &Pipe_[dwPipeIndex].oOverlap                                    // overlapped
                                );  
        }
    }


    VOID Server::Stop_Helper(VOID)
    {        
        printf("Stop_Helper: Server\n");

        eState_ = STATE::stopping;

        StopMainThread();

        for (DWORD counter = 0; counter < S_PIPES_INSTANCES; counter++ ) 
        {
            DisconnectNamedPipe(Pipe_[counter].hPipeInst);

            CLOSE_HANDLE(Pipe_[counter].hPipeInst);
        }

        for (DWORD counter = 0; counter < S_PIPES_EVENTS; counter++ )
        {
            CLOSE_HANDLE(hPipes_Events_[counter]);
        }

        for (DWORD counter = 0; counter < S_EVENTS_; counter++ )
        {
            CLOSE_HANDLE(hS_Events_[counter]); 
        }

        CLOSE_HANDLE(hServerIsUp_Mutex);

        DeleteCriticalSection(&csReadPipe_);
        for (DWORD counter = 0; counter < S_PIPES_INSTANCES; counter++)
        {        
            DeleteCriticalSection(&Pipe_[counter].csWritePipe);
        }  

        eState_ = STATE::stopped;
    }


    VOID Server::StopMainThread()
    {
        if (hMainThread_ != NULL)
        {
            SetEvent(hPipes_Events_[S_PIPES_EVENTS - 1]);    
            WaitForSingleObject(hMainThread_,  INFINITE);
    
            CLOSE_HANDLE(hMainThread_);            
            nMainThreadId_ = NULL;
        }
    }


    DWORD Server::Read(t_string & read_buffer, DWORD & pipe_instance)
    {
        if (eState_ != STATE::started)
        {
            return ERROR_NOT_FOUND;
        }

        EnterCriticalSection(&csReadPipe_);

            // Если буфер пустой, будем ждать чтение пайпа.
            if (GlobalReadBuffer_.empty())
            { 
                ResetEvent(hS_Events_[S_EVENT_READING_FINISHED]);   
            }

        LeaveCriticalSection(&csReadPipe_);

        // Ждем окончание чтения пайпа.
        WaitForSingleObject(hS_Events_[S_EVENT_READING_FINISHED], INFINITE);  

        EnterCriticalSection(&csReadPipe_);

            //if (S_GlobalReadBuffer_.size() > 1)
            //{
            //    Sleep(1);
            //}

            std::pair<DWORD, t_string> pipe_number_and_buffer = GlobalReadBuffer_.front();
            GlobalReadBuffer_.pop();

            pipe_instance = pipe_number_and_buffer.first;
            read_buffer = pipe_number_and_buffer.second;

        LeaveCriticalSection(&csReadPipe_); 

        return ERROR_SUCCESS; // TODO: Вернуть корректное значение.
    }


    DWORD Server::Write(t_string write_buffer, DWORD pipe_instance)
    {   
        return Base::Write(write_buffer, Pipe_[pipe_instance].hPipeInst, Pipe_[pipe_instance].csWritePipe);        
    }
}
