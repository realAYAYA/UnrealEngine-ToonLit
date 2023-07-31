// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/Guid.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#include "RenderCommandFence.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "SceneTypes.h"
#include "Engine/EngineTypes.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Engine/TextureStreamingTypes.h"
#include "AI/Navigation/NavRelevantInterface.h"
#include "VT/RuntimeVirtualTextureEnum.h"
#include "HitProxies.h"
#include "Interfaces/Interface_AsyncCompilation.h"
#include "HLOD/HLODBatchingPolicy.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_1
#include "Engine/OverlapInfo.h"
#endif
#include "PrimitiveComponent.generated.h"

class AController;
class FPrimitiveSceneProxy;
class UMaterialInterface;
class UPrimitiveComponent;
class UTexture;
class URuntimeVirtualTexture;
struct FCollisionShape;
struct FConvexVolume;
struct FEngineShowFlags;
struct FNavigableGeometryExport;
struct FPSOPrecacheParams;

namespace PrimitiveComponentCVars
{
	extern float HitDistanceToleranceCVar;
	extern float InitialOverlapToleranceCVar;
}

/** Determines whether a Character can attempt to step up onto a component when they walk in to it. */
UENUM()
enum ECanBeCharacterBase
{
	/** Character cannot step up onto this Component. */
	ECB_No UMETA(DisplayName="No"),
	/** Character can step up onto this Component. */
	ECB_Yes UMETA(DisplayName="Yes"),
	/**
	 * Owning actor determines whether character can step up onto this Component (default true unless overridden in code).
	 * @see AActor::CanBeBaseForCharacter()
	 */
	ECB_Owner UMETA(DisplayName="(Owner)"),
	ECB_MAX,
};

/** Determines if a primitive component contains custom collision for navigation/AI */
UENUM()
namespace EHasCustomNavigableGeometry
{
	enum Type
	{
		/** Primitive doesn't have custom navigation geometry, if collision is enabled then its convex/trimesh collision will be used for generating the navmesh */
		No,

		/** If primitive would normally affect navmesh, DoCustomNavigableGeometryExport() should be called to export this primitive's navigable geometry */
		Yes,

		/** DoCustomNavigableGeometryExport() should be called even if the mesh is non-collidable and wouldn't normally affect the navmesh */
		EvenIfNotCollidable,

		/** Don't export navigable geometry even if primitive is relevant for navigation (can still add modifiers) */
		DontExport,
	};
}

/** Information about the sprite category, used for visualization in the editor */
USTRUCT()
struct FSpriteCategoryInfo
{
	GENERATED_BODY()

	/** Sprite category that the component belongs to */
	UPROPERTY()
	FName Category;

	/** Localized name of the sprite category */
	UPROPERTY()
	FText DisplayName;

	/** Localized description of the sprite category */
	UPROPERTY()
	FText Description;
};

/** Exposed enum to parallel RHI's EStencilMask and show up in the editor. Has a paired struct to convert between the two. */
UENUM()
enum class ERendererStencilMask : uint8
{
	ERSM_Default UMETA(DisplayName = "Default"),
	ERSM_255 UMETA(DisplayName = "All bits (255), ignore depth"),
	ERSM_1 UMETA(DisplayName = "First bit (1), ignore depth"),
	ERSM_2 UMETA(DisplayName = "Second bit (2), ignore depth"),
	ERSM_4 UMETA(DisplayName = "Third bit (4), ignore depth"),
	ERSM_8 UMETA(DisplayName = "Fourth bit (8), ignore depth"),
	ERSM_16 UMETA(DisplayName = "Fifth bit (16), ignore depth"),
	ERSM_32 UMETA(DisplayName = "Sixth bit (32), ignore depth"),
	ERSM_64 UMETA(DisplayName = "Seventh bit (64), ignore depth"),
	ERSM_128 UMETA(DisplayName = "Eighth bit (128), ignore depth")
};

/** How quickly component should be culled. */
UENUM()
enum class ERayTracingGroupCullingPriority : uint8
{
	CP_0_NEVER_CULL UMETA(DisplayName = "0 - Never cull"),
	CP_1 UMETA(DisplayName = "1"),
	CP_2 UMETA(DisplayName = "2"),
	CP_3 UMETA(DisplayName = "3"),
	CP_4_DEFAULT UMETA(DisplayName = "4 - Default"),
	CP_5 UMETA(DisplayName = "5"),
	CP_6 UMETA(DisplayName = "6"),
	CP_7 UMETA(DisplayName = "7"),
	CP_8_QUICKLY_CULL UMETA(DisplayName = "8 - Quickly cull")
};

/** Converts a stencil mask from the editor's USTRUCT version to the version the renderer uses. */
struct FRendererStencilMaskEvaluation
{
	static FORCEINLINE EStencilMask ToStencilMask(const ERendererStencilMask InEnum)
	{
		switch (InEnum)
		{
		case ERendererStencilMask::ERSM_Default:
			return EStencilMask::SM_Default;
		case ERendererStencilMask::ERSM_255:
			return EStencilMask::SM_255;
		case ERendererStencilMask::ERSM_1:
			return EStencilMask::SM_1;
		case ERendererStencilMask::ERSM_2:
			return EStencilMask::SM_2;
		case ERendererStencilMask::ERSM_4:
			return EStencilMask::SM_4;
		case ERendererStencilMask::ERSM_8:
			return EStencilMask::SM_8;
		case ERendererStencilMask::ERSM_16:
			return EStencilMask::SM_16;
		case ERendererStencilMask::ERSM_32:
			return EStencilMask::SM_32;
		case ERendererStencilMask::ERSM_64:
			return EStencilMask::SM_64;
		case ERendererStencilMask::ERSM_128:
			return EStencilMask::SM_128;
		default:
			// Unsupported EStencilMask - return a safe default.
			check(false);
			return EStencilMask::SM_Default;
		}
	}
};

// TODO: Add sleep and wake state change types to this enum, so that the
// OnComponentWake and OnComponentSleep delegates may be deprecated.
// Doing so would save a couple bytes per primitive component.
UENUM(BlueprintType)
enum class EComponentPhysicsStateChange : uint8
{
	Created,
	Destroyed
};

/**
 * Delegate for notification of blocking collision against a specific component.  
 * NormalImpulse will be filled in for physics-simulating bodies, but will be zero for swept-component blocking collisions. 
 */
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_FiveParams( FComponentHitSignature, UPrimitiveComponent, OnComponentHit, UPrimitiveComponent*, HitComponent, AActor*, OtherActor, UPrimitiveComponent*, OtherComp, FVector, NormalImpulse, const FHitResult&, Hit );
/** Delegate for notification of start of overlap with a specific component */
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_SixParams( FComponentBeginOverlapSignature, UPrimitiveComponent, OnComponentBeginOverlap, UPrimitiveComponent*, OverlappedComponent, AActor*, OtherActor, UPrimitiveComponent*, OtherComp, int32, OtherBodyIndex, bool, bFromSweep, const FHitResult &, SweepResult);
/** Delegate for notification of end of overlap with a specific component */
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_FourParams( FComponentEndOverlapSignature, UPrimitiveComponent, OnComponentEndOverlap, UPrimitiveComponent*, OverlappedComponent, AActor*, OtherActor, UPrimitiveComponent*, OtherComp, int32, OtherBodyIndex);
/** Delegate for notification when a wake event is fired by physics*/
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FComponentWakeSignature, UPrimitiveComponent, OnComponentWake, UPrimitiveComponent*, WakingComponent, FName, BoneName);
/** Delegate for notification when a sleep event is fired by physics*/
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FComponentSleepSignature, UPrimitiveComponent, OnComponentSleep, UPrimitiveComponent*, SleepingComponent, FName, BoneName);
/** Delegate for notification when collision settings change. */
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FComponentCollisionSettingsChangedSignature, UPrimitiveComponent, OnComponentCollisionSettingsChangedEvent, UPrimitiveComponent*, ChangedComponent);
/** Delegate for physics state created */
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams(FComponentPhysicsStateChanged, UPrimitiveComponent, OnComponentPhysicsStateChanged, UPrimitiveComponent*, ChangedComponent, EComponentPhysicsStateChange, StateChange);

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam( FComponentBeginCursorOverSignature, UPrimitiveComponent, OnBeginCursorOver, UPrimitiveComponent*, TouchedComponent );
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam( FComponentEndCursorOverSignature, UPrimitiveComponent, OnEndCursorOver, UPrimitiveComponent*, TouchedComponent );
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams( FComponentOnClickedSignature, UPrimitiveComponent, OnClicked, UPrimitiveComponent*, TouchedComponent , FKey, ButtonPressed);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams( FComponentOnReleasedSignature, UPrimitiveComponent, OnReleased, UPrimitiveComponent*, TouchedComponent, FKey, ButtonReleased);
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams( FComponentOnInputTouchBeginSignature, UPrimitiveComponent, OnInputTouchBegin, ETouchIndex::Type, FingerIndex, UPrimitiveComponent*, TouchedComponent );
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams( FComponentOnInputTouchEndSignature, UPrimitiveComponent, OnInputTouchEnd, ETouchIndex::Type, FingerIndex, UPrimitiveComponent*, TouchedComponent );
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams( FComponentBeginTouchOverSignature, UPrimitiveComponent, OnInputTouchEnter, ETouchIndex::Type, FingerIndex, UPrimitiveComponent*, TouchedComponent );
DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_TwoParams( FComponentEndTouchOverSignature, UPrimitiveComponent, OnInputTouchLeave, ETouchIndex::Type, FingerIndex, UPrimitiveComponent*, TouchedComponent );

/**
 * PrimitiveComponents are SceneComponents that contain or generate some sort of geometry, generally to be rendered or used as collision data.
 * There are several subclasses for the various types of geometry, but the most common by far are the ShapeComponents (Capsule, Sphere, Box), StaticMeshComponent, and SkeletalMeshComponent.
 * ShapeComponents generate geometry that is used for collision detection but are not rendered, while StaticMeshComponents and SkeletalMeshComponents contain pre-built geometry that is rendered, but can also be used for collision detection.
 */
UCLASS(abstract, HideCategories=(Mobility, VirtualTexture), ShowCategories=(PhysicsVolume))
class ENGINE_API UPrimitiveComponent : public USceneComponent, public INavRelevantInterface, public IInterface_AsyncCompilation
{
	GENERATED_BODY()

public:
	/**
	 * Default UObject constructor.
	 */
	UPrimitiveComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	UPrimitiveComponent(FVTableHelper& Helper);
	~UPrimitiveComponent();

	// Rendering
	static FName RVTActorDescProperty;

	/**
	 * The minimum distance at which the primitive should be rendered, 
	 * measured in world space units from the center of the primitive's bounding sphere to the camera position.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	float MinDrawDistance;

	/**  Max draw distance exposed to LDs. The real max draw distance is the min (disregarding 0) of this and volumes affecting this object. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD, meta=(DisplayName="Desired Max Draw Distance") )
	float LDMaxDrawDistance;

	/**
	 * The distance to cull this primitive at.  
	 * A CachedMaxDrawDistance of 0 indicates that the primitive should not be culled by distance.
	 */
	UPROPERTY(Category=LOD, AdvancedDisplay, VisibleAnywhere, BlueprintReadOnly, meta=(DisplayName="Current Max Draw Distance") )
	float CachedMaxDrawDistance;

	/** The scene depth priority group to draw the primitive in. */
	UPROPERTY()
	TEnumAsByte<enum ESceneDepthPriorityGroup> DepthPriorityGroup;

	/** The scene depth priority group to draw the primitive in, if it's being viewed by its owner. */
	UPROPERTY()
	TEnumAsByte<enum ESceneDepthPriorityGroup> ViewOwnerDepthPriorityGroup;

	/** Quality of indirect lighting for Movable primitives.  This has a large effect on Indirect Lighting Cache update time. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	TEnumAsByte<EIndirectLightingCacheQuality> IndirectLightingCacheQuality;

	/** Controls the type of lightmap used for this component. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	ELightmapType LightmapType;

#if WITH_EDITORONLY_DATA
	/** Whether to include this component in HLODs or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=HLOD, meta=(DisplayName="Include Component in HLOD"))
	uint8 bEnableAutoLODGeneration : 1;

	/** Which specific HLOD levels this component should be excluded from */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=HLOD, meta=(DisplayName="Exclude from HLOD Levels", EditConditionHides, EditCondition="bEnableAutoLODGeneration"))
	TArray<int32> ExcludeForSpecificHLODLevels;

	/** Determines how the geometry of a component will be incorporated in proxy (simplified) HLODs. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=HLOD, meta=(DisplayName="HLOD Batching Policy", EditConditionHides, EditCondition="bEnableAutoLODGeneration"))
	EHLODBatchingPolicy HLODBatchingPolicy;


	/** Indicates that the texture streaming built data is local to the Actor (see UActorTextureStreamingBuildDataComponent). */
	UPROPERTY()
	uint8 bIsActorTextureStreamingBuiltData : 1;
#endif 

	/** Indicates to the texture streaming wether it can use the pre-built texture streaming data (even if empty). */
	UPROPERTY()
	uint8 bIsValidTextureStreamingBuiltData : 1;

	/**
	 * When enabled this object will not be culled by distance. This is ignored if a child of a HLOD.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	uint8 bNeverDistanceCull:1;

	/** Whether this primitive is referenced by a FLevelRenderAssetManager  */
	mutable uint8 bAttachedToStreamingManagerAsStatic : 1;
	/** Whether this primitive is referenced by a FDynamicRenderAssetInstanceManager */
	mutable uint8 bAttachedToStreamingManagerAsDynamic : 1;
	/** Whether this primitive is handled as dynamic, although it could have no references */
	mutable uint8 bHandledByStreamingManagerAsDynamic : 1;
	/** When true, texture streaming manager won't update the component state. Used to perform early exits when updating component. */
	mutable uint8 bIgnoreStreamingManagerUpdate : 1;

	/** Whether this primitive is referenced by a Nanite::FCoarseMeshStreamingManager  */
	mutable uint8 bAttachedToCoarseMeshStreamingManager : 1;

	/** Whether this primitive is referenced by the streaming manager and should sent callbacks when detached or destroyed */
	FORCEINLINE bool IsAttachedToStreamingManager() const { return !!(bAttachedToStreamingManagerAsStatic | bAttachedToStreamingManagerAsDynamic); }

	/** 
	 * Indicates if we'd like to create physics state all the time (for collision and simulation). 
	 * If you set this to false, it still will create physics state if collision or simulation activated. 
	 * This can help performance if you'd like to avoid overhead of creating physics state when triggers 
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Collision)
	uint8 bAlwaysCreatePhysicsState:1;

	/**
	 * If true, this component will generate overlap events when it is overlapping other components (eg Begin Overlap).
	 * Both components (this and the other) must have this enabled for overlap events to occur.
	 *
	 * @see [Overlap Events](https://docs.unrealengine.com/InteractiveExperiences/Physics/Collision/Overview#overlapandgenerateoverlapevents)
	 * @see UpdateOverlaps(), BeginComponentOverlap(), EndComponentOverlap()
	 */
	UFUNCTION(BlueprintGetter)
	bool GetGenerateOverlapEvents() const;

	/** Modifies value returned by GetGenerateOverlapEvents() */
	UFUNCTION(BlueprintSetter)
	void SetGenerateOverlapEvents(bool bInGenerateOverlapEvents);

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components")
	void SetLightingChannels(bool bChannel0, bool bChannel1, bool bChannel2);

	/** Invalidates Lumen surface cache and forces it to be refreshed. Useful to make material updates more responsive. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Lighting")
	void InvalidateLumenSurfaceCache();

private:
	UPROPERTY(EditAnywhere, BlueprintGetter = GetGenerateOverlapEvents, BlueprintSetter = SetGenerateOverlapEvents, Category = Collision)
	uint8 bGenerateOverlapEvents : 1;

public:
	/**
	 * If true, this component will generate individual overlaps for each overlapping physics body if it is a multi-body component. When false, this component will
	 * generate only one overlap, regardless of how many physics bodies it has and how many of them are overlapping another component/body. This flag has no
	 * influence on single body components.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Collision)
	uint8 bMultiBodyOverlap:1;

	/**
	 * If true, component sweeps with this component should trace against complex collision during movement (for example, each triangle of a mesh).
	 * If false, collision will be resolved against simple collision bounds instead.
	 * @see MoveComponent()
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Collision)
	uint8 bTraceComplexOnMove:1;

	/**
	 * If true, component sweeps will return the material in their hit result.
	 * @see MoveComponent(), FHitResult
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadWrite, Category=Collision)
	uint8 bReturnMaterialOnMove:1;

	/** True if the primitive should be rendered using ViewOwnerDepthPriorityGroup if viewed by its owner. */
	UPROPERTY()
	uint8 bUseViewOwnerDepthPriorityGroup:1;

	/** Whether to accept cull distance volumes to modify cached cull distance. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=LOD)
	uint8 bAllowCullDistanceVolume:1;

	/** If true, this component will be visible in reflection captures. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bVisibleInReflectionCaptures:1;
	
	/** If true, this component will be visible in real-time sky light reflection captures. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bVisibleInRealTimeSkyCaptures :1;

	/** If true, this component will be visible in ray tracing effects. Turning this off will remove it from ray traced reflections, shadows, etc. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bVisibleInRayTracing : 1;

	/** If true, this component will be rendered in the main pass (z prepass, basepass, transparency) */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bRenderInMainPass:1;

	/** If true, this component will be rendered in the depth pass even if it's not rendered in the main pass */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering, meta = (EditCondition = "!bRenderInMainPass"))
	uint8 bRenderInDepthPass:1;

	/** Whether the primitive receives decals. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering)
	uint8 bReceivesDecals:1;

	/** If this is True, this component won't be visible when the view actor is the component's owner, directly or indirectly. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bOwnerNoSee:1;

	/** If this is True, this component will only be visible when the view actor is the component's owner, directly or indirectly. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering)
	uint8 bOnlyOwnerSee:1;

	/** Treat this primitive as part of the background for occlusion purposes. This can be used as an optimization to reduce the cost of rendering skyboxes, large ground planes that are part of the vista, etc. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering)
	uint8 bTreatAsBackgroundForOcclusion:1;

	/** 
	 * Whether to render the primitive in the depth only pass.  
	 * This should generally be true for all objects, and let the renderer make decisions about whether to render objects in the depth only pass.
	 * @todo - if any rendering features rely on a complete depth only pass, this variable needs to go away.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering)
	uint8 bUseAsOccluder:1;

	/** If this is True, this component can be selected in the editor. */
	UPROPERTY()
	uint8 bSelectable:1;

