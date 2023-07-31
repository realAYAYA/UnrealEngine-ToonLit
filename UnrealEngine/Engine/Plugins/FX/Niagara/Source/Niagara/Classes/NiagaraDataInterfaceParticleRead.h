// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataInterfaceRW.h"
#include "NiagaraCommon.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraDataInterfaceParticleRead.generated.h"

UCLASS(EditInlineNew, Category = "ParticleRead", meta = (DisplayName = "Particle Attribute Reader"))
class NIAGARA_API UNiagaraDataInterfaceParticleRead : public UNiagaraDataInterfaceRWBase
{
	GENERATED_UCLASS_BODY()

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER(int,				NumSpawnedParticles)
		SHADER_PARAMETER(int,				SpawnedParticlesAcquireTag)
		SHADER_PARAMETER(uint32,			InstanceCountOffset)
		SHADER_PARAMETER(uint32,			ParticleStrideFloat)
		SHADER_PARAMETER(uint32,			ParticleStrideInt)
		SHADER_PARAMETER(uint32,			ParticleStrideHalf)
		SHADER_PARAMETER(int,				AcquireTagRegisterIndex)
		SHADER_PARAMETER_SRV(Buffer<int>,	SpawnedIDsBuffer)
		SHADER_PARAMETER_SRV(Buffer<int>,	IDToIndexTable)
		SHADER_PARAMETER_SRV(Buffer<float>,	InputFloatBuffer)
		SHADER_PARAMETER_SRV(Buffer<int>,	InputIntBuffer)
		SHADER_PARAMETER_SRV(Buffer<half>,	InputHalfBuffer)
	END_SHADER_PARAMETER_STRUCT()

public:
	UPROPERTY(EditAnywhere, Category = "ParticleRead")
	FString EmitterName;

	//UObject Interface
	virtual void PostInitProperties()override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent);
#endif
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual int32 PerInstanceDataSize() const override;
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc) override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
#endif
	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;
	virtual FNiagaraDataInterfaceParametersCS* CreateShaderStorage(const FNiagaraDataInterfaceGPUParamInfo& ParameterInfo, const FShaderParameterMap& ParameterMap) const override;
	virtual const FTypeLayoutDesc* GetShaderStorageType() const override;

	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override;
#if WITH_EDITOR	
	virtual void GetFeedback(UNiagaraSystem* Asset, UNiagaraComponent* Component, TArray<FNiagaraDataInterfaceError>& OutErrors, TArray<FNiagaraDataInterfaceFeedback>& Warnings, TArray<FNiagaraDataInterfaceFeedback>& Info) override;
#endif
#if WITH_EDITORONLY_DATA
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override;
#endif
	virtual void GetEmitterDependencies(UNiagaraSystem* Asset, TArray<FVersionedNiagaraEmitter>& Dependencies) const override;
	virtual bool ReadsEmitterParticleData(const FString& EmitterName) const override;

	virtual bool HasInternalAttributeReads(const UNiagaraEmitter* OwnerEmitter, const UNiagaraEmitter* Provider) const override;
	//UNiagaraDataInterface Interface End

	void VMGetLocalSpace(FVectorVMExternalFunctionContext& Context);
	void GetNumSpawnedParticles(FVectorVMExternalFunctionContext& Context);
	void GetSpawnedIDAtIndex(FVectorVMExternalFunctionContext& Context);
	void GetNumParticles(FVectorVMExternalFunctionContext& Context);
	void GetParticleIndex(FVectorVMExternalFunctionContext& Context);
	void GetParticleIndexFromIDTable(FVectorVMExternalFunctionContext& Context);
	void ReadInt(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadBool(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadFloat(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadVector2(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadVector3(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadVector4(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadColor(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadPosition(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadQuat(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadID(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadIntByIndex(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadBoolByIndex(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadFloatByIndex(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadVector2ByIndex(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadVector3ByIndex(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadVector4ByIndex(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadPositionByIndex(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadColorByIndex(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadQuatByIndex(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);
	void ReadIDByIndex(FVectorVMExternalFunctionContext& Context, FName AttributeToRead);

protected:
	void GetPersistentIDFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions);
	void GetIndexFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions);
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
};