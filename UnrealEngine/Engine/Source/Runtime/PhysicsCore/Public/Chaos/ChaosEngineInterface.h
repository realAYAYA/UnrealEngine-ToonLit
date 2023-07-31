// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Chaos/Declares.h"
#include "PhysicsInterfaceDeclaresCore.h"
#include "PhysicsInterfaceWrapperShared.h"
#include "PhysicsInterfaceTypesCore.h"
#include "PhysicsProxy/SingleParticlePhysicsProxyFwd.h"
#include "ChaosEngineInterface.generated.h"

//NOTE: Do not include Chaos headers directly as it means recompiling all of engine. This should be reworked to avoid allocations


/** Types of surfaces in the game, used by Physical Materials */
UENUM(BlueprintType)
enum EPhysicalSurface : int
{
	SurfaceType_Default UMETA(DisplayName="Default"),
	SurfaceType1 UMETA(Hidden),
	SurfaceType2 UMETA(Hidden),
	SurfaceType3 UMETA(Hidden),
	SurfaceType4 UMETA(Hidden),
	SurfaceType5 UMETA(Hidden),
	SurfaceType6 UMETA(Hidden),
	SurfaceType7 UMETA(Hidden),
	SurfaceType8 UMETA(Hidden),
	SurfaceType9 UMETA(Hidden),
	SurfaceType10 UMETA(Hidden),
	SurfaceType11 UMETA(Hidden),
	SurfaceType12 UMETA(Hidden),
	SurfaceType13 UMETA(Hidden),
	SurfaceType14 UMETA(Hidden),
	SurfaceType15 UMETA(Hidden),
	SurfaceType16 UMETA(Hidden),
	SurfaceType17 UMETA(Hidden),
	SurfaceType18 UMETA(Hidden),
	SurfaceType19 UMETA(Hidden),
	SurfaceType20 UMETA(Hidden),
	SurfaceType21 UMETA(Hidden),
	SurfaceType22 UMETA(Hidden),
	SurfaceType23 UMETA(Hidden),
	SurfaceType24 UMETA(Hidden),
	SurfaceType25 UMETA(Hidden),
	SurfaceType26 UMETA(Hidden),
	SurfaceType27 UMETA(Hidden),
	SurfaceType28 UMETA(Hidden),
	SurfaceType29 UMETA(Hidden),
	SurfaceType30 UMETA(Hidden),
	SurfaceType31 UMETA(Hidden),
	SurfaceType32 UMETA(Hidden),
	SurfaceType33 UMETA(Hidden),
	SurfaceType34 UMETA(Hidden),
	SurfaceType35 UMETA(Hidden),
	SurfaceType36 UMETA(Hidden),
	SurfaceType37 UMETA(Hidden),
	SurfaceType38 UMETA(Hidden),
	SurfaceType39 UMETA(Hidden),
	SurfaceType40 UMETA(Hidden),
	SurfaceType41 UMETA(Hidden),
	SurfaceType42 UMETA(Hidden),
	SurfaceType43 UMETA(Hidden),
	SurfaceType44 UMETA(Hidden),
	SurfaceType45 UMETA(Hidden),
	SurfaceType46 UMETA(Hidden),
	SurfaceType47 UMETA(Hidden),
	SurfaceType48 UMETA(Hidden),
	SurfaceType49 UMETA(Hidden),
	SurfaceType50 UMETA(Hidden),
	SurfaceType51 UMETA(Hidden),
	SurfaceType52 UMETA(Hidden),
	SurfaceType53 UMETA(Hidden),
	SurfaceType54 UMETA(Hidden),
	SurfaceType55 UMETA(Hidden),
	SurfaceType56 UMETA(Hidden),
	SurfaceType57 UMETA(Hidden),
	SurfaceType58 UMETA(Hidden),
	SurfaceType59 UMETA(Hidden),
	SurfaceType60 UMETA(Hidden),
	SurfaceType61 UMETA(Hidden),
	SurfaceType62 UMETA(Hidden),
	SurfaceType_Max UMETA(Hidden)
};

