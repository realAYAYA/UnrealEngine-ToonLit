// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Selection/GeometrySelector.h"
#include "Selection/DynamicMeshSelector.h"

#include "UObject/StrongObjectPtr.h"


class UDynamicMesh;
class UStaticMesh;
class UStaticMeshComponent;


class MODELINGCOMPONENTSEDITORONLY_API FStaticMeshSelector : public FBaseDynamicMeshSelector
{
public:
	using FBaseDynamicMeshSelector::Initialize;

	virtual bool Initialize(
		FGeometryIdentifier SourceGeometryIdentifier);

	//
	// IGeometrySelector API implementation
	//

	virtual void Shutdown() override;
	// disable sleep on StaticMesh Selector until we can properly track changes...
	virtual bool SupportsSleep() const override { return false; }
	//virtual bool Sleep();
	//virtual bool Restore();



	virtual IGeometrySelectionTransformer* InitializeTransformation(const FGeometrySelection& Selection) override;
	virtual void ShutdownTransformation(IGeometrySelectionTransformer* Transformer) override;

protected:
	UStaticMeshComponent* StaticMeshComponent = nullptr;
	UStaticMesh* StaticMesh = nullptr;
	FDelegateHandle StaticMesh_OnMeshChangedHandle;

	// TODO: this is not a great design, it would be better if something external could own this mesh...
	TStrongObjectPtr<UDynamicMesh> LocalTargetMesh;
	void CopyFromStaticMesh();

	TPimplPtr<FBasicDynamicMeshSelectionTransformer> ActiveTransformer;
	void CommitMeshTransform();
};



/**
 * FStaticMeshComponentSelectorFactory constructs FStaticMeshSelector instances 
 * for UBrushComponents
 */
class MODELINGCOMPONENTSEDITORONLY_API FStaticMeshComponentSelectorFactory : public IGeometrySelectorFactory
{
public:
	virtual bool CanBuildForTarget(FGeometryIdentifier TargetIdentifier) const;
	virtual TUniquePtr<IGeometrySelector> BuildForTarget(FGeometryIdentifier TargetIdentifier) const;
};




