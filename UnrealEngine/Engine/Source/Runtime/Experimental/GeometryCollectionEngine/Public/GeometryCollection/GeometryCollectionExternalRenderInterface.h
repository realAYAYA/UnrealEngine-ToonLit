// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"

#include "GeometryCollectionExternalRenderInterface.generated.h"

class UGeometryCollection;
class UGeometryCollectionComponent;

UINTERFACE(MinimalAPI)
class UGeometryCollectionExternalRenderInterface : public UInterface
{
	GENERATED_BODY()
};

class IGeometryCollectionExternalRenderInterface
{
	GENERATED_BODY()

public:
	virtual void OnRegisterGeometryCollection(UGeometryCollectionComponent const& InComponent) = 0;
	virtual void OnUnregisterGeometryCollection() = 0;
	virtual void UpdateState(UGeometryCollection const& InGeometryCollection, FTransform const& InComponentTransform, bool bInIsBroken, bool bInIsVisible) = 0;
	virtual void UpdateRootTransform(UGeometryCollection const& InGeometryCollection, FTransform const& InRootTransform) = 0;
	virtual void UpdateTransforms(UGeometryCollection const& InGeometryCollection, TArrayView<const FTransform> InTransforms) = 0;

	UE_DEPRECATED(5.3, "Use FTransform version of UpdateTransforms instead")
	virtual void UpdateTransforms(UGeometryCollection const& InGeometryCollection, TArrayView<const FMatrix> InMatrices) {};
};
