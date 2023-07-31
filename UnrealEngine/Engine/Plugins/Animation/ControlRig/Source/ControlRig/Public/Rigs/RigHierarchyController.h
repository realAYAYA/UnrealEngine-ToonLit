// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rigs/RigHierarchy.h"
#include "ReferenceSkeleton.h"
#include "RigHierarchyContainer.h"
#include "Animation/Skeleton.h"
#include "RigHierarchyContainer.h"
#include "RigHierarchyController.generated.h"

UCLASS(BlueprintType)
class CONTROLRIG_API URigHierarchyController : public UObject
{
	GENERATED_BODY()

public:

	URigHierarchyController()
	: bReportWarningsAndErrors(true)
	, Hierarchy(nullptr)
	, bSuspendAllNotifications(false)
	, bSuspendSelectionNotifications(false)
	, bSuspendPythonPrinting(false)
	, CurrentInstructionIndex(INDEX_NONE)
	{}

	virtual ~URigHierarchyController();

	// Returns the hierarchy currently linked to this controller
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	URigHierarchy* GetHierarchy() const
	{
		return Hierarchy.Get();
	}

	// Sets the hierarchy currently linked to this controller
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	void SetHierarchy(URigHierarchy* InHierarchy);

	/**
	 * Selects or deselects an element in the hierarchy
	 * @param InKey The key of the element to select
	 * @param bSelect If set to false the element will be deselected
	 * @return Returns true if the selection was applied
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	bool SelectElement(FRigElementKey InKey, bool bSelect = true, bool bClearSelection = false);

	/**
	 * Deselects or deselects an element in the hierarchy
	 * @param InKey The key of the element to deselect
	 * @return Returns true if the selection was applied
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    FORCEINLINE bool DeselectElement(FRigElementKey InKey)
	{
		return SelectElement(InKey, false);
	}

	/**
	 * Sets the selection based on a list of keys
	 * @param InKeys The array of keys of the elements to select
	 * @return Returns true if the selection was applied
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    bool SetSelection(const TArray<FRigElementKey>& InKeys, bool bPrintPythonCommand = false);

	/**
	 * Clears the selection
	 * @return Returns true if the selection was applied
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    FORCEINLINE bool ClearSelection()
	{
		return SetSelection(TArray<FRigElementKey>());
	}
	
	/**
	 * Adds a bone to the hierarchy
	 * @param InName The suggested name of the new bone - will eventually be corrected by the namespace
	 * @param InParent The (optional) parent of the new bone. If you don't need a parent, pass FRigElementKey()
	 * @param InTransform The transform for the new bone - either in local or global space, based on bTransformInGlobal
	 * @param bTransformInGlobal Set this to true if the Transform passed is expressed in global space, false for local space.
	 * @param InBoneType The type of bone to add. This can be used to differentiate between imported bones and user defined bones.
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return The key for the newly created bone.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	FRigElementKey AddBone(FName InName, FRigElementKey InParent, FTransform InTransform, bool bTransformInGlobal = true, ERigBoneType InBoneType = ERigBoneType::User, bool bSetupUndo = false, bool bPrintPythonCommand = false);

	/**
	 * Adds a null to the hierarchy
	 * @param InName The suggested name of the new null - will eventually be corrected by the namespace
	 * @param InParent The (optional) parent of the new null. If you don't need a parent, pass FRigElementKey()
	 * @param InTransform The transform for the new null - either in local or global null, based on bTransformInGlobal
	 * @param bTransformInGlobal Set this to true if the Transform passed is expressed in global null, false for local null.
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return The key for the newly created null.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    FRigElementKey AddNull(FName InName, FRigElementKey InParent, FTransform InTransform, bool bTransformInGlobal = true, bool bSetupUndo = false, bool bPrintPythonCommand = false);

	/**
	 * Adds a control to the hierarchy
	 * @param InName The suggested name of the new control - will eventually be corrected by the namespace
	 * @param InParent The (optional) parent of the new control. If you don't need a parent, pass FRigElementKey()
	 * @param InSettings All of the control's settings
	 * @param InValue The value to use for the control
	 * @param InOffsetTransform The transform to use for the offset
	 * @param InShapeTransform The transform to use for the shape
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return The key for the newly created control.
	 */
    FRigElementKey AddControl(
    	FName InName,
    	FRigElementKey InParent,
    	FRigControlSettings InSettings,
    	FRigControlValue InValue,
    	FTransform InOffsetTransform = FTransform::Identity,
        FTransform InShapeTransform = FTransform::Identity,
        bool bSetupUndo = true,
        bool bPrintPythonCommand = false
        );