/** Enum for controlling the falloff of strength of a radial impulse as a function of distance from Origin. */
UENUM()
enum ERadialImpulseFalloff
{
	/** Impulse is a constant strength, up to the limit of its range. */
	RIF_Constant,
	/** Impulse should get linearly weaker the further from origin. */
	RIF_Linear,
	RIF_MAX,
};

/** Presets of values used in considering when put this body to sleep. */
UENUM()
enum class ESleepFamily: uint8
{
	/** Engine defaults. */
	Normal,
	/** A family of values with a lower sleep threshold; good for slower pendulum-like physics. */
	Sensitive,
	/** Specify your own sleep threshold multiplier */
	Custom,
};

/** Specifies angular degrees of freedom */
UENUM()
enum EAngularConstraintMotion
{
	/** No constraint against this axis. */
	ACM_Free		UMETA(DisplayName="Free"),
	/** Limited freedom along this axis. */
	ACM_Limited		UMETA(DisplayName="Limited"),
	/** Fully constraint against this axis. */
	ACM_Locked		UMETA(DisplayName="Locked"),

	ACM_MAX,
};

/** Enum to indicate which context frame we use for physical constraints */
UENUM()
namespace EConstraintFrame
{
enum Type
{
	Frame1,
	Frame2
};
}


namespace PhysicsInterfaceTypes
{
	enum class ELimitAxis : uint8
	{
		X,
		Y,
		Z,
		Twist,
		Swing1,
		Swing2
	};

	enum class EDriveType : uint8
	{
		X,
		Y,
		Z,
		Swing,
		Twist,
		Slerp
	};

	/**
	* Default number of inlined elements used in FInlineShapeArray.
	* Increase if for instance character meshes use more than this number of physics bodies and are involved in many queries.
	*/
	enum { NumInlinedPxShapeElements = 32 };

	/** Array that is intended for use when fetching shapes from a rigid body. */
	typedef TArray<FPhysicsShapeHandle, TInlineAllocator<NumInlinedPxShapeElements>> FInlineShapeArray;
}


// LINEAR CCPT
UENUM()
enum EConstraintPlasticityType
{
	/** */
	CCPT_Free	UMETA(DisplayName = "Free"),
	/** */
	CCPT_Shrink	UMETA(DisplayName = "Shirnk"),
	/** */
	CCPT_Grow	UMETA(DisplayName = "Grow"),

	CCPT_MAX,
};

// LINEAR DOF
UENUM()
enum ELinearConstraintMotion
{
	/** No constraint against this axis. */
	LCM_Free	UMETA(DisplayName="Free"),
	/** Limited freedom along this axis. */
	LCM_Limited UMETA(DisplayName="Limited"),
	/** Fully constraint against this axis. */
	LCM_Locked UMETA(DisplayName="Locked"),

	LCM_MAX,
};

/** This filter allows us to refine queries (channel, object) with an additional level of ignore by tagging entire classes of objects (e.g. "Red team", "Blue team")
    If(QueryIgnoreMask & ShapeFilter != 0) filter out */
typedef uint8 FMaskFilter;

namespace Chaos
{
	class FBVHParticles;

	template <typename T, int>
	class TPBDRigidParticles;

	class FPerParticleGravity;

	class FConvex;
	class FCapsule;

	template <typename T, int>
	class TAABB;
	using FAABB3 = TAABB<FReal, 3>;

	template <typename T, int>
	class TBox;

	template <typename T, int>
	class TSphere;

	class FConstraintBase;
	class FJointConstraint;
	class FSuspensionConstraint;

	class FTriangleMeshImplicitObject;
}

struct FCollisionShape;
class UPhysicalMaterial;

class PHYSICSCORE_API FPhysicsAggregateReference_Chaos
{
public:
	bool IsValid() const { return false; }
};

class PHYSICSCORE_API FPhysicsConstraintReference_Chaos
{
public:
	FPhysicsConstraintReference_Chaos() { Reset(); }
	void Reset() { Constraint = nullptr; }

	bool IsValid() const;



	Chaos::FConstraintBase* operator->() { return Constraint; }
	const Chaos::FConstraintBase* operator->() const { return Constraint; }

	Chaos::FConstraintBase* Constraint;
};

