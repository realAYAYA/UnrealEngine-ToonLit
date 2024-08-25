// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "EditorInteractiveGizmoRegistry.h"
#include "InteractiveGizmoManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "EditorInteractiveGizmoManager.generated.h"

class FCanvas;
class FEditorModeTools;
class IToolsContextQueriesAPI;
class IToolsContextRenderAPI;
class IToolsContextTransactionsAPI;
class UEdModeInteractiveToolsContext;
class UInputRouter;
class UInteractiveGizmo;
class UInteractiveGizmoBuilder;
class UObject;
class UTypedElementSelectionSet;
struct FToolBuilderState;
class UTransformGizmo;
struct FGizmosParameters;

USTRUCT()
struct FActiveEditorGizmo
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UInteractiveGizmo> Gizmo = nullptr;
	void* Owner = nullptr;
};

/**
 * UEditorInteractiveGizmoManager allows users of the Tools framework to register and create selection-based Gizmo instances.
 * For each selection-based Gizmo, a builder derived from UInteractiveGizmoSelectionBuilder is registered with the GizmoManager.
 * When the section changes, the highest priority builders for which SatisfiesCondition() return true, will be used to
 * build gizmos.
 */
UCLASS(Transient)
class EDITORINTERACTIVETOOLSFRAMEWORK_API UEditorInteractiveGizmoManager : public UInteractiveGizmoManager
{
	GENERATED_BODY()

protected:
	friend class UEditorInteractiveToolsContext;		// to call Initialize/Shutdown

	UEditorInteractiveGizmoManager();

	/** Initialize the GizmoManager with the necessary Context-level state. UEdModeInteractiveToolsContext calls this, you should not. */
	virtual void InitializeWithEditorModeManager(IToolsContextQueriesAPI* QueriesAPI, IToolsContextTransactionsAPI* TransactionsAPI, UInputRouter* InputRouter, FEditorModeTools* InEditorModeManager);

	// UInteractiveGizmoManager interface
	virtual void Shutdown() override;

public:

	// UInteractiveGizmoManager interface
	virtual void Tick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;
	virtual void DrawHUD(FCanvas* Canvas, IToolsContextRenderAPI* RenderAPI) override;

	/**
	 * Register a new Editor gizmo type.
	 * @param InGizmoCategory category in which to register gizmo builder
	 * @param InGizmonBuilder new Editor gizmo builder
	 * - Accessory and Primary gizmo builders must derive from UInteractiveGizmoBuilder (of from a builder derived from it)
	 *   and must implement the IEditorInteractiveConditionalGizmoBuilder and IEditorInteractiveSelectionGizmoBuilder interfaces.
	 */
	void RegisterEditorGizmoType(EEditorGizmoCategory InGizmoCategory, UInteractiveGizmoBuilder* InGizmoBuilder);

	/**
	* Remove an Editor gizmo type from the set of known Editor gizmo types
	* @param InGizmoBuilder same object pointer that was passed to RegisterEditorGizmoType()
	* @return true if gizmo type was found and deregistered
	*/
	void DeregisterEditorGizmoType(EEditorGizmoCategory InGizmoCategory, UInteractiveGizmoBuilder* InGizmoBuilder);

	/**
	 * Get all qualified Editor gizmo builders for the specified category, based on the current state. Qualification is determined by the gizmo builder
	 * returning true from SatisfiesCondition() and relative priority. All qualified builders at the highest found priority
	 * will be returned.
	 * @param InGizmoCategory category in which to search for qualified builders
	 * @param InToolBuilderState current selection and other state
	 * @return array of qualified Gizmo selection builders based on current state
	 */
	void GetQualifiedEditorGizmoBuilders(EEditorGizmoCategory InGizmoCategory, const FToolBuilderState& InToolBuilderState, TArray<UInteractiveGizmoBuilder*>& InFoundBuilders);

	/** 
	 * Set how auto gizmo resolution should occur when CreateGizmosForCurrentState is invoked. If bSearchLocalOnly is true, only the current
	 * @param bLocalOnly - if true, only the current gizmo manager registry will be searched for candidate gizmos. If false,
	 *   both the gizmo manager registry and any higher gizmo manager or gizmo subsystem (in the case of selection builders) will be searched
	 */
	virtual void SetEditorGizmoBuilderResolution(bool bLocalOnly)
	{
		bSearchLocalBuildersOnly = bLocalOnly;
	}

