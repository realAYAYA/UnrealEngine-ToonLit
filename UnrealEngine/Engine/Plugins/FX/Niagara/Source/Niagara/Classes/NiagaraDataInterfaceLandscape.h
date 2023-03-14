// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "VectorVM.h"

#include "NiagaraDataInterfaceLandscape.generated.h"

class ALandscape;
struct FNDILandscapeData_GameThread;
class UPhysicalMaterial;

UENUM()
enum class ENDILandscape_SourceMode : uint8
{
	/**
	Default behavior.
	- Use "Source" when explicitly specified.
	- When no source is specified, fall back on attached actor or component or world.
	*/
	Default,

	/**
	Only use "Source".
	*/
	Source,

	/**
	Only use the parent actor or component the system is attached to.
	*/
	AttachParent
};

/** Data Interface allowing sampling of a Landscape */
UCLASS(EditInlineNew, Category = "Landscape", meta = (DisplayName = "Landscape Sample"))
class NIAGARA_API UNiagaraDataInterfaceLandscape : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Landscape")
	TObjectPtr<AActor> SourceLandscape;

	UPROPERTY(EditAnywhere, Category = "Landscape")
	ENDILandscape_SourceMode SourceMode = ENDILandscape_SourceMode::Default;

	UPROPERTY(EditAnywhere, Category = "Landscape")
	TArray<TObjectPtr<UPhysicalMaterial>> PhysicalMaterials;

	//UObject Interface
	virtual void PostInitProperties() override;	
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return Target == ENiagaraSimTarget::GPUComputeSim; }
#if WITH_EDITORONLY_DATA
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override;
#endif
	//UNiagaraDataInterface Interface End

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	// GPU sim functionality
#if WITH_EDITORONLY_DATA
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
#endif
	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override;
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual int32 PerInstanceDataSize() const override;
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	virtual bool HasPreSimulateTick() const override { return true; }

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	void ApplyLandscape(const FNiagaraSystemInstance& SystemInstance, FNDILandscapeData_GameThread& InstanceData) const;
	ALandscape* GetLandscape(const FNiagaraSystemInstance& SystemInstance, ALandscape* Hint) const;

	static const FName GetBaseColorName;
	static const FName GetHeightName;
	static const FName GetWorldNormalName;
	static const FName GetPhysicalMaterialIndexName;
};
