#include <windows.h>
#include <string>
#include <queue>

#pragma once

#define STOP_SERVER_EVENT_GUID                  "Global\\NP-3EAACE24-DBD1-4F64-A32A-522BDD2EE2C1"
#define SERVER_IS_WORKING_MUTEX_GUID            "Global\\NP-683FBDF8-B363-4746-A0A3-224CA657072E"

#define BUFSIZE                                 16384

#define ERROR_OPERATION_IN_PROGRESS             329L // ¬ WinError.h по какой-то причине этого кода не нашлось.

enum STATE                                      {undefined, starting, started, connecting, connected, stopping, stopped, errored};

#ifdef UNICODE
#define CHAR_TYPE                               TCHAR
#define STRINGSTREAM_TYPE                       std::wstringstream 
#else
#define CHAR_TYPE                               char
#define STRINGSTREAM_TYPE                       std::stringstream 
#endif

typedef std::basic_string<CHAR_TYPE>            t_string;

#include <sstream>
template<class T>
t_string t_to_string(T variable)
{
    STRINGSTREAM_TYPE ss; 
    t_string s;
    ss << variable;
    s = ss.str();

    return s;
}

#define ERROR_OPERATION_IN_PROGRESS             329L // ¬ WinError.h по какой-то причине этого кода не нашлось.

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

#define CLOSE_HANDLE(hName)                     if ((hName != NULL)) { CloseHandle(hName); hName = NULL; }                                                               


namespace NamedPipe
{    
    class Base
    {
        
    private:

        static  VOID                            MainThread(VOID *arg) { ((Base*)arg)->MainThread_Helper(); };
        virtual VOID                            MainThread_Helper(VOID) = 0;
        virtual DWORD                           Start_Helper(VOID) = 0;
        virtual VOID                            Stop_Helper(VOID)  = 0 ;
        
    protected:
                t_string                        PipeName_;        
                STATE                           eState_;

                HANDLE                          hMainThread_;
                DWORD                           nMainThreadId_;

                DWORD                           Write(const t_string & write_buffer, HANDLE pipe_handle, CRITICAL_SECTION & write_critical_section);
                DWORD                           StartMainThread(VOID);        
        
    public:
                                                Base(t_string pipe_name);
                                                ~Base(VOID);

                DWORD                           Start(VOID);
                DWORD                           Stop(VOID);
    };
}