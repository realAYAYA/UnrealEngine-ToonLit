// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysicsSettings.h: Declares the PhysicsSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Templates/Casts.h"
#include "Engine/DeveloperSettings.h"
#include "PhysicsSettingsEnums.h"
#include "BodySetupEnums.h"
#include "Chaos/ChaosEngineInterface.h"
#include "GameFramework/WorldSettings.h"
#include "PhysicsCoreTypes.h"
#include "PhysicsSettingsCore.h"

#include "PhysicsSettings.generated.h"

/**
 * Structure that represents the name of physical surfaces.
 */
USTRUCT()
struct FPhysicalSurfaceName
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TEnumAsByte<enum EPhysicalSurface> Type;

	UPROPERTY()
	FName Name;

	FPhysicalSurfaceName()
		: Type(SurfaceType_Max)
	{}
	FPhysicalSurfaceName(EPhysicalSurface InType, const FName& InName)
		: Type(InType)
		, Name(InName)
	{}
};

/** 
  * Settings container for Chaos physics engine settings, accessed in Chaos through a setting provider interface.
  * See: IChaosSettingsProvider
  */
USTRUCT()
struct FChaosPhysicsSettings
{
	GENERATED_BODY()

	ENGINE_API FChaosPhysicsSettings();

	/** Default threading model to use on module initialisation. Can be switched at runtime using p.Chaos.ThreadingModel */
	UPROPERTY(EditAnywhere, Category = ChaosPhysics)
	EChaosThreadingMode DefaultThreadingModel;

	/** The framerate/timestep ticking mode when running with a dedicated thread */
	UPROPERTY(EditAnywhere, Category = Framerate)
	EChaosSolverTickMode DedicatedThreadTickMode;

	/** The buffering mode to use when running with a dedicated thread */
	UPROPERTY(EditAnywhere, Category = Framerate)
	EChaosBufferMode DedicatedThreadBufferMode;

	ENGINE_API void OnSettingsUpdated();
};

UENUM()
namespace ESettingsDOF
{
	enum Type : int
	{
		/** Allows for full 3D movement and rotation. */
		Full3D,
		/** Allows 2D movement along the Y-Z plane. */
		YZPlane,
		/** Allows 2D movement along the X-Z plane. */
		XZPlane,
		/** Allows 2D movement along the X-Y plane. */
		XYPlane,
	};
}

/** Physics Prediction Settings */
USTRUCT()
struct FPhysicsPredictionSettings
{
	GENERATED_BODY();

	/** Enable networked physics prediction (experimental)
	* Note: If an AActor::PhysicsReplicationMode is set to use Resimulation this will allow physics to cache history which is required by resimulation replication.
	* Note: This can also affect how physics is solved even when not using resimulation. */
	UPROPERTY(EditAnywhere, Category = "Replication")
	bool bEnablePhysicsPrediction;

	/** Forces the PlayerController to sync inputs as used in Physics Prediction.
	* Only enable this if actively using a custom solution that needs this enabled for resimulation.
	* This is automatically enabled when using the recommended NetworkPhysicsComponent on a pawn to handle Rewind / Resimulation. */
	UPROPERTY(EditAnywhere, Category = "Replication", meta = (editcondition = "bEnablePhysicsPrediction"))
	bool bEnablePhysicsResimulation;

	/** Distance in centimeters before a state discrepancy triggers a resimulation */
	UPROPERTY(EditAnywhere, Category = "Replication", meta = (editcondition = "bEnablePhysicsPrediction"))
	float ResimulationErrorThreshold;

	/** Amount of RTT (Round Trip Time) latency for the prediction to support in milliseconds. */
	UPROPERTY(EditAnywhere, Category = "Replication", meta = (editcondition = "bEnablePhysicsPrediction"))
	float MaxSupportedLatencyPrediction;

