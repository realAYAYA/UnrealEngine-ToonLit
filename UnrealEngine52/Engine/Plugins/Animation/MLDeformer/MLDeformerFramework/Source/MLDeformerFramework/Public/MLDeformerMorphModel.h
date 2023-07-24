// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerGeomCacheModel.h"
#include "MLDeformerMorphModelQualityLevel.h"
#include "MLDeformerMorphModel.generated.h"

struct FExternalMorphSet;
struct FExternalMorphSetWeights;
class USkinnedMeshComponent;

UCLASS()
class MLDEFORMERFRAMEWORK_API UMLDeformerMorphModel
	: public UMLDeformerGeomCacheModel
{
	GENERATED_BODY()

public:
	UMLDeformerMorphModel(const FObjectInitializer& ObjectInitializer);

	// UMLDeformerModel overrides.
	virtual bool IsNeuralNetworkOnGPU() const override				{ return false; }	// CPU based neural network.
	virtual bool DoesSupportQualityLevels() const override			{ return true; }	// We can disable morph targets based on the Deformer LOD float value.
	virtual void Serialize(FArchive& Archive) override;
	virtual UMLDeformerModelInstance* CreateModelInstance(UMLDeformerComponent* Component) override;
#if WITH_EDITOR
	virtual void UpdateMemoryUsage() override;
#endif
	// ~END UMLDeformerModel overrides.

	// UObject overrides.
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	// ~END UObject overrides.

	int32 GetNumMorphTargets() const								{ return NumMorphTargets; }
	uint64 GetCompressedMorphDataSizeInBytes() const				{ return CompressedMorphDataSizeInBytes; }
	uint64 GetUncompressedMorphDataSizeInBytes() const				{ return UncompressedMorphDataSizeInBytes; }

	float GetMorphDeltaZeroThreshold() const						{ return MorphDeltaZeroThreshold; }
	float GetMorphCompressionLevel() const							{ return MorphCompressionLevel; }
	float GetMaxMorphTargetErrorInUnits() const						{ return !MorphTargetErrorOrder.IsEmpty() ? MorphTargetErrors[MorphTargetErrorOrder[0]] : 0.0f; }
	bool GetIncludeMorphTargetNormals() const						{ return bIncludeNormals; }
	EMLDeformerMaskChannel GetMaskChannel() const					{ return MaskChannel; }
	bool GetInvertMaskChannel() const								{ return bInvertMaskChannel; }

	bool CanDynamicallyUpdateMorphTargets() const;

	void SetMorphDeltaZeroThreshold(float Threshold)				{ MorphDeltaZeroThreshold = Threshold; }
	void SetMorphCompressionlevel(float Tolerance)					{ MorphCompressionLevel = Tolerance; }
	void SetIncludeMorphTargetNormals(bool bInclude)				{ bIncludeNormals = bInclude; }
	void SetWeightMask(EMLDeformerMaskChannel Channel)				{ MaskChannel = Channel; }
	void SetInvertMaskChannel(bool bInvert)							{ bInvertMaskChannel = bInvert; }

	UE_DEPRECATED(5.2, "Please use GetMorphDeltaZeroThreshold instead.")
	float GetMorphTargetDeltaThreshold() const						{ return MorphDeltaZeroThreshold; }

	UE_DEPRECATED(5.2, "Please use SetMorphDeltaZeroThreshold instead.")
	void SetMorphTargetDeltaThreshold(float Threshold)				{ MorphDeltaZeroThreshold = Threshold; }

	UE_DEPRECATED(5.2, "Please use GetMorphCompressionLevel instead.")
	float GetMorphTargetErrorTolerance() const						{ return MorphCompressionLevel; }

	UE_DEPRECATED(5.2, "Please use SetMorphCompressionLevel instead.")
	void SetMorphTargetErrorTolerance(float Tolerance)				{ MorphCompressionLevel = Tolerance; }

	// Get property names.
	static FName GetMorphDeltaZeroThresholdPropertyName()			{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModel, MorphDeltaZeroThreshold); }
	static FName GetMorphCompressionLevelPropertyName()				{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModel, MorphCompressionLevel); }
	static FName GetIncludeMorphTargetNormalsPropertyName()			{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModel, bIncludeNormals); }
	static FName GetMaskChannelPropertyName()						{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModel, MaskChannel); }
	static FName GetInvertMaskChannelPropertyName()					{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModel, bInvertMaskChannel); }
	static FName GetNumMorphTargetsPropertyName()					{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModel, NumMorphTargets); }
	static FName GetCompressedMorphDataSizeInBytesPropertyName()	{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModel, CompressedMorphDataSizeInBytes); }
	static FName GetUncompressedMorphDataSizeInBytesPropertyName()	{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModel, UncompressedMorphDataSizeInBytes); }
	static FName GetQualityLevelsPropertyName()						{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModel, QualityLevels); }

	UE_DEPRECATED(5.2, "Please use GetMorphDeltaZeroThresholdPropertyName instead.")
	static FName GetMorphTargetDeltaThresholdPropertyName()			{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModel, MorphDeltaZeroThreshold); }

	UE_DEPRECATED(5.2, "Please use GetMorphCompressionLevelPropertyName instead.")
	static FName GetMorphTargetErrorTolerancePropertyName()			{ return GET_MEMBER_NAME_CHECKED(UMLDeformerMorphModel, MorphCompressionLevel); }

	/** Update the statistics properties of this model. */
	virtual void UpdateStatistics();

	/**
	 * Set the per vertex deltas, as a set of floats. Each vertex delta must have 3 floats.
	 * These deltas are used to generate compressed morph targets internally. You typically call this method from inside
	 * the python training script once your morph target deltas have been generated there.
	 * Concatenate all deltas into one buffer, so like this [morphdeltas_target0, morphdeltas_target1, ..., morphdeltas_targetN].
	 * The vertex ordering should be: [(x, y, z), (x, y, z), (x, y, z)].
	 * This is the same as SetMorphTargetDeltas, except that this takes an array of floats instead of vectors.
	 * @param Deltas The array of floats that contains the deltas. The number of items in the array must be equal to (NumMorphs * NumBaseMeshVerts * 3).
	 */
	UFUNCTION(BlueprintCallable, Category = "MLDeformerMorphModel")
	void SetMorphTargetDeltaFloats(const TArray<float>& Deltas);

	/**
	 * Set the morph target model deltas as an array of 3D vectors.
	 * These deltas are used to generate compressed morph targets internally. You typically call this method from inside
	 * the python training script once your morph target deltas have been generated there.
	 * Concatenate all deltas into one buffer, so like this [morphdeltas_target0, morphdeltas_target1, ..., morphdeltas_targetN].
	 * This is the same as SetMorphTargetDeltaFloats, except that it takes vectors instead of floats.
	 * @param Deltas The array of 3D vectors that contains the vertex deltas. The number of items in the array must be equal to (NumMorphs * NumBaseMeshVerts).
	 */
	UFUNCTION(BlueprintCallable, Category = "MLDeformerMorphModel")
	void SetMorphTargetDeltas(const TArray<FVector3f>& Deltas);

	/**
	 * Set the order of importance during LOD, for the morph targets.
	 * Basically this specifies the sorted order of the morph targets, based on the error they introduce when disabling them.
	 * @param MorphTargetOrder The array of morph target indices, starting with the most important morph target, and ending with the least important morph target's index.
	 * @param ErrorValues The error value for each morph target (not sorted), so index 0 contains the error value of the first morph target, which isn't necesarrily the one with highest error.
	 */
	UFUNCTION(BlueprintCallable, Category = "MLDeformerMorphModel")
	void SetMorphTargetsErrorOrder(const TArray<int32>& MorphTargetOrder, const TArray<float>& ErrorValues);

	/**
	 * Set the maximum weights for each morph target when we pass the training inputs into the trained neural network.
	 * After training we run the inputs through the trained network, and get the weights for each morph target.
	 * The array passed to this function must be the maximum of the absolute weight value over all training samples for each specific morph.
	 * We use this to scale the morph target deltas to get an estimate of the maximum length they will ever have, which we can in turn use
	 * to estimate how important a given morph target is.
	 * Keep in mind that these are the maximum of the absolute weights. As weights can be negative, we first take the absolute value of them.
	 * Training python scripts mostly will call this function to set the values.
	 * @param MaxWeights The maximum of the absolute weight values.
	 */
	UFUNCTION(BlueprintCallable, Category = "MLDeformerMorphModel")
	void SetMorphTargetsMaxWeights(const TArray<float>& MaxWeights);

	/**
	 * Get the estimated maximum weight values that the morph targets will ever have. It is not guaranteed that the weights will never be larger than these values though.
	 * These values are used to estimate the importance levels of the morph targets.
	 * @see SetMorphTargetsMaxWeights.
	 */
	TArrayView<const float> GetMorphTargetMaxWeights() const;

	/**
	 * Get the morph target delta vectors array.
	 * The layout of this array is [morphdeltas_target0, morphdeltas_target1, ..., morphdeltas_targetN].
	 * So the total number of items in the array returned equals (NumMorphTargets * NumBaseMeshVerts).
	 */
	const TArray<FVector3f>& GetMorphTargetDeltas() const		{ return MorphTargetDeltas; }

	/**
	 * Get the morph target set.
	 */
	TSharedPtr<FExternalMorphSet> GetMorphTargetSet() const		{ return MorphTargetSet; }

	/**
	 * Get the start index into the array of deltas (vectors3's), for a given morph target.
	 * This does not perform a bounds check to see if MorphTargetIndex is in a valid range, so be aware.
	 * @param MorphTargetIndex The morph target index.
	 * @return The start index, or INDEX_NONE in case there are no deltas.
	 */
	int32 GetMorphTargetDeltaStartIndex(int32 MorphTargetIndex) const;

	/**
	 * Get the morph target error values.
	 * This is an error value for each morph target, which indicates how much error you would introduce when disabling that specific morph target.
	 * It can be used as some sort of LOD system, where we disable morph targets that are marked with an error below a specific threshold.
	 * Keep in mind that the first morph target always contains the means, and is always active. So actually you have to +1 the index.
	 * @return The array view of the error values.
	 */
	TArrayView<const float> GetMorphTargetErrorValues() const;

	/**
	 * Get the array of morph target indices, sorted by error value, from largest to smallest error.
	 * So the first array value contains the index of the most important morph target and the last element contains the least important morph target.
	 * @return An array view of the morph target indices.
	 */
	TArrayView<const int32> GetMorphTargetErrorOrder() const;

	/**
	 * Get the quality levels.
	 * @return An array view of the quality levels.
	 */
	TArrayView<const FMLDeformerMorphModelQualityLevel> GetQualityLevels() const;

	/**
	 * Get the quality levels, with access to modify the quality levels.
	 * @return The array containing the quality levels.
	 */
	TArray<FMLDeformerMorphModelQualityLevel>& GetQualityLevelsArray();
	
	/**
	 * Get the estimated error that disabling a specific morph target would introduce.
	 * This error is represented in units, so in cm.
	 * @param MorphIndex The morph target index to get the error for.
	 * @return The estimated error value, in cm.
	 */
	float GetMorphTargetError(int32 MorphIndex) const;

	/**
	 * Set the estimated error that disabling a specific morph target would introduce.
	 * This error is represented in units, so in cm.
	 * @param MorphIndex The morph target index to get the error for.
	 * @param Error The estimated error value, in cm.
	 */
	void SetMorphTargetError(int32 MorphIndex, float Error);

	/** 
	 * Get the number of active morph targets for a given quality level.
	 * If no morph error data is available, or if no quality levels have been setup, the total number of morph targets is returned.
	 * If there is such data, the quality level will be clamped to be within a valid range.
	 * Finally, if the number of active morph targets specified by the quality level will be clamped within a valid range as well.
	 * So basically this method will always return a valid number of morph targets, whatever the input is.
	 * @param QualityLevel The quality level to get the number of active morph targets for.
	 */
	int32 GetNumActiveMorphs(int32 QualityLevel) const;

