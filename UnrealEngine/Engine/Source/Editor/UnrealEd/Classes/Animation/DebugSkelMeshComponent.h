// Copyright Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EngineDefines.h"
#include "Components/SkeletalMeshComponent.h"
#include "Delegates/DelegateCombinations.h"
#include "DebugSkelMeshComponent.generated.h"

class Error;

DECLARE_DELEGATE_RetVal(FText, FGetExtendedViewportText);
DECLARE_DELEGATE(FOnDebugForceLODChanged);

USTRUCT()
struct FSelectedSocketInfo
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor */
	FSelectedSocketInfo()
		: Socket(nullptr)
		, bSocketIsOnSkeleton( false )
	{

	}

	/** Constructor */
	FSelectedSocketInfo( class USkeletalMeshSocket* InSocket, bool bInSocketIsOnSkeleton )
		: Socket( InSocket )
		, bSocketIsOnSkeleton( bInSocketIsOnSkeleton )
	{

	}

	bool IsValid() const
	{
		return Socket != nullptr;
	}

	void Reset()
	{
		Socket = nullptr;
	}

	/** The socket we have selected */
	class USkeletalMeshSocket* Socket;

	/** true if on skeleton, false if on mesh */
	bool bSocketIsOnSkeleton;
};

/** Different modes for Persona's Turn Table. */
namespace EPersonaTurnTableMode
{
	enum Type
	{
		Stopped,
		Playing,
		Paused
	};
};

/** Different modes for when processing root motion */
UENUM()
enum class EProcessRootMotionMode : uint8
{
	/** Preview mesh will not consume root motion */
	Ignore,

	/** Preview mesh will consume root motion continually */
	Loop,

	/** Preview mesh will consume root motion resetting the position back to the origin every time the animation loops */
	LoopAndReset
};

//////////////////////////////////////////////////////////////////////////
// FDebugSkelMeshSceneProxy

class UDebugSkelMeshComponent;
class FSkeletalMeshRenderData;

class FDebugSkelMeshDynamicData
{
public:

	FDebugSkelMeshDynamicData(UDebugSkelMeshComponent* InComponent);

	bool bDrawMesh;
	bool bDrawNormals;
	bool bDrawTangents;
	bool bDrawBinormals;
	bool bDrawClothPaintPreview;

	bool bFlipNormal;

	int32 ClothingSimDataIndexWhenPainting;
	TArray<uint32> ClothingSimIndices;

	TArray<float> ClothingVisiblePropertyValues;
	float PropertyViewMin;
	float PropertyViewMax;

	TArray<FVector3f> SkinnedPositions;
	TArray<FVector3f> SkinnedNormals;
};

/**
* A skeletal mesh component scene proxy with additional debugging options.
*/
class FDebugSkelMeshSceneProxy : public FSkeletalMeshSceneProxy
{
public:
	SIZE_T GetTypeHash() const override;

	/**
	* Constructor.
	* @param	Component - skeletal mesh primitive being added
	*/
	FDebugSkelMeshSceneProxy(const UDebugSkelMeshComponent* InComponent, FSkeletalMeshRenderData* InSkelMeshRenderData, FLinearColor InWireframeOverlayColor = FLinearColor::White);

	virtual ~FDebugSkelMeshSceneProxy()
	{}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	FDebugSkelMeshDynamicData* DynamicData;

	SIZE_T GetAllocatedSize() const
	{
		return FSkeletalMeshSceneProxy::GetAllocatedSize();
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

private:

	bool bSelectable;
};

/** Generic modes used to render debug skeletons depending on editor-specific context */
UENUM()
enum class ESkeletonDrawMode : uint8
{
	/** Bones are visible and selectable */
	Default,

	/** Bones are completely hidden */
	Hidden,

	/** Bones are visible but non-selectable */
	GreyedOut
};

UCLASS(transient)
class UNREALED_API UDebugSkelMeshComponent : public USkeletalMeshComponent
{
	GENERATED_UCLASS_BODY()
	
	/** Global drawing mode for this skeleton. Depends on context of specific editor using the component. */
	UPROPERTY()
	ESkeletonDrawMode SkeletonDrawMode = ESkeletonDrawMode::Default;

	/** If true then the skeletal mesh associated with the component is drawn. */
	UPROPERTY()
	uint32 bDrawMesh:1;
	
	/** If true then the bone names associated with the skeletal mesh are displayed */
	UPROPERTY()
	uint32 bShowBoneNames:1;

