// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Serialisation.h"
#include "Containers/Array.h"
#include "HAL/PlatformMath.h"
#include "Math/Transform.h"
#include "Math/VectorRegister.h"
#include "Misc/AssertionMacros.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"

// Remove when removin deprecated data
#include <string>


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

		//! DEBUG. Return the FName of the bone at index BoneIndex. Only valid in the editor
		const FName GetBoneFName(int32 Index) const;
		void SetBoneFName(const int32 Index, const FName BoneName);

        //! Get and set the parent bone of each bone. The parent can be -1 if the bone is a root.
        int32 GetBoneParent(int32 boneIndex) const;
        void SetBoneParent(int32 boneIndex, int32 parentBoneIndex);

		//! Return the BoneIndex of the bone at index from the BoneIndices array
		uint16 GetBoneId(int32 Index) const;
		void SetBoneId(int32 Index, uint16 BoneIndex);

		//! Return the index of BoneIndex inside the BoneIndices array.
		int32 FindBone(const uint16 BoneIndex) const;
		

	protected:

		//! Forbidden. Manage with the Ptr<> template.
		~Skeleton() {}

	public:

		//! Deprecated
		TArray<std::string> m_bones_DEPRECATED;
		TArray<FTransform3f> m_boneTransforms_DEPRECATED;



		//! DEBUG. FNames of the bones. Only valid in the editor. Do not serialize.
		TArray<FName> BoneNames;

		//! Ids of the bones. The Id is the BoneName index in the CO BoneNames array
		TArray<uint16> BoneIds;

		//! For each bone, index of the parent bone in the bone vectors. -1 means no parent.
		//! This array must have the same size than the m_bones array.
		TArray<int16> BoneParents;

		//!
		void Serialise(OutputArchive& arch) const;

		//!
		void Unserialise(InputArchive& arch);

		//!
		inline bool operator==(const Skeleton& o) const
		{
			return BoneIds == o.BoneIds
				&& BoneParents == o.BoneParents;
		}
	};

}

