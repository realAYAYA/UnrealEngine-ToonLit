// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TargetInterfaces/DynamicMeshCommitter.h"

#include "GeometryBase.h"

PREDECLARE_GEOMETRY(class FDynamicMesh3);
class IMeshDescriptionCommitter;
class IMeshDescriptionProvider;
struct FMeshDescription;

namespace UE {
namespace Geometry {

	MODELINGCOMPONENTS_API FDynamicMesh3 GetDynamicMeshViaMeshDescription(
		IMeshDescriptionProvider& MeshDescriptionProvider);

	MODELINGCOMPONENTS_API void CommitDynamicMeshViaMeshDescription(
		FMeshDescription&& CurrentMeshDescription,
		IMeshDescriptionCommitter& MeshDescriptionCommitter,
		const UE::Geometry::FDynamicMesh3& Mesh, 
		const IDynamicMeshCommitter::FDynamicMeshCommitInfo& CommitInfo);

}}