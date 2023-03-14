// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraDataInterface.h"
#include "NiagaraSettings.h"
#include "NiagaraShared.h"

#include "NiagaraDataInterfaceAsyncGpuTrace.generated.h"

class FNiagaraSystemInstance;

/** Data interface for handling latent (delayed 1 frame) traces against the scene for GPU simulations. */
UCLASS(EditInlineNew, Category = "Collision", meta = (DisplayName = "Async Gpu Trace"))
class NIAGARA_API UNiagaraDataInterfaceAsyncGpuTrace : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	/** The maximum number of traces (per particle) that can be created per frame.  Defines the size of the preallocated 
		buffer that is used to contain the traces. */
	UPROPERTY(EditAnywhere, Category = "Async GPU Trace")
	int32 MaxTracesPerParticle = 1;

	/** If a collision is rejected, how many times do we attempt to retrace from that collision point forward to find a new, valid collision.*/
	UPROPERTY(EditAnywhere, Category = "Async GPU Trace")
	int32 MaxRetraces = 0;

	/** Defines which technique is used to resolve the trace - see 'AsyncGpuTraceDI/Provider Priority' in the Niagara project settings. */
	UPROPERTY(EditAnywhere, Category = "Async GPU Trace")
	TEnumAsByte<ENDICollisionQuery_AsyncGpuTraceProvider::Type> TraceProvider = ENDICollisionQuery_AsyncGpuTraceProvider::Default;

	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//UObject Interface End

	virtual int32 PerInstanceDataSize() const override;
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* InSystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* InSystemInstance) override;

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return Target == ENiagaraSimTarget::GPUComputeSim; }
	virtual bool RequiresDistanceFieldData() const override;
	virtual bool RequiresRayTracingScene() const override;

#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetCommonHLSL(FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override;
#endif
	virtual bool UseLegacyShaderBindings() const  override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

private:
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	virtual void PushToRenderThreadImpl() override;
};