private:
	/** The compressed morph target data, ready for the GPU. */
	TSharedPtr<FExternalMorphSet> MorphTargetSet;

	/**
	 * The entire set of uncompressed morph target deltas, 3 per vertex, for each morph target, as one flattened buffer.
	 * So the size of this buffer is: (NumVertsPerMorphTarget * 3 * NumMorphTargets).
	 */
	UPROPERTY()
	TArray<FVector3f> MorphTargetDeltas;

	/** The number of morph targets. */
	UPROPERTY()
	int32 NumMorphTargets = 0;

	/** The compressed memory usage of the morph targets. This is approximately what this MLD asset will use in your packaged project. */
	UPROPERTY()
	uint64 CompressedMorphDataSizeInBytes = 0;

	/** The uncompressed memory usage of the morph targets. Please note that your packaged project will use the compressed amount of memory, and not the uncompresed amount of memory. */
	UPROPERTY()
	uint64 UncompressedMorphDataSizeInBytes = 0;

	/** 
	 * An array of integers where the first element is the most important morph target and the last element is the least important one.
	 * The number of elements in the array is equal to the total number of morph targets.
	 */
	UPROPERTY()
	TArray<int32> MorphTargetErrorOrder;

	/** An error value, for each morph target. */
	UPROPERTY()
	TArray<float> MorphTargetErrors;

	/** The maximum absolute weight values of the morph targets, during training. One value for each morph target. */
	UPROPERTY()
	TArray<float> MaxMorphWeights;

	/** 
	 * The list of quality levels, where the first item represents the highest quality and the last element the lowest quality level.
	 * The number in each quality level represents the number of active morph targets for that quality level.
	 * These numbers will be clamped internally to be within valid ranges in case they go beyond the amount of morphs that exist.
	 */
	UPROPERTY(EditAnywhere, Category = "Deformer Quality", meta = (ClampMin = "1"))
	TArray<FMLDeformerMorphModelQualityLevel> QualityLevels;

	/**
	 * Include vertex normals in the morph targets?
	 * The advantage of this can be that it is higher performance than recomputing the normals.
	 * The disadvantage is it can result in lower quality and uses more memory for the stored morph targets.
	 */
	UPROPERTY(EditAnywhere, Category = "Morph Targets")
	bool bIncludeNormals = false;

	/**
	 * Morph target delta values that are smaller than or equal to this threshold will be zeroed out.
	 * This essentially removes small deltas from morph targets, which will lower the memory usage at runtime, however when set too high it can also introduce visual artifacts.
	 * A value of 0 will result in the highest quality morph targets, at the cost of higher runtime memory usage.
	 */
	UPROPERTY(EditAnywhere, Category = "Morph Targets", DisplayName = "Delta Zero Threshold", meta = (ClampMin = "0.0", ClampMax = "1.0", ForceUnits="cm"))
	float MorphDeltaZeroThreshold = 0.0025f;

	/** 
	 * The morph target compression level. Higher values result in larger compression, but could result in visual artifacts.
	 * Most of the times this is a value between 20 and 200.
	 */
	UPROPERTY(EditAnywhere, Category = "Morph Targets", DisplayName = "Compression Level", meta = (ClampMin = "0.01", ClampMax = "1000"))
	float MorphCompressionLevel = 20.0f;

	/**
	 * The channel data that represents the delta mask multipliers.
	 * You can use this feather out influence of the ML Deformer in specific areas, such as neck line seams, where the head mesh connects with the body.
	 * The painted vertex color values will be like a weight multiplier on the ML deformer deltas applied to that vertex. You can invert the mask as well.
	 */
	UPROPERTY(EditAnywhere, Category = "Morph Targets")
	EMLDeformerMaskChannel MaskChannel = EMLDeformerMaskChannel::Disabled;

	/** 
	 * Enable this if you want to invert the mask channel values. For example if you painted the neck seam vertices in red, and you wish the vertices that got painted to NOT move, you have to invert the mask.
	 * On default you paint areas where the deformer should be active. If you enable the invert option, you paint areas where the deformer will not be active.
	 */
	UPROPERTY(EditAnywhere, Category = "Morph Targets", meta = (EditCondition = "MaskChannel != EMLDeformerMaskChannel::Disabled"))
	bool bInvertMaskChannel = false;
};