#if WITH_EDITORONLY_DATA
	/** If true, this component will be considered for placement when dragging and placing items in the editor even if it is not visible, such as in the case of hidden collision meshes */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = Collision)
	uint8 bConsiderForActorPlacementWhenHidden:1;
#endif //WITH_EDITORONLY_DATA

	/** If true, forces mips for textures used by this component to be resident when this component's level is loaded. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=TextureStreaming)
	uint8 bForceMipStreaming:1;

	/** If true a hit-proxy will be generated for each instance of instanced static meshes */
	UPROPERTY()
	uint8 bHasPerInstanceHitProxies:1;

	// Lighting flags
	
	/** Controls whether the primitive component should cast a shadow or not. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting)
	uint8 CastShadow:1;

	/** Whether the primitive will be used as an emissive light source. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay)
	uint8 bEmissiveLightSource:1;

	/** Controls whether the primitive should inject light into the Light Propagation Volume.  This flag is only used if CastShadow is true. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay, meta=(EditCondition="CastShadow"))
	uint8 bAffectDynamicIndirectLighting:1;

	/** Controls whether the primitive should affect indirect lighting when hidden. This flag is only used if bAffectDynamicIndirectLighting is true. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="bAffectDynamicIndirectLighting", DisplayName = "Affect Indirect Lighting While Hidden"))
	uint8 bAffectIndirectLightingWhileHidden:1;

	/** Controls whether the primitive should affect dynamic distance field lighting methods.  This flag is only used if CastShadow is true. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay)
	uint8 bAffectDistanceFieldLighting:1;

	/** Controls whether the primitive should cast shadows in the case of non precomputed shadowing.  This flag is only used if CastShadow is true. **/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay, meta=(EditCondition="CastShadow", DisplayName = "Dynamic Shadow"))
	uint8 bCastDynamicShadow:1;

	/** Whether the object should cast a static shadow from shadow casting lights.  This flag is only used if CastShadow is true. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay, meta=(EditCondition="CastShadow", DisplayName = "Static Shadow"))
	uint8 bCastStaticShadow:1;

	/** 
	 * Whether the object should cast a volumetric translucent shadow.
	 * Volumetric translucent shadows are useful for primitives with smoothly changing opacity like particles representing a volume, 
	 * But have artifacts when used on highly opaque surfaces.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow", DisplayName = "Volumetric Translucent Shadow"))
	uint8 bCastVolumetricTranslucentShadow:1;

	/**
	 * Whether the object should cast contact shadows.
	 * This flag is only used if CastShadow is true.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Lighting, AdvancedDisplay, meta=(EditCondition="CastShadow", DisplayName = "Contact Shadow"))
	uint8 bCastContactShadow:1;

	/** 
	 * When enabled, the component will only cast a shadow on itself and not other components in the world.  
	 * This is especially useful for first person weapons, and forces bCastInsetShadow to be enabled.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow"))
	uint8 bSelfShadowOnly:1;

	/** 
	 * When enabled, the component will be rendering into the far shadow cascades (only for directional lights).
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow", DisplayName = "Far Shadow"))
	uint8 bCastFarShadow:1;

	/** 
	 * Whether this component should create a per-object shadow that gives higher effective shadow resolution. 
	 * Useful for cinematic character shadowing. Assumed to be enabled if bSelfShadowOnly is enabled.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow", DisplayName = "Dynamic Inset Shadow"))
	uint8 bCastInsetShadow:1;

	/** 
	 * Whether this component should cast shadows from lights that have bCastShadowsFromCinematicObjectsOnly enabled.
	 * This is useful for characters in a cinematic with special cinematic lights, where the cost of shadowmap rendering of the environment is undesired.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow"))
	uint8 bCastCinematicShadow:1;

	/** 
	 *	If true, the primitive will cast shadows even if bHidden is true.
	 *	Controls whether the primitive should cast shadows when hidden.
	 *	This flag is only used if CastShadow is true.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow", DisplayName = "Hidden Shadow"))
	uint8 bCastHiddenShadow:1;

	/** Whether this primitive should cast dynamic shadows as if it were a two sided material. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting, meta=(EditCondition="CastShadow", DisplayName = "Shadow Two Sided"))
	uint8 bCastShadowAsTwoSided:1;

	/** @deprecated Replaced by LightmapType */
	UPROPERTY()
	uint8 bLightAsIfStatic_DEPRECATED:1;

	/** 
	 * Whether to light this component and any attachments as a group.  This only has effect on the root component of an attachment tree.
	 * When enabled, attached component shadowing settings like bCastInsetShadow, bCastVolumetricTranslucentShadow, etc, will be ignored.
	 * This is useful for improving performance when multiple movable components are attached together.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	uint8 bLightAttachmentsAsGroup:1;

	/** 
	 * If set, then it overrides any bLightAttachmentsAsGroup set in a parent.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	uint8 bExcludeFromLightAttachmentGroup :1;

	/**
	* Mobile only:
	* If disabled this component will not receive CSM shadows. (Components that do not receive CSM may have reduced shading cost)
	*/
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Mobile, meta = (DisplayName = "Receive CSM Shadows"))
	uint8 bReceiveMobileCSMShadows : 1;

	/** 
	 * Whether the whole component should be shadowed as one from stationary lights, which makes shadow receiving much cheaper.
	 * When enabled shadowing data comes from the volume lighting samples precomputed by Lightmass, which are very sparse.
	 * This is currently only used on stationary directional lights.  
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	uint8 bSingleSampleShadowFromStationaryLights:1;

	// Physics
	
	/** Will ignore radial impulses applied to this component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	uint8 bIgnoreRadialImpulse:1;

	/** Will ignore radial forces applied to this component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	uint8 bIgnoreRadialForce:1;

	/** True for damage to this component to apply physics impulse, false to opt out of these impulses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	uint8 bApplyImpulseOnDamage : 1;

	/** True if physics should be replicated to autonomous proxies. This should be true for
		server-authoritative simulations, and false for client authoritative simulations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Physics)
	uint8 bReplicatePhysicsToAutonomousProxy : 1;

	// Navigation

	/** If set, navmesh will not be generated under the surface of the geometry */
	UPROPERTY(EditAnywhere, Category = Navigation)
	uint8 bFillCollisionUnderneathForNavmesh:1;

	// General flags.
	
	/** If this is True, this component must always be loaded on clients, even if Hidden and CollisionEnabled is NoCollision. */
	UPROPERTY()
	uint8 AlwaysLoadOnClient:1;

	/** If this is True, this component must always be loaded on servers, even if Hidden and CollisionEnabled is NoCollision */
	UPROPERTY()
	uint8 AlwaysLoadOnServer:1;

	/** Composite the drawing of this component onto the scene after post processing (only applies to editor drawing) */
	UPROPERTY()
	uint8 bUseEditorCompositing:1;

	/** Set to true while the editor is moving the component, which notifies the Renderer to track velocities even if the component is Static. */
	UPROPERTY(Transient, DuplicateTransient)
	uint8 bIsBeingMovedByEditor:1;

	/** If true, this component will be rendered in the CustomDepth pass (usually used for outlines) */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering, meta=(DisplayName = "Render CustomDepth Pass"))
	uint8 bRenderCustomDepth:1;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering, meta = (DisplayName = "Visible In Scene Capture Only", ToolTip = "When true, will only be visible in Scene Capture"))
	uint8 bVisibleInSceneCaptureOnly : 1;

	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = Rendering, meta = (DisplayName = "Hidden In Scene Capture", ToolTip = "When true, will not be captured by Scene Capture"))
	uint8 bHiddenInSceneCapture : 1;

	/** If true, this component will be available to ray trace as a far field primitive even if hidden. */
	UPROPERTY()
	uint8 bRayTracingFarField : 1;

protected:
	/** Result of last call to AreAllCollideableDescendantsRelative(). */
	uint8 bCachedAllCollideableDescendantsRelative : 1;

	UPROPERTY()
	uint8 bHasNoStreamableTextures : 1;

#if WITH_EDITOR
	bool bIgnoreBoundsForEditorFocus = false;
#endif

public:
	/** If true then DoCustomNavigableGeometryExport will be called to collect navigable geometry of this component. */
	UPROPERTY()
	TEnumAsByte<EHasCustomNavigableGeometry::Type> bHasCustomNavigableGeometry;

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TEnumAsByte<enum EHitProxyPriority> HitProxyPriority;
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TEnumAsByte<enum ECanBeCharacterBase> CanBeCharacterBase_DEPRECATED;

	/** Deprecated - represented by HLODBatchingPolicy == EHLODBatchingPolicy::MeshSection */
	UPROPERTY()
	uint8 bUseMaxLODAsImposter_DEPRECATED : 1;

	/** Deprecated - represented by HLODBatchingPolicy == EHLODBatchingPolicy::Instancing */
	UPROPERTY()
	uint8 bBatchImpostersAsInstances_DEPRECATED : 1;
#endif

	FMaskFilter MoveIgnoreMask;

public:
	/**
	 * Determine whether a Character can step up onto this component.
	 * This controls whether they can try to step up on it when they bump in to it, not whether they can walk on it after landing on it.
	 * @see FWalkableSlopeOverride
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Collision)
	TEnumAsByte<enum ECanBeCharacterBase> CanCharacterStepUpOn;

	/** 
	 * Channels that this component should be in.  Lights with matching channels will affect the component.  
	 * These channels only apply to opaque materials, direct lighting, and dynamic lighting and shadowing.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Lighting)
	FLightingChannels LightingChannels;

	/**
	 * Defines run-time groups of components. For example allows to assemble multiple parts of a building at runtime.
	 * -1 means that component doesn't belong to any group.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = RayTracing)
	int32 RayTracingGroupId;

	/** Used for precomputed visibility */
	UPROPERTY()
	int32 VisibilityId=0;

	/** Optionally write this 0-255 value to the stencil buffer in CustomDepth pass (Requires project setting or r.CustomDepth == 3) */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category=Rendering,  meta=(UIMin = "0", UIMax = "255", editcondition = "bRenderCustomDepth", DisplayName = "CustomDepth Stencil Value"))
	int32 CustomDepthStencilValue;

private:
	/** Optional user defined default values for the custom primitive data of this primitive */
	UPROPERTY(EditAnywhere, Category=Rendering, meta = (DisplayName = "Custom Primitive Data Defaults"))
	FCustomPrimitiveData CustomPrimitiveData;

	/** Custom data that can be read by a material through a material parameter expression. Set data using SetCustomPrimitiveData* functions */
	UPROPERTY(Transient)
	FCustomPrimitiveData CustomPrimitiveDataInternal;
public:

	/** If non-null, physics state creation has been deferred to ULevel::IncrementalUpdateComponents or this scene's StartFrame.*/
	FPhysScene* DeferredCreatePhysicsStateScene;

	/**
	 * Translucent objects with a lower sort priority draw behind objects with a higher priority.
	 * Translucent objects with the same priority are rendered from back-to-front based on their bounds origin.
	 * This setting is also used to sort objects being drawn into a runtime virtual texture.
	 *
	 * Ignored if the object is not translucent.  The default priority is zero.
	 * Warning: This should never be set to a non-default value unless you know what you are doing, as it will prevent the renderer from sorting correctly.  
	 * It is especially problematic on dynamic gameplay effects.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category=Rendering)
	int32 TranslucencySortPriority;

	/**
	 * Modified sort distance offset for translucent objects in world units.
	 * A positive number will move the sort distance further and a negative number will move the distance closer.
	 *
	 * Ignored if the object is not translucent.
	 * Warning: Adjusting this value will prevent the renderer from correctly sorting based on distance.  Only modify this value if you are certain it will not cause visual artifacts.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category = Rendering)
	float TranslucencySortDistanceOffset = 0.0f;

	/** 
	 * Array of runtime virtual textures into which we draw the mesh for this actor. 
	 * The material also needs to be set up to output to a virtual texture. 
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = VirtualTexture, meta = (DisplayName = "Draw in Virtual Textures"))
	TArray<TObjectPtr<URuntimeVirtualTexture>> RuntimeVirtualTextures;

	/** Bias to the LOD selected for rendering to runtime virtual textures. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VirtualTexture, meta = (DisplayName = "Virtual Texture LOD Bias", UIMin = "-7", UIMax = "8"))
	int8 VirtualTextureLodBias = 0;

	/**
	 * Number of lower mips in the runtime virtual texture to skip for rendering this primitive.
	 * Larger values reduce the effective draw distance in the runtime virtual texture.
	 * This culling method doesn't take into account primitive size or virtual texture size.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VirtualTexture, meta = (DisplayName = "Virtual Texture Skip Mips", UIMin = "0", UIMax = "7"))
	int8 VirtualTextureCullMips = 0;

	/**
	 * Set the minimum pixel coverage before culling from the runtime virtual texture.
	 * Larger values reduce the effective draw distance in the runtime virtual texture.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category = VirtualTexture, meta = (UIMin = "0", UIMax = "7"))
	int8 VirtualTextureMinCoverage = 0;

	/** Controls if this component draws in the main pass as well as in the virtual texture. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = VirtualTexture, meta = (DisplayName = "Draw in Main Pass"))
	ERuntimeVirtualTextureMainPassType VirtualTextureRenderPassType = ERuntimeVirtualTextureMainPassType::Exclusive;

	/** Get the array of runtime virtual textures into which we render the mesh for this actor. */
	virtual TArray<URuntimeVirtualTexture*> const& GetRuntimeVirtualTextures() const { return RuntimeVirtualTextures; }
	/** Get the runtime virtual texture pass settings. */
	virtual ERuntimeVirtualTextureMainPassType GetVirtualTextureRenderPassType() const { return VirtualTextureRenderPassType; }
	/** Get the max draw distance to use in the main pass when also rendering to a runtime virtual texture. This is combined with the other max draw distance settings. */
	virtual float GetVirtualTextureMainPassMaxDrawDistance() const { return 0.f; }

	/** Used by the renderer, to identify a component across re-registers. */
	FPrimitiveComponentId ComponentId;

	/**
	* Identifier used to track the time that this component was registered with the world / renderer.
	* Updated to unique incremental value each time OnRegister() is called. The value of 0 is unused.
	* */
	int32 RegistrationSerialNumber = -1;

	/**
	* Incremented by the main thread before being attached to the scene, decremented
	* by the rendering thread after removal. This counter exists to assert that 
	* operations are safe in order to help avoid race conditions.
	*
	*           *** Runtime logic should NEVER rely on this value. ***
	*
	* The only safe assertions to make are:
	*
	*     AttachmentCounter == 0: The primitive is not exposed to the rendering
	*                             thread, it is safe to modify shared members.
	*                             This assertion is valid ONLY from the main thread.
	*
	*     AttachmentCounter >= 1: The primitive IS exposed to the rendering
	*                             thread and therefore shared members must not
	*                             be modified. This assertion may be made from
	*                             any thread. Note that it is valid and expected
	*                             for AttachmentCounter to be larger than 1, e.g.
	*                             during reattachment.
	*/
	FThreadSafeCounter AttachmentCounter;

	/** Used to detach physics objects before simulation begins. This is needed because at runtime we can't have simulated objects inside the attachment hierarchy */
	virtual void BeginPlay() override;