	/** Bone influences viewing */
	UPROPERTY(transient)
	uint32 bDrawBoneInfluences:1;

	/** Morphtarget viewing */
	UPROPERTY(transient)
	uint32 bDrawMorphTargetVerts : 1;

	/** Vertex normal viewing */
	UPROPERTY(transient)
	uint32 bDrawNormals:1;

	/** Vertex tangent viewing */
	UPROPERTY(transient)
	uint32 bDrawTangents:1;

	/** Vertex binormal viewing */
	UPROPERTY(transient)
	uint32 bDrawBinormals:1;

	/** Socket hit points viewing */
	UPROPERTY(transient)
	uint32 bDrawSockets:1;

	/** Attribute visualization */
	UPROPERTY(transient)
	uint32 bDrawAttributes : 1;

	/** Skeleton sockets visible? */
	UPROPERTY(transient)
	uint32 bSkeletonSocketsVisible:1;

	/** Mesh sockets visible? */
	UPROPERTY(transient)
	uint32 bMeshSocketsVisible:1;

	/** Display raw animation bone transform */
	UPROPERTY(transient)
	uint32 bDisplayRawAnimation:1;

	/** Display non retargeted animation pose */
	UPROPERTY(Transient)
	uint32 bDisplayNonRetargetedPose:1;

	/** Display additive base bone transform */
	UPROPERTY(transient)
	uint32 bDisplayAdditiveBasePose:1;

	/** Display baked animation pose */
	UPROPERTY(Transient)
	uint32 bDisplayBakedAnimation:1;

	/** Display source animation pose */
	UPROPERTY(Transient)
	uint32 bDisplaySourceAnimation:1;

	/** Display Bound **/
	UPROPERTY(transient)
	bool bDisplayBound;

	UPROPERTY(transient)
	bool bDisplayVertexColors;

	UPROPERTY(transient)
	FLinearColor WireframeMeshOverlayColor;

	UE_DEPRECATED(5.0, "This variable is no longer used. Use ProcessRootMotionMode instead.")
	UPROPERTY()
	uint32 bPreviewRootMotion_DEPRECATED : 1;

	/** Process root motion mode */
	UPROPERTY(transient)
	EProcessRootMotionMode ProcessRootMotionMode;

	/** Playback time last time ConsumeRootmotion was called */
	UPROPERTY(transient)
	float ConsumeRootMotionPreviousPlaybackTime;

	UPROPERTY(transient)
	uint32 bShowClothData : 1;

	UPROPERTY(transient)
	float MinClothPropertyView;

	UPROPERTY(transient)
	float MaxClothPropertyView;

	UPROPERTY(transient)
	float ClothMeshOpacity;

	UPROPERTY(transient)
	bool bClothFlipNormal;

	UPROPERTY(transient)
	bool bClothCullBackface;

	UPROPERTY(transient)
	uint32 bRequiredBonesUpToDateDuringTick : 1;

	/** Multiplier for the bone radius rendering */
	UPROPERTY(transient)
	float BoneRadiusMultiplier;

	/* Bounds computed from cloth. */
	FBoxSphereBounds CachedClothBounds;

	/** Non Compressed SpaceBases for when bDisplayRawAnimation == true **/
	TArray<FTransform> UncompressedSpaceBases;

	/** Storage of Additive Base Pose for when bDisplayAdditiveBasePose == true, as they have to be calculated */
	TArray<FTransform> AdditiveBasePoses;

	/** Storage for non retargeted pose. */
	TArray<FTransform> NonRetargetedSpaceBases;

	/** Storage of Baked Animation Pose for when bDisplayBakedAnimation == true, as they have to be calculated */
	TArray<FTransform> BakedAnimationPoses;

	/** Storage of Source Animation Pose for when bDisplaySourceAnimation == true, as they have to be calculated */
	TArray<FTransform> SourceAnimationPoses;
	
	/** Array of bones to render bone weights for */
	UPROPERTY(transient)
	TArray<int32> BonesOfInterest;

	/** Array of morphtargets to render verts for */
	UPROPERTY(transient)
	TArray<TObjectPtr<class UMorphTarget>> MorphTargetOfInterests;

	/** Array of materials to restore when not rendering blend weights */
	UPROPERTY(transient)
	TArray<TObjectPtr<class UMaterialInterface>> SkelMaterials;
	
	UPROPERTY(transient, NonTransactional)
	TObjectPtr<class UAnimPreviewInstance> PreviewInstance;

