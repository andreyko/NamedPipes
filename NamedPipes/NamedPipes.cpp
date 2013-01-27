// NamedPipes.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <windows.h>

#include "NamedPipeServer.h"

BOOL WINAPI ControlHandler(DWORD dwCtrlType)
{
    switch (dwCtrlType) {
      case CTRL_C_EVENT:
      case CTRL_BREAK_EVENT:
      case CTRL_CLOSE_EVENT:
      case CTRL_LOGOFF_EVENT:
      case CTRL_SHUTDOWN_EVENT:
        //LogMessageV(SYELOG_SEVERITY_INFORMATION, "User requested stop.");
        //_tprintf("\nSYELOGD: Closing connections.\n");
        //if (s_hOutFile != INVALID_HANDLE_VALUE) {
        //    _tprintf("Closing file.\n");
        //    FlushFileBuffers(s_hOutFile);
        //    CloseHandle(s_hOutFile);
        //    s_hOutFile = INVALID_HANDLE_VALUE;
        //}
        ExitProcess(0);
    }
    return FALSE;
}

int _tmain(int argc, _TCHAR* argv[])
{
	//SetConsoleCtrlHandler(ControlHandler, TRUE);

	// TODO: добавить "SetPipeName"
static	NamedPipeServer nps(TEXT("\\\\.\\pipe\\mynamedpipe"));

	nps.Start();
	
	t_string str_tmp = TEXT("Started: counter = %d\n");

	DWORD counter = 0;
	while (true)
	{
		_tprintf(str_tmp.c_str(), counter);
		
		DWORD pipe_number = counter;
		t_string readed_string = TEXT("");


		nps.Read(pipe_number, readed_string);

		_tprintf(TEXT("Pipe #%d readed: %s\n"), pipe_number, readed_string.c_str());

		switch (counter)
		{


			//case 10:
			//	nps.Write(TEXT("987"));
			//	break;

			//case 20:
			//	nps.Stop();
			//	str_tmp = TEXT("Stopped: counter = %d\n");
			//	break;
			//
			//case 30:
			//	nps.Start();
			//	t_string str_tmp = TEXT("Started: counter = %d\n");
			//	break;
		}

		counter++;
	}
	

	return 0;
}

