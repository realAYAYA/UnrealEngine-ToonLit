// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "MeshAttributeArray.h"
#include "MeshAttributes.h"
#include "SkinWeightsAttributesRef.h"
#include "StaticMeshAttributes.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"
#include "MeshTypes.h"

// Forward declarations
template <typename AttributeType> class TArrayAttribute;
struct FMeshDescription;

// Add any skeletalmesh specific attributes here
namespace MeshAttribute
{
 	namespace Vertex
 	{
		extern SKELETALMESHDESCRIPTION_API const FName SkinWeights;
 	}

	namespace Bone
	{
		extern SKELETALMESHDESCRIPTION_API const FName Name;
		extern SKELETALMESHDESCRIPTION_API const FName ParentIndex;
		extern SKELETALMESHDESCRIPTION_API const FName Pose;
		extern SKELETALMESHDESCRIPTION_API const FName Color;
	}
}


class FSkeletalMeshAttributesShared
{
public:
	using FBoneArray = TMeshElementContainer<FBoneID>;
	
	using FBoneNameAttributesRef = TMeshAttributesRef<FBoneID, FName>;
	using FBoneNameAttributesConstRef = TMeshAttributesConstRef<FBoneID, FName>;
	
	using FBoneParentIndexAttributesRef = TMeshAttributesRef<FBoneID, int32>;
	using FBoneParentIndexAttributesConstRef = TMeshAttributesConstRef<FBoneID, int32>;
	
	using FBonePoseAttributesRef = TMeshAttributesRef<FBoneID, FTransform>;
	using FBonePoseAttributesConstRef = TMeshAttributesConstRef<FBoneID, FTransform>;
	
	using FBoneColorAttributesRef = TMeshAttributesRef<FBoneID, FVector4f>;
	using FBoneColorAttributesConstRef = TMeshAttributesConstRef<FBoneID, FVector4f>;

	/** 
	 * Name of the mesh element type representing bones.
	 * 
	 * @note this is different from the MeshAttribute::Bone::Name attribute. This is a name of the element that is 
	 * added to the  mesh description to represent Bones (similar to the Vertex/Polygons/Edges elements). 
	 * MeshAttribute::Bone::Name is just one of the attributes of the Bones element.
	 */
	static SKELETALMESHDESCRIPTION_API FName BonesElementName;

	// The name of the default skin weight profile.
	static SKELETALMESHDESCRIPTION_API FName DefaultSkinWeightProfileName;
	
	SKELETALMESHDESCRIPTION_API FSkeletalMeshAttributesShared(const FMeshDescription& InMeshDescription);

	//
	// Skin Weights Methods
	//

	/// Returns the list of all registered skin weight profile names on this mesh.
	SKELETALMESHDESCRIPTION_API TArray<FName> GetSkinWeightProfileNames() const;
	
	/// Returns \c true if the given identifier is a valid profile name. If the name is empty, or matches the default profile,
	/// then the profile name is considered invalid. 
	static SKELETALMESHDESCRIPTION_API bool IsValidSkinWeightProfileName(const FName& InProfileName);

	/// Helper function that indicates whether an attribute name represents a skin weight attribute.
	static SKELETALMESHDESCRIPTION_API bool IsSkinWeightAttribute(const FName& InAttributeName);

	/// Returns a skin profile name from the attribute name, if the attribute name is a valid skin weights
	/// attribute.
	static SKELETALMESHDESCRIPTION_API FName GetProfileNameFromAttribute(const FName& InAttributeName);

	SKELETALMESHDESCRIPTION_API FSkinWeightsVertexAttributesConstRef GetVertexSkinWeights(const FName& InProfileName = NAME_None) const;
	
	SKELETALMESHDESCRIPTION_API FSkinWeightsVertexAttributesConstRef GetVertexSkinWeightsFromAttributeName(const FName& InAttributeName = NAME_None) const;


	//
	// Bones Methods
	//

	SKELETALMESHDESCRIPTION_API bool HasBoneColorAttribute() const;

	SKELETALMESHDESCRIPTION_API bool HasBoneNameAttribute() const;

	SKELETALMESHDESCRIPTION_API bool HasBonePoseAttribute() const;

	SKELETALMESHDESCRIPTION_API bool HasBoneParentIndexAttribute() const;

	SKELETALMESHDESCRIPTION_API const FBoneArray& Bones() const;

	SKELETALMESHDESCRIPTION_API const TAttributesSet<FBoneID>& BoneAttributes() const;

	/**  @return true, if bone element was added to the MeshDescription. */
	bool HasBones() const
	{
		return BoneElementsShared != nullptr;
	}

