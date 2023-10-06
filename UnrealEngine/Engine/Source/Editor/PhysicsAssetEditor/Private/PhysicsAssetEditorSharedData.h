// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PreviewScene.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/ShapeElem.h"
#include "Preferences/PhysicsAssetEditorOptions.h"

class UBodySetup;
class UPhysicsAssetEditorSkeletalMeshComponent;
class UPhysicsAsset;
class UPhysicsConstraintTemplate;
class UPhysicsAssetEditorPhysicsHandleComponent;
class USkeletalMesh;
class UStaticMeshComponent;
struct FBoneVertInfo;
class IPersonaPreviewScene;
class FPhysicsAssetEditorSharedData;

/** Scoped object that blocks selection broadcasts until it leaves scope */
struct FScopedBulkSelection
{
	FScopedBulkSelection(TSharedPtr<FPhysicsAssetEditorSharedData> InSharedData);
	~FScopedBulkSelection();

	TSharedPtr<FPhysicsAssetEditorSharedData> SharedData;
};

/*-----------------------------------------------------------------------------
   FPhysicsAssetEditorSharedData
-----------------------------------------------------------------------------*/

class FPhysicsAssetEditorSharedData
{
public:
	/** Constructor/Destructor */
	FPhysicsAssetEditorSharedData();
	virtual ~FPhysicsAssetEditorSharedData();

	enum EPhysicsAssetEditorConstraintType
	{
		PCT_Swing1,
		PCT_Swing2,
		PCT_Twist,
	};

	/** Encapsulates a selected set of bodies or constraints */
	struct FSelection
	{
		int32 Index;
		EAggCollisionShape::Type PrimitiveType;
		int32 PrimitiveIndex;
		FTransform WidgetTM;
		FTransform ManipulateTM;

		FSelection(int32 GivenBodyIndex, EAggCollisionShape::Type GivenPrimitiveType, int32 GivenPrimitiveIndex) :
			Index(GivenBodyIndex), PrimitiveType(GivenPrimitiveType), PrimitiveIndex(GivenPrimitiveIndex),
			WidgetTM(FTransform::Identity), ManipulateTM(FTransform::Identity)
		{
		}

		bool operator==(const FSelection& rhs) const
		{
			return Index == rhs.Index && PrimitiveType == rhs.PrimitiveType && PrimitiveIndex == rhs.PrimitiveIndex;
		}
	};

	/** Initializes members */
	void Initialize(const TSharedRef<IPersonaPreviewScene>& InPreviewScene);

	/** Caches a preview mesh. Sets us to a default mesh if none is set yet (or if an older one got deleted) */
	void CachePreviewMesh();

	/** Accessor for mesh view mode, allows access for simulation and non-simulation modes */
	EPhysicsAssetEditorMeshViewMode GetCurrentMeshViewMode(bool bSimulation);

	/** Accessor for collision view mode, allows access for simulation and non-simulation modes */
	EPhysicsAssetEditorCollisionViewMode GetCurrentCollisionViewMode(bool bSimulation);

	/** Accessor for constraint view mode, allows access for simulation and non-simulation modes */
	EPhysicsAssetEditorConstraintViewMode GetCurrentConstraintViewMode(bool bSimulation);
	
	/** Clear all of the selected constraints */
	void ClearSelectedConstraints();

	/** Set the selection state of a constraint */
	void SetSelectedConstraint(int32 ConstraintIndex, bool bSelected);
	void SetSelectedConstraints(const TArray<int32> ConstraintsIndices, bool bSelected);

	/** Check whether the constraint at the specified index is selected */
	bool IsConstraintSelected(int32 ConstraintIndex) const;

	/** Get the world transform of the specified selected constraint */
	FTransform GetConstraintWorldTM(const FSelection* Constraint, EConstraintFrame::Type Frame) const;

	/** Get the world transform of the specified constraint */
	FTransform GetConstraintWorldTM(const UPhysicsConstraintTemplate* const ConstraintSetup, const EConstraintFrame::Type Frame, const float Scale = 1.0f) const;

	/** Get the world transform of the specified constraint */
	FTransform GetConstraintMatrix(int32 ConstraintIndex, EConstraintFrame::Type Frame, float Scale) const;
	
	/** Get the body transform of the specified constraint */
	FTransform GetConstraintBodyTM(const UPhysicsConstraintTemplate* ConstraintSetup, EConstraintFrame::Type Frame) const;

