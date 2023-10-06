// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Engine/AssetUserData.h"
#include "Misc/Guid.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UObjectGlobals.h"
#include "Animation/AnimTypes.h"
#include "Curves/RealCurve.h"
#include "LevelSequenceAnimSequenceLink.generated.h"

class UAnimSequence;
class UObject;

/** Link To Anim Sequence that we are linked too.*/
USTRUCT(BlueprintType)
struct FLevelSequenceAnimSequenceLinkItem
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = Property)
	FGuid SkelTrackGuid;

	UPROPERTY(BlueprintReadWrite, Category = Property)
	FSoftObjectPath PathToAnimSequence;

	//From Editor Only UAnimSeqExportOption we cache this since we can re-import dynamically
	UPROPERTY(BlueprintReadWrite, Category = Property)
	bool bExportTransforms = true;
	UPROPERTY(BlueprintReadWrite, Category = Property)
	bool bExportMorphTargets = true;
	UPROPERTY(BlueprintReadWrite, Category = Property)
	bool bExportAttributeCurves = true;
	UPROPERTY(BlueprintReadWrite, Category = Property)
	bool bExportMaterialCurves = true;
	UPROPERTY(BlueprintReadWrite, Category = Property);
	EAnimInterpolationType Interpolation = EAnimInterpolationType::Linear;
	UPROPERTY(BlueprintReadWrite, Category = Property);
	TEnumAsByte<ERichCurveInterpMode> CurveInterpolation = ERichCurveInterpMode::RCIM_Linear;	
	UPROPERTY(BlueprintReadWrite, Category = Property)
	bool bRecordInWorldSpace = false;
	UPROPERTY(BlueprintReadWrite, Category = Property)
	bool bEvaluateAllSkeletalMeshComponents = true;

	LEVELSEQUENCE_API void SetAnimSequence(UAnimSequence* InAnimSequence);
	LEVELSEQUENCE_API UAnimSequence* ResolveAnimSequence();

};

/** Link To Set of Anim Sequences that we may be linked to.*/
UCLASS(BlueprintType, MinimalAPI)
class ULevelSequenceAnimSequenceLink : public UAssetUserData
{
	GENERATED_UCLASS_BODY()

public:
	virtual bool IsEditorOnly() const override { return true; }

	UPROPERTY(BlueprintReadWrite, Category = Links)
	TArray< FLevelSequenceAnimSequenceLinkItem> AnimSequenceLinks;
};
