// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h" // IWYU pragma: keep

class FGeometryCollection;
struct FManagedArrayCollection;

class FRACTUREENGINE_API FFractureEngineEdit
{
public:

	static void DeleteBranch(FGeometryCollection& GeometryCollection, const TArray<int32>& InBoneSelection);

	static void SetVisibilityInCollectionFromTransformSelection(FManagedArrayCollection& InCollection, const TArray<int32>& InTransformSelection, bool bVisible);

	static void SetVisibilityInCollectionFromFaceSelection(FManagedArrayCollection& InCollection, const TArray<int32>& InFaceSelection, bool bVisible);

	static void Merge(FGeometryCollection& GeometryCollection, const TArray<int32>& InBoneSelection);

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Containers/Array.h"
#include "Dataflow/DataflowSelection.h"
#include "GeometryCollection/GeometryCollection.h"
#endif