	UPROPERTY(transient)
	TObjectPtr<class UAnimInstance> SavedAnimScriptInstance;

	/** Does this component use in game bounds or does it use bounds calculated from bones */
	UPROPERTY(transient)
	bool bIsUsingInGameBounds;
	
	/** Does this component use pre-skinned bounds? This overrides other bounds settings */
	UPROPERTY(transient)
	bool bIsUsingPreSkinnedBounds;

	/** Base skel mesh has support for suspending clothing, but single ticks are more of a debug feature when stepping through an animation
	 *  So we control that using this flag
	 */
	UPROPERTY(transient)
	bool bPerformSingleClothingTick;

	UPROPERTY(transient)
	bool bPauseClothingSimulationWithAnim;

	/** Should the LOD of the debug mesh component track the LOD of the instance being debugged */
	UPROPERTY(transient)
	bool bTrackAttachedInstanceLOD;

	// Helper method that sets the forced lod
	void SetDebugForcedLOD(int32 InNewForcedLOD);

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	
	// engine only draw bounds IF selected
	// @todo fix this properly
	// this isn't really the best way to do this, but for now
	// we'll just mark as selected
	virtual bool ShouldRenderSelected() const override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin SkinnedMeshComponent Interface
	virtual bool ShouldCPUSkin() override;
	virtual void PostInitMeshObject(class FSkeletalMeshObject* MeshObject) override;
	virtual void RefreshBoneTransforms(FActorComponentTickFunction* TickFunction = NULL) override;
	virtual int32 GetLODBias() const override { return 0; }
	//~ End SkinnedMeshComponent Interface

	//~ Begin SkeletalMeshComponent Interface
	virtual void InitAnim(bool bForceReinit) override;
	virtual bool IsWindEnabled() const override { return true; }
	virtual void SetAnimClass(class UClass* NewClass) override;
	//~ End SkeletalMeshComponent Interface

	// return true if currently preview animation asset is on
	virtual bool IsPreviewOn() const;

	// @todo document
	FString GetPreviewText() const;

	// @todo anim : you still need to give asset, so that we know which one to disable
	// we can disable per asset, so that if some other window disabled before me, I don't accidently turn it off
	virtual void EnablePreview(bool bEnable, class UAnimationAsset * PreviewAsset);

	// reference pose for this component
	// we don't want to use default refpose because you still want to move joint when this mode is on
	virtual void ShowReferencePose(bool bRefPose);
	virtual bool IsReferencePoseShown() const;

	/**
	 * Update material information depending on color render mode 
	 * Refresh/replace materials 
	 */
	void SetShowBoneWeight(bool bNewShowBoneWeight);

	/**
	* Update material information depending on color render mode
	* Refresh/replace materials
	*/
	void SetShowMorphTargetVerts(bool bNewShowMorphTargetVerts);

	/**
	 * Does it use in-game bounds or bounds calculated from bones
	 */
	bool IsUsingInGameBounds() const;

	/**
	 * Set to use in-game bounds or bounds calculated from bones
	 */
	void UseInGameBounds(bool bUseInGameBounds);

	/**
	 * Does it use pre-skinned bounds
	 */
	bool IsUsingPreSkinnedBounds() const;

	/**
	 * Set to use pre-skinned bounds
	 */
	void UsePreSkinnedBounds(bool bUsePreSkinnedBounds);

	/**
	 * Test if in-game bounds are as big as preview bounds
	 */
	bool CheckIfBoundsAreCorrrect();

	/** Get the in-game bounds of the skeleton mesh */
	FBoxSphereBounds CalcGameBounds(const FTransform& LocalToWorld) const;

	/** 
	 * Update components position based on animation root motion
	 */
	void ConsumeRootMotion(const FVector& FloorMin, const FVector& FloorMax);

	/** Sets the flag used to determine whether or not the current active cloth sim mesh should be rendered */
	void SetShowClothProperty(bool bState);

	/** Get whether we should be previewing root motion */
	UE_DEPRECATED(5.0, "Please use IsProcessingRootMotion or GetProcessRootMotionMode")
	bool GetPreviewRootMotion() const { return IsProcessingRootMotion(); }

	/** Set whether we should be previewing root motion. Note: disabling root motion preview resets transform. */
	UE_DEPRECATED(5.0, "Please use SetProcessRootMotionMode")
	void SetPreviewRootMotion(bool bInPreviewRootMotion) { SetProcessRootMotionMode(bInPreviewRootMotion ? EProcessRootMotionMode::Loop : EProcessRootMotionMode::Ignore); }

