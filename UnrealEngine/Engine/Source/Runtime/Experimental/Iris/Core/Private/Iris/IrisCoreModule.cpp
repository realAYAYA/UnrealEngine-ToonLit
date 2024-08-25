// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/CommandLine.h"

#include "Net/Core/Connection/NetEnums.h"

#include "Iris/ReplicationState/DefaultPropertyNetSerializerInfos.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationSystem/LegacyPushModel.h"
#include "Iris/Serialization/InternalNetSerializerDelegates.h"
#include "Iris/IrisConfigInternal.h"


class FIrisCoreModule : public IModuleInterface
{
private:

	void RegisterPropertyNetSerializerSelectorTypes()
	{
		using namespace UE::Net;
		using namespace UE::Net::Private;

		FPropertyNetSerializerInfoRegistry::Reset();

		FInternalNetSerializerDelegates::BroadcastPreFreezeNetSerializerRegistry();
		RegisterDefaultPropertyNetSerializerInfos();

		FPropertyNetSerializerInfoRegistry::Freeze();
		FInternalNetSerializerDelegates::BroadcastPostFreezeNetSerializerRegistry();
	}

	virtual void StartupModule() override
	{
		// Iris requires NetCore
		FModuleManager::LoadModuleChecked<IModuleInterface>("NetCore");

		FCoreDelegates::OnAllModuleLoadingPhasesComplete.AddRaw(this, &FIrisCoreModule::OnAllModuleLoadingPhasesComplete);

		// Check command line for whether we should override the net.Iris.UseIrisReplication cvar, as we need to do that early
		const EReplicationSystem CmdlineRepSystem = UE::Net::GetUseIrisReplicationCmdlineValue();
		if (CmdlineRepSystem != EReplicationSystem::Default)
		{
			const bool bEnableIris = CmdlineRepSystem == EReplicationSystem::Iris;
			UE::Net::SetUseIrisReplication(bEnableIris);
		}

		RegisterPropertyNetSerializerSelectorTypes();

		UE_NET_IRIS_INIT_LEGACY_PUSH_MODEL();
		
		ModulesChangedHandle = FModuleManager::Get().OnModulesChanged().AddRaw(this, &FIrisCoreModule::OnModulesChanged);
	}

	virtual void ShutdownModule() override
	{
		if (ModulesChangedHandle.IsValid())
		{
			FModuleManager::Get().OnModulesChanged().Remove(ModulesChangedHandle);
			ModulesChangedHandle.Reset();
		}
	
		UE_NET_IRIS_SHUTDOWN_LEGACY_PUSH_MODEL();
	}

	virtual bool SupportsDynamicReloading() override
	{
		return false;
	}

	void OnAllModuleLoadingPhasesComplete()
	{
		bAllowLoadedModulesUpdatedCallback = true;
		UE::Net::Private::FInternalNetSerializerDelegates::BroadcastLoadedModulesUpdated();

		FCoreDelegates::OnAllModuleLoadingPhasesComplete.RemoveAll(this);
	}

	void OnModulesChanged(FName ModuleThatChanged, EModuleChangeReason ReasonForChange)
	{
		switch (ReasonForChange)
		{
			case EModuleChangeReason::ModuleLoaded:
			{
				UE::Net::Private::FInternalNetSerializerDelegates::BroadcastPreFreezeNetSerializerRegistry();
				UE::Net::Private::FInternalNetSerializerDelegates::BroadcastPostFreezeNetSerializerRegistry();
				if (bAllowLoadedModulesUpdatedCallback)
				{
					UE::Net::Private::FInternalNetSerializerDelegates::BroadcastLoadedModulesUpdated();
				}
			}
			break;

			default:
			{
			}
			break;
		}
	}

private:
	FDelegateHandle ModulesChangedHandle;
	bool bAllowLoadedModulesUpdatedCallback = false;
};
IMPLEMENT_MODULE(FIrisCoreModule, IrisCore);
