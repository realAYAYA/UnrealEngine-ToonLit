// Copyright Epic Games, Inc. All Rights Reserved.

#include "HordeExecutor.h"

#include "ContentAddressableStorage.h"
#include "Execution.h"
#include "Internationalization/Internationalization.h"

#define LOCTEXT_NAMESPACE "HordeExecutor"


namespace UE::RemoteExecution
{
	void FHordeExecutor::Initialize(const FSettings& Settings)
	{
		Shutdown();

		ContentAddressableStorage.Reset(new FContentAddressableStorage(Settings.ContentAddressableStorageTarget, Settings.ContentAddressableStorageHeaders));
		Execution.Reset(new FExecution(Settings.ExecutionTarget, Settings.ExecutionHeaders));
	}

	void FHordeExecutor::Shutdown()
	{
		ContentAddressableStorage.Reset();
		Execution.Reset();
	}

	FName FHordeExecutor::GetFName() const
	{
		return FName("Horde");
	}

	FText FHordeExecutor::GetNameText() const
	{
		return LOCTEXT("DefaultDisplayName", "Horde");
	}

	FText FHordeExecutor::GetDescriptionText() const
	{
		return LOCTEXT("DefaultDisplayDesc", "Horde remote execution.");
	}

	bool FHordeExecutor::CanRemoteExecute() const
	{
		return ContentAddressableStorage.IsValid() && Execution.IsValid();
	}
}

#undef LOCTEXT_NAMESPACE