protected:
	/** Returns true if all descendant components that we can possibly overlap with use relative location and rotation. */
	virtual bool AreAllCollideableDescendantsRelative(bool bAllowCachedValue = true) const;

	/** Last time we checked AreAllCollideableDescendantsRelative(), so we can throttle those tests since it rarely changes once false. */
	float LastCheckedAllCollideableDescendantsTime;

	/** Next id to be used by a component. */
	static FThreadSafeCounter NextComponentId;

	/** Next registration serial number to be assigned to a component when it is registered. */
	static FThreadSafeCounter NextRegistrationSerialNumber;

public:
	/** 
	 * Scales the bounds of the object.
	 * This is useful when using World Position Offset to animate the vertices of the object outside of its bounds. 
	 * Warning: Increasing the bounds of an object will reduce performance and shadow quality!
	 * Currently only used by StaticMeshComponent and SkeletalMeshComponent.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category=Rendering, meta=(UIMin = "1", UIMax = "10.0"))
	float BoundsScale;

	/** Last time the component was submitted for rendering (called FScene::AddPrimitive). */
	float LastSubmitTime;

private:
	/**
	 * The value of WorldSettings->TimeSeconds for the frame when this component was last rendered.  This is written
	 * from the render thread, which is up to a frame behind the game thread, so you should allow this time to
	 * be at least a frame behind the game thread's world time before you consider the actor non-visible.
	 */
	mutable float LastRenderTime;

	/** Same as LastRenderTime but only updated if the component is on screen. Used by the texture streamer. */
	mutable float LastRenderTimeOnScreen;

	float OcclusionBoundsSlack;

	friend class FPrimitiveSceneInfo;

public:
	int32 GetRayTracingGroupId() const;

	/**
	 * Returns true if this component has been rendered "recently", with a tolerance in seconds to define what "recent" means.
	 * e.g.: If a tolerance of 0.1 is used, this function will return true only if the actor was rendered in the last 0.1 seconds of game time.
	 *
	 * @param Tolerance  How many seconds ago the actor last render time can be and still count as having been "recently" rendered.
	 * @return Whether this actor was recently rendered.
	 */
	UFUNCTION(Category = "Rendering", BlueprintCallable, meta=(DisplayName="Was Component Recently Rendered", Keywords="scene visible"))
	bool WasRecentlyRendered(float Tolerance = 0.2) const;

	void SetLastRenderTime(float InLastRenderTime);
	float GetLastRenderTime() const { return LastRenderTime; }
	float GetLastRenderTimeOnScreen() const { return LastRenderTimeOnScreen; }

	/**
	 * Setup the parameter struct used to precache the PSOs used by this component. 
	 * Precaching uses certain component attributes to derive the shader or state used to render the component such as static lighting, cast shadows, ...
	 */
	virtual void SetupPrecachePSOParams(FPSOPrecacheParams& Params);

	/**
	 * Set of actors to ignore during component sweeps in MoveComponent().
	 * All components owned by these actors will be ignored when this component moves or updates overlaps.
	 * Components on the other Actor may also need to be told to do the same when they move.
	 * Does not affect movement of this component when simulating physics.
	 * @see IgnoreActorWhenMoving()
	 */
	UPROPERTY(Transient, DuplicateTransient)
	TArray<TObjectPtr<AActor>> MoveIgnoreActors;

	/**
	 * Tells this component whether to ignore collision with all components of a specific Actor when this component is moved.
	 * Components on the other Actor may also need to be told to do the same when they move.
	 * Does not affect movement of this component when simulating physics.
	 */
	UFUNCTION(BlueprintCallable, Category = "Collision", meta=(Keywords="Move MoveIgnore", UnsafeDuringActorConstruction="true"))
	void IgnoreActorWhenMoving(AActor* Actor, bool bShouldIgnore);

	/**
	 * Returns the list of actors we currently ignore when moving.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Get Move Ignore Actors", UnsafeDuringActorConstruction="true"), Category = "Collision")
	TArray<AActor*> CopyArrayOfMoveIgnoreActors();

	/**
	 * Returns the list of actors (as WeakObjectPtr) we currently ignore when moving.
	 */
	const TArray<AActor*>& GetMoveIgnoreActors() const { return MoveIgnoreActors; }

	/**
	 * Clear the list of actors we ignore when moving.
	 */
	UFUNCTION(BlueprintCallable, Category = "Collision", meta=(UnsafeDuringActorConstruction="true"))
	void ClearMoveIgnoreActors();

	/**
	* Set of components to ignore during component sweeps in MoveComponent().
	* These components will be ignored when this component moves or updates overlaps.
	* The other components may also need to be told to do the same when they move.
	* Does not affect movement of this component when simulating physics.
	* @see IgnoreComponentWhenMoving()
	*/
	UPROPERTY(Transient, DuplicateTransient)
	TArray<TObjectPtr<UPrimitiveComponent>> MoveIgnoreComponents;

	/**
	* Tells this component whether to ignore collision with another component when this component is moved.
	* The other components may also need to be told to do the same when they move.
	* Does not affect movement of this component when simulating physics.
	*/
	UFUNCTION(BlueprintCallable, Category = "Collision", meta=(Keywords="Move MoveIgnore", UnsafeDuringActorConstruction="true"))
	void IgnoreComponentWhenMoving(UPrimitiveComponent* Component, bool bShouldIgnore);

	/**
	* Returns the list of actors we currently ignore when moving.
	*/
	UFUNCTION(BlueprintCallable, meta=(DisplayName="Get Move Ignore Components", UnsafeDuringActorConstruction="true"), Category = "Collision")
	TArray<UPrimitiveComponent*> CopyArrayOfMoveIgnoreComponents();

	/**
	* Returns the list of components we currently ignore when moving.
	*/
	const TArray<UPrimitiveComponent*>& GetMoveIgnoreComponents() const { return MoveIgnoreComponents; }

	/**
	* Clear the list of components we ignore when moving.
	*/
	UFUNCTION(BlueprintCallable, Category = "Collision", meta=(UnsafeDuringActorConstruction="true"))
	void ClearMoveIgnoreComponents() { MoveIgnoreComponents.Empty(); }

	/** Set the mask filter we use when moving. */
	void SetMoveIgnoreMask(FMaskFilter InMoveIgnoreMask);

	/** Get the mask filter we use when moving. */
	FMaskFilter GetMoveIgnoreMask() const { return MoveIgnoreMask; }

	/** Set the mask filter checked when others move into us. */
	void SetMaskFilterOnBodyInstance(FMaskFilter InMaskFilter) { BodyInstance.SetMaskFilter(InMaskFilter); }

	/** Get the mask filter checked when others move into us. */
	FMaskFilter GetMaskFilterOnBodyInstance(FMaskFilter InMaskFilter) const { return BodyInstance.GetMaskFilter(); }

	/**
	 * Gets the index of the scalar parameter for the custom primitive data array
	 * @param	ParameterName	The parameter name of the custom primitive
	 * @return	The index of the custom primitive, INDEX_NONE (-1) if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	int32 GetCustomPrimitiveDataIndexForScalarParameter(FName ParameterName) const;

	/**
	 * Gets the index of the vector parameter for the custom primitive data array
	 * @param	ParameterName	The parameter name of the custom primitive
	 * @return	The index of the custom primitive, INDEX_NONE (-1) if not found
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	int32 GetCustomPrimitiveDataIndexForVectorParameter(FName ParameterName) const;

	/**
	 * Set a scalar parameter for custom primitive data. This sets the run-time data only, so it doesn't serialize.
	 * @param	ParameterName	The parameter name of the custom primitive
	 * @param	Value			The new value of the custom primitive
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetScalarParameterForCustomPrimitiveData(FName ParameterName, float Value);

	/**
	 * Set a vector parameter for custom primitive data. This sets the run-time data only, so it doesn't serialize.
	 * @param	ParameterName	The parameter name of the custom primitive
	 * @param	Value			The new value of the custom primitive
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetVectorParameterForCustomPrimitiveData(FName ParameterName, FVector4 Value);

	/** Set custom primitive data at index DataIndex. This sets the run-time data only, so it doesn't serialize. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material")
	void SetCustomPrimitiveDataFloat(int32 DataIndex, float Value);

	/** Set custom primitive data, two floats at once, from index DataIndex to index DataIndex + 1. This sets the run-time data only, so it doesn't serialize. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material")
	void SetCustomPrimitiveDataVector2(int32 DataIndex, FVector2D Value);

	/** Set custom primitive data, three floats at once, from index DataIndex to index DataIndex + 2. This sets the run-time data only, so it doesn't serialize. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material")
	void SetCustomPrimitiveDataVector3(int32 DataIndex, FVector Value);

	/** Set custom primitive data, four floats at once, from index DataIndex to index DataIndex + 3. This sets the run-time data only, so it doesn't serialize. */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material")
	void SetCustomPrimitiveDataVector4(int32 DataIndex, FVector4 Value);

	/** 
	 * Get the custom primitive data for this primitive component.
	 * @return The payload of custom data that will be set on the primitive and accessible in the material through a material expression.
	 */
	const FCustomPrimitiveData& GetCustomPrimitiveData() const { return CustomPrimitiveDataInternal; }

	/**
	 * Set a scalar parameter for default custom primitive data. This will be serialized and is useful in construction scripts.
	 * @param	ParameterName	The parameter name of the custom primitive
	 * @param	Value			The new value of the custom primitive
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetScalarParameterForDefaultCustomPrimitiveData(FName ParameterName, float Value);

	/**
	 * Set a vector parameter for default custom primitive data. This will be serialized and is useful in construction scripts.
	 * @param	ParameterName	The parameter name of the custom primitive
	 * @param	Value			The new value of the custom primitive
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetVectorParameterForDefaultCustomPrimitiveData(FName ParameterName, FVector4 Value);

	/** Set default custom primitive data at index DataIndex, and marks the render state dirty */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetDefaultCustomPrimitiveDataFloat(int32 DataIndex, float Value);

	/** Set default custom primitive data, two floats at once, from index DataIndex to index DataIndex + 1, and marks the render state dirty */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetDefaultCustomPrimitiveDataVector2(int32 DataIndex, FVector2D Value);

	/** Set default custom primitive data, three floats at once, from index DataIndex to index DataIndex + 2, and marks the render state dirty */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetDefaultCustomPrimitiveDataVector3(int32 DataIndex, FVector Value);

	/** Set default custom primitive data, four floats at once, from index DataIndex to index DataIndex + 3, and marks the render state dirty */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	void SetDefaultCustomPrimitiveDataVector4(int32 DataIndex, FVector4 Value);

	/**
	 * Get the default custom primitive data for this primitive component.
	 * @return The payload of custom data that will be set on the primitive and accessible in the material through a material expression.
	 */
	const FCustomPrimitiveData& GetDefaultCustomPrimitiveData() const { return CustomPrimitiveData; }

#if WITH_EDITOR
	/** Override delegate used for checking the selection state of a component */
	DECLARE_DELEGATE_RetVal_OneParam( bool, FSelectionOverride, const UPrimitiveComponent* );
	FSelectionOverride SelectionOverrideDelegate;
#endif

protected:

	/** Reset the custom primitive data of this primitive to the optional user defined default */
	void ResetCustomPrimitiveData();

	/** Insert an array of floats into the CustomPrimitiveData, starting at the given index */
	void SetCustomPrimitiveDataInternal(int32 DataIndex, const TArray<float>& Values);

	/** Insert an array of floats into the CustomPrimitiveData defaults, starting at the given index */
	void SetDefaultCustomPrimitiveData(int32 DataIndex, const TArray<float>& Values);

	/** Set of components that this component is currently overlapping. */
	TArray<FOverlapInfo> OverlappingComponents;

private:
	/** Convert a set of overlaps from a sweep to a subset that includes only those at the end location (filling in OverlapsAtEndLocation). */
	template<typename AllocatorType>
	bool ConvertSweptOverlapsToCurrentOverlaps(TArray<FOverlapInfo, AllocatorType>& OutOverlapsAtEndLocation, const TOverlapArrayView& SweptOverlaps, int32 SweptOverlapsIndex, const FVector& EndLocation, const FQuat& EndRotationQuat);

	/** Convert a set of overlaps from a symmetric change in rotation to a subset that includes only those at the end location (filling in OverlapsAtEndLocation). */
	template<typename AllocatorType>
	bool ConvertRotationOverlapsToCurrentOverlaps(TArray<FOverlapInfo, AllocatorType>& OutOverlapsAtEndLocation, const TOverlapArrayView& CurrentOverlaps);

	template<typename AllocatorType>
	bool GetOverlapsWithActor_Template(const AActor* Actor, TArray<FOverlapInfo, AllocatorType>& OutOverlaps) const;

	// FScopedMovementUpdate needs access to the above two functions.
	friend FScopedMovementUpdate;

