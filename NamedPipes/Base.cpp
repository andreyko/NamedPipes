#include "StdAfx.h"
#include "Base.h"

namespace NamedPipe
{
    Base::Base(t_string pipe_name): eState_(STATE::stopped)
    {
        PipeName_ = pipe_name;

        printf("Base\n");
    }


    Base::~Base(VOID)
    {
        printf("~Base\n");        
    }


    DWORD Base::Start(VOID)
    {
        switch (eState_)
        {
            case STATE::starting:
                return ERROR_OPERATION_IN_PROGRESS;

            case STATE::started:
                return ERROR_ALREADY_EXISTS;

            case STATE::stopping:
                return ERROR_INVALID_OPERATION;
        } 

        DWORD result_of_start_helper = Start_Helper();

        if (result_of_start_helper != ERROR_SUCCESS)
        { 
            eState_ = STATE::errored;
            Stop();
        }

        return result_of_start_helper;
    }


    DWORD Base::Stop(VOID)
    { 
        switch (eState_)
        {
            case STATE::stopping:
                return ERROR_OPERATION_IN_PROGRESS;

            case STATE::stopped:
                return ERROR_NOT_FOUND;

            case STATE::starting:
                return ERROR_INVALID_OPERATION;  
        }

        Stop_Helper();

        return ERROR_SUCCESS;
    }


    DWORD Base::Write(const t_string & write_buffer, HANDLE pipe_handle, CRITICAL_SECTION & write_critical_section)
    {
        if (eState_ != STATE::started)
        {
            return ERROR_NOT_FOUND;
        }    

        EnterCriticalSection(&write_critical_section);

            DWORD cbWritten;

            BOOL bSuccess = WriteFile ( 
                                            pipe_handle,                                            // pipe handle 
                                            write_buffer.c_str(),                                   // message 
                                            write_buffer.length() * 
                                            sizeof(t_string::traits_type::char_type),               // message length 
                                            &cbWritten,                                             // bytes written 
                                            NULL                                                    // not overlapped 
                                        );

        LeaveCriticalSection(&write_critical_section);

        return bSuccess ? ERROR_SUCCESS : GetLastError();
    }


    DWORD Base::StartMainThread(VOID)
    {
        printf("Before MainThread_Helper %d\n", GetCurrentThreadId ());

        hMainThread_ = CreateThread  (NULL, 0, (LPTHREAD_START_ROUTINE)MainThread, this, 0, &nMainThreadId_); 
        // MainThread -> MainThread_Helper

        printf("After MainThread_Helper %d\n", GetCurrentThreadId ());

        return (hMainThread_ == NULL) ? GetLastError() : ERROR_SUCCESS;
    }
}