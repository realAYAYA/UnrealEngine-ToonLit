// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
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

	// UObject overrides.
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	// ~END UObject overrides.

	/**
	 * This method is executed post loading of the ML Deformer asset.
	 */
	virtual void OnPostLoad();

	/**
	 * Check whether the current inputs are compatible with a given skeletal mesh.
	 * @param InSkeletalMesh The skeletal mesh to check compatibility with. This may not be a nullptr.
	 * @return Returns true when we can safely apply the ML Deformer to a character using this skeletal mesh, otherwise false is returned.
	 * @note Use GenerateCompatibilityErrorString to get the error report.
	 */
	virtual bool IsCompatible(USkeletalMesh* InSkeletalMesh) const;

	/**
	 * Clear all contents.
	 * This can clear the list of all bone and curves that are part of this input info.
	 */
	virtual void Reset();

	/**
	 * Get the compatibility error report.
	 * @param InSkeletalMesh The skeletal mesh to check compatibility with.
	 * @return Returns an empty string in case there are no compatibility issues, otherwise it contains a string that describes the issue(s).
	 *         In case a nullptr is passed as SkeletalMesh parameter, an empty string is returned.
	 */
	virtual FString GenerateCompatibilityErrorString(USkeletalMesh* InSkeletalMesh) const;

	/**
	 * Check whether we have any training inputs or not.
	 * This happens when there are no bones or curves to use as inputs.
	 * @return Returns true when there are no bones or curves specified as inputs.
	 */
	virtual bool IsEmpty() const;

	/**
	 * Calculate how many inputs this input info generates for the neural network.
	 * A single bone on default takes 6 inputs.
	 * @return THe number of input float values to the neural network.
	 */
	UE_DEPRECATED(5.2, "Please use the CalcNumNeuralNetInputs that takes parameters instead.")
	virtual int32 CalcNumNeuralNetInputs() const;

	/**
	 * Calculate how many inputs this input info generates for the neural network.
	 * @param NumFloatsPerBone The number of floats used per bone.
	 * @param NumFloatsPerCurve The number of floats per curve. This can be used when padding is used on curve values.
	 * @return THe number of input float values to the neural network.
	 */
	virtual int32 CalcNumNeuralNetInputs(int32 NumFloatsPerBone, int32 NumFloatsPerCurve) const;

	/**
	 * Copy members from the other class.
	 * @param Other The other class to copy over members from.
	 **/
	virtual void CopyMembersFrom(UMLDeformerInputInfo* Other);

	/**
	 * Get the number of bones that we trained on.
	 * @return The number of bones.
	 */
	int32 GetNumBones() const;

	/**
	 * Get the bone name as an FName, for a given bone we included during training.
	 * @param Index The bone index, which is a number in range of [0..GetNumBones()-1].
	 * @result The name of the bone.
	 */
	FName GetBoneName(int32 Index) const;

	/**
	 * Get the number of curves that we trained on.
	 * @return The number of curves.
	 */
	int32 GetNumCurves() const;

	/**
	 * Get the list of bone names that should be included as training inputs.
	 * @return An array of FName objects, one for each bone.
	 */
	TArray<FName>& GetBoneNames();
	const TArray<FName>& GetBoneNames() const;

	/**
	 * Get the list of curve names that should be included as training inputs.
	 * @return An array of FName objects, one for each curve.
	 */
	TArray<FName>& GetCurveNames();
	const TArray<FName>& GetCurveNames() const;

	/**
	 * Get the curve name as an FName, for a given curve we included during training.
	 * @param Index The curve index, which is a number in range of [0..GetNumCurves()-1].
	 * @result The name of the curve.
	 */
	FName GetCurveName(int32 Index) const;

#if WITH_EDITORONLY_DATA
	UE_DEPRECATED(5.2, "Please use GetBoneNames instead.")
	TArray<FString>& GetBoneNameStrings()						{ return BoneNameStrings_DEPRECATED; }

	UE_DEPRECATED(5.2, "Please use GetCurveNames instead.")
	TArray<FString>& GetCurveNameStrings()						{ return CurveNameStrings_DEPRECATED; }

	UE_DEPRECATED(5.2, "Please use GetBoneName instead.")
	const FString& GetBoneNameString(int32 Index) const			{ return BoneNameStrings_DEPRECATED[Index]; }

	UE_DEPRECATED(5.2, "Please use GetCurveName instead.")
	const FString& GetCurveNameString(int32 Index) const		{ return CurveNameStrings_DEPRECATED[Index]; }
