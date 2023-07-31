// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "NiagaraDataInterface.h"
#include "NiagaraParameterStore.h"
#include "NiagaraDataInterfaceSpriteRendererInfo.generated.h"

class UNiagaraSpriteRendererProperties;

/** This Data Interface can be used to query information about the sprite renderers of an emitter */
UCLASS(EditInlineNew, Category = "Sprite Particles", meta = (DisplayName = "Sprite Renderer Info"))
class NIAGARA_API UNiagaraDataInterfaceSpriteRendererInfo : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	UNiagaraSpriteRendererProperties* GetSpriteRenderer() const { return SpriteRenderer; }

	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR	
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif 
	//UObject Interface End

	void OnSpriteRendererChanged(UNiagaraSpriteRendererProperties* NewSpriteRenderer);

	//UNiagaraDataInterface Interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
#endif
	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }
#if WITH_EDITOR
	virtual void GetFeedback(UNiagaraSystem* InAsset, UNiagaraComponent* InComponent, TArray<FNiagaraDataInterfaceError>& OutErrors, TArray<FNiagaraDataInterfaceFeedback>& OutWarnings, TArray<FNiagaraDataInterfaceFeedback>& OutInfo) override;
#endif
	//UNiagaraDataInterface Interface	

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	virtual void PushToRenderThreadImpl() override;

	void VMIsValid(FVectorVMExternalFunctionContext& Context);
	void VMGetSourceMode(FVectorVMExternalFunctionContext& Context);
	void VMGetAlignment(FVectorVMExternalFunctionContext& Context);
	void VMGetFacingMode(FVectorVMExternalFunctionContext& Context);
	void VMGetSubUVDetails(FVectorVMExternalFunctionContext& Context);

	/** The name of the sprite renderer */
	UPROPERTY(EditAnywhere, Category = "Source")
	TObjectPtr<UNiagaraSpriteRendererProperties> SpriteRenderer;

#if WITH_EDITOR
	FDelegateHandle OnSpriteRendererChangedHandle;
#endif
};
