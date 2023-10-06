// Copyright Epic Games, Inc. All Rights Reserved.

#include "SkeletalMeshAttributes.h"
#include "MeshDescription.h"

namespace MeshAttribute
{
	const FName Vertex::SkinWeights("SkinWeights");
	
	namespace Bone
	{
		const FName Name("Name");
		const FName ParentIndex("ParentIndex");
		const FName Pose("Pose");
		const FName Color("Color");
	}
}


FName FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName("Default");

FName FSkeletalMeshAttributesShared::BonesElementName("BonesElementName");

static FString SkinWeightAttributeNamePrefix()
{
	return MeshAttribute::Vertex::SkinWeights.ToString() + TEXT("-");
}


//
// FSkeletalMeshAttributes
//

FSkeletalMeshAttributes::FSkeletalMeshAttributes(FMeshDescription& InMeshDescription)
:
FStaticMeshAttributes(InMeshDescription),
FSkeletalMeshAttributesShared(InMeshDescription)
{
	if (MeshDescription.GetElements().Contains(BonesElementName))
	{
		BoneElements = MeshDescription.GetElements()[BonesElementName].Get();
	}
}

void FSkeletalMeshAttributes::Register(bool bKeepExistingAttribute)
{
	if (!MeshDescription.VertexAttributes().HasAttribute(MeshAttribute::Vertex::SkinWeights) || !bKeepExistingAttribute)
	{
		MeshDescription.VertexAttributes().RegisterAttribute<int32[]>(MeshAttribute::Vertex::SkinWeights, 1, 0, EMeshAttributeFlags::Mandatory);
	}

	if (MeshDescription.GetElements().Contains(BonesElementName) == false)
	{
		BoneElements = MeshDescription.GetElements().Emplace(BonesElementName).Get();
	}

	BoneAttributes().RegisterAttribute<FName>(MeshAttribute::Bone::Name, 1, NAME_None, EMeshAttributeFlags::Mandatory);
	
	BoneAttributes().RegisterAttribute<int32>(MeshAttribute::Bone::ParentIndex, 1, INDEX_NONE, EMeshAttributeFlags::Mandatory);
	
	BoneAttributes().RegisterAttribute<FTransform>(MeshAttribute::Bone::Pose, 1, FTransform::Identity, EMeshAttributeFlags::Mandatory);

	// Call super class
	FStaticMeshAttributes::Register(bKeepExistingAttribute);
}

void FSkeletalMeshAttributes::RegisterColorAttribute()
{
	checkSlow(MeshDescription.GetElements().Contains(BonesElementName));
	BoneAttributes().RegisterAttribute<FVector4f>(MeshAttribute::Bone::Color, 1, FVector4f(1.0f, 1.0f, 1.0f, 1.0f), EMeshAttributeFlags::Mandatory);
}

bool FSkeletalMeshAttributes::RegisterSkinWeightAttribute(const FName& InProfileName)
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

FSkinWeightsVertexAttributesRef FSkeletalMeshAttributes::GetVertexSkinWeights(const FName& InProfileName)
{
	return MeshDescription.VertexAttributes().GetAttributesRef<TArrayAttribute<int32>>(CreateSkinWeightAttributeName(InProfileName));
}

FSkinWeightsVertexAttributesRef FSkeletalMeshAttributes::GetVertexSkinWeightsFromAttributeName(const FName& InAttributeName)
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

FSkeletalMeshAttributes::FBoneArray& FSkeletalMeshAttributes::Bones() 
{ 
	return static_cast<TMeshElementContainer<FBoneID>&>(BoneElements->Get()); 
}

TAttributesSet<FBoneID>& FSkeletalMeshAttributes::BoneAttributes() 
{ 
	return Bones().GetAttributes();
}

void FSkeletalMeshAttributes::ReserveNewBones(const int InNumBones) 
{
	BoneElements->Get().Reserve(InNumBones);
}

FBoneID FSkeletalMeshAttributes::CreateBone()
{
	return BoneElements->Get().Add();
}

void FSkeletalMeshAttributes::CreateBone(const FBoneID BoneID)
{
	BoneElements->Get().Insert(BoneID);
}

void FSkeletalMeshAttributes::DeleteBone(const FBoneID BoneID)
{
	BoneElements->Get().Remove(BoneID);
}

FSkeletalMeshAttributes::FBoneNameAttributesRef FSkeletalMeshAttributes::GetBoneNames()
{	
	return BoneAttributes().GetAttributesRef<FName>(MeshAttribute::Bone::Name);
}

FSkeletalMeshAttributes::FBoneParentIndexAttributesRef FSkeletalMeshAttributes::GetBoneParentIndices()
{	
	return BoneAttributes().GetAttributesRef<int32>(MeshAttribute::Bone::ParentIndex);
}