public:
	/** 
	 * Begin tracking an overlap interaction with the component specified.
	 * @param OtherComp - The component of the other actor that this component is now overlapping
	 * @param bDoNotifies - True to dispatch appropriate begin/end overlap notifications when these events occur.
	 * @see [Overlap Events](https://docs.unrealengine.com/InteractiveExperiences/Physics/Collision/Overview#overlapandgenerateoverlapevents)
	 */
	void BeginComponentOverlap(const FOverlapInfo& OtherOverlap, bool bDoNotifies);
	
	/** 
	 * Finish tracking an overlap interaction that is no longer occurring between this component and the component specified. 
	 * @param OtherComp The component of the other actor to stop overlapping
	 * @param bDoNotifies True to dispatch appropriate begin/end overlap notifications when these events occur.
	 * @param bSkipNotifySelf True to skip end overlap notifications to this component's.  Does not affect notifications to OtherComp's actor.
	 * @see [Overlap Events](https://docs.unrealengine.com/InteractiveExperiences/Physics/Collision/Overview#overlapandgenerateoverlapevents)
	 */
	void EndComponentOverlap(const FOverlapInfo& OtherOverlap, bool bDoNotifies=true, bool bSkipNotifySelf=false);

	/**
	 * Check whether this component is overlapping another component.
	 * @param OtherComp Component to test this component against.
	 * @return Whether this component is overlapping another component.
	 */
	UFUNCTION(BlueprintPure, Category="Collision", meta=(UnsafeDuringActorConstruction="true"))
	bool IsOverlappingComponent(const UPrimitiveComponent* OtherComp) const;
	
	/** Check whether this component has the specified overlap. */
	bool IsOverlappingComponent(const FOverlapInfo& Overlap) const;

	/**
	 * Check whether this component is overlapping any component of the given Actor.
	 * @param Other Actor to test this component against.
	 * @return Whether this component is overlapping any component of the given Actor.
	 */
	UFUNCTION(BlueprintPure, Category="Collision", meta=(UnsafeDuringActorConstruction="true"))
	bool IsOverlappingActor(const AActor* Other) const;

	/** Appends list of overlaps with components owned by the given actor to the 'OutOverlaps' array. Returns true if any overlaps were added. */
	bool GetOverlapsWithActor(const AActor* Actor, TArray<FOverlapInfo>& OutOverlaps) const;

	/** 
	 * Returns a list of actors that this component is overlapping.
	 * @param OverlappingActors		[out] Returned list of overlapping actors
	 * @param ClassFilter			[optional] If set, only returns actors of this class or subclasses
	 */
	UFUNCTION(BlueprintPure, Category="Collision", meta=(UnsafeDuringActorConstruction="true"))
	void GetOverlappingActors(TArray<AActor*>& OverlappingActors, TSubclassOf<AActor> ClassFilter=nullptr) const;

	/** 
	* Returns the set of actors that this component is overlapping.
	* @param OverlappingActors		[out] Returned list of overlapping actors
	* @param ClassFilter			[optional] If set, only returns actors of this class or subclasses
	*/
	void GetOverlappingActors(TSet<AActor*>& OverlappingActors, TSubclassOf<AActor> ClassFilter=nullptr) const;

	/** Returns unique list of components this component is overlapping. */
	UFUNCTION(BlueprintPure, Category="Collision", meta=(UnsafeDuringActorConstruction="true"))
	void GetOverlappingComponents(TArray<UPrimitiveComponent*>& OutOverlappingComponents) const;

	/** Returns unique set of components this component is overlapping. */
	void GetOverlappingComponents(TSet<UPrimitiveComponent*>& OutOverlappingComponents) const;

	/** Returns list of components this component is overlapping. */
	const TArray<FOverlapInfo>& GetOverlapInfos() const;

	/** 
	 * Queries world and updates overlap tracking state for this component.
	 * @param NewPendingOverlaps		An ordered list of components that the MovedComponent overlapped during its movement (eg. generated during a sweep). Only used to add potentially new overlaps.
	 *									Might not be overlapping them now.
	 * @param bDoNotifies				True to dispatch being/end overlap notifications when these events occur.
	 * @param OverlapsAtEndLocation		If non-null, the given list of overlaps will be used as the overlaps for this component at the current location, rather than checking for them with a scene query.
	 *									Generally this should only be used if this component is the RootComponent of the owning actor and overlaps with other descendant components have been verified.
	 * @return							True if we can skip calling this in the future (i.e. no useful work is being done.)
	 */
	virtual bool UpdateOverlapsImpl(const TOverlapArrayView* NewPendingOverlaps=nullptr, bool bDoNotifies=true, const TOverlapArrayView* OverlapsAtEndLocation=nullptr) override;

#if WITH_EDITOR
	/**
	 * Whether or not the bounds of this component should be considered when focusing the editor camera to an actor with this component in it.
	 * Useful for debug components which need a bounds for rendering but don't contribute to the visible part of the mesh in a meaningful way
	 */
	virtual bool IgnoreBoundsForEditorFocus() const { return bIgnoreBoundsForEditorFocus; }
	
	/**
	 * Set if we should ignore bounds when focusing the editor camera.
	*/
	void SetIgnoreBoundsForEditorFocus(bool bIgnore) { bIgnoreBoundsForEditorFocus = bIgnore; }
#endif

	/** Update current physics volume for this component, if bShouldUpdatePhysicsVolume is true. Overridden to use the overlaps to find the physics volume. */
	virtual void UpdatePhysicsVolume( bool bTriggerNotifiers ) override;

	/**
	 *  Test the collision of the supplied component at the supplied location/rotation, and determine the set of components that it overlaps.
	 *  @note This overload taking rotation as a FQuat is slightly faster than the version using FRotator.
	 *  @note This simply calls the virtual ComponentOverlapMultiImpl() which can be overridden to implement custom behavior.
	 *  @param  OutOverlaps     Array of overlaps found between this component in specified pose and the world
	 *  @param  World			World to use for overlap test
	 *  @param  Pos             Location of component's geometry for the test against the world
	 *  @param  Rot             Rotation of component's geometry for the test against the world
	 *  @param  TestChannel		The 'channel' that this ray is in, used to determine which components to hit
	 *  @param	ObjectQueryParams	List of object types it's looking for. When this enters, we do object query with component shape
	 *  @return true if OutOverlaps contains any blocking results
	 */
	bool ComponentOverlapMulti(TArray<struct FOverlapResult>& OutOverlaps, const class UWorld* InWorld, const FVector& Pos, const FQuat& Rot, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams, const struct FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam) const;
	bool ComponentOverlapMulti(TArray<struct FOverlapResult>& OutOverlaps, const class UWorld* InWorld, const FVector& Pos, const FRotator& Rot, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params = FComponentQueryParams::DefaultComponentQueryParams, const struct FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam) const;

	/**
	 *	Walks up the attachment tree until a primitive component with LightAttachmentsAsGroup enabled is found. This component will effectively act as the root of the attachment group.
	 *	Return nullptr if none is found. 
	 */
	const UPrimitiveComponent* GetLightingAttachmentRoot() const;

protected:
	/** Override this method for custom behavior for ComponentOverlapMulti() */
	virtual bool ComponentOverlapMultiImpl(TArray<struct FOverlapResult>& OutOverlaps, const class UWorld* InWorld, const FVector& Pos, const FQuat& Rot, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams = FCollisionObjectQueryParams::DefaultObjectQueryParam) const;

