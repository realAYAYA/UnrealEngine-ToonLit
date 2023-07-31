// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterfaceRW.h"
#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "NiagaraShader.h"
#include "ShaderParameterUtils.h"
#include "RHIUtilities.h"

#include "NiagaraDataInterfaceArray.generated.h"

struct INDIArrayProxyBase : public FNiagaraDataInterfaceProxyRW
{
	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters,)
		SHADER_PARAMETER(FIntPoint,		ArrayBufferParams)
		SHADER_PARAMETER_SRV(Buffer,	ArrayReadBuffer)
		SHADER_PARAMETER_UAV(RWBuffer,	ArrayRWBuffer)
	END_SHADER_PARAMETER_STRUCT()

	virtual ~INDIArrayProxyBase() {}
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) const = 0;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) = 0;
#if WITH_EDITORONLY_DATA
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) const = 0;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) const = 0;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const = 0;
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) const = 0;
#endif
#if WITH_NIAGARA_DEBUGGER
	virtual void DrawDebugHud(UCanvas* Canvas, FNiagaraSystemInstance* SystemInstance, FString& VariableDataString, bool bVerbose) const = 0;
#endif
	virtual bool CopyToInternal(INDIArrayProxyBase* Destination) const = 0;
	virtual bool Equals(const INDIArrayProxyBase* Other) const = 0;
	virtual int32 PerInstanceDataSize() const = 0;
	virtual bool InitPerInstanceData(UNiagaraDataInterface* DataInterface, void* InPerInstanceData, FNiagaraSystemInstance* SystemInstance) = 0;
	virtual void DestroyPerInstanceData(void* InPerInstanceData, FNiagaraSystemInstance* SystemInstance) = 0;
	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) = 0;
	virtual void SetShaderParameters(FShaderParameters* ShaderParameters, FNiagaraSystemInstanceID SystemInstanceID) const = 0;
};

UCLASS(abstract, EditInlineNew)
class NIAGARA_API UNiagaraDataInterfaceArray : public UNiagaraDataInterfaceRWBase
{
	GENERATED_UCLASS_BODY()

public:
	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override { GetProxyAs<INDIArrayProxyBase>()->GetFunctions(OutFunctions); }
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override { GetProxyAs<INDIArrayProxyBase>()->GetVMExternalFunction(BindingInfo, InstanceData, OutFunc); }
#if WITH_EDITORONLY_DATA
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override { GetProxyAs<INDIArrayProxyBase>()->GetParameterDefinitionHLSL(ParamInfo, OutHLSL); }
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override { return GetProxyAs<INDIArrayProxyBase>()->GetFunctionHLSL(ParamInfo, FunctionInfo, FunctionInstanceIndex, OutHLSL); }
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override { return GetProxyAs<INDIArrayProxyBase>()->UpgradeFunctionCall(FunctionSignature); }
#endif
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }

#if WITH_NIAGARA_DEBUGGER
	virtual void DrawDebugHud(UCanvas* Canvas, FNiagaraSystemInstance* SystemInstance, FString& VariableDataString, bool bVerbose) const { GetProxyAs<INDIArrayProxyBase>()->DrawDebugHud(Canvas, SystemInstance, VariableDataString, bVerbose); }
#endif

	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const;
	virtual bool Equals(const UNiagaraDataInterface* Other) const;

	virtual int32 PerInstanceDataSize() const override { return GetProxyAs<INDIArrayProxyBase>()->PerInstanceDataSize(); }
	virtual bool InitPerInstanceData(void* InPerInstanceData, FNiagaraSystemInstance* SystemInstance) override { return GetProxyAs<INDIArrayProxyBase>()->InitPerInstanceData(this, InPerInstanceData, SystemInstance); }
	virtual void DestroyPerInstanceData(void* InPerInstanceData, FNiagaraSystemInstance * SystemInstance) override { GetProxyAs<INDIArrayProxyBase>()->DestroyPerInstanceData(InPerInstanceData, SystemInstance); }

	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override { GetProxyAs<INDIArrayProxyBase>()->ProvidePerInstanceDataForRenderThread(DataForRenderThread, PerInstanceData, SystemInstance); }

	virtual bool UseLegacyShaderBindings() const  override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;
	//UNiagaraDataInterface Interface

	/** ReadWrite lock to ensure safe access to the underlying array. */
	FRWLock ArrayRWGuard;

	/** How to do we want to synchronize modifications to the array data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Array")
	ENiagaraGpuSyncMode GpuSyncMode = ENiagaraGpuSyncMode::SyncCpuToGpu;

	/** When greater than 0 sets the maximum number of elements the array can hold, only relevant when using operations that modify the array size. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="Array", meta=(ClampMin="0"))
	int32 MaxElements;
};
