// Copyright Epic Games, Inc. All Rights Reserved.

/*
UNiagaraDataInterfaceDataChannelSpawn
A specialized version of the Read DI that allows for direct spawning into emitters from a SpawnInfo variable in the Data Channel Payload.
*/

#include "NiagaraDataInterfaceDataChannelSpawn.h"

#include "CoreMinimal.h"

#include "NiagaraCommon.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelHandler.h"
#include "NiagaraDataChannelDefinitions.h"

#include "NiagaraDataSetAccessor.h"
#include "NiagaraSystemInstance.h"


#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceDataChannelSpawn"

DECLARE_CYCLE_STAT(TEXT("NDIDataChannelSpawn Spawn"), STAT_NDIDataChannelSpawn_Spawn, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelSpawn Tick"), STAT_NDIDataChannelSpawn_Tick, STATGROUP_NiagaraDataChannels);
DECLARE_CYCLE_STAT(TEXT("NDIDataChannelSpawn PostTick"), STAT_NDIDataChannelSpawn_PostTick, STATGROUP_NiagaraDataChannels);

namespace NDIDataChannelLocal
{
	static const FName SpawnName(TEXT("Spawn"));
}

struct FNDIDataChannelSpawnInstanceData : public FNDIDataChannelReadInstanceData
{
	virtual ~FNDIDataChannelSpawnInstanceData() {}

	bool Init(UNiagaraDataInterfaceDataChannelSpawn* Interface, FNiagaraSystemInstance* Instance)
	{
		if (FNDIDataChannelReadInstanceData::Init(Interface, Instance))
		{
			EmitterInstance = Interface->EmitterBinding.Resolve(Instance, Interface);
			if (EmitterInstance)
			{
				return true;
			}
		}

		return false;
	}

	bool Tick(UNiagaraDataInterfaceDataChannelSpawn* Interface, FNiagaraSystemInstance* Instance)
	{
		//Cache the current CPU dataset to ensure we don't need to regen the bindings.
		FNiagaraDataBuffer* CachedData = ExternalCPU;

		bool bSuccess = FNDIDataChannelReadInstanceData::Tick(Interface, Instance);

		if (bSuccess)
		{
			//Do we need to regen the read offsets.
			if (CachedData != ExternalCPU)
			{
				const FNiagaraDataSetCompiledData& DataSetCompiledData = ExternalCPU->GetOwner()->GetCompiledData();
				SpawnInfoAccessor = FNiagaraDataSetAccessor<FNiagaraSpawnInfo>(DataSetCompiledData, SpawnInfoName);
			}
			return true;
		}

		return false;
	}

	bool PostTick(UNiagaraDataInterfaceDataChannelSpawn* Interface, FNiagaraSystemInstance* Instance)
	{
		return FNDIDataChannelReadInstanceData::PostTick(Interface, Instance);
	}

	FNiagaraEmitterInstance* EmitterInstance = nullptr;

	/*
	We read a spawn info for future proofing here but currently we only use the spawn count.
	NOTE: This is 100% dependent on SpawnInfo class not changing. If it does we'll need to update this.
	TODO: To support continuous interpolated spawning we'd need;
	- Support persistent IDs when adding data to the data channel buffers.
	- Support getting previous and current frame data by ID to do interpolation.
	- Likely more issues to resolve.
	*/
	FNiagaraDataSetAccessor<FNiagaraSpawnInfo> SpawnInfoAccessor;

	static const FName SpawnInfoName;
};

const FName FNDIDataChannelSpawnInstanceData::SpawnInfoName(TEXT("SpawnInfo"));

UNiagaraDataInterfaceDataChannelSpawn::UNiagaraDataInterfaceDataChannelSpawn(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UNiagaraDataInterfaceDataChannelSpawn::Equals(const UNiagaraDataInterface* Other) const
{
	if (const UNiagaraDataInterfaceDataChannelSpawn* OtherTyped = CastChecked<UNiagaraDataInterfaceDataChannelSpawn>(Other))
	{
		if (Super::Equals(Other) &&
			EmitterBinding == OtherTyped->EmitterBinding &&
			bOverrideSpawnGroupToDataChannelIndex == OtherTyped->bOverrideSpawnGroupToDataChannelIndex)
		{
			return true;
		}
	}
	return false;
}

void UNiagaraDataInterfaceDataChannelSpawn::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	Super::GetFunctions(OutFunctions);
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = NDIDataChannelLocal::SpawnName;
#if WITH_EDITORONLY_DATA
		Sig.Description = LOCTEXT("SpawnFunctionDescription", "Spawns particles in the given emitter for every entry in a Data Channel.");
#endif
		Sig.bMemberFunction = true;
		Sig.bRequiresExecPin = true;
		Sig.bExperimental = true;
		Sig.ModuleUsageBitmask = ENiagaraScriptUsageMask::Emitter | ENiagaraScriptUsageMask::System;
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Interface")));
		Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Max Spawns")), LOCTEXT("Spawn_MaxSpawnsDesc", "Maximum number of elements to process into individual spawns. Number of particles spawned per element is determined by the Spawn Count property in each element."));
		OutFunctions.Add(Sig);
	}
}
void UNiagaraDataInterfaceDataChannelSpawn::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
	if (BindingInfo.Name == NDIDataChannelLocal::SpawnName)
	{
		OutFunc = FVMExternalFunction::CreateLambda([this](FVectorVMExternalFunctionContext& Context) { this->Spawn(Context); });
	}
	else
	{
		Super::GetVMExternalFunction(BindingInfo, InstanceData, OutFunc);
	}

	if (!OutFunc.IsBound())
	{
		UE_LOG(LogTemp, Display, TEXT("Could not find data interface external function in %s. Received Name: %s"), *GetPathNameSafe(this), *BindingInfo.Name.ToString());
	}
}


