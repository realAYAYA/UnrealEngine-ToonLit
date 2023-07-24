// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "MLDeformerMorphModel.h"
#include "GeometryCache.h"
#include "NeuralMorphTypes.h"
#include "NeuralMorphModel.generated.h"

class USkeletalMesh;
class UGeometryCache;
class UAnimSequence;
class UNeuralMorphNetwork;
class UMLDeformerAsset;
class UMLDeformerModelInstance;
class USkeleton;
struct FExternalMorphSet;

/** 
 * Specify whether we want to use ISPC if available.
 * You can comment out this line, to force disable the use of ISPC in this plugin.
 */
#define NEURALMORPHMODEL_USE_ISPC INTEL_ISPC
#if !defined(NEURALMORPHMODEL_USE_ISPC)
	#define NEURALMORPHMODEL_USE_ISPC 0
#endif

// Declare our log category.
NEURALMORPHMODEL_API DECLARE_LOG_CATEGORY_EXTERN(LogNeuralMorphModel, Log, All);

/**
 * The neural morph model.
 * This generates a set of highly compressed morph targets to approximate a target deformation based on bone rotations and/or curve inputs.
 * During inference, the neural network inside this model runs on the CPU and outputs the morph target weights for the morph targets it generated during training.
 * Groups of bones and curves can also be defined. Those groups will generate a set of morph targets together as well. This can help when shapes depend on multiple inputs.
 * An external morph target set is generated during training and serialized inside the ML Deformer asset that contains this model.
 * When the ML Deformer component initializes the morph target set is registered. Most of the heavy lifting is done by the UMLDeformerMorphModel class.
 * The neural morph model has two modes: local and global. See the ENeuralMorphMode for a description of the two.
 * @see ENeuralMorphMode
 */
