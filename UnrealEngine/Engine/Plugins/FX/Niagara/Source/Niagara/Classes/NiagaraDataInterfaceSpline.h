// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraDataInterface.h"
#include "NiagaraCommon.h"
#include "VectorVM.h"
#include "NiagaraParameterStore.h"
#include "Components/SplineComponent.h"
#include "Niagara/Private/NiagaraStats.h"
#include "NiagaraDataInterfaceSpline.generated.h"

class UNiagaraDataInterfaceSpline;

/** Proxy data for splines */
struct FNDISpline_InstanceData_RenderThread
{
	~FNDISpline_InstanceData_RenderThread()
	{
		Reset();
	}
	
	FMatrix44f SplineTransform;
	FMatrix44f SplineTransformRotationMat;
	FMatrix44f SplineTransformInverseTranspose;
	FQuat4f SplineTransformRotation;

	FVector3f DefaultUpVector;

	float SplineLength;
	float SplineDistanceStep;
	float InvSplineDistanceStep;
	int32 MaxIndex;
	
	FReadBuffer SplinePositionsLUT;
	FReadBuffer SplineScalesLUT;
	FReadBuffer SplineRotationsLUT;

	void Reset()
	{
		check(IsInRenderingThread());
		DEC_MEMORY_STAT_BY(STAT_NiagaraGPUDataInterfaceMemory, SplinePositionsLUT.NumBytes);
		DEC_MEMORY_STAT_BY(STAT_NiagaraGPUDataInterfaceMemory, SplineScalesLUT.NumBytes);
		DEC_MEMORY_STAT_BY(STAT_NiagaraGPUDataInterfaceMemory, SplineRotationsLUT.NumBytes);
		SplinePositionsLUT.Release();
		SplineScalesLUT.Release();
		SplineRotationsLUT.Release();


		SplineTransform = FMatrix44f::Identity;
		SplineTransformRotationMat = FMatrix44f::Identity;
		SplineTransformInverseTranspose = FMatrix44f::Identity;
		SplineTransformRotation = FQuat4f::Identity;

		DefaultUpVector = FVector3f::ZAxisVector;

		SplineLength = 0.0f;
		SplineDistanceStep = 0.0f;
		InvSplineDistanceStep = 0.0f;
		MaxIndex = INDEX_NONE;
	}
};


struct FNiagaraDataInterfaceProxySpline : public FNiagaraDataInterfaceProxy
{	
	virtual void ConsumePerInstanceDataFromGameThread(void* PerInstanceData, const FNiagaraSystemInstanceID& Instance) override {}
	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override { return 0; }

	// List of proxy data for each system instance
	TMap<FNiagaraSystemInstanceID, FNDISpline_InstanceData_RenderThread> SystemInstancesToProxyData_RT;
};



USTRUCT()
struct FNiagaraDataInterfaceSplineLUT
{
	GENERATED_BODY()
public:	
	UPROPERTY()
	TArray<FVector> Positions;
	UPROPERTY()
	TArray<FVector> Scales;
	UPROPERTY()
	TArray<FQuat> Rotations;
	UPROPERTY()
	float SplineLength = 0;
	UPROPERTY()
	float SplineDistanceStep = 0;
	UPROPERTY()
	float InvSplineDistanceStep = 0;
	UPROPERTY()
	int32 MaxIndex = INDEX_NONE;

	void BuildLUT(const FSplineCurves& SplineCurves, int32 NumSteps);
	void Reset();

	void FindNeighborKeys(float InDistance, int32& PrevKey, int32& NextKey, float& Alpha) const;
};

struct FNDISpline_InstanceData
{
	FNDISpline_InstanceData()
		: CachedUserParam(nullptr)
		, bSyncedGPUCopy(false)
	{}
	//Cached ptr to component we sample from. 
	TWeakObjectPtr<USplineComponent> Component;

	UObject* CachedUserParam;

	/** A binding to the user ptr we're reading the mesh from (if we are). */
	FNiagaraParameterDirectBinding<UObject*> UserParamBinding;


	//Cached ComponentToWorld Rotation
	FQuat TransformQuat;
	//Cached ComponentToWorld.
	FMatrix Transform;
	//InverseTranspose of above for transforming normals/tangents.
	FMatrix TransformInverseTransposed;
	FTransform ComponentTransform;
	FNiagaraLWCConverter LwcConverter;

	FVector DefaultUpVector;
	FSplineCurves SplineCurves;
	FNiagaraDataInterfaceSplineLUT SplineLUT;

	bool bSyncedGPUCopy;
	

	// We cache the version of the current spline curves so that we can reset the curves structure if we're using the LUT
	uint32 SplineCurvesVersion;

	FORCEINLINE_DEBUGGABLE bool ResetRequired(UNiagaraDataInterfaceSpline* Interface, FNiagaraSystemInstance* SystemInstance) const;
	
	template<typename UseLUT>
	float GetSplineLength() const;
	
