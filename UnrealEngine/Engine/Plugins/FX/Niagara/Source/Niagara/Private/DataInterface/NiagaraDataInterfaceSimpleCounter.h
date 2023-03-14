// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "NiagaraDataInterface.h"
#include "HAL/PlatformAtomics.h"
#include "NiagaraDataInterfaceSimpleCounter.generated.h"

/**
Thread safe counter starts at the initial value on start / reset.
When operating between CPU & GPU ensure you set the appropriate sync mode.
*/
UCLASS(EditInlineNew, Category = "Counting", meta = (DisplayName = "Simple Counter"))
class NIAGARA_API UNiagaraDataInterfaceSimpleCounter : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER(int32,		CountOffset)
	END_SHADER_PARAMETER_STRUCT();

public:
	// UObject Interface
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	// UObject Interface End

	// UNiagaraDataInterface Interface Begin
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual int32 PerInstanceDataSize() const override;

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
#if WITH_EDITORONLY_DATA
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
#endif
	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;
	virtual void PushToRenderThreadImpl() override;
	// UNiagaraDataInterface Interface End

	// VM functions
	void VMGet(FVectorVMExternalFunctionContext& Context);
	void VMExchange(FVectorVMExternalFunctionContext& Context);
	void VMAdd(FVectorVMExternalFunctionContext& Context);
	void VMIncrement(FVectorVMExternalFunctionContext& Context);
	void VMDecrement(FVectorVMExternalFunctionContext& Context);

	void GetNextValue_Deprecated(FVectorVMExternalFunctionContext& Context);

	/** Select how we should synchronize the counter between Cpu & Gpu */
	UPROPERTY(EditAnywhere, Category = "Simple Counter")
	ENiagaraGpuSyncMode GpuSyncMode = ENiagaraGpuSyncMode::None;

	/** This is the value the counter will have when the instance is reset / created. */
	UPROPERTY(EditAnywhere, Category = "Simple Counter")
	int32 InitialValue = 0;
};