	/**
	 * Adds a control to the hierarchy
	 * @param InName The suggested name of the new control - will eventually be corrected by the namespace
	 * @param InParent The (optional) parent of the new control. If you don't need a parent, pass FRigElementKey()
	 * @param InSettings All of the control's settings
	 * @param InValue The value to use for the control
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @return The key for the newly created control.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController, meta = (DisplayName = "Add Control", ScriptName = "AddControl"))
    FORCEINLINE FRigElementKey AddControl_ForBlueprint(
        FName InName,
        FRigElementKey InParent,
        FRigControlSettings InSettings,
        FRigControlValue InValue,
        bool bSetupUndo = true,
        bool bPrintPythonCommand = false
    )
	{
		return AddControl(InName, InParent, InSettings, InValue, FTransform::Identity, FTransform::Identity, bSetupUndo, bPrintPythonCommand);
	}

		/**
	 * Adds a control to the hierarchy
	 * @param InName The suggested name of the new animation channel - will eventually be corrected by the namespace
	 * @param InParentControl The parent of the new animation channel.
	 * @param InSettings All of the animation channel's settings
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return The key for the newly created animation channel.
	 */
    FRigElementKey AddAnimationChannel(
    	FName InName,
    	FRigElementKey InParentControl,
    	FRigControlSettings InSettings,
        bool bSetupUndo = true,
        bool bPrintPythonCommand = false
    );

	/**
	 * Adds a control to the hierarchy
	 * @param InName The suggested name of the new animation channel - will eventually be corrected by the namespace
	 * @param InParentControl The parent of the new animation channel.
	 * @param InSettings All of the animation channel's settings
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @return The key for the newly created animation channel.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController, meta = (DisplayName = "Add Control", ScriptName = "AddAnimationChannel"))
    FORCEINLINE FRigElementKey AddAnimationChannel_ForBlueprint(
        FName InName,
        FRigElementKey InParentControl,
        FRigControlSettings InSettings,
        bool bSetupUndo = true,
        bool bPrintPythonCommand = false
    )
	{
		return AddAnimationChannel(InName, InParentControl, InSettings, bSetupUndo, bPrintPythonCommand);
	}

	/**
	 * Adds a curve to the hierarchy
	 * @param InName The suggested name of the new curve - will eventually be corrected by the namespace
	 * @param InValue The value to use for the curve
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return The key for the newly created curve.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    FRigElementKey AddCurve(
        FName InName,
        float InValue = 0.f,
        bool bSetupUndo = true,
		bool bPrintPythonCommand = false
        );

	/**
	* Adds a rigidbody to the hierarchy
	* @param InName The suggested name of the new rigidbody - will eventually be corrected by the namespace
	* @param InParent The (optional) parent of the new rigidbody. If you don't need a parent, pass FRigElementKey()
	* @param InSettings All of the rigidbody's settings
	* @param InLocalTransform The transform for the new rigidbody - in the space of the provided parent
	* @param bSetupUndo If set to true the stack will record the change for undo / redo
	* @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	* @return The key for the newly created rigidbody.
	*/
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    FRigElementKey AddRigidBody(
    	FName InName,
    	FRigElementKey InParent,
        FRigRigidBodySettings InSettings,
    	FTransform InLocalTransform,
    	bool bSetupUndo = false,
		bool bPrintPythonCommand = false);

