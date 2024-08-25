// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaBinaryReaderWriter.h"
#include "UbaDetoursShared.h"
#include "UbaEvent.h"
#include "UbaProtocol.h"
#include "UbaProcessStats.h"
#include <dlfcn.h>
#if PLATFORM_LINUX
#include <sys/prctl.h>
#endif
using namespace uba;

namespace uba
{
	int g_comFd = -1;
	int g_sessionPid = -1;
	Event* g_cancelEvent;
	Event* g_readEvent;
	Event* g_writeEvent;
	u8* g_messageMappingMem;

	void PreInit(const char* logFile);
	void Init();
	void Deinit();
}

static const int g_allSignals[] = {
	#ifdef SIGHUP
	SIGHUP,
	#endif
	#ifdef SIGQUIT
	SIGQUIT,
	#endif
	#ifdef SIGTRAP
	SIGTRAP,
	#endif
	#ifdef SIGIO
	SIGIO,
	#endif
	SIGABRT,
	SIGFPE,
	SIGILL,
	SIGINT,
	SIGSEGV,
	SIGTERM 
};


static void segfault_sigaction(int signal)
{
	uba::UbaAssert("Segmentation fault", "", 0, "", -1);
}

static void __attribute__((constructor(102))) PreInitCtor()
{
	using namespace uba;
	SuppressDetourScope s;

	#if PLATFORM_LINUX
	prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0); // We want the process to die if the parent die
	#endif
	
	InitSharedVariables();

	const char* logFile = getenv("UBA_LOGFILE");
	PreInit(logFile);
	unsetenv("UBA_LOGFILE");

	const char* comId = getenv("UBA_COMID");
	//printf("Starting up %s... (comid: %s) %u\n", __progname, (comId && *comId) ? comId : "NOTSET", getpid());

	if (!comId || !*comId)
		return;

	StringBuffer<256> comIdName;
	const char* plusIndex = strchr(comId, '+');
	comIdName.Append(comId, u64(plusIndex - comId));
	u64 comIdUid;
	if (!comIdName.Parse(comIdUid))
		UBA_ASSERT(false);

	u32 comIdOffset = strtoul(plusIndex + 1, nullptr, 10);
	unsetenv("UBA_COMID");

	comIdName.Clear();
	GetMappingHandleName(comIdName, comIdUid);

	g_comFd = shm_open(comIdName.data, O_RDWR, S_IRUSR | S_IWUSR);

	if (g_comFd == -1)
	{
		printf("UbaDetours: Failed to open shared mem: %s\n", comIdName.data);
		return;
	}
	u8* rptr = (u8*)mmap(NULL, CommunicationMemSize, PROT_READ | PROT_WRITE, MAP_SHARED, g_comFd, s64(comIdOffset));
	if (rptr == MAP_FAILED)
	{
		printf("UbaDetours: Failed to mmap fd: %u\n", g_comFd);
		return;
	}

	g_cancelEvent = ((Event*)rptr);
	g_readEvent = ((Event*)rptr) + 1;
	g_writeEvent = ((Event*)rptr) + 2;
	g_messageMappingMem = rptr + sizeof(Event) * 3;

	#if 0 // TODO: For some reason this does not work
	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sigemptyset(&sa.sa_mask);
	sa.sa_handler = segfault_sigaction;
	sa.sa_flags = 0;// SA_SIGINFO;
	for (auto signal : g_allSignals)
		sigaction(signal, &sa, NULL);
	#endif

	g_sessionPid = atoi(getenv("UBA_SESSION_PROCESS"));
	g_runningRemote = getenv("UBA_REMOTE") != nullptr;

	g_rulesIndex = strtoul(getenv("UBA_RULES"), nullptr, 10);
	g_rules = GetApplicationRules()[g_rulesIndex].rules;
	unsetenv("UBA_RULES");

	const char* realCwd = getenv("UBA_CWD");
	UBA_ASSERT(realCwd);
	chdir(realCwd);
	unsetenv("UBA_CWD");

	Init();
}

void CloseCom()
{
	if (g_comFd == -1)
		return;
	void* mem = g_cancelEvent;
	g_cancelEvent = nullptr;
	g_readEvent = nullptr;
	g_writeEvent = nullptr;
	g_messageMappingMem = nullptr;
	munmap(mem, CommunicationMemSize);
	close(g_comFd);
	g_comFd = -1;
}

static void __attribute__((destructor(102))) InitDtor()
{
	using namespace uba;

	Deinit();
	CloseCom();
	//printf("Exiting... %s   %u \n", __progname, getpid());
}

namespace uba
{
	BinaryWriter::BinaryWriter()
	{
		m_begin = g_messageMappingMem;
		m_pos = m_begin;
		m_end = m_begin + CommunicationMemSize - sizeof(Event) * 3;
	}

	void BinaryWriter::Flush(bool waitOnResponse)
	{
		g_writeEvent->Set();

		if (!waitOnResponse)
			return;

		TimerScope ts(g_stats.waitOnResponse);
		do
		{
			if (g_readEvent->IsSet(1000))
				break;
			
			// check if session process is gone
			if (kill(g_sessionPid, 0) == -1 && errno == ESRCH)
				exit(1337);
			
			if (g_cancelEvent->IsSet(0))
				exit(1339);
		} while (true);
	}

	BinaryReader::BinaryReader()
	{
		m_begin = g_messageMappingMem;
		m_pos = m_begin;
		m_end = m_begin + CommunicationMemSize - sizeof(Event) * 3;
	}
}
