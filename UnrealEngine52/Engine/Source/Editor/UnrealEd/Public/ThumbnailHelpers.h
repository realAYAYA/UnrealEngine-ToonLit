// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "PreviewScene.h"
#include "TickableEditorObject.h"
#include "Animation/SkeletalMeshActor.h"
#include "ThumbnailHelpers.generated.h"

class FSceneViewFamily;
class USceneThumbnailInfo;

class UNREALED_API FThumbnailPreviewScene : public FPreviewScene, public FTickableEditorObject
{
public:
	/** Constructor */
	FThumbnailPreviewScene();

	UE_DEPRECATED(5.0, "Use CreateView")
	void GetView(FSceneViewFamily* ViewFamily, int32 X, int32 Y, uint32 SizeX, uint32 SizeY) const;

	/** Allocates then adds an FSceneView to the ViewFamily. */
	[[nodiscard]] FSceneView* CreateView(FSceneViewFamily* ViewFamily, int32 X, int32 Y, uint32 SizeX, uint32 SizeY) const;

	/* Begin FTickableEditorObject */
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override { return ETickableTickType::Always; }
	virtual TStatId GetStatId() const override;
	/* End FTickableEditorObject */

protected:
	/** Helper function to get the bounds offset to display an asset */
	float GetBoundsZOffset(const FBoxSphereBounds& Bounds) const;

	/**
	  * Gets parameters to create a view matrix to be used by CreateView(). Implemented in children classes.
	  * @param InFOVDegrees  The FOV used to display the thumbnail. Often used to calculate the output parameters.
	  * @param OutOrigin	 The origin of the orbit view. Typically the center of the bounds of the target object.
	  * @param OutOrbitPitch The pitch of the orbit cam around the object.
	  * @param OutOrbitYaw	 The yaw of the orbit cam around the object.
	  * @param OutOrbitZoom  The camera distance from the object.
	  */
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const = 0;
};

class UNREALED_API FParticleSystemThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor/Destructor */
	FParticleSystemThumbnailScene();
	virtual ~FParticleSystemThumbnailScene();

	/** Sets the particle system to use in the next CreateView() */
	void SetParticleSystem(class UParticleSystem* ParticleSystem);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

protected:
	/** The particle system component used to display all particle system thumbnails */
	class UParticleSystemComponent* PartComponent;

	/** The FXSystem used to render all thumbnail particle systems */
	class FFXSystemInterface* ThumbnailFXSystem;
};

class UNREALED_API FMaterialThumbnailScene : public FThumbnailPreviewScene
{
public:	
	/** Constructor */
	FMaterialThumbnailScene();

	/** Sets the material to use in the next CreateView() */
	void SetMaterialInterface(class UMaterialInterface* InMaterial);

	bool ShouldSetSeparateTranslucency(class UMaterialInterface* InMaterial) const;

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

protected:
	/** The static mesh actor used to display all material thumbnails */
	class AStaticMeshActor* PreviewActor;
	/** Material being used for something that only makes sense to visualize as a plane (UI, particle sprites)*/
	bool bForcePlaneThumbnail;
};

class UNREALED_API FSkeletalMeshThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FSkeletalMeshThumbnailScene();

	/** Sets the skeletal mesh to use in the next CreateView() */
	void SetSkeletalMesh(class USkeletalMesh* InSkeletalMesh);

	/** Set whether to draw debug skeleton */
	void SetDrawDebugSkeleton(bool bInDrawDebugSkeleton, const FLinearColor& InSkeletonColor);
	
	/** Returns the preview actor within the scene */
	class ASkeletalMeshActor* GetPreviewActor() { return PreviewActor; }

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:
	/** The skeletal mesh actor used to display all skeletal mesh thumbnails */
	class ASkeletalMeshActor* PreviewActor;

	/** The color used to draw the debug skeleton */
	FLinearColor DrawDebugColor = FLinearColor::White;
	
	/** Whether to draw debug skeleton */
	bool bDrawDebugSkeleton = false;
};

class UNREALED_API FStaticMeshThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FStaticMeshThumbnailScene();

	/** Sets the static mesh to use in the next CreateView() */
	void SetStaticMesh(class UStaticMesh* StaticMesh);

	/** Sets override materials for the static mesh  */
	void SetOverrideMaterials(const TArray<class UMaterialInterface*>& OverrideMaterials);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:
	/** The static mesh actor used to display all static mesh thumbnails */
	class AStaticMeshActor* PreviewActor;
};

UCLASS(ClassGroup = ISkeletalMeshes, ComponentWrapperClass, ConversionRoot, meta = (ChildCanTick))
class AAnimationThumbnailSkeletalMeshActor : public ASkeletalMeshActor
{
	GENERATED_UCLASS_BODY()
};

