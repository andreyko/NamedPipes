#pragma once
#include "base.h"

#define INTERVAL_WATCHING_MSEC                  1000

namespace NamedPipe
{
    class Client : public Base
    {
       
    private:
                HANDLE                          hPipe_;            
                CRITICAL_SECTION                csPipe_Read_;
                CRITICAL_SECTION                csPipe_Write_;
                HANDLE                          hC_Event_Stop_Server_;
                TCHAR                           chReply[BUFSIZE];

                BOOL                            WatchOverServer(VOID);
                VOID                            StopMainThread(VOID);
        static  VOID                            StoppingThread(VOID *arg){ ((Client*)arg)->Stop_Helper();};
        virtual DWORD                           Start_Helper(VOID);
        virtual VOID                            Stop_Helper(VOID);
        virtual VOID                            MainThread_Helper(VOID);                
        
    public:
                                                Client(t_string pipe_name);
                                                ~Client(VOID);

                DWORD                           Read(t_string & read_buffer);
                DWORD                           Write(t_string write_buffer);        
    };
}