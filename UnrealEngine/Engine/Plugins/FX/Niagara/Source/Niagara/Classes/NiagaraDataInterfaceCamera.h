// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"
#include "NiagaraShared.h"
#include "NiagaraDataInterface.h"
#include "NiagaraDataInterfaceCamera.generated.h"

struct FDistanceData
{
	FNiagaraID ParticleID;
	double DistanceSquared;
};

struct FCameraDataInterface_InstanceData
{
	FNiagaraPosition CameraLocation = FVector::ZeroVector;
	FRotator CameraRotation = FRotator::ZeroRotator;
	float CameraFOV = 0.0f;
	FNiagaraLWCConverter LWCConverter;

	TQueue<FDistanceData, EQueueMode::Mpsc> DistanceSortQueue;
	TArray<FDistanceData> ParticlesSortedByDistance;	
};

UCLASS(EditInlineNew, Category = "Camera", meta = (DisplayName = "Camera Query"))
class NIAGARA_API UNiagaraDataInterfaceCamera : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

	BEGIN_SHADER_PARAMETER_STRUCT(FShaderParameters, )
		SHADER_PARAMETER(FVector3f, SystemLWCTile)
	END_SHADER_PARAMETER_STRUCT();

public:
	/** This is used to determine which camera position to query for cpu emitters. If no valid index is supplied, the first controller is used as camera reference. */
	UPROPERTY(EditAnywhere, Category = "Camera")
	int32 PlayerControllerIndex = 0;


	/** When this option is disabled, we use the previous frame's data for the camera and issue the simulation early. This greatly
	reduces overhead and allows the game thread to run faster, but comes at a tradeoff if the dependencies might leave gaps or other visual artifacts.*/
	UPROPERTY(EditAnywhere, Category = "Performance")
	bool bRequireCurrentFrameData = true;

	//UObject Interface
	virtual void PostInitProperties() override;
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual int32 PerInstanceDataSize() const override { return sizeof(FCameraDataInterface_InstanceData); }
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
#if WITH_EDITORONLY_DATA
	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;
#endif
	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target) const override { return true; }
	virtual bool HasTickGroupPrereqs() const override { return true; }
	virtual ETickingGroup CalculateTickGroup(const void* PerInstanceData) const override;
	virtual bool RequiresEarlyViewData() const override { return true; }
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool HasPreSimulateTick() const override { return true; }
#if WITH_EDITOR	
	virtual void GetFeedback(UNiagaraSystem* Asset, UNiagaraComponent* Component, TArray<FNiagaraDataInterfaceError>& OutErrors,
        TArray<FNiagaraDataInterfaceFeedback>& Warnings, TArray<FNiagaraDataInterfaceFeedback>& Info) override;
#endif
	//UNiagaraDataInterface Interface

	void CalculateParticleDistances(FVectorVMExternalFunctionContext& Context);
	void GetClosestParticles(FVectorVMExternalFunctionContext& Context);
	void GetCameraFOV(FVectorVMExternalFunctionContext& Context);
	void GetCameraProperties(FVectorVMExternalFunctionContext& Context);

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;
private:
	static const TCHAR* TemplateShaderFilePath;
	static const FName CalculateDistancesName;
	static const FName QueryClosestName;
	static const FName GetViewPropertiesName;
	static const FName GetClipSpaceTransformsName;
	static const FName GetViewSpaceTransformsName;
	static const FName GetCameraPropertiesName;
	static const FName GetFieldOfViewName;
	static const FName GetTAAJitterName;
};

struct FNiagaraDataIntefaceProxyCameraQuery : public FNiagaraDataInterfaceProxy
{
	// There's nothing in this proxy.
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return 0;
	}
};
