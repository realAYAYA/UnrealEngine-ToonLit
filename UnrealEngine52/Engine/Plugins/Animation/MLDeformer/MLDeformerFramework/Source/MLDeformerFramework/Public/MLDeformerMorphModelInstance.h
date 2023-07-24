// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MLDeformerModelInstance.h"
#include "MLDeformerMorphModelInstance.generated.h"

struct FExternalMorphSetWeights;
class USkeletalMeshComponent;

/**
 * The model instance for the UMLDeformerMorphModel.
 * This instance will assume the neural network outputs a set of weights, one for each morph target.
 * The weights of the morph targets in the external morph target set related to the ID of the model will
 * be set to the weights that the neural network outputs.
 * The first morph target contains the means, which need to always be added to the results. Therefore the 
 * weight of the first morph target will always be forced to a value of 1.
 */
UCLASS()
class MLDEFORMERFRAMEWORK_API UMLDeformerMorphModelInstance
	: public UMLDeformerModelInstance
{
	GENERATED_BODY()

public:
	// UMLDeformerModelInstance overrides.
	virtual void Init(USkeletalMeshComponent* SkelMeshComponent) override;
	virtual void PostMLDeformerComponentInit() override;
	virtual void Release() override;
	virtual void Tick(float DeltaTime, float ModelWeight) override;
	virtual void HandleZeroModelWeight() override;
	virtual bool IsValidForDataProvider() const override;
	// ~END UMLDeformerModelInstance overrides.

	int32 GetExternalMorphSetID() const;

	/**
	 * Find the external morph target weight data for this model instance.
	 * @param LOD The LOD level to get the weight data for.
	 * @return A pointer to the weight data, or a nullptr in case it cannot be found.
	 */
	FExternalMorphSetWeights* FindWeightData(int32 LOD) const;

protected:
	/** The next free morph target set ID. This is used to generate unique ID's for each morph model. */
	static TAtomic<int32> NextFreeMorphSetID;

	/** The ID of the external morph target set for this instance. This gets initialized during Init. */
	int32 ExternalMorphSetID = -1;

	/** The morph target weights we are blending towards. */
	TArray<float> StartMorphWeights;

	/** The current lerp alpha (between 0 and 1). */
	float MorphLerpAlpha = 1.0f;

	/** The quality level in the previous tick. */
	int32 LastQualityLevel = -1;
};