class UNREALED_API FAnimationSequenceThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FAnimationSequenceThumbnailScene();

	/** Sets the animation to use in the next CreateView() */
	bool SetAnimation(class UAnimSequenceBase* InAnimation);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;
	
	//Clean up the children of this component
	void CleanupComponentChildren(USceneComponent* Component);

private:
	/** The skeletal mesh actor used to display all animation thumbnails */
	class AAnimationThumbnailSkeletalMeshActor* PreviewActor;

	/** Animation we are generating the thumbnail for */
	class UAnimSequenceBase* PreviewAnimation;
};

class UNREALED_API FBlendSpaceThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FBlendSpaceThumbnailScene();

	/** Sets the animation to use in the next CreateView() */
	bool SetBlendSpace(class UBlendSpace* InBlendSpace);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

	//Clean up the children of this component
	void CleanupComponentChildren(USceneComponent* Component);

private:
	/** The skeletal mesh actor used to display all animation thumbnails */
	class AAnimationThumbnailSkeletalMeshActor* PreviewActor;

	/** Animation we are generating the thumbnail for */
	class UBlendSpace* PreviewAnimation;
};

class UNREALED_API FAnimBlueprintThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FAnimBlueprintThumbnailScene();

	/** Sets the animation blueprint to use in the next CreateView() */
	bool SetAnimBlueprint(class UAnimBlueprint* InBlueprint);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

	//Clean up the children of this component
	void CleanupComponentChildren(USceneComponent* Component);

private:
	/** The skeletal mesh actor used to display all animation thumbnails */
	class ASkeletalMeshActor* PreviewActor;

	/** Animation Blueprint we are generating the thumbnail for */
	class UAnimBlueprint* PreviewBlueprint;
};

class UNREALED_API FPhysicsAssetThumbnailScene : public FThumbnailPreviewScene
{
public:
	/** Constructor */
	FPhysicsAssetThumbnailScene();

	/** Sets the skeletal mesh to use in the next CreateView() */
	void SetPhysicsAsset(class UPhysicsAsset* InPhysicsAsset);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

private:
	/** The skeletal mesh actor used to display all physics asset thumbnails */
	class ASkeletalMeshActor* PreviewActor;
};

class UActorComponent;

class UNREALED_API FClassActorThumbnailScene : public FThumbnailPreviewScene
{
public:

	FClassActorThumbnailScene();

	/** Returns true if this component can be visualized */
	static bool IsValidComponentForVisualization(UActorComponent* Component);

protected:
	// FThumbnailPreviewScene implementation
	virtual void GetViewMatrixParameters(const float InFOVDegrees, FVector& OutOrigin, float& OutOrbitPitch, float& OutOrbitYaw, float& OutOrbitZoom) const override;

	/** Sets the object (class or blueprint) used in the next CreateView() */
	void SpawnPreviewActor(class UClass* Obj);

	/** Get the scene thumbnail info to use for the object currently being rendered */
	virtual USceneThumbnailInfo* GetSceneThumbnailInfo(const float TargetDistance) const = 0;

	FBoxSphereBounds GetPreviewActorBounds() const;

private:

	/** Clears out any stale actors in this scene if PreviewActor enters a stale state */
	void ClearStaleActors();

	int32 NumStartingActors;
	TWeakObjectPtr<class AActor> PreviewActor;
};

class UNREALED_API FBlueprintThumbnailScene : public FClassActorThumbnailScene
{
public:
	/** Constructor/Destructor */
	FBlueprintThumbnailScene();

	/** Sets the static mesh to use in the next CreateView() */
	void SetBlueprint(class UBlueprint* Blueprint);

	/** Refreshes components for the specified blueprint */
	void BlueprintChanged(class UBlueprint* Blueprint);

protected:

	/** Get the scene thumbnail info to use for the object currently being rendered */
	virtual USceneThumbnailInfo* GetSceneThumbnailInfo(const float TargetDistance) const override;

private:
	/** The blueprint that is currently being rendered. NULL when not rendering. */
	TWeakObjectPtr<class UBlueprint> CurrentBlueprint;
};

class UNREALED_API FClassThumbnailScene : public FClassActorThumbnailScene
{
public:
	/** Constructor/Destructor */
	FClassThumbnailScene();

	/** Sets the class use in the next CreateView() */
	void SetClass(class UClass* Class);

protected:
	/** Get the scene thumbnail info to use for the object currently being rendered */
	virtual USceneThumbnailInfo* GetSceneThumbnailInfo(const float TargetDistance) const override;

private:
	/** The class that is currently being rendered. NULL when not rendering. */
	UClass* CurrentClass;
};

