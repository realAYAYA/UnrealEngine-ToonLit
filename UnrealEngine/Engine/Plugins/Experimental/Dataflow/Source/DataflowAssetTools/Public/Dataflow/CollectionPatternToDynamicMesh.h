// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dataflow/DataflowContent.h"
#include "Templates/SharedPointer.h"


namespace UE::Geometry { class FDynamicMesh3; }
struct FManagedArrayCollection;

namespace Dataflow
{

/**
* Convert a single pattern from a colleciton asset to a FDynamicMesh3. 
* When PatternIndex = INDEX_NONE, convert the entire collection asset to an FDynamicMesh3.
* When VertexDataType = EDataflowPatternVertexType::Sim3D, this generates a welded mesh (with native welded indexing) when PatternIndex = INDEX_NONE,
*	and an unwelded (with native 2D unwelded indexing) when PatternIndex is a valid SimPattern Index.
*/
class DATAFLOWASSETTOOLS_API FCollectionPatternToDynamicMesh
{
public:

	void Convert(const FManagedArrayCollection* FleshCollection, int32 PatternIndex, EDataflowPatternVertexType VertexDataType, UE::Geometry::FDynamicMesh3& MeshOut, bool bDisableAttributes = false);

	void Convert(const UObject* AssetIn, int32 LODIndex, int32 PatternIndex, EDataflowPatternVertexType VertexDataType, UE::Geometry::FDynamicMesh3& MeshOut);
};

}	// namespace Dataflow

