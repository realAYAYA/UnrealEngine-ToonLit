// Copyright Epic Games, Inc. All Rights Reserved. 

#if WITH_EDITOR
#include "Dataflow/CollectionPatternToDynamicMesh.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/NonManifoldMappingSupport.h"
#include "Engine/SkeletalMesh.h"
#include "SkeletalMeshAttributes.h"
#include "ToDynamicMesh.h"

namespace Dataflow
{

	void FCollectionPatternToDynamicMesh::Convert(const FManagedArrayCollection* FleshCollection, int32 PatternIndex, EDataflowPatternVertexType VertexDataType, UE::Geometry::FDynamicMesh3& MeshOut, bool bDisableAttributes)
	{
		// @todo(brice)(do the conversion)
		check(0);
	}

	void FCollectionPatternToDynamicMesh::Convert(const UObject* FleshAssetIn, int32 LODIndex, int32 PatternIndex, EDataflowPatternVertexType VertexDataType, UE::Geometry::FDynamicMesh3& MeshOut)
	{
		// @todo(brice)(make this generic)
		//Convert(FleshAssetIn->GetCollection(), PatternIndex, VertexDataType, MeshOut);
	}

}	// namespace Dataflow

#else

namespace Dataflow
{

	void FCollectionPatternToDynamicMesh::Convert(const FManagedArrayCollection* FleshCollection, int32 PatternIndex, EDataflowPatternVertexType VertexDataType, UE::Geometry::FDynamicMesh3& MeshOut, bool bDisableAttributes)
	{
		// Conversion only supported with editor.
		check(0);
	}

	void FCollectionPatternToDynamicMesh::Convert(const UObject* AssetIn, int32 LODIndex, int32 PatternIndex, EDataflowPatternVertexType VertexDataType, UE::Geometry::FDynamicMesh3& MeshOut)
	{
		// Conversion only supported with editor.
		check(0);
	}

}	// namespace Dataflow

#endif  // end with editor

