// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
#include "SceneTypes.h"
#include "SceneView.h"
#include "ShowFlags.h"
#include "Engine/GameViewportClient.h"
#include "SceneCaptureComponent.generated.h"

class AActor;
class FSceneViewStateInterface;
class UPrimitiveComponent;

/** 
* View state needed to create a scene capture renderer 
* Inherits from FSceneViewProjectionData to unify resolving of possible projection correction calculations.
*/
struct FSceneCaptureViewInfo : public FSceneViewProjectionData
{
	FVector ViewLocation;
	FRotator ViewRotation;
	EStereoscopicPass StereoPass;
	int32 StereoViewIndex;
};

#if WITH_EDITORONLY_DATA
/** Editor only structure for gathering memory size */
struct FSceneCaptureMemorySize : public FThreadSafeRefCountedObject
{
	uint64 Size = 0;
};
#endif

USTRUCT(BlueprintType)
struct FEngineShowFlagsSetting
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	FString ShowFlagName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	bool Enabled = false;


	bool operator == (const FEngineShowFlagsSetting& Other) const
	{
		return ShowFlagName == Other.ShowFlagName && Other.Enabled == Enabled;
	}
};

UENUM()
enum class ESceneCapturePrimitiveRenderMode : uint8
{
	/** Legacy */
	PRM_LegacySceneCapture UMETA(DisplayName = "Render Scene Primitives (Legacy)"),
	/** Render primitives in the scene, minus HiddenActors. */
	PRM_RenderScenePrimitives UMETA(DisplayName = "Render Scene Primitives"),
	/** Render only primitives in the ShowOnlyActors list, or components specified with ShowOnlyComponent(). */
	PRM_UseShowOnlyList UMETA(DisplayName = "Use ShowOnly List")
};

	// -> will be exported to EngineDecalClasses.h
UCLASS(abstract, hidecategories=(Collision, Object, Physics, SceneComponent, Mobility), MinimalAPI)
class USceneCaptureComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** Controls what primitives get rendered into the scene capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	ESceneCapturePrimitiveRenderMode PrimitiveRenderMode;

	UPROPERTY(interp, Category = SceneCapture, meta = (DisplayName = "Capture Source"))
	TEnumAsByte<enum ESceneCaptureSource> CaptureSource;

	/** Whether to update the capture's contents every frame.  If disabled, the component will render once on load and then only when moved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	uint8 bCaptureEveryFrame : 1;

	/** Whether to update the capture's contents on movement.  Disable if you are going to capture manually from blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	uint8 bCaptureOnMovement : 1;

	/** Whether to persist the rendering state even if bCaptureEveryFrame==false.  This allows velocities for Motion Blur and Temporal AA to be computed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture, meta = (editcondition = "!bCaptureEveryFrame"))
	bool bAlwaysPersistRenderingState;

	/** The components won't rendered by current component.*/
 	UPROPERTY()
 	TArray<TWeakObjectPtr<UPrimitiveComponent> > HiddenComponents;

	/** The actors to hide in the scene capture. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=SceneCapture)
	TArray<TObjectPtr<AActor>> HiddenActors;

	/** The only components to be rendered by this scene capture, if PrimitiveRenderMode is set to UseShowOnlyList. */
 	UPROPERTY()
 	TArray<TWeakObjectPtr<UPrimitiveComponent> > ShowOnlyComponents;

	/** The only actors to be rendered by this scene capture, if PrimitiveRenderMode is set to UseShowOnlyList.*/
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category=SceneCapture)
	TArray<TObjectPtr<AActor>> ShowOnlyActors;

	/** Scales the distance used by LOD. Set to values greater than 1 to cause the scene capture to use lower LODs than the main view to speed up the scene capture pass. */
	UPROPERTY(EditAnywhere, Category=PlanarReflection, meta=(UIMin = ".1", UIMax = "10"), AdvancedDisplay)
	float LODDistanceFactor;

	/** if > 0, sets a maximum render distance override.  Can be used to cull distant objects from a reflection if the reflecting plane is in an enclosed area like a hallway or room */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture, meta=(UIMin = "100", UIMax = "10000"))
	float MaxViewDistanceOverride;

	/** Capture priority within the frame to sort scene capture on GPU to resolve interdependencies between multiple capture components. Highest come first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=SceneCapture)
	int32 CaptureSortPriority;

	/** Whether to use ray tracing for this capture. Ray Tracing must be enabled in the project. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	bool bUseRayTracingIfEnabled;

	/** ShowFlags for the SceneCapture's ViewFamily, to control rendering settings for this view. Hidden but accessible through details customization */
	UPROPERTY(EditAnywhere, interp, Category=SceneCapture)
	TArray<struct FEngineShowFlagsSetting> ShowFlagSettings;

	// TODO: Make this a UStruct to set directly?
	/** Settings stored here read from the strings and int values in the ShowFlagSettings array */
	FEngineShowFlags ShowFlags;

