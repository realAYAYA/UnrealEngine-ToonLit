// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ControlRig.h"
#include "Rigs/RigHierarchy.h"
#include "Editor/SRigHierarchy.h"
#include "Widgets/Layout/SBox.h"
#include "Rigs/RigSpaceHierarchy.h"
#include "IStructureDetailsView.h"
#include "Misc/FrameNumber.h"
#include "EditorUndoClient.h"
#include "RigSpacePickerBakeSettings.h"


DECLARE_DELEGATE_RetVal_TwoParams(FRigElementKey, FRigSpacePickerGetActiveSpace, URigHierarchy*, const FRigElementKey&);
DECLARE_DELEGATE_RetVal_TwoParams(const FRigControlElementCustomization*, FRigSpacePickerGetControlCustomization, URigHierarchy*, const FRigElementKey&);
DECLARE_EVENT_ThreeParams(SRigSpacePickerWidget, FRigSpacePickerActiveSpaceChanged, URigHierarchy*, const FRigElementKey&, const FRigElementKey&);
DECLARE_EVENT_ThreeParams(SRigSpacePickerWidget, FRigSpacePickerSpaceListChanged, URigHierarchy*, const FRigElementKey&, const TArray<FRigElementKey>&);
DECLARE_DELEGATE_RetVal_TwoParams(TArray<FRigElementKey>, FRigSpacePickerGetAdditionalSpaces, URigHierarchy*, const FRigElementKey&);
DECLARE_DELEGATE_RetVal_ThreeParams(FReply, SRigSpacePickerOnBake, URigHierarchy*, TArray<FRigElementKey> /* Controls */, FRigSpacePickerBakeSettings);

/** Widget allowing picking of a space source for space switching */
class CONTROLRIGEDITOR_API SRigSpacePickerWidget : public SCompoundWidget, public FEditorUndoClient
{
public:

	SLATE_BEGIN_ARGS(SRigSpacePickerWidget)
		: _Hierarchy(nullptr)
		, _Controls()
		, _ShowDefaultSpaces(true)
		, _ShowFavoriteSpaces(true)
		, _ShowAdditionalSpaces(true)
		, _AllowReorder(false)
		, _AllowDelete(false)
		, _AllowAdd(false)
		, _ShowBakeButton(false)
		, _Title()
		, _BackgroundBrush(FAppStyle::GetBrush("Menu.Background"))
		{}
		SLATE_ARGUMENT(URigHierarchy*, Hierarchy)
		SLATE_ARGUMENT(TArray<FRigElementKey>, Controls)
		SLATE_ARGUMENT(bool, ShowDefaultSpaces)
		SLATE_ARGUMENT(bool, ShowFavoriteSpaces)
		SLATE_ARGUMENT(bool, ShowAdditionalSpaces)
		SLATE_ARGUMENT(bool, AllowReorder)
		SLATE_ARGUMENT(bool, AllowDelete)
		SLATE_ARGUMENT(bool, AllowAdd)
		SLATE_ARGUMENT(bool, ShowBakeButton)
		SLATE_ARGUMENT(FText, Title)
		SLATE_ARGUMENT(const FSlateBrush*, BackgroundBrush)
	
		SLATE_EVENT(FRigSpacePickerGetActiveSpace, GetActiveSpace)
		SLATE_EVENT(FRigSpacePickerGetControlCustomization, GetControlCustomization)
		SLATE_EVENT(FRigSpacePickerActiveSpaceChanged::FDelegate, OnActiveSpaceChanged)
		SLATE_EVENT(FRigSpacePickerSpaceListChanged::FDelegate, OnSpaceListChanged)
		SLATE_ARGUMENT(FRigSpacePickerGetAdditionalSpaces, GetAdditionalSpaces)
		SLATE_EVENT( FOnClicked, OnBakeButtonClicked )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SRigSpacePickerWidget() override;

	void SetControls(URigHierarchy* InHierarchy, const TArray<FRigElementKey>& InControls);

	FReply OpenDialog(bool bModal = true);
	void CloseDialog();
	
	virtual FReply OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent ) override;
	virtual bool SupportsKeyboardFocus() const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	URigHierarchy* GetHierarchy() const
	{
		if (Hierarchy.IsValid())
		{
			return Hierarchy.Get();
		}
		return nullptr;
	}
	const URigHierarchy* GetHierarchyConst() const { return GetHierarchy(); }
	
	const TArray<FRigElementKey>& GetControls() const { return ControlKeys; }
	const TArray<FRigElementKey>& GetActiveSpaces() const;
	TArray<FRigElementKey> GetDefaultSpaces() const;
	TArray<FRigElementKey> GetSpaceList(bool bIncludeDefaultSpaces = false) const;
	FRigSpacePickerActiveSpaceChanged& OnActiveSpaceChanged() { return ActiveSpaceChangedEvent; }
	FRigSpacePickerSpaceListChanged& OnSpaceListChanged() { return SpaceListChangedEvent; }
	void RefreshContents();

	// FEditorUndoClient interface
	virtual void PostUndo(bool bSuccess);
	virtual void PostRedo(bool bSuccess);
	// End FEditorUndoClient interface
	
