// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultRemoteExecutor.h"

#include "Internationalization/Internationalization.h"

namespace UE::RemoteExecution { class IContentAddressableStorage; }
namespace UE::RemoteExecution { class IExecution; }

#define LOCTEXT_NAMESPACE "DefaultRemoteExecutor"


namespace UE::RemoteExecution
{
	FName FDefaultRemoteExecutor::GetFName() const
	{
		return FName("None");
	}

	FText FDefaultRemoteExecutor::GetNameText() const
	{
		return LOCTEXT("DefaultDisplayName", "None");
	}

	FText FDefaultRemoteExecutor::GetDescriptionText() const
	{
		return LOCTEXT("DefaultDisplayDesc", "Disable remote execution.");
	}

	bool FDefaultRemoteExecutor::CanRemoteExecute() const
	{
		return false;
	}

	IContentAddressableStorage* FDefaultRemoteExecutor::GetContentAddressableStorage() const
	{
		return nullptr;
	}

	IExecution* FDefaultRemoteExecutor::GetExecution() const
	{
		return nullptr;
	}
}

#undef LOCTEXT_NAMESPACE