public:
	/** Name of the profiling event. */
	UPROPERTY(EditAnywhere, interp, Category = SceneCapture)
	FString ProfilingEventName;

	ENGINE_API virtual void BeginDestroy() override;

	//~ Begin UActorComponent Interface
	ENGINE_API virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;
	ENGINE_API virtual void OnRegister() override;
	//~ End UActorComponent Interface

	/** Adds the component to our list of hidden components. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void HideComponent(UPrimitiveComponent* InComponent);

	/**
	 * Adds all primitive components in the actor to our list of hidden components.
	 * @param bIncludeFromChildActors Whether to include the components from child actors
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void HideActorComponents(AActor* InActor, const bool bIncludeFromChildActors = false);

	/** Adds the component to our list of show-only components. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void ShowOnlyComponent(UPrimitiveComponent* InComponent);

	/**
	 * Adds all primitive components in the actor to our list of show-only components.
	 * @param bIncludeFromChildActors Whether to include the components from child actors
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void ShowOnlyActorComponents(AActor* InActor, const bool bIncludeFromChildActors = false);

	/** Removes a component from the Show Only list. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void RemoveShowOnlyComponent(UPrimitiveComponent* InComponent);

	/**
	 * Removes an actor's components from the Show Only list.
	 * @param bIncludeFromChildActors Whether to remove the components from child actors
	 */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void RemoveShowOnlyActorComponents(AActor* InActor, const bool bIncludeFromChildActors = false);

	/** Clears the Show Only list. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void ClearShowOnlyComponents();

	/** Clears the hidden list. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void ClearHiddenComponents();

	/** Changes the value of TranslucentSortPriority. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	ENGINE_API void SetCaptureSortPriority(int32 NewCaptureSortPriority);

	/** Returns the view state, if any, and allocates one if needed. This function can return NULL, e.g. when bCaptureEveryFrame is false. */
	ENGINE_API FSceneViewStateInterface* GetViewState(int32 ViewIndex);

#if WITH_EDITOR
	ENGINE_API virtual bool CanEditChange(const FProperty* InProperty) const override;
	ENGINE_API virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif	

	ENGINE_API virtual void Serialize(FArchive& Ar);

	ENGINE_API virtual void OnUnregister() override;
	ENGINE_API virtual void PostLoad() override;

	/** To leverage a component's bOwnerNoSee/bOnlyOwnerSee properties, the capture view requires an "owner". Override this to set a "ViewActor" for the scene. */
	virtual const AActor* GetViewOwner() const { return nullptr; }

	static ENGINE_API void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	static ENGINE_API void UpdateDeferredCaptures(FSceneInterface* Scene);

	/** Whether this component is a USceneCaptureComponentCube */
	virtual bool IsCube() const { return false; }

protected:
	/** Update the show flags from our show flags settings (ideally, you'd be able to set this more directly, but currently unable to make FEngineShowFlags a UStruct to use it as a FProperty...) */
	ENGINE_API void UpdateShowFlags();

	ENGINE_API void RegisterDelegates();
	ENGINE_API void UnregisterDelegates();
	ENGINE_API void ReleaseGarbageReferences();

	ENGINE_API bool IsCulledByDetailMode() const;

	virtual void UpdateSceneCaptureContents(FSceneInterface* Scene) {};

	/**
	 * The view state holds persistent scene rendering state and enables occlusion culling in scene captures.
	 * NOTE: This object is used by the rendering thread. When the game thread attempts to destroy it, FDeferredCleanupInterface will keep the object around until the RT is done accessing it.
	 * NOTE: It is not safe to put a FSceneViewStateReference in a TArray, which moves its contents around without calling element constructors during realloc.
	 */
	TIndirectArray<FSceneViewStateReference> ViewStates;

#if WITH_EDITORONLY_DATA
	/** The mesh used by ProxyMeshComponent */
	UPROPERTY(transient)
	TObjectPtr<class UStaticMesh> CaptureMesh;

	/** The mesh to show visually where the camera is placed */
	TObjectPtr<class UStaticMeshComponent> ProxyMeshComponent;

public:
	/** Thread safe storage for memory statistics for a scene capture */
	TRefCountPtr<FSceneCaptureMemorySize> CaptureMemorySize;
#endif
};

