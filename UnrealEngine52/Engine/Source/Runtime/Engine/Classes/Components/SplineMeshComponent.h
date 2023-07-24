// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Components/StaticMeshComponent.h"

#include "SplineMeshComponent.generated.h"

class FPrimitiveSceneProxy;
class ULightComponent;
struct FNavigableGeometryExport;
class UBodySetup;

UENUM(BlueprintType)
namespace ESplineMeshAxis
{
	enum Type : int
	{
		X,
		Y,
		Z,
	};
}

/** 
 * Structure that holds info about spline, passed to renderer to deform UStaticMesh.
 * Also used by Lightmass, so be sure to update Lightmass::FSplineMeshParams and the static lighting code if this changes!
 */
USTRUCT()
struct FSplineMeshParams
{
	GENERATED_USTRUCT_BODY()

	/** Start location of spline, in component space. */
	UPROPERTY(EditAnywhere, Category=SplineMesh)
	FVector StartPos;

	/** Start tangent of spline, in component space. */
	UPROPERTY(EditAnywhere, Category=SplineMesh)
	FVector StartTangent;

	/** X and Y scale applied to mesh at start of spline. */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	FVector2D StartScale;

	/** Roll around spline applied at start */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	float StartRoll;

	/** Starting offset of the mesh from the spline, in component space. */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	FVector2D StartOffset;

	/** End location of spline, in component space. */
	UPROPERTY(EditAnywhere, Category=SplineMesh)
	FVector EndPos;

	/** X and Y scale applied to mesh at end of spline. */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	FVector2D EndScale;

	/** End tangent of spline, in component space. */
	UPROPERTY(EditAnywhere, Category = SplineMesh)
	FVector EndTangent;

	/** Roll around spline applied at end. */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	float EndRoll;

	/** Ending offset of the mesh from the spline, in component space. */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	FVector2D EndOffset;


	FSplineMeshParams()
		: StartPos(ForceInit)
		, StartTangent(ForceInit)
		, StartScale(ForceInit)
		, StartRoll(0)
		, StartOffset(ForceInit)
		, EndPos(ForceInit)
		, EndScale(ForceInit)
		, EndTangent(ForceInit)
		, EndRoll(0)
		, EndOffset(ForceInit)
	{
	}

};

/** 
 *	A Spline Mesh Component is a derivation of a Static Mesh Component which can be deformed using a spline. Only a start and end position (and tangent) can be specified.  
 *	@see https://docs.unrealengine.com/latest/INT/Resources/ContentExamples/Blueprint_Splines
 */
UCLASS(ClassGroup=Rendering, hidecategories=(Physics), meta=(BlueprintSpawnableComponent))
class ENGINE_API USplineMeshComponent : public UStaticMeshComponent, public IInterface_CollisionDataProvider
{
	GENERATED_UCLASS_BODY()

	/** Spline that is used to deform mesh */
	UPROPERTY(EditAnywhere, Category=SplineMesh, meta=(ShowOnlyInnerProperties))
	FSplineMeshParams SplineParams;

	/** Axis (in component space) that is used to determine X axis for co-ordinates along spline */
	UPROPERTY(EditAnywhere, Category=SplineMesh)
	FVector SplineUpDir;

	/** Minimum coordinate along the spline forward axis which corresponds to start of spline. If set to 0.0, will use bounding box to determine bounds */
	UPROPERTY(EditAnywhere, Category = SplineMesh, AdvancedDisplay)
	float SplineBoundaryMin;

	// Used to automatically trigger rebuild of collision data
	UPROPERTY()
	FGuid CachedMeshBodySetupGuid;

	// Physics data.
	UPROPERTY()
	TObjectPtr<UBodySetup> BodySetup;

	/** Maximum coordinate along the spline forward axis which corresponds to end of spline. If set to 0.0, will use bounding box to determine bounds */
	UPROPERTY(EditAnywhere, Category = SplineMesh, AdvancedDisplay)
	float SplineBoundaryMax;

	/** If true, spline mesh properties - StartPos, EndPos, StartTangent and EndTangent- may be edited per instance in the level viewport. Otherwise, the spline mesh should be initialized in the construction script. */
	UPROPERTY(EditDefaultsOnly, Category = Spline)
	uint8 bAllowSplineEditingPerInstance:1;

