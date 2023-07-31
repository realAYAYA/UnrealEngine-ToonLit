// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "MeshPaintModule.h"
#include "Templates/SharedPointer.h"

class FReferenceCollector;
class IMeshPaintGeometryAdapterFactory;
class UMeshComponent;

class MESHPAINT_API FMeshPaintAdapterFactory
{
public:
	static TArray<TSharedPtr<IMeshPaintGeometryAdapterFactory>> FactoryList;

public:
	static TSharedPtr<class IMeshPaintGeometryAdapter> CreateAdapterForMesh(UMeshComponent* InComponent, int32 InPaintingMeshLODIndex);
	static void InitializeAdapterGlobals();
	static void AddReferencedObjectsGlobals(FReferenceCollector& Collector);
	static void CleanupGlobals();
};