	/** Set the constraint relative transform */
    void SetConstraintRelTM(const FSelection* Constraint, const FTransform& RelTM);

	/** Set the constraint relative transform for a single selected constraint */
    inline void SetSelectedConstraintRelTM(const FTransform& RelTM)
    {
        SetConstraintRelTM(GetSelectedConstraint(), RelTM);
    }

	/** Snaps a constraint at the specified index to it's bone */
	void SnapConstraintToBone(const int32 ConstraintIndex, const EConstraintTransformComponentFlags ComponentFlags = EConstraintTransformComponentFlags::All);

	/** Snaps the specified constraint to it's bone */
	void SnapConstraintToBone(FConstraintInstance& ConstraintInstance, const EConstraintTransformComponentFlags ComponentFlags = EConstraintTransformComponentFlags::All);

	/** Deletes the currently selected constraints */
	void DeleteCurrentConstraint();

	/** Paste the currently-copied constraint properties onto the single selected constraint */
	void PasteConstraintProperties();
	
	/** Cycles the rows of the transform matrix for the selected constraint. Assumes the selected constraint
	  * is valid and that we are in constraint editing mode*/
	void CycleCurrentConstraintOrientation();

	/** Cycles the active constraint*/
	void CycleCurrentConstraintActive();

	/** Cycles the active constraint between locked and limited */
	void ToggleConstraint(EPhysicsAssetEditorConstraintType Constraint);

	/** Gets whether the active constraint is locked */
	bool IsAngularConstraintLocked(EPhysicsAssetEditorConstraintType Constraint) const;

	/** Collision geometry editing */
	void ClearSelectedBody();
	void SetSelectedBody(const FSelection& Body, bool bSelected);
	void SetSelectedBodies(const TArray<FSelection>& Bodies, bool bSelected);
	bool IsBodySelected(const FSelection& Body) const;
	void ToggleSelectionType(bool bIgnoreUserConstraints = true);
	void ToggleShowSelected();
	bool IsBodyHidden(const int32 BodyIndex) const;
	bool IsConstraintHidden(const int32 ConstraintIndex) const;
	void HideBody(const int32 BodyIndex);
	void ShowBody(const int32 BodyIndex);
	void HideConstraint(const int32 ConstraintIndex);
	void ShowConstraint(const int32 ConstraintIndex);
	void ShowAll();
	void HideAll();
	void HideAllBodies();
	void HideAllConstraints();
	void ToggleShowOnlyColliding();
	void ToggleShowOnlyConstrained();
	void ToggleShowOnlySelected();
	void ShowSelected();
	void HideSelected();
	void SetSelectedBodyAnyPrimitive(int32 BodyIndex, bool bSelected);
	void SetSelectedBodiesAnyPrimitive(const TArray<int32>& BodiesIndices, bool bSelected);
	void SetSelectedBodiesAllPrimitive(const TArray<int32>& BodiesIndices, bool bSelected);
	void SetSelectedBodiesPrimitivesWithCollisionType(const TArray<int32>& BodiesIndices, const ECollisionEnabled::Type CollisionType, bool bSelected);
	void SetSelectedBodiesPrimitives(const TArray<int32>& BodiesIndices, bool bSelected, const TFunction<bool(const TArray<FSelection>&, const int32 BodyIndex, const FKShapeElem&)>& Predicate);
	void DeleteCurrentPrim();
	void DeleteBody(int32 DelBodyIndex, bool bRefreshComponent=true);
	void RefreshPhysicsAssetChange(const UPhysicsAsset* InPhysAsset, bool bFullClothRefresh = true);
	void MakeNewBody(int32 NewBoneIndex, bool bAutoSelect = true);
	void MakeNewConstraints(int32 ParentBodyIndex, const TArray<int32>& ChildBodyIndices);
	void MakeNewConstraint(int32 ParentBodyIndex, int32 ChildBodyIndex);
	void CopySelectedBodiesAndConstraintsToClipboard(int32& OutNumCopiedBodies, int32& OutNumCopiedConstraints);
	bool CanPasteBodiesAndConstraintsFromClipboard() const;
	void PasteBodiesAndConstraintsFromClipboard(int32& OutNumPastedBodies, int32& OutNumPastedConstraints);
	void CopySelectedShapesToClipboard(int32& OutNumCopiedShapes, int32& OutNumBodiesCopiedFrom);
	bool CanPasteShapesFromClipboard() const;
	void PasteShapesFromClipboard(int32& OutNumPastedShapes, int32& OutNumBodiesPastedInto);
	void CopyBodyProperties();
	void CopyConstraintProperties();
	void PasteBodyProperties();
	bool WeldSelectedBodies(bool bWeld = true);
	void Mirror();