public:
	// Internal physics engine data.
	
	/** Physics scene information for this component, holds a single rigid body with multiple shapes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=Collision, meta=(ShowOnlyInnerProperties, SkipUCSModifiedProperties))
	FBodyInstance BodyInstance;

	/** 
	 *	Event called when a component hits (or is hit by) something solid. This could happen due to things like Character movement, using Set Location with 'sweep' enabled, or physics simulation.
	 *	For events when objects overlap (e.g. walking into a trigger) see the 'Overlap' event.
	 *
	 *	@note For collisions during physics simulation to generate hit events, 'Simulation Generates Hit Events' must be enabled for this component.
	 *	@note When receiving a hit from another object's movement, the directions of 'Hit.Normal' and 'Hit.ImpactNormal'
	 *	will be adjusted to indicate force from the other object against this object.
	 *	@note NormalImpulse will be filled in for physics-simulating bodies, but will be zero for swept-component blocking collisions.
	 */
	UPROPERTY(BlueprintAssignable, Category="Collision")
	FComponentHitSignature OnComponentHit;

	/** 
	 *	Event called when something starts to overlaps this component, for example a player walking into a trigger.
	 *	For events when objects have a blocking collision, for example a player hitting a wall, see 'Hit' events.
	 *
	 *	@note Both this component and the other one must have GetGenerateOverlapEvents() set to true to generate overlap events.
	 *	@note When receiving an overlap from another object's movement, the directions of 'Hit.Normal' and 'Hit.ImpactNormal'
	 *	will be adjusted to indicate force from the other object against this object.
	 */
	UPROPERTY(BlueprintAssignable, Category="Collision")
	FComponentBeginOverlapSignature OnComponentBeginOverlap;

	/** 
	 *	Event called when something stops overlapping this component 
	 *	@note Both this component and the other one must have GetGenerateOverlapEvents() set to true to generate overlap events.
	 */
	UPROPERTY(BlueprintAssignable, Category="Collision")
	FComponentEndOverlapSignature OnComponentEndOverlap;

	/** 
	 *	Event called when the underlying physics objects is woken up
	 */
	UPROPERTY(BlueprintAssignable, Category="Collision")
	FComponentWakeSignature OnComponentWake;

	/** 
	 *	Event called when the underlying physics objects is put to sleep
	 */
	UPROPERTY(BlueprintAssignable, Category = "Collision")
	FComponentSleepSignature OnComponentSleep;

	/**
	 *	Event called when collision settings change for this component.
	 */
	FComponentCollisionSettingsChangedSignature OnComponentCollisionSettingsChangedEvent;

	/**
	 *	Event called when physics state is created or destroyed for this component
	 */
	UPROPERTY(BlueprintAssignable, Category = "Physics")
	FComponentPhysicsStateChanged OnComponentPhysicsStateChanged;

	/** Event called when the mouse cursor is moved over this component and mouse over events are enabled in the player controller */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FComponentBeginCursorOverSignature OnBeginCursorOver;
		 
	/** Event called when the mouse cursor is moved off this component and mouse over events are enabled in the player controller */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FComponentEndCursorOverSignature OnEndCursorOver;

	/** Event called when the left mouse button is clicked while the mouse is over this component and click events are enabled in the player controller */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FComponentOnClickedSignature OnClicked;

	/** Event called when the left mouse button is released while the mouse is over this component click events are enabled in the player controller */
	UPROPERTY(BlueprintAssignable, Category="Input|Mouse Input")
	FComponentOnReleasedSignature OnReleased;
		 
	/** Event called when a touch input is received over this component when touch events are enabled in the player controller */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FComponentOnInputTouchBeginSignature OnInputTouchBegin;

	/** Event called when a touch input is released over this component when touch events are enabled in the player controller */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FComponentOnInputTouchEndSignature OnInputTouchEnd;

	/** Event called when a finger is moved over this component when touch over events are enabled in the player controller */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FComponentBeginTouchOverSignature OnInputTouchEnter;

	/** Event called when a finger is moved off this component when touch over events are enabled in the player controller */
	UPROPERTY(BlueprintAssignable, Category="Input|Touch Input")
	FComponentEndTouchOverSignature OnInputTouchLeave;

	/**
	 * Defines how quickly it should be culled. For example buildings should have a low priority, but small dressing should have a high priority.
	 */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = RayTracing)
	ERayTracingGroupCullingPriority RayTracingGroupCullingPriority;

	/** Mask used for stencil buffer writes. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, BlueprintReadOnly, Category = "Rendering", meta = (editcondition = "bRenderCustomDepth"))
	ERendererStencilMask CustomDepthStencilWriteMask;

	/** Scale the bounds of this object, used for frustum culling. Useful for features like WorldPositionOffset. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetBoundsScale(float NewBoundsScale=1.f);

	/**
	 * Returns the material used by the element at the specified index
	 * @param ElementIndex - The element to access the material of.
	 * @return the material used by the indexed element of this mesh.
	 */
	UFUNCTION(BlueprintPure, Category="Rendering|Material")
	virtual class UMaterialInterface* GetMaterial(int32 ElementIndex) const;

	/** Returns the material to show in the editor details panel as being used. */
	virtual class UMaterialInterface* GetEditorMaterial(int32 ElementIndex) const 
	{ 
		return GetMaterial(ElementIndex);
	}

	/**
	 * Changes the material applied to an element of the mesh.
	 * @param ElementIndex - The element to access the material of.
	 * @return the material used by the indexed element of this mesh.
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material")
	virtual void SetMaterial(int32 ElementIndex, class UMaterialInterface* Material);

	/**
	* Changes the material applied to an element of the mesh.
	* @param MaterialSlotName - The slot name to access the material of.
	* @return the material used by the indexed element of this mesh.
	*/
	UFUNCTION(BlueprintCallable, Category = "Rendering|Material")
	virtual void SetMaterialByName(FName MaterialSlotName, class UMaterialInterface* Material);

	/**
	 * Creates a Dynamic Material Instance for the specified element index.  The parent of the instance is set to the material being replaced.
	 * @param ElementIndex - The index of the skin to replace the material for.  If invalid, the material is unchanged and NULL is returned.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "CreateMIDForElement", DeprecatedFunction, DeprecationMessage="Use CreateDynamicMaterialInstance instead."), Category="Rendering|Material")
	virtual class UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamic(int32 ElementIndex);

	/**
	 * Creates a Dynamic Material Instance for the specified element index.  The parent of the instance is set to the material being replaced.
	 * @param ElementIndex - The index of the skin to replace the material for.  If invalid, the material is unchanged and NULL is returned.
	 */
	UFUNCTION(BlueprintCallable, meta=(DisplayName = "CreateMIDForElementFromMaterial", DeprecatedFunction, DeprecationMessage="Use CreateDynamicMaterialInstance instead."), Category="Rendering|Material")
	virtual class UMaterialInstanceDynamic* CreateAndSetMaterialInstanceDynamicFromMaterial(int32 ElementIndex, class UMaterialInterface* Parent);

	/**
	 * Creates a Dynamic Material Instance for the specified element index, optionally from the supplied material.
	 * @param ElementIndex - The index of the skin to replace the material for.  If invalid, the material is unchanged and NULL is returned.
	 */
	UFUNCTION(BlueprintCallable, Category="Rendering|Material")
	virtual class UMaterialInstanceDynamic* CreateDynamicMaterialInstance(int32 ElementIndex, class UMaterialInterface* SourceMaterial = NULL, FName OptionalName = NAME_None);

	/** 
	 * Try and retrieve the material applied to a particular collision face of mesh. Used with face index returned from collision trace. 
	 *	@param	FaceIndex		Face index from hit result that was hit by a trace
	 *	@param	SectionIndex	Section of the mesh that the face belongs to
	 *	@return					Material applied to section that the hit face belongs to
	 */
	UFUNCTION(BlueprintPure, Category = "Rendering|Material")
	virtual UMaterialInterface* GetMaterialFromCollisionFaceIndex(int32 FaceIndex, int32& SectionIndex) const;

	/** Returns the slope override struct for this component. */
	UFUNCTION(BlueprintPure, Category="Physics")
	const struct FWalkableSlopeOverride& GetWalkableSlopeOverride() const;

	/** Sets a new slope override for this component instance. */
	UFUNCTION(BlueprintCallable, Category="Physics")
	virtual void SetWalkableSlopeOverride(const FWalkableSlopeOverride& NewOverride);

	/** 
	 *	Sets whether or not a single body should use physics simulation, or should be 'fixed' (kinematic).
	 *	Note that if this component is currently attached to something, beginning simulation will detach it.
	 *
	 *	@param	bSimulate	New simulation state for single body
	 */
	UFUNCTION(BlueprintCallable, Category="Physics")
	virtual void SetSimulatePhysics(bool bSimulate);

	/**
	 * Determines whether or not the simulate physics setting can be edited interactively on this component
	 */
	virtual bool CanEditSimulatePhysics();

	/**
	* Sets the constraint mode of the component.
	* @param ConstraintMode	The type of constraint to use.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Set Constraint Mode", Keywords = "set locked axis constraint physics"), Category = Physics)
	virtual void SetConstraintMode(EDOFMode::Type ConstraintMode);

	/**
	 *	Add an impulse to a single rigid body. Good for one time instant burst.
	 *
	 *	@param	Impulse		Magnitude and direction of impulse to apply.
	 *	@param	BoneName	If a SkeletalMeshComponent, name of body to apply impulse to. 'None' indicates root body.
	 *	@param	bVelChange	If true, the Strength is taken as a change in velocity instead of an impulse (ie. mass will have no effect).
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void AddImpulse(FVector Impulse, FName BoneName = NAME_None, bool bVelChange = false);

	/**
	*	Add an angular impulse to a single rigid body. Good for one time instant burst.
	*
	*	@param	AngularImpulse	Magnitude and direction of impulse to apply. Direction is axis of rotation.
	*	@param	BoneName	If a SkeletalMeshComponent, name of body to apply angular impulse to. 'None' indicates root body.
	*	@param	bVelChange	If true, the Strength is taken as a change in angular velocity instead of an impulse (ie. mass will have no effect).
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void AddAngularImpulseInRadians(FVector Impulse, FName BoneName = NAME_None, bool bVelChange = false);

	/**
	*	Add an angular impulse to a single rigid body. Good for one time instant burst.
	*
	*	@param	AngularImpulse	Magnitude and direction of impulse to apply. Direction is axis of rotation.
	*	@param	BoneName	If a SkeletalMeshComponent, name of body to apply angular impulse to. 'None' indicates root body.
	*	@param	bVelChange	If true, the Strength is taken as a change in angular velocity instead of an impulse (ie. mass will have no effect).
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics", meta=(UnsafeDuringActorConstruction="true"))
	void AddAngularImpulseInDegrees(FVector Impulse, FName BoneName = NAME_None, bool bVelChange = false)
	{
		AddAngularImpulseInRadians(FMath::DegreesToRadians(Impulse), BoneName, bVelChange);
	}

	/**
	 *	Add an impulse to a single rigid body at a specific location. 
	 *
	 *	@param	Impulse		Magnitude and direction of impulse to apply.
	 *	@param	Location	Point in world space to apply impulse at.
	 *	@param	BoneName	If a SkeletalMeshComponent, name of bone to apply impulse to. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void AddImpulseAtLocation(FVector Impulse, FVector Location, FName BoneName = NAME_None);

	/**
	 *	Add an impulse to a single rigid body at a specific location. The Strength is taken as a change in angular velocity instead of an impulse (ie. mass will have no effect).
	 *
	 *	@param	Impulse		Magnitude and direction of impulse to apply.
	 *	@param	Location	Point in world space to apply impulse at.
	 *	@param	BoneName	If a SkeletalMeshComponent, name of bone to apply impulse to. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category = "Physics", meta = (UnsafeDuringActorConstruction = "true"))
	virtual void AddVelocityChangeImpulseAtLocation(FVector Impulse, FVector Location, FName BoneName = NAME_None);


	/**
	 * Add an impulse to all rigid bodies in this component, radiating out from the specified position.
	 *
	 * @param Origin		Point of origin for the radial impulse blast, in world space
	 * @param Radius		Size of radial impulse. Beyond this distance from Origin, there will be no affect.
	 * @param Strength		Maximum strength of impulse applied to body.
	 * @param Falloff		Allows you to control the strength of the impulse as a function of distance from Origin.
	 * @param bVelChange	If true, the Strength is taken as a change in velocity instead of an impulse (ie. mass will have no effect).
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void AddRadialImpulse(FVector Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, bool bVelChange = false);

	/**
	 *	Add a force to a single rigid body.
	 *  This is like a 'thruster'. Good for adding a burst over some (non zero) time. Should be called every frame for the duration of the force.
	 *
	 *	@param	Force		 Force vector to apply. Magnitude indicates strength of force.
	 *	@param	BoneName	 If a SkeletalMeshComponent, name of body to apply force to. 'None' indicates root body.
	 *  @param  bAccelChange If true, Force is taken as a change in acceleration instead of a physical force (i.e. mass will have no effect).
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void AddForce(FVector Force, FName BoneName = NAME_None, bool bAccelChange = false);

	/**
	 *	Add a force to a single rigid body at a particular location in world space.
	 *  This is like a 'thruster'. Good for adding a burst over some (non zero) time. Should be called every frame for the duration of the force.
	 *
	 *	@param Force		Force vector to apply. Magnitude indicates strength of force.
	 *	@param Location		Location to apply force, in world space.
	 *	@param BoneName		If a SkeletalMeshComponent, name of body to apply force to. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void AddForceAtLocation(FVector Force, FVector Location, FName BoneName = NAME_None);

	/**
	 *	Add a force to a single rigid body at a particular location. Both Force and Location should be in body space.
	 *  This is like a 'thruster'. Good for adding a burst over some (non zero) time. Should be called every frame for the duration of the force.
	 *
	 *	@param Force		Force vector to apply. Magnitude indicates strength of force.
	 *	@param Location		Location to apply force, in component space.
	 *	@param BoneName		If a SkeletalMeshComponent, name of body to apply force to. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void AddForceAtLocationLocal(FVector Force, FVector Location, FName BoneName = NAME_None);

	/**
	 *	Add a force to all bodies in this component, originating from the supplied world-space location.
	 *
	 *	@param Origin		Origin of force in world space.
	 *	@param Radius		Radius within which to apply the force.
	 *	@param Strength		Strength of force to apply.
	 *  @param Falloff		Allows you to control the strength of the force as a function of distance from Origin.
	 *  @param bAccelChange If true, Strength is taken as a change in acceleration instead of a physical force (i.e. mass will have no effect).
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void AddRadialForce(FVector Origin, float Radius, float Strength, enum ERadialImpulseFalloff Falloff, bool bAccelChange = false);

	/**
	 *	Add a torque to a single rigid body.
	 *	@param Torque		Torque to apply. Direction is axis of rotation and magnitude is strength of torque.
	 *	@param BoneName		If a SkeletalMeshComponent, name of body to apply torque to. 'None' indicates root body.
	 *  @param bAccelChange If true, Torque is taken as a change in angular acceleration instead of a physical torque (i.e. mass will have no effect).
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void AddTorqueInRadians(FVector Torque, FName BoneName = NAME_None, bool bAccelChange = false);

	/**
	 *	Add a torque to a single rigid body.
	 *	@param Torque		Torque to apply. Direction is axis of rotation and magnitude is strength of torque.
	 *	@param BoneName		If a SkeletalMeshComponent, name of body to apply torque to. 'None' indicates root body.
	 *	@param bAccelChange If true, Torque is taken as a change in angular acceleration instead of a physical torque (i.e. mass will have no effect).
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	void AddTorqueInDegrees(FVector Torque, FName BoneName = NAME_None, bool bAccelChange = false)
	{
		AddTorqueInRadians(FMath::DegreesToRadians(Torque), BoneName, bAccelChange);
	}

	/**
	 *	Set the linear velocity of a single body.
	 *	This should be used cautiously - it may be better to use AddForce or AddImpulse.
	 *
	 *	@param NewVel			New linear velocity to apply to physics.
	 *	@param bAddToCurrent	If true, NewVel is added to the existing velocity of the body.
	 *	@param BoneName			If a SkeletalMeshComponent, name of body to modify velocity of. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void SetPhysicsLinearVelocity(FVector NewVel, bool bAddToCurrent = false, FName BoneName = NAME_None);

	/** 
	 *	Get the linear velocity of a single body. 
	 *	@param BoneName			If a SkeletalMeshComponent, name of body to get velocity of. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))	
	FVector GetPhysicsLinearVelocity(FName BoneName = NAME_None);

	/**
	*	Get the linear velocity of a point on a single body.
	*	@param Point			Point is specified in world space.
	*	@param BoneName			If a SkeletalMeshComponent, name of body to get velocity of. 'None' indicates root body.
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics", meta=(UnsafeDuringActorConstruction="true"))
	FVector GetPhysicsLinearVelocityAtPoint(FVector Point, FName BoneName = NAME_None);

	/**
	 *	Set the linear velocity of all bodies in this component.
	 *
	 *	@param NewVel			New linear velocity to apply to physics.
	 *	@param bAddToCurrent	If true, NewVel is added to the existing velocity of the body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void SetAllPhysicsLinearVelocity(FVector NewVel, bool bAddToCurrent = false);

	/**
	 *	Set the angular velocity of a single body.
	 *	This should be used cautiously - it may be better to use AddTorque or AddImpulse.
	 *
	 *	@param NewAngVel		New angular velocity to apply to body, in radians per second.
	 *	@param bAddToCurrent	If true, NewAngVel is added to the existing angular velocity of the body.
	 *	@param BoneName			If a SkeletalMeshComponent, name of body to modify angular velocity of. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void SetPhysicsAngularVelocityInRadians(FVector NewAngVel, bool bAddToCurrent = false, FName BoneName = NAME_None);

	/**
	 *	Set the angular velocity of a single body.
	 *	This should be used cautiously - it may be better to use AddTorque or AddImpulse.
	 *
	 *	@param NewAngVel		New angular velocity to apply to body, in degrees per second.
	 *	@param bAddToCurrent	If true, NewAngVel is added to the existing angular velocity of the body.
	 *	@param BoneName			If a SkeletalMeshComponent, name of body to modify angular velocity of. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	void SetPhysicsAngularVelocityInDegrees(FVector NewAngVel, bool bAddToCurrent = false, FName BoneName = NAME_None)
	{
		SetPhysicsAngularVelocityInRadians(FMath::DegreesToRadians(NewAngVel), bAddToCurrent, BoneName);
	}

	/**
	*	Set the maximum angular velocity of a single body.
	*
	*	@param NewMaxAngVel		New maximum angular velocity to apply to body, in degrees per second.
	*	@param bAddToCurrent	If true, NewMaxAngVel is added to the existing maximum angular velocity of the body.
	*	@param BoneName			If a SkeletalMeshComponent, name of body to modify maximum angular velocity of. 'None' indicates root body.
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics", meta=(UnsafeDuringActorConstruction="true"))
	void SetPhysicsMaxAngularVelocityInDegrees(float NewMaxAngVel, bool bAddToCurrent = false, FName BoneName = NAME_None)
	{
		SetPhysicsMaxAngularVelocityInRadians(FMath::DegreesToRadians(NewMaxAngVel), bAddToCurrent, BoneName);
	}

	/**
	*	Set the maximum angular velocity of a single body.
	*
	*	@param NewMaxAngVel		New maximum angular velocity to apply to body, in radians per second.
	*	@param bAddToCurrent	If true, NewMaxAngVel is added to the existing maximum angular velocity of the body.
	*	@param BoneName			If a SkeletalMeshComponent, name of body to modify maximum angular velocity of. 'None' indicates root body.
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics", meta=(UnsafeDuringActorConstruction="true"))
	void SetPhysicsMaxAngularVelocityInRadians(float NewMaxAngVel, bool bAddToCurrent = false, FName BoneName = NAME_None);

	/** 
	 *	Get the angular velocity of a single body, in degrees per second. 
	 *	@param BoneName			If a SkeletalMeshComponent, name of body to get velocity of. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))	
	FVector GetPhysicsAngularVelocityInDegrees(FName BoneName = NAME_None) const
	{
		return FMath::RadiansToDegrees(GetPhysicsAngularVelocityInRadians(BoneName));
	}

	/** 
	 *	Get the angular velocity of a single body, in radians per second. 
	 *	@param BoneName			If a SkeletalMeshComponent, name of body to get velocity of. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))	
	FVector GetPhysicsAngularVelocityInRadians(FName BoneName = NAME_None) const;

	/**
	*	Get the center of mass of a single body. In the case of a welded body this will return the center of mass of the entire welded body (including its parent and children)
	*   Objects that are not simulated return (0,0,0) as they do not have COM
	*	@param BoneName			If a SkeletalMeshComponent, name of body to get center of mass of. 'None' indicates root body.
	*/
	UFUNCTION(BlueprintPure, Category = "Physics", meta=(UnsafeDuringActorConstruction="true"))
	FVector GetCenterOfMass(FName BoneName = NAME_None) const;

	/**
	*	Set the center of mass of a single body. This will offset the physx-calculated center of mass.
	*	Note that in the case where multiple bodies are attached together, the center of mass will be set for the entire group.
	*	@param CenterOfMassOffset		User specified offset for the center of mass of this object, from the calculated location.
	*	@param BoneName			If a SkeletalMeshComponent, name of body to set center of mass of. 'None' indicates root body.
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics", meta=(UnsafeDuringActorConstruction="true"))
	void SetCenterOfMass(FVector CenterOfMassOffset, FName BoneName = NAME_None);

	/**
	 *	'Wake' physics simulation for a single body.
	 *	@param	BoneName	If a SkeletalMeshComponent, name of body to wake. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void WakeRigidBody(FName BoneName = NAME_None);

	/** 
	 *	Force a single body back to sleep. 
	 *	@param	BoneName	If a SkeletalMeshComponent, name of body to put to sleep. 'None' indicates root body.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	void PutRigidBodyToSleep(FName BoneName = NAME_None);

	/** Changes the value of bNotifyRigidBodyCollision */
	UFUNCTION(BlueprintCallable, Category="Physics")
	virtual void SetNotifyRigidBodyCollision(bool bNewNotifyRigidBodyCollision);

	/** Changes the value of bOwnerNoSee. */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void SetOwnerNoSee(bool bNewOwnerNoSee);
	
	/** Changes the value of bOnlyOwnerSee. */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void SetOnlyOwnerSee(bool bNewOnlyOwnerSee);

	/** Changes the value of bIsVisibleInRayTracing. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetVisibleInRayTracing(bool bNewVisibleInRayTracing);

	/** Changes the value of CastShadow. */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void SetCastShadow(bool NewCastShadow);

	/** Changes the value of EmissiveLightSource. */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void SetEmissiveLightSource(bool NewEmissiveLightSource);

	/** Changes the value of CastHiddenShadow. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetCastHiddenShadow(bool NewCastHiddenShadow);

	/** Changes the value of CastInsetShadow. */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void SetCastInsetShadow(UPARAM(DisplayName="CastInsetShadow") bool bInCastInsetShadow);

	/** Changes the value of bCastContactShadow. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetCastContactShadow(UPARAM(DisplayName = "CastContactShadow") bool bInCastContactShadow);

	/** Changes the value of LightAttachmentsAsGroup. */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void SetLightAttachmentsAsGroup(UPARAM(DisplayName="LightAttachmentsAsGroup") bool bInLightAttachmentsAsGroup);

	/** Changes the value of ExcludeFromLightAttachmentGroup. */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void SetExcludeFromLightAttachmentGroup(UPARAM(DisplayName = "ExcludeFromLightAttachmentGroup") bool bInExcludeFromLightAttachmentGroup);

	/** Changes the value of bSingleSampleShadowFromStationaryLights. */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void SetSingleSampleShadowFromStationaryLights(bool bNewSingleSampleShadowFromStationaryLights);

	/** Changes the value of TranslucentSortPriority. */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void SetTranslucentSortPriority(int32 NewTranslucentSortPriority);

	/** Changes the value of TranslucencySortDistanceOffset. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetTranslucencySortDistanceOffset(float NewTranslucencySortDistanceOffset);

	/** Changes the value of bReceivesDecals. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetReceivesDecals(bool bNewReceivesDecals);

	/** Controls what kind of collision is enabled for this body */
	UFUNCTION(BlueprintCallable, Category="Collision")
	virtual void SetCollisionEnabled(ECollisionEnabled::Type NewType);

	/**  
	 * Set Collision Profile Name
	 * This function is called by constructors when they set ProfileName
	 * This will change current CollisionProfileName to be this, and overwrite Collision Setting
	 * 
	 * @param InCollisionProfileName : New Profile Name
	 */
	UFUNCTION(BlueprintCallable, Category="Collision")	
	virtual void SetCollisionProfileName(FName InCollisionProfileName, bool bUpdateOverlaps=true);

	/** Get the collision profile name */
	UFUNCTION(BlueprintPure, Category="Collision")
	FName GetCollisionProfileName() const;

	/**
	 *	Changes the collision channel that this object uses when it moves
	 *	@param      Channel     The new channel for this component to use
	 */
	UFUNCTION(BlueprintCallable, Category="Collision")	
	virtual void SetCollisionObjectType(ECollisionChannel Channel);

	/** Perform a line trace against a single component
	 * @param TraceStart The start of the trace in world-space
	 * @param TraceEnd The end of the trace in world-space
	 * @param bTraceComplex Whether or not to trace the complex physics representation or just the simple representation
	 * @param bShowTrace Whether or not to draw the trace in the world (for debugging)
	 * @param bPersistentShowTrace Whether or not to make the debugging draw stay in the world permanently
	 */
	UFUNCTION(BlueprintCallable, Category="Collision", meta=(DisplayName = "Line Trace Component", ScriptName = "LineTraceComponent", bTraceComplex="true", bPersistentShowTrace="false", UnsafeDuringActorConstruction="true"))	
	bool K2_LineTraceComponent(FVector TraceStart, FVector TraceEnd, bool bTraceComplex, bool bShowTrace, bool bPersistentShowTrace, FVector& HitLocation, FVector& HitNormal, FName& BoneName, FHitResult& OutHit);

	/** Perform a sphere trace against a single component
	* @param TraceStart The start of the trace in world-space
	* @param TraceEnd The end of the trace in world-space
	* @param SphereRadius Radius of the sphere to trace against the component
	* @param bTraceComplex Whether or not to trace the complex physics representation or just the simple representation
	* @param bShowTrace Whether or not to draw the trace in the world (for debugging)
	* @param bPersistentShowTrace Whether or not to make the debugging draw stay in the world permanently
	*/
	UFUNCTION(BlueprintCallable, Category = "Collision", meta = (DisplayName = "Sphere Trace Component", ScriptName = "SphereTraceComponent", bTraceComplex = "true", bPersistentShowTrace="false", UnsafeDuringActorConstruction = "true"))
	bool K2_SphereTraceComponent(FVector TraceStart, FVector TraceEnd, float SphereRadius, bool bTraceComplex, bool bShowTrace, bool bPersistentShowTrace, FVector& HitLocation, FVector& HitNormal, FName& BoneName, FHitResult& OutHit);

	/** Perform a box overlap against a single component as an AABB (No rotation)
	* @param InBoxCentre The centre of the box to overlap with the component
	* @param InBox Description of the box to use in the overlap
	* @param bTraceComplex Whether or not to trace the complex physics representation or just the simple representation
	* @param bShowTrace Whether or not to draw the trace in the world (for debugging)
	* @param bPersistentShowTrace Whether or not to make the debugging draw stay in the world permanently
	*/
	UFUNCTION(BlueprintCallable, Category = "Collision", meta = (DisplayName = "Box Overlap Component", ScriptName = "BoxOverlapComponent", bTraceComplex = "true", bPersistentShowTrace="false", UnsafeDuringActorConstruction = "true"))
	bool K2_BoxOverlapComponent(FVector InBoxCentre, const FBox InBox, bool bTraceComplex, bool bShowTrace, bool bPersistentShowTrace, FVector& HitLocation, FVector& HitNormal, FName& BoneName, FHitResult& OutHit);

	/** Perform a sphere overlap against a single component
	* @param InSphereCentre The centre of the sphere to overlap with the component
	* @param InSphereRadius The Radius of the sphere to overlap with the component
	* @param bTraceComplex Whether or not to trace the complex physics representation or just the simple representation
	* @param bShowTrace Whether or not to draw the trace in the world (for debugging)
	* @param bPersistentShowTrace Whether or not to make the debugging draw stay in the world permanently
	*/
	UFUNCTION(BlueprintCallable, Category = "Collision", meta = (DisplayName = "Sphere Overlap Component", ScriptName = "SphereOverlapComponent", bTraceComplex = "true", bPersistentShowTrace="false", UnsafeDuringActorConstruction = "true"))
	bool K2_SphereOverlapComponent(FVector InSphereCentre, float InSphereRadius, bool bTraceComplex, bool bShowTrace, bool bPersistentShowTrace, FVector& HitLocation, FVector& HitNormal, FName& BoneName, FHitResult& OutHit);

	/** Sets the bRenderCustomDepth property and marks the render state dirty. */
	UFUNCTION(BlueprintCallable, Category="Rendering")
	void SetRenderCustomDepth(bool bValue);

	/** Sets the CustomDepth stencil value (0 - 255) and marks the render state dirty. */
	UFUNCTION(BlueprintCallable, Category = "Rendering", meta=(UIMin = "0", UIMax = "255"))
	void SetCustomDepthStencilValue(int32 Value);

	/** Sets the CustomDepth stencil write mask and marks the render state dirty. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetCustomDepthStencilWriteMask(ERendererStencilMask WriteMaskBit);

	/** Sets bRenderInMainPass property and marks the render state dirty. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetRenderInMainPass(bool bValue);
	
	/** Sets bRenderInDepthPass property and marks the render state dirty. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetRenderInDepthPass(bool bValue);

	/** Sets bVisibleInSceneCaptureOnly property and marks the render state dirty. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetVisibleInSceneCaptureOnly(bool bValue);

	/** Sets bHideInSceneCapture property and marks the render state dirty. */
	UFUNCTION(BlueprintCallable, Category = "Rendering")
	void SetHiddenInSceneCapture(bool bValue);

	/**
	 * Count of all component overlap events (begin or end) ever generated for any components.
	 * Changes to this number within a scope can also be a simple way to know if any events were triggered.
	 * It can also be useful for identifying performance issues due to high numbers of events.
	 */
	static uint32 GlobalOverlapEventsCounter;

	/** The primitive's scene info. */
	FPrimitiveSceneProxy* SceneProxy;
	
	/** A fence to track when the primitive is detached from the scene in the rendering thread. */
	FRenderCommandFence DetachFence;

