// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MLDeformerModule.h"
#include "MLDeformerInputInfo.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * The neural network input information.
 * This contains arrays of names for things such as bones and curves.
 * Knowing what bones etc are used as inputs, and in what order, helps us feeding the data during inference.
 * It can also help us detect issues, for example when the character we apply the deformer to is missing any of those bones.
 */
UCLASS()
class MLDEFORMERFRAMEWORK_API UMLDeformerInputInfo
	: public UObject
{
	GENERATED_BODY()

public:
	virtual ~UMLDeformerInputInfo() = default;

	/**
	 * This method is executed post loading.
	 * On default it will initialize the FNames based on the bone and curve string based names.
	 */
	virtual void OnPostLoad();

	/**
	 * Check whether the current inputs are compatible with a given skeletal mesh.
	 * @param SkeletalMesh The skeletal mesh to check compatibility with. This may not be a nullptr.
	 * @return Returns true when we can safely apply the ML Deformer to a character using this skeletal mesh, otherwise false is returned.
	 * @note Use GenerateCompatibilityErrorString to get the error report.
	 */
	virtual bool IsCompatible(USkeletalMesh* SkeletalMesh) const;

	/**
	 * Clear all contents.
	 * This can clear the list of all bone and curves that are part of this input info.
	 */
	virtual void Reset();

	/**
	 * Get the compatibility error report.
	 * @param SkeletalMesh The skeletal mesh to check compatibility with.
	 * @return Returns an empty string in case there are no compatibility issues, otherwise it contains a string that describes the issue(s).
	 *         In case a nullptr is passed as SkeletalMesh parameter, an empty string is returned.
	 */
	virtual FString GenerateCompatibilityErrorString(USkeletalMesh* SkeletalMesh) const;

	/** 
	 * Update the FName arrays based on the name string arrays.
	 * This is automatically called on PostLoad of the UMLDeformerAsset.
	 */
	virtual void UpdateFNames();

	/**
	 * Check whether we have any training inputs or not.
	 * This happens when there are no bones or curves to use as inputs.
	 * @return Returns true when there are no bones or curves specified as inputs.
	 */
	virtual bool IsEmpty() const;

	/**
	 * Calculate how many inputs this input info generates for the neural network.
	 * A single bone would take 4 inputs, while a curve takes one input.
	 * @return THe number of input float values to the neural network.
	 */
	virtual int32 CalcNumNeuralNetInputs() const;

	/**
	 * Get the number of bones that we trained on.
	 * @return The number of bones.
	 */
	int32 GetNumBones() const;

	/**
	 * Get the bone name as a string, for a given bone we included during training.
	 * @param Index The bone index, which is a number in range of [0..GetNumBones()-1].
	 * @result The name of the bone.
	 */
	const FString& GetBoneNameString(int32 Index) const;

	/**
	 * Get the bone name as an FName, for a given bone we included during training.
	 * @param Index The bone index, which is a number in range of [0..GetNumBones()-1].
	 * @result The name of the bone.
	 */
	const FName GetBoneName(int32 Index) const;

	/**
	 * Get the number of curves that we trained on.
	 * @return The number of curves.
	 */
	int32 GetNumCurves() const;

	/**
	 * Get the list of bone names that should be included as training inputs.
	 * @return An array of strings, one for each bone name.
	 */
	TArray<FString>& GetBoneNameStrings();

	/**
	 * Get the list of curve names that should be included as training inputs.
	 * @return An array of strings, one for each curve name.
	 */
	TArray<FString>& GetCurveNameStrings();

	/**
	 * Get the list of bone names that should be included as training inputs.
	 * @return An array of FName objects, one for each bone.
	 */
	TArray<FName>& GetBoneNames();

	/**
	 * Get the list of curve names that should be included as training inputs.
	 * @return An array of FName objects, one for each curve.
	 */
	TArray<FName>& GetCurveNames();

	/**
	 * Get the curve name as a string, for a given bone we included during training.
	 * @param Index The curve index, which is a number in range of [0..GetNumCurves()-1].
	 * @result The name of the curve.
	 */
	const FString& GetCurveNameString(int32 Index) const;

	/**
	 * Get the curve name as an FName, for a given curve we included during training.
	 * @param Index The curve index, which is a number in range of [0..GetNumCurves()-1].
	 * @result The name of the curve.
	 */
	const FName GetCurveName(int32 Index) const;

	/** 
	 * Extract the curve values for all curves we're interested in.
	 * @param SkelMeshComponent The skeletal mesh component to sample from.
	 * @param OutValues The array to write the values to. This array will be reset/resized by this method.
	 */
	virtual void ExtractCurveValues(USkeletalMeshComponent* SkelMeshComponent, TArray<float>& OutValues) const;

	/**
	 * Extract bone space rotations, as a float array.
	 * The number of output rotations are NumBones * 6, where the array contains a set two columns of a 3x3 rotation matrix values.
	 * components of the bone's bone space (local space) rotation quaternion.
	 * @param SkelMeshComponent The skeletal mesh component to sample from.
	 * @param OutRotations The output rotation values. This array will be resized internally.
	 */
	virtual void ExtractBoneRotations(USkeletalMeshComponent* SkelMeshComponent, TArray<float>& OutRotations) const;

	/**
	 * Get the number of imported vertices in the base mesh, which is the linear skinned skeletal mesh.
	 * @return The number of imported vertices in the base mesh.
	 */
	int32 GetNumBaseMeshVertices() const;

	/**
	 * Set the number of vertices of our base mesh, which is the linear skinned skeletal mesh.
	 * The vertex count must be the same as in the DCC, so not the render mesh vertex count.
	 * @param NumVerts The number of vertices of our base mesh.
	 */
	void SetNumBaseVertices(int32 NumVerts);

	/**
	 * Get the number of imported vertices in the target mesh, which is our training target/ground truth mesh.
	 * The number of vertices is the vertex count as in the DCC.
	 * @return The number of imported vertices in the target mesh.
	 */
	int32 GetNumTargetMeshVertices() const;

	/**
	 * Set the number of target mesh vertices. The target mesh is our training target/ground truth mesh.
	 * The vertex count must be the same as in the DCC, so not the render mesh vertex count.
	 * @param NumVerts The number of vertices in the target mesh.
	 */
	void SetNumTargetVertices(int32 NumVerts);

protected:
	/** 
	 * The name of each bone. The inputs to the network are in the order of this array.
	 * So if the array contains ["Root", "Child1", "Child2"] then the first bone transforms that we 
	 * input to the neural network is the transform for "Root", followed by "Child1", followed by "Child2".
	 */
	UPROPERTY()
	TArray<FString> BoneNameStrings;

	/** The same as the BoneNames member, but stored as pre-created FName objects. These are not serialized. */
	UPROPERTY(Transient)
	TArray<FName> BoneNames;

	/**
	 * The name of each curve. The inputs to the network are in the order of this array.
	 * So if the array contains ["Smile", "LeftEyeClosed", "RightEyeClosed"] then the first curve that we
	 * input to the neural network is the one for "Smile", followed by "LeftEyeClosed", followed by "RightEyeClosed".
	 */
	UPROPERTY()
	TArray<FString> CurveNameStrings;

	/** The same as the CurveNames member, but stored as pre-created FName objects. These are not serialized. */
	UPROPERTY(Transient)
	TArray<FName> CurveNames;

	/** Number of imported base mesh vertices, so not render vertices. */
	UPROPERTY()
	int32 NumBaseMeshVertices = 0;

	/** Number of imported target mesh vertices, so not render vertices. */
	UPROPERTY()
	int32 NumTargetMeshVertices = 0;
};
