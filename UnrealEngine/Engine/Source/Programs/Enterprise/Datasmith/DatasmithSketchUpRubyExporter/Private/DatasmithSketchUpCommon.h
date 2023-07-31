// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// SketchUp SDK.
#include "DatasmithSketchUpSDKBegins.h"
#include "SketchUpAPI/model/defs.h"
#include "SketchUpAPI/transformation.h"
#include "DatasmithSketchUpSDKCeases.h"

#include "Templates/TypeHash.h"

class FDatasmithMesh;

class IDatasmithActorElement;
class IDatasmithBaseMaterialElement;
class IDatasmithCameraActorElement;
class IDatasmithMeshActorElement;
class IDatasmithMeshElement;
class IDatasmithMetaDataElement;
class IDatasmithScene;

namespace DatasmithSketchUp
{

	struct FEntityIDType
	{
		int32 EntityID;

		FEntityIDType() : EntityID(0) {} // todo: check that 0 is the 'undefined'
		explicit FEntityIDType(int32 InEntityID) : EntityID(InEntityID) {}
	};

	typedef FEntityIDType FComponentDefinitionIDType;
	typedef FEntityIDType FComponentInstanceIDType;
	typedef FEntityIDType FGroupIDType;
	typedef FEntityIDType FMaterialIDType;
	typedef FEntityIDType FSceneIDType;
	typedef FEntityIDType FTextureIDType;
	typedef FEntityIDType FLayerIDType;

	static FORCEINLINE uint32 GetTypeHash(FEntityIDType Key)
	{
		return ::GetTypeHash(Key.EntityID);
	}

	static FORCEINLINE bool operator==(FEntityIDType A, FEntityIDType B)
	{
		return A.EntityID == B.EntityID;
	}

	static FORCEINLINE bool operator!=(FEntityIDType A, FEntityIDType B)
	{
		return A.EntityID != B.EntityID;
	}

}