	/**
	* Adds an reference to the hierarchy
	* @param InName The suggested name of the new reference - will eventually be corrected by the namespace
	* @param InParent The (optional) parent of the new reference. If you don't need a parent, pass FRigElementKey()
	* @param InDelegate The delegate to use to pull the local transform
	* @param bSetupUndo If set to true the stack will record the change for undo / redo
	* @return The key for the newly created reference.
	*/
    FRigElementKey AddReference(
        FName InName,
        FRigElementKey InParent,
        FRigReferenceGetWorldTransformDelegate InDelegate,
        bool bSetupUndo = false);

	/**
	 * Returns the control settings of a given control
	 * @param InKey The key of the control to receive the settings for
	 * @return The settings of the given control
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	FRigControlSettings GetControlSettings(FRigElementKey InKey) const;

	/**
	 * Sets a control's settings given a control key
	 * @param InKey The key of the control to set the settings for
	 * @param The settings to set
	 * @return Returns true if the settings have been set correctly
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    bool SetControlSettings(FRigElementKey InKey, FRigControlSettings InSettings, bool bSetupUndo = false) const;

	/**
	 * Imports an existing skeleton to the hierarchy
	 * @param InSkeleton The skeleton to import
	 * @param InNameSpace The namespace to prefix the bone names with
	 * @param bReplaceExistingBones If true existing bones will be removed
	 * @param bRemoveObsoleteBones If true bones non-existent in the skeleton will be removed from the hierarchy
	 * @param bSelectBones If true the bones will be selected upon import
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @return The keys of the imported elements
	 */
	TArray<FRigElementKey> ImportBones(
		const FReferenceSkeleton& InSkeleton,
		const FName& InNameSpace,
		bool bReplaceExistingBones = true,
		bool bRemoveObsoleteBones = true,
		bool bSelectBones = false,
		bool bSetupUndo = false);

	/**
	 * Imports an existing skeleton to the hierarchy
	 * @param InSkeleton The skeleton to import
	 * @param InNameSpace The namespace to prefix the bone names with
	 * @param bReplaceExistingBones If true existing bones will be removed
	 * @param bRemoveObsoleteBones If true bones non-existent in the skeleton will be removed from the hierarchy
	 * @param bSelectBones If true the bones will be selected upon import
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return The keys of the imported elements
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	TArray<FRigElementKey> ImportBones(
		USkeleton* InSkeleton,
		FName InNameSpace = NAME_None,
		bool bReplaceExistingBones = true,
		bool bRemoveObsoleteBones = true,
		bool bSelectBones = false,
		bool bSetupUndo = false,
		bool bPrintPythonCommand = false);

#if WITH_EDITOR
	/**
	 * Imports an existing skeleton to the hierarchy
	 * @param InAssetPath The path to the uasset to import from
	 * @param InNameSpace The namespace to prefix the bone names with
	 * @param bReplaceExistingBones If true existing bones will be removed
	 * @param bRemoveObsoleteBones If true bones non-existent in the skeleton will be removed from the hierarchy
	 * @param bSelectBones If true the bones will be selected upon import
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @return The keys of the imported elements
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    TArray<FRigElementKey> ImportBonesFromAsset(
        FString InAssetPath,
        FName InNameSpace = NAME_None,
        bool bReplaceExistingBones = true,
        bool bRemoveObsoleteBones = true,
        bool bSelectBones = false,
        bool bSetupUndo = false);
#endif

	/**
	 * Imports all curves from a skeleton to the hierarchy
	 * @param InSkeleton The skeleton to import the curves from
	 * @param InNameSpace The namespace to prefix the bone names with
	 * @param bSelectCurves If true the curves will be selected upon import
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @return The keys of the imported elements
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	TArray<FRigElementKey> ImportCurves(
		USkeleton* InSkeleton, 
		FName InNameSpace = NAME_None,  
		bool bSelectCurves = false,
		bool bSetupUndo = false,
		bool bPrintPythonCommand = false);

#if WITH_EDITOR
	/**
	 * Imports all curves from a skeleton to the hierarchy
	 * @param InAssetPath The path to the uasset to import from
	 * @param InNameSpace The namespace to prefix the bone names with
	 * @param bSelectCurves If true the curves will be selected upon import
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @return The keys of the imported elements
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    TArray<FRigElementKey> ImportCurvesFromAsset(
        FString InAssetPath,
        FName InNameSpace = NAME_None, 
        bool bSelectCurves = false,
        bool bSetupUndo = false);
#endif

	/**
	 * Exports the selected items to text
	 * @return The text representation of the selected items
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	FString ExportSelectionToText() const;

	/**
	 * Exports a list of items to text
	 * @param InKeys The keys to export to text
	 * @return The text representation of the requested elements
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	FString ExportToText(TArray<FRigElementKey> InKeys) const;

	/**
	 * Imports the content of a text buffer to the hierarchy
	 * @param InContent The string buffer representing the content to import
	 * @param bReplaceExistingElements If set to true existing items will be replaced / updated with the content in the buffer
	 * @param bSelectNewElements If set to true the new elements will be selected
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	TArray<FRigElementKey> ImportFromText(
		FString InContent,
		bool bReplaceExistingElements = false,
		bool bSelectNewElements = true,
		bool bSetupUndo = false,
		bool bPrintPythonCommands = false);

	/**
	* Imports the content of a RigHierachyContainer (the hierarchy v1 pre 5.0)
	* This is used for backwards compatbility only during load and does not support undo.
	* @param InContainer The input hierarchy container
	*/
    TArray<FRigElementKey> ImportFromHierarchyContainer(const FRigHierarchyContainer& InContainer, bool bIsCopyAndPaste);

