// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConsoleExec.h"
#include "Misc/DisplayClusterLog.h"
#include "Cluster/DisplayClusterClusterEvent.h"

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Features/IModularFeatures.h"


bool FDisplayClusterConsoleExec::Exec(const FDisplayClusterClusterEventJson& InEvent)
{
	const FString* const ExecString = InEvent.Parameters.Find("ExecString");
	if (!ExecString || !ExecString->Len())
	{
		UE_LOG(LogDisplayClusterModule, Warning, TEXT("ConsoleExec ignoring cluster event with no ExecString"));
		return false;
	}

	const FString* const RequestedExecutor = InEvent.Parameters.Find("Executor");

	UE_LOG(LogDisplayClusterModule, Log, TEXT("ConsoleExec cluster event: Executor='%s', ExecString='%s'"), RequestedExecutor ? **RequestedExecutor : TEXT(""), **ExecString);

	if (RequestedExecutor && RequestedExecutor->Len())
	{
		TArray<IConsoleCommandExecutor*> CommandExecutors = IModularFeatures::Get().GetModularFeatureImplementations<IConsoleCommandExecutor>(IConsoleCommandExecutor::ModularFeatureName());
		for (IConsoleCommandExecutor* CommandExecutor : CommandExecutors)
		{
			if (CommandExecutor->GetName() == FName(*RequestedExecutor))
			{
				return CommandExecutor->Exec(**ExecString);
			}
		}

		UE_LOG(LogDisplayClusterModule, Warning, TEXT("ConsoleExec couldn't find requested executor: %s"), **RequestedExecutor);
		return false;
	}
	else
	{
		// With no explicit executor, try to route this as an Unreal console
		// command as best we can. Available context is limited here.
		ULocalPlayer* const Player = GEngine->GetDebugLocalPlayer();
		if (Player)
		{
			return Player->Exec(Player->GetWorld(), **ExecString, *GLog);
		}
		else
		{
			return GEngine->Exec(nullptr, **ExecString, *GLog);
		}
	}
}
