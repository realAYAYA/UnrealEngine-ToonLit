// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceWater.generated.h"

class UWaterBodyComponent;

UCLASS(EditInlineNew, Category = "Water", meta = (DisplayName = "Water"))
class WATER_API UNiagaraDataInterfaceWater : public UNiagaraDataInterface
{
	GENERATED_BODY()

public:
	virtual void PostInitProperties() override;

	/** UNiagaraDataInterface interface */
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	virtual int32 PerInstanceDataSize() const override;
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	virtual bool HasPreSimulateTick() const override { return true; }
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return Target == ENiagaraSimTarget::CPUSim; }

#if WITH_EDITORONLY_DATA
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override;
#endif

#if WITH_NIAGARA_DEBUGGER
	virtual void DrawDebugHud(FNDIDrawDebugHudContext& DebugHudContext) const override;
#endif

	void GetWaterDataAtPoint(FVectorVMExternalFunctionContext& Context);

	void GetWaveParamLookupTableOffset(FVectorVMExternalFunctionContext& Context);

	/** Sets the current water body to be used by this data interface */
	void SetWaterBodyComponent(UWaterBodyComponent* InWaterBodyComponent) { SourceBodyComponent = InWaterBodyComponent; }

protected:
#if WITH_EDITORONLY_DATA
	virtual void GetFunctionsInternal(TArray<FNiagaraFunctionSignature>& OutFunctions) const override;
#endif

private:
	UPROPERTY(EditAnywhere, Category = "Water") 
	TObjectPtr<UWaterBodyComponent> SourceBodyComponent;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Components/SplineComponent.h"
#endif