	/** Whether we are processing root motion or not */
	bool IsProcessingRootMotion() const;

	/** Gets process root motion mode */
	EProcessRootMotionMode GetProcessRootMotionMode() const;

	/** Sets process root motion mode. Note: disabling root motion preview resets transform. */
	void SetProcessRootMotionMode(EProcessRootMotionMode Mode);

	/** Whether the supplied root motion mode can be used for the current asset */
	bool CanUseProcessRootMotionMode(EProcessRootMotionMode Mode) const;

	/** Whether the current asset or animation blueprint is using root motion */
	bool DoesCurrentAssetHaveRootMotion() const;

	/** Whether the current LOD of the debug mesh is being synced with the attached (preview) mesh instance. */
	bool IsTrackingAttachedLOD() const;

	/** Set the wireframe mesh overlay color, which basically controls the color of the wireframe. */
	void SetWireframeMeshOverlayColor(FLinearColor Color) { WireframeMeshOverlayColor = Color; }

	/** Get the wireframe mesh overlay color, which basically controls the color of the wireframe. */
	FLinearColor GetWireframeMeshOverlayColor() const { return WireframeMeshOverlayColor; }


#if WITH_EDITOR
	//TODO - This is a really poor way to post errors to the user. Work out a better way.
	struct FAnimNotifyErrors
	{
		FAnimNotifyErrors(UObject* InSourceNotify)
		: SourceNotify(InSourceNotify)
		{}
		UObject* SourceNotify;
		TArray<FString> Errors;
	};
	TArray<FAnimNotifyErrors> AnimNotifyErrors;
	virtual void ReportAnimNotifyError(const FText& Error, UObject* InSourceNotify) override;
	virtual void ClearAnimNotifyErrors(UObject* InSourceNotify) override;

	/** 
	 * Extended viewport text delegate handling. Registering a delegate allows external
	 * objects to place custom text in the anim tools viewports.
	 */
	FDelegateHandle RegisterExtendedViewportTextDelegate(const FGetExtendedViewportText& InDelegate);
	void UnregisterExtendedViewportTextDelegate(const FDelegateHandle& InDelegateHandle);
	const TArray<FGetExtendedViewportText>& GetExtendedViewportTextDelegates() const { return ExtendedViewportTextDelegates; }

	FDelegateHandle RegisterOnDebugForceLODChangedDelegate(const FOnDebugForceLODChanged& InDelegate);
	void UnregisterOnDebugForceLODChangedDelegate();

private:
	TArray<FGetExtendedViewportText> ExtendedViewportTextDelegates;
	FOnDebugForceLODChanged OnDebugForceLODChangedDelegate;
public:

#endif

	/**
	 * Force all body instance to not simulate physics regardless of their physic type
	 * To get back in a default state call : ResetAllBodiesSimulatePhysics
	 */
	void DisableAllBodiesSimulatePhysics()
	{
		for (int32 BodyIdx = 0; BodyIdx < Bodies.Num(); ++BodyIdx)
		{
			if (FBodyInstance* BodyInst = Bodies[BodyIdx])
			{
				BodyInst->SetInstanceSimulatePhysics(false);
			}
		}
	}

	/** 
	 * toggle visibility between cloth sections and non-cloth sections for all LODs
	 * if bShowOnlyClothSections is true, shows only cloth sections. On the other hand, 
	 * if bShowOnlyClothSections is false, hides only cloth sections.
	 */
	void ToggleClothSectionsVisibility(bool bShowOnlyClothSections);
	/** Restore all section visibilities to original states for all LODs */
	void RestoreClothSectionsVisibility();

	/** 
	 * To normal game/runtime code we don't want to expose a non-const pointer to the simulation, so we can only get
	 * one from this editor-only component. Intended for debug options/visualisations/editor-only code to poke the sim
	 */
	IClothingSimulation* GetMutableClothingSimulation();

	/** to avoid clothing reset while modifying properties in Persona */
	virtual void CheckClothTeleport() override;

	/** The currently selected asset guid if we're painting, used to build dynamic mesh to paint sim parameters */
	FGuid SelectedClothingGuidForPainting;

	/** The currently selected LOD for painting */
	int32 SelectedClothingLodForPainting;

	/** The currently selected mask inside the above LOD to be painted */
	int32 SelectedClothingLodMaskForPainting;