/** Handles instancing thumbnail scenes for Class and Blueprint types (use the class or generated class as the key). */
template <typename ThumbnailSceneType, int32 MaxNumScenes>
class TClassInstanceThumbnailScene
{
public:
	/** Constructor */
	TClassInstanceThumbnailScene()
	{
		InstancedThumbnailScenes.Reserve(MaxNumScenes);
	}

	/** Find an existing thumbnail scene instance for this class type. */
	TSharedPtr<ThumbnailSceneType> FindThumbnailScene(const UClass* InClass) const
	{
		check(InClass);
		const FName ClassName = InClass->GetFName();

		return InstancedThumbnailScenes.FindRef(ClassName);
	}

	/** Find or create a thumbnail scene instance for this class type. */
	TSharedRef<ThumbnailSceneType> EnsureThumbnailScene(const UClass* InClass)
	{
		check(InClass);
		const FName ClassName = InClass->GetFName();

		TSharedPtr<ThumbnailSceneType> ExistingThumbnailScene = InstancedThumbnailScenes.FindRef(ClassName);
		if (!ExistingThumbnailScene.IsValid())
		{
			if (InstancedThumbnailScenes.Num() >= MaxNumScenes)
			{
				InstancedThumbnailScenes.Reset();
				// Will hitch but is better than a crash
				CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			}

			ExistingThumbnailScene = MakeShareable(new ThumbnailSceneType());
			InstancedThumbnailScenes.Add(ClassName, ExistingThumbnailScene);
		}

		return ExistingThumbnailScene.ToSharedRef();
	}

	/** Removes the thumbnail scene instance for the specified class */
	void RemoveThumbnailScene(const UClass* InClass)
	{
		check(InClass);
		InstancedThumbnailScenes.Remove(InClass->GetFName());
	}

	/** Clears all thumbnail scenes */
	void Clear()
	{
		InstancedThumbnailScenes.Reset();
	}

private:
	/**
	 * Mapping between the class type and its thumbnail scene.
	 * @note This uses the class name rather than the class pointer to avoid leaving behind stale class instances as Blueprints are re-compiled.
	 */
	TMap<FName, TSharedPtr<ThumbnailSceneType>> InstancedThumbnailScenes;
};


/** Handles instancing thumbnail scenes for Object based Asset types (use the path as the key). */
template <typename ThumbnailSceneType, int32 MaxNumScenes>
class TObjectInstanceThumbnailScene
{
public:
	/** Constructor */
	TObjectInstanceThumbnailScene()
	{
		InstancedThumbnailScenes.Reserve(MaxNumScenes);
	}

	/** Find an existing thumbnail scene instance for this class type. */
	TSharedPtr<ThumbnailSceneType> FindThumbnailScene(const UObject* InObject) const
	{
		check(InObject);
		const FString ObjectPath = InObject->GetPathName();

		return InstancedThumbnailScenes.FindRef(ObjectPath);
	}

	/** Find or create a thumbnail scene instance for this class type. */
	TSharedRef<ThumbnailSceneType> EnsureThumbnailScene(const UObject* InObject)
	{
		check(InObject);
		const FString ObjectPath = InObject->GetPathName();

		TSharedPtr<ThumbnailSceneType> ExistingThumbnailScene = InstancedThumbnailScenes.FindRef(ObjectPath);
		if (!ExistingThumbnailScene.IsValid())
		{
			if (InstancedThumbnailScenes.Num() >= MaxNumScenes)
			{
				InstancedThumbnailScenes.Reset();
				// Will hitch but is better than a crash
				CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			}

			ExistingThumbnailScene = MakeShareable(new ThumbnailSceneType());
			InstancedThumbnailScenes.Add(ObjectPath, ExistingThumbnailScene);
		}

		return ExistingThumbnailScene.ToSharedRef();
	}

	/** Removes the thumbnail scene instance for the specified class */
	void RemoveThumbnailScene(const UObject* InObject)
	{
		check(InObject);
		InstancedThumbnailScenes.Remove(InObject->GetPathName());
	}

	/** Clears all thumbnail scenes */
	void Clear()
	{
		InstancedThumbnailScenes.Reset();
	}

private:
	/**
	 * Mapping between the class type and its thumbnail scene.
	 * @note This uses the class name rather than the class pointer to avoid leaving behind stale class instances as Blueprints are re-compiled.
	 */
	TMap<FString, TSharedPtr<ThumbnailSceneType>> InstancedThumbnailScenes;
};

