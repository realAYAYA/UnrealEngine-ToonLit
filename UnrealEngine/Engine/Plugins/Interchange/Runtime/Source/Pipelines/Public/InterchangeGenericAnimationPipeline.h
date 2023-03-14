// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeGenericAssetsPipelineSharedSettings.h"
#include "InterchangePipelineBase.h"
#include "InterchangeSourceData.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeGenericAnimationPipeline.generated.h"

class UInterchangeAnimationTrackSetNode;
class UInterchangeMeshNode;
class UInterchangeSkeletalAnimationTrackNode;

/** Animation length type when importing */
UENUM(BlueprintType)
enum class EInterchangeAnimationRange : uint8
{
	/** This option imports the range of frames based on timeline definition in the source. */
	Timeline				UMETA(DisplayName = "Source Timeline"),
	/** This option imports the range of frames that have animation. */
	Animated				UMETA(DisplayName = "Animated Time"),
	/** This option imports the range of frames specified by "FrameImportRange". */
	SetRange				UMETA(DisplayName = "Set Range"),
	MAX,
};


UCLASS(BlueprintType, hidedropdown)
class INTERCHANGEPIPELINES_API UInterchangeGenericAnimationPipeline : public UInterchangePipelineBase
{
	GENERATED_BODY()

public:

	//Common SkeletalMeshes And Animations Properties Settings Pointer
	UPROPERTY(Transient)
	TWeakObjectPtr<UInterchangeGenericCommonSkeletalMeshesAndAnimationsProperties> CommonSkeletalMeshesAndAnimationsProperties;

	//////	ANIMATION_CATEGORY Properties //////
	/** If enable, import all animation assets find in the sources. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	bool bImportAnimations = true;

	/** Import bone transform tracks. If false, this will discard any bone transform tracks.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	bool bImportBoneTracks = true;

	/** Which animation range to import. The one defined at Exported, at Animated time or define a range manually */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations", meta = (EditCondition = "bImportBoneTracks", DisplayName = "Animation Length"))
	EInterchangeAnimationRange AnimationRange = EInterchangeAnimationRange::Timeline;

	/** Frame range used when Set Range is used in Animation Length */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations", meta = (EditCondition = "bImportBoneTracks", UIMin = 0, ClampMin = 0))
	FInt32Interval FrameImportRange = FInt32Interval(0, 0);

	/** Enable this option to use default sample rate for the imported animation at 30 frames per second */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations", meta = (EditCondition = "bImportBoneTracks", ToolTip = "If enabled, samples all animation curves to 30 FPS"))
	bool bUse30HzToBakeBoneAnimation = false;

	/** Use this option to specify a sample rate for the imported animation, a value of 0 use the best matching sample rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations", meta = (EditCondition = "bImportBoneTracks && !bUse30HzToBakeBoneAnimation", ToolTip = "Sample fbx animation data at the specified sample rate, 0 find automaticaly the best sample rate", ClampMin = 0, UIMin = 0, ClampMax = 48000, UIMax = 60))
	int32 CustomBoneAnimationSampleRate = 0;

	/** If enabled, snaps the animation to the closest frame boundary using the import sampling rate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	bool bSnapToClosestFrameBoundary = false;

	/** If true, import node attributes as either Animation Curves or Animation Attributes */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations", meta = (DisplayName = "Import Attributes as Curves or Animation Attributes"))
	bool bImportCustomAttribute = true;

	/** Set Material Curve Type for all custom attributes that exists */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations", meta = (EditCondition = "bImportCustomAttribute", DisplayName = "Set Material Curve Type"))
	bool bSetMaterialDriveParameterOnCustomAttribute = false;

	/** Set Material Curve Type for the custom attribute with the following suffixes. This doesn't matter if Set Material Curve Type is true  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations", meta = (EditCondition = "bImportCustomAttribute", DisplayName = "Material Curve Suffixes"))
	TArray<FString> MaterialCurveSuffixes = {TEXT("_mat")};

	/** When importing custom attribute as curve, remove redundant keys */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations", meta = (EditCondition = "bImportCustomAttribute", DisplayName = "Remove Redundant Keys"))
	bool bRemoveCurveRedundantKeys = false;

	/** When importing custom attribute or morphtarget as curve, do not import if it doesn't have any value other than zero. This is to avoid adding extra curves to evaluate */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations", meta = (EditCondition = "bImportCustomAttribute", DisplayName = "Do not import curves with only 0 values"))
	bool bDoNotImportCurveWithZero = false;

	/** If true, all previous node attributes imported as Animation Attributes will be deleted when doing a re-import. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations", meta = (DisplayName = "Delete existing Animation Attributes"))
	bool bDeleteExistingNonCurveCustomAttributes = false;

	/** If true, all previous node attributes imported as Animation Curves will be deleted when doing a re-import. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations", meta = (DisplayName = "Delete existing Animation Curves"))
	bool bDeleteExistingCustomAttributeCurves = false;

	/** If true, all previous morph target curves will be deleted when doing a re-import */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animations")
	bool bDeleteExistingMorphTargetCurves = false;

	/** Name of source animation that was imported, used to reimport correct animation from the translated source */
	UPROPERTY()
	FString SourceAnimationName;

	virtual void AdjustSettingsForContext(EInterchangePipelineContext ImportType, TObjectPtr<UObject> ReimportAsset) override;
	
protected:

	virtual void ExecutePreImportPipeline(UInterchangeBaseNodeContainer* InBaseNodeContainer, const TArray<UInterchangeSourceData*>& InSourceDatas) override;

	virtual bool CanExecuteOnAnyThread(EInterchangePipelineTask PipelineTask) override
	{
		return true;
	}

private:

	void CreateAnimationTrackSetFactoryNode(UInterchangeAnimationTrackSetNode& Node);

	void CreateAnimSequenceFactoryNode(UInterchangeSkeletalAnimationTrackNode& Node);

	UInterchangeBaseNodeContainer* BaseNodeContainer = nullptr;
	TArray<const UInterchangeSourceData*> SourceDatas;

};