private:
	/** LOD parent primitive to draw instead of this one (multiple UPrim's will point to the same LODParent ) */
	UPROPERTY(NonPIEDuplicateTransient)
	TObjectPtr<class UPrimitiveComponent> LODParentPrimitive;

public:

	/** Set the LOD parent component */
	void SetLODParentPrimitive(UPrimitiveComponent* InLODParentPrimitive);

	/** Gets the LOD Parent, which is used to compute visibility when hierarchical LOD is enabled */
	UPrimitiveComponent* GetLODParentPrimitive() const;

#if WITH_EDITOR
	/** This function is used to create hierarchical LOD for the level. You can decide to opt out if you don't want. */
	virtual const bool ShouldGenerateAutoLOD(const int32 HierarchicalLevelIndex) const;
#endif
	/** Return true if the owner is selected and this component is selectable */
	virtual bool ShouldRenderSelected() const;

	/** Returns true if the owning actor is part of a level instance which is being edited. */
	bool GetLevelInstanceEditingState() const;

	/** Component is directly selected in the editor separate from its parent actor */
	bool IsComponentIndividuallySelected() const;

	/** Return True if a primitive's parameters as well as its position is static during gameplay, and can thus use static lighting. */
	bool HasStaticLighting() const;

	/** Return true if primitive can skip getting texture streaming render asset info. */
	bool CanSkipGetTextureStreamingRenderAssetInfo() const;

	virtual float GetStreamingScale() const { return 1.f; }

	/** Returns true if the component is static and has the right static mesh setup to support lightmaps. */
	virtual bool HasValidSettingsForStaticLighting(bool bOverlookInvalidComponents) const 
	{
		return HasStaticLighting();
	}

	/** Returns true if only unlit materials are used for rendering, false otherwise. */
	virtual bool UsesOnlyUnlitMaterials() const;

	/**
	 * Returns the lightmap resolution used for this primitive instance in the case of it supporting texture light/ shadow maps.
	 * 0 if not supported or no static shadowing.
	 *
	 * @param	Width	[out]	Width of light/shadow map
	 * @param	Height	[out]	Height of light/shadow map
	 * @return	bool			true if LightMap values are padded, false if not
	 */
	virtual bool GetLightMapResolution( int32& Width, int32& Height ) const;

	/**
	 *	Returns the static lightmap resolution used for this primitive.
	 *	0 if not supported or no static shadowing.
	 *
	 * @return	int32		The StaticLightmapResolution for the component
	 */
	virtual int32 GetStaticLightMapResolution() const { return 0; }

	/**
	 * Returns the light and shadow map memory for this primitive in its out variables.
	 *
	 * Shadow map memory usage is per light whereof lightmap data is independent of number of lights, assuming at least one.
	 *
	 * @param [out] LightMapMemoryUsage		Memory usage in bytes for light map (either texel or vertex) data
	 * @param [out]	ShadowMapMemoryUsage	Memory usage in bytes for shadow map (either texel or vertex) data
	 */
	virtual void GetLightAndShadowMapMemoryUsage( int32& LightMapMemoryUsage, int32& ShadowMapMemoryUsage ) const;

#if WITH_EDITOR
	/**
	 * Requests the information about the component that the static lighting system needs.
	 * @param OutPrimitiveInfo - Upon return, contains the component's static lighting information.
	 * @param InRelevantLights - The lights relevant to the primitive.
	 * @param InOptions - The options for the static lighting build.
	 */
	virtual void GetStaticLightingInfo(struct FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<class ULightComponent*>& InRelevantLights,const class FLightingBuildOptions& Options) {}

	/** Add the used GUIDs from UMapBuildDataRegistry::MeshBuildData. Used to preserve hidden level data in lighting scenario. */
	virtual void AddMapBuildDataGUIDs(TSet<FGuid>& InGUIDs) const {}

	/**
	 *	Remaps the texture streaming built data that was built for the actor back to the level.
	 *	
	 */
	virtual bool RemapActorTextureStreamingBuiltDataToLevel(const class UActorTextureStreamingBuildDataComponent* InActorTextureBuildData) { return false; }

	/** Computes a hash of component's texture streaming built data. */
	virtual uint32 ComputeHashTextureStreamingBuiltData() const { return 0; }
#endif // WITH_EDITOR

	/**
	 *	Requests whether the component will use texture, vertex or no lightmaps.
	 *
	 *	@return	ELightMapInteractionType		The type of lightmap interaction the component will use.
	 */
	virtual ELightMapInteractionType GetStaticLightingType() const	{ return LMIT_None;	}

	/**
	 * Enumerates the streaming textures/meshes used by the primitive.
	 * @param LevelContext - Level scope context used to process texture streaming build data.
	 * @param OutStreamingRenderAssets - Upon return, contains a list of the streaming textures/meshes used by the primitive.
	 */
	virtual void GetStreamingRenderAssetInfo(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingRenderAssetPrimitiveInfo>& OutStreamingRenderAssets) const;

	/**
	 * Call GetStreamingRenderAssetInfo and remove the elements with a NULL texture
	 * @param OutStreamingRenderAssets - Upon return, contains a list of the non-null streaming textures or meshes used by the primitive.
	 */
	void GetStreamingRenderAssetInfoWithNULLRemoval(FStreamingTextureLevelContext& LevelContext, TArray<FStreamingRenderAssetPrimitiveInfo>& OutStreamingRenderAssets) const;

	/**
	 *	Update the streaming data of this component.
	 *
	 *	@param	BuildType		[in]		The type of build. Affects what the build is allowed to do.
	 *	@param	QualityLevel	[in]		The quality level being used in the texture streaming build.
	 *	@param	FeatureLevel	[in]		The feature level being used in the texture streaming build.
	 *	@param	DependentResources [out]	The resource the build depends on.
	 *	@return								Returns false if some data needs rebuild but couldn't be rebuilt (because of the build type).
	 */
	bool BuildTextureStreamingData(ETextureStreamingBuildType BuildType, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TSet<FGuid>& DependentResources);

	/**
	 *	Component type implementation of updating the streaming data of this component.
	 *
	 *	@param	BuildType		[in]		The type of build. Affects what the build is allowed to do.
	 *	@param	QualityLevel	[in]		The quality level being used in the texture streaming build.
	 *	@param	FeatureLevel	[in]		The feature level being used in the texture streaming build.
	 *	@param	DependentResources [out]	The resource the build depends on.
	 *	@return								Returns false if some data needs rebuild but couldn't be rebuilt (because of the build type).
	 */
	virtual bool BuildTextureStreamingDataImpl(ETextureStreamingBuildType BuildType, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TSet<FGuid>& DependentResources, bool& bOutSupportsBuildTextureStreamingData);

	/**
	 * Determines the DPG the primitive's primary elements are drawn in.
	 * Even if the primitive's elements are drawn in multiple DPGs, a primary DPG is needed for occlusion culling and shadow projection.
	 * @return The DPG the primitive's primary elements will be drawn in.
	 */
	virtual ESceneDepthPriorityGroup GetStaticDepthPriorityGroup() const { return DepthPriorityGroup; }

	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials(TArray<UMaterialInterface*>& OutMaterials, bool bGetDebugMaterials = false) const {}

	/**
	 * Returns the material textures used to render this primitive for the given platform.
	 * Internally calls GetUsedMaterials() and GetUsedTextures() for each material.
	 *
	 * @param OutTextures	[out] The list of used textures.
	 */
	virtual void GetUsedTextures(TArray<UTexture*>& OutTextures, EMaterialQualityLevel::Type QualityLevel);

	/** Return the BodySetup to use for this PrimitiveComponent (single body case) */
	virtual class UBodySetup* GetBodySetup() { return NULL; }

	/** Move this component to match the physics rigid body pose. Note, a warning will be generated if you call this function on a component that is attached to something */
	void SyncComponentToRBPhysics();
	
	/** 
	 * Returns the matrix that should be used to render this component. 
	 * Allows component class to perform graphical distortion to the component not supported by an FTransform 
	 * NOTE: When overriding this method to alter the transform used for rendering it is typically neccessary to also implement USceneComponent::UpdateBounds.
	 *       Otherwise the Local bounds will not match the world space bounds, causing incorrect culling.
	 */
	virtual FMatrix GetRenderMatrix() const;

	/** Return number of material elements in this primitive */
	UFUNCTION(BlueprintPure, Category="Rendering|Material")
	virtual int32 GetNumMaterials() const;
	
	/**
	 * Returns BodyInstance of the component.
	*
	* @param BoneName				Used to get body associated with specific bone. NAME_None automatically gets the root most body
	* @param bGetWelded				If the component has been welded to another component and bGetWelded is true we return the single welded BodyInstance that is used in the simulation
	* @param Index					Index used in Components with multiple body instances
	*
	* @return		Returns the BodyInstance based on various states (does component have multiple bodies? Is the body welded to another body?)
	*/
	virtual FBodyInstance* GetBodyInstance(FName BoneName = NAME_None, bool bGetWelded = true, int32 Index = INDEX_NONE) const;

	/**
	 * Returns BodyInstanceAsyncPhysicsTickHandle of the component. For use in the Async Physics Tick event
	*
	* @param BoneName				Used to get body associated with specific bone. NAME_None automatically gets the root most body
	* @param bGetWelded				If the component has been welded to another component and bGetWelded is true we return the single welded BodyInstance that is used in the simulation
	* @param Index					Index used in Components with multiple body instances
	*
	* @return		Returns the BodyInstanceAsyncPhysicsTickHandle based on various states (does component have multiple bodies? Is the body welded to another body?)
	*/
	UFUNCTION(BlueprintPure, Category = "Physics")
		FBodyInstanceAsyncPhysicsTickHandle GetBodyInstanceAsyncPhysicsTickHandle(FName BoneName = NAME_None, bool bGetWelded = true, int32 Index = -1) const;

	/** 
	 * Returns The square of the distance to closest Body Instance surface. 
	 *
	 * @param Point				World 3D vector
	 * @param OutSquaredDistance The squared distance to closest Body Instance surface. 0 if inside of the body
	 * @param OutPointOnBody	Point on the surface of collision closest to Point
	 * 
	 * @return		true if a distance to the body was found and OutDistanceSquared has been populated
	 */
	virtual bool GetSquaredDistanceToCollision(const FVector& Point, float& OutSquaredDistance, FVector& OutClosestPointOnCollision) const;

	/** 
	 * Returns Distance to closest Body Instance surface. 
	*
	* @param Point				World 3D vector
	* @param OutPointOnBody	Point on the surface of collision closest to Point
	* 
	* @return		Success if returns > 0.f, if returns 0.f, point is inside the geometry
	*				If returns < 0.f, this primitive does not have collsion or if geometry is not supported
	*/	
	float GetDistanceToCollision(const FVector& Point, FVector& ClosestPointOnCollision) const 
	{
		float DistanceSqr = -1.f;
		return (GetSquaredDistanceToCollision(Point, DistanceSqr, ClosestPointOnCollision) ? FMath::Sqrt(DistanceSqr) : -1.f);
	}

	/**
	* Returns the distance and closest point to the collision surface.
	* Component must have simple collision to be queried for closest point.
	*
	* @param Point				World 3D vector
	* @param OutPointOnBody		Point on the surface of collision closest to Point
	* @param BoneName			If a SkeletalMeshComponent, name of body to set center of mass of. 'None' indicates root body.
	*
	* @return		Success if returns > 0.f, if returns 0.f, it is either not convex or inside of the point
	*				If returns < 0.f, this primitive does not have collsion
	*/
	UFUNCTION(BlueprintCallable, Category = "Collision", meta=(UnsafeDuringActorConstruction="true"))
	float GetClosestPointOnCollision(const FVector& Point, FVector& OutPointOnBody, FName BoneName = NAME_None) const;

	/**
	 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
	 * @return The proxy object.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy()
	{
		return NULL;
	}

	/**
	 * Determines whether the proxy for this primitive type needs to be recreated whenever the primitive moves.
	 * @return true to recreate the proxy when UpdateTransform is called.
	 */
	virtual bool ShouldRecreateProxyOnUpdateTransform() const
	{
		return false;
	}

	/** 
	 * This isn't bound extent, but for shape component to utilize extent is 0. 
	 * For normal primitive, this is 0, for ShapeComponent, this will have valid information
	 */
	virtual bool IsZeroExtent() const
	{
		return false;
	}

	/** Event called when a component is 'damaged', allowing for component class specific behaviour */
	virtual void ReceiveComponentDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser);

	/**
	*   Welds this component to another scene component, optionally at a named socket. Component is automatically attached if not already
	*	Welding allows the child physics object to become physically connected to its parent. This is useful for creating compound rigid bodies with correct mass distribution.
	*   @param InParent the component to be physically attached to
	*   @param InSocketName optional socket to attach component to
	*/
	virtual void WeldTo(class USceneComponent* InParent, FName InSocketName = NAME_None);

	/**
	*	Does the actual work for welding.
	*	@return true if did a true weld of shapes, meaning body initialization is not needed
	*/
	virtual bool WeldToImplementation(USceneComponent * InParent, FName ParentSocketName = NAME_None, bool bWeldSimulatedChild = true);

	/**
	*   UnWelds this component from its parent component. Attachment is maintained (DetachFromParent automatically unwelds)
	*/
	virtual void UnWeldFromParent();

	/**
	*   Unwelds the children of this component. Attachment is maintained
	*/
	virtual void UnWeldChildren();

	/**
	*	Adds the bodies that are currently welded to the OutWeldedBodies array 
	*/
	virtual void GetWeldedBodies(TArray<FBodyInstance*> & OutWeldedBodies, TArray<FName> & OutLabels, bool bIncludingAutoWeld = false);

	/** Whether the component has been welded to another simulating component */
	bool IsWelded() const;
	
	/**
	 * Called to get the Component To World Transform from the Root BodyInstance
	 * This needs to be virtual since SkeletalMeshComponent Root has to undo its own transform
	 * Without this, the root LocalToAtom is overridden by physics simulation, causing kinematic velocity to 
	 * accelerate simulation
	 *
	 * @param : UseBI - root body instance
	 * @return : New GetComponentTransform() to use
	 */
	virtual FTransform GetComponentTransformFromBodyInstance(FBodyInstance* UseBI);	

	/**
	 * Would this primitive be shown with these rendering flags.
	 * 
	 * @Note: Currently this only implemented properly for the editor selectable primitives.
	 */
	virtual bool IsShown(const FEngineShowFlags& ShowFlags) const;

	/** Returns false if this primitive should never output velocity based on its WPO state. */
	virtual bool SupportsWorldPositionOffsetVelocity() const { return true; }

