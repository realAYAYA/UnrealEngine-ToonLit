// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuR/Mesh.h"

#include "Containers/UnrealString.h"
#include "HAL/LowLevelMemTracker.h"
#include "HAL/PlatformCrt.h"
#include "HAL/UnrealMemory.h"
#include "MuR/MeshPrivate.h"
#include "MuR/MutableTrace.h"

#include <limits>

namespace mu
{

//---------------------------------------------------------------------------------------------
void Mesh::Serialise( const Mesh* p, OutputArchive& arch )
{
    //p->m_pD->CheckIntegrity();
    arch << *p;
}


//---------------------------------------------------------------------------------------------
MeshPtr Mesh::StaticUnserialise( InputArchive& arch )
{
    MUTABLE_CPUPROFILER_SCOPE(MeshUnserialise)
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

    MeshPtr pResult = new Mesh();
    arch >> *pResult;

    //pResult->m_pD->CheckIntegrity();

    return pResult;
}


//---------------------------------------------------------------------------------------------
MeshPtr Mesh::Clone() const
{
    //MUTABLE_CPUPROFILER_SCOPE(MeshClone);
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

    MeshPtr pResult = new Mesh();

    pResult->m_internalId = m_internalId;
	pResult->m_staticFormatFlags = m_staticFormatFlags;
	pResult->m_surfaces = m_surfaces;
	pResult->m_pSkeleton = m_pSkeleton;
	pResult->m_pPhysicsBody = m_pPhysicsBody;
	pResult->m_faceGroups = m_faceGroups;
	pResult->m_tags = m_tags;

    // Clone the main buffers
    pResult->m_VertexBuffers = m_VertexBuffers;
    pResult->m_IndexBuffers = m_IndexBuffers;
    pResult->m_FaceBuffers = m_FaceBuffers;

	// Clone additional buffers
	pResult->m_AdditionalBuffers = m_AdditionalBuffers;

    // Clone the layouts
	pResult->m_layouts = m_layouts;

    // The skeleton is not cloned because it is not owned by this mesh and it is always assumed
    // to be shared.

	// physics body doen't need to be deep cloned either as they are also assumed to be shared.
	
	// Clone bone poses
	pResult->m_bonePoses = m_bonePoses;

    return pResult;
}

MeshPtr Mesh::Clone( EMeshCloneFlags Flags ) const
{
    //MUTABLE_CPUPROFILER_SCOPE(MeshClone);
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

    MeshPtr pResult = new Mesh();

    pResult->m_internalId = m_internalId;
	pResult->m_staticFormatFlags = m_staticFormatFlags;

	if ( EnumHasAnyFlags( Flags, EMeshCloneFlags::WithSurfaces ))
	{
		pResult->m_surfaces = m_surfaces;
	}

	if ( EnumHasAnyFlags( Flags, EMeshCloneFlags::WithSkeleton ))
	{
		pResult->m_pSkeleton = m_pSkeleton;
	}

	if ( EnumHasAnyFlags( Flags, EMeshCloneFlags::WithPhysicsBody ))
	{
		pResult->m_pPhysicsBody = m_pPhysicsBody;
	}

	if ( EnumHasAnyFlags( Flags, EMeshCloneFlags::WithFaceGroups ))
	{
		pResult->m_faceGroups = m_faceGroups;
	}

	if ( EnumHasAnyFlags( Flags, EMeshCloneFlags::WithTags ))
	{
		pResult->m_tags = m_tags;
	}

    // Clone the main buffers
	if ( EnumHasAnyFlags( Flags, EMeshCloneFlags::WithVertexBuffers ))
    {
		pResult->m_VertexBuffers = m_VertexBuffers;
	}

	if ( EnumHasAnyFlags( Flags, EMeshCloneFlags::WithIndexBuffers ))
    {
		pResult->m_IndexBuffers = m_IndexBuffers;
	}

	if ( EnumHasAnyFlags( Flags, EMeshCloneFlags::WithFaceBuffers ))
    {
		pResult->m_FaceBuffers = m_FaceBuffers;
	}

	// Clone additional buffers
	if ( EnumHasAnyFlags( Flags, EMeshCloneFlags::WithAdditionalBuffers ))
	{
		pResult->m_AdditionalBuffers = m_AdditionalBuffers;
	}

    // Clone the layout	
	if ( EnumHasAnyFlags( Flags, EMeshCloneFlags::WithLayouts ))
	{
		pResult->m_layouts = m_layouts;
	}
    // The skeleton is not cloned because it is not owned by this mesh and it is always assumed
    // to be shared.

	// physics body doen't need to be deep cloned either as they are also assumed to be shared.
	
	// Clone bone poses
	if (EnumHasAnyFlags(Flags, EMeshCloneFlags::WithPoses))
	{
		pResult->m_bonePoses = m_bonePoses;
	}

    return pResult;
}


//---------------------------------------------------------------------------------------------
uint32_t Mesh::GetId() const
{
    return m_internalId;
}


//---------------------------------------------------------------------------------------------
int Mesh::GetVertexCount() const
{
    return GetVertexBuffers().GetElementCount();
}


//---------------------------------------------------------------------------------------------
FMeshBufferSet& Mesh::GetVertexBuffers()
{
    return m_VertexBuffers;
}


//---------------------------------------------------------------------------------------------
const FMeshBufferSet& Mesh::GetVertexBuffers() const
{
    return m_VertexBuffers;
}


//---------------------------------------------------------------------------------------------
mu::Ptr<const Skeleton> Mesh::GetSkeleton() const
{
    return m_pSkeleton;
}


//---------------------------------------------------------------------------------------------
void Mesh::SetSkeleton( Ptr<const Skeleton> s )
{
    m_pSkeleton = s;
}

//---------------------------------------------------------------------------------------------
mu::Ptr<const PhysicsBody> Mesh::GetPhysicsBody() const
{
    return m_pPhysicsBody;
}


//---------------------------------------------------------------------------------------------
void Mesh::SetPhysicsBody( Ptr<const PhysicsBody> PhysicsBody )
{
    m_pPhysicsBody = PhysicsBody;
}

//---------------------------------------------------------------------------------------------
int Mesh::GetFaceCount() const
{
    int result = GetFaceBuffers().GetElementCount();
    if (result)
    {
        check(GetIndexBuffers().GetElementCount() == result * 3);
    }
    else
    {
        result = GetIndexBuffers().GetElementCount() / 3;
    }
    return result;
}


//---------------------------------------------------------------------------------------------
FMeshBufferSet& Mesh::GetFaceBuffers()
{
    return m_FaceBuffers;
}


//---------------------------------------------------------------------------------------------
const FMeshBufferSet& Mesh::GetFaceBuffers() const
{
    return m_FaceBuffers;
}


//---------------------------------------------------------------------------------------------
int Mesh::GetIndexCount() const
{
    // It is possible to ignore face buffers altogether
    //check( GetIndexBuffers().GetElementCount()==GetFaceBuffers().GetElementCount()*3 );
    return GetIndexBuffers().GetElementCount();
}


//---------------------------------------------------------------------------------------------
FMeshBufferSet& Mesh::GetIndexBuffers()
{
    return m_IndexBuffers;
}


//---------------------------------------------------------------------------------------------
const FMeshBufferSet& Mesh::GetIndexBuffers() const
{
    return m_IndexBuffers;
}


//---------------------------------------------------------------------------------------------
int Mesh::GetSurfaceCount() const
{
    return m_surfaces.Num();
}


//---------------------------------------------------------------------------------------------
void Mesh::GetSurface( int surfaceIndex,
                       int* firstVertex, int* vertexCount,
                       int* firstIndex, int* indexCount ) const
{
    int count = GetSurfaceCount();

    if (surfaceIndex>=0 && surfaceIndex<count)
    {
        if (surfaceIndex<m_surfaces.Num())
        {
            const MESH_SURFACE& surf = m_surfaces[surfaceIndex];
            if (firstVertex) *firstVertex = surf.m_firstVertex;
            if (vertexCount) *vertexCount = surf.m_vertexCount;
            if (firstIndex) *firstIndex = surf.m_firstIndex;
            if (indexCount) *indexCount = surf.m_indexCount;
        }
        else
        {
            // No surfaces defined, means only one surface using all the mesh
            if (firstVertex) *firstVertex = 0;
            if (vertexCount) *vertexCount = GetVertexCount();
            if (firstIndex) *firstIndex = 0;
            if (indexCount) *indexCount = GetIndexCount();
        }
    }
    else
    {
        check( false );
        if (firstVertex) *firstVertex = 0;
        if (vertexCount) *vertexCount = 0;
        if (firstIndex) *firstIndex = 0;
        if (indexCount) *indexCount = 0;
    }
}


//---------------------------------------------------------------------------------------------
uint32_t Mesh::GetSurfaceId( int surfaceIndex ) const
{
    if (surfaceIndex>=0 && surfaceIndex<m_surfaces.Num())
    {
        const MESH_SURFACE& surf = m_surfaces[surfaceIndex];
        return surf.m_id;
    }

    return 0;
}


//---------------------------------------------------------------------------------------------
void Mesh::AddLayout(Ptr<const Layout> pLayout )
{
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
    m_layouts.Add( pLayout );
}


//---------------------------------------------------------------------------------------------
int Mesh::GetLayoutCount() const
{
    return m_layouts.Num();
}


//---------------------------------------------------------------------------------------------
const Layout* Mesh::GetLayout( int i ) const
{
    check( i>=0 && i<m_layouts.Num() );

    return m_layouts[i].get();
}


//---------------------------------------------------------------------------------------------
void Mesh::SetLayout( int i, Ptr<const Layout> pLayout )
{
    check( i>=0 && i<m_layouts.Num() );

    m_layouts[i] = pLayout;
}


//---------------------------------------------------------------------------------------------
void Mesh::SetFaceGroupCount( int count )
{
    m_faceGroups.SetNum( count );
}


//---------------------------------------------------------------------------------------------
int Mesh::GetFaceGroupCount() const
{
    return m_faceGroups.Num();
}


//---------------------------------------------------------------------------------------------
const char* Mesh::GetFaceGroupName( int group ) const
{
    check( group>=0 && group<m_faceGroups.Num() );
    return m_faceGroups[group].m_name.c_str();
}


//---------------------------------------------------------------------------------------------
void Mesh::SetFaceGroupName( int group, const char* strName )
{
    check( group>=0 && group<m_faceGroups.Num() );
    m_faceGroups[group].m_name = strName;
}


//---------------------------------------------------------------------------------------------
int Mesh::GetFaceGroupFaceCount( int group ) const
{
    check( group>=0 && group<m_faceGroups.Num() );
    return m_faceGroups[group].m_faces.Num();
}


//---------------------------------------------------------------------------------------------
const int32* Mesh::GetFaceGroupFaces( int group ) const
{
    check( group>=0 && group<m_faceGroups.Num() );
    const int32* pResult = m_faceGroups[group].m_faces.GetData();
    return pResult;
}


//---------------------------------------------------------------------------------------------
void Mesh::SetFaceGroupFaces( int group, int count, const int32* faces )
{
    check( group>=0 && group<m_faceGroups.Num() );
    check( !count || faces );
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

    m_faceGroups[group].m_faces.SetNum( count );

    if (count)
    {
		FMemory::Memcpy( m_faceGroups[group].m_faces.GetData(), faces, count*sizeof(int32) );
    }

}


//---------------------------------------------------------------------------------------------
int Mesh::GetTagCount() const
{
    return m_tags.Num();
}


//---------------------------------------------------------------------------------------------
void Mesh::SetTagCount( int count )
{
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
    m_tags.SetNum( count );
}


//---------------------------------------------------------------------------------------------
const char* Mesh::GetTag( int tagIndex ) const
{
    check( tagIndex>=0 && tagIndex<GetTagCount() );

    if (tagIndex >= 0 && tagIndex < GetTagCount())
    {
        return m_tags[tagIndex].c_str();
    }
    else
    {
        return nullptr;
    }
}


//---------------------------------------------------------------------------------------------
void Mesh::SetTag( int tagIndex, const char* strName )
{
    check( tagIndex>=0 && tagIndex<GetTagCount() );
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));

    if (tagIndex >= 0 && tagIndex < GetTagCount())
    {
        m_tags[tagIndex] = strName ? strName : "";
    }
}


