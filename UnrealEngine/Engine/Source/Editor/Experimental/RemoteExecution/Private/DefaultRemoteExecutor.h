// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IRemoteExecutor.h"
#include "Internationalization/Text.h"
#include "UObject/NameTypes.h"


namespace UE::RemoteExecution
{
class IContentAddressableStorage;
class IExecution;

	class FDefaultRemoteExecutor : public IRemoteExecutor
	{
	public:
		virtual FName GetFName() const override;
		virtual FText GetNameText() const override;
		virtual FText GetDescriptionText() const override;

		virtual bool CanRemoteExecute() const override;

		virtual IContentAddressableStorage* GetContentAddressableStorage() const;
		virtual IExecution* GetExecution() const;
	};
}