	/** auto name a primitive, if PrimitiveIndex is INDEX_NONE, then the last primitive of specified typed is renamed */
	void AutoNamePrimitive(int32 BodyIndex, EAggCollisionShape::Type PrimitiveType, int32 PrimitiveIndex = INDEX_NONE);
	void AutoNameAllPrimitives(int32 BodyIndex, EAggCollisionShape::Type PrimitiveType);
	void AutoNameAllPrimitives(int32 BodyIndex, EPhysAssetFitGeomType PrimitiveType);

	/** Toggle simulation on and off */
	void ToggleSimulation();

	/** Open a new body dialog */
	void OpenNewBodyDlg();

	/** Open a new body dialog, filling in NewBodyResponse when the dialog is closed */
	static void OpenNewBodyDlg(EAppReturnType::Type* NewBodyResponse);

	/** Helper function for creating the details panel widget and other controls that form the New body dialog (used by OpenNewBodyDlg and the tools tab) */
	static TSharedRef<SWidget> CreateGenerateBodiesWidget(const FSimpleDelegate& InOnCreate, const FSimpleDelegate& InOnCancel = FSimpleDelegate(), const TAttribute<bool>& InIsEnabled = TAttribute<bool>(), const TAttribute<FText>& InCreateButtonText = TAttribute<FText>(), bool bForNewAsset = false);

	/** Handle clicking on a body */
	void HitBone(int32 BodyIndex, EAggCollisionShape::Type PrimType, int32 PrimIndex, bool bGroupSelect);

	/** Handle clikcing on a constraint */
	void HitConstraint(int32 ConstraintIndex, bool bGroupSelect);

	/** Undo/Redo */
	void PostUndo();
	void Redo();

	/** Helpers to enable/disable collision on selected bodies */
	void SetCollisionBetweenSelected(bool bEnableCollision);
	bool CanSetCollisionBetweenSelected(bool bEnableCollision) const;
	void SetCollisionBetweenSelectedAndAll(bool bEnableCollision);
	bool CanSetCollisionBetweenSelectedAndAll(bool bEnableCollision) const;

	/** Helpers to set primitive-level collision filtering on selected bodies */
	void SetPrimitiveCollision(ECollisionEnabled::Type CollisionEnabled);
	bool CanSetPrimitiveCollision(ECollisionEnabled::Type CollisionEnabled) const;
	bool GetIsPrimitiveCollisionEnabled(ECollisionEnabled::Type CollisionEnabled) const;
	void SetPrimitiveContributeToMass(bool bContributeToMass);
	bool CanSetPrimitiveContributeToMass() const;
	bool GetPrimitiveContributeToMass() const;

	/** Prevents GC from collecting our objects */
	void AddReferencedObjects(FReferenceCollector& Collector);

	/** Enables and disables simulation. Used by ToggleSimulation */
	void EnableSimulation(bool bEnableSimulation);

	/** Force simulation off for all bodies, regardless of physics type */
	void ForceDisableSimulation();

	/** Update the clothing simulation's (if any) collision */
	void UpdateClothPhysics();

	/** broadcast a selection change ( if bSuspendSelectionBroadcast is false ) */
	void BroadcastSelectionChanged();

	/** broadcast a change in the hierarchy */
	void BroadcastHierarchyChanged();

	/** broadcast a change in the preview*/
	void BroadcastPreviewChanged();

	/** Returns true if the clipboard contains data this class can process */
	static bool ClipboardHasCompatibleData();

	/** Control whether we draw a CoM marker in the viewport */
	void ToggleShowCom();
	void SetShowCom(bool InValue);
	bool GetShowCom() const;

private:
	/** Initializes a constraint setup */
	void InitConstraintSetup(UPhysicsConstraintTemplate* ConstraintSetup, int32 ChildBodyIndex, int32 ParentBodyIndex);

	/** Collision editing helper methods */
	void SetCollisionBetween(int32 Body1Index, int32 Body2Index, bool bEnableCollision);

	/** Update the cached array of bodies that do not collide with the current body selection */
	void UpdateNoCollisionBodies();

	/** Copy the properties of the one and only selected constraint */
	void CopyConstraintProperties(const UPhysicsConstraintTemplate * FromConstraintSetup, UPhysicsConstraintTemplate * ToConstraintSetup, bool bKeepOldRotation = false);