//---------------------------------------------------------------------------------------------
const char* Mesh::GetBonePoseName(int32 boneIndex) const
{
	check(boneIndex >= 0 && boneIndex < GetBonePoseCount());
	if (boneIndex >= 0 && boneIndex < GetBonePoseCount())
	{
		return m_bonePoses[boneIndex].m_boneName.c_str();
	}
	return "";
}


//---------------------------------------------------------------------------------------------
int32 Mesh::FindBonePose(const char* strName) const
{
	for (int32 i = 0; i < m_bonePoses.Num(); ++i)
	{
		if (m_bonePoses[i].m_boneName == strName)
		{
			return i;
		}
	}

	return INDEX_NONE;
}


//---------------------------------------------------------------------------------------------
void mu::Mesh::SetBonePoseCount(int32 count)
{
	LLM_SCOPE_BYNAME(TEXT("MutableRuntime"));
	m_bonePoses.SetNum(count);
}


//---------------------------------------------------------------------------------------------
int32 mu::Mesh::GetBonePoseCount() const
{
	return m_bonePoses.Num();
}


//---------------------------------------------------------------------------------------------
void mu::Mesh::SetBonePose(int32 boneIndex, const char* strName, FTransform3f transform, bool bSkinned)
{
	if (boneIndex >= 0 && boneIndex < m_bonePoses.Num())
	{
		FBonePose& pose = m_bonePoses[boneIndex];
		pose.m_boneName = strName;
		pose.m_boneTransform = transform;
		pose.m_boneSkinned = bSkinned;
	}
	else 
	{
		check(false);
	}
}


