// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithCloth.h"
#include "MeshDescription.h"


struct FDatasmithPayload
{
	TArray<class UDatasmithAdditionalData*> AdditionalData;
};


struct FDatasmithMeshElementPayload : public FDatasmithPayload
{
	TArray<FMeshDescription> LodMeshes;
	FMeshDescription CollisionMesh;
	TArray<FVector3f> CollisionPointCloud; // compatibility, favor the CollisionMesh member
};


/**
 * Describes a Cloth element payload, which is the actual data to be imported.
 */
struct FDatasmithClothElementPayload : public FDatasmithPayload
{
	FDatasmithCloth Cloth;
};


struct FDatasmithLevelSequencePayload : public FDatasmithPayload
{
// #ueent_todo: split element in metadata/payload
};