private:

	enum ESpacePickerType
	{
		ESpacePickerType_Parent,
		ESpacePickerType_World,
		ESpacePickerType_Item
	};

	void AddSpacePickerRow(
		TSharedPtr<SVerticalBox> InListBox,
		ESpacePickerType InType,
		const FRigElementKey& InKey,
		const FSlateBrush* InBush,
		const FSlateColor& InColor,
		const FText& InTitle,
		FOnClicked OnClickedDelegate);

	void RepopulateItemSpaces();
	void ClearListBox(TSharedPtr<SVerticalBox> InListBox);
	void UpdateActiveSpaces();
	bool IsValidKey(const FRigElementKey& InKey) const;
	bool IsDefaultSpace(const FRigElementKey& InKey) const;

	FReply HandleParentSpaceClicked();
	FReply HandleWorldSpaceClicked();
	FReply HandleElementSpaceClicked(FRigElementKey InKey);
	FReply HandleSpaceMoveUp(FRigElementKey InKey);
	FReply HandleSpaceMoveDown(FRigElementKey InKey);
	void HandleSpaceDelete(FRigElementKey InKey);

public:
	
	FReply HandleAddElementClicked();

private:
	
	bool IsSpaceMoveUpEnabled(FRigElementKey InKey) const;
	bool IsSpaceMoveDownEnabled(FRigElementKey InKey) const;

	void OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement);

	FSlateColor GetButtonColor(ESpacePickerType InType, FRigElementKey InKey) const;
	FRigElementKey GetActiveSpace_Private(URigHierarchy* InHierarchy, const FRigElementKey& InControlKey) const;
	TArray<FRigElementKey> GetCurrentParents_Private(URigHierarchy* InHierarchy, const FRigElementKey& InControlKey) const;
	
	FRigSpacePickerActiveSpaceChanged ActiveSpaceChangedEvent;
	FRigSpacePickerSpaceListChanged SpaceListChangedEvent;

	TWeakObjectPtr<URigHierarchy> Hierarchy;
	TArray<FRigElementKey> ControlKeys;
	TArray<FRigElementKey> CurrentSpaceKeys;
	TArray<FRigElementKey> ActiveSpaceKeys;
	bool bRepopulateRequired;

	bool bShowDefaultSpaces;
	bool bShowFavoriteSpaces;
	bool bShowAdditionalSpaces;
	bool bAllowReorder;
	bool bAllowDelete;
	bool bAllowAdd;
	bool bShowBakeButton;
	bool bLaunchingContextMenu;

	FRigSpacePickerGetControlCustomization GetControlCustomizationDelegate;
	FRigSpacePickerGetActiveSpace GetActiveSpaceDelegate;
	FRigSpacePickerGetAdditionalSpaces GetAdditionalSpacesDelegate; 
	TArray<FRigElementKey> AdditionalSpaces;

	TSharedPtr<SVerticalBox> TopLevelListBox;
	TSharedPtr<SVerticalBox> ItemSpacesListBox;
	TSharedPtr<SHorizontalBox> BottomButtonsListBox;
	TWeakPtr<SWindow> DialogWindow;
	TWeakPtr<IMenu> ContextMenu;
	FDelegateHandle HierarchyModifiedHandle;
	FDelegateHandle ActiveSpaceChangedWindowHandle;

	static FRigElementKey InValidKey;
};

class ISequencer;

/** Widget allowing baking controls from one space to another */
class CONTROLRIGEDITOR_API SRigSpacePickerBakeWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SRigSpacePickerBakeWidget)
	: _Hierarchy(nullptr)
	, _Controls()
	, _Sequencer(nullptr)
	{}
	SLATE_ARGUMENT(URigHierarchy*, Hierarchy)
	SLATE_ARGUMENT(TArray<FRigElementKey>, Controls)
	SLATE_ARGUMENT(ISequencer*, Sequencer)
	SLATE_EVENT(FRigSpacePickerGetControlCustomization, GetControlCustomization)
	SLATE_ARGUMENT(FRigSpacePickerBakeSettings, Settings)
	SLATE_EVENT(SRigSpacePickerOnBake, OnBake)

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SRigSpacePickerBakeWidget() override {}

	FReply OpenDialog(bool bModal = true);
	void CloseDialog();

private:

	//FRigSpacePickerBakeSettings Settings;
	TSharedPtr<TStructOnScope<FRigSpacePickerBakeSettings>> Settings;

	ISequencer* Sequencer;
	FRigControlElementCustomization Customization;
	
	TWeakPtr<SWindow> DialogWindow;
	TSharedPtr<SRigSpacePickerWidget> SpacePickerWidget;
	TSharedPtr<IStructureDetailsView> DetailsView;
};