//---------------------------------------------------------------------------------------------
void mu::Mesh::GetBoneTransform(int32 boneIndex, FTransform3f& transform) const
{
	check(boneIndex >= 0 && boneIndex < m_bonePoses.Num());
	transform = boneIndex >= INDEX_NONE ? m_bonePoses[boneIndex].m_boneTransform : FTransform3f::Identity;
}


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
size_t Mesh::GetDataSize() const
{
    return m_IndexBuffers.GetDataSize()
            + m_VertexBuffers.GetDataSize()
            + m_FaceBuffers.GetDataSize();
}


//---------------------------------------------------------------------------------------------
bool Mesh::HasCompatibleFormat( const Mesh* pOther ) const
{
    bool compatible = true;

    compatible &= m_layouts.Num()==pOther->m_layouts.Num();
    compatible &= m_VertexBuffers.GetBufferCount()
            == pOther->m_VertexBuffers.GetBufferCount();


    // Indices
    //-----------------
    if ( m_IndexBuffers.GetElementCount()>0 && pOther->GetIndexCount()>0 )
    {
        check( m_IndexBuffers.m_buffers.Num() == 1 );
        check( pOther->GetIndexBuffers().m_buffers.Num() == 1 );
        check( m_IndexBuffers.GetBufferChannelCount(0) == 1 );
        check( pOther->GetIndexBuffers().GetBufferChannelCount(0) == 1 );

        const MESH_BUFFER& dest = m_IndexBuffers.m_buffers[0];
        const MESH_BUFFER& source = pOther->GetIndexBuffers().m_buffers[0];

        compatible &= dest.m_channels[0].m_format == source.m_channels[0].m_format;
    }


    // Layouts
    //-----------------
    // TODO?


    // Vertices
    //-----------------
    for ( int vb = 0; vb<m_VertexBuffers.GetBufferCount(); ++vb )
    {
        const MESH_BUFFER& dest = m_VertexBuffers.m_buffers[vb];
        const MESH_BUFFER& source = pOther->GetVertexBuffers().m_buffers[vb];

        // TODO: More checks about channels formats and semantics
        //compatible &= GetVertexBufferElementSize(vb) == pOther->GetVertexBufferElementSize(vb);
        compatible &= dest.m_channels.Num()==source.m_channels.Num();
    }

    return compatible;
}