	/** If true, will use smooth interpolation (ease in/out) for Scale, Roll, and Offset along this section of spline. If false, uses linear */
	UPROPERTY(EditAnywhere, Category=SplineMesh, AdvancedDisplay)
	uint8 bSmoothInterpRollScale:1;

	// Indicates that the mesh needs updating
	UPROPERTY(transient)
	uint8 bMeshDirty : 1;

	/** Chooses the forward axis for the spline mesh orientation */
	UPROPERTY(EditAnywhere, Category=SplineMesh)
	TEnumAsByte<ESplineMeshAxis::Type> ForwardAxis;

	/** 
	 * The max draw distance to use in the main pass when also rendering to a runtime virtual texture. 
	 * This is only exposed to the user through the same setting on ULandscapeSplineSegment. 
	 */
	UPROPERTY()
	float VirtualTextureMainPassMaxDrawDistance = 0.f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(transient)
	uint8 bSelected:1;
#endif

private:
	/** Indicates that we will never use convex or trimesh shapes. This is an optimization to skip checking for binary data. */
	/**
	* TODO Chaos this is to opt out of CreatePhysicsMeshes for certain meshes
	* Better long term mesh is to not call CreatePhysicsMeshes until it is known there is a mesh instance that needs it.
	*/
	UPROPERTY(EditAnywhere, Getter, Setter, Category = Collision)
	uint8 bNeverNeedsCookedCollisionData:1;

public:
	//Begin UObject Interface
	virtual void Serialize(FArchive& Ar) override;
#if WITH_EDITOR
	virtual bool IsEditorOnly() const override;
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//End UObject Interface

	//~ Begin UActorComponent Interface.
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
	//~ End UActorComponent Interface.

	void ApplyComponentInstanceData(struct FSplineMeshInstanceData* ComponentInstanceData);

	//Begin USceneComponent Interface
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual FTransform GetSocketTransform(FName InSocketName, ERelativeTransformSpace TransformSpace = RTS_World) const override;
	//End USceneComponent Interface

	//Begin UPrimitiveComponent Interface
protected:
	virtual void OnCreatePhysicsState() override;
public:
	virtual class UBodySetup* GetBodySetup() override;
#if WITH_EDITOR
	virtual bool ShouldRenderSelected() const override
	{
		return Super::ShouldRenderSelected() || bSelected;
	}
#endif
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
	virtual float GetVirtualTextureMainPassMaxDrawDistance() const override { return VirtualTextureMainPassMaxDrawDistance; }
	//End UPrimitiveComponent Interface

	//Begin UStaticMeshComponent Interface
	virtual class FStaticMeshStaticLightingMesh* AllocateStaticLightingMesh(int32 LODIndex, const TArray<ULightComponent*>& InRelevantLights) override;
	//End UStaticMeshComponent Interface

	//~ Begin Interface_CollisionDataProvider Interface
	virtual bool GetPhysicsTriMeshData(struct FTriMeshCollisionData* CollisionData, bool InUseAllTriData) override;
	virtual bool ContainsPhysicsTriMeshData(bool InUseAllTriData) const override;
	virtual bool WantsNegXTriMesh() override { return false; }
	virtual void GetMeshId(FString& OutMeshId) override;
	virtual bool GetTriMeshSizeEstimates(struct FTriMeshCollisionDataEstimates& OutTriMeshEstimates, bool bInUseAllTriData) const override;
	//~ End Interface_CollisionDataProvider Interface

	/** Determines the mesh proxy values for SplineMeshScaleZ and SplineMeshMinZ*/
	void CalculateScaleZAndMinZ(float& OutScaleZ, float& OutMinZ) const;

	/** Called to notify render thread and possibly collision of a change in spline params or mesh */
	void UpdateRenderStateAndCollision();

	/** Update the collision and render state on the spline mesh following changes to its geometry */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void UpdateMesh();

	/** Same as UpdateMesh, but does not wait until the end of frame and can be used in non-game threads */
	void UpdateMesh_Concurrent();

	/** Get the start position of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector GetStartPosition() const;

	/** Set the start position of spline in local space */
	UFUNCTION(BlueprintCallable, Category=SplineMesh)
	void SetStartPosition(FVector StartPos, bool bUpdateMesh = true);