bool UNiagaraDataInterfaceDataChannelSpawn::InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelSpawn_Spawn);
	FNDIDataChannelSpawnInstanceData* InstanceData = new (PerInstanceData) FNDIDataChannelSpawnInstanceData;

	if (INiagaraModule::DataChannelsEnabled() == false)
	{
		return false;
	}

	if (InstanceData->Init(this, SystemInstance) == false)
	{
		return false;
	}

	return true;
}

void UNiagaraDataInterfaceDataChannelSpawn::DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)
{
	FNDIDataChannelSpawnInstanceData* InstanceData = static_cast<FNDIDataChannelSpawnInstanceData*>(PerInstanceData);
	InstanceData->~FNDIDataChannelSpawnInstanceData();

	ENQUEUE_RENDER_COMMAND(RemoveProxy)
	(
		[RT_Proxy = GetProxyAs<FNiagaraDataInterfaceProxy_DataChannelRead>(), InstanceID = SystemInstance->GetId()](FRHICommandListImmediate& CmdList)
		{
			RT_Proxy->SystemInstancesToProxyData_RT.Remove(InstanceID);
		}
	);
}

bool UNiagaraDataInterfaceDataChannelSpawn::PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelSpawn_Tick);
	check(SystemInstance);
	if (INiagaraModule::DataChannelsEnabled() == false)
	{
		return true;
	}

	FNDIDataChannelSpawnInstanceData* InstanceData = static_cast<FNDIDataChannelSpawnInstanceData*>(PerInstanceData);
	if (!InstanceData)
	{
		return true;
	}

	if (InstanceData->Tick(this, SystemInstance) == false)
	{
		return true;
	}
	return false;
}

bool UNiagaraDataInterfaceDataChannelSpawn::PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds)
{
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelSpawn_PostTick);
	check(SystemInstance);
	if(INiagaraModule::DataChannelsEnabled() == false)
	{
		return true;
	}
	
	FNDIDataChannelSpawnInstanceData* InstanceData = static_cast<FNDIDataChannelSpawnInstanceData*>(PerInstanceData);
	if (!InstanceData)
	{
		return true;
	}

	if (InstanceData->PostTick(this, SystemInstance) == false)
	{
		return true;
	}

	return false;
}

int32 UNiagaraDataInterfaceDataChannelSpawn::PerInstanceDataSize() const
{
	return sizeof(FNDIDataChannelSpawnInstanceData);
}

void UNiagaraDataInterfaceDataChannelSpawn::Spawn(FVectorVMExternalFunctionContext& Context)
{
	SCOPE_CYCLE_COUNTER(STAT_NDIDataChannelSpawn_Spawn);

	//This should only be called from emitter scripts and since it has per instance data then we process them individually.
	check(Context.GetNumInstances() == 1);

	VectorVM::FUserPtrHandler<FNDIDataChannelSpawnInstanceData> InstData(Context);
	FNDIInputParam<int32> InMax(Context);

	FNiagaraEmitterInstance* EmittterInst = InstData->EmitterInstance;
	if (InstData->ExternalCPU && EmittterInst && InstData->SpawnInfoAccessor.IsValid() && INiagaraModule::DataChannelsEnabled())
	{
		TArray<FNiagaraSpawnInfo>& EmitterSpawnInfos = EmittterInst->GetSpawnInfo();
		FNiagaraDataSetReaderStruct<FNiagaraSpawnInfo> Reader = InstData->SpawnInfoAccessor.GetReader(InstData->ExternalCPU);
		int32 TotalSpawns = 0;
		int32 MaxSpawns = InMax.GetAndAdvance();
		if (TotalSpawns < MaxSpawns)
		{
			for (int32 DataChannelIdx = 0; DataChannelIdx < Reader.GetNumInstances(); ++DataChannelIdx)
			{
				FNiagaraSpawnInfo SpawnInfo = Reader.Get(DataChannelIdx);
				if (SpawnInfo.Count > 0)
				{
					++TotalSpawns;
					if (bOverrideSpawnGroupToDataChannelIndex)
					{
						SpawnInfo.SpawnGroup = DataChannelIdx;
					}

					EmitterSpawnInfos.Emplace(SpawnInfo);
				}
			}
		}
	}
}

bool UNiagaraDataInterfaceDataChannelSpawn::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}

	if (UNiagaraDataInterfaceDataChannelSpawn* DestTyped = CastChecked<UNiagaraDataInterfaceDataChannelSpawn>(Destination))
	{
		DestTyped->EmitterBinding = EmitterBinding;
		DestTyped->bOverrideSpawnGroupToDataChannelIndex = bOverrideSpawnGroupToDataChannelIndex;
		return true;
	}

	return false;
}


#undef LOCTEXT_NAMESPACE