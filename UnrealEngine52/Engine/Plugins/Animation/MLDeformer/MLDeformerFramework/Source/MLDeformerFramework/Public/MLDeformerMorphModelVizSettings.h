// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerGeomCacheVizSettings.h"
#include "MLDeformerMorphModelVizSettings.generated.h"

/**
 * The vizualization settings specific to the UMLDeformerMorphModel class, or inherited classes.
 */
UCLASS()
class MLDEFORMERFRAMEWORK_API UMLDeformerMorphModelVizSettings
	: public UMLDeformerGeomCacheVizSettings
{
	GENERATED_BODY()

public:
	void SetMorphTargetNumber(int32 MorphIndexToPreview)	{ MorphTargetNumber = MorphIndexToPreview; }
	void SetDrawMorphTargets(bool bDraw)					{ bDrawMorphTargets = bDraw; }

	UE_DEPRECATED(5.2, "This method will be removed, please stop using it.")
	float GetMorphTargetDeltaThreshold() const				{ return MorphTargetDeltaThreshold_DEPRECATED; }

	int32 GetMorphTargetNumber() const						{ return MorphTargetNumber; }
	bool GetDrawMorphTargets() const						{ return bDrawMorphTargets; }

	// Get property names.
	static FName GetMorphTargetNumberPropertyName()			{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModelVizSettings, MorphTargetNumber); }
	static FName GetDrawMorphTargetsPropertyName()			{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModelVizSettings, bDrawMorphTargets); }

	UE_DEPRECATED(5.2, "This method will be removed, as well as the MorphTargetDeltaThreshold member, please stop using it.")
	static FName GetMorphTargetDeltaThresholdPropertyName() { return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModelVizSettings, MorphTargetDeltaThreshold_DEPRECATED); }

protected:
	/**
	 * The morph target delta threshold. This is a preview of what deltas would be included in the selected morph target
	 * when using a delta threshold during training that is equal to this value.
	 * This only can be used after you trained, in the same editor session directly after training.
	 */
	UPROPERTY()
	float MorphTargetDeltaThreshold_DEPRECATED = 0.01f;

	/**
	 * The morph target to visualize. The first one always being the means, so not a sparse target.
	 * This only can be used after you trained, in the same editor session directly after training.
	 */
	UPROPERTY(EditAnywhere, Category = "Live Settings", meta = (ClampMin = "0"))
	int32 MorphTargetNumber = 0;

	/**
	 * Specify whether we want to debug draw the morph targets.
	 * Enabling this can help you see what the sparsity of the generated morph targets is.
	 * It also takes the morph target delta threshold into account, so you can preview how that effects the sparsity of the morphs.
	 * This only can be used after you trained, in the same editor session directly after training.
	 */
	UPROPERTY(EditAnywhere, Category = "Live Settings")
	bool bDrawMorphTargets = false;
};