//---------------------------------------------------------------------------------------------
vec3<uint32_t>  Mesh::GetFaceVertexIndices( int f ) const
{
    vec3<uint32_t> res;

    MeshBufferIteratorConst<MBF_UINT32,uint32_t,1> it( m_IndexBuffers, MBS_VERTEXINDEX );
    it += f*3;

    res[0] = (*it)[0];
    ++it;

    res[1] = (*it)[0];
    ++it;

    res[2] = (*it)[0];
    ++it;

    return res;
}


//---------------------------------------------------------------------------------------------
bool Mesh::HasFace
    (
        const Mesh& other,
        int otherFaceIndex,
        const VERTEX_MATCH_MAP& vertexMap
    ) const
{
    bool found = false;

    vec3<uint32_t> ov = other.GetFaceVertexIndices( otherFaceIndex );

    UntypedMeshBufferIteratorConst it( m_IndexBuffers, MBS_VERTEXINDEX );

    for ( int f=0; !found && f<m_FaceBuffers.GetElementCount(); f++ )
    {
        vec3<uint32_t> v;
        v[0] = it.GetAsUINT32(); ++it;
        v[1] = it.GetAsUINT32(); ++it;
        v[2] = it.GetAsUINT32(); ++it;

        found = true;
        for ( int vi=0; found && vi<3; ++vi )
        {
            found = vertexMap.Matches(v[vi],ov[0])
                 || vertexMap.Matches(v[vi],ov[1])
                 || vertexMap.Matches(v[vi],ov[2]);
        }
    }

    return found;
}


//---------------------------------------------------------------------------------------------
bool Mesh::IsSameVertex
    (
        uint32_t vertexIndex,
        const Mesh& other,
        uint32_t otherVertexIndex,
        float tolerance
    ) const
{
    bool same = true;

    // For all the attributes in this mesh
    for ( int b=0; same && b<m_VertexBuffers.GetBufferCount(); ++b )
    {
        for ( int c=0; same && c<m_VertexBuffers.GetBufferChannelCount(b); ++c )
        {
            MESH_BUFFER_SEMANTIC semantic;
            int semanticIndex = 0;
            MESH_BUFFER_FORMAT format;
            int components;
            int offset = 0;
            m_VertexBuffers.GetChannel( b, c, &semantic, &semanticIndex, &format, &components, &offset );

            // If it is not one of the relevant semantics
            if ( semantic!=MBS_POSITION &&
                    //semantic!=MBS_TEXCOORDS &&
                    //semantic!=MBS_NORMAL &&
                    //semantic!=MBS_TANGENT &&
                    //semantic!=MBS_BINORMAL &&
                    semantic!=MBS_BONEINDICES &&
                    semantic!=MBS_BONEWEIGHTS )
            {
                break;
            }

            // Find the channel in the other mesh
            int otherBuffer = -1;
            int otherChannel = -1;
            other.m_VertexBuffers.FindChannel( semantic, semanticIndex, &otherBuffer, &otherChannel );
            check( otherBuffer>=0 && otherChannel>=0 );

            MESH_BUFFER_SEMANTIC otherSemantic;
            int otherSemanticIndex = 0;
            MESH_BUFFER_FORMAT otherFormat;
            int otherComponents;
            int otherOffset;
            other.m_VertexBuffers.GetChannel
                    (
                        b, c,
                        &otherSemantic, &otherSemanticIndex,
                        &otherFormat, &otherComponents,
                        &otherOffset
                    );
            check( otherSemantic == semantic );
            check( otherFormat == format );
            check( otherComponents == components );

            int elemSize = m_VertexBuffers.GetElementSize( b );
            const uint8_t* pData = m_VertexBuffers.GetBufferData( b )
                    + elemSize*vertexIndex + offset;

            int otherElemSize = other.m_VertexBuffers.GetElementSize( b );
            const uint8_t* pOtherData = other.m_VertexBuffers.GetBufferData( b )
                    + otherElemSize*otherVertexIndex + otherOffset;

            switch (format)
            {
            case MBF_FLOAT32:
            {
                const float* pfData = (const float*)pData;
                const float* pfOtherData = (const float*)pOtherData;
                for ( int d=0; same && d<components; ++d )
                {
                    float diff = fabs( (*pfData) - (*pfOtherData) );
                    same = diff <= tolerance;
                    ++pfData;
                    ++pfOtherData;
                }
                break;
            }

            case MBF_UINT8:
            {
                for ( int d=0; same && d<components; ++d )
                {
                    same = (*pData) == (*pOtherData);
                    ++pData;
                    ++pOtherData;
                }
                break;
            }

            default:
                check( false );
            }
        }
    }

    return same;
}


