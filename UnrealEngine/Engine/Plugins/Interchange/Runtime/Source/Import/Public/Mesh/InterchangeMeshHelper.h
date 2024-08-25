// Copyright Epic Games, Inc. All Rights Reserved. 
#include "MeshDescription.h"
#include "StaticMeshOperations.h"

namespace UE::Interchange::Private::MeshHelper
{
	INTERCHANGEIMPORT_API void RemapPolygonGroups(const FMeshDescription& SourceMesh, FMeshDescription& TargetMesh, PolygonGroupMap& RemapPolygonGroup);
} //ns UE::Interchange::Private::MeshHelper