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
#if WITH_EDITORONLY_DATA
	void SetMorphTargetNumber(int32 MorphIndexToPreview)	{ MorphTargetNumber = MorphIndexToPreview; }
	int32 GetMorphTargetNumber() const						{ return MorphTargetNumber; }
	float GetMorphTargetDeltaThreshold() const				{ return MorphTargetDeltaThreshold; }
	bool GetDrawMorphTargets() const						{ return bDrawMorphTargets; }

	// Get property names.
	static FName GetMorphTargetNumberPropertyName()			{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModelVizSettings, MorphTargetNumber); }
	static FName GetMorphTargetDeltaThresholdPropertyName() { return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModelVizSettings, MorphTargetDeltaThreshold); }
	static FName GetDrawMorphTargetsPropertyName()			{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModelVizSettings, bDrawMorphTargets); }

protected:
	/**
	 * The morph target to visualize. The first one always being the means, so not a sparse target.
	 * This only can be used after you trained, in the same editor session directly after training.
	 */
	UPROPERTY(EditAnywhere, Category = "Live Settings", meta = (ClampMin = "0"))
	int32 MorphTargetNumber = 0;

	/**
	 * The morph target delta threshold. This is a preview of what deltas would be included in the selected morph target
	 * when using a delta threshold during training that is equal to this value.
	 * This only can be used after you trained, in the same editor session directly after training.
	 */
	UPROPERTY(EditAnywhere, Category = "Live Settings", meta = (ClampMin = "0.001", ClampMax = "1.0", ForceUnits="cm"))
	float MorphTargetDeltaThreshold = 0.01f;

	/**
	 * Specify whether we want to debug draw the morph targets.
	 * Enabling this can help you see what the sparsity of the generated morph targets is.
	 * It also takes the morph target delta threshold into account, so you can preview how that effects the sparsity of the morphs.
	 * This only can be used after you trained, in the same editor session directly after training.
	 */
	UPROPERTY(EditAnywhere, Category = "Live Settings")
	bool bDrawMorphTargets = false;
#endif // WITH_EDITORONLY_DATA
};
