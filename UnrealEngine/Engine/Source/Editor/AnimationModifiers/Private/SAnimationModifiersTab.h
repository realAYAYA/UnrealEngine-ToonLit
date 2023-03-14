// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "EditorUndoClient.h"
#include "Input/Reply.h"
#include "SModifierListview.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class IAssetEditorInstance;
class IDetailsView;
class SMenuAnchor;
class UAnimSequence;
class UAnimationModifier;
class UAnimationModifiersAssetUserData;
class UBlueprint;
class UClass;
class UObject;
class USkeleton;
struct FGeometry;

class ANIMATIONMODIFIERS_API SAnimationModifiersTab : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SAnimationModifiersTab)
	{}	
	SLATE_ARGUMENT(TWeakPtr<class FAssetEditorToolkit>, InHostingApp)
	SLATE_END_ARGS()

	SAnimationModifiersTab();
	~SAnimationModifiersTab();

	/** SWidget functions */
	void Construct(const FArguments& InArgs);

	/** Begin SCompoundWidget */
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	/** End SCompoundWidget */

	/** Begin FEditorUndoClient */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;
	/** End FEditorUndoClient */
protected:
	/** Callback for when user has picked a modifier to add */
	void OnModifierPicked(UClass* PickedClass);

	void CreateInstanceDetailsView();	

	/** UI apply all modifiers button callback */
	FReply OnApplyAllModifiersClicked();

	/** Callbacks for available modifier actions */
	void OnApplyModifier(const TArray<TWeakObjectPtr<UAnimationModifier>>& Instances);	
	void OnRevertModifier(const TArray<TWeakObjectPtr<UAnimationModifier>>& Instances);
	bool OnCanRevertModifier(const TArray<TWeakObjectPtr<UAnimationModifier>>& Instances);	
	void OnRemoveModifier(const TArray<TWeakObjectPtr<UAnimationModifier>>& Instances);
	void OnOpenModifier(const TWeakObjectPtr<UAnimationModifier>& Instance);

	void OnMoveModifierUp(const TWeakObjectPtr<UAnimationModifier>& Instance);
	void OnMoveModifierDown(const TWeakObjectPtr<UAnimationModifier>& Instance);

	/** Flags UI dirty and will refresh during the next Tick*/
	void Refresh();

	/** Callback for compiled blueprints, this ensures to refresh the UI */
	void OnBlueprintCompiled(UBlueprint* Blueprint);
	/** Callback to keep track of when an asset is opened, this is necessary for when an editor document tab is reused and this tab isn't recreated */
	void OnAssetOpened(UObject* Object, IAssetEditorInstance* Instance);

	/** Applying and reverting of modifiers */
	void ApplyModifiers(const TArray<UAnimationModifier*>& Modifiers);
	void RevertModifiers(const TArray<UAnimationModifier*>& Modifiers);

	/** Retrieves the currently opened animation asset type and modifier user data */
	void RetrieveAnimationAsset();

	/** Retrieves all animation sequences which are dependent on the current opened skeleton */
	void FindAnimSequencesForSkeleton(TArray<UAnimSequence *> &ReferencedAnimSequences);
protected:
	TWeakPtr<class FAssetEditorToolkit> HostingApp;

	/** Retrieved currently open animation asset type */
	USkeleton* Skeleton;
	UAnimSequence* AnimationSequence;
	/** Asset user data retrieved from AnimSequence or Skeleton */
	UAnimationModifiersAssetUserData* AssetUserData;
	/** List of blueprints for which a delegate was registered for OnCompiled */
	TArray<UBlueprint*> DelegateRegisteredBlueprints;
	/** Flag whether or not the UI should be refreshed  */
	bool bDirty;
protected:
	/** UI elements and data */
	TSharedPtr<IDetailsView> ModifierInstanceDetailsView;
	TArray<ModifierListviewItem> ModifierItems;
	TSharedPtr<SModifierListView> ModifierListView;
	TSharedPtr<SMenuAnchor> AddModifierCombobox;	
private:
	void RetrieveModifierData();
	void ResetModifierData();
};
