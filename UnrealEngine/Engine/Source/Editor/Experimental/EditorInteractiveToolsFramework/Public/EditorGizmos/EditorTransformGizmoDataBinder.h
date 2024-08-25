// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Containers/Set.h"

class UGizmoEditorStateTarget;
class UEditorTransformGizmoContextObject;
class UTransformGizmo;
class UTransformProxy;
class FEditorViewportClient;

/**
 * FEditorTransformGizmoDataBinder is a helper class for binding a UTransformGizmo to a FEditorModeTools
 */

class EDITORINTERACTIVETOOLSFRAMEWORK_API FEditorTransformGizmoDataBinder : public TSharedFromThis<FEditorTransformGizmoDataBinder>
{
public:

	virtual ~FEditorTransformGizmoDataBinder();
	
	/**
	 * Makes it so that the gizmo binder attaches to any gizmos created by the context object
	 * in the future. The binding is automatically removed if FEditorTransformGizmoDataBinder is
	 * destroyed.
	 */
	void BindToGizmoContextObject(UEditorTransformGizmoContextObject* InContextObject);

	/** Used for binding to context object, called from the OnGizmoCreated delegate. */
	void BindToUninitializedGizmo(UTransformGizmo* Gizmo);
	
	/** Binds to a specific gizmo for tracking. Requires ActiveTarget to be set. */
	void BindToInitializedGizmo(UTransformGizmo* InGizmo, UTransformProxy* InProxy);
	
	/** Unbinds from a given gizmo. Done automatically for gizmos when their ActiveTarget is cleared. */
	void UnbindFromGizmo(UTransformGizmo* InGizmo, UTransformProxy* InProxy);

private:

	/** List of the gizmos currently bound to this so that we ensure that they are actually unbound on destruction.
	 * Note that this should not happen and bound gizmos should have called their Shutdown function before this being destroyed.
	 */
	TSet<TWeakObjectPtr<UTransformGizmo>> BoundGizmos;

	/** Weak ptr to the context so we can interface with the current mode manager */
	TWeakObjectPtr<UEditorTransformGizmoContextObject> WeakContext;

	FEditorViewportClient* GetViewportClient() const;

	void OnProxyBeginTransformEdit(UTransformProxy* InTransformProxy);
	void OnProxyTransformChanged(UTransformProxy* InTransformProxy, FTransform InTransform);
	void OnProxyEndTransformEdit(UTransformProxy* InTransformProxy);

	bool bHasTransformChanged = false;
};