class PHYSICSCORE_API FPhysicsShapeReference_Chaos
{
public:
	
	FPhysicsShapeReference_Chaos()
		: Shape(nullptr), ActorRef() { }
	FPhysicsShapeReference_Chaos(Chaos::FPerShapeData* ShapeIn, const FPhysicsActorHandle& ActorRefIn)
		: Shape(ShapeIn), ActorRef(ActorRefIn) { }
	FPhysicsShapeReference_Chaos(const FPhysicsShapeReference_Chaos& Other)
		: Shape(Other.Shape)
		, ActorRef(Other.ActorRef){}

	bool IsValid() const { return (Shape != nullptr); }
	bool Equals(const FPhysicsShapeReference_Chaos& Other) const { return Shape == Other.Shape; }
    bool operator==(const FPhysicsShapeReference_Chaos& Other) const { return Equals(Other); }
	const Chaos::FImplicitObject& GetGeometry() const;

	Chaos::FPerShapeData* Shape;
    FPhysicsActorHandle ActorRef;
};

class PHYSICSCORE_API FPhysicsShapeAdapter_Chaos
{
public:
	FPhysicsShapeAdapter_Chaos(const FQuat& Rot, const FCollisionShape& CollisionShape);
	~FPhysicsShapeAdapter_Chaos();

	const FPhysicsGeometry& GetGeometry() const;
	FTransform GetGeomPose(const FVector& Pos) const;
	const FQuat& GetGeomOrientation() const;

private:
	TUniquePtr<FPhysicsGeometry> Geometry;
	FQuat GeometryRotation;
};

FORCEINLINE uint32 GetTypeHash(const FPhysicsShapeReference_Chaos& InShapeReference)
{
    return PointerHash(InShapeReference.Shape);
}

/**
 Wrapper around geometry. This is really just needed to make the physx chaos abstraction easier
 */
struct PHYSICSCORE_API FPhysicsGeometryCollection_Chaos
{
	// Delete default constructor, want only construction by interface (private constructor below)
	FPhysicsGeometryCollection_Chaos() = delete;
	// No copying or assignment, move construction only, these are defaulted in the source file as they need
	// to be able to delete physx::PxGeometryHolder which is incomplete here
	FPhysicsGeometryCollection_Chaos(const FPhysicsGeometryCollection_Chaos& Copy) = delete;
	FPhysicsGeometryCollection_Chaos& operator=(const FPhysicsGeometryCollection_Chaos& Copy) = delete;
	FPhysicsGeometryCollection_Chaos(FPhysicsGeometryCollection_Chaos&& Steal);
	FPhysicsGeometryCollection_Chaos& operator=(FPhysicsGeometryCollection_Chaos&& Steal) = delete;
	~FPhysicsGeometryCollection_Chaos();

	ECollisionShapeType GetType() const;
	const Chaos::FImplicitObject& GetGeometry() const;
	const Chaos::TBox<Chaos::FReal, 3>& GetBoxGeometry() const;
	const Chaos::TSphere<Chaos::FReal, 3>&  GetSphereGeometry() const;
	const Chaos::FCapsule&  GetCapsuleGeometry() const;
	const Chaos::FConvex& GetConvexGeometry() const;
	const Chaos::FTriangleMeshImplicitObject& GetTriMeshGeometry() const;

private:
	friend class FChaosEngineInterface;
	explicit FPhysicsGeometryCollection_Chaos(const FPhysicsShapeReference_Chaos& InShape);

	const Chaos::FImplicitObject& Geom;
};


// Temp interface
namespace physx
{
    class PxActor;
    class PxScene;
	class PxSimulationEventCallback;
    class PxGeometry;
    class PxTransform;
    class PxQuat;
	class PxMassProperties;
}
struct FContactModifyCallback;
class ULineBatchComponent;

class PHYSICSCORE_API FChaosEngineInterface
{
public:
	virtual ~FChaosEngineInterface() = default;

	static void CreateActor(const FActorCreationParams& InParams,FPhysicsActorHandle& Handle);
	static void ReleaseActor(FPhysicsActorHandle& InActorReference,FChaosScene* InScene = nullptr,bool bNeverDeferRelease=false);