//---------------------------------------------------------------------------------------------
bool Mesh::VERTEX_MATCH_MAP::Matches(int v, int ov) const
{
	if (v >= 0 && v < (int)m_firstMatch.Num())
	{
		int start = m_firstMatch[v];
		int end = v + 1 < m_firstMatch.Num() ? m_firstMatch[v + 1] : m_matches.Num();
		bool res = false;

		while (!res && start < end)
		{
			if (m_matches[start] == ov)
			{
				res = true;
			}
			++start;
		}

		return res;
	}

	return false;
}


//---------------------------------------------------------------------------------------------
void Mesh::GetVertexMap
    (
        const Mesh& other,
        VERTEX_MATCH_MAP& vertexMap,
        float tolerance
    ) const
{
    int vertexCount = m_VertexBuffers.GetElementCount();
    vertexMap.m_firstMatch.SetNum( vertexCount );
    vertexMap.m_matches.SetNum( vertexCount+(vertexCount>>2) );

    int otherVertexCount = other.m_VertexBuffers.GetElementCount();

    if ( !vertexCount || !otherVertexCount )
    {
        return;
    }


    MeshBufferIteratorConst< MBF_FLOAT32, float, 3 > itp( m_VertexBuffers, MBS_POSITION);
    MeshBufferIteratorConst< MBF_FLOAT32, float, 3 > itopBegin( other.m_VertexBuffers, MBS_POSITION);


    // Bucket the other mesh
#define MUTABLE_NUM_BUCKETS 256
#define MUTABLE_BUCKET_CHANNEL 0

    float rangeMin = std::numeric_limits<float>::max();
    float rangeMax = -std::numeric_limits<float>::max();
    MeshBufferIteratorConst< MBF_FLOAT32, float, 3 >  itop = itopBegin;
    for ( int ov=0; ov<otherVertexCount; ++ov )
    {
        float v = (*itop)[MUTABLE_BUCKET_CHANNEL];
        rangeMin = FMath::Min( rangeMin, v );
        rangeMax = FMath::Max( rangeMax, v );
        ++itop;
    }
    rangeMin -= tolerance;
    rangeMax += tolerance;

    TArray<int> buckets[MUTABLE_NUM_BUCKETS];
    for ( int b=0; b<MUTABLE_NUM_BUCKETS; ++b )
    {
        buckets[b].Reserve( otherVertexCount/MUTABLE_NUM_BUCKETS*2 );
    }

    float bucketSize = (rangeMax-rangeMin)/float(MUTABLE_NUM_BUCKETS);
    itop = itopBegin;
    for ( int ov=0; ov<otherVertexCount; ++ov )
    {
        float v = (*itop)[MUTABLE_BUCKET_CHANNEL];

        int bucket0 = int( floor( (v-tolerance-rangeMin)/bucketSize ) );
        bucket0 = FMath::Min( MUTABLE_NUM_BUCKETS-1, FMath::Max( 0, bucket0 ) );
        buckets[bucket0].Add(ov);

        int bucket1 = int( floor( (v+tolerance-rangeMin)/bucketSize ) );
        bucket1 = FMath::Min( MUTABLE_NUM_BUCKETS-1, FMath::Max( 0, bucket1 ) );

        if (bucket1!=bucket0)
        {
            buckets[bucket1].Add(ov);
        }

        ++itop;
    }


    // TODO Compare only positions?
    if (true)
    {

        /*
        // Don't use buckets
        for ( int v=0; v<vertexCount; ++v )
        {
            itop = itopBegin;

            vertexMap.m_firstMatch[v] = (int)vertexMap.m_matches.size();
            for ( int ov=0; ov<otherVertexCount; ++ov )
            {
                bool same = true;
                for ( int d=0; same && d<3; ++d )
                {
                    float diff = fabs( (*itp)[d] - (*itop)[d] );
                    same = diff <= tolerance;
                }

                if ( same )
                {
                    vertexMap.m_matches.push_back(ov);
                }

                ++itop;
            }

            ++itp;
        }
/*/
        // Use buckets
        for ( int v=0; v<vertexCount; ++v )
        {
            vertexMap.m_firstMatch[v] = (int)vertexMap.m_matches.Num();

            float vbucket = (*itp)[MUTABLE_BUCKET_CHANNEL];
            int bucket = int( floor( (vbucket-rangeMin)/bucketSize ) );

            if (bucket>=0 && bucket<MUTABLE_NUM_BUCKETS)
            {
                int bucketVertexCount = (int)buckets[bucket].Num();
                for ( int ov=0; ov<bucketVertexCount; ++ov )
                {
                    int otherVertexIndex = buckets[bucket][ov];
                    vec3<float> p = itopBegin[ otherVertexIndex ];

                    bool same = true;
                    for ( int d=0; same && d<3; ++d )
                    {
                        float diff = fabs( (*itp)[d] - p[d] );
                        same = diff <= tolerance;
                    }

                    if ( same )
                    {
                        vertexMap.m_matches.Add( otherVertexIndex );
                    }
                }
            }

            ++itp;
        }

    }
    else
    {
        // Slow generic way
        for ( int v=0; v<vertexCount; ++v )
        {
            vertexMap.m_firstMatch[v] = (int)vertexMap.m_matches.Num();
            for ( int ov=0; ov<otherVertexCount; ++ov )
            {
                // TODO: Optimize this by not using IsSameVertex
                if ( IsSameVertex( v, other, ov, tolerance ) )
                {
                    vertexMap.m_matches.Add(ov);
                }
            }
        }
    }

}


