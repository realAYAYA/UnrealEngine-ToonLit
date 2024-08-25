// Copyright Epic Games, Inc. All Rights Reserved.

#include "TargetInterfaces/DynamicMeshProvider.h"
#include "DynamicMesh/DynamicMesh3.h"

#define LOCTEXT_NAMESPACE "DynamicMeshProvider"

using namespace UE::Geometry;

FDynamicMesh3 IDynamicMeshProvider::GetDynamicMesh(bool bRequestTangents)
{
	return GetDynamicMesh();
}

#undef LOCTEXT_NAMESPACE 