	static bool IsValid(const FPhysicsActorHandle& Handle) { return Handle != nullptr; }
	static void AddActorToSolver(FPhysicsActorHandle& Handle,Chaos::FPhysicsSolver* Solver);
	static void RemoveActorFromSolver(FPhysicsActorHandle& Handle,Chaos::FPhysicsSolver* Solver);

	static void SetGlobalPose_AssumesLocked(const FPhysicsActorHandle& InActorReference,const FTransform& InNewPose,bool bAutoWake = true);
	static void SetKinematicTarget_AssumesLocked(const FPhysicsActorHandle& InActorReference,const FTransform& InNewTarget);

	static FChaosScene* GetCurrentScene(const FPhysicsActorHandle& InHandle);

	static FPhysicsMaterialHandle CreateMaterial(const UPhysicalMaterial* InMaterial);
	static void UpdateMaterial(FPhysicsMaterialHandle& InHandle, UPhysicalMaterial* InMaterial);
	static void ReleaseMaterial(FPhysicsMaterialHandle& InHandle);
	static void SetUserData(FPhysicsMaterialHandle& InHandle,void* InUserData);

	static void ReleaseMaterialMask(FPhysicsMaterialMaskHandle& InHandle);

	static FPhysicsAggregateReference_Chaos CreateAggregate(int32 MaxBodies);
	static void ReleaseAggregate(FPhysicsAggregateReference_Chaos& InAggregate);
	static int32 GetNumActorsInAggregate(const FPhysicsAggregateReference_Chaos& InAggregate);
	static void AddActorToAggregate_AssumesLocked(const FPhysicsAggregateReference_Chaos& InAggregate,const FPhysicsActorHandle& InActor);

	static int32 GetNumShapes(const FPhysicsActorHandle& InHandle);

	static void ReleaseShape(const FPhysicsShapeHandle& InShape);

	static void AttachShape(const FPhysicsActorHandle& InActor,const FPhysicsShapeHandle& InNewShape);
	static void DetachShape(const FPhysicsActorHandle& InActor,FPhysicsShapeHandle& InShape,bool bWakeTouching = true);

	static void SetSmoothEdgeCollisionsEnabled_AssumesLocked(const FPhysicsActorHandle& InActor, const bool bSmoothEdgeCollisionsEnabled);

	static void AddDisabledCollisionsFor_AssumesLocked(const TMap<FPhysicsActorHandle, TArray< FPhysicsActorHandle > >& InMap);
	static void RemoveDisabledCollisionsFor_AssumesLocked(TArray< FPhysicsActorHandle > & InPhysicsActors);

	static void SetDisabled(const FPhysicsActorHandle& InPhysicsActor, bool bSetDisabled);
	static bool IsDisabled(const FPhysicsActorHandle& InPhysicsActor);

	static void SetActorUserData_AssumesLocked(FPhysicsActorHandle& InActorReference,FPhysicsUserData* InUserData);

	static bool IsRigidBody(const FPhysicsActorHandle& InActorReference);
	static bool IsDynamic(const FPhysicsActorHandle& InActorReference);
	static bool IsStatic(const FPhysicsActorHandle& InActorReference);
	static bool IsKinematic(const FPhysicsActorHandle& InActorReference);
	static bool IsKinematic_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static bool IsSleeping(const FPhysicsActorHandle& InActorReference);
	static bool IsCcdEnabled(const FPhysicsActorHandle& InActorReference);
	// @todo(mlentine): We don't have a notion of sync vs async and are a bit of both. Does this work?
	static bool HasSyncSceneData(const FPhysicsActorHandle& InHandle) { return true; }
	static bool HasAsyncSceneData(const FPhysicsActorHandle& InHandle) { return false; }
	static bool IsInScene(const FPhysicsActorHandle& InActorReference);

	static int32 GetAllShapes_AssumedLocked(const FPhysicsActorHandle& InActorHandle,TArray<FPhysicsShapeReference_Chaos,FDefaultAllocator>& OutShapes);
	static int32 GetAllShapes_AssumedLocked(const FPhysicsActorHandle& InActorHandle,PhysicsInterfaceTypes::FInlineShapeArray& OutShapes);