#endif

	/** 
	 * Extract the curve values for all curves we're interested in. Assume one float per curve.
	 * @param SkelMeshComponent The skeletal mesh component to sample from.
	 * @param OutValues The array to write the values to. This array will be reset/resized by this method.
	 */
	UE_DEPRECATED(5.2, "Please use the ExtractCurveValues method that takes the NumFloatsPerCurve parameter.")
	virtual void ExtractCurveValues(USkeletalMeshComponent* SkelMeshComponent, TArray<float>& OutValues) const;

	/** 
	 * Extract the curve values for all curves we're interested in.
	 * @param SkelMeshComponent The skeletal mesh component to sample from.
	 * @param OutValues The array to write the values to. This array will be reset/resized by this method.
	 * @param NumFloatsPerCurve The number of floats per curve. If larger than 1, the remaining floats (after the first one), will be set to 0.
	 *        So if this is set to 4, you get an array like "0.75, 0, 0, 0", where 0.75 would represent the actual curve value.
	 */
	virtual void ExtractCurveValues(USkeletalMeshComponent* SkelMeshComponent, TArray<float>& OutValues, int32 NumFloatsPerCurve) const;

	/**
	 * Batch convert a set of transform rotation quaternions into two column vectors.
	 * 
	 */
	static void RotationToTwoVectorsAsSixFloats(TArrayView<FTransform> Transforms, float* OutputBuffer);

	/**
	 * Convert a rotation quaternion to two basis vectors, each represented as 3 floats, so 6 floats in total.
	 * @param Rotation The rotation quaternion to convert into the six float values.
	 * @param SixFloatsOutputBuffer The float buffer that we will write the 6 floats to. Make sure this buffer is at least 6 floats large.
	 */
	static void RotationToTwoVectorsAsSixFloats(const FQuat& Rotation, float* SixFloatsOutputBuffer);

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

	/** 
	 * Get the path to the skeletal mesh that this deformer was trained on.
	 * @return The path to the skeletal mesh we used during training.
	 */
	const FSoftObjectPath& GetSkeletalMesh() const;

	/**
	 * Set skeletal mesh that we trained on.
	 * @param InSkeletalMesh A pointer to the skeletal mesh.
	 */
	void SetSkeletalMesh(USkeletalMesh* InSkeletalMesh);

	UE_DEPRECATED(5.2, "This method will be removed soon. Please only use the BoneNames and CurveNames and not their string versions.")
	void UpdateNameStrings();

	UE_DEPRECATED(5.2, "This method will be removed soon. It shouldn't be needed anymore.")
	void UpdateFNames();

protected:
	/**
	 * The path to the skeletal mesh that this model was trained on.
	 */
	UPROPERTY()
	FSoftObjectPath SkeletalMesh;

#if WITH_EDITORONLY_DATA
	/** The list of bone names, but as string. This is deprecated since UE 5.2. */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use BoneNames instead."))
	TArray<FString> BoneNameStrings_DEPRECATED;

	/** The list of curve names, but as string. This is deprecated since UE 5.2. */
	UPROPERTY(meta = (DeprecatedProperty, DeprecationMessage = "Use CurveNames instead."))
	TArray<FString> CurveNameStrings_DEPRECATED;
#endif

	/** 
	 * The name of each bone. The inputs to the network are in the order of this array.
	 * So if the array contains ["Root", "Child1", "Child2"] then the first bone transforms that we 
	 * input to the neural network is the transform for "Root", followed by "Child1", followed by "Child2".
	 */
	UPROPERTY()
	TArray<FName> BoneNames;

	/**
	 * The name of each curve. The inputs to the network are in the order of this array.
	 * So if the array contains ["Smile", "LeftEyeClosed", "RightEyeClosed"] then the first curve that we
	 * input to the neural network is the one for "Smile", followed by "LeftEyeClosed", followed by "RightEyeClosed".
	 */
	UPROPERTY()
	TArray<FName> CurveNames;

	/** Number of imported base mesh vertices, so not render vertices. */
	UPROPERTY(AssetRegistrySearchable)
	int32 NumBaseMeshVertices = 0;

	/** Number of imported target mesh vertices, so not render vertices. */
	UPROPERTY()
	int32 NumTargetMeshVertices = 0;
};
