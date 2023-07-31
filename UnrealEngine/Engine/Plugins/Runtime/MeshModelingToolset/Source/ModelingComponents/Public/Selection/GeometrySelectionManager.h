// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Selections/GeometrySelection.h"
#include "Selection/GeometrySelector.h"
#include "Selection/GeometrySelectionChanges.h"

#include "GeometrySelectionManager.generated.h"

class IGeometrySelector;
class UInteractiveToolsContext;
class IToolsContextRenderAPI;
class IToolsContextTransactionsAPI;
class UPersistentMeshSelection;
class UGeometrySelectionEditCommand;
class UGeometrySelectionEditCommandArguments;


/**
 * UGeometrySelectionManager provides the infrastructure for "Element Selection", ie 
 * geometric sub-elements of some geometry object like a Triangle Mesh. The Manager is
 * designed to work with a relatively vague concept of "element", so it doesn't explicitly
 * reference triangles/etc, and the selectable-elements and how-elements-are-selected
 * concepts are provided by abstract-interfaces that allow various implememtations.
 * 
 * The "Geometry Objects", eg like a DynamicMeshComponent, Gameplay Volume, etc, are
 * referred to as "Active Targets" in the Manager. External code provides and updates
 * the set of Active Targets, eg for example tracking the active Actor Selection in the Editor.
 * 
 * For a given Target, a tuple (Selector, Selection, SelectionEditor) is created and maintained.
 * The FGeometrySelection is ultimately a basic list of integers and does not have any knowledge
 * of what it is a selection *of*, and is not intended to be directly edited. Instead the
 * SelectionEditor provides that functionality. This separation allows "selection editing" to
 * be customized, eg to enforce invariants or constraints that might apply to certain kinds of selections.
 * 
 * The IGeometrySelector provides the core implementation of what "selection" means for a given
 * Target, eg like a mesh Component, or mesh object like a UDynamicMesh. The Selector is
 * created by a registered Factory, allowing client code to provide custom implementations for
 * different Target Types. Updates to the Selection are done via the Selector, as well as queries
 * about (eg) renderable selection geometry. 3D Transforms are also applied via the Selector,
 * as only it has the knowledge about what can be transformed and how it can be applied.
 * 
 * The GeometrySelectionManager provides high-level interfaces for this system, for example
 * external code (eg such as something that creates a Gizmo for the active selection) only
 * needs to interact with SelectionManager, calling functions like 
 * ::BeginTransformation() / ::UpdateTransformation() / ::EndTransformation().
 * The SelectionManager also handles Transactions/FChanges for the active Targets and Selections. 
 * 
 */
UCLASS()
class MODELINGCOMPONENTS_API UGeometrySelectionManager : public UObject
{
	GENERATED_BODY()

public:

	using FGeometrySelection = UE::Geometry::FGeometrySelection;
	using FGeometrySelectionEditor = UE::Geometry::FGeometrySelectionEditor;
	using FGeometrySelectionBounds = UE::Geometry::FGeometrySelectionBounds;
	using FGeometrySelectionElements = UE::Geometry::FGeometrySelectionElements;
	using FGeometrySelectionUpdateConfig = UE::Geometry::FGeometrySelectionUpdateConfig;
	using FGeometrySelectionUpdateResult = UE::Geometry::FGeometrySelectionUpdateResult;
	using EGeometryElementType = UE::Geometry::EGeometryElementType;
	using EGeometryTopologyType = UE::Geometry::EGeometryTopologyType;

	//
	// Setup/Teardown
	//

	virtual void Initialize(UInteractiveToolsContext* ToolsContextIn, IToolsContextTransactionsAPI* TransactionsAPIIn);

	virtual void RegisterSelectorFactory(TUniquePtr<IGeometrySelectorFactory> Factory);

	virtual void Shutdown();
	virtual bool HasBeenShutDown() const;

	UInteractiveToolsContext* GetToolsContext() const { return ToolsContext; }
	IToolsContextTransactionsAPI* GetTransactionsAPI() const { return TransactionsAPI; }

	//
	// Configuration
	//

	/** EMeshTopologyMode determines what level of mesh element will be selected  */
	enum class EMeshTopologyMode
	{
		None = 0,
		/** Select mesh triangles, edges, and vertices */
		Triangle = 1,
		/** Select mesh polygroups, polygroup-borders, and polygroup-corners */
		Polygroup = 2
	};

	virtual void SetSelectionElementType(EGeometryElementType ElementType);
	virtual EGeometryElementType GetSelectionElementType() const { return SelectionElementType; }

	virtual void SetMeshTopologyMode(EMeshTopologyMode SelectionMode);
	virtual EMeshTopologyMode GetMeshTopologyMode() const { return MeshTopologyMode; }
	virtual EGeometryTopologyType GetSelectionTopologyType() const;

	


	//
	// Target Management / Queries
	// TODO: be able to update active target set w/o losing current selections?
	//

