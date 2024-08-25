// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaNetworkBackendMemory.h"
#include "UbaNetworkServer.h"
#include "UbaSessionClient.h"
#include "UbaStorageClient.h"
#include "UbaStorageProxy.h"

namespace uba
{
	struct ClientInitInfo
	{
		LogWriter& logWriter;
		NetworkBackend& networkBackend;
		const tchar* rootDir = nullptr;
		const tchar* host = nullptr;
		u16 port = 0;
		const tchar* zone = nullptr;
		u32 maxProcessorCount = 0;
		u32 index = 0;
	};

	class Client
	{
	public:
		bool Init(const ClientInitInfo& info)
		{
			networkBackend = &info.networkBackend;
			networkBackendMem = new NetworkBackendMemory(info.logWriter);
			bool ctorSuccess = true;
			networkClient = new NetworkClient(ctorSuccess, { info.logWriter });
			if (!ctorSuccess)
				return false;

			StringBuffer<> clientRootDir;
			clientRootDir.Append(info.rootDir).Append("Agent").AppendValue(info.index);
			StorageClientCreateInfo storageClientInfo(*networkClient, clientRootDir.data);
			storageClientInfo.zone = info.zone;
			storageClientInfo.getProxyBackendCallback = [](void* ud, const tchar* h) -> NetworkBackend& { return ((Client*)ud)->GetProxyBackend(h); };
			storageClientInfo.getProxyBackendUserData = this;
			storageClientInfo.startProxyCallback = [](void* ud, u16 p, const Guid& ssu) { return ((Client*)ud)->StartProxy(p, ssu); };
			storageClientInfo.startProxyUserData = this;
			storageClient = new StorageClient(storageClientInfo);

			SessionClientCreateInfo sessionClientInfo(*storageClient, *networkClient, info.logWriter);
			sessionClientInfo.maxProcessCount = info.maxProcessorCount;
			sessionClientInfo.rootDir = clientRootDir.data;
			sessionClientInfo.deleteSessionsOlderThanSeconds = 1;

			sessionClient = new SessionClient(sessionClientInfo);

			storageClient->Start();
			sessionClient->Start();
			
			return networkClient->Connect(*networkBackend, info.host, info.port);
		}

		bool StartProxy(u16 proxyPort, const Guid& storageServerUid)
		{
			NetworkServerCreateInfo nsci(networkClient->GetLogWriter());
			nsci.workerCount = 192;
			nsci.receiveTimeoutSeconds = 60;

			StringBuffer<256> prefix;
			prefix.Append(TC("UbaProxyServer (")).Append(GuidToString(networkClient->GetUid()).str).Append(')');
			serverPrefix = prefix.data;
			bool ctorSuccess = true;
			proxyNetworkServer = new NetworkServer(ctorSuccess, nsci, serverPrefix.c_str());
			if (!ctorSuccess)
			{
				delete proxyNetworkServer;
				return false;
			}
			proxyStorage = new StorageProxy(*proxyNetworkServer, *networkClient, storageServerUid, TC("Wooohoo"), storageClient);
			proxyNetworkServer->StartListen(*networkBackendMem, proxyPort);
			proxyNetworkServer->StartListen(*networkBackend, proxyPort);
			return true;
		}

		NetworkBackend& GetProxyBackend(const tchar* host)
		{
			return Equals(host, TC("inprocess")) ? *networkBackendMem : *networkBackend;
		}

		~Client()
		{
			if (proxyNetworkServer)
				proxyNetworkServer->DisconnectClients();
			if (storageClient)
				storageClient->StopProxy();
			if (sessionClient)
				sessionClient->Stop();
			if (networkClient)
				networkClient->Disconnect();
			delete proxyStorage;
			delete proxyNetworkServer;
			delete sessionClient;
			delete storageClient;
			delete networkClient;
			delete networkBackendMem;
		}

		NetworkBackendMemory* networkBackendMem = nullptr;
		NetworkClient* networkClient = nullptr;
		StorageClient* storageClient = nullptr;
		SessionClient* sessionClient = nullptr;

		NetworkBackend* networkBackend = nullptr;
		NetworkServer* proxyNetworkServer = nullptr;
		StorageProxy* proxyStorage = nullptr;
		TString serverPrefix;
	};
}
