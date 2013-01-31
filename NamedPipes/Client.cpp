#include "StdAfx.h"
#include "Client.h"

namespace NamedPipe
{
    Client::Client(t_string pipe_name) : Base(pipe_name)
    {
        printf("Client\n");
    }


    Client::~Client(VOID)
    {
        printf("~Client\n");

        Stop_Helper();
    }


    DWORD Client::Start_Helper(VOID)
    {
        eState_ = STATE::starting;

        // Проверка запущен ли сервер: клиент не должен запускаться, если не запущен сервер. ∙ ▼
        hC_Event_Stop_Server_ = CreateEvent ( 
                                                NULL,                                               // default security attribute 
                                                TRUE,                                               // manual-reset event 
                                                FALSE,                                              // initial state = unsignaled 
                                                TEXT(STOP_SERVER_EVENT_GUID)                        // named event object 
                                            );
        DWORD create_event_result = GetLastError();

        if ((hC_Event_Stop_Server_ == NULL) || (create_event_result != ERROR_ALREADY_EXISTS))
        {  
            return ERROR_NOT_FOUND;
        }
        // ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ▲

        // Инициализация pipe-a. ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ▼
        hPipe_ = CreateFile ( 
                                PipeName_.c_str(),            // pipe name 
                                GENERIC_READ |             // read and write access 
                                GENERIC_WRITE, 
                                0,                // no sharing 
                                NULL,               // default security attributes
                                OPEN_EXISTING,             // opens existing pipe 
                                0,                // default attributes 
                                NULL               // no template file 
                            ); 

        if (hPipe_ == INVALID_HANDLE_VALUE) 
        {
            //return GetLastError();
            DWORD last_error = GetLastError();
        
            if (last_error == ERROR_PIPE_BUSY)
            {
                WaitNamedPipe(PipeName_.c_str(), NMPWAIT_WAIT_FOREVER);


                   hPipe_ = CreateFile ( 
                                            PipeName_.c_str(),                                      // pipe name 
                                            GENERIC_READ |                                          // read and write access 
                                            GENERIC_WRITE, 
                                            0,                                                      // no sharing 
                                            NULL,                                                   // default security attributes
                                            OPEN_EXISTING,                                          // opens existing pipe 
                                            0,                                                      // default attributes 
                                            NULL                                                    // no template file 
                                        ); 
            }
        }

        // The pipe connected; change to message-read mode. 
        DWORD dwMode = PIPE_READMODE_MESSAGE; 
        BOOL bSuccess = SetNamedPipeHandleState ( 
                                                    hPipe_,                                         // pipe handle 
                                                    &dwMode,                                        // new pipe mode 
                                                    NULL,                                           // don't set maximum bytes 
                                                    NULL                                            // don't set maximum time 
                                                );
        if (!bSuccess) 
        {
            return GetLastError();
        } 
        // ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ▲

        // Старт потока(ов). ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ▼
        if (DWORD start_thread_result = StartMainThread() != ERROR_SUCCESS)    
            return start_thread_result;    
        // ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ∙ ▲

        InitializeCriticalSection(&csPipe_Write_);
        InitializeCriticalSection(&csPipe_Read_);

        eState_ = STATE::started;

        return ERROR_SUCCESS;
    }


    // Мониторинг работы сервера: при отключении сервера производит остановку клиента.
    VOID Client::MainThread_Helper(VOID)
    {
        UINT_PTR  nTimerId = SetTimer(NULL, NULL, INTERVAL_WATCHING_MSEC, NULL); 

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (WatchOverServer())
                break;
        }

        KillTimer(NULL, nTimerId);

        // Создаем дополнительный поток, в котором выполнятся действия по остановке.
        DWORD nStoppingThreadId_; 
                
        CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)StoppingThread, this, 0, &nStoppingThreadId_);  
        // StoppingThread -> Stop_Helper 
    }


    BOOL Client::WatchOverServer(VOID)
    {
        BOOL watch_over_server_result = false;

        HANDLE watch_over_server_mutex_handle = CreateMutexA(NULL, FALSE,  SERVER_IS_WORKING_MUTEX_GUID);  
        if (( watch_over_server_mutex_handle != NULL ) && (GetLastError() != ERROR_ALREADY_EXISTS))
        { 
            watch_over_server_result = true;
        }

        CLOSE_HANDLE(watch_over_server_mutex_handle);

        return watch_over_server_result;
    }


    VOID Client::Stop_Helper(VOID)
    {        
        printf("Stop_Helper: Client\n");        

        eState_ = STATE::stopping;
    
        StopMainThread();

        CLOSE_HANDLE(hPipe_);
        CLOSE_HANDLE(hC_Event_Stop_Server_); 

        DeleteCriticalSection(&csPipe_Write_);
        DeleteCriticalSection(&csPipe_Read_);

        eState_ = STATE::stopped;
    }


    VOID Client::StopMainThread(VOID)
    {
        if (hMainThread_ != NULL)
        {
            do {

                DWORD res = PostThreadMessage(nMainThreadId_,  WM_QUIT, 0, 0); 
                DWORD error = GetLastError();  

            } while (WAIT_TIMEOUT == WaitForSingleObject(hMainThread_,  0));
            // В WaitForSingleObject не используем INFINITE, т.к. возможен вариант:
            // "Maybe the thread doesn't have a message queue.
            // A thread can create a message queue for itself
            // by calling GetMessage or PeekMessage, for example."
            // TODO: Решить.

            CLOSE_HANDLE(hMainThread_);
            nMainThreadId_ = NULL;
        }
    }


    DWORD Client::Read(t_string & read_buffer)
    {
        if (eState_ != STATE::started)
        {
            return ERROR_NOT_FOUND;
        }

        EnterCriticalSection(&csPipe_Read_);

            DWORD cbRead;
            read_buffer = TEXT("");
            BOOL bSuccess;

            do 
            {
                bSuccess = ReadFile    ( 
                                            hPipe_,                                                 // pipe handle 
                                            chReply,                                                // buffer to receive reply 
                                            BUFSIZE*sizeof(TCHAR),                                  // size of buffer 
                                            &cbRead,                                                // number of bytes read 
                                            NULL                                                    // not overlapped 
                                        );

                read_buffer += chReply;

                if (!bSuccess && (GetLastError() != ERROR_MORE_DATA))
                    break; 

            } while ( ! bSuccess);  // repeat loop if ERROR_MORE_DATA 

        LeaveCriticalSection(&csPipe_Read_);

        return bSuccess ? ERROR_SUCCESS : GetLastError();
    }


    DWORD Client::Write(t_string write_buffer)
    {
        if (eState_ != STATE::started)
        {
            return ERROR_NOT_FOUND;
        }

        DWORD cbWritten;

        EnterCriticalSection(&csPipe_Write_);

            BOOL bSuccess = WriteFile ( 
                                            hPipe_,                                                 // pipe handle 
                                            write_buffer.c_str(),                                   // message 
                                            write_buffer.length() * 
                                            sizeof(t_string::traits_type::char_type),               // message length 
                                            &cbWritten,                                             // bytes written 
                                            NULL                                                    // not overlapped 
                                        );

        LeaveCriticalSection(&csPipe_Write_);

        return bSuccess ? ERROR_SUCCESS : GetLastError();
    }
}
