// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Iris/ReplicationState/DefaultPropertyNetSerializerInfos.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationSystem/LegacyPushModel.h"
#include "Iris/Serialization/InternalNetSerializerDelegates.h"
#include "Iris/IrisConfigInternal.h"

#include "Misc/CommandLine.h"

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

		// Check command line for whether we should override the net.Iris.UseIrisReplication cvar, as we need to do that early
		int32 UseIrisReplication;
		if(FParse::Value(FCommandLine::Get(), TEXT("UseIrisReplication="), UseIrisReplication))
		{
			UE::Net::SetUseIrisReplication(UseIrisReplication > 0);
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

	void OnModulesChanged(FName ModuleThatChanged, EModuleChangeReason ReasonForChange)
	{
		switch (ReasonForChange)
		{
			case EModuleChangeReason::ModuleLoaded:
			{
				UE::Net::Private::FInternalNetSerializerDelegates::BroadcastPreFreezeNetSerializerRegistry();
				UE::Net::Private::FInternalNetSerializerDelegates::BroadcastPostFreezeNetSerializerRegistry();
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
};
IMPLEMENT_MODULE(FIrisCoreModule, IrisCore);