UCLASS()
class NEURALMORPHMODEL_API UNeuralMorphModel 
	: public UMLDeformerMorphModel
{
	GENERATED_BODY()

public:
	UNeuralMorphModel(const FObjectInitializer& ObjectInitializer);

	// UObject overrides.
	virtual void Serialize(FArchive& Archive) override;
	// ~END UObject overrides.

	// UMLDeformerModel overrides.
	virtual FString GetDisplayName() const override			{ return "Neural Morph Model"; }
	virtual UMLDeformerModelInstance* CreateModelInstance(UMLDeformerComponent* Component) override;
	virtual UMLDeformerInputInfo* CreateInputInfo();
	// ~END UMLDeformerModel overrides.

	const TArray<FNeuralMorphBoneGroup>& GetBoneGroups() const		{ return BoneGroups; }
	const TArray<FNeuralMorphCurveGroup>& GetCurveGroups() const	{ return CurveGroups; }
	UNeuralMorphNetwork* GetNeuralMorphNetwork() const		{ return NeuralMorphNetwork.Get(); }
	void SetNeuralMorphNetwork(UNeuralMorphNetwork* Net)	{ NeuralMorphNetwork = Net; }
	ENeuralMorphMode GetModelMode() const					{ return Mode; }
	int32 GetLocalNumHiddenLayers() const					{ return LocalNumHiddenLayers; }
	int32 GetLocalNumNeuronsPerLayer() const				{ return LocalNumNeuronsPerLayer; }
	int32 GetGlobalNumHiddenLayers() const					{ return GlobalNumHiddenLayers; }
	int32 GetGlobalNumNeuronsPerLayer() const				{ return GlobalNumNeuronsPerLayer; }
	int32 GetNumIterations() const							{ return NumIterations; }
	int32 GetBatchSize() const								{ return BatchSize; }
	float GetLearningRate() const							{ return LearningRate; }
	float GetLearningRateDecay() const						{ return LearningRateDecay; }
	float GetRegularizationFactor() const					{ return RegularizationFactor; }

public:
	/**
	 * The set of bones that are grouped together and generate morph targets together as a whole.
	 * This can be used in case multiple bones are correlated to each other and work together to produce given shapes.
	 * Groups are only used when the model is in Local mode.
	 */
	UPROPERTY(EditAnywhere, Category = "Inputs and Output")
	TArray<FNeuralMorphBoneGroup> BoneGroups;

	/**
	 * The set of curves that are grouped together and generate morph targets together as a whole.
	 * This can be used in case multiple curves are correlated to each other and work together to produce given shapes.
	 * Groups are only used when the model is in Local mode.
	 */
	UPROPERTY(EditAnywhere, Category = "Inputs and Output")
	TArray<FNeuralMorphCurveGroup> CurveGroups;

	/**
	 * The mode that the neural network will operate in. 
	 * Local mode means there is one tiny network per bone, while global mode has one network for all bones together.
	 * The advantage of local mode is that it has higher performance, while global mode might result in better deformations.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Settings")
	ENeuralMorphMode Mode = ENeuralMorphMode::Local;

	/** 
	 * The number of morph targets to generate per bone, curve or group.
	 * Higher numbers result in better approximation of the target deformation, but also result in a higher memory footprint and slower performance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "Num Morphs Per Bone/Curve/Group", Category = "Training Settings", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 LocalNumMorphTargetsPerBone = 6;

	/** 
	 * The number of morph targets to generate in total. 
	 * Higher numbers result in better approximation of the target deformation, but also result in a higher memory footprint and slower performance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, DisplayName = "Num Morph Targets", Category = "Training Settings", meta = (ClampMin = "1", ClampMax = "1000"))
	int32 GlobalNumMorphTargets = 128;

	/** 
	 * The number of iterations to train the model for. 
	 * If you are quickly iterating then around 1000 to 3000 iterations should be enough.
	 * If you want to generate final assets you might want to use a higher number of iterations, like 10k to 100k.
	 * Once the loss doesn't go down anymore, you know that more iterations most likely won't help much.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training Settings", meta = (ClampMin = "1", ClampMax = "1000000"))
	int32 NumIterations = 5000;

	/** 
	 * The number of hidden layers that the neural network model will have.
	 * Higher numbers will slow down performance but can deal with more complex deformations. 
	 * For the local model you most likely want to stick with a value of one or two.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, DisplayName = "Num Hidden Layers", Category = "Training Settings", meta = (ClampMin = "0", ClampMax = "5"))
	int32 LocalNumHiddenLayers = 1;

	/** 
	 * The number of units/neurons per hidden layer. 
	 * Higher numbers will slow down performance but allow for more complex mesh deformations. 
	 * For the local mode you probably want to keep this around the same value as the number of morph targets per bone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, DisplayName = "Num Neurons Per Layer", Category = "Training Settings", meta = (ClampMin = "1", ClampMax = "4096"))
	int32 LocalNumNeuronsPerLayer = 6;

	/** The number of hidden layers that the neural network model will have.\nHigher numbers will slow down performance but can deal with more complex deformations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, DisplayName = "Num Hidden Layers", Category = "Training Settings", meta = (ClampMin = "0", ClampMax = "6"))
	int32 GlobalNumHiddenLayers = 2;

	/** The number of units/neurons per hidden layer. Higher numbers will slow down performance but allow for more complex mesh deformations. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, DisplayName = "Num Neurons Per Layer", Category = "Training Settings", meta = (ClampMin = "1", ClampMax = "4096"))
	int32 GlobalNumNeuronsPerLayer = 128;

	/** The number of frames per batch when training the model. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Training Settings", meta = (ClampMin = "1"))
	int32 BatchSize = 128;

	/** The learning rate used during the model training. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Training Settings", meta = (ClampMin = "0.000001", ClampMax = "1.0"))
	float LearningRate = 0.001f;

	/** 
	 * The learning rate decay rate. If this is set to 1, the learning rate will not change. 
	 * This is a multiplication factor. Every 1000 iterations, the new learning rate will be equal to
	 * CurrentLearningRate * LearningRateDecay. This allows us to take slightly larger steps in the first iterations, and slowly take smaller steps.
	 * Generally you want this to be between 0.9 and 1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Training Settings", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float LearningRateDecay = 0.98f;

	/** 
	 * The regularization factor. Higher values can help generate more sparse morph targets, but can also lead to visual artifacts. 
	 * A value of 0 disables the regularization, and gives the highest quality, at the cost of higher runtime memory usage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, AdvancedDisplay, Category = "Training Settings", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float RegularizationFactor = 1.0f;

	/**
	 * The neural morph model network.
	 * This is a specialized network used for inference, for performance reasons.
	 */
	UPROPERTY()
	TObjectPtr<UNeuralMorphNetwork> NeuralMorphNetwork;
};
