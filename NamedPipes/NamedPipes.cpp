// NamedPipes.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Client.h"
#include "Server.h"

int _tmain(int argc, _TCHAR* argv[])
{
    static NamedPipe::Server nps(TEXT("\\\\.\\pipe\\mynamedpipe"));
    nps.Start();

    t_string readed_string;
    DWORD pipe_number;

    while (true)
    {
        nps.Read(readed_string, pipe_number);
        _tprintf(TEXT("Pipe #%d Readed: %s\n"), pipe_number, readed_string.c_str());

        readed_string += t_string(TEXT("  Pipe: #") + t_to_string(pipe_number) + TEXT("\n"));
        _tprintf(TEXT("Pipe #%d Writing: %s\n"), pipe_number, readed_string.c_str());
        nps.Write(readed_string, pipe_number);
    }

    nps.Stop();

    return 0;
}