	/** Get the start tangent vector of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector GetStartTangent() const;

	/** Set the start tangent vector of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetStartTangent(FVector StartTangent, bool bUpdateMesh = true);

	/** Get the end position of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector GetEndPosition() const;

	/** Set the end position of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetEndPosition(FVector EndPos, bool bUpdateMesh = true);

	/** Get the end tangent vector of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector GetEndTangent() const;

	/** Set the end tangent vector of spline in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetEndTangent(FVector EndTangent, bool bUpdateMesh = true);

	/** Set the start and end, position and tangent, all in local space */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetStartAndEnd(FVector StartPos, FVector StartTangent, FVector EndPos, FVector EndTangent, bool bUpdateMesh = true);

	/** Get the start scaling */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector2D GetStartScale() const;

	/** Set the start scaling */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetStartScale(FVector2D StartScale = FVector2D(1,1), bool bUpdateMesh = true);

	/** Get the start roll */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	float GetStartRoll() const;

	/** Set the start roll */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetStartRoll(float StartRoll, bool bUpdateMesh = true);

	/** Get the start offset */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector2D GetStartOffset() const;

	/** Set the start offset */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetStartOffset(FVector2D StartOffset, bool bUpdateMesh = true);

	/** Get the end scaling */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector2D GetEndScale() const;

	/** Set the end scaling */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetEndScale(FVector2D EndScale = FVector2D(1,1), bool bUpdateMesh = true);

	/** Get the end roll */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	float GetEndRoll() const;

	/** Set the end roll */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetEndRoll(float EndRoll, bool bUpdateMesh = true);

	/** Get the end offset */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector2D GetEndOffset() const;

	/** Set the end offset */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetEndOffset(FVector2D EndOffset, bool bUpdateMesh = true);

	/** Get the forward axis */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	ESplineMeshAxis::Type GetForwardAxis() const;

	/** Set the forward axis */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetForwardAxis(ESplineMeshAxis::Type InForwardAxis, bool bUpdateMesh = true);

	/** Get the spline up direction */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	FVector GetSplineUpDir() const;

	/** Set the spline up direction */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetSplineUpDir(const FVector& InSplineUpDir, bool bUpdateMesh = true);

	/** Get the boundary min */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	float GetBoundaryMin() const;

	/** Set the boundary min */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetBoundaryMin(float InBoundaryMin, bool bUpdateMesh = true);

	/** Get the boundary max */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	float GetBoundaryMax() const;

	/** Set the boundary max */
	UFUNCTION(BlueprintCallable, Category = SplineMesh)
	void SetBoundaryMax(float InBoundaryMax, bool bUpdateMesh = true);

	/** Setter for bNeverNeedsCookedCollisionData */
	void SetbNeverNeedsCookedCollisionData(bool bInValue); 

	/** Getter for bNeverNeedsCookedCollisionData */
	bool GetbNeverNeedsCookedCollisionData() const { return bNeverNeedsCookedCollisionData; }

	// Destroys the body setup, used to clear collision if the mesh goes missing
	void DestroyBodySetup();
	// Builds collision for the spline mesh (if collision is enabled)
	void RecreateCollision();

	/**
	 * Calculates the spline transform, including roll, scale, and offset along the spline at a specified distance
	 */
	FTransform CalcSliceTransform(const float DistanceAlong) const;

	/**
	 * Calculates the spline transform, including roll, scale, and offset along the spline at a specified alpha interpolation parameter along the spline
	 * @Note:  This is mirrored to Lightmass::CalcSliceTransform() and LocalVertexShader.usf.  If you update one of these, please update them all!
	 */
	FTransform CalcSliceTransformAtSplineOffset(const float Alpha) const;

	UE_DEPRECATED(5.2, "Use GetAxisValueRef() instead.")
	static const double& GetAxisValue(const FVector3d& InVector, ESplineMeshAxis::Type InAxis) { return GetAxisValueRef(InVector, InAxis); }
	UE_DEPRECATED(5.2, "Use GetAxisValueRef() instead.")
	static double& GetAxisValue(FVector3d& InVector, ESplineMeshAxis::Type InAxis) { return GetAxisValueRef(InVector, InAxis); }

