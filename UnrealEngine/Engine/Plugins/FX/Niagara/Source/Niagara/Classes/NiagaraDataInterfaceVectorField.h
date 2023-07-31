// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraDataInterface.h"
#include "VectorField/VectorField.h"
#include "NiagaraDataInterfaceVectorField.generated.h"

class FNiagaraSystemInstance;

UCLASS(EditInlineNew, Category = "Vector Field", meta = (DisplayName = "Vector Field"))
class NIAGARA_API UNiagaraDataInterfaceVectorField : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER(FVector3f,				TilingAxes)
		SHADER_PARAMETER(FVector3f,				Dimensions)
		SHADER_PARAMETER(FVector3f,				MinBounds)
		SHADER_PARAMETER(FVector3f,				MaxBounds)
		SHADER_PARAMETER_TEXTURE(Texture3D,		Texture)
		SHADER_PARAMETER_SAMPLER(SamplerState,	Sampler)
	END_SHADER_PARAMETER_STRUCT();

public:
	/** Vector field to sample from. */
	UPROPERTY(EditAnywhere, Category = VectorField)
	TObjectPtr<UVectorField> Field;

	UPROPERTY(EditAnywhere, Category = VectorField)
	bool bTileX;
	UPROPERTY(EditAnywhere, Category = VectorField)
	bool bTileY;
	UPROPERTY(EditAnywhere, Category = VectorField)
	bool bTileZ;

public:
	//~ UObject interface
	virtual void PostInitProperties() override;
	virtual void PostLoad() override; 
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
#endif
	//~ UObject interface END

	//~ UNiagaraDataInterface interface
	// VM functionality
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override;

#if WITH_EDITOR	
	// Editor functionality
	virtual void GetFeedback(UNiagaraSystem* InAsset, UNiagaraComponent* InComponent, TArray<FNiagaraDataInterfaceError>& OutErrors, TArray<FNiagaraDataInterfaceFeedback>& OutWarnings, TArray<FNiagaraDataInterfaceFeedback>& OutInfo) override;

#endif

	// GPU sim functionality
#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
#endif
	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;
	//~ UNiagaraDataInterface interface END

	// VM functions
	void GetFieldDimensions(FVectorVMExternalFunctionContext& Context);
	void GetFieldBounds(FVectorVMExternalFunctionContext& Context); 
	void GetFieldTilingAxes(FVectorVMExternalFunctionContext& Context);
	void SampleVectorField(FVectorVMExternalFunctionContext& Context);
	void LoadVectorField(FVectorVMExternalFunctionContext& Context);
	
	//	
	FVector GetTilingAxes() const;
	FVector GetDimensions() const;
	FVector GetMinBounds() const;
	FVector GetMaxBounds() const;
protected:
	//~ UNiagaraDataInterface interface
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	//~ UNiagaraDataInterface interface END

	virtual void PushToRenderThreadImpl() override;
};

struct FNiagaraDataInterfaceProxyVectorField : public FNiagaraDataInterfaceProxy
{
	FVector Dimensions;
	FVector MinBounds;
	FVector MaxBounds;
	bool bTileX;
	bool bTileY;
	bool bTileZ;
	FTextureRHIRef TextureRHI;

	FVector GetTilingAxes() const
	{
		return FVector(float(bTileX), float(bTileY), float(bTileZ));
	}

	virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) override { check(false); }
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return 0;
	}
};