	/**
	 * Removes an existing element from the hierarchy
	 * @param InElement The key of the element to remove
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return Returns true if successful.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	bool RemoveElement(FRigElementKey InElement, bool bSetupUndo = false, bool bPrintPythonCommand = false);

	/**
	 * Renames an existing element in the hierarchy
	 * @param InElement The key of the element to rename
	 * @param InName The new name to set for the element
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @param bClearSelection True if the selection should be cleared after a rename
	 * @return Returns the new element key used for the element
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    FRigElementKey RenameElement(FRigElementKey InElement, FName InName, bool bSetupUndo = false, bool bPrintPythonCommand = false, bool bClearSelection = true);

	/**
 	 * Sets the display name on a control
 	 * @param InControl The key of the control to change the display name for
 	 * @param InDisplayName The new display name to set for the control
 	 * @param bRenameElement True if the control should also be renamed
 	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
 	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return Returns the new display name used for the control
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	FName SetDisplayName(FRigElementKey InControl, FName InDisplayName, bool bRenameElement = false, bool bSetupUndo = false, bool bPrintPythonCommand = false);

	/**
	 * Adds a new parent to an element. For elements that allow only one parent the parent will be replaced (Same as ::SetParent).
	 * @param InChild The key of the element to add the parent for
	 * @param InParent The key of the new parent to add
	 * @param InWeight The initial weight to give to the parent
	 * @param bMaintainGlobalTransform If set to true the child will stay in the same place spatially, otherwise it will maintain it's local transform (and potential move).
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @return Returns true if successful.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	bool AddParent(FRigElementKey InChild, FRigElementKey InParent, float InWeight = 0.f, bool bMaintainGlobalTransform = true, bool bSetupUndo = false);

	/**
	* Adds a new parent to an element. For elements that allow only one parent the parent will be replaced (Same as ::SetParent).
	* @param InChild The element to add the parent for
	* @param InParent The new parent to add
	* @param InWeight The initial weight to give to the parent
	* @param bMaintainGlobalTransform If set to true the child will stay in the same place spatially, otherwise it will maintain it's local transform (and potential move).
	* @param bRemoveAllParents If set to true all parents of the child will be removed first.
	* @return Returns true if successful.
	*/
	bool AddParent(FRigBaseElement* InChild, FRigBaseElement* InParent, float InWeight = 0.f, bool bMaintainGlobalTransform = true, bool bRemoveAllParents = false);

