// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithSketchUpCommon.h"

// SketchUp SDK.
#include "DatasmithSketchUpSDKBegins.h"
#include "SketchUpAPI/geometry.h"
#include "SketchUpAPI/model/uv_helper.h"
#include "DatasmithSketchUpSDKCeases.h"

// Datasmith SDK.
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"


namespace DatasmithSketchUp
{
	class FDatasmithInstantiatedMesh
	{
	public:
		TSharedPtr<IDatasmithMeshElement> DatasmithMesh;
		TMap<FMaterialIDType, int32> SlotIdForMaterialId;
		TMap<FLayerIDType, int32> SlotIdForLayerId;
		bool bIsUsingInheritedMaterial; // Whether mesh has faces without material assigned(so override/inherited material will apply)
	};
}


