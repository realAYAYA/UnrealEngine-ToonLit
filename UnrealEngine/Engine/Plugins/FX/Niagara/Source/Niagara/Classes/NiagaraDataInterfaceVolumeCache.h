// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "VectorVM.h"
#include "Engine/VolumeTexture.h"
#include "VolumeCache.h"

#include "NiagaraDataInterfaceVolumeCache.generated.h"

struct FVolumeCacheInstanceData_GameThread;

UCLASS(EditInlineNew, Category = "Grid",  meta = (DisplayName = "Volume Cache"))
class NIAGARA_API UNiagaraDataInterfaceVolumeCache : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER(FVector3f, TextureSize)
		SHADER_PARAMETER_TEXTURE(Texture3D, Texture)
		SHADER_PARAMETER_SAMPLER(SamplerState, TextureSampler)
	END_SHADER_PARAMETER_STRUCT()

public:
		
	UPROPERTY(EditAnywhere, Category = "File")
	TObjectPtr<UVolumeCache> VolumeCache;

	virtual void PostInitProperties() override;

	// UNiagaraDataInterface Interface Begin
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual int32 PerInstanceDataSize()const override;
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;	
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	virtual bool HasPreSimulateTick() const override { return true; }
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	
	virtual void ProvidePerInstanceDataForRenderThread(void* InDataFromGT, void* InInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override;

#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
#endif

	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	void SetFrame(FVectorVMExternalFunctionContext& Context);
	void ReadFile(FVectorVMExternalFunctionContext& Context);	
	void GetNumCells(FVectorVMExternalFunctionContext& Context);

	FString GetAssetPath(FString PathFormat, int32 FrameIndex) const;

protected:
	//~ UNiagaraDataInterface interface
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
	//~ UNiagaraDataInterface interface END

	TMap<FNiagaraSystemInstanceID, FVolumeCacheInstanceData_GameThread*> SystemInstancesToProxyData_GT;	

	static const FName SetFrameName;
	static const FName ReadFileName;
	static const FName GetNumCellsName;
	static const FName IndexToUnitName;
	static const FName SampleCurrentFrameValueName;
	static const FName GetCurrentFrameValue;
	static const FName GetCurrentFrameNumCells;
	static const TCHAR* TemplateShaderFilePath;
};