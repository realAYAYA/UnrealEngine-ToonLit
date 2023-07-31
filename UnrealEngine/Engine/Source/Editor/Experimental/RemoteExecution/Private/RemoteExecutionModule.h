// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DefaultRemoteExecutor.h"
#include "IRemoteExecutionModule.h"
#include "Logging/LogMacros.h"
#include "UObject/NameTypes.h"

class IModularFeature;
namespace UE::RemoteExecution { class IRemoteExecutor; }

REMOTEEXECUTION_API DECLARE_LOG_CATEGORY_EXTERN(LogRemoteExecution, Display, All);


namespace UE::RemoteExecution
{
	class FRemoteExecutionModule : public IRemoteExecutionModule
	{
	public:
		/** Default constructor. */
		FRemoteExecutionModule();

		// IModuleInterface interface

		virtual void StartupModule() override;
		virtual void ShutdownModule() override;

		// IRemoteExecutionModule interface
		virtual bool CanRemoteExecute() const override;
		virtual IRemoteExecutor& GetRemoteExecutor() const override;
		virtual void SetRemoteExecutor(const FName& InName);

	private:
		/** Handle when one of the modular features we are interested in is registered */
		void HandleModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature);

		/** Handle when one of the modular features we are interested in is unregistered */
		void HandleModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature);

		FDefaultRemoteExecutor DefaultExecutor;

		IRemoteExecutor* CurrentExecutor;
	};
}
