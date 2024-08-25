// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Skeleton.h"

#include "MuR/SerialisationPrivate.h"


namespace mu
{


	//---------------------------------------------------------------------------------------------
	void Skeleton::Serialise(const Skeleton* p, OutputArchive& arch)
	{
		arch << *p;
	}


	//---------------------------------------------------------------------------------------------
	mu::Ptr<Skeleton> Skeleton::StaticUnserialise(InputArchive& arch)
	{
		SkeletonPtr pResult = new Skeleton();
		arch >> *pResult;
		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	SkeletonPtr Skeleton::Clone() const
	{
		SkeletonPtr pResult = new Skeleton();

		pResult->BoneIds = BoneIds;
		pResult->BoneParents = BoneParents;

		// For debug
		pResult->BoneNames = BoneNames;

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	int32 Skeleton::GetBoneCount() const
	{
		return BoneIds.Num();
	}


	//---------------------------------------------------------------------------------------------
	void Skeleton::SetBoneCount(int32 NumBones)
	{
		BoneNames.SetNum(NumBones);
		BoneIds.SetNum(NumBones);
		BoneParents.Init(INDEX_NONE, NumBones);
	}


	//---------------------------------------------------------------------------------------------
	const FName Skeleton::GetBoneFName(int32 Index) const
	{
		if (BoneNames.IsValidIndex(Index))
		{
			return BoneNames[Index];
		}

		return FName("Unknown Bone");
	}


	//---------------------------------------------------------------------------------------------
	void Skeleton::SetBoneFName(const int32 Index, const FName BoneName)
	{
		if (BoneNames.IsValidIndex(Index))
		{
			BoneNames[Index] = BoneName;
		}
	}


	//---------------------------------------------------------------------------------------------
	int32 Skeleton::GetBoneParent(int32 Index) const
	{
		if (BoneParents.IsValidIndex(Index))
		{
			return BoneParents[Index];
		}

		return INDEX_NONE;
	}


	//-------------------------------------------------------------------------------------------------
	void Skeleton::SetBoneParent(int32 Index, int32 ParentIndex)
	{
		check(ParentIndex >= -1 && ParentIndex < GetBoneCount() && ParentIndex < 0xffff);
		check(BoneParents.IsValidIndex(Index));
		
		if (BoneParents.IsValidIndex(Index))
		{
			BoneParents[Index] = (int16)ParentIndex;
		}
	}


	//-------------------------------------------------------------------------------------------------
	uint16 Skeleton::GetBoneId(const int32 Index) const
	{
		check(BoneIds.IsValidIndex(Index));
		return BoneIds[Index];
	}

	
	//-------------------------------------------------------------------------------------------------
	void Skeleton::SetBoneId(const int32 Index, const uint16 BoneId)
	{
		check(BoneIds.IsValidIndex(Index));
		if (BoneIds.IsValidIndex(Index))
		{
			BoneIds[Index] = BoneId;
		}
	}


	//-------------------------------------------------------------------------------------------------
	int32 Skeleton::FindBone(const uint16 BoneId) const
	{
		return BoneIds.Find(BoneId);
	}


	//-------------------------------------------------------------------------------------------------
	void Skeleton::Serialise(OutputArchive& arch) const
	{
		uint32 ver = 6;
		arch << ver;

		arch << BoneIds;
		arch << BoneParents;
	}


	//-------------------------------------------------------------------------------------------------
	void Skeleton::Unserialise(InputArchive& arch)
	{
		uint32 ver;
		arch >> ver;
		check(ver >= 3);

		if(ver >= 6)
		{
			arch >> BoneIds;
		}
		else
		{
			TArray<std::string> OldBoneNames;
			arch >> OldBoneNames;

			const int32 NumBones = OldBoneNames.Num();

			BoneIds.SetNum(NumBones);
			for (int32 Index = 0; Index < NumBones; ++Index)
			{
				BoneIds[Index] = Index;
			}
		}

		if (ver == 3)
		{
			arch >> m_boneTransforms_DEPRECATED;
		}

		arch >> BoneParents;

		if (ver < 6) // before BoneIds 
		{
			int16 ParentIndex = INDEX_NONE;
			for (int16& BoneParent : BoneParents)
			{
				BoneParent = ParentIndex;
				ParentIndex++;
			}
		}

		if (ver <= 4)
		{
			TArray<int32> BoneIds_DEPRECATED;
			arch >> BoneIds_DEPRECATED;
		}

		if (ver == 3)
		{
			bool bBoneTransformModified;
			arch >> bBoneTransformModified;
		}
	}
}
