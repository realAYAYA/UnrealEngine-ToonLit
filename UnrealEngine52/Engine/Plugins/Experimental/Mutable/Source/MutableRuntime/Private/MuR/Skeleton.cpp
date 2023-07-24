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

		pResult->m_bones = m_bones;
		pResult->m_boneParents = m_boneParents;

		return pResult;
	}


	//---------------------------------------------------------------------------------------------
	int32 Skeleton::GetBoneCount() const
	{
		return m_bones.Num();
	}


	//---------------------------------------------------------------------------------------------
	void Skeleton::SetBoneCount(int32 b)
	{
		m_bones.SetNum(b);
		m_boneParents.Init(INDEX_NONE, b);
	}


	//---------------------------------------------------------------------------------------------
	const char* Skeleton::GetBoneName(int32 boneIndex) const
	{
		check(boneIndex >= 0 && boneIndex < GetBoneCount());
		if (boneIndex >= 0 && boneIndex < GetBoneCount())
		{
			return m_bones[boneIndex].c_str();
		}
		return "";
	}


	//---------------------------------------------------------------------------------------------
	void Skeleton::SetBoneName(int32 boneIndex, const char* strName)
	{
		check(boneIndex >= 0 && boneIndex < GetBoneCount());
		if (boneIndex >= 0 && boneIndex < GetBoneCount())
		{
			m_bones[boneIndex] = strName;
		}
	}


	//---------------------------------------------------------------------------------------------
	int Skeleton::GetBoneParent(int32 boneIndex) const
	{
		if (boneIndex >= 0 && boneIndex < m_boneParents.Num())
		{
			return m_boneParents[boneIndex];
		}

		return -1;
	}


	//-------------------------------------------------------------------------------------------------
	void Skeleton::SetBoneParent(int32 boneIndex, int32 parentBoneIndex)
	{
		check(boneIndex >= 0 && boneIndex < GetBoneCount());
		check(parentBoneIndex >= -1 && parentBoneIndex < GetBoneCount() &&
			parentBoneIndex < 0xffff);
		if (boneIndex >= 0 && boneIndex < m_boneParents.Num())
		{
			m_boneParents[boneIndex] = (int16_t)parentBoneIndex;
		}
	}


}