	/**
	 * Removes an existing parent from an element in the hierarchy. For elements that allow only one parent the element will be unparented (same as ::RemoveAllParents)
	 * @param InChild The key of the element to remove the parent for
	 * @param InParent The key of the parent to remove
	 * @param bMaintainGlobalTransform If set to true the child will stay in the same place spatially, otherwise it will maintain it's local transform (and potential move).
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return Returns true if successful.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	bool RemoveParent(FRigElementKey InChild, FRigElementKey InParent, bool bMaintainGlobalTransform = true, bool bSetupUndo = false, bool bPrintPythonCommand = false);

	/**
 	 * Removes all parents from an element in the hierarchy.
 	 * @param InChild The key of the element to remove all parents for
 	 * @param bMaintainGlobalTransform If set to true the child will stay in the same place spatially, otherwise it will maintain it's local transform (and potential move).
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return Returns true if successful.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	bool RemoveAllParents(FRigElementKey InChild, bool bMaintainGlobalTransform = true, bool bSetupUndo = false, bool bPrintPythonCommand = false);

	/**
	 * Sets a new parent to an element. For elements that allow more than one parent the parent list will be replaced.
	 * @param InChild The key of the element to set the parent for
	 * @param InParent The key of the new parent to set
	 * @param bMaintainGlobalTransform If set to true the child will stay in the same place spatially, otherwise it will maintain it's local transform (and potential move).
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return Returns true if successful.
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	bool SetParent(FRigElementKey InChild, FRigElementKey InParent, bool bMaintainGlobalTransform = true, bool bSetupUndo = false, bool bPrintPythonCommand = false);

	/**
	 * Duplicate the given elements
	 * @param InKeys The keys of the elements to duplicate
	 * @param bSelectNewElements If set to true the new elements will be selected
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return The keys of the 4d items
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    TArray<FRigElementKey> DuplicateElements(TArray<FRigElementKey> InKeys, bool bSelectNewElements = true, bool bSetupUndo = false, bool bPrintPythonCommands = false);

	/**
	 * Mirrors the given elements
	 * @param InKeys The keys of the elements to mirror
	 * @param InSettings The settings to use for the mirror operation
	 * @param bSelectNewElements If set to true the new elements will be selected
	 * @param bSetupUndo If set to true the stack will record the change for undo / redo
	 * @param bPrintPythonCommand If set to true a python command equivalent to this call will be printed out
	 * @return The keys of the mirrored items
	 */
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
    TArray<FRigElementKey> MirrorElements(TArray<FRigElementKey> InKeys, FRigMirrorSettings InSettings, bool bSelectNewElements = true, bool bSetupUndo = false, bool bPrintPythonCommands = false);

	/**
	 * Returns the modified event, which can be used to 
	 * subscribe to topological changes happening within the hierarchy.
	 * @return The event used for subscription.
	 */
	FRigHierarchyModifiedEvent& OnModified() { return ModifiedEvent; }

	/**
	 * Reports a warning to the console. This does nothing if bReportWarningsAndErrors is false.
	 * @param InMessage The warning message to report.
	 */
	void ReportWarning(const FString& InMessage) const;

	/**
	 * Reports an error to the console. This does nothing if bReportWarningsAndErrors is false.
	 * @param InMessage The error message to report.
	 */
	void ReportError(const FString& InMessage) const;

	/**
	 * Reports an error to the console and logs a notification to the UI. This does nothing if bReportWarningsAndErrors is false.
	 * @param InMessage The error message to report / notify.
	 */
	void ReportAndNotifyError(const FString& InMessage) const;