	static bool CanSimulate_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static float GetMass_AssumesLocked(const FPhysicsActorHandle& InActorReference);

	static void SetSendsSleepNotifies_AssumesLocked(const FPhysicsActorHandle& InActorReference,bool bSendSleepNotifies);
	static void PutToSleep_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static void WakeUp_AssumesLocked(const FPhysicsActorHandle& InActorReference);

	static void SetIsKinematic_AssumesLocked(const FPhysicsActorHandle& InActorReference,bool bIsKinematic);
	static void SetCcdEnabled_AssumesLocked(const FPhysicsActorHandle& InActorReference,bool bIsCcdEnabled);
	static void SetIgnoreAnalyticCollisions_AssumesLocked(const FPhysicsActorHandle& InActorReference,bool bIsCcdEnabled);

	static FTransform GetGlobalPose_AssumesLocked(const FPhysicsActorHandle& InActorReference);

	static FTransform GetTransform_AssumesLocked(const FPhysicsActorHandle& InRef,bool bForceGlobalPose = false);

	static bool HasKinematicTarget_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static FTransform GetKinematicTarget_AssumesLocked(const FPhysicsActorHandle& InActorReference);

	static FVector GetLinearVelocity_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static void SetLinearVelocity_AssumesLocked(const FPhysicsActorHandle& InActorReference,const FVector& InNewVelocity,bool bAutoWake = true);

	static FVector GetAngularVelocity_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static void SetAngularVelocity_AssumesLocked(const FPhysicsActorHandle& InActorReference,const FVector& InNewVelocity,bool bAutoWake = true);
	static float GetMaxAngularVelocity_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static float GetMaxLinearVelocity_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static void SetMaxAngularVelocity_AssumesLocked(const FPhysicsActorHandle& InActorReference,float InMaxAngularVelocityRadians);
	static void SetMaxLinearVelocity_AssumesLocked(const FPhysicsActorHandle& InActorReference, float InMaxLinearVelocity);

	static float GetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static void SetMaxDepenetrationVelocity_AssumesLocked(const FPhysicsActorHandle& InActorReference,float InMaxDepenetrationVelocity);

	static FVector GetWorldVelocityAtPoint_AssumesLocked(const FPhysicsActorHandle& InActorReference,const FVector& InPoint);
	static FVector GetWorldVelocityAtPoint_AssumesLocked(const Chaos::FRigidBodyHandle_Internal* InActorReference, const FVector& InPoint);

	static FTransform GetComTransform_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static FTransform GetComTransformLocal_AssumesLocked(const FPhysicsActorHandle& InActorReference);

	static FVector GetLocalInertiaTensor_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static FBox GetBounds_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static FBox GetBounds_AssumesLocked(const FPhysicsActorHandle& InActorReference, const FTransform& InTransform);

	static void SetLinearDamping_AssumesLocked(const FPhysicsActorHandle& InActorReference,float InDamping);
	static void SetAngularDamping_AssumesLocked(const FPhysicsActorHandle& InActorReference,float InDamping);

	static void AddImpulse_AssumesLocked(const FPhysicsActorHandle& InActorReference,const FVector& InForce);
	static void AddAngularImpulseInRadians_AssumesLocked(const FPhysicsActorHandle& InActorReference,const FVector& InTorque);
	static void AddVelocity_AssumesLocked(const FPhysicsActorHandle& InActorReference,const FVector& InForce);
	static void AddAngularVelocityInRadians_AssumesLocked(const FPhysicsActorHandle& InActorReference,const FVector& InTorque);
	static void AddImpulseAtLocation_AssumesLocked(const FPhysicsActorHandle& InActorReference,const FVector& InImpulse,const FVector& InLocation);
	static void AddVelocityChangeImpulseAtLocation_AssumesLocked(const FPhysicsActorHandle& InActorReference, const FVector& InVelocityDelta, const FVector& InLocation);
	static void AddRadialImpulse_AssumesLocked(const FPhysicsActorHandle& InActorReference,const FVector& InOrigin,float InRadius,float InStrength,ERadialImpulseFalloff InFalloff,bool bInVelChange);