	bool IsValid() const;



	
	template<typename UseLUT>
	FVector GetLocationAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;	
	template<typename UseLUT>
	FQuat GetQuaternionAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;
	template<typename UseLUT>
	FVector GetDirectionAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;
	template<typename UseLUT>
	FVector GetTangentAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;
	template<typename UseLUT>
	FVector GetUpVectorAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;
	template<typename UseLUT>
	FVector GetRightVectorAtDistanceAlongSpline(float Distance, ESplineCoordinateSpace::Type CoordinateSpace) const;
	
	template<typename UseLUT>
	float FindInputKeyClosestToWorldLocation(const FVector& WorldLocation) const;


	FInterpCurveVector& GetSplinePointsPosition() { return SplineCurves.Position; }

private:
	template<typename UseLUT>
	float ConvertDistanceToKey(float InDistance) const;

	template<typename UseLUT>
	FVector EvaluatePosition(float InKey) const;

	template<typename UseLUT>
	FVector EvaluateScale(float InKey) const;

	template<typename UseLUT>
	FQuat EvaluateRotation(float InKey) const;

	template<typename UseLUT>
	float EvaluateFindNearestPosition(FVector InPosition) const;

	template<typename UseLUT>
	FVector EvaluateDerivativePosition(float InKey) const;
};


/** Data Interface allowing sampling of in-world spline components. Note that this data interface is very experimental. */
UCLASS(EditInlineNew, Category = "Splines", meta = (DisplayName = "Spline"))
class NIAGARA_API UNiagaraDataInterfaceSpline : public UNiagaraDataInterface
{
	GENERATED_UCLASS_BODY()

public:
	/** The source actor from which to sample.  Note that this can only be set when used as a user variable on a component in the world.*/
	UPROPERTY(EditAnywhere, Category = "Spline")
	TObjectPtr<AActor> Source;

	/** Reference to a user parameter if we're reading one. This should  be an Object user parameter that is either a USplineComponent or an AActor containing a USplineComponent. */
	UPROPERTY(EditAnywhere, Category = "Spline")
	FNiagaraUserParameterBinding SplineUserParameter;

	UPROPERTY(EditAnywhere, Category = "Spline")
	bool bUseLUT;

	UPROPERTY(EditAnywhere, Category = "Spline", Meta=(EditCondition="bUseLuT"))
	int32 NumLUTSteps;
	
	//UObject Interface
	virtual void PostInitProperties()override;
	//UObject Interface End

	//UNiagaraDataInterface Interface
	virtual bool InitPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance) override;
	virtual void DestroyPerInstanceData(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance)override;
	virtual bool PerInstanceTick(void* PerInstanceData, FNiagaraSystemInstance* SystemInstance, float DeltaSeconds) override;
	virtual int32 PerInstanceDataSize()const override;
	virtual void GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)override;
	virtual void GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc) override;
	virtual bool Equals(const UNiagaraDataInterface* Other) const override;
	virtual bool CanExecuteOnTarget(ENiagaraSimTarget Target)const override { return true; }
	virtual bool HasPreSimulateTick() const override { return true; }
	//UNiagaraDataInterface Interface End

	template<typename UseLUT, typename TransformHandlerType, typename SplineSampleType>
	void SampleSplinePositionByUnitDistance(FVectorVMExternalFunctionContext& Context);
	template<typename UseLUT, typename TransformHandlerType, typename SplineSampleType>
	void SampleSplineRotationByUnitDistance(FVectorVMExternalFunctionContext& Context);
	template<typename UseLUT, typename TransformHandlerType, typename SplineSampleType>
	void SampleSplineUpVectorByUnitDistance(FVectorVMExternalFunctionContext& Context);
	template<typename UseLUT, typename TransformHandlerType, typename SplineSampleType>
	void SampleSplineRightVectorByUnitDistance(FVectorVMExternalFunctionContext& Context);
	template<typename UseLUT, typename TransformHandlerType, typename SplineSampleType>
	void SampleSplineDirectionByUnitDistance(FVectorVMExternalFunctionContext& Context);
	template<typename UseLUT, typename TransformHandlerType, typename SplineSampleType>
	void SampleSplineTangentByUnitDistance(FVectorVMExternalFunctionContext& Context);
	template<typename UseLUT>
	void FindClosestUnitDistanceFromPositionWS(FVectorVMExternalFunctionContext& Context);
	
	void GetLocalToWorld(FVectorVMExternalFunctionContext& Context);
	void GetLocalToWorldInverseTransposed(FVectorVMExternalFunctionContext& Context);

	void VMGetSplineLength(FVectorVMExternalFunctionContext& Context);

	virtual int32 PerInstanceDataPassedToRenderThreadSize() const override
	{
		return 0;
	}

protected:
	virtual bool CopyToInternal(UNiagaraDataInterface* Destination) const override;

#if WITH_EDITORONLY_DATA
	virtual bool UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature) override;
	virtual void GetCommonHLSL(FString& OutHLSL) override;
	virtual void GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL) override;

	virtual bool GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL) override;

	virtual bool AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const override;
#endif
	virtual bool UseLegacyShaderBindings() const override { return false; }
	virtual void BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const override;
	virtual void SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const override;

private:
	
	void WriteTransform(const FMatrix& ToWrite, FVectorVMExternalFunctionContext& Context);

	TMap<FNiagaraSystemInstanceID, FNDISpline_InstanceData*> SystemInstancesToProxyData_GT;
};