//---------------------------------------------------------------------------------------------
void Mesh::EnsureSurfaceData()
{
	if (!m_surfaces.Num() && m_VertexBuffers.GetElementCount())
	{
		MESH_SURFACE s;
		s.m_vertexCount = m_VertexBuffers.GetElementCount();
		s.m_indexCount = m_IndexBuffers.GetElementCount();
		m_surfaces.Add(s);
	}
}


//---------------------------------------------------------------------------------------------
void Mesh::ResetBufferIndices()
{
	m_VertexBuffers.ResetBufferIndices();
	m_IndexBuffers.ResetBufferIndices();
	m_FaceBuffers.ResetBufferIndices();
}


//---------------------------------------------------------------------------------------------
bool Mesh::IsSimilar(const Mesh& o, bool bCompareLayouts) const
{
	// Some meshes are just vertex indices (masks) we don't consider them for similarity,
	// because the kind of vertex channel data they store is the kind that is ignored.
	if (m_IndexBuffers.GetElementCount() == 0)
	{
		return false;
	}

	bool equal = m_IndexBuffers == o.m_IndexBuffers;
	if (equal) equal = m_FaceBuffers == o.m_FaceBuffers;
	if (equal && bCompareLayouts) equal = (m_layouts.Num() == o.m_layouts.Num());
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
	
	if (equal && m_pPhysicsBody != o.m_pPhysicsBody)
	{
		if (m_pPhysicsBody && o.m_pPhysicsBody)
		{
			equal = (*m_pPhysicsBody == *o.m_pPhysicsBody);
		}
		else
		{
			equal = false;
		}
	}

	if (equal) equal = (m_surfaces == o.m_surfaces);
	if (equal) equal = (m_faceGroups == o.m_faceGroups);
	if (equal) equal = (m_tags == o.m_tags);

	// Special comparison for layouts
	if (bCompareLayouts)
	{
		for (int32 i = 0; equal && i < m_layouts.Num(); ++i)
		{
			equal &= m_layouts[i]->IsSimilar(*o.m_layouts[i]);
		}
	}

	// Special comparison for vertex buffers
	if (equal)
	{
		equal = m_VertexBuffers.IsSimilarRobust(o.m_VertexBuffers, bCompareLayouts);
	}

	return equal;

}


//---------------------------------------------------------------------------------------------
void Mesh::ResetStaticFormatFlags() const
{
    m_staticFormatFlags = 0;

    for ( int f=0; f<SMF_COUNT; ++f )
    {
        if ( s_staticMeshFormatIdentify[f]
             &&
             s_staticMeshFormatIdentify[f]( this ) )
        {
            m_staticFormatFlags |= (1<<f);
        }
    }
}