	/**
	 * @return true if there are any active selection targets
	 */
	bool HasActiveTargets() const;
	
	/**
	 * Empty the active selection target set
	 * @warning Active selection must be cleared (eg via ClearSelection()) before calling this function
	 */
	void ClearActiveTargets();

	/**
	 * Add a target to the active target set, if a valid IGeometrySelectorFactory can be found
	 * @return true on success
	 */
	bool AddActiveTarget(FGeometryIdentifier Target);

	/**
	 * Update the current active target set based on DesiredActiveSet, assuming that a valid IGeometrySelectorFactory
	 * can be found for each identifier. 
	 * This function will emit a transaction/change if the target set is modified.
	 * @param WillChangeActiveTargetsCallback this function will be called if the DesiredActiveSet is not the same as the current active set, allowing calling code to (eg) clear the active selection
	 */
	void SynchronizeActiveTargets(
		const TArray<FGeometryIdentifier>& DesiredActiveSet,
		TFunctionRef<void()> WillChangeActiveTargetsCallback );

	/**
	 * Test if a World-space ray "hits" the current active target set, which can be used to (eg)
	 * determine if a higher-level user interaction for selection should "Capture" the click
	 * @param HitResultOut hit information is returned here
	 * @return true on hit
	 */
	virtual bool RayHitTest(
		const FRay3d& WorldRay,
		FInputRayHit& HitResultOut
	);


	//
	// Selection Updates
	//
public:
	/**
	 * Clear any active element selections. 
	 * This function will emit a Transaction for the selection change.
	 */
	virtual void ClearSelection();

	/**
	 * Use the given WorldRay to update the active element selection based on UpdateConfig.
	 * The intention is that this function is called by higher-level user interaction code after
	 * RayHitTest() has returned true.
	 * @param ResultOut information on any element selection modifications is returned here
	 */
	virtual void UpdateSelectionViaRaycast(
		const FRay3d& WorldRay,
		const FGeometrySelectionUpdateConfig& UpdateConfig,
		FGeometrySelectionUpdateResult& ResultOut
	);


	DECLARE_MULTICAST_DELEGATE(FModelingSelectionInteraction_SelectionModified);
	/**
	 * OnSelectionModified is broadcast if the selection is modified via the above functions.
	 * There are no arguments.
	 */
	FModelingSelectionInteraction_SelectionModified OnSelectionModified;


	//
	// Selection queries
	//
public:
	/** @return true if there is an active element selection */
	virtual bool HasSelection() const;

	/** @return a world-space bounding box for the active element selection */
	virtual bool GetSelectionBounds(FGeometrySelectionBounds& BoundsOut) const;
	/** @return a 3D transformation frame suitable for use with the active element selection */
	virtual void GetSelectionWorldFrame(UE::Geometry::FFrame3d& SelectionFrame) const;

	/** @return true if there is an active IGeometrySelector target for the given Component and it has a non-empty selection */
	virtual bool HasSelectionForComponent(UPrimitiveComponent* Component) const;
	/** 
	 * Get the active element selection for the given Component, if it exists
	 * @return true if a valid non-empty selection was returned in SelectionOut
	 */
	virtual bool GetSelectionForComponent(UPrimitiveComponent* Component, FGeometrySelection& SelectionOut) const;



	//
	// Transformations
	//
protected:
	// Set of existing transformer objects, collected from active IGeometrySelector::InitializeTransformation().
	// The SelectionManager does not own these objects, they must not be deleted. 
	// Instead they need to be returned via IGeometrySelector::ShutdownTransformation
	TArray<IGeometrySelectionTransformer*> ActiveTransformations;

public:
	/** @return true if SelectionManager is actively transforming element selections (ie during a mouse-drag) */
	virtual bool IsInActiveTransformation() const { return (ActiveTransformations.Num() > 0); }
	
	/**
	 * Begin a transformation of element selections in active Targets.
	 * @return true if at least one valid Transformer was initialized, ie the transformation will do something
	 */
	virtual bool BeginTransformation();

	/**
	 * Update the active transformations with the given PositionTransformFunc.
	 * See IGeometrySelectionTransformer::UpdateTransform for details on this callback
	 */
	virtual void UpdateTransformation( TFunctionRef<FVector3d(int32 VertexID, const FVector3d&, const FTransform&)> PositionTransformFunc );

	/**
	 * End the current active transformation, and emit changes/transactions
	 */
	virtual void EndTransformation();


	//
	// Command Execution
	//
public:
	/**
	 * @return true if Command->CanExecuteCommand() returns true for *all* the current Selections
	 */
	bool CanExecuteSelectionCommand(UGeometrySelectionEditCommand* Command);

