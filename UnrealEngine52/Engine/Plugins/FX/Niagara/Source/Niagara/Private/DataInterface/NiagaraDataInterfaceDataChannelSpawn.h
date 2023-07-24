// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

/**
Data Channel Spawn Interface.
Specialization of the Read DI that allows Emitters to Spawn directly from Data Channel contents.
*/

#include "NiagaraDataInterfaceDataChannelRead.h"
#include "NiagaraDataInterfaceEmitterBinding.h"
#include "NiagaraDataInterfaceDataChannelSpawn.generated.h"


UCLASS(Experimental, EditInlineNew, Category = "Data Channels", meta = (DisplayName = "Spawn From Data Channel"))
class NIAGARA_API UNiagaraDataInterfaceDataChannelSpawn : public UNiagaraDataInterfaceDataChannelRead
{
	GENERATED_UCLASS_BODY()

	/** The emitter into which we will spawn. */
	UPROPERTY(EditAnywhere, Category = "Spawning")
	FNiagaraDataInterfaceEmitterBinding EmitterBinding;
	
	/** 
	If true, we override the SpawnGroup of any generated spawns with the element index of the data channel that generated the spawn. 
	This allows the particle spawn script to subsequently access further data from the Data Channel at this index.
	*/
	UPROPERTY(EditAnywhere, Category = "Spawning")
	bool bOverrideSpawnGroupToDataChannelIndex = true;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual int32 PerInstanceDataSize() const override;

	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	virtual bool PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	
	void Spawn(FVectorVMExternalFunctionContext& Context);

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

};