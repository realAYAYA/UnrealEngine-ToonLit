// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "MeshAttributeArray.h"
#include "MeshAttributes.h"
#include "MeshDescription.h"
#include "SkinWeightsAttributesRef.h"
#include "StaticMeshAttributes.h"
#include "UObject/NameTypes.h"
#include "UObject/UnrealNames.h"

template <typename AttributeType> class TArrayAttribute;

//Add any skeletalmesh specific attributes here

namespace MeshAttribute
{
 	namespace Vertex
 	{
		extern SKELETALMESHDESCRIPTION_API const FName SkinWeights;
 	}
}


class SKELETALMESHDESCRIPTION_API FSkeletalMeshAttributesShared
{
public:
	// The name of the default skin weight profile.
	static FName DefaultSkinWeightProfileName;
	
	FSkeletalMeshAttributesShared(const FMeshDescription& InMeshDescription) :
		MeshDescriptionShared(InMeshDescription)
	{}

	/// Returns the list of all registered skin weight profile names on this mesh.
	TArray<FName> GetSkinWeightProfileNames() const;
	
	/// Returns \c true if the given identifier is a valid profile name. If the name is empty, or matches the default profile,
	/// then the profile name is considered invalid. 
	static bool IsValidSkinWeightProfileName(const FName InProfileName);

	/// Helper function that indicates whether an attribute name represents a skin weight attribute.
	static bool IsSkinWeightAttribute(const FName InAttributeName);

	/// Returns a skin profile name from the attribute name, if the attribute name is a valid skin weights
	/// attribute.
	static FName GetProfileNameFromAttribute(const FName InAttributeName);

	FSkinWeightsVertexAttributesConstRef GetVertexSkinWeights(const FName InProfileName = NAME_None) const
	{
		return MeshDescriptionShared.VertexAttributes().GetAttributesRef<TArrayAttribute<int32>>(CreateSkinWeightAttributeName(InProfileName));
	}
	
	FSkinWeightsVertexAttributesConstRef GetVertexSkinWeightsFromAttributeName(const FName InAttributeName = NAME_None) const
	{
		if (IsSkinWeightAttribute(InAttributeName))
		{
			return MeshDescriptionShared.VertexAttributes().GetAttributesRef<TArrayAttribute<int32>>(InAttributeName);
		}
		else
		{
			return {};
		}
	}

protected:
	/// Construct a name for a skin weight attribute with the given skin weight profile name.
	/// Each mesh description can hold different skin weight profiles, although the default
	/// is always present.
	static FName CreateSkinWeightAttributeName(const FName InProfileName);

private:
	const FMeshDescription& MeshDescriptionShared;
};


class SKELETALMESHDESCRIPTION_API FSkeletalMeshAttributes :
	public FStaticMeshAttributes,
	public FSkeletalMeshAttributesShared
{
public:

	explicit FSkeletalMeshAttributes(FMeshDescription& InMeshDescription) :
		FStaticMeshAttributes(InMeshDescription),
		FSkeletalMeshAttributesShared(InMeshDescription)
	{}

	virtual void Register() override;

	/// Register a new skin weight profile with the given name. The attribute name will encode the profile name and
	/// it will be listed in GetSkinWeightProfileNames(). Returns \c true if the profile was successfully registered.
	/// Returns \c false if the attribute was already registered or if IsValidSkinWeightProfileName() returned false.
	bool RegisterSkinWeightAttribute(const FName InProfileName);

	static bool IsReservedAttributeName(const FName InAttributeName)
	{
		return FStaticMeshAttributes::IsReservedAttributeName(InAttributeName) ||
               InAttributeName == MeshAttribute::VertexInstance::TextureCoordinate ||
               InAttributeName == MeshAttribute::VertexInstance::Normal ||
               InAttributeName == MeshAttribute::VertexInstance::Tangent ||
               InAttributeName == MeshAttribute::VertexInstance::BinormalSign ||
               InAttributeName == MeshAttribute::VertexInstance::Color ||
               InAttributeName == MeshAttribute::Triangle::Normal ||
               InAttributeName == MeshAttribute::Triangle::Tangent ||
               InAttributeName == MeshAttribute::Triangle::Binormal ||
			   IsSkinWeightAttribute(InAttributeName);
	}	
	
	/// Returns the skin weight profile given by its name. NAME_None corresponds to the default profile.
	FSkinWeightsVertexAttributesRef GetVertexSkinWeights(const FName InProfileName = NAME_None)
	{
		return MeshDescription.VertexAttributes().GetAttributesRef<TArrayAttribute<int32>>(CreateSkinWeightAttributeName(InProfileName));
	}

	FSkinWeightsVertexAttributesRef GetVertexSkinWeightsFromAttributeName(const FName InAttributeName = NAME_None)
	{
		if (IsSkinWeightAttribute(InAttributeName))
		{
			return MeshDescription.VertexAttributes().GetAttributesRef<TArrayAttribute<int32>>(InAttributeName);
		}
		else
		{
			return {};
		}
	}

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
