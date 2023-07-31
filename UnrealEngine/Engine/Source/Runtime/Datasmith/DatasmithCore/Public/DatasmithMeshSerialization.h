// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithCore.h"

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "MeshDescription.h"
#include "Misc/SecureHash.h"
#include "DatasmithCloth.h"

class FArchive;


struct DATASMITHCORE_API FDatasmithMeshModels
{
	FString MeshName;
	bool bIsCollisionMesh = false;
	TArray<FMeshDescription> SourceModels;

	friend FArchive& operator<<(FArchive& Ar, FDatasmithMeshModels& Models);
};

struct DATASMITHCORE_API FDatasmithPackedMeshes
{
	TArray<FDatasmithMeshModels> Meshes;

	FMD5Hash Serialize(FArchive& Ar, bool bSaveCompressed=true);
};

DATASMITHCORE_API FDatasmithPackedMeshes GetDatasmithMeshFromFile(const FString& MeshPath);



struct DATASMITHCORE_API FDatasmithClothInfo
{
	FDatasmithCloth Cloth;
	friend FArchive& operator<<(FArchive& Ar, FDatasmithClothInfo& Info);
};

struct DATASMITHCORE_API FDatasmithPackedCloths
{
	TArray<FDatasmithClothInfo> ClothInfos;

	FMD5Hash Serialize(FArchive& Ar, bool bSaveCompressed=true);
};


DATASMITHCORE_API FDatasmithPackedCloths GetDatasmithClothFromFile(const FString& Path);

