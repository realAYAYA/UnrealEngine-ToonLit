// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "VectorVM.h"
#include "NiagaraDataInterfaceTexture.generated.h"

/** Data Interface allowing sampling of a texture */
UCLASS(EditInlineNew, Category = "Texture", meta = (DisplayName = "Texture Sample"))
class NIAGARA_API UNiagaraDataInterfaceTexture : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER(FIntPoint,				TextureSize)
		SHADER_PARAMETER(int32,					MipLevels)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D,	Texture)
		SHADER_PARAMETER_SAMPLER(SamplerState,	TextureSampler)
	END_SHADER_PARAMETER_STRUCT()

public:
	UPROPERTY(EditAnywhere, Category = "Texture")
	TObjectPtr<UTexture> Texture;

	UPROPERTY(EditAnywhere, Category = "Texture", meta = (ToolTip = "When valid the user parameter is used as the texture rather than the one on the data interface"))
	FNiagaraUserParameterBinding TextureUserParameter;

	//UObject Interface
	virtual void PostInitProperties()override;
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return Target==ENiagaraSimTarget::GPUComputeSim; }

	virtual int32 PerInstanceDataSize() const override;
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;

	virtual bool HasPreSimulateTick() const override { return true; }
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	//UNiagaraDataInterface Interface End

	void VMSampleTexture(FVectorVMExternalFunctionContext& Context);
	void VMSamplePseudoVolumeTexture(FVectorVMExternalFunctionContext& Context);
	void VMGetTextureDimensions(FVectorVMExternalFunctionContext& Context);
	void VMGetNumMipLevels(FVectorVMExternalFunctionContext& Context);

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;

	// GPU sim functionality
#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override;
#endif
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	void SetTexture(UTexture* InTexture);

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

protected:
	static const TCHAR* TemplateShaderFilePath;
	static const FName LoadTexture2DName;
	static const FName GatherRedTexture2DName;
	static const FName SampleTexture2DName;
	static const FName SamplePseudoVolumeTextureName;
	static const FName GetTextureDimensionsName;
	static const FName GetNumMipLevelsName;
};