	/** Copies a reference to a given element to the clipboard */
	void CopyToClipboard(const FString& ObjectType, UObject* Object);

	/** Pastes data from the clipboard on a given type */
	bool PasteFromClipboard(const FString& InObjectType, UPhysicsAsset*& OutAsset, UObject*& OutObject);

	/** Clears data in clipboard if it was pointing to the given type/data */
	void ConditionalClearClipboard(const FString& ObjectType, UObject* Object);

	/** Checks and parses clipboard data */
	static bool ParseClipboard(UPhysicsAsset*& OutAsset, FString& OutObjectType, UObject*& OutObject);

	/** Gneerate a new unique name for a constraint */
	FString MakeUniqueNewConstraintName();

public:
	/** Callback for handling selection changes */
	DECLARE_EVENT_TwoParams(FPhysicsAssetEditorSharedData, FSelectionChanged, const TArray<FSelection>&, const TArray<FSelection>&);
	FSelectionChanged SelectionChangedEvent;

	/** Callback for handling changes to the bone/body/constraint hierarchy */
	DECLARE_EVENT(FPhysicsAssetEditorSharedData, FHierarchyChanged);
	FHierarchyChanged HierarchyChangedEvent;

	/** Callback for handling changes to the current selection in the tree */
	DECLARE_EVENT(FPhysicsAssetEditorSharedData, FHierarchySelectionChangedEvent);
	FHierarchySelectionChangedEvent HierarchySelectionChangedEvent;
	

	/** Callback for triggering a refresh of the preview viewport */
	DECLARE_EVENT(FPhysicsAssetEditorSharedData, FPreviewChanged);
	FPreviewChanged PreviewChangedEvent;

	/** The PhysicsAsset asset being inspected */
	TObjectPtr<UPhysicsAsset> PhysicsAsset;

	/** PhysicsAssetEditor specific skeletal mesh component */
	TObjectPtr<UPhysicsAssetEditorSkeletalMeshComponent> EditorSkelComp;

	/** PhysicsAssetEditor specific physical animation component */
	TObjectPtr<class UPhysicalAnimationComponent> PhysicalAnimationComponent;

	/** Preview scene */
	TWeakPtr<IPersonaPreviewScene> PreviewScene;

	/** Editor options */
	TObjectPtr<UPhysicsAssetEditorOptions> EditorOptions;

	/** Results from the new body dialog */
	EAppReturnType::Type NewBodyResponse;

	/** Helps define how the asset behaves given user interaction in simulation mode*/
	TObjectPtr<UPhysicsAssetEditorPhysicsHandleComponent> MouseHandle;

	/** Draw color for center of mass debug strings */
	const FColor COMRenderColor;

	/** List of bodies that don't collide with the currently selected collision body */
	TArray<int32> NoCollisionBodies;

	/** Bone info */
	TArray<FBoneVertInfo> DominantWeightBoneInfos;
	TArray<FBoneVertInfo> AnyWeightBoneInfos;

	TArray<FSelection> SelectedBodies;

	FSelection * GetSelectedBody()
	{
		int32 Count = SelectedBodies.Num();
		return Count ? &SelectedBodies[Count - 1] : NULL;
	}

	/** Constraint editing */
	TArray<FSelection> SelectedConstraints;
	FSelection * GetSelectedConstraint()
	{
		int32 Count = SelectedConstraints.Num();
		return Count ? &SelectedConstraints[Count - 1] : NULL;
	}

	const FSelection * GetSelectedConstraint() const
	{
		int32 Count = SelectedConstraints.Num();
		return Count ? &SelectedConstraints[Count - 1] : NULL;
	}

	struct FPhysicsAssetRenderSettings* GetRenderSettings() const;

	/** Misc toggles */
	bool bRunningSimulation;
	bool bNoGravitySimulation;

	/** Manipulation (rotate, translate, scale) */
	bool bManipulating;

	/** when true, we dont broadcast every selection change - allows for bulk changes without so much overhead */
	bool bSuspendSelectionBroadcast;

	/** Used to prevent recursion with tree hierarchy ... needs to be rewritten! */
	int32 InsideSelChange;

	FTransform ResetTM;

	FIntPoint LastClickPos;
	FVector LastClickOrigin;
	FVector LastClickDirection;
	FVector LastClickHitPos;
	FVector LastClickHitNormal;
	bool bLastClickHit;
};
