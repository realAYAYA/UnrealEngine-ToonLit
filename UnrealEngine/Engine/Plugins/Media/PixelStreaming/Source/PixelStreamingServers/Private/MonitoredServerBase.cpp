// Copyright Epic Games, Inc. All Rights Reserved.

#include "MonitoredServerBase.h"
#include "PixelStreamingServersLog.h"
#include "HAL/PlatformFileManager.h"
#include "ServerUtils.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/CoreDelegates.h"

namespace UE::PixelStreamingServers
{

	FMonitoredServerBase::~FMonitoredServerBase()
	{
		Stop();
	}

	bool FMonitoredServerBase::FindServerAbsPath(FLaunchArgs& InLaunchArgs, FString& OutServerAbsPath)
	{
		bool bUseServerBinary = InLaunchArgs.ServerBinaryOverridePath.IsSet();

		if(!bUseServerBinary)
		{
			bool bFoundServer = Utils::GetDownloadedServer(OutServerAbsPath, GetServerResourceDirectory());
			if(bFoundServer)
			{
				return true;
			}
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Downloaded server not found at - %s"), *OutServerAbsPath);
		}
		else
		{
			OutServerAbsPath = InLaunchArgs.ServerBinaryOverridePath.GetValue();
			IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
			if(PlatformFile.FileExists(*OutServerAbsPath))
			{
				return true;
			}
			UE_LOG(LogPixelStreamingServers, Log, TEXT("Server binary not found at - %s"), *OutServerAbsPath);
		}
		return false;
	}

	bool FMonitoredServerBase::LaunchImpl(FLaunchArgs& InLaunchArgs, TMap<EEndpoint, FURL>& OutEndpoints)
	{
		// Bind to OnEnginePreExit in case our child process is still running when engine is shutting down
		EngineShutdownHandle = FCoreDelegates::OnEnginePreExit.AddSP(this, &FMonitoredServerBase::Stop);

		bool bFoundServerPath = FindServerAbsPath(InLaunchArgs, ServerRootAbsPath);
		if(!bFoundServerPath)
		{
			return false;
		}

		ServerProcess = LaunchServerProcess(InLaunchArgs, ServerRootAbsPath, Endpoints);
		return ServerProcess.IsValid();
	}

	FString FMonitoredServerBase::GetPathOnDisk()
	{
		return ServerRootAbsPath;
	}

	void FMonitoredServerBase::Stop()
	{
		if (ServerProcess)
		{
			ServerProcess->Cancel();
			ServerProcess.Reset();
		}
		FCoreDelegates::OnEnginePreExit.Remove(EngineShutdownHandle);
	}

} // UE::PixelStreamingServers