	/**
	 * execute the selection command for *all* the current selections
	 */
	void ExecuteSelectionCommand(UGeometrySelectionEditCommand* Command);


protected:
	// This is set to current selection during CanExecuteSelectionCommand/ExecuteSelectionCommand, to keep the UObject alive
	// Not expected to be used outside that context
	UPROPERTY()
	TObjectPtr<UGeometrySelectionEditCommandArguments> SelectionArguments;

	// apply ProcessFunc to active selections via handles, perhaps should be public?
	void ProcessActiveSelections(TFunctionRef<void(FGeometrySelectionHandle)> ProcessFunc);



	//
	// Undo/Redo
	//
public:
	virtual void ApplyChange(IGeometrySelectionChange* Change);
	virtual void RevertChange(IGeometrySelectionChange* Change);


	//
	// Debugging stuff
	//
public:
	/** Print information about the active selection using UE_LOG */
	virtual void DebugPrintSelection();
	/** Visualize the active selection using PDI drawing */
	virtual void DebugRender(IToolsContextRenderAPI* RenderAPI);

	/**
	 * Convert the active selection to a UPersistentMeshSelection. 
	 * SelectionManager will hold a reference to UPersistentMeshSelection to prevent it from being garbage collected,
	 * but only until the next call to GetActiveSingleSelectionConverted_Legacy()
	 * This function will be removed in future when UPersistentMeshSelection is no longer needed.
	 */
	UPersistentMeshSelection* GetActiveSingleSelectionConverted_Legacy(UPrimitiveComponent* ForComponent);


protected:

	// current selection mode settings

	EGeometryElementType SelectionElementType = EGeometryElementType::Face;
	void SetSelectionElementTypeInternal(EGeometryElementType NewElementType);

	EMeshTopologyMode MeshTopologyMode = EMeshTopologyMode::None;
	void SetMeshTopologyModeInternal(EMeshTopologyMode NewTopologyMode);

	// ITF references

	UPROPERTY()
	TObjectPtr<UInteractiveToolsContext> ToolsContext;

	IToolsContextTransactionsAPI* TransactionsAPI;

	// set of registered IGeometrySelector factories
	TArray<TUniquePtr<IGeometrySelectorFactory>> Factories;



	// FGeometrySelectionTarget is the set of information tracked for a given "Active Target",
	// which is (eg) a Mesh Component or other external object that "owns" selectable Geometry.
	// This includes the IGeometrySelector for that target, the SelectionEditor, and the active Selection
	struct FGeometrySelectionTarget
	{
	public:
		FGeometryIdentifier TargetIdentifier;		// identifier of target object used to initialize the selection (eg Component/etc)
		FGeometryIdentifier SelectionIdentifer;		// identifier of object that is being selected-on, eg UDynamicMesh/etc

		TUniquePtr<IGeometrySelector> Selector;					// active Selector

		FGeometrySelection Selection;				// current Selection
		TUniquePtr<FGeometrySelectionEditor> SelectionEditor;	// active Selection Editor

		FDelegateHandle OnGeometryModifiedHandle;	// hooked up to (eg) UDynamicMesh OnMeshChanged, etc
	};

	// Set of active Selection Targets updated by SynchronizeActiveTargets / etc
	TArray<TSharedPtr<FGeometrySelectionTarget>> ActiveTargetReferences;

	// map from external Identifiers to active Selection Targets
	TMap<FGeometryIdentifier, TSharedPtr<FGeometrySelectionTarget>> ActiveTargetMap;


	//
	// Support for cached FGeometrySelectionTarget / IGeometrySelectors. 
	// The intention here is to reduce the overhead on selection changes.
	// Functional, but needs to be smarter.
	//
	TMap<FGeometryIdentifier, TSharedPtr<FGeometrySelectionTarget>> TargetCache;
	void SleepOrShutdownTarget(FGeometrySelectionTarget* Target, bool bForceShutdown);
	TSharedPtr<FGeometrySelectionTarget> GetCachedTarget(FGeometryIdentifier Identifier, const IGeometrySelectorFactory* UseFactory);
	void ResetTargetCache();
	void SetTargetsOnUndoRedo(TArray<FGeometryIdentifier> NewTargets);
	TArray<FGeometryIdentifier> GetCurrentTargetIdentifiers() const;

	void OnTargetGeometryModified(IGeometrySelector* Selector);

	//
	// 3D geometry for element selections of each ActiveTarget is cached
	// to improve rendering performance
	//
	TArray<FGeometrySelectionElements> CachedSelectionRenderElements;
	bool bSelectionRenderCachesDirty = false;
	void UpdateSelectionRenderCacheOnTargetChange();
	void RebuildSelectionRenderCaches();

	// various change types need internal access

	friend class FGeometrySelectionManager_SelectionTypeChange;
	friend class FGeometrySelectionManager_ActiveTargetsChange;


	// legacy / to-deprecate/remove
	
	// this is to support GetActiveSingleSelectionConverted_Legacy, which will eventually be removed
	UPROPERTY()
	TObjectPtr<UPersistentMeshSelection> OldSelection;

};





