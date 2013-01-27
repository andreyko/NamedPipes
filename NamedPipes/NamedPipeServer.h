#include <windows.h>
#include <string>
#include <queue>

#pragma once

// TODO_URGENT: начальный статус установить S_STATE_DISCONNECTED
#define S_STATE_DISCONNECTED					0
#define S_STATE_CONNECTING						1 
#define S_STATE_CONNECTED						2
//#define S_READING_STATE							1 
//#define S_WRITING_STATE							2 

#define S_PIPES_INSTANCES						4 
#define S_PIPES_EVENTS							S_PIPES_INSTANCES + 1		/* +1 - евент для стопа */

#define	S_MANAGING_EVENTS						4
#define	S_EVENT_START_READING					0
#define	S_EVENT_START_WRITING					1
#define	S_EVENT_STOP							2
#define	S_EVENT_STOP_SERVER						3

#define	S_EVENTS_								2
#define	S_EVENT_READING_FINISHED				0
#define	S_EVENT_WRITING_FINISHED				1

#define S_PIPE_TIMEOUT							5000
#define S_BUFSIZE								4096

#define STOP_SERVER_EVENT_GUID					"Global\\NP-3EAACE24-DBD1-4F64-A32A-522BDD2EE2C1"
#define SERVER_IS_WORKING_MUTEX_GUID			"Global\\NP-683FBDF8-B363-4746-A0A3-224CA657072E"

#ifdef UNICODE
#define CHAR_TYPE								TCHAR
#else
#define CHAR_TYPE								char
#endif

typedef std::basic_string<CHAR_TYPE>			t_string;

enum	STATE									{undefined, started, stopped};

class NamedPipeServer
{
private:
typedef
	struct 
	{ 
		OVERLAPPED								oOverlap; 
		HANDLE									hPipeInst; 
		TCHAR									chRequest[S_BUFSIZE]; 
		DWORD									cbRead;
		TCHAR									chReply[S_BUFSIZE];
		DWORD									cbToWrite; 
		DWORD									dwState; 
		//BOOL									fPendingIO; 
	}	PIPEINST, *LPPIPEINST; 
		
		t_string								PipeName_;
		t_string								S_ReadBuffer_;
		t_string								S_WriteBuffer_;
		std::queue<std::pair<DWORD, t_string>>	S_GlobalReadBuffer_;

		PIPEINST								Pipe_[S_PIPES_INSTANCES]; 
		CRITICAL_SECTION						csPipe_;
		HANDLE									hS_Pipes_Events_[S_PIPES_EVENTS];
		HANDLE									hS_Managing_Events_[S_MANAGING_EVENTS];
		HANDLE									hS_Events_[S_EVENTS_];
		HANDLE									hS_ServerIsUp_Mutex;
		HANDLE									hS_MainThread_;
		DWORD									nS_MainThreadId_;	
		HANDLE									hS_ReadThread_;
		DWORD									nS_ReadThreadId_;
		STATE									eS_State_;

		DWORD									nWriteToPipe_;
		DWORD									nReadFromPipe_;

		DWORD									nReadResult_;
		DWORD									nWriteResult_;

		VOID									StartMainThread();
		VOID									StopMainThread();
		VOID									StartReadThread();
		VOID									StopReadThread();
		DWORD									ConnectToNewClient(DWORD pipe_number);
		VOID									DisconnectAndReconnect(DWORD dwPipeIndex);

static	VOID									MainThread(VOID *arg) { ((NamedPipeServer*)arg)->MainThread_Helper(); };
static	VOID									ReadThread(VOID *arg) { ((NamedPipeServer*)arg)->ReadThread_Helper(); };

		VOID									GetAnswerToRequest(LPPIPEINST pipe);
		DWORD									Start_Helper();
		VOID									Stop_Helper();
		DWORD									Read_Helper(VOID);
		DWORD									Write_Helper(VOID);

protected:		
		VOID									MainThread_Helper();
		VOID									ReadThread_Helper();
	
public:
												NamedPipeServer(t_string pipe_name);
												~NamedPipeServer(VOID);

		DWORD									Start(VOID);
		VOID									Stop(VOID);
		DWORD									Read(DWORD & pipe_number, t_string & read_buffer);		
		DWORD									Write(DWORD pipe_number, t_string write_buffer);		
};