	static bool IsGravityEnabled_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static void SetGravityEnabled_AssumesLocked(const FPhysicsActorHandle& InActorReference,bool bEnabled);

	static void SetOneWayInteraction_AssumesLocked(const FPhysicsActorHandle& InHandle, bool InOneWayInteraction);

	static float GetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static void SetSleepEnergyThreshold_AssumesLocked(const FPhysicsActorHandle& InActorReference,float InEnergyThreshold);

	static void SetMass_AssumesLocked(FPhysicsActorHandle& InHandle,float InMass);
	static void SetMassSpaceInertiaTensor_AssumesLocked(FPhysicsActorHandle& InHandle,const FVector& InTensor);
	static void SetComLocalPose_AssumesLocked(const FPhysicsActorHandle& InHandle,const FTransform& InComLocalPose);

	static bool IsInertiaConditioningEnabled_AssumesLocked(const FPhysicsActorHandle& InActorReference);
	static void SetInertiaConditioningEnabled_AssumesLocked(const FPhysicsActorHandle& InActorReference, bool bEnabled);

	static float GetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorHandle& InHandle);
	static void SetStabilizationEnergyThreshold_AssumesLocked(const FPhysicsActorHandle& InHandle,float InThreshold);
	static uint32 GetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorHandle& InHandle);
	static void SetSolverPositionIterationCount_AssumesLocked(const FPhysicsActorHandle& InHandle,uint32 InSolverIterationCount);
	static uint32 GetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorHandle& InHandle);
	static void SetSolverVelocityIterationCount_AssumesLocked(const FPhysicsActorHandle& InHandle,uint32 InSolverIterationCount);
	static float GetWakeCounter_AssumesLocked(const FPhysicsActorHandle& InHandle);
	static void SetWakeCounter_AssumesLocked(const FPhysicsActorHandle& InHandle,float InWakeCounter);

	static void SetInitialized_AssumesLocked(const FPhysicsActorHandle& InHandle,bool InInitialized);

	static SIZE_T GetResourceSizeEx(const FPhysicsActorHandle& InActorRef);

	static FPhysicsConstraintHandle CreateConstraint(const FPhysicsActorHandle& InActorRef1,const FPhysicsActorHandle& InActorRef2,const FTransform& InLocalFrame1,const FTransform& InLocalFrame2);
	static FPhysicsConstraintHandle CreateSuspension(const FPhysicsActorHandle& InActorRef, const FVector& InLocalFrame);
	static void SetConstraintUserData(const FPhysicsConstraintHandle& InConstraintRef,void* InUserData);
	static void ReleaseConstraint(FPhysicsConstraintHandle& InConstraintRef);

	static FTransform GetLocalPose(const FPhysicsConstraintHandle& InConstraintRef,EConstraintFrame::Type InFrame);
	static FTransform GetGlobalPose(const FPhysicsConstraintHandle& InConstraintRef,EConstraintFrame::Type InFrame);
	static FVector GetLocation(const FPhysicsConstraintHandle& InConstraintRef);
	static void GetForce(const FPhysicsConstraintHandle& InConstraintRef,FVector& OutLinForce,FVector& OutAngForce);
	static void GetDriveLinearVelocity(const FPhysicsConstraintHandle& InConstraintRef,FVector& OutLinVelocity);
	static void GetDriveAngularVelocity(const FPhysicsConstraintHandle& InConstraintRef,FVector& OutAngVelocity);

	static float GetCurrentSwing1(const FPhysicsConstraintHandle& InConstraintRef);
	static float GetCurrentSwing2(const FPhysicsConstraintHandle& InConstraintRef);
	static float GetCurrentTwist(const FPhysicsConstraintHandle& InConstraintRef);

	static void SetCanVisualize(const FPhysicsConstraintHandle& InConstraintRef,bool bInCanVisualize);
	static void SetCollisionEnabled(const FPhysicsConstraintHandle& InConstraintRef,bool bInCollisionEnabled);
	static void SetProjectionEnabled_AssumesLocked(const FPhysicsConstraintHandle& InConstraintRef,bool bInProjectionEnabled,float InLinearAlpha = 1.0f,float InAngularAlpha = 0.0f, float InLinearTolerance = 0.0f, float InAngularToleranceDeg = 0.0f);
	static void SetShockPropagationEnabled_AssumesLocked(const FPhysicsConstraintHandle& InConstraintRef, bool bInShockPropagationEnabled, float InShockPropagationAlpha);
	static void SetParentDominates_AssumesLocked(const FPhysicsConstraintHandle& InConstraintRef,bool bInParentDominates);
	static void SetBreakForces_AssumesLocked(const FPhysicsConstraintHandle& InConstraintRef,float InLinearBreakForce,float InAngularBreakForce);
	static void SetPlasticityLimits_AssumesLocked(const FPhysicsConstraintHandle& InConstraintRef, float InLinearPlasticityLimit, float InAngularPlasticityLimit, EConstraintPlasticityType InLinearPlasticityType);
	static void SetContactTransferScale_AssumesLocked(const FPhysicsConstraintHandle& InConstraintRef, float InContactTransferScale);
	static void SetLocalPose(const FPhysicsConstraintHandle& InConstraintRef, const FTransform& InPose, EConstraintFrame::Type InFrame);

	static void SetDrivePosition(const FPhysicsConstraintHandle& InConstraintRef,const FVector& InPosition);
	static void SetDriveOrientation(const FPhysicsConstraintHandle& InConstraintRef,const FQuat& InOrientation);
	static void SetDriveLinearVelocity(const FPhysicsConstraintHandle& InConstraintRef,const FVector& InLinVelocity);
	static void SetDriveAngularVelocity(const FPhysicsConstraintHandle& InConstraintRef,const FVector& InAngVelocity);

	static void SetTwistLimit(const FPhysicsConstraintHandle& InConstraintRef,float InLowerLimit,float InUpperLimit,float InContactDistance);
	static void SetSwingLimit(const FPhysicsConstraintHandle& InConstraintRef,float InYLimit,float InZLimit,float InContactDistance);
	static void SetLinearLimit(const FPhysicsConstraintHandle& InConstraintRef,float InLimit);

	static bool IsBroken(const FPhysicsConstraintHandle& InConstraintRef);

	static void SetGeometry(FPhysicsShapeHandle& InShape, TUniquePtr<Chaos::FImplicitObject>&& InGeometry);
	
	static FCollisionFilterData GetSimulationFilter(const FPhysicsShapeHandle& InShape);
	static FCollisionFilterData GetQueryFilter(const FPhysicsShapeHandle& InShape);
	static bool IsSimulationShape(const FPhysicsShapeHandle& InShape);
	static bool IsQueryShape(const FPhysicsShapeHandle& InShape);
	static ECollisionShapeType GetShapeType(const FPhysicsShapeHandle& InShape);
	static FTransform GetLocalTransform(const FPhysicsShapeHandle& InShape);


	static void* GetUserData(const FPhysicsShapeHandle& InShape);
	static void SetUserData(const FPhysicsShapeHandle& InShape, void* InUserData);
	static FPhysicsShapeHandle CloneShape(const FPhysicsShapeHandle& InShape);
	static FPhysicsGeometryCollection_Chaos GetGeometryCollection(const FPhysicsShapeHandle& InShape);


	// @todo(mlentine): Which of these do we need to support?
	// Set the mask filter of a shape, which is an extra level of filtering during collision detection / query for extra channels like "Blue Team" and "Red Team"
	static void SetMaskFilter(const FPhysicsShapeHandle& InShape,FMaskFilter InFilter);
	static void SetSimulationFilter(const FPhysicsShapeHandle& InShape,const FCollisionFilterData& InFilter);
	static void SetQueryFilter(const FPhysicsShapeHandle& InShape,const FCollisionFilterData& InFilter);
	static void SetIsSimulationShape(const FPhysicsShapeHandle& InShape,bool bIsSimShape);
	static void SetIsProbeShape(const FPhysicsShapeHandle& InShape,bool bIsProbeShape);
	static void SetIsQueryShape(const FPhysicsShapeHandle& InShape,bool bIsQueryShape);
	static void SetLocalTransform(const FPhysicsShapeHandle& InShape,const FTransform& NewLocalTransform);
};