//---------------------------------------------------------------------------------------------
void Mesh::CheckIntegrity() const
{
#ifdef MUTABLE_DEBUG

    // Check vertex indices
    {
        for ( int b=0; b<m_IndexBuffers.GetBufferCount(); ++b )
        {
            int elemSize = m_IndexBuffers.GetElementSize( b );

            for ( int c=0; c<m_IndexBuffers.GetBufferChannelCount(b); ++c )
            {
                MESH_BUFFER_SEMANTIC semantic;
                int semanticIndex = 0;
                MESH_BUFFER_FORMAT format;
                int components;
                int offset = 0;
                m_IndexBuffers.GetChannel
                        ( b, c, &semantic, &semanticIndex, &format, &components, &offset );

                if ( semantic==MBS_VERTEXINDEX )
                {
                    size_t icount = (size_t)m_IndexBuffers.GetElementCount();
                    int elemCount = m_VertexBuffers.GetElementCount();
                    for ( size_t indexIndex = 0; indexIndex < icount; ++indexIndex )
                    {
                        const uint8_t* pData = m_IndexBuffers.GetBufferData( b )
                                + elemSize*indexIndex + offset;

                        switch (format)
                        {
                        case MBF_UINT32:
                        {
                            uint32_t index = *(const uint32_t*)pData;
                            check( index < uint32_t( elemCount ) );
                            break;
                        }
                        case MBF_UINT16:
                        {
                            uint16 index = *(const uint16*)pData;
                            check( index < uint16( elemCount ) );
                            break;
                        }
                        case MBF_UINT8:
                        {
                            uint8_t index = *(const uint8_t*)pData;
                            check( index < uint8_t( elemCount ) );
                            break;
                        }
                        default:
                            check(false);
                            break;
                        }
                    }
                }
            }
        }
    }


    // Check bone indices, if there are bones. Bones could have been removed for later addition as an optimisation.
    // For all the attributes in this mesh
    int boneCount = m_pSkeleton ? m_pSkeleton->GetBoneCount() : 0;
    if ( m_pSkeleton && boneCount )
    {
        for ( int b = 0; b < m_VertexBuffers.GetBufferCount(); ++b )
        {
            int channelCount = m_VertexBuffers.GetBufferChannelCount( b );
            for ( int c = 0; c < channelCount; ++c )
            {
                MESH_BUFFER_SEMANTIC semantic;
                int semanticIndex = 0;
                MESH_BUFFER_FORMAT format;
                int components;
                int offset = 0;
                m_VertexBuffers.GetChannel
                        ( b, c, &semantic, &semanticIndex, &format, &components, &offset );

                // If it is not one of the relevant semantics
                if (
                        //semantic!=MBS_POSITION &&
                        //semantic!=MBS_TEXCOORDS &&
                        //semantic!=MBS_NORMAL &&
                        //semantic!=MBS_TANGENT &&
                        //semantic!=MBS_BINORMAL &&
                        semantic!=MBS_BONEINDICES
                        // && semantic!=MBS_BONEWEIGHTS
                        )
                {
                    continue;
                }

                size_t elemCount = (size_t)m_VertexBuffers.GetElementCount();
                int elemSize = m_VertexBuffers.GetElementSize( b );

                for ( size_t vertexIndex = 0;
                      vertexIndex < elemCount; ++vertexIndex )
                {
                    const uint8_t* pData = m_VertexBuffers.GetBufferData( b )
                            + elemSize*vertexIndex + offset;

                    switch (format)
                    {
                    case MBF_UINT8:
                    {
                        for ( int d = 0; d < components; ++d )
                        {
                            uint8_t index = *pData;
                            check( index < uint64_t(boneCount) );
                            ++pData;
                        }
                        break;
                    }

                    case MBF_UINT16:
                    {
                        for ( int d = 0; d < components; ++d )
                        {
                            uint16 index = *(uint16*)pData;
                            check( index < uint64_t( boneCount ) );
                            pData+=2;
                        }
                        break;
                    }

                    case MBF_UINT32:
                    {
                        for ( int d = 0; d < components; ++d )
                        {
                            uint32_t index = *(uint32_t*)pData;
                            check( index < uint64_t( boneCount ) );
                            pData+=4;
                        }
                        break;
                    }

                    default:
                        check( false );
                    }
                }
            }
        }
    }
#endif
}


//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
//---------------------------------------------------------------------------------------------
static bool StaticMeshFormatIdentify_None( const Mesh* )
{
    return false;
}

//---------------------------------------------------------------------------------------------
static bool StaticMeshFormatIdentify_Project( const Mesh* pM )
{
    // This format is used internally for the mesh project
    bool res = true;

    // The first vertex buffer must be texcoords(2f), position(3f), normal(3f)
    // all tightly packed
    res &= pM->m_VertexBuffers.GetBufferCount()>=1;

    if ( res )
    {
        res &= pM->m_VertexBuffers.m_buffers[0].m_channels.Num()==3;
    }

    if ( res )
    {
        const MESH_BUFFER_CHANNEL& chan =
                pM->m_VertexBuffers.m_buffers[0].m_channels[0];

        res &= chan.m_semantic == MBS_TEXCOORDS;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 2;
        //we don't really care about the semantic index
        //res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 0;
    }

    if ( res )
    {
        const MESH_BUFFER_CHANNEL& chan =
                pM->m_VertexBuffers.m_buffers[0].m_channels[1];

        res &= chan.m_semantic == MBS_POSITION;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 3;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 8;
    }

    if ( res )
    {
        const MESH_BUFFER_CHANNEL& chan =
                pM->m_VertexBuffers.m_buffers[0].m_channels[2];

        res &= chan.m_semantic == MBS_NORMAL;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 3;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 20;
    }

    // The first index buffer must be just index buffers u32
    if ( res )
    {
        res &= pM->m_IndexBuffers.m_buffers[0].m_channels.Num()>=1;
    }

    if ( res )
    {
        const MESH_BUFFER_CHANNEL& chan =
                pM->m_IndexBuffers.m_buffers[0].m_channels[0];

        res &= chan.m_semantic == MBS_VERTEXINDEX;
        res &= chan.m_format == MBF_UINT32;
        res &= chan.m_componentCount == 1;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 0;
    }

    return res;
}


