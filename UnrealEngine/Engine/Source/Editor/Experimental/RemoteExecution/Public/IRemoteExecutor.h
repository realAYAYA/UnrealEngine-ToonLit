// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "IContentAddressableStorage.h"
#include "IExecution.h"
#include "Features/IModularFeatures.h"


/**
 * Interface for remote execution functionality
 */
namespace UE::RemoteExecution
{
	class IRemoteExecutor : public IModularFeature
	{
	public:
		/** Virtual destructor */
		virtual ~IRemoteExecutor() {}

		virtual FName GetFName() const = 0;
		virtual FText GetNameText() const = 0;
		virtual FText GetDescriptionText() const = 0;

		virtual bool CanRemoteExecute() const = 0;

		virtual IContentAddressableStorage* GetContentAddressableStorage() const = 0;
		virtual IExecution* GetExecution() const = 0;
	};
}
