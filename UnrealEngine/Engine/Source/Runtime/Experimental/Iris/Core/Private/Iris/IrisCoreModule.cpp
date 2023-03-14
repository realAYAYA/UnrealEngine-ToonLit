// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "Iris/ReplicationState/DefaultPropertyNetSerializerInfos.h"
#include "Iris/ReplicationState/PropertyNetSerializerInfoRegistry.h"
#include "Iris/ReplicationSystem/LegacyPushModel.h"
#include "Iris/Serialization/InternalNetSerializerDelegates.h"
#include "Iris/IrisConfigInternal.h"

#include "Misc/CommandLine.h"

// $IRIS: $TODO: Remove when UE-158358 is completed
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Net/Core/PropertyConditions/RepChangedPropertyTracker.h"
// End Remove when UE-158358 is completed

class FIrisCoreModule : public IModuleInterface
{
private:

	// $IRIS: $TODO: Make proper impl of support for SetPropertyCustomCondition - UE-158358
	static void SetPropertyCustomCondition(const UObject* Owner, uint16 RepIndex, bool bEnable)
	{
		using namespace UE::Net;
		// See if we can find the instance in any replication system
		for (uint32 RepSystemIt = 0; RepSystemIt < FReplicationSystemFactory::MaxReplicationSystemCount; ++RepSystemIt)
		{
			UReplicationSystem* ReplicationSystem = GetReplicationSystem(RepSystemIt);
			if (ReplicationSystem)
			{
				if (const UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
				{
					const FNetHandle Handle = Bridge->GetReplicatedHandle(Owner);
					if (Handle.IsValid())
					{
						ReplicationSystem->SetPropertyCustomCondition(Handle, Owner, RepIndex, bEnable);
					}
				}
			}
		}
	}

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
		// Check command line if we should disable Iris as we need to do that early
		int32 UseIrisReplication;
		if(FParse::Value(FCommandLine::Get(), TEXT("UseIrisReplication="), UseIrisReplication))
		{
			UE::Net::SetUseIrisReplication(UseIrisReplication > 0);
		}
		else
		{
			UE::Net::SetUseIrisReplication(true);
		}
		
		RegisterPropertyNetSerializerSelectorTypes();

		UE_NET_IRIS_INIT_LEGACY_PUSH_MODEL();

		UE::Net::Private::SetIrisSetPropertyCustomConditionDelegate(UE::Net::Private::FIrisSetPropertyCustomCondition::CreateStatic(&FIrisCoreModule::SetPropertyCustomCondition));
		
		ModulesChangedHandle = FModuleManager::Get().OnModulesChanged().AddRaw(this, &FIrisCoreModule::OnModulesChanged);
	}

	virtual void ShutdownModule() override
	{
		if (ModulesChangedHandle.IsValid())
		{
			FModuleManager::Get().OnModulesChanged().Remove(ModulesChangedHandle);
			ModulesChangedHandle.Reset();
		}
	
		UE::Net::Private::SetIrisSetPropertyCustomConditionDelegate({});

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