	/**  @return the number of the bones. 0 if bone element does not exist in the MeshDescripion. */
	SKELETALMESHDESCRIPTION_API int32 GetNumBones() const;

	/** @return true, if the passed bone ID is valid */
	SKELETALMESHDESCRIPTION_API bool IsBoneValid(const FBoneID BoneID) const;

	SKELETALMESHDESCRIPTION_API FBoneNameAttributesConstRef GetBoneNames() const;

	SKELETALMESHDESCRIPTION_API FBoneParentIndexAttributesConstRef GetBoneParentIndices() const;

	SKELETALMESHDESCRIPTION_API FBonePoseAttributesConstRef GetBonePoses() const;

	SKELETALMESHDESCRIPTION_API FBoneColorAttributesConstRef GetBoneColors() const;

protected:
	/// Construct a name for a skin weight attribute with the given skin weight profile name.
	/// Each mesh description can hold different skin weight profiles, although the default
	/// is always present.
	static SKELETALMESHDESCRIPTION_API FName CreateSkinWeightAttributeName(const FName& InProfileName);

private:
	const FMeshElementChannels* BoneElementsShared = nullptr;

	const FMeshDescription& MeshDescriptionShared;
};


class FSkeletalMeshAttributes :
	public FStaticMeshAttributes,
	public FSkeletalMeshAttributesShared
{
public:

	SKELETALMESHDESCRIPTION_API explicit FSkeletalMeshAttributes(FMeshDescription& InMeshDescription);

	SKELETALMESHDESCRIPTION_API virtual void Register(bool bKeepExistingAttribute = false) override;
	
	//
	// Skin Weights Methods
	//

	/// Register a new skin weight profile with the given name. The attribute name will encode the profile name and
	/// it will be listed in GetSkinWeightProfileNames(). Returns \c true if the profile was successfully registered.
	/// Returns \c false if the attribute was already registered or if IsValidSkinWeightProfileName() returned false.
	SKELETALMESHDESCRIPTION_API bool RegisterSkinWeightAttribute(const FName& InProfileName);

	static bool IsReservedAttributeName(const FName& InAttributeName)
	{
		return FStaticMeshAttributes::IsReservedAttributeName(InAttributeName) ||
			   IsSkinWeightAttribute(InAttributeName) ||
			   InAttributeName == MeshAttribute::Bone::Name ||
			   InAttributeName == MeshAttribute::Bone::ParentIndex ||
			   InAttributeName == MeshAttribute::Bone::Pose ||
			   InAttributeName == MeshAttribute::Bone::Color;
	}
	
	/// Returns the skin weight profile given by its name. NAME_None corresponds to the default profile.
	SKELETALMESHDESCRIPTION_API FSkinWeightsVertexAttributesRef GetVertexSkinWeights(const FName& InProfileName = NAME_None);

	SKELETALMESHDESCRIPTION_API FSkinWeightsVertexAttributesRef GetVertexSkinWeightsFromAttributeName(const FName& InAttributeName = NAME_None);

	//
	// Bones Methods
	//

	/** Register an optional color attribute for bones */
	SKELETALMESHDESCRIPTION_API void RegisterColorAttribute();

	SKELETALMESHDESCRIPTION_API FBoneArray& Bones();

	SKELETALMESHDESCRIPTION_API TAttributesSet<FBoneID>& BoneAttributes();

	/** Reserves space for this number of new bones */
	SKELETALMESHDESCRIPTION_API void ReserveNewBones(const int InNumBones);

	/** Adds a new bone and returns its ID */
	SKELETALMESHDESCRIPTION_API FBoneID CreateBone();

	/** Adds a new bone  with the given ID */
	SKELETALMESHDESCRIPTION_API void CreateBone(const FBoneID BoneID);

	/** Deletes a bone with the given ID */
	SKELETALMESHDESCRIPTION_API void DeleteBone(const FBoneID BoneID);

	SKELETALMESHDESCRIPTION_API FBoneNameAttributesRef GetBoneNames();
	
	SKELETALMESHDESCRIPTION_API FBoneParentIndexAttributesRef GetBoneParentIndices();

	SKELETALMESHDESCRIPTION_API FBonePoseAttributesRef GetBonePoses();

	SKELETALMESHDESCRIPTION_API FBoneColorAttributesRef GetBoneColors();

private:
	FMeshElementChannels* BoneElements = nullptr;
};


class FSkeletalMeshConstAttributes :
	public FStaticMeshConstAttributes,
	public FSkeletalMeshAttributesShared
{
public:

	explicit FSkeletalMeshConstAttributes(const FMeshDescription& InMeshDescription) :
		FStaticMeshConstAttributes(InMeshDescription),
		FSkeletalMeshAttributesShared(InMeshDescription)
	{}

};
