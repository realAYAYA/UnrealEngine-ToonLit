// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "CADOptions.h"

#ifdef USE_OPENMODEL

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif

#include "AlShadingFields.h"
#include "AlDagNode.h"
#include "AlPersistentID.h"

#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif

class IDatasmithActorElement;
class AlMesh;

struct FMeshDescription;

typedef double AlMatrix4x4[4][4];

namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{

enum class ETesselatorType : uint8
{
	Fast,
	Accurate,
};

enum class EAlShaderModelType : uint8
{
	BLINN,
	LAMBERT,
	LIGHTSOURCE,
	PHONG,
};

namespace OpenModelUtils
{
	void SetActorTransform(TSharedPtr<IDatasmithActorElement>& OutActorElement, const AlDagNode& InDagNode);

	bool IsValidActor(const TSharedPtr<IDatasmithActorElement>& ActorElement);

	inline FString UuidToString(const uint32& Uuid)
	{
		return FString::Printf(TEXT("0x%08x"), Uuid);
	}

	inline uint32 GetTypeHash(AlPersistentID& GroupNodeId)
	{
		int IdA, IdB, IdC, IdD;
		GroupNodeId.id(IdA, IdB, IdC, IdD);
		return HashCombine(IdA, HashCombine(IdB, HashCombine(IdC, IdD)));
	}

	inline uint32 GetAlDagNodeUuid(AlDagNode& DagNode)
	{
		if (DagNode.hasPersistentID() == sSuccess)
		{
			AlPersistentID* PersistentID;
			DagNode.persistentID(PersistentID);
			return GetTypeHash(*PersistentID);
		}
		FString Label = UTF8_TO_TCHAR(DagNode.name());
		return GetTypeHash(Label);
	}

	bool TransferAlMeshToMeshDescription(const AlMesh& Mesh, const TCHAR* SlotMaterialName, FMeshDescription& MeshDescription, CADLibrary::FMeshParameters& SymmetricParameters, const bool bMerge = false);

	TSharedPtr<AlDagNode> TesselateDagLeaf(const AlDagNode& DagLeaf, ETesselatorType TessType, double Tolerance);
}

}

#endif


