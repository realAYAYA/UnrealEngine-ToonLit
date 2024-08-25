// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "MLDeformerTrainingModel.h"
#include "MLDeformerGeomCacheTrainingModel.generated.h"

/**
 * The training model base class for geometry cache based models.
 * This class is our link to the Python training.
 */
UCLASS(Blueprintable)
class MLDEFORMERFRAMEWORKEDITOR_API UMLDeformerGeomCacheTrainingModel
	: public UMLDeformerTrainingModel
{
	GENERATED_BODY()

public:
	// UMLDeformerTrainingModel overrides.
	virtual void Init(UE::MLDeformer::FMLDeformerEditorModel* InEditorModel) override;
	virtual bool SampleNextFrame() override;
	// ~END UMLDeformerTrainingModel overrides;

protected:
	/**
	 * Find the next input animation to sample from.
	 * This is an index inside the training input animations list.
	 * This assumes the SampleAnimIndex member as starting point. The method does not modify this member directly, unless passed in as parameter.
	 * @param OutNextAnimIndex The next animation index to sample from when we take our next sample.
	 * @return Returns true when we found our next animation to sample. Returns false when we already sampled everything.
	 */
	virtual bool FindNextAnimToSample(int32& OutNextAnimIndex) const override;
};