#if WITH_EDITOR
	/**
	 * Determines whether the supplied bounding box intersects with the component.
	 * Used by the editor in orthographic viewports.
	 *
	 * @param	InSelBBox						Bounding box to test against
	 * @param	ShowFlags						Engine ShowFlags for the viewport
	 * @param	bConsiderOnlyBSP				If only BSP geometry should be tested
	 * @param	bMustEncompassEntireComponent	Whether the component bounding box must lay wholly within the supplied bounding box
	 *
	 * @return	true if the supplied bounding box is determined to intersect the component (partially or wholly)
	 */
	UE_DEPRECATED(5.1, "This function is deprecated. Use the function IsShown and the overload that doesn't take an EngineShowFlags instead.")
	virtual bool ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const;

	/**
	 * Determines whether the supplied bounding box intersects with the component.
	 * Used by the editor in orthographic viewports.
	 *
	 * @param	InSelBBox						Bounding box to test against
	 * @param	bConsiderOnlyBSP				If only BSP geometry should be tested
	 * @param	bMustEncompassEntireComponent	Whether the component bounding box must lay wholly within the supplied bounding box
	 *
	 * @return	true if the supplied bounding box is determined to intersect the component (partially or wholly)
	 */
	virtual bool ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const;

	/**
	 * Determines whether the supplied frustum intersects with the component.
	 * Used by the editor in perspective viewports.
	 *
	 * @param	InFrustum						Frustum to test against
	 * @param	ShowFlags						Engine ShowFlags for the viewport
	 * @param	bConsiderOnlyBSP				If only BSP geometry should be tested
	 * @param	bMustEncompassEntireComponent	Whether the component bounding box must lay wholly within the supplied bounding box
	 *
	 * @return	true if the supplied bounding box is determined to intersect the component (partially or wholly)
	 */
	UE_DEPRECATED(5.1, "This function is deprecated. Use the function IsShown and the overload that doesn't take an EngineShowFlags instead.")
	virtual bool ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const;

	/**
	 * Determines whether the supplied frustum intersects with the component.
	 * Used by the editor in perspective viewports.
	 *
	 * @param	InFrustum						Frustum to test against
	 * @param	bConsiderOnlyBSP				If only BSP geometry should be tested
	 * @param	bMustEncompassEntireComponent	Whether the component bounding box must lay wholly within the supplied bounding box
	 *
	 * @return	true if the supplied bounding box is determined to intersect the component (partially or wholly)
	 */
	virtual bool ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const;
#endif

protected:

	/** Give the static mesh component recreate render state context access to Create/DestroyRenderState_Concurrent(). */
	friend class FStaticMeshComponentRecreateRenderStateContext;

	/** Whether the component type supports static lighting. */
	virtual bool SupportsStaticLighting() const 
	{
		return false;
	}

public:
#if UE_ENABLE_DEBUG_DRAWING
	/** Updates the renderer with the center of mass data */
	virtual void SendRenderDebugPhysics(FPrimitiveSceneProxy* OverrideSceneProxy = nullptr);
#endif

	//~ Begin UActorComponent Interface
	virtual void CreateRenderState_Concurrent(FRegisterComponentContext* Context) override;
	virtual void SendRenderTransform_Concurrent() override;
	virtual void OnRegister()  override;
	virtual void OnUnregister()  override;
	virtual void DestroyRenderState_Concurrent() override;
	virtual void OnCreatePhysicsState() override;
	virtual void OnDestroyPhysicsState() override;
	virtual void OnActorEnableCollisionChanged() override;
	virtual void InvalidateLightingCacheDetailed(bool bInvalidateBuildEnqueuedLighting, bool bTranslationOnly) override;
	virtual bool IsEditorOnly() const override;
	virtual bool ShouldCreatePhysicsState() const override;
	virtual bool HasValidPhysicsState() const override;
	virtual TStructOnScope<FActorComponentInstanceData> GetComponentInstanceData() const override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual void GetActorDescProperties(FPropertyPairsMap& PropertyPairsMap) const;
#endif // WITH_EDITOR	
	//~ End UActorComponent Interface

protected:
	/** Internal function that updates physics objects to match the component collision settings. */
	virtual void UpdatePhysicsToRBChannels();

	/** Called to send a transform update for this component to the physics engine */
	void SendPhysicsTransform(ETeleportType Teleport);

	/** Ensure physics state created **/
	void EnsurePhysicsStateCreated();

	/**  Go through attached primitive components and call MarkRenderStateDirty */
	void MarkChildPrimitiveComponentRenderStateDirty();
public:

	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	virtual bool NeedsLoadForClient() const override;
	virtual bool NeedsLoadForServer() const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void UpdateCollisionProfile();
	virtual void PostEditImport() override;
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#endif // WITH_EDITOR
	//~ End UObject Interface.

	//~ Begin USceneComponent Interface

	/** Returns the form of collision for this component */
	UFUNCTION(BlueprintPure, Category="Collision")
	virtual ECollisionEnabled::Type GetCollisionEnabled() const override;

	/** Utility to see if there is any form of collision (query or physics) enabled on this component. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Is Collision Enabled", ScriptName="IsCollisionEnabled"), Category="Collision")
	bool K2_IsCollisionEnabled() const;

	/** Utility to see if there is any query collision enabled on this component. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Is Query Collision Enabled", ScriptName="IsQueryCollisionEnabled"), Category="Collision")
	bool K2_IsQueryCollisionEnabled() const;

	/** Utility to see if there is any physics collision enabled on this component. */
	UFUNCTION(BlueprintPure, meta=(DisplayName="Is Physics Collision Enabled", ScriptName="IsPhysicsCollisionEnabled"), Category="Collision")
	bool K2_IsPhysicsCollisionEnabled() const;

	/** Gets the response type given a specific channel */
	UFUNCTION(BlueprintPure, Category="Collision")
	virtual ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const override;

	/** Gets the collision object type */
	UFUNCTION(BlueprintPure, Category="Collision")
	virtual ECollisionChannel GetCollisionObjectType() const override;
	
	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;
	virtual void OnAttachmentChanged() override;
	virtual bool IsSimulatingPhysics(FName BoneName = NAME_None) const override;
	virtual bool MoveComponentImpl(const FVector& Delta, const FQuat& NewRotation, bool bSweep, FHitResult* OutHit = NULL, EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags, ETeleportType Teleport = ETeleportType::None) override;
	virtual bool IsWorldGeometry() const override;
	virtual const FCollisionResponseContainer& GetCollisionResponseToChannels() const override;
	virtual FVector GetComponentVelocity() const override;
#if WITH_EDITOR
	virtual void UpdateBounds() override;
	virtual const int32 GetNumUncachedStaticLightingInteractions() const override;
#endif
	//~ End USceneComponentInterface

	void UpdateOcclusionBoundsSlack(float NewSlack);

	/**
	 * Dispatch notifications for the given HitResult.
	 *
	 * @param Owner: AActor that owns this component
	 * @param BlockingHit: FHitResult that generated the blocking hit.
	 */
	void DispatchBlockingHit(AActor& OutOwner, FHitResult const& BlockingHit);

	/**
	 * Dispatch notification for wake events and propagate to any welded bodies
	 */

	void DispatchWakeEvents(ESleepEvent WakeEvent, FName BoneName);

	/**
	 * Set collision params on OutParams (such as CollisionResponse) to match the settings on this PrimitiveComponent.
	 */
	virtual void InitSweepCollisionParams(FCollisionQueryParams &OutParams, FCollisionResponseParams& OutResponseParam) const;

	/**
	 * Return a CollisionShape that most closely matches this primitive.
	 */
	virtual struct FCollisionShape GetCollisionShape(float Inflation = 0.0f) const;

	/**
	 * Returns true if the given transforms result in the same bounds, due to rotational symmetry.
	 * For example, this is true for a sphere with uniform scale undergoing any rotation.
	 * This is NOT intended to detect every case where this is true, only the common cases to aid optimizations.
	 */
	virtual bool AreSymmetricRotations(const FQuat& A, const FQuat& B, const FVector& Scale3D) const { return A.Equals(B); }

	/**
	 * Pushes new selection state to the render thread primitive proxy
	 */
	virtual void PushSelectionToProxy();

	/**
	 * Pushes new LevelInstance editing state to the render thread primitive proxy.
	 */
	void PushLevelInstanceEditingStateToProxy(bool bInEditingState);

	/**
	 * Pushes new hover state to the render thread primitive proxy
	 * @param bInHovered - true if the proxy should display as if hovered
	 */
	void PushHoveredToProxy(const bool bInHovered);

	/** Sends editor visibility updates to the render thread */
	void PushEditorVisibilityToProxy( uint64 InVisibility );

	/** Gets the emissive boost for the primitive component. */
	virtual float GetEmissiveBoost(int32 ElementIndex) const		{ return 1.0f; };

	/** Gets the diffuse boost for the primitive component. */
	virtual float GetDiffuseBoost(int32 ElementIndex) const		{ return 1.0f; };
	
	/** Disable dynamic shadow casting if the primitive only casts indirect shadows, since dynamic shadows are always shadowing direct lighting */
	virtual bool GetShadowIndirectOnly() const { return false; }
	
	/** Returns whether this component is still being compiled or dependent on other objects being compiled. */
	virtual bool IsCompiling() const { return false; }

#if WITH_EDITOR
	/** Returns mask that represents in which views this primitive is hidden */
	virtual uint64 GetHiddenEditorViews() const;

	/** Sets whether this component is being moved by the editor so the renderer can render velocities for it, even when Static. */
	void SetIsBeingMovedByEditor(bool bNewIsBeingMoved);
#endif// WITH_EDITOR

	/**
	 *	Set the angular velocity of all bodies in this component.
	 *
	 *	@param NewAngVel		New angular velocity to apply to physics, in degrees per second.
	 *	@param bAddToCurrent	If true, NewAngVel is added to the existing angular velocity of all bodies.
	 */
	UFUNCTION(BlueprintCallable, Category = "Physics", meta = (UnsafeDuringActorConstruction = "true"))
	void SetAllPhysicsAngularVelocityInDegrees(const FVector& NewAngVel, bool bAddToCurrent = false)
	{
		SetAllPhysicsAngularVelocityInRadians(FMath::DegreesToRadians(NewAngVel), bAddToCurrent);
	}

	/**
	 *	Set the angular velocity of all bodies in this component.
	 *
	 *	@param NewAngVel		New angular velocity to apply to physics, in radians per second.
	 *	@param bAddToCurrent	If true, NewAngVel is added to the existing angular velocity of all bodies.
	 */
	UFUNCTION(BlueprintCallable, Category = "Physics", meta = (UnsafeDuringActorConstruction = "true"))
	virtual void SetAllPhysicsAngularVelocityInRadians(const FVector& NewAngVel, bool bAddToCurrent = false);

	/**
	 *	Set the position of all bodies in this component.
	 *	If a SkeletalMeshComponent, the root body will be placed at the desired position, and the same delta is applied to all other bodies.
	 *
	 *	@param	NewPos		New position for the body
	 */
	virtual void SetAllPhysicsPosition(FVector NewPos);
	
	/**
	 *	Set the rotation of all bodies in this component.
	 *	If a SkeletalMeshComponent, the root body will be changed to the desired orientation, and the same delta is applied to all other bodies.
	 *
	 *	@param NewRot	New orienatation for the body
	 */
	virtual void SetAllPhysicsRotation(FRotator NewRot);

	/**
	 *	Set the rotation of all bodies in this component.
	 *	If a SkeletalMeshComponent, the root body will be changed to the desired orientation, and the same delta is applied to all other bodies.
	 *
	 *	@param NewRot	New orienatation for the body
	 */
	virtual void SetAllPhysicsRotation(const FQuat& NewRot);
	
	/**
	 *	Ensure simulation is running for all bodies in this component.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual void WakeAllRigidBodies();
	
	/** Enables/disables whether this component is affected by gravity. This applies only to components with bSimulatePhysics set to true. */
	UFUNCTION(BlueprintCallable, Category="Physics")
	virtual void SetEnableGravity(bool bGravityEnabled);

	/** Returns whether this component is affected by gravity. Returns always false if the component is not simulated. */
	UFUNCTION(BlueprintPure, Category="Physics")
	virtual bool IsGravityEnabled() const;

	/** Sets the linear damping of this component. */
	UFUNCTION(BlueprintCallable, Category="Physics")
	virtual void SetLinearDamping(float InDamping);

	/** Returns the linear damping of this component. */
	UFUNCTION(BlueprintPure, Category="Physics")
	virtual float GetLinearDamping() const;

	/** Sets the angular damping of this component. */
	UFUNCTION(BlueprintCallable, Category="Physics")
	virtual void SetAngularDamping(float InDamping);
	
	/** Returns the angular damping of this component. */
	UFUNCTION(BlueprintPure, Category="Physics")
	virtual float GetAngularDamping() const;

	/** Change the mass scale used to calculate the mass of a single physics body */
	UFUNCTION(BlueprintCallable, Category="Physics")
	virtual void SetMassScale(FName BoneName = NAME_None, float InMassScale = 1.f);

	/** Returns the mass scale used to calculate the mass of a single physics body */
	UFUNCTION(BlueprintPure, Category = "Physics")
	virtual float GetMassScale(FName BoneName = NAME_None) const;

	/** Change the mass scale used fo all bodies in this component */
	UFUNCTION(BlueprintCallable, Category="Physics")
	virtual void SetAllMassScale(float InMassScale = 1.f);

	/**
	*	Override the mass (in Kg) of a single physics body.
	*	Note that in the case where multiple bodies are attached together, the override mass will be set for the entire group.
	*	Set the Override Mass to false if you want to reset the body's mass to the auto-calculated physx mass.
	*/
	UFUNCTION(BlueprintCallable, Category = "Physics")
	virtual void SetMassOverrideInKg(FName BoneName = NAME_None, float MassInKg = 1.f, bool bOverrideMass = true);

	/** Returns the mass of this component in kg. */
	UFUNCTION(BlueprintPure, Category="Physics", meta=(UnsafeDuringActorConstruction="true"))
	virtual float GetMass() const;

	/** Returns the inertia tensor of this component in kg cm^2. The inertia tensor is in local component space.*/
	UFUNCTION(BlueprintPure, Category = "Physics", meta =(Keywords = "physics moment of inertia tensor MOI", UnsafeDuringActorConstruction="true"))
	virtual FVector GetInertiaTensor(FName BoneName = NAME_None) const;

	/** Scales the given vector by the world space moment of inertia. Useful for computing the torque needed to rotate an object.*/
	UFUNCTION(BlueprintPure, Category = "Physics", meta = (Keywords = "physics moment of inertia tensor MOI", UnsafeDuringActorConstruction="true"))
	virtual FVector ScaleByMomentOfInertia(FVector InputVector, FName BoneName = NAME_None) const;

	/** Returns the calculated mass in kg. This is not 100% exactly the mass physx will calculate, but it is very close ( difference < 0.1kg ). */
	virtual float CalculateMass(FName BoneName = NAME_None);

	/** Set whether this component should use Continuous Collision Detection */
	UFUNCTION(BlueprintCallable, Category = "Physics")
	virtual void SetUseCCD(bool InUseCCD, FName BoneName = NAME_None);

	/** Set whether all bodies in this component should use Continuous Collision Detection */
	UFUNCTION(BlueprintCallable, Category = "Physics")
	virtual void SetAllUseCCD(bool InUseCCD);

	/**
	 *	Force all bodies in this component to sleep.
	 */
	virtual void PutAllRigidBodiesToSleep();
	
	/**
	 *	Returns if a single body is currently awake and simulating.
	 *	@param	BoneName	If a SkeletalMeshComponent, name of body to return wakeful state from. 'None' indicates root body.
	 */
	bool RigidBodyIsAwake(FName BoneName = NAME_None) const;

	/**
	 *	Returns if any body in this component is currently awake and simulating.
	 */
	UFUNCTION(BlueprintPure, Category = "Physics", meta = (Keywords = "physics asleep sleeping awake simulating", UnsafeDuringActorConstruction="true"))
	virtual bool IsAnyRigidBodyAwake();
	
	/**
	 *	Changes a member of the ResponseToChannels container for this PrimitiveComponent.
	 *
	 * @param       Channel      The channel to change the response of
	 * @param       NewResponse  What the new response should be to the supplied Channel
	 */
	UFUNCTION(BlueprintCallable, Category="Collision")
	virtual void SetCollisionResponseToChannel(ECollisionChannel Channel, ECollisionResponse NewResponse);
	
	/**
	 *	Changes all ResponseToChannels container for this PrimitiveComponent. to be NewResponse
	 *
	 * @param       NewResponse  What the new response should be to the supplied Channel
	 */
	UFUNCTION(BlueprintCallable, Category="Collision")
	virtual void SetCollisionResponseToAllChannels(ECollisionResponse NewResponse);
	
	/**
	 *	Changes the whole ResponseToChannels container for this PrimitiveComponent.
	 *
	 * @param       NewResponses  New set of responses for this component
	 */
	virtual void SetCollisionResponseToChannels(const FCollisionResponseContainer& NewReponses);
	
