// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "NiagaraDataInterface.h"
#include "NiagaraParameterStore.h"
#include "NiagaraDataInterfaceMeshRendererInfo.generated.h"

class UNiagaraMeshRendererProperties;
class FNDIMeshRendererInfo;

/** This Data Interface can be used to query information about the mesh renderers of an emitter */
UCLASS(EditInlineNew, Category = "Mesh Particles", meta = (DisplayName = "Mesh Renderer Info"))
class NIAGARA_API UNiagaraDataInterfaceMeshRendererInfo : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

	struct FMeshData
	{
		FVector3f MinLocalBounds = FVector3f(ForceInitToZero);
		FVector3f MaxLocalBounds = FVector3f(ForceInitToZero);
	};

public:
	UNiagaraMeshRendererProperties* GetMeshRenderer() const { return MeshRenderer; }

	//UObject Interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
#if WITH_EDITOR	
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif 
	//UObject Interface End

	void OnMeshRendererChanged(UNiagaraMeshRendererProperties* NewMeshRenderer);

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
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override;
	virtual void GetFeedback(UNiagaraSystem* InAsset, UNiagaraComponent* InComponent, TArray<FNiagaraDataInterfaceError>& OutErrors, TArray<FNiagaraDataInterfaceFeedback>& OutWarnings,
		TArray<FNiagaraDataInterfaceFeedback>& OutInfo) override;
#endif
	//UNiagaraDataInterface Interface	

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	virtual void PushToRenderThreadImpl() override;
	void UpdateCachedData();

	void VMGetNumMeshes(FVectorVMExternalFunctionContext& Context);
	void VMGetMeshLocalBounds(FVectorVMExternalFunctionContext& Context);
	void VMGetSubUVDetails(FVectorVMExternalFunctionContext& Context);

	/** The name of the mesh renderer */
	UPROPERTY(EditAnywhere, Category = "Source")
	TObjectPtr<UNiagaraMeshRendererProperties> MeshRenderer;

	TArray<FMeshData> CachedMeshData;

#if WITH_EDITOR
	FDelegateHandle OnMeshRendererChangedHandle;
#endif
};