	FPhysicsPredictionSettings()
		: bEnablePhysicsPrediction(false)
		, bEnablePhysicsResimulation(false)
		, ResimulationErrorThreshold(10.0)
		, MaxSupportedLatencyPrediction(1000)
	{ }
};

UENUM()
namespace ESettingsLockedAxis
{
	enum Type : int
	{
		/** No axis is locked. */
		None,
		/** Lock movement along the x-axis. */
		X,
		/** Lock movement along the y-axis. */
		Y,
		/** Lock movement along the z-axis. */
		Z,
		/** Used for backwards compatibility. Indicates that we've updated into the new struct. */
		Invalid
	};
}

/**
 * Default physics settings.
 */
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Physics"), MinimalAPI)
class UPhysicsSettings : public UPhysicsSettingsCore
{
	GENERATED_UCLASS_BODY()

	/** Settings for Networked Physics Prediction, experimental. */
	UPROPERTY(Config, EditAnywhere, Category = Replication, meta = (DisplayName = "Physics Prediction (Experimental)"))
	FPhysicsPredictionSettings PhysicsPrediction;

	/** Error correction data for replicating simulated physics (rigid bodies) */
	UPROPERTY(config, EditAnywhere, Category = Replication)
	FRigidBodyErrorCorrection PhysicErrorCorrection;

	UPROPERTY(config)
	TEnumAsByte<ESettingsLockedAxis::Type> LockedAxis_DEPRECATED;

	/** Useful for constraining all objects in the world, for example if you are making a 2D game using 3D environments.*/
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	TEnumAsByte<ESettingsDOF::Type> DefaultDegreesOfFreedom;
	
	/**
	*  If true, the internal physx face to UE face mapping will not be generated. This is a memory optimization available if you do not rely on face indices returned by scene queries. */
	UPROPERTY(config, EditAnywhere, Category = Optimization)
	bool bSuppressFaceRemapTable;

	/** If true, store extra information to allow FindCollisionUV to derive UV info from a line trace hit result, using the FindCollisionUV utility */
	UPROPERTY(config, EditAnywhere, Category = Optimization, meta = (DisplayName = "Support UV From Hit Results", ConfigRestartRequired = true))
	bool bSupportUVFromHitResults;

	/**
	* If true, physx will not update unreal with any bodies that have moved during the simulation. This should only be used if you have no physx simulation or you are manually updating the unreal data via polling physx.  */
	UPROPERTY(config, EditAnywhere, Category = Optimization)
	bool bDisableActiveActors;

	/** Whether to disable generating KS pairs, enabling this makes switching between dynamic and static slower for actors - but speeds up contact generation by early rejecting these pairs*/
	UPROPERTY(config, EditAnywhere, Category = Optimization)
	bool bDisableKinematicStaticPairs;

	/** Whether to disable generating KK pairs, enabling this speeds up contact generation, however it is required when using APEX destruction. */
	UPROPERTY(config, EditAnywhere, Category = Optimization)
	bool bDisableKinematicKinematicPairs;

	/**
	*  If true CCD will be ignored. This is an optimization when CCD is never used which removes the need for physx to check it internally. */
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	bool bDisableCCD;

