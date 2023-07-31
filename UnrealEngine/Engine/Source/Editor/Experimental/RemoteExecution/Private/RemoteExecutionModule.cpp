// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteExecutionModule.h"

#include "Containers/UnrealString.h"
#include "Delegates/Delegate.h"
#include "Features/IModularFeatures.h"
#include "HAL/Platform.h"
#include "IRemoteExecutor.h"
#include "Misc/AssertionMacros.h"
#include "Modules/ModuleManager.h"
#include "RemoteExecutionSettings.h"
#include "UObject/UObjectGlobals.h"

class IModularFeature;

IMPLEMENT_MODULE(UE::RemoteExecution::FRemoteExecutionModule, RemoteExecution);

#define LOCTEXT_NAMESPACE "RemoteExecutionModule"

DEFINE_LOG_CATEGORY(LogRemoteExecution);

static FName RemoteExecutionFeatureName(TEXT("RemoteExecution"));


namespace UE::RemoteExecution
{
	FRemoteExecutionModule::FRemoteExecutionModule()
		: CurrentExecutor(nullptr)
	{
	}

	void FRemoteExecutionModule::StartupModule()
	{
		GetMutableDefault<URemoteExecutionSettings>()->LoadConfig();

		// Register to check for remote execution features
		IModularFeatures::Get().OnModularFeatureRegistered().AddRaw(this, &FRemoteExecutionModule::HandleModularFeatureRegistered);
		IModularFeatures::Get().OnModularFeatureUnregistered().AddRaw(this, &FRemoteExecutionModule::HandleModularFeatureUnregistered);

		// bind default accessor to editor
		IModularFeatures::Get().RegisterModularFeature(RemoteExecutionFeatureName, &DefaultExecutor);
	}

	void FRemoteExecutionModule::ShutdownModule()
	{
		// unbind default provider from editor
		IModularFeatures::Get().UnregisterModularFeature(RemoteExecutionFeatureName, &DefaultExecutor);

		// we don't care about modular features any more
		IModularFeatures::Get().OnModularFeatureRegistered().RemoveAll(this);
		IModularFeatures::Get().OnModularFeatureUnregistered().RemoveAll(this);
	}

	bool FRemoteExecutionModule::CanRemoteExecute() const
	{
		return CurrentExecutor->CanRemoteExecute();;
	}

	IRemoteExecutor& FRemoteExecutionModule::GetRemoteExecutor() const
	{
		return *CurrentExecutor;
	}

	void FRemoteExecutionModule::SetRemoteExecutor(const FName& InName)
	{
		const int32 FeatureCount = IModularFeatures::Get().GetModularFeatureImplementationCount(RemoteExecutionFeatureName);
		for (int32 FeatureIndex = 0; FeatureIndex < FeatureCount; FeatureIndex++)
		{
			IModularFeature* Feature = IModularFeatures::Get().GetModularFeatureImplementation(RemoteExecutionFeatureName, FeatureIndex);
			check(Feature);

			IRemoteExecutor& RemoteExecution = *static_cast<IRemoteExecutor*>(Feature);
			if (InName == RemoteExecution.GetFName())
			{
				CurrentExecutor = static_cast<IRemoteExecutor*>(Feature);
				break;
			}
		}
	}

	void FRemoteExecutionModule::HandleModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature)
	{
		if (Type == RemoteExecutionFeatureName)
		{
			const FString PreferredRemoteExecutor = GetDefault<URemoteExecutionSettings>()->PreferredRemoteExecutor;

			CurrentExecutor = &DefaultExecutor;
			SetRemoteExecutor(FName(PreferredRemoteExecutor));
		}
	}

	void FRemoteExecutionModule::HandleModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature)
	{
		if (Type == RemoteExecutionFeatureName && CurrentExecutor == static_cast<IRemoteExecutor*>(ModularFeature))
		{
			CurrentExecutor = &DefaultExecutor;
		}
	}
}

#undef LOCTEXT_NAMESPACE
