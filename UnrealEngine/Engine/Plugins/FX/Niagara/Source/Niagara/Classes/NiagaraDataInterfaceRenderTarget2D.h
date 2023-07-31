// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceRW.h"
#include "ClearQuad.h"
#include "NiagaraComponent.h"
#include "NiagaraGenerateMips.h"
#include "NiagaraDataInterfaceRenderTarget2D.generated.h"

class FNiagaraSystemInstance;
class UTextureRenderTarget2D;

struct FRenderTarget2DRWInstanceData_GameThread
{
	FRenderTarget2DRWInstanceData_GameThread()
	{
#if WITH_EDITORONLY_DATA
		bPreviewTexture = false;
#endif
	}

	FIntPoint Size = FIntPoint(EForceInit::ForceInitToZero);
	ETextureRenderTargetFormat Format = RTF_RGBA16f;
	ENiagaraMipMapGeneration MipMapGeneration = ENiagaraMipMapGeneration::Disabled;
	ENiagaraMipMapGenerationType MipMapGenerationType = ENiagaraMipMapGenerationType::Linear;
	
	UTextureRenderTarget2D* TargetTexture = nullptr;
#if WITH_EDITORONLY_DATA
	uint32 bPreviewTexture : 1;
#endif
	FNiagaraParameterDirectBinding<UObject*> RTUserParamBinding;
};

struct FRenderTarget2DRWInstanceData_RenderThread
{
	FRenderTarget2DRWInstanceData_RenderThread()
	{
#if WITH_EDITORONLY_DATA
		bPreviewTexture = false;
#endif
	}

	FIntPoint Size = FIntPoint(EForceInit::ForceInitToZero);
	ENiagaraMipMapGeneration MipMapGeneration = ENiagaraMipMapGeneration::Disabled;
	ENiagaraMipMapGenerationType MipMapGenerationType = ENiagaraMipMapGenerationType::Linear;
	bool bRebuildMips = false;
	bool bReadThisFrame = false;
	bool bWroteThisFrame = false;

	FSamplerStateRHIRef	SamplerStateRHI;
	FTexture2DRHIRef	TextureRHI;

	FRDGTextureRef		TransientRDGTexture = nullptr;
	FRDGTextureSRVRef	TransientRDGSRV = nullptr;
	FRDGTextureUAVRef	TransientRDGUAV = nullptr;

#if WITH_EDITORONLY_DATA
	uint32 bPreviewTexture : 1;
#endif
#if STATS
	void UpdateMemoryStats();
	uint64 MemorySize = 0;
#endif
};

struct FNiagaraDataInterfaceProxyRenderTarget2DProxy : public FNiagaraDataInterfaceProxyRW
{
	FNiagaraDataInterfaceProxyRenderTarget2DProxy() {}
	virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) override {}
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override { return 0; }

	virtual void PostStage(const FNDIGpuComputePostStageContext& Context) override;
	virtual void PostSimulate(const FNDIGpuComputePostSimulateContext& Context) override;

	virtual FIntVector GetElementCount(FNiagaraSystemInstanceID SystemInstanceID) const override;

	/* List of proxy data for each system instances*/
	// #todo(dmp): this should all be refactored to avoid duplicate code
	TMap<FNiagaraSystemInstanceID, FRenderTarget2DRWInstanceData_RenderThread> SystemInstancesToProxyData_RT;
};

UCLASS(EditInlineNew, Category = "Grid", meta = (DisplayName = "Render Target 2D", Experimental), Blueprintable, BlueprintType)
class NIAGARA_API UNiagaraDataInterfaceRenderTarget2D : public UNiagaraDataInterfaceRWBase
{
	GENERATED_UCLASS_BODY()

public:
	virtual void PostInitProperties() override;
	
	//~ UNiagaraDataInterface interface
	// VM functionality
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return true; }
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
#if WITH_EDITORONLY_DATA
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override;
#endif
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	// GPU sim functionality
#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
#endif
	virtual bool UseLegacyShaderBindings() const  override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override {}
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	virtual int32 PerInstanceDataSize()const override { return sizeof(FRenderTarget2DRWInstanceData_GameThread); }
	virtual bool PerInstanceTickPostSimulate(void* PerInstanceData, FNiagaraSystemInstance* InSystemInstance, float DeltaSeconds) override;
	virtual bool HasPreSimulateTick() const override { return true; }
	virtual bool HasPostSimulateTick() const override { return true; }

	virtual bool CanExposeVariables() const override { return true;}
	virtual void GetExposedVariables(TArray<FNiagaraVariableBase>& OutVariables) const override;
	virtual bool GetExposedVariableValue(const FNiagaraVariableBase& InVariable, void* InPerInstanceData, FNiagaraSystemInstance* InSystemInstance, void* OutData) const override;

	virtual bool CanRenderVariablesToCanvas() const { return true; }
	virtual void GetCanvasVariables(TArray<FNiagaraVariableBase>& OutVariables) const override;
	virtual bool RenderVariableToCanvas(FNiagaraSystemInstanceID SystemInstanceID, FName VariableName, class FCanvas* Canvas, const FIntRect& DrawRect) const override;
	//~ UNiagaraDataInterface interface END

	void VMGetSize(FVectorVMExternalFunctionContext& Context); 
	void VMSetSize(FVectorVMExternalFunctionContext& Context);

	UPROPERTY(EditAnywhere, Category = "Render Target", meta = (EditCondition = "!bInheritUserParameterSettings"))
	FIntPoint Size;

	/** Controls if and when we generate mips for the render target. */
	UPROPERTY(EditAnywhere, Category = "Render Target", meta = (EditCondition = "!bInheritUserParameterSettings"))
	ENiagaraMipMapGeneration MipMapGeneration = ENiagaraMipMapGeneration::Disabled;

	UPROPERTY(EditAnywhere, Category = "Render Target")
	ENiagaraMipMapGenerationType MipMapGenerationType = ENiagaraMipMapGenerationType::Linear;

	/** When enabled overrides the format of the render target, otherwise uses the project default setting. */
	UPROPERTY(EditAnywhere, Category = "Render Target", meta = (EditCondition = "!bInheritUserParameterSettings && bOverrideFormat"))
	TEnumAsByte<ETextureRenderTargetFormat> OverrideRenderTargetFormat;

	/**
	When enabled texture parameters (size / etc) are taken from the user provided render target.
	If no valid user parameter is set the system will be invalid.
	Note: The resource will be recreated if UAV access is not available, which will reset the contents.
	*/
	UPROPERTY(EditAnywhere, Category = "Render Target")
	uint8 bInheritUserParameterSettings : 1;

	UPROPERTY(EditAnywhere, Category = "Render Target")
	uint8 bOverrideFormat : 1;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Render Target")
	uint8 bPreviewRenderTarget : 1;
#endif

	UPROPERTY(EditAnywhere, Category = "Render Target", meta = (ToolTip = "When valid the user parameter is used as the render target rather than creating one internal, note that the input render target will be adjusted by the Niagara simulation"))
	FNiagaraUserParameterBinding RenderTargetUserParameter;

protected:
	static FNiagaraVariableBase ExposedRTVar;

	TMap<FNiagaraSystemInstanceID, FRenderTarget2DRWInstanceData_GameThread*> SystemInstancesToProxyData_GT;
	
	UPROPERTY(Transient, DuplicateTransient)
	TMap<uint64, TObjectPtr<UTextureRenderTarget2D>> ManagedRenderTargets;
};
