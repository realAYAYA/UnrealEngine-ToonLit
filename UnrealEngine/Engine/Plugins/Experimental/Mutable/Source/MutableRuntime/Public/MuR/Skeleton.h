// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/MemoryPrivate.h"
#include "MuR/SerialisationPrivate.h"
#include "Containers/Array.h"
#include "HAL/PlatformMath.h"
#include "Math/Transform.h"
#include "Math/VectorRegister.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"


namespace mu
{

	// Forward references
    class Skeleton;

    typedef Ptr<Skeleton> SkeletonPtr;
    typedef Ptr<const Skeleton> SkeletonPtrConst;


    //! \brief Skeleton object.
	//! \ingroup runtime
    class MUTABLERUNTIME_API Skeleton : public RefCounted
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

		Skeleton() {}

        //! Deep clone this skeleton.
        Ptr<Skeleton> Clone() const;

		//! Serialisation
        static void Serialise( const Skeleton* p, OutputArchive& arch );
        static Ptr<Skeleton> StaticUnserialise( InputArchive& arch );


		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

        //! Return the number of bones used by this skeleton
		int32 GetBoneCount() const;
        void SetBoneCount(int32 c);

        //! Return the name of one of the bones used by this skeleton
		//! \param boneIndex goes from 0 to GetBoneCount()-1
		const char* GetBoneName(int32 boneIndex) const;
        void SetBoneName( int b, const char* strName );

        //! Get and set the parent bone of each bone. The parent can be -1 if the bone is a root.
        int32 GetBoneParent(int32 boneIndex) const;
        void SetBoneParent(int32 boneIndex, int32 parentBoneIndex);

        //! Get and set an optional external Id for each bone that can be retrieved from generated
        //! skeletons. This data is opaque to mutable, so it is copied around and merged as
        //! necessary, but never changed. By default all indices are 0.
        int32 GetBoneId(int32 boneIndex) const;
        void SetBoneId(int32 boneIndex, int32 boneId);

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~Skeleton() {}

	public:

		//!
		TArray<string> m_bones;
		TArray<FTransform3f> m_boneTransforms_DEPRECATED;

		//! For each bone, index of the parent bone in the bone vectors. -1 means no parent.
		//! This array must have the same size than the m_bones array.
		TArray<int16> m_boneParents;

		//! For each bone, an externally provided optional id. This vector can be empty or smaller
		//! Than the number of bones, and missing bones are considered to have id 0.
		TArray<int32> m_boneIds;


		//!
		inline void Serialise(OutputArchive& arch) const
		{
			uint32 ver = 4;
			arch << ver;

			arch << m_bones;
			arch << m_boneParents;
			arch << m_boneIds;
		}

		//!
		inline void Unserialise(InputArchive& arch)
		{
			uint32 ver;
			arch >> ver;
			check(ver >= 3);

			arch >> m_bones;

			if (ver == 3)
			{
				arch >> m_boneTransforms_DEPRECATED;
			}

			arch >> m_boneParents;
			arch >> m_boneIds;

			if (ver == 3)
			{
				// ensure m_boneIds' size is equal to the number of bones
				m_boneIds.SetNum(m_bones.Num());

				bool bBoneTransformModified;
				arch >> bBoneTransformModified;
			}
		}


		//!
		inline bool operator==(const Skeleton& o) const
		{
			return m_bones == o.m_bones 
				&& m_boneParents == o.m_boneParents
				&& m_boneIds == o.m_boneIds;
		}


		//-----------------------------------------------------------------------------------------
		//! Fins a bone index by name
		//-----------------------------------------------------------------------------------------
		int FindBone(const char* strName) const
		{
			for (int32 i = 0; i < m_bones.Num(); ++i)
			{
				if (m_bones[i] == strName)
				{
					return (int)i;
				}
			}
			return -1;
		}

	};

}

