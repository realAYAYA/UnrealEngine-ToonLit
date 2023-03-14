// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshAttributes.h"

namespace MeshAttribute
{
	const FName Vertex::SkinWeights("SkinWeights");
}


FName FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName("Default");


static FString SkinWeightAttributeNamePrefix()
{
	return MeshAttribute::Vertex::SkinWeights.ToString() + TEXT("-");
}

void FSkeletalMeshAttributes::Register()
{
	MeshDescription.VertexAttributes().RegisterAttribute<int32[]>(MeshAttribute::Vertex::SkinWeights, 1, 0.0f, EMeshAttributeFlags::Mandatory);

	// Call super class
	FStaticMeshAttributes::Register();
}

bool FSkeletalMeshAttributes::RegisterSkinWeightAttribute(const FName InProfileName)
{
	if (!IsValidSkinWeightProfileName(InProfileName))
	{
		return false;
	}	
	
	const FName AttributeName = CreateSkinWeightAttributeName(InProfileName);
	if (ensure(AttributeName.IsValid()))
	{
		TArray<FName> AllAttributeNames;
		MeshDescription.VertexAttributes().GetAttributeNames(AllAttributeNames);
		if (AllAttributeNames.Contains(AttributeName))
		{
			return false;
		}

		return MeshDescription.VertexAttributes().RegisterAttribute<int32[]>(AttributeName, 1, 0, EMeshAttributeFlags::Mandatory).IsValid();
	}
	else
	{
		return false;
	}	
}

TArray<FName> FSkeletalMeshAttributesShared::GetSkinWeightProfileNames() const
{
	TArray<FName> AllAttributeNames;
	MeshDescriptionShared.VertexAttributes().GetAttributeNames(AllAttributeNames);

	TArray<FName> SkinWeightProfileNames;
	bool bHasDefault = false; 

	for (const FName& AttributeName: AllAttributeNames)
	{
		if (AttributeName == MeshAttribute::Vertex::SkinWeights)
		{
			bHasDefault = true;
		}
		else if (IsSkinWeightAttribute(AttributeName))
		{
			SkinWeightProfileNames.Add(GetProfileNameFromAttribute(AttributeName));
		}
	}

	SkinWeightProfileNames.Sort([](const FName A, const FName B) -> bool { return A.FastLess(B); });
	if (bHasDefault)
	{
		SkinWeightProfileNames.Insert(DefaultSkinWeightProfileName, 0);
	}
	
	return SkinWeightProfileNames;
}

bool FSkeletalMeshAttributesShared::IsValidSkinWeightProfileName(const FName InProfileName)
{
	return !InProfileName.IsNone() && !InProfileName.IsEqual(DefaultSkinWeightProfileName, ENameCase::IgnoreCase);
}

bool FSkeletalMeshAttributesShared::IsSkinWeightAttribute(const FName InAttributeName)
{
	return InAttributeName == MeshAttribute::Vertex::SkinWeights ||
		   InAttributeName.ToString().StartsWith(SkinWeightAttributeNamePrefix());
}

FName FSkeletalMeshAttributesShared::GetProfileNameFromAttribute(const FName InAttributeName)
{
	if (InAttributeName == MeshAttribute::Vertex::SkinWeights)
	{
		return DefaultSkinWeightProfileName;
	}
	
	const FString Prefix = SkinWeightAttributeNamePrefix();
	if (InAttributeName.ToString().StartsWith(Prefix))
	{
		return FName(InAttributeName.ToString().Mid(Prefix.Len()));
	}
	else
	{
		return NAME_None;
	}
}

FName FSkeletalMeshAttributesShared::CreateSkinWeightAttributeName(const FName InProfileName)
{
	// If it's the default profile, then return the base skin weights attribute name.
	if (InProfileName.IsNone() || InProfileName.IsEqual(FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName, ENameCase::IgnoreCase))
	{
		return MeshAttribute::Vertex::SkinWeights;
	}	

	return FName(SkinWeightAttributeNamePrefix() + InProfileName.ToString());
}
