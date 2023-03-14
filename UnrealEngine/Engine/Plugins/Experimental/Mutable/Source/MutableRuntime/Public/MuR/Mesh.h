// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/PlatformMath.h"
#include "Math/Transform.h"
#include "Math/VectorRegister.h"
#include "Misc/AssertionMacros.h"
#include "Misc/EnumClassFlags.h"
#include "MuR/Layout.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/MeshBufferSet.h"
#include "MuR/MutableMath.h"
#include "MuR/PhysicsBody.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/SerialisationPrivate.h"
#include "MuR/Skeleton.h"
#include "Templates/Tuple.h"

class FString;

namespace mu
{

	// Forward references
	class Layout;
	typedef Ptr<Layout> LayoutPtr;
	typedef Ptr<const Layout> LayoutPtrConst;

    class Skeleton;
    typedef Ptr<Skeleton> SkeletonPtr;
    typedef Ptr<const Skeleton> SkeletonPtrConst;

    class PhysicsBody;
    typedef Ptr<PhysicsBody> PhysicsBodyPtr;
    typedef Ptr<const PhysicsBody> PhysicsBodyPtrConst;

    class Mesh;
    typedef Ptr<Mesh> MeshPtr;
    typedef Ptr<const Mesh> MeshPtrConst;

	struct MESH_SURFACE
	{
		MESH_SURFACE()
		{
			m_firstVertex = 0;
			m_vertexCount = 0;
			m_firstIndex = 0;
			m_indexCount = 0;
			m_id = 0;
		}

		int32 m_firstVertex;
		int32 m_vertexCount;
		int32 m_firstIndex;
		int32 m_indexCount;
		uint32 m_id;


		//!
		inline bool operator==(const MESH_SURFACE& o) const
		{
			return m_firstVertex == o.m_firstVertex
				&& m_vertexCount == o.m_vertexCount
				&& m_firstIndex == o.m_firstIndex
				&& m_indexCount == o.m_indexCount
				&& m_id == o.m_id;
		}

	};

	MUTABLE_DEFINE_POD_VECTOR_SERIALISABLE(MESH_SURFACE);