protected:
	/** Called when the BodyInstance ResponseToChannels, CollisionEnabled or bNotifyRigidBodyCollision changes, in case subclasses want to use that information. */
	virtual void OnComponentCollisionSettingsChanged(bool bUpdateOverlaps=true);

	/** Called when bGenerateOverlapEvents changes, in case subclasses want to use that information. */
	virtual void OnGenerateOverlapEventsChanged();

	/** Ends all current component overlaps. Generally used when destroying this component or when it can no longer generate overlaps. */
	void ClearComponentOverlaps(bool bDoNotifies, bool bSkipNotifySelf);

private:
	/** Check if mobility is set to non-static. If BodyInstanceRequiresSimulation is non-null we check that it is simulated. Triggers a PIE warning if conditions fails */
	void WarnInvalidPhysicsOperations_Internal(const FText& ActionText, const FBodyInstance* BodyInstanceRequiresSimulation, FName BoneName) const;

public:
	/**
	 * Applies RigidBodyState only if it needs to be updated
	 * NeedsUpdate flag will be removed from UpdatedState after all velocity corrections are finished
	 */
	void SetRigidBodyReplicatedTarget(FRigidBodyState& UpdatedState, const FName BoneName = NAME_None, int32 ServerFrame = 0, int32 ServerHandle = 0);

	/** 
	 *	Get the state of the rigid body responsible for this Actor's physics, and fill in the supplied FRigidBodyState struct based on it.
	 *
	 *	@return	true if we successfully found a physics-engine body and update the state structure from it.
	 */
	bool GetRigidBodyState(FRigidBodyState& OutState, FName BoneName = NAME_None);

	/** 
	 *	Changes the current PhysMaterialOverride for this component. 
	 *	Note that if physics is already running on this component, this will _not_ alter its mass/inertia etc,  
	 *	it will only change its surface properties like friction.
	 */
	UFUNCTION(BlueprintCallable, Category="Physics", meta=(DisplayName="Set PhysicalMaterial Override"))
	virtual void SetPhysMaterialOverride(class UPhysicalMaterial* NewPhysMaterial);

	/** 
	 *  Looking at various values of the component, determines if this
	 *  component should be added to the scene
	 * @return true if the component is visible and should be added to the scene, false otherwise
	 */
	bool ShouldComponentAddToScene() const;
	
	/**
	 * Changes the value of CullDistance.
	 * @param NewCullDistance - The value to assign to CullDistance.
	 */
	UFUNCTION(BlueprintCallable, Category="LOD", meta=(DisplayName="Set Max Draw Distance"))
	void SetCullDistance(float NewCullDistance);
	
	/**
	 * Utility to cache the max draw distance based on cull distance volumes or the desired max draw distance
	 */
	void SetCachedMaxDrawDistance(const float NewCachedMaxDrawDistance);

	/**
	 * Changes the value of DepthPriorityGroup.
	 * @param NewDepthPriorityGroup - The value to assign to DepthPriorityGroup.
	 */
	void SetDepthPriorityGroup(ESceneDepthPriorityGroup NewDepthPriorityGroup);
	
	/**
	 * Changes the value of bUseViewOwnerDepthPriorityGroup and ViewOwnerDepthPriorityGroup.
	 * @param bNewUseViewOwnerDepthPriorityGroup - The value to assign to bUseViewOwnerDepthPriorityGroup.
	 * @param NewViewOwnerDepthPriorityGroup - The value to assign to ViewOwnerDepthPriorityGroup.
	 */
	void SetViewOwnerDepthPriorityGroup(
		bool bNewUseViewOwnerDepthPriorityGroup,
		ESceneDepthPriorityGroup NewViewOwnerDepthPriorityGroup
		);
	
	/** 
	 *  Trace a ray against just this component.
	 *  @param  OutHit          Information about hit against this component, if true is returned
	 *  @param  Start           Start location of the ray
	 *  @param  End             End location of the ray
	 *  @param  Params          Additional parameters used for the trace
	 *  @return true if a hit is found
	 */
	virtual bool LineTraceComponent( FHitResult& OutHit, const FVector Start, const FVector End, const FCollisionQueryParams& Params );
	
	/** 
	 *  Trace a shape against just this component.
	 *  @param  OutHit          	Information about hit against this component, if true is returned
	 *  @param  Start           	Start location of the box
	 *  @param  End             	End location of the box
	 *  @param  ShapeWorldRotation  The rotation applied to the collision shape in world space.
	 *  @param  CollisionShape  	Collision Shape
	 *	@param	bTraceComplex	Whether or not to trace complex
	 *  @return true if a hit is found
	 */
	virtual bool SweepComponent(FHitResult& OutHit, const FVector Start, const FVector End, const FQuat& ShapeWorldRotation, const FCollisionShape &CollisionShape, bool bTraceComplex=false);

	/** 
	 *  Test the collision of the supplied component at the supplied location/rotation, and determine if it overlaps this component.
	 *  @note This overload taking rotation as a FQuat is slightly faster than the version using FRotator.
	 *  @note This simply calls the virtual ComponentOverlapComponentImpl() which can be overridden to implement custom behavior.
	 *  @param  PrimComp        Component to use geometry from to test against this component. Transform of this component is ignored.
	 *  @param  Pos             Location to place PrimComp geometry at 
	 *  @param  Rot             Rotation to place PrimComp geometry at 
	 *  @param	Params			Parameter for trace. TraceTag is only used.
	 *  @return true if PrimComp overlaps this component at the specified location/rotation
	 */
	bool ComponentOverlapComponent(class UPrimitiveComponent* PrimComp, const FVector Pos, const FQuat& Rot, const FCollisionQueryParams& Params);
	bool ComponentOverlapComponent(class UPrimitiveComponent* PrimComp, const FVector Pos, const FRotator Rot, const FCollisionQueryParams& Params);

protected:
	/** Override this method for custom behavior for ComponentOverlapComponent() */
	virtual bool ComponentOverlapComponentImpl(class UPrimitiveComponent* PrimComp, const FVector Pos, const FQuat& Rot, const FCollisionQueryParams& Params);

public:	
	/** 
	 *  Test the collision of the supplied shape at the supplied location, and determine if it overlaps this component.
	 *
	 *  @param  Pos             Location to place PrimComp geometry at 
	 *	@param	Rot				Rotation of PrimComp geometry
	 *  @param  CollisionShape 	Shape of collision of PrimComp geometry
	 *  @return true if PrimComp overlaps this component at the specified location/rotation
	 */

    //UE_DEPRECATED(5.0, "Use the const version of OverlapComponent. This deprecation cannot be uncommented as it would produce false positives." )
	virtual bool OverlapComponent(const FVector& Pos, const FQuat& Rot, const FCollisionShape& CollisionShape) final 
	{
		return const_cast<const UPrimitiveComponent*>(this)->OverlapComponent(Pos, Rot, CollisionShape);
	}
	virtual bool OverlapComponent(const FVector& Pos, const FQuat& Rot, const FCollisionShape& CollisionShape) const;

	/**
	 * Computes the minimum translation direction (MTD) when an overlap exists between the component and the given shape.
	 * @param OutMTD			Outputs the MTD to move CollisionShape out of penetration
	 * @param CollisionShape	Shape information for the geometry testing against
	 * @param Pos				Location of collision shape
	 * @param Rot				Rotation of collision shape
	 * @return true if the computation succeeded - assumes that there is an overlap at the specified position/rotation
	 */
	virtual bool ComputePenetration(FMTDResult & OutMTD, const FCollisionShape& CollisionShape, const FVector& Pos, const FQuat& Rot);

	/**
	 * Return true if the given Pawn can step up onto this component.
	 * This controls whether they can try to step up on it when they bump in to it, not whether they can walk on it after landing on it.
	 * @param Pawn the Pawn that wants to step onto this component.
	 * @see CanCharacterStepUpOn
	 */
	UFUNCTION(BlueprintCallable, Category=Collision)
	virtual bool CanCharacterStepUp(class APawn* Pawn) const;

	//~ Begin INavRelevantInterface Interface
	virtual void GetNavigationData(FNavigationRelevantData& OutData) const override;
	virtual FBox GetNavigationBounds() const override;
	virtual bool IsNavigationRelevant() const override;
	//~ End INavRelevantInterface Interface

	/** If true then DoCustomNavigableGeometryExport will be called to collect navigable geometry of this component. */
	FORCEINLINE EHasCustomNavigableGeometry::Type HasCustomNavigableGeometry() const { return bHasCustomNavigableGeometry; }

	/** Set value of HasCustomNavigableGeometry */
	void SetCustomNavigableGeometry(const EHasCustomNavigableGeometry::Type InType);

	/** Collects custom navigable geometry of component.
	*	@return true if regular navigable geometry exporting should be run as well */
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const { return true; }

	static void DispatchMouseOverEvents(UPrimitiveComponent* CurrentComponent, UPrimitiveComponent* NewComponent);
	static void DispatchTouchOverEvents(ETouchIndex::Type FingerIndex, UPrimitiveComponent* CurrentComponent, UPrimitiveComponent* NewComponent);
	void DispatchOnClicked(FKey ButtonClicked = EKeys::LeftMouseButton);
	void DispatchOnReleased(FKey ButtonReleased = EKeys::LeftMouseButton);
	void DispatchOnInputTouchBegin(const ETouchIndex::Type Key);
	void DispatchOnInputTouchEnd(const ETouchIndex::Type Key);
};

/** 
 *  Component instance cached data base class for primitive components. 
 *  Stores a list of instance components attached to the 
 */
USTRUCT()
struct ENGINE_API FPrimitiveComponentInstanceData : public FSceneComponentInstanceData
{
	GENERATED_BODY()
public:
	FPrimitiveComponentInstanceData() = default;
	FPrimitiveComponentInstanceData(const UPrimitiveComponent* SourceComponent);
	virtual ~FPrimitiveComponentInstanceData() = default;

	virtual bool ContainsData() const override;

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override;
	virtual void FindAndReplaceInstances(const TMap<UObject*, UObject*>& OldToNewInstanceMap) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	const FTransform& GetComponentTransform() const { return ComponentTransform; }

private:
	UPROPERTY()
	FTransform ComponentTransform;

	UPROPERTY()
	int32 VisibilityId = INDEX_NONE;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> LODParent = nullptr;
};


//////////////////////////////////////////////////////////////////////////
// PrimitiveComponent inlines

FORCEINLINE_DEBUGGABLE bool UPrimitiveComponent::ComponentOverlapMulti(TArray<struct FOverlapResult>& OutOverlaps, const class UWorld* InWorld, const FVector& Pos, const FQuat& Rot, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const
{
	return ComponentOverlapMultiImpl(OutOverlaps, InWorld, Pos, Rot, TestChannel, Params, ObjectQueryParams);
}

FORCEINLINE_DEBUGGABLE bool UPrimitiveComponent::ComponentOverlapMulti(TArray<struct FOverlapResult>& OutOverlaps, const class UWorld* InWorld, const FVector& Pos, const FRotator& Rot, ECollisionChannel TestChannel, const struct FComponentQueryParams& Params, const struct FCollisionObjectQueryParams& ObjectQueryParams) const
{
	return ComponentOverlapMultiImpl(OutOverlaps, InWorld, Pos, Rot.Quaternion(), TestChannel, Params, ObjectQueryParams);
}

FORCEINLINE_DEBUGGABLE bool UPrimitiveComponent::ComponentOverlapComponent(class UPrimitiveComponent* PrimComp, const FVector Pos, const FQuat& Rot, const FCollisionQueryParams& Params)
{
	return ComponentOverlapComponentImpl(PrimComp, Pos, Rot, Params);
}

FORCEINLINE_DEBUGGABLE bool UPrimitiveComponent::ComponentOverlapComponent(class UPrimitiveComponent* PrimComp, const FVector Pos, const FRotator Rot, const FCollisionQueryParams& Params)
{
	return ComponentOverlapComponentImpl(PrimComp, Pos, Rot.Quaternion(), Params);
}

FORCEINLINE_DEBUGGABLE const TArray<FOverlapInfo>& UPrimitiveComponent::GetOverlapInfos() const
{
	return OverlappingComponents;
}

FORCEINLINE_DEBUGGABLE bool UPrimitiveComponent::K2_IsCollisionEnabled() const
{
	return IsCollisionEnabled();
}

FORCEINLINE_DEBUGGABLE bool UPrimitiveComponent::K2_IsQueryCollisionEnabled() const
{
	return IsQueryCollisionEnabled();
}

FORCEINLINE_DEBUGGABLE bool UPrimitiveComponent::K2_IsPhysicsCollisionEnabled() const
{
	return IsPhysicsCollisionEnabled();
}

FORCEINLINE_DEBUGGABLE bool UPrimitiveComponent::GetGenerateOverlapEvents() const
{
	return bGenerateOverlapEvents;
}