	/**
	 * Returns the current auto gizmo resolution setting 
	 */
	virtual bool GetEditorGizmoBuilderResolution() const
	{
		return bSearchLocalBuildersOnly;
	}

	/**
	 * Shutdown and remove a selection-based Gizmo
	 * @param Gizmo the Gizmo to shutdown and remove
	 * @return true if the Gizmo was found and removed
	 */
	virtual bool DestroyEditorGizmo(UInteractiveGizmo* Gizmo);

	/**
	 * Shutdown and remove all active auto gizmos
	 */
	virtual void DestroyAllEditorGizmos();

	/** Try to activate a new Gizmo instance (UInteractiveGizmoManager override) */	
	virtual UInteractiveGizmo* CreateGizmo(
		const FString& BuilderIdentifier, const FString& InstanceIdentifier = FString(), void* Owner = nullptr) override;

	/** Shutdown and remove a Gizmo (UInteractiveGizmoManager override) */
	virtual bool DestroyGizmo(UInteractiveGizmo* InGizmo) override;

	/** instance/builder identifiers for transform gizmo */
	static const FString& TransformInstanceIdentifier();
	static const FString& TransformBuilderIdentifier();

	/**
	 * Returns true if the new TRS gizmos are used.
	 */
	static bool UsesNewTRSGizmos();

	/**
	 * Updates the current New TRS Gizmo state and notifies that change using OnUsesNewTRSGizmosChangedDelegate  
	 */
	static void SetUsesNewTRSGizmos(const bool bUseNewTRSGizmos);

	/**
	 * Delegate to notify from bUseNewTRSGizmos changes  
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnUsesNewTRSGizmosChanged, const bool bUseNewTRSGizmos);
	static FOnUsesNewTRSGizmosChanged& OnUsesNewTRSGizmosChangedDelegate();

	/**
	 * Notifies FGizmosParameters changes across bound transform gizmos
	 */
	static void SetGizmosParameters(const FGizmosParameters& InParameters);

	/**
	 * Delegate to notify from FGizmosParameters changes  
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnGizmosParametersChanged, const FGizmosParameters& InParameters);
    static FOnGizmosParametersChanged& OnGizmosParametersChangedDelegate();

	/**
	 * Returns the default gizmos parameters if set
	 */
    static const TOptional<FGizmosParameters>& GetDefaultGizmosParameters();

protected:

	/**
	 * Returns true if selection gizmos should be visible. 
	 * @todo move this to a gizmo context object
	 */
	virtual bool GetShowEditorGizmos();

	/**
	 * Returns true if gizmos should be visible based on the current view's engine show flag.
	 * @todo move this to a gizmo context object
	 */
	virtual bool GetShowEditorGizmosForView(IToolsContextRenderAPI* RenderAPI);

	/**
	 * Updates active selection gizmos when show selection state changes
	 */
	void UpdateActiveEditorGizmos();

	/** Actual registry */
	UPROPERTY()
	TObjectPtr<UEditorInteractiveGizmoRegistry> Registry;

	/** set of Currently-active Gizmos */
	UPROPERTY()
	TArray<FActiveEditorGizmo> ActiveEditorGizmos;

	/** If false, only search gizmo builders in current gizmo manager. If true, also search gizmo subsystem */
	bool bSearchLocalBuildersOnly = false;

	/** Cache for already built gizmos, currently this only caches the transform */
	UPROPERTY()
	TMap<TObjectPtr<UInteractiveGizmoBuilder>, TObjectPtr<UInteractiveGizmo>> CachedGizmoMap;

	/** @todo: remove when GetShowEditorGizmos() is moved to gizmo context object */
	FEditorModeTools* EditorModeManager = nullptr;

	/** Whether Editor gizmos are enabled. UpdateActiveEditorGizmos() determines this value each tick and updates if it has changed. */
	bool bShowEditorGizmos = false;

private:

	/** Returns the existing default Gizmo instance if any. */
	UTransformGizmo* FindDefaultTransformGizmo() const;
};
