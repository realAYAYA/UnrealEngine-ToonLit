// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Selection/GeometrySelector.h"
#include "Selection/DynamicMeshSelector.h"

#include "UObject/StrongObjectPtr.h"
#include "EditorUndoClient.h"

class UDynamicMesh;
class AVolume;
class UBrushComponent;


class MODELINGCOMPONENTSEDITORONLY_API FVolumeSelector : public FBaseDynamicMeshSelector, public FEditorUndoClient
{
public:
	using FBaseDynamicMeshSelector::Initialize;

	virtual bool Initialize(
		FGeometryIdentifier SourceGeometryIdentifier);

	//
	// IGeometrySelector API implementation
	//

	virtual void Shutdown() override;
	// disable sleep on VolumeSelector until we can properly track changes...
	virtual bool SupportsSleep() const override { return false; }
	//virtual bool Sleep();
	//virtual bool Restore();

	virtual bool IsLockable() const override;
	virtual bool IsLocked() const override;
	virtual void SetLockedState(bool bLocked) override;

	virtual IGeometrySelectionTransformer* InitializeTransformation(const FGeometrySelection& Selection) override;
	virtual void ShutdownTransformation(IGeometrySelectionTransformer* Transformer) override;

	virtual void UpdateAfterGeometryEdit(
		IToolsContextTransactionsAPI* TransactionsAPI,
		bool bInTransaction,
		TUniquePtr<UE::Geometry::FDynamicMeshChange> DynamicMeshChange,
		FText GeometryEditTransactionString) override;


	// FEditorUndoClient implementation
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

public:
	static void SetComponentUnlockedOnCreation(UBrushComponent* Component);
	static void ResetUnlockedBrushComponents();

protected:
	AVolume* ParentVolume = nullptr;
	UBrushComponent* BrushComponent = nullptr;

	// TODO: this is not a great design, it would be better if something external could own this mesh...
	TStrongObjectPtr<UDynamicMesh> LocalTargetMesh;

	void UpdateDynamicMeshFromVolume();

	TSharedPtr<FBasicDynamicMeshSelectionTransformer> ActiveTransformer;
	void CommitMeshTransform();
};




/**
 * FVolumeComponentSelectorFactory constructs FVolumeSelector instances 
 * for UBrushComponents
 */
class MODELINGCOMPONENTSEDITORONLY_API FBrushComponentSelectorFactory : public IGeometrySelectorFactory
{
public:
	virtual bool CanBuildForTarget(FGeometryIdentifier TargetIdentifier) const;
	virtual TUniquePtr<IGeometrySelector> BuildForTarget(FGeometryIdentifier TargetIdentifier) const;
};




