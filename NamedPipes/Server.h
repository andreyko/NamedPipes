#pragma once
#include "base.h"

#define S_PIPES_INSTANCES                       4 
#define S_PIPES_EVENTS                          S_PIPES_INSTANCES + 2        /* +1 - евент для стопа +1 - евент для стоп-сервера*/

#define S_EVENT_STOP                            S_PIPES_INSTANCES + 0
#define S_EVENT_STOP_SERVER                     S_PIPES_INSTANCES + 1

#define S_EVENTS_                               2
#define S_EVENT_READING_FINISHED                0
#define S_EVENT_WRITING_FINISHED                1

#define S_PIPE_TIMEOUT                          5000

#define CREATE_EVENT__SIGNALED(hEvent, szName)  hEvent      =   CreateEvent        ( NULL, TRUE, TRUE, szName  );
                                                /* default security attribute -------^     ^     ^      ^      */
                                                /* manual-reset event ---------------------^     ^      ^      */
                                                /* initial state = signaled ---------------------^      ^      */
                                                /* unnamed event object --------------------------------^      */

#define CREATE_EVENT__UNSIGNALED(hEvent, szName)hEvent      =   CreateEvent        ( NULL, TRUE, FALSE, szName );
                                                /* default security attribute -------^     ^     ^      ^      */
                                                /* manual-reset event ---------------------^     ^      ^      */
                                                /* initial state = unsignaled -------------------^      ^      */
                                                /* unnamed event object --------------------------------^      */

namespace NamedPipe
{
    class Server : public Base
    {
        
    private:   

        typedef struct 
            { 
                OVERLAPPED                      oOverlap; 
                HANDLE                          hPipeInst;         
                TCHAR                           chRequest[BUFSIZE]; 
                DWORD                           cbRead;        
                DWORD                           dwState;         
                CRITICAL_SECTION                csWritePipe;
            }   PIPEINST, *LPPIPEINST;

                

                PIPEINST                        Pipe_[S_PIPES_INSTANCES];
                HANDLE                          hServerIsUp_Mutex;
                HANDLE                          hPipes_Events_[S_PIPES_EVENTS];
                std::queue
                <std::pair<DWORD, t_string>>    GlobalReadBuffer_;
                CRITICAL_SECTION                csReadPipe_;
                HANDLE                          hS_Events_[S_EVENTS_];

                VOID                            StopMainThread();
                DWORD                           ConnectToNewClient(DWORD pipe_number);
                VOID                            DisconnectAndReconnect(DWORD dwPipeIndex);
        virtual DWORD                           Start_Helper(VOID);
        virtual VOID                            Stop_Helper(VOID);
        virtual VOID                            MainThread_Helper(VOID);
        
    public:
                                                Server(t_string pipe_name);
                                                ~Server(void);

                DWORD                           Read(t_string & read_buffer, DWORD & pipe_instance);
                DWORD                           Write(t_string write_buffer, DWORD pipe_instance);
    };
}

