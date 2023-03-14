// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "NiagaraDataInterface.h"
#include "NiagaraParameterStore.h"
#include "NiagaraDataInterfaceActorComponent.generated.h"

UENUM()
enum class ENDIActorComponentSourceMode : uint8
{
	/**
	The default binding mode first we look for a valid binding on the ActorOrComponentParameter.
	If this it no valid we then look at the SourceActor.
	If these both fail we are bound to nothing.
	*/
	Default,
	/**
	We will first look at the attach parent.
	If this is not valid we fallback to the Default binding mode.
	*/
	AttachParent,
	/**
	We will look for a ULocalPlayer with the provided index.
	If this is not valid we fallback to the Default binding mode.
	*/
	LocalPlayer,
};

/**
Data interface that gives you access to actor & component information.
*/
UCLASS(EditInlineNew, Category = "Actor", meta = (DisplayName = "Actor Component Interface"))
class NIAGARA_API UNiagaraDataInterfaceActorComponent : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER(uint32,		Valid)
		SHADER_PARAMETER(FMatrix44f,	Matrix)
		SHADER_PARAMETER(FQuat4f,		Rotation)
		SHADER_PARAMETER(FVector3f,		Scale)
		SHADER_PARAMETER(FVector3f,		Velocity)
	END_SHADER_PARAMETER_STRUCT();

public:
	/** When this option is disabled, we use the previous frame's data for the skeletal mesh and can often issue the simulation early. This greatly
	reduces overhead and allows the game thread to run faster, but comes at a tradeoff if the dependencies might leave gaps or other visual artifacts.*/
	UPROPERTY(EditAnywhere, Category = "Performance")
	bool bRequireCurrentFrameData = true;

	/** Controls how we find the actor / component we want to bind to. */
	UPROPERTY(EditAnywhere, Category = "ActorComponent")
	ENDIActorComponentSourceMode SourceMode = ENDIActorComponentSourceMode::Default;

	UPROPERTY(EditAnywhere, Category = "ActorComponent", meta=(EditCondition="SourceMode==ENDIActorComponentSourceMode::LocalPlayer"))//, EditConditionHides))
	int32 LocalPlayerIndex = 0;

	/** Optional source actor to use, if the user parameter binding is valid this will be ignored. */
	UPROPERTY(EditAnywhere, Category = "ActorComponent")
	TLazyObjectPtr<AActor> SourceActor;

	/** User parameter binding to use, overrides SourceActor.  Can be set by Blueprint, etc. */
	UPROPERTY(EditAnywhere, Category = "ActorComponent")
	FNiagaraUserParameterBinding ActorOrComponentParameter;

	//UObject Interface
	virtual void PostInitProperties() override;
	//UObject Interface End

	class UActorComponent* ResolveComponent(FNiagaraSystemInstance* SystemInstance, const void* PerInstanceData) const;

	//UNiagaraDataInterface Interface
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }

	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions) override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override;
#endif
	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual int32 PerInstanceDataSize() const override;
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;

	virtual void ProvidePerInstanceDataForRenderThread(void* DataForRenderThread, void* PerInstanceData, const FNiagaraSystemInstanceID& SystemInstance) override;

	virtual bool HasTickGroupPrereqs() const override { return true; }
	virtual ETickingGroup CalculateTickGroup(const void* PerInstanceData) const override;

	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

	virtual bool HasPreSimulateTick() const override { return true; }
	//UNiagaraDataInterface Interface

private:
	void VMGetMatrix(FVectorVMExternalFunctionContext& Context);
	void VMGetTransform(FVectorVMExternalFunctionContext& Context);
	void VMGetPhysicsVelocity(FVectorVMExternalFunctionContext& Context);
};