	template <typename FmtType, typename... Types>
    void ReportWarningf(const FmtType& Fmt, Types... Args) const
	{
		ReportWarning(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
    void ReportErrorf(const FmtType& Fmt, Types... Args) const
	{
		ReportError(FString::Printf(Fmt, Args...));
	}

	template <typename FmtType, typename... Types>
    void ReportAndNotifyErrorf(const FmtType& Fmt, Types... Args) const
	{
		ReportAndNotifyError(FString::Printf(Fmt, Args...));
	}

	UPROPERTY(transient)
	bool bReportWarningsAndErrors;

	/**
	 * Returns a reference to the suspend notifications flag
	 */
	FORCEINLINE bool& GetSuspendNotificationsFlag() { return bSuspendAllNotifications; }

#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = URigHierarchyController)
	TArray<FString> GeneratePythonCommands();

	TArray<FString> GetAddElementPythonCommands(FRigBaseElement* Element) const;

	TArray<FString> GetAddBonePythonCommands(FRigBoneElement* Bone) const;

	TArray<FString> GetAddNullPythonCommands(FRigNullElement* Null) const;

	TArray<FString> GetAddControlPythonCommands(FRigControlElement* Control) const;

	TArray<FString> GetAddCurvePythonCommands(FRigCurveElement* Curve) const;

	TArray<FString> GetAddRigidBodyPythonCommands(FRigRigidBodyElement* RigidBody) const;

	TArray<FString> GetSetControlValuePythonCommands(const FRigControlElement* Control, const FRigControlValue& Value, const ERigControlValueType& Type) const;
	
	TArray<FString> GetSetControlOffsetTransformPythonCommands(const FRigControlElement* Control, const FTransform& Offset, bool bInitial = false, bool bAffectChildren = true) const;
	
	TArray<FString> GetSetControlShapeTransformPythonCommands(const FRigControlElement* Control, const FTransform& Transform, bool bInitial = false) const;
#endif
	
private:

	UPROPERTY(transient)
	TWeakObjectPtr<URigHierarchy> Hierarchy;

	FRigHierarchyModifiedEvent ModifiedEvent;
	void Notify(ERigHierarchyNotification InNotifType, const FRigBaseElement* InElement);
	void HandleHierarchyModified(ERigHierarchyNotification InNotifType, URigHierarchy* InHierarchy, const FRigBaseElement* InElement) const;

	/**
	 * Returns true if this controller is valid / linked to a valid hierarchy.
	 * @return Returns true if this controller is valid / linked to a valid hierarchy.
	 */
	FORCEINLINE bool IsValid() const { return Hierarchy.IsValid(); }

	/**
	 * Adds a new element to the hierarchy
	 * @param InElementToAdd The new element to add to the hierarchy 
	 * @param InFirstParent The (optional) parent of the new bone. If you don't need a parent, pass nullptr
	 * @param bMaintainGlobalTransform If set to true the child will stay in the same place spatially, otherwise it will maintain it's local transform (and potential move).
	 * @return The index of the newly added element
	 */
	int32 AddElement(FRigBaseElement* InElementToAdd, FRigBaseElement* InFirstParent, bool bMaintainGlobalTransform);

	/**
	 * Removes an existing element from the hierarchy.
	 * @param InElement The element to remove
	 * @return Returns true if successful.
	 */
	bool RemoveElement(FRigBaseElement* InElement);

	/**
	 * Renames an existing element in the hierarchy
	 * @param InElement The element to rename
	 * @param InName The new name to set for the element
	 * @param bClearSelection True if the selection should be cleared after a rename
	 * @return Returns true if successful.
	 */
    bool RenameElement(FRigBaseElement* InElement, const FName &InName, bool bClearSelection = true);

	/**
 	 * Sets the display name on a control
 	 * @param InControlElement The element to change the display name for
 	 * @param InDisplayName The new display name to set for the control
 	 * @param bRenameElement True if the control should also be renamed
 	 * @return Returns true if successful.
 	 */
	FName SetDisplayName(FRigControlElement* InControlElement, const FName &InDisplayName, bool bRenameElement = false);

	/**
	 * Removes an existing parent from an element in the hierarchy. For elements that allow only one parent the element will be unparented (same as ::RemoveAllParents)
	 * @param InChild The element to remove the parent for
	 * @param InParent The parent to remove
	 * @param bMaintainGlobalTransform If set to true the child will stay in the same place spatially, otherwise it will maintain it's local transform (and potential move).
	 * @return Returns true if successful.
	 */
	bool RemoveParent(FRigBaseElement* InChild, FRigBaseElement* InParent, bool bMaintainGlobalTransform = true);

	/**
	 * Removes all parents from an element in the hierarchy.
	 * @param InChild The element to remove all parents for
	 * @param bMaintainGlobalTransform If set to true the child will stay in the same place spatially, otherwise it will maintain it's local transform (and potential move).
	 * @return Returns true if successful.
	 */
	bool RemoveAllParents(FRigBaseElement* InChild, bool bMaintainGlobalTransform = true);

	/**
	 * Sets a new parent to an element. For elements that allow more than one parent the parent list will be replaced.
	 * @param InChild The element to set the parent for
	 * @param InParent The new parent to set
	 * @param bMaintainGlobalTransform If set to true the child will stay in the same place spatially, otherwise it will maintain it's local transform (and potential move).
	 * @return Returns true if successful.
 	 */
	bool SetParent(FRigBaseElement* InChild, FRigBaseElement* InParent, bool bMaintainGlobalTransform = true);

	/**
	 * Adds a new element to the dirty list of the given parent.
	 * This function is recursive and will affect all parents in the tree.
	 * @param InParent The parent element to change the dirty list for
	 * @param InElementToAdd The child element to add to the dirty list
	 * @param InHierarchyDistance The distance of number of elements in the hierarchy
	 */
	void AddElementToDirty(FRigBaseElement* InParent, FRigBaseElement* InElementToAdd, int32 InHierarchyDistance = 1) const;

	/**
	 * Remove an existing element to the dirty list of the given parent.
	 * This function is recursive and will affect all parents in the tree.
	 * @param InParent The parent element to change the dirty list for
	 * @param InElementToRemove The child element to remove from the dirty list
	 */
	void RemoveElementToDirty(FRigBaseElement* InParent, FRigBaseElement* InElementToRemove) const;

#if WITH_EDITOR
	static USkeleton* GetSkeletonFromAssetPath(const FString& InAssetPath);
#endif

	/** 
	 * If set to true all notifications coming from this hierarchy will be suspended
	 */
	bool bSuspendAllNotifications;

	/** 
	 * If set to true selection related notifications coming from this hierarchy will be suspended
	 */
	bool bSuspendSelectionNotifications;

	/** 
	* If set to true all python printing can be disabled.  
	*/
	bool bSuspendPythonPrinting;

	/**
	 * If set the controller will mark new items as procedural and created at the current instruction
	 */
	int32 CurrentInstructionIndex;

	/**
	 * This function can be used to override the controller's logging mechanism
	 */
	TFunction<void(EMessageSeverity::Type,const FString&)> LogFunction = nullptr;

	template<typename T>
	T* MakeElement()
	{
		T* Element = GetHierarchy()->NewElement<T>();
		Element->CreatedAtInstructionIndex = CurrentInstructionIndex;
		return Element;
	}
	
	friend class UControlRig;
	friend class URigHierarchy;
	friend class FRigHierarchyControllerInstructionBracket;
};

class CONTROLRIG_API FRigHierarchyControllerInstructionBracket : TGuardValue<int32>
{
public:
	
	FRigHierarchyControllerInstructionBracket(URigHierarchyController* InController, int32 InInstructionIndex)
		: TGuardValue<int32>(InController->CurrentInstructionIndex, InInstructionIndex)
	{
	}
};
