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
#include "MuR/MeshBufferSet.h"
#include "MuR/PhysicsBody.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"
#include "MuR/Serialisation.h"
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

			BoneMapIndex = 0;
			BoneMapCount = 0;

			bCastShadow = false;
		}

		int32 m_firstVertex;
		int32 m_vertexCount;
		int32 m_firstIndex;
		int32 m_indexCount;
		uint32 m_id;
		
		uint32 BoneMapIndex;
		uint32 BoneMapCount;

		bool bCastShadow;

		//!
		inline bool operator==(const MESH_SURFACE& o) const
		{
			return m_firstVertex == o.m_firstVertex
				&& m_vertexCount == o.m_vertexCount
				&& m_firstIndex == o.m_firstIndex
				&& m_indexCount == o.m_indexCount
				&& m_id == o.m_id
				&& BoneMapIndex == o.BoneMapIndex
				&& BoneMapCount == o.BoneMapCount
				&& bCastShadow == o.bCastShadow;
		}

		inline void Serialise(OutputArchive& arch) const;

		inline void Unserialise(InputArchive& arch);

	};


	enum class EBoneUsageFlags : uint32
	{
		None		   = 0,
		Root		   = 1 << 1,
		Skinning	   = 1 << 2,
		SkinningParent = 1 << 3,
		Physics	       = 1 << 4,
		PhysicsParent  = 1 << 5,
		Deform         = 1 << 6,
		DeformParent   = 1 << 7,
		Reshaped       = 1 << 8	
	};

	ENUM_CLASS_FLAGS(EBoneUsageFlags);

	//!
	enum class EMeshBufferType
	{
		None,
		SkeletonDeformBinding,
		PhysicsBodyDeformBinding,
		PhysicsBodyDeformSelection,
		PhysicsBodyDeformOffsets,
		MeshLaplacianData,
		MeshLaplacianOffsets,
		UniqueVertexMap
	};

	//!
	enum class EShapeBindingMethod : uint32
	{
		ReshapeClosestProject = 0,
		ClipDeformClosestProject = 1,
		ClipDeformClosestToSurface = 2,
		ClipDeformNormalProject = 3	
	};

	enum class EVertexColorUsage : uint32
	{
		None = 0,
		ReshapeMaskWeight = 1,
		ReshapeClusterId = 2
	};

	enum class EMeshCopyFlags : uint32
	{
		None = 0,
		WithSkeletalMesh = 1 << 1,
		WithSurfaces = 1 << 2,
		WithSkeleton = 1 << 3,
		WithPhysicsBody = 1 << 4,
		WithFaceGroups = 1 << 5,
		WithTags = 1 << 6,
		WithVertexBuffers = 1 << 7,
		WithIndexBuffers = 1 << 8,
		WithFaceBuffers = 1 << 9,
		WithAdditionalBuffers = 1 << 10,
		WithLayouts = 1 << 11,
		WithPoses = 1 << 12,
		WithBoneMap = 1 << 13,
		WithSkeletonIDs = 1 << 14,
		WithAdditionalPhysics = 1 << 15,
		WithStreamedResources = 1 << 16,

		AllFlags = 0xFFFFFFFF
	};
	
	ENUM_CLASS_FLAGS(EMeshCopyFlags);

    //! \brief Mesh object containing any number of buffers with any number of channels.
    //! The buffers can be per-index or per-vertex.
    //! The mesh also includes layout information for every texture channel for internal usage, and
    //! it can be ignored.
    //! The meshes are always assumed to be triangle list primitives.
    //! \ingroup runtime
    class MUTABLERUNTIME_API Mesh : public Resource
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------

        //! Deep clone this mesh.
        MeshPtr Clone() const;
		
		// Clone with flags allowing to not include some parts in the cloned mesh
		MeshPtr Clone(EMeshCopyFlags Flags) const;

		// Copy form another mesh.
		void CopyFrom(const Mesh& From, EMeshCopyFlags Flags = EMeshCopyFlags::AllFlags);

        //! Serialisation
        static void Serialise( const Mesh* p, OutputArchive& arch );
        static MeshPtr StaticUnserialise( InputArchive& arch );

		// Resource interface
		int32 GetDataSize() const override;

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
        void GetSurface( int32 surfaceIndex,
                         int32* FirstVertex, int32* VertexCount,
                         int32* FirstIndex, int32* IndexCount,
						 int32* FirstBone, int32* BoneCount,
						 bool* bCastShadow) const;

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

		int32 AddAdditionalPhysicsBody(Ptr<const PhysicsBody> Body);
		Ptr<const PhysicsBody> GetAdditionalPhysicsBody(int32 I) const;
		//int32 GetAdditionalPhysicsBodyExternalId(int32 I) const;

        //! \}

        //! \name Tags
        //! \{

        //!
        void SetTagCount( int count );

        //!
        int GetTagCount() const;

        //!
        const FString& GetTag( int tagIndex ) const;

        //!
        void SetTag( int tagIndex, const FString& Name );

		//!
		void AddStreamedResource(uint32 ResourceId);

		//!
		const TArray<uint32>& GetStreamedResources() const;

		//!
		int32 FindBonePose(uint16 BoneId) const;
		
		//!
		void SetBonePoseCount(int32 count);

		//!
		int32 GetBonePoseCount() const;

		//!
		void SetBonePose(int32 Index, uint16 BoneId, FTransform3f Transform, EBoneUsageFlags BoneUsageFlags);

		//! Return BoneId (uint16) of the pose at index PoseIndex or INDEX_NONE
		int32 GetBonePoseBoneId(int32 PoseIndex) const;

		//! Return a matrix stored per bone. It is a set of 16-float values.
		void GetBoneTransform(int32 BoneIndex, FTransform3f& Transform) const;

		//! 
		EBoneUsageFlags GetBoneUsageFlags(int32 BoneIndex) const;

		//! Set the bonemap of this mesh
		void SetBoneMap(const TArray<uint16>& InBoneMap);

		//! Return an array containing the bonemap indices of all surfaces in the mesh.
		const TArray<uint16>& GetBoneMap() const;

		//!
		int32 GetSkeletonIDsCount() const;

		//!
		int32 GetSkeletonID(int32 SkeletonIndex) const;

		//!
		void AddSkeletonID(int32 SkeletonID);

        //! \}


        //! Get an internal identifier used to reference this mesh in operations like deferred
        //! mesh building, or instance updating.
        uint32 GetId() const;


    protected:

        //! Forbidden. Manage with the Ptr<> template.
		~Mesh() {}

    
	public:

		template<typename Type>
		using TMemoryTrackedArray = TArray<Type, FDefaultMemoryTrackingAllocator<MemoryCounters::FMeshMemoryCounter>>;

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
		TArray<TPair<EMeshBufferType, FMeshBufferSet>> AdditionalBuffers;

		//! This is bit-mask on the STATIC_MESH_FORMATS enumeration, marking what static formats
		//! are compatible with this one. Usually precalculated at model compilation time.
		//! It should be reset after any operation that modifies the format.
		mutable uint32 m_staticFormatFlags = 0;

		TArray<MESH_SURFACE> m_surfaces;

		// Externally provided SkeletonIDs of the skeletons required by this mesh.
		TArray<uint32> SkeletonIDs;

		//! This skeleton and physics body are not owned and may be used by other meshes, so it cannot be modified
		//! once the mesh has been fully created.
		Ptr<const Skeleton> m_pSkeleton;
		Ptr<const PhysicsBody> m_pPhysicsBody;

		//! Additional physics bodies referenced by the mesh that don't merge.
		TArray<Ptr<const PhysicsBody>> AdditionalPhysicsBodies;

		//! Texture Layout blocks attached to this mesh. They are const because they could be shared with
		//! other meshes, so they need to be cloned and replaced if a modification is needed.
		TArray<Ptr<const Layout>> m_layouts;		

		//!
		TArray<FString> m_tags;

		// Opaque handle to external resources.
		TArray<uint32> StreamedResources;

		struct FBonePose
		{
			// Index of the bone in the CO BoneNames array
			uint16 BoneId;

			EBoneUsageFlags BoneUsageFlags = EBoneUsageFlags::None;
			FTransform3f BoneTransform;

			inline void Serialise(OutputArchive& arch) const;


			inline void Unserialise(InputArchive& arch);


			//!
			inline bool operator==(const FBonePose& Other) const
			{
				return BoneUsageFlags == Other.BoneUsageFlags && BoneId == Other.BoneId;
			}
		};
		// This is the pose used by this mesh fragment, used to update the transforms of the final skeleton
		// taking into consideration the meshes being used.
		TMemoryTrackedArray<FBonePose> BonePoses;

		// Array containing the bonemaps of all surfaces in the mesh.
		TArray<uint16> BoneMap;

		//!
		inline void Serialise(OutputArchive& arch) const;

        //!
		inline void Unserialise(InputArchive& arch);


        //!
		inline bool operator==(const Mesh& o) const
		{
			bool equal = (m_IndexBuffers == o.m_IndexBuffers);
			if (equal) equal = (m_VertexBuffers == o.m_VertexBuffers);
			if (equal) equal = (m_FaceBuffers == o.m_FaceBuffers);
			if (equal) equal = (m_layouts.Num() == o.m_layouts.Num());
			if (equal) equal = (BonePoses.Num() == o.BonePoses.Num());
			if (equal) equal = (BoneMap.Num() == o.BoneMap.Num());
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
			if (equal) equal = (StreamedResources == o.StreamedResources);
			if (equal) equal = (m_surfaces == o.m_surfaces);
			if (equal) equal = (m_tags == o.m_tags);
			if (equal) equal = (SkeletonIDs == o.SkeletonIDs);

			for (int32 i = 0; equal && i < m_layouts.Num(); ++i)
			{
				equal &= (*m_layouts[i]) == (*o.m_layouts[i]);
			}

			equal &= AdditionalBuffers.Num() == o.AdditionalBuffers.Num();
			for (int32 i = 0; equal && i < AdditionalBuffers.Num(); ++i)
			{
				equal &= AdditionalBuffers[i] == o.AdditionalBuffers[i];
			}

			equal &= BonePoses.Num() == o.BonePoses.Num();
			for (int32 i = 0; equal && i < BonePoses.Num(); ++i)
			{
				equal &= BonePoses[i] == o.BonePoses[i];
			}

			if (equal) equal = BoneMap == o.BoneMap;

			equal &= AdditionalPhysicsBodies.Num() == o.AdditionalPhysicsBodies.Num();
			for (int32 i = 0; equal && i < AdditionalPhysicsBodies.Num(); ++i)
			{
				equal &= *AdditionalPhysicsBodies[i] == *o.AdditionalPhysicsBodies[i];
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
		UE::Math::TIntVector3<uint32> GetFaceVertexIndices(int f) const;

		//! Return true if the given mesh has the same vertex and index formats, and in the same
		//! buffer structure.
		bool HasCompatibleFormat(const Mesh* pOther) const;

		//! Update the flags identifying the mesh format as some of the optimised formats.
		void ResetStaticFormatFlags() const;

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


	MUTABLE_DEFINE_ENUM_SERIALISABLE(EBoneUsageFlags)
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EMeshBufferType)
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EShapeBindingMethod)
	MUTABLE_DEFINE_ENUM_SERIALISABLE(EVertexColorUsage)
}