	UE_DEPRECATED(5.2, "Use GetAxisValueRef() instead.")
	static const float& GetAxisValue(const FVector3f& InVector, ESplineMeshAxis::Type InAxis) { return GetAxisValueRef(InVector, InAxis); }
	UE_DEPRECATED(5.2, "Use GetAxisValueRef() instead.")
	static float& GetAxisValue(FVector3f& InVector, ESplineMeshAxis::Type InAxis) { return GetAxisValueRef(InVector, InAxis); }

	inline static const double& GetAxisValueRef(const FVector3d& InVector, ESplineMeshAxis::Type InAxis);
	inline static double& GetAxisValueRef(FVector3d& InVector, ESplineMeshAxis::Type InAxis);

	inline static const float& GetAxisValueRef(const FVector3f& InVector, ESplineMeshAxis::Type InAxis);
	inline static float& GetAxisValueRef(FVector3f& InVector, ESplineMeshAxis::Type InAxis);

	/** Returns a vector which, when componentwise-multiplied by another vector, will zero all the components not corresponding to the supplied ESplineMeshAxis */
	inline static FVector GetAxisMask(ESplineMeshAxis::Type InAxis);

	virtual float GetTextureStreamingTransformScale() const override;

	virtual void CollectPSOPrecacheData(const FPSOPrecacheParams& BasePrecachePSOParams, FComponentPSOPrecacheParamsList& OutParams) override;

private:
	void UpdateRenderStateAndCollision_Internal(bool bConcurrent);
};

/** Used to store spline mesh data during RerunConstructionScripts */
USTRUCT()
struct FSplineMeshInstanceData : public FStaticMeshComponentInstanceData
{
	GENERATED_BODY()
public:
	FSplineMeshInstanceData() = default;
	explicit FSplineMeshInstanceData(const USplineMeshComponent* SourceComponent);

	virtual ~FSplineMeshInstanceData() override = default;

	virtual bool ContainsData() const override
	{
		return true;
	}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		Super::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<USplineMeshComponent>(Component)->ApplyComponentInstanceData(this);
	}

	UPROPERTY()
	FVector StartPos = FVector::ZeroVector;

	UPROPERTY()
	FVector EndPos = FVector::ZeroVector;

	UPROPERTY()
	FVector StartTangent = FVector::ZeroVector;

	UPROPERTY()
	FVector EndTangent = FVector::ZeroVector;
};

const double& USplineMeshComponent::GetAxisValueRef(const FVector3d& InVector, ESplineMeshAxis::Type InAxis)
{
	switch (InAxis)
	{
	case ESplineMeshAxis::X:
		return InVector.X;
	case ESplineMeshAxis::Y:
		return InVector.Y;
	case ESplineMeshAxis::Z:
		return InVector.Z;
	default:
		check(0);
		return InVector.Z;
	}
}

double& USplineMeshComponent::GetAxisValueRef(FVector3d& InVector, ESplineMeshAxis::Type InAxis)
{
	switch (InAxis)
	{
	case ESplineMeshAxis::X:
		return InVector.X;
	case ESplineMeshAxis::Y:
		return InVector.Y;
	case ESplineMeshAxis::Z:
		return InVector.Z;
	default:
		check(0);
		return InVector.Z;
	}
}


const float& USplineMeshComponent::GetAxisValueRef(const FVector3f& InVector, ESplineMeshAxis::Type InAxis)
{
	switch (InAxis)
	{
	case ESplineMeshAxis::X:
		return InVector.X;
	case ESplineMeshAxis::Y:
		return InVector.Y;
	case ESplineMeshAxis::Z:
		return InVector.Z;
	default:
		check(0);
		return InVector.Z;
	}
}

float& USplineMeshComponent::GetAxisValueRef(FVector3f& InVector, ESplineMeshAxis::Type InAxis)
{
	switch (InAxis)
	{
	case ESplineMeshAxis::X:
		return InVector.X;
	case ESplineMeshAxis::Y:
		return InVector.Y;
	case ESplineMeshAxis::Z:
		return InVector.Z;
	default:
		check(0);
		return InVector.Z;
	}
}

FVector USplineMeshComponent::GetAxisMask(ESplineMeshAxis::Type InAxis)
{
	switch (InAxis)
	{
	case ESplineMeshAxis::X:
		return FVector(0, 1, 1);
	case ESplineMeshAxis::Y:
		return FVector(1, 0, 1);
	case ESplineMeshAxis::Z:
		return FVector(1, 1, 0);
	default:
		check(0);
		return FVector::ZeroVector;
	}
}
