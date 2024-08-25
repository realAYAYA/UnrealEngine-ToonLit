// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaNetworkBackendTcp.h"
#include "UbaNetworkClient.h"
#include "UbaNetworkServer.h"
#include "UbaStorageProxy.h"
#include "UbaVersion.h"

#if PLATFORM_WINDOWS

namespace uba
{
	const tchar*	Version = GetVersionString();
	u16				DefaultListenPort = DefaultPort + 1;
	const wchar_t*	DefaultName = []() { static wchar_t buf[256]; GetComputerNameW(buf, sizeof_array(buf)); return buf; }();

	int PrintHelp(const wchar_t* message)
	{
		LoggerWithWriter logger(g_consoleLogWriter, L"");
		if (*message)
		{
			logger.Info(L"");
			logger.Error(L"%s", message);
		}

		logger.Info(L"");
		logger.Info(L"------------------------");
		logger.Info(L"   UbaProxy v%s", Version);
		logger.Info(L"------------------------");
		logger.Info(L"");
		logger.Info(L"  When started UbaProxy will keep trying to connect to provided host address.");
		logger.Info(L"  Once connected it will proxy messages from agents to host.");
		logger.Info(L"");
		logger.Info(L"  -host=<host>[:<port>]    The ip/name and port (default: %u) of the machine we want to help", DefaultPort);
		logger.Info(L"  -listen=[<name>]:<port>  The port to connect to. Defaults to \"%u\"", DefaultListenPort);
		logger.Info(L"  -quiet                   Does not output any logging in console");
		logger.Info(L"");
		return -1;
	}
}

int wmain(int argc, wchar_t *argv[])
{
	using namespace uba;

	StringBuffer<> proxyName(DefaultName);
	StringBuffer<> host;
	u16 hostPort = DefaultPort;
	StringBuffer<> listenHost;
	u16 listenPort = DefaultListenPort;
	bool poll = true;
	bool quiet = false;

	for (int i = 1; i != argc; ++i)
	{
		StringBuffer<> name;
		StringBuffer<> value;

		if (const wchar_t* equals = wcschr(argv[i], '='))
		{
			name.Append(argv[i], equals - argv[i]);
			value.Append(equals + 1);
		}
		else
		{
			name.Append(argv[i]);
		}

		if (name.Equals(L"-host"))
		{
			if (const wchar_t* portIndex = value.First(':'))
			{
				StringBuffer<> portStr(portIndex + 1);
				if (!portStr.Parse(hostPort))
					return PrintHelp(L"Invalid value for port in -host");
				value.Resize(portIndex - value.data);
			}
			if (value.IsEmpty())
				return PrintHelp(L"-host needs a name/ip");
			host.Append(value);
		}
		else if (name.Equals(L"-listen"))
		{
			if (const wchar_t* portIndex = value.First(':'))
			{
				StringBuffer portStr(portIndex + 1);
				if (!portStr.Parse(listenPort))
					return PrintHelp(TC("Invalid value for port in -listen"));
				value.Resize(portIndex - value.data);
			}
			listenHost.Append(value);
		}
		else if (name.Equals(L"-nopoll"))
		{
			poll = false;
		}
		else if (name.Equals(L"-quiet"))
		{
			quiet = true;
		}
		else if (name.Equals(L"-?"))
		{
			return PrintHelp(L"");
		}
		else
		{
			StringBuffer<> msg;
			msg.Appendf(L"Unknown argument '%s'", name.data);
			return PrintHelp(msg.data);
		}
	}

	if (host.IsEmpty())
		return PrintHelp(L"No host provided. Add -host=<host>");

	FilteredLogWriter logWriter(g_consoleLogWriter, quiet ? LogEntryType_Info : LogEntryType_Detail);
	LoggerWithWriter logger(logWriter, L"");

	const wchar_t* dbgStr = L"";
	#if UBA_DEBUG
	dbgStr = L" (DEBUG)";
	#endif
	logger.Info(L"UbaStorageProxy v%s%s", Version, dbgStr);
	logger.Info(L"");

	while (true)
	{
		u32 receiveTimeoutSeconds = 0; // We can't set this to something else atm since there is no ping going

		NetworkBackendTcp networkBackend(logWriter);

		NetworkServerCreateInfo nsci(logWriter);
		nsci.receiveTimeoutSeconds = receiveTimeoutSeconds;
		bool ctorSuccess = true;
		NetworkServer server(ctorSuccess, nsci);
		if (!ctorSuccess)
			return -1;

		if (!server.StartListen(networkBackend, listenPort, listenHost.data))
			return -1;

		NetworkClientCreateInfo ncci(logWriter);
		ncci.receiveTimeoutSeconds = receiveTimeoutSeconds;
		NetworkClient* client = new NetworkClient(ctorSuccess, ncci);
		auto csg = MakeGuard([&]() { delete client; });
		if (!ctorSuccess)
			return -1;

		// TODO: We need to send a message to the storage server to get the uid
		Guid storageServerUid;

		StorageProxy proxy(server, *client, storageServerUid, proxyName.data);

		logger.Info(L"Waiting to connect to %s:%u", host.data, u16(hostPort));
		int retryCount = 5;
		u64 startTime = GetTime();
		bool timedOut = false;
		bool exit = false;
		while (!client->Connect(networkBackend, host.data, u16(hostPort), &timedOut))
		{
			if (IsEscapePressed()) //escape
			{
				exit = true;
				break;
			}
			if (!timedOut)
				return -1;

			if (!poll && !--retryCount)
			{
				logger.Error(L"Failed to connect to %s:%u (after %s)", host.data, hostPort, TimeToText(GetTime() - startTime).str);
				return -1;
			}
		}

		if (exit)
			return 0;

		Event disconnected(true);

		client->RegisterOnDisconnected([&]()
			{
				disconnected.Set();
			});

		disconnected.IsSet();
		networkBackend.StopListen();
		server.DisconnectClients();
		server.PrintSummary(logger);
		client->PrintSummary(logger);

		proxy.PrintSummary();

		if (!poll)
			return 0;
	}
	return 0;
}
#else
int main(int argc, char* argv[])
{
	return 0;
}
#endif // PLATFORM_WINDOWS
