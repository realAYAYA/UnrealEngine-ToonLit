// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class IMeshPaintComponentAdapter;
class UMeshComponent;
class FReferenceCollector;

/**
 * Factory for IMeshPaintGeometryAdapter
 */
class MESHPAINTINGTOOLSET_API IMeshPaintComponentAdapterFactory
{
public:
	virtual TSharedPtr<IMeshPaintComponentAdapter> Construct(UMeshComponent* InComponent, int32 InMeshLODIndex) const = 0;
	virtual void InitializeAdapterGlobals() = 0;
	virtual void AddReferencedObjectsGlobals(FReferenceCollector& Collector) = 0;
	virtual void CleanupGlobals() = 0;
	virtual ~IMeshPaintComponentAdapterFactory() {}
};