	/** Min Delta Time below which anim dynamics and rigidbody nodes will not simulate. */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = Framerate)
	float AnimPhysicsMinDeltaTime;

	/** Whether to simulate anim physics nodes in the tick where they're reset. */
	UPROPERTY(config, EditAnywhere, Category = Simulation)
	bool bSimulateAnimPhysicsAfterReset;

	/** Min Physics Delta Time; the simulation will not step if the delta time is below this value */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "0.0001", UIMax = "0.0001"), Category = Framerate)
	float MinPhysicsDeltaTime;

	/** Max Physics Delta Time to be clamped. */
	UPROPERTY(config, EditAnywhere, meta=(ClampMin="0.0013", UIMin = "0.0013", ClampMax="1.0", UIMax="1.0"), Category=Framerate)
	float MaxPhysicsDeltaTime;

	/** Whether to substep the physics simulation. This feature is still experimental. Certain functionality might not work correctly*/
	UPROPERTY(config, EditAnywhere, Category = Framerate)
	bool bSubstepping;

	/** Whether to substep the async physics simulation. This feature is still experimental. Certain functionality might not work correctly*/
	UPROPERTY(config, EditAnywhere, Category = Framerate)
	bool bSubsteppingAsync;

	/** Whether to tick physics simulation on an async thread. This feature is still experimental. Certain functionality might not work correctly*/
	UPROPERTY(config, EditAnywhere, Category = Framerate)
	bool bTickPhysicsAsync;

	/** If using async, the time step size to tick at. This feature is still experimental. Certain functionality might not work correctly*/
	UPROPERTY(config, EditAnywhere, Category = Framerate, meta=(editcondition = "bTickPhysicsAsync"))
	float AsyncFixedTimeStepSize;

	/** Max delta time (in seconds) for an individual simulation substep. */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "0.0013", UIMin = "0.0013", ClampMax = "1.0", UIMax = "1.0", editcondition = "bSubStepping"), Category=Framerate)
	float MaxSubstepDeltaTime;

	/** Max number of substeps for physics simulation. */
	UPROPERTY(config, EditAnywhere, meta = (ClampMin = "1", UIMin = "1", ClampMax = "16", UIMax = "16", editcondition = "bSubstepping"), Category=Framerate)
	int32 MaxSubsteps;

	/** Physics delta time smoothing factor for sync scene. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "0.0", UIMin = "0.0", ClampMax = "1.0", UIMax = "1.0"), Category = Framerate)
	float SyncSceneSmoothingFactor;

	/** Physics delta time initial average. */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "0.0013", UIMin = "0.0013", ClampMax = "1.0", UIMax = "1.0"), Category = Framerate)
	float InitialAverageFrameRate;

	/** The number of frames it takes to rebuild the PhysX scene query AABB tree. The bigger the number, the smaller fetchResults takes per frame, but the more the tree deteriorates until a new tree is built */
	UPROPERTY(config, EditAnywhere, AdvancedDisplay, meta = (ClampMin = "4", UIMin = "4"), Category = Framerate)
	int PhysXTreeRebuildRate;

	// PhysicalMaterial Surface Types
	UPROPERTY(config, EditAnywhere, Category=PhysicalSurfaces)
	TArray<FPhysicalSurfaceName> PhysicalSurfaces;

	/** If we want to Enable MPB or not globally. This is then overridden by project settings if not enabled. **/
	UPROPERTY(config, EditAnywhere, Category = Broadphase)
	FBroadphaseSettings DefaultBroadphaseSettings;

	/** Minimum velocity delta required on a collinding object for Chaos to send a hit event */
	UPROPERTY(config, EditAnywhere, Category = ChaosPhysics)
	float MinDeltaVelocityForHitEvents;

	/** Chaos physics engine settings */
	UPROPERTY(config, EditAnywhere, Category = ChaosPhysics)
	FChaosPhysicsSettings ChaosSettings;

public:

	static UPhysicsSettings* Get() { return CastChecked<UPhysicsSettings>(UPhysicsSettings::StaticClass()->GetDefaultObject()); }

	UFUNCTION(BlueprintCallable, Category = "Physics")
	int32 GetPhysicsHistoryCount() const
	{
		return FMath::Max<int32>(1, FMath::CeilToInt(0.001f * PhysicsPrediction.MaxSupportedLatencyPrediction / AsyncFixedTimeStepSize));
	}

	ENGINE_API virtual void PostInitProperties() override;

#if WITH_EDITOR
	ENGINE_API virtual bool CanEditChange( const FProperty* Property ) const override;
	ENGINE_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Load Material Type data from INI file **/
	/** this changes displayname meta data. That means we won't need it outside of editor*/
	ENGINE_API void LoadSurfaceType();

#endif // WITH_EDITOR
};
