// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "IContentAddressableStorage.h"
#include "IExecution.h"
#include "IRemoteExecutor.h"
#include "Internationalization/Text.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"


namespace UE::RemoteExecution
{
	class FHordeExecutor : public IRemoteExecutor
	{
	public:
		struct FSettings
		{
			FString ContentAddressableStorageTarget;
			FString ExecutionTarget;
			TMap<FString, FString> ContentAddressableStorageHeaders;
			TMap<FString, FString> ExecutionHeaders;
		};

	private:
		TUniquePtr<IContentAddressableStorage> ContentAddressableStorage;
		TUniquePtr<IExecution> Execution;

	public:
		void Initialize(const FSettings& Settings);
		void Shutdown();

		virtual FName GetFName() const override;
		virtual FText GetNameText() const override;
		virtual FText GetDescriptionText() const override;

		virtual bool CanRemoteExecute() const override;

		IContentAddressableStorage* GetContentAddressableStorage() const override { return ContentAddressableStorage.Get(); }
		IExecution* GetExecution() const override { return Execution.Get(); }
	};
}