//---------------------------------------------------------------------------------------------
static bool StaticMeshFormatIdentify_ProjectWrapping( const Mesh* pM )
{
    // This format is used internally for the mesh project
    bool res = true;

    // The first vertex buffer must be texcoords(2f), position(3f), normal(3f), layoutBlock(uint32_t)
    // all tightly packed
    res &= pM->m_VertexBuffers.GetBufferCount()>=1;

    if ( res )
    {
        res &= pM->m_VertexBuffers.m_buffers[0].m_channels.Num()==4;
    }

    if ( res )
    {
        const MESH_BUFFER_CHANNEL& chan = pM->m_VertexBuffers.m_buffers[0].m_channels[0];

        res &= chan.m_semantic == MBS_TEXCOORDS;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 2;
        // we don't really care about the semantic index as long as there is only one
        // res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 0;
    }

    if ( res )
    {
        const MESH_BUFFER_CHANNEL& chan = pM->m_VertexBuffers.m_buffers[0].m_channels[1];

        res &= chan.m_semantic == MBS_POSITION;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 3;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 8;
    }

    if ( res )
    {
        const MESH_BUFFER_CHANNEL& chan = pM->m_VertexBuffers.m_buffers[0].m_channels[2];

        res &= chan.m_semantic == MBS_NORMAL;
        res &= chan.m_format == MBF_FLOAT32;
        res &= chan.m_componentCount == 3;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 20;
    }

    if ( res )
    {
        const MESH_BUFFER_CHANNEL& chan = pM->m_VertexBuffers.m_buffers[0].m_channels[3];

        res &= chan.m_semantic == MBS_LAYOUTBLOCK;
        res &= chan.m_format == MBF_UINT32;
        res &= chan.m_componentCount == 1;
        // we don't really care about the semantic index as long as there is only one
        // res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 32;
    }

    // The first index buffer must be just index buffers u32
    if ( res )
    {
        res &= pM->m_IndexBuffers.m_buffers[0].m_channels.Num()>=1;
    }

    if ( res )
    {
        const MESH_BUFFER_CHANNEL& chan = pM->m_IndexBuffers.m_buffers[0].m_channels[0];

        res &= chan.m_semantic == MBS_VERTEXINDEX;
        res &= chan.m_format == MBF_UINT32;
        res &= chan.m_componentCount == 1;
        res &= chan.m_semanticIndex == 0;
        res &= chan.m_offset == 0;
    }

    return res;
}


//---------------------------------------------------------------------------------------------
STATIC_MESH_FORMAT_ID_FUNC s_staticMeshFormatIdentify[] =
{
    StaticMeshFormatIdentify_None,
    StaticMeshFormatIdentify_Project,
    StaticMeshFormatIdentify_ProjectWrapping
};


//---------------------------------------------------------------------------------------------
namespace
{
    void LogBuffer( FString& out, const FMeshBufferSet& bufset, int32 BufferElementLimit)
    {
        (void)out;
        (void)bufset;

        size_t elemCount = bufset.m_elementCount;
        out += "  Set with "
                + FString::Printf(TEXT("%d"), bufset.m_buffers.Num())
                + " buffers and "
                + FString::Printf(TEXT("%d"), elemCount)
                + " elements.\n";

        for( const auto& buf : bufset.m_buffers )
        {
            const uint8_t* pData = buf.m_data.GetData();

            out += "    Buffer with "+ FString::Printf(TEXT("%d"), buf.m_channels.Num())
                    + " channels and "+ FString::Printf(TEXT("%d"), buf.m_elementSize)+" elementsize\n";
            for( const auto& chan : buf.m_channels )
            {
                out += "      Channel with format: "+ FString::Printf(TEXT("%d"), chan.m_format)
                        + " semantic: "+ FString::Printf(TEXT("%d"), chan.m_semantic)
                        + " " + FString::Printf(TEXT("%d"), chan.m_semanticIndex)
                        + " components: " + FString::Printf(TEXT("%d"), chan.m_componentCount)
                        + " offset: " + FString::Printf(TEXT("%d"), chan.m_offset)+"\n";
                for( size_t e=0; e<elemCount && e<BufferElementLimit; ++e )
                {
                    const uint8_t* pElementData = pData+buf.m_elementSize*e;
                    const uint8_t* pChanData = pElementData+chan.m_offset;
                    out += "        ";
                    for (int c=0; c<chan.m_componentCount; ++c)
                    {
                        out += "\t";
                        switch (chan.m_format)
                        {
                        case MBF_UINT32:
                        case MBF_NUINT32: out += FString::Printf(TEXT("%d"), *(const uint32_t*)pChanData); pChanData +=4; break;
                        case MBF_UINT16:
                        case MBF_NUINT16: out += FString::Printf(TEXT("%d"), *(const uint16*)pChanData); pChanData +=2; break;
                        case MBF_UINT8:
                        case MBF_NUINT8: out += FString::Printf(TEXT("%d"), *(const uint8_t*)pChanData); pChanData +=1; break;
						case MBF_FLOAT32: out += FString::Printf(TEXT("%.3f"), *(const float*)pChanData); pChanData += 4; break;
                        case MBF_FLOAT16: out += FString::Printf(TEXT("%d"), *(const uint16*)pChanData); pChanData +=2; break;
                        default: break;
                        }
                        out += ",";
                    }
                    out += "\n";
                }
            }
        }
    }
}


void Mesh::Log( FString& out, int32 BufferElementLimit)
{
    out += "Mesh:\n";

    out += "Indices:\n";
    LogBuffer( out, m_IndexBuffers, BufferElementLimit);

    out += "Vertices:\n";
    LogBuffer( out, m_VertexBuffers, BufferElementLimit);

    out += "Faces:\n";
    LogBuffer( out, m_FaceBuffers, BufferElementLimit);
}

}