	//!
	enum class EMeshBufferType
	{
		None,
		SkeletonDeformBinding,
		PhysicsBodyDeformBinding,
		PhysicsBodyDeformSelection
	};
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EMeshBufferType)

	//!
	enum class EShapeBindingMethod : uint32
	{
		ReshapeClosestProject = 0,
		ClipDeformClosestProject = 1,
		ClipDeformClosestToSurface = 2,
		ClipDeformNormalProject = 3	
	};
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EShapeBindingMethod)

	enum class EMeshCloneFlags : uint32
	{
		None = 0,
		WithSkeletalMesh      = 1 << 1,
		WithSurfaces          = 1 << 2,
		WithSkeleton          = 1 << 3,
		WithPhysicsBody       = 1 << 4,
		WithFaceGroups        = 1 << 5,
		WithTags              = 1 << 6,
		WithVertexBuffers     = 1 << 7,
		WithIndexBuffers      = 1 << 8,
		WithFaceBuffers       = 1 << 9,
		WithAdditionalBuffers = 1 << 10,
		WithLayouts           = 1 << 11,
		WithPoses			  = 1 << 12
	};
	ENUM_CLASS_FLAGS(EMeshCloneFlags);
	

    //! \brief Mesh object containing any number of buffers with any number of channels.
    //! The buffers can be per-index or per-vertex.
    //! The mesh also includes layout information for every texture channel for internal usage, and
    //! it can be ignored.
    //! The meshes are always assumed to be triangle list primitives.
    //! \ingroup runtime
    class MUTABLERUNTIME_API Mesh : public RefCounted
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        //! Deep clone this mesh.
        MeshPtr Clone() const;
		
		// Clone with flags allowing to not include some parts in the cloned mesh
		MeshPtr Clone(EMeshCloneFlags Flags) const;

        //! Serialisation
        static void Serialise( const Mesh* p, OutputArchive& arch );
        static MeshPtr StaticUnserialise( InputArchive& arch );

        //-----------------------------------------------------------------------------------------
        // Own interface
        //-----------------------------------------------------------------------------------------


        //! \name Buffers
        //! \{

        //!
        int GetIndexCount() const;

        //! Index buffers. They are owned by this mesh.
        FMeshBufferSet& GetIndexBuffers();
        const FMeshBufferSet& GetIndexBuffers() const;

        //
        int GetVertexCount() const;

        //! Vertex buffers. They are owned by this mesh.
        FMeshBufferSet& GetVertexBuffers();
        const FMeshBufferSet& GetVertexBuffers() const;

        //
        int GetFaceCount() const;

        //! Face buffers. They are owned by this mesh.
        FMeshBufferSet& GetFaceBuffers();
        const FMeshBufferSet& GetFaceBuffers() const;

        //! Get the number of surfaces defined in this mesh. Surfaces are buffer-contiguous mesh
        //! fragments that share common properties (usually material)
        int GetSurfaceCount() const;
        void GetSurface( int surfaceIndex,
                         int* firstVertex, int* vertexCount,
                         int* firstIndex, int* indexCount ) const;

        //! Return an internal id that can be used to match mesh surfaces and instance surfaces.
        //! Only valid for meshes that are part of instances.
        uint32 GetSurfaceId( int surfaceIndex ) const;

        //! \}


        //! \name Texture layouts
        //! \{

        //!
        void AddLayout( Ptr<const Layout> pLayout );

        //!
        int GetLayoutCount() const;

        //!
        const Layout* GetLayout( int i ) const;

        //!
        void SetLayout( int i, Ptr<const Layout> );
        //! \}

        //! \name Skeleton information
        //! \{

        void SetSkeleton( Ptr<const Skeleton> );
        Ptr<const Skeleton> GetSkeleton() const;

        //! \}

        //! \name PhysicsBody information
        //! \{

        void SetPhysicsBody( Ptr<const PhysicsBody> );
        Ptr<const PhysicsBody> GetPhysicsBody() const;

        //! \}

        //! \name Face groups
        //! \{

        //!
        void SetFaceGroupCount( int count );

        //!
        int GetFaceGroupCount() const;

        //!
        const char* GetFaceGroupName( int group ) const;

        //!
        void SetFaceGroupName( int group, const char* strName );

        //!
        int GetFaceGroupFaceCount( int group ) const;

        //!
        const int32* GetFaceGroupFaces( int group ) const;

        //!
        void SetFaceGroupFaces( int group, int count, const int32* faces );

        //! \}


        //! \name Tags
        //! \{

        //!
        void SetTagCount( int count );

        //!
        int GetTagCount() const;

        //!
        const char* GetTag( int tagIndex ) const;

        //!
        void SetTag( int tagIndex, const char* strName );

		//!
		const char* GetBonePoseName( int32 boneIndex ) const;

		//!
		int32 FindBonePose(const char* strName) const;
		
		//!
		void SetBonePoseCount(int32 count);

		//!
		int32 GetBonePoseCount() const;

		//!
		void SetBonePose(int32 boneIndex, const char* strName, FTransform3f transform, bool bSkinned );

		//! Return a matrix stored per bone. It is a set of 16-float values.
		void GetBoneTransform(int32 boneIndex, FTransform3f& transform) const;

        //! \}


        //! Get an internal identifier used to reference this mesh in operations like deferred
        //! mesh building, or instance updating.
        uint32 GetId() const;


    protected:

        //! Forbidden. Manage with the Ptr<> template.
		~Mesh() {}

    
	public:

		//! Non-persistent internal id unique for a mesh generated for a specific state and
		//! parameter values.
		mutable uint32 m_internalId = 0;

		//!
		FMeshBufferSet m_VertexBuffers;

		//!
		FMeshBufferSet m_IndexBuffers;

		//!
		FMeshBufferSet m_FaceBuffers;

		//! Additional buffers used for temporary or custom data in different algorithms.
		TArray< TPair<EMeshBufferType, FMeshBufferSet> > m_AdditionalBuffers;

		//! This is bit-mask on the STATIC_MESH_FORMATS enumeration, marking what static formats
		//! are compatible with this one. Usually precalculated at model compilation time.
		//! It should be reset after any operation that modifies the format.
		mutable uint32 m_staticFormatFlags = 0;

		TArray<MESH_SURFACE> m_surfaces;

		//! This skeleton and physics body are not owned and may be used by other meshes, so it cannot be modified
		//! once the mesh has been fully created.
		Ptr<const Skeleton> m_pSkeleton;
		Ptr<const PhysicsBody> m_pPhysicsBody;

		//! Texture Layout blocks attached to this mesh. They are const because they could be shared with
		//! other meshes, so they need to be cloned and replaced if a modification is needed.
		TArray<Ptr<const Layout>> m_layouts;

		struct FACE_GROUP
		{
			string m_name;
			TArray<int32> m_faces;

			inline void Serialise(OutputArchive& arch) const
			{
				const int32 ver = 0;
				arch << ver;

				arch << m_name;
				arch << m_faces;
			}

			inline void Unserialise(InputArchive& arch)
			{
				int32 ver = 0;
				arch >> ver;
				check(ver == 0);

				arch >> m_name;
				arch >> m_faces;
			}

			//!
			inline bool operator==(const FACE_GROUP& o) const
			{
				return m_name == o.m_name
					&& m_faces == o.m_faces;
			}

		};
		TArray<FACE_GROUP> m_faceGroups;

		//!
		TArray<string> m_tags;

		struct FBonePose
		{
			string m_boneName;
			uint8 m_boneSkinned : 1;
			FTransform3f m_boneTransform;

			FBonePose() : m_boneSkinned(0) {};

			inline void Serialise(OutputArchive& arch) const
			{
				const int32 ver = 0;
				arch << ver;

				arch << m_boneName;
				arch << m_boneSkinned;
				arch << m_boneTransform;
			}

			inline void Unserialise(InputArchive& arch)
			{
				int32 ver = 0;
				arch >> ver;
				check(ver == 0);

				arch >> m_boneName;

				uint8 skinned = 0;
				arch >> skinned;
				m_boneSkinned = skinned;

				arch >> m_boneTransform;
			}

			//!
			inline bool operator==(const FBonePose& o) const
			{
				return m_boneSkinned == o.m_boneSkinned
					&& m_boneName == o.m_boneName;
			}

		};
		// This is the pose used by this mesh fragment, used to update the transforms of the final skeleton
		// taking into consideration the meshes being used.
		TArray<FBonePose> m_bonePoses;

		//!
		inline void Serialise(OutputArchive& arch) const
		{
			uint32 ver = 13;
			arch << ver;

			arch << m_IndexBuffers;
			arch << m_VertexBuffers;
			arch << m_FaceBuffers;
			arch << m_AdditionalBuffers;
			arch << m_layouts;

			arch << m_pSkeleton;
			arch << m_pPhysicsBody;

			arch << m_staticFormatFlags;
			arch << m_surfaces;
			arch << m_faceGroups;

			arch << m_tags;

			arch << m_bonePoses;
		}

		//!
		inline void Unserialise(InputArchive& arch)
		{
			uint32 ver;
			arch >> ver;
			check(ver <= 13);

			arch >> m_IndexBuffers;
			arch >> m_VertexBuffers;
			arch >> m_FaceBuffers;
			arch >> m_AdditionalBuffers;
			arch >> m_layouts;

			arch >> m_pSkeleton;
			if (ver >= 12)
			{ 
				arch >> m_pPhysicsBody;
			}
			else
			{
				m_pPhysicsBody = nullptr;
			}

			arch >> m_staticFormatFlags;
			arch >> m_surfaces;
			arch >> m_faceGroups;

			arch >> m_tags;

			if (ver >= 13)
			{
				arch >> m_bonePoses;
			}
			else if (m_pSkeleton)
			{
				const int32 NumBones = m_pSkeleton->GetBoneCount();
				m_bonePoses.SetNum(NumBones);
				check(m_pSkeleton->m_boneTransforms_DEPRECATED.Num() == NumBones);

				for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
				{
					m_bonePoses[BoneIndex].m_boneName = m_pSkeleton->GetBoneName(BoneIndex);
					m_bonePoses[BoneIndex].m_boneSkinned = true;
					m_bonePoses[BoneIndex].m_boneTransform = m_pSkeleton->m_boneTransforms_DEPRECATED[BoneIndex];
				}
			}
		}


		//!
		inline bool operator==(const Mesh& o) const
		{
			bool equal = (m_IndexBuffers == o.m_IndexBuffers);
			if (equal) equal = (m_VertexBuffers == o.m_VertexBuffers);
			if (equal) equal = (m_FaceBuffers == o.m_FaceBuffers);
			if (equal) equal = (m_layouts.Num() == o.m_layouts.Num());
			if (equal) equal = (m_bonePoses.Num() == o.m_bonePoses.Num());
			if (equal && m_pSkeleton != o.m_pSkeleton)
			{
				if (m_pSkeleton && o.m_pSkeleton)
				{
					equal = (*m_pSkeleton == *o.m_pSkeleton);
				}
				else
				{
					equal = false;
				}
			}
			if (equal) equal = (m_surfaces == o.m_surfaces);
			if (equal) equal = (m_faceGroups == o.m_faceGroups);
			if (equal) equal = (m_tags == o.m_tags);

			for (int32 i = 0; equal && i < m_layouts.Num(); ++i)
			{
				equal &= (*m_layouts[i]) == (*o.m_layouts[i]);
			}

			for (int32 i = 0; equal && i < m_AdditionalBuffers.Num(); ++i)
			{
				equal &= m_AdditionalBuffers[i] == o.m_AdditionalBuffers[i];
			}

			for (int32 i = 0; equal && i < m_bonePoses.Num(); ++i)
			{
				equal &= m_bonePoses[i] == o.m_bonePoses[i];
			}

			return equal;
		}

		//! Compare the mesh with another one, but ignore internal data like generated vertex
		//! indices.
		bool IsSimilar(const Mesh& o, bool bCompareLayouts) const;

		//! Compare the vertex attributes to check if they match.
		bool IsSameVertex( uint32 v, const Mesh& other, uint32 otherVertexIndex, float tolerance = UE_SMALL_NUMBER) const;


		//! Make a map from the vertices in this mesh to thefirst matching vertex of the given
		//! mesh. If non is found, the index is set to -1.
		struct VERTEX_MATCH_MAP
		{
			//! One for every vertex
			TArray<int> m_firstMatch;

			//! The matches of every vertex in a sequence
			TArray<int> m_matches;

			//!
			bool Matches(int v, int ov) const;
		};

		void GetVertexMap
		(
			const Mesh& other,
			VERTEX_MATCH_MAP& vertexMap,
			float tolerance = 1e-3f//1e-5f //std::numeric_limits<float>::min()
		) const;

		//! Return true if the mesh has a face with the same vertices than the face in the given
		//! mesh. The actual vertex data is used in the comparison.
		bool HasFace( const Mesh& other, int otherFaceIndex, const VERTEX_MATCH_MAP& vertexMap ) const;

		//! Compare the vertex attributes to check if they match.
		vec3<uint32> GetFaceVertexIndices(int f) const;

		//! Return true if the given mesh has the same vertex and index formats, and in the same
		//! buffer structure.
		bool HasCompatibleFormat(const Mesh* pOther) const;

		//! Update the flags identifying the mesh format as some of the optimised formats.
		void ResetStaticFormatFlags() const;

		//! Get the total memory size of the buffers
		size_t GetDataSize() const;

		//! Create the surface data if not present.
		void EnsureSurfaceData();

		//! Check mesh buffer data for possible inconsistencies
		void CheckIntegrity() const;

		//! Change the buffer descriptions so that all buffer indices start at 0 and are in the
		//! same order than memory.
		void ResetBufferIndices();

		//! Debug: get a text representation of the mesh
		void Log(FString& out, int32 VertrexLimit);
    };


}

