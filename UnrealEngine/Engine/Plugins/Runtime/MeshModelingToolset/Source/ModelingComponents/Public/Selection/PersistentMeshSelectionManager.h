// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Selection/PersistentMeshSelection.h"
#include "InteractiveToolChange.h"

#include "PersistentMeshSelectionManager.generated.h"

class UInteractiveToolsContext;
class UInteractiveToolManager;
class UPrimitiveComponent;
class UInteractiveTool;
class UPreviewGeometry;


/**
 * UDEPRECATED_PersistentMeshSelectionManager manages an active Mesh Selection.
 * The assumption is that this class will be registered with the ContextStore of an InteractiveToolsContext,
 * and the various Tool implementations will get/set the selection.
 * 
 * 
 * 
 * StoredMeshSelectionUtil.h contains various utility functions to simplify usage of this class,
 * those functions are preferable to directly using the Manager.
 * 
 */
//UE_DEPRECATED(5.1, "UPersistentMeshSelection and related functions are deprecated")
UCLASS(Deprecated) 
class MODELINGCOMPONENTS_API UDEPRECATED_PersistentMeshSelectionManager : public UObject
{
	GENERATED_BODY()

public:
	
	virtual void Initialize(TObjectPtr<UInteractiveToolsContext> ToolsContext);
	virtual void Shutdown();

	/** @return true if there is an active selection */
	virtual bool HasActiveSelection();

	/** @return the active selection */
	virtual UDEPRECATED_PersistentMeshSelection* GetActiveSelection();

	/** Set a new active selection. This is undoable, ie a FPersistentMeshSelectionChange will be emitted */
	virtual void SetNewActiveSelection(UDEPRECATED_PersistentMeshSelection* Selection);

	/** Clear the active selection. This is undoable, ie a FPersistentMeshSelectionChange will be emitted */
	virtual void ClearActiveSelection();


protected:
	UPROPERTY()
	TObjectPtr<UInteractiveToolsContext> ParentContext;

	UE_DEPRECATED(5.2, "UPersistentMeshSelection and related functions are deprecated")
	UPROPERTY()
	TObjectPtr<UDEPRECATED_PersistentMeshSelection> ActiveSelection_DEPRECATED;

	UPROPERTY()
	TObjectPtr<UPreviewGeometry> SelectionDisplay;

	virtual void OnSelectionModified();

	virtual void SetNewActiveSelectionInternal(UDEPRECATED_PersistentMeshSelection* Selection);
	friend class FPersistentMeshSelectionChange;
};





/**
 * FPersistentMeshSelectionChange stores a selection change, ie before-and-after states.
 * UDEPRECATED_PersistentMeshSelectionManager emits these changes on selection modifications.
 */
class MODELINGCOMPONENTS_API 
UE_DEPRECATED(5.1, "UDEPRECATED_PersistentMeshSelectionManager has been deprecated and FPersistentMeshSelectionChange is no longer used") 
FPersistentMeshSelectionChange : public FToolCommandChange
{
public:
	FGenericMeshSelection From;
	FGenericMeshSelection To;

	virtual void Apply(UObject* Object) override;
	virtual void Revert(UObject* Object) override;
	virtual bool HasExpired(UObject* Object) const override;
	virtual FString ToString() const override;
};








namespace UE
{
namespace Geometry
{
	//
	// The functions below are helper functions that simplify usage of a UDEPRECATED_PersistentMeshSelectionManager 
	// that is registered as a ContextStoreObject in an InteractiveToolsContext
	//

	/**
	 * If one does not already exist, create a new instance of UDEPRECATED_PersistentMeshSelectionManager and add it to the
	 * ToolsContext's ContextObjectStore
	 * @return true if the ContextObjectStore now has a UDEPRECATED_PersistentMeshSelectionManager (whether it already existed, or was created)
	 */
	UE_DEPRECATED(5.1, "UE::Geometry::RegisterPersistentMeshSelectionManager is deprecated")
	MODELINGCOMPONENTS_API bool RegisterPersistentMeshSelectionManager(UInteractiveToolsContext* ToolsContext);

	/**
	 * Remove any existing UDEPRECATED_PersistentMeshSelectionManager from the ToolsContext's ContextObjectStore
	 * @return true if the ContextObjectStore no longer has a UDEPRECATED_PersistentMeshSelectionManager (whether it was removed, or did not exist)
	 */
	UE_DEPRECATED(5.1, "UE::Geometry::DeregisterPersistentMeshSelectionManager is deprecated")
	MODELINGCOMPONENTS_API bool DeregisterPersistentMeshSelectionManager(UInteractiveToolsContext* ToolsContext);


	/**
	 * Find an existing UDEPRECATED_PersistentMeshSelectionManager in the ToolsContext's ContextObjectStore
	 * @return SelectionManager pointer or nullptr if not found
	 */
	UE_DEPRECATED(5.1, "UE::Geometry::FindPersistentMeshSelectionManager is deprecated")
	MODELINGCOMPONENTS_API UDEPRECATED_PersistentMeshSelectionManager* FindPersistentMeshSelectionManager(UInteractiveToolManager* ToolManager);


}
}