FSkeletalMeshAttributes::FBonePoseAttributesRef FSkeletalMeshAttributes::GetBonePoses()
{
	return BoneAttributes().GetAttributesRef<FTransform>(MeshAttribute::Bone::Pose);
}

FSkeletalMeshAttributes::FBoneColorAttributesRef FSkeletalMeshAttributes::GetBoneColors()
{
	return BoneAttributes().GetAttributesRef<FVector4f>(MeshAttribute::Bone::Color);
}



//
// FSkeletalMeshAttributesShared
//

FSkeletalMeshAttributesShared::FSkeletalMeshAttributesShared(const FMeshDescription& InMeshDescription) 
:
MeshDescriptionShared(InMeshDescription)
{
	if (MeshDescriptionShared.GetElements().Contains(BonesElementName))
	{
		BoneElementsShared = MeshDescriptionShared.GetElements()[BonesElementName].Get();
	}
}

FSkinWeightsVertexAttributesConstRef FSkeletalMeshAttributesShared::GetVertexSkinWeights(const FName& InProfileName) const
{
	return MeshDescriptionShared.VertexAttributes().GetAttributesRef<TArrayAttribute<int32>>(CreateSkinWeightAttributeName(InProfileName));
}

FSkinWeightsVertexAttributesConstRef FSkeletalMeshAttributesShared::GetVertexSkinWeightsFromAttributeName(const FName& InAttributeName) const
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

bool FSkeletalMeshAttributesShared::IsValidSkinWeightProfileName(const FName& InProfileName)
{
	return !InProfileName.IsNone() && !InProfileName.IsEqual(DefaultSkinWeightProfileName, ENameCase::IgnoreCase);
}

bool FSkeletalMeshAttributesShared::IsSkinWeightAttribute(const FName& InAttributeName)
{
	return InAttributeName == MeshAttribute::Vertex::SkinWeights ||
		   InAttributeName.ToString().StartsWith(SkinWeightAttributeNamePrefix());
}

FName FSkeletalMeshAttributesShared::GetProfileNameFromAttribute(const FName& InAttributeName)
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

FName FSkeletalMeshAttributesShared::CreateSkinWeightAttributeName(const FName& InProfileName)
{
	// If it's the default profile, then return the base skin weights attribute name.
	if (InProfileName.IsNone() || InProfileName.IsEqual(FSkeletalMeshAttributesShared::DefaultSkinWeightProfileName, ENameCase::IgnoreCase))
	{
		return MeshAttribute::Vertex::SkinWeights;
	}	

	return FName(SkinWeightAttributeNamePrefix() + InProfileName.ToString());
}

bool FSkeletalMeshAttributesShared::HasBoneColorAttribute() const
{
	return BoneAttributes().HasAttribute(MeshAttribute::Bone::Color);
}

bool FSkeletalMeshAttributesShared::HasBoneNameAttribute() const
{
	return BoneAttributes().HasAttribute(MeshAttribute::Bone::Name);
}

bool FSkeletalMeshAttributesShared::HasBonePoseAttribute() const
{
	return BoneAttributes().HasAttribute(MeshAttribute::Bone::Pose);
}

bool FSkeletalMeshAttributesShared::HasBoneParentIndexAttribute() const
{
	return BoneAttributes().HasAttribute(MeshAttribute::Bone::ParentIndex);
}

const FSkeletalMeshAttributesShared::FBoneArray& FSkeletalMeshAttributesShared::Bones() const
{ 
	return static_cast<const TMeshElementContainer<FBoneID>&>(BoneElementsShared->Get());
}

const TAttributesSet<FBoneID>& FSkeletalMeshAttributesShared::BoneAttributes() const 
{ 
	return Bones().GetAttributes(); 
}

int32 FSkeletalMeshAttributesShared::GetNumBones() const
{
	return HasBones() ? BoneElementsShared->Get().Num() : 0;
}

FSkeletalMeshAttributesShared::FBoneNameAttributesConstRef FSkeletalMeshAttributesShared::GetBoneNames() const
{
	return BoneAttributes().GetAttributesRef<FName>(MeshAttribute::Bone::Name);
}

FSkeletalMeshAttributesShared::FBoneParentIndexAttributesConstRef FSkeletalMeshAttributesShared::GetBoneParentIndices() const
{	
	return BoneAttributes().GetAttributesRef<int32>(MeshAttribute::Bone::ParentIndex);
}

FSkeletalMeshAttributesShared::FBonePoseAttributesConstRef FSkeletalMeshAttributesShared::GetBonePoses() const
{
	return BoneAttributes().GetAttributesRef<FTransform>(MeshAttribute::Bone::Pose);
}

FSkeletalMeshAttributesShared::FBoneColorAttributesConstRef FSkeletalMeshAttributesShared::GetBoneColors() const
{
	return BoneAttributes().GetAttributesRef<FVector4f>(MeshAttribute::Bone::Color);
}