	/** Find a section using a clothing asset with the given GUID and set its visiblity */
	void SetMeshSectionVisibilityForCloth(FGuid InClothGuid, bool bVisibility);

	// fixes up the disabled flags so clothing is enabled and originals are disabled as
	// ToggleMeshSectionForCloth will make these get out of sync
	void ResetMeshSectionVisibility();

	// Rebuilds the fixed parameter on the mesh to mesh data, to be used if the editor has
	// changed a vert to be fixed or unfixed otherwise the simulation will not work
	// bInvalidateDerivedDataCache can only be false during previewing as otherwise the changes won't be correctly saved
	UE_DEPRECATED(5.0, "This function is redundant, since it is always called after ApplyParameterMasks and therefore will be removed.")
	void RebuildClothingSectionsFixedVerts(bool bInvalidateDerivedDataCache = true);

	TArray<FVector3f> SkinnedSelectedClothingPositions;
	TArray<FVector3f> SkinnedSelectedClothingNormals;

private:
	// Rebuilds the fixed vertex attribute on any cloth deformer mappings,
	// including LOD bias mappings, that reference the specified LOD section.
	UE_DEPRECATED(5.0, "This function is redundant, since it is always called after ApplyParameterMasks and therefore will be removed.")
	void RebuildClothingSectionFixedVerts(int32 LODIndex, int32 SectionIndex);

	// Helper function to generate space bases for current frame
	void GenSpaceBases(TArray<FTransform>& OutSpaceBases);

	// Helper function to enable overlay material
	void EnableOverlayMaterial(bool bEnable);

	// Rebuilds the cloth bounds for the asset.
	void RebuildCachedClothBounds();
protected:

	// Overridden to support single clothing ticks
	virtual bool ShouldRunClothTick() const override;

	virtual void SendRenderDynamicData_Concurrent() override;

public:
	/** Current turn table mode */
	EPersonaTurnTableMode::Type TurnTableMode;
	/** Current turn table speed scaling */
	float TurnTableSpeedScaling;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	void RefreshSelectedClothingSkinnedPositions();

	virtual bool CanOverrideCollisionProfile() const { return true; }

	virtual void GetUsedMaterials(TArray<UMaterialInterface *>& OutMaterials, bool bGetDebugMaterials = false) const override;

	/**
	 * Define Custom Default pose for this component for preview
	 */
	virtual void SetCustomDefaultPose() {};

	/*
	 *	return RefSkeleton for drawing; 
	 */
	virtual const FReferenceSkeleton& GetReferenceSkeleton() const
	{
		if (GetSkeletalMeshAsset())
		{
			return GetSkeletalMeshAsset()->GetRefSkeleton();
		}

		static FReferenceSkeleton EmptySkeleton;
		return EmptySkeleton;
	}

	/*
	*	return bone indices to draw
	*/
	virtual const TArray<FBoneIndexType>& GetDrawBoneIndices() const
	{
		return RequiredBones;
	}

	virtual int32 GetNumDrawTransform() const 
	{
		return GetNumComponentSpaceTransforms();
	}
	/*
	 *	returns the transform of the joint 
	 */
	virtual FTransform GetDrawTransform(int32 BoneIndex) const
	{
		const TArray<FTransform>& SpaceTransforms = GetComponentSpaceTransforms();
		if (SpaceTransforms.IsValidIndex(BoneIndex))
		{
			return SpaceTransforms[BoneIndex];
		}

		return FTransform::Identity;
	}

};


/*
 * This class is use to remove the alternate skinning preview from the multiple editor that can show it.
 * Important it should be destroy after the PostEditChange of the skeletalmesh is done and the renderdata have been recreate
 * i.e. FScopedSkeletalMeshPostEditChange should be create after FScopedSuspendAlternateSkinWeightPreview and delete before FScopedSuspendAlternateSkinWeightPreview
 */
class UNREALED_API FScopedSuspendAlternateSkinWeightPreview
{
public:
	/*
	 * This constructor suspend the alternate skinning preview for all editor component that use the specified skeletalmesh
	 * Parameters:
	 * @param InSkeletalMesh - SkeletalMesh use to know which preview component we have to suspend the alternate skinning preview.
	 */
	FScopedSuspendAlternateSkinWeightPreview(class USkeletalMesh* InSkeletalMesh);

	/*
	 * This destructor put back the preview alternate skinning
	 */
	~FScopedSuspendAlternateSkinWeightPreview();

private:
	TArray< TTuple<UDebugSkelMeshComponent*, FName> > SuspendedComponentArray;
};