// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SRigHierarchy.h"
#include "Widgets/Input/SComboButton.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Editor/ControlRigEditor.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintVariableNodeSpawner.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "K2Node_VariableGet.h"
#include "ControlRigBlueprintUtils.h"
#include "ControlRigHierarchyCommands.h"
#include "ControlRigBlueprint.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "Graph/ControlRigGraphSchema.h"
#include "GraphEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AnimationRuntime.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/EditorEngine.h"
#include "HelperUtil.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ControlRig.h"
#include "ControlRigEditorStyle.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformTime.h"
#include "Dialogs/Dialogs.h"
#include "IPersonaToolkit.h"
#include "SKismetInspector.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "Dialog/SCustomDialog.h"
#include "EditMode/ControlRigEditMode.h"
#include "ToolMenus.h"
#include "Editor/ControlRigContextMenuContext.h"
#include "Editor/SRigSpacePickerWidget.h"
#include "Settings/ControlRigSettings.h"
#include "Widgets/Views/SListPanel.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/AppStyle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SRigHierarchy)

#define LOCTEXT_NAMESPACE "SRigHierarchy"

//////////////////////////////////////////////////////////////
/// FRigElementHierarchyDragDropOp
///////////////////////////////////////////////////////////
TSharedRef<FRigElementHierarchyDragDropOp> FRigElementHierarchyDragDropOp::New(const TArray<FRigElementKey>& InElements)
{
	TSharedRef<FRigElementHierarchyDragDropOp> Operation = MakeShared<FRigElementHierarchyDragDropOp>();
	Operation->Elements = InElements;
	Operation->Construct();
	return Operation;
}

TSharedPtr<SWidget> FRigElementHierarchyDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(STextBlock)
			.Text(FText::FromString(GetJoinedElementNames()))
			//.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
		];
}

FString FRigElementHierarchyDragDropOp::GetJoinedElementNames() const
{
	TArray<FString> ElementNameStrings;
	for (const FRigElementKey& Element: Elements)
	{
		ElementNameStrings.Add(Element.Name.ToString());
	}
	return FString::Join(ElementNameStrings, TEXT(","));
}

///////////////////////////////////////////////////////////

SRigHierarchy::~SRigHierarchy()
{
	const FControlRigEditor* Editor = ControlRigEditor.IsValid() ? ControlRigEditor.Pin().Get() : nullptr;
	OnEditorClose(Editor, ControlRigBlueprint.Get());
}

void SRigHierarchy::Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor)
{
	ControlRigEditor = InControlRigEditor;

	ControlRigBlueprint = ControlRigEditor.Pin()->GetControlRigBlueprint();

	ControlRigBlueprint->Hierarchy->OnModified().AddRaw(this, &SRigHierarchy::OnHierarchyModified);
	ControlRigBlueprint->OnRefreshEditor().AddRaw(this, &SRigHierarchy::HandleRefreshEditorFromBlueprint);
	ControlRigBlueprint->OnSetObjectBeingDebugged().AddRaw(this, &SRigHierarchy::HandleSetObjectBeingDebugged);

	// for deleting, renaming, dragging
	CommandList = MakeShared<FUICommandList>();

	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->RegisterForUndo(this);
	}

	BindCommands();

	// setup all delegates for the rig hierarchy widget
	FRigTreeDelegates Delegates;
	Delegates.OnGetHierarchy = FOnGetRigTreeHierarchy::CreateSP(this, &SRigHierarchy::GetHierarchyForTreeView);
	Delegates.OnGetDisplaySettings = FOnGetRigTreeDisplaySettings::CreateSP(this, &SRigHierarchy::GetDisplaySettings);
	Delegates.OnRenameElement = FOnRigTreeRenameElement::CreateSP(this, &SRigHierarchy::HandleRenameElement);
	Delegates.OnVerifyElementNameChanged = FOnRigTreeVerifyElementNameChanged::CreateSP(this, &SRigHierarchy::HandleVerifyNameChanged);
	Delegates.OnSelectionChanged = FOnRigTreeSelectionChanged::CreateSP(this, &SRigHierarchy::OnSelectionChanged);
	Delegates.OnContextMenuOpening = FOnContextMenuOpening::CreateSP(this, &SRigHierarchy::CreateContextMenuWidget);
	Delegates.OnMouseButtonClick = FOnRigTreeMouseButtonClick::CreateSP(this, &SRigHierarchy::OnItemClicked);
	Delegates.OnMouseButtonDoubleClick = FOnRigTreeMouseButtonClick::CreateSP(this, &SRigHierarchy::OnItemDoubleClicked);
	Delegates.OnSetExpansionRecursive = FOnRigTreeSetExpansionRecursive::CreateSP(this, &SRigHierarchy::OnSetExpansionRecursive);
	Delegates.OnCanAcceptDrop = FOnRigTreeCanAcceptDrop::CreateSP(this, &SRigHierarchy::OnCanAcceptDrop);
	Delegates.OnAcceptDrop = FOnRigTreeAcceptDrop::CreateSP(this, &SRigHierarchy::OnAcceptDrop);
	Delegates.OnDragDetected = FOnDragDetected::CreateSP(this, &SRigHierarchy::OnDragDetected);

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		.Padding(0.0f)
		[
			SNew(SBorder)
			.Padding(0.0f)
			.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
			.BorderBackgroundColor(FLinearColor(0.6f, 0.6f, 0.6f, 1.0f))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)
					.Visibility(this, &SRigHierarchy::IsToolbarVisible)

					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.MaxWidth(180.0f)
					.Padding(3.0f, 1.0f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Success")
						.ForegroundColor(FLinearColor::White)
						.OnClicked(FOnClicked::CreateSP(this, &SRigHierarchy::OnImportSkeletonClicked))
						.Text(FText::FromString(TEXT("Import Hierarchy")))
					]
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Top)
				[
					SNew(SHorizontalBox)
					.Visibility(this, &SRigHierarchy::IsSearchbarVisible)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(0.0f, 0.0f, 2.0f, 0.0f)
					[
						SNew(SComboButton)
						.Visibility(EVisibility::Visible)
						.ComboButtonStyle(FAppStyle::Get(), "GenericFilters.ComboButtonStyle")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(0.0f)
						.OnGetMenuContent(this, &SRigHierarchy::CreateFilterMenu)
						.ButtonContent()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.TextStyle(FAppStyle::Get(), "GenericFilters.TextStyle")
								.Font(FAppStyle::Get().GetFontStyle("FontAwesome.9"))
								.Text(FText::FromString(FString(TEXT("\xf0b0"))) /*fa-filter*/)
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(2, 0, 0, 0)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.TextStyle(FAppStyle::Get(), "GenericFilters.TextStyle")
								.Text(LOCTEXT("FilterMenuLabel", "Options"))
							]
						]
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.Padding(3.0f, 1.0f)
					[
						SAssignNew(FilterBox, SSearchBox)
						.OnTextChanged(this, &SRigHierarchy::OnFilterTextChanged)
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f)
		[
			SNew(SBorder)
			.Padding(0.0f)
			.ShowEffectWhenDisabled(false)
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FAppStyle::GetBrush("SCSEditor.TreePanel"))
				[
					SAssignNew(TreeView, SRigHierarchyTreeView)
					.RigTreeDelegates(Delegates)
				]
			]
		]

		/*
		+ SVerticalBox::Slot()
		.Padding(0.0f, 0.0f)
		.FillHeight(0.1f)
		[
			SNew(SBorder)
			.Padding(2.0f)
			.BorderImage(FAppStyle::GetBrush("SCSEditor.TreePanel"))
			[
			SNew(SSpacer)
			]
		]
		*/
	];

	bIsChangingRigHierarchy = false;
	LastHierarchyHash = INDEX_NONE;
	bIsConstructionEventRunning = false;
	
	RefreshTreeView();

	if (ControlRigEditor.IsValid())
	{
		ControlRigEditor.Pin()->GetKeyDownDelegate().BindLambda([&](const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)->FReply {
			return OnKeyDown(MyGeometry, InKeyEvent);
		});
		ControlRigEditor.Pin()->OnGetViewportContextMenu().BindSP(this, &SRigHierarchy::GetContextMenu);
		ControlRigEditor.Pin()->OnViewportContextMenuCommands().BindSP(this, &SRigHierarchy::GetContextMenuCommands);
		ControlRigEditor.Pin()->OnControlRigEditorClosed().AddSP(this, &SRigHierarchy::OnEditorClose);
	}
	
	CreateContextMenu();
	CreateDragDropMenu();
}

void SRigHierarchy::OnEditorClose(const FControlRigEditor* InEditor, UControlRigBlueprint* InBlueprint)
{
	if (InEditor)
	{
		FControlRigEditor* Editor = (FControlRigEditor*)InEditor;  
		Editor->GetKeyDownDelegate().Unbind();
		Editor->OnGetViewportContextMenu().Unbind();
		Editor->OnViewportContextMenuCommands().Unbind();
	}

	if (InBlueprint)
	{
		InBlueprint->Hierarchy->OnModified().RemoveAll(this);
		InBlueprint->OnRefreshEditor().RemoveAll(this);
	}
	
	ControlRigEditor.Reset();
	ControlRigBlueprint.Reset();
}

void SRigHierarchy::BindCommands()
{
	// create new command
	const FControlRigHierarchyCommands& Commands = FControlRigHierarchyCommands::Get();

	CommandList->MapAction(Commands.AddBoneItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleNewItem, ERigElementType::Bone, false),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsNonProceduralElementSelected));

	CommandList->MapAction(Commands.AddControlItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleNewItem, ERigElementType::Control, false),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsNonProceduralElementSelected));

	CommandList->MapAction(Commands.AddAnimationChannelItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleNewItem, ERigElementType::Control, true),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsControlSelected, false));

	CommandList->MapAction(Commands.AddNullItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleNewItem, ERigElementType::Null, false),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsNonProceduralElementSelected));

	CommandList->MapAction(Commands.DuplicateItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleDuplicateItem),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::CanDuplicateItem));

	CommandList->MapAction(Commands.MirrorItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleMirrorItem),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::CanDuplicateItem));

	CommandList->MapAction(Commands.DeleteItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleDeleteItem),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::CanDeleteItem));

	CommandList->MapAction(Commands.RenameItem,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleRenameItem),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::CanRenameItem));

	CommandList->MapAction(Commands.CopyItems,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleCopyItems),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::CanCopyOrPasteItems));

	CommandList->MapAction(Commands.PasteItems,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandlePasteItems),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::CanPasteItems));

	CommandList->MapAction(Commands.PasteLocalTransforms,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandlePasteLocalTransforms),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::CanCopyOrPasteItems));

	CommandList->MapAction(Commands.PasteGlobalTransforms,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandlePasteGlobalTransforms),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::CanCopyOrPasteItems));

	CommandList->MapAction(
		Commands.ResetTransform,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleResetTransform, true),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsMultiSelected, true));

	CommandList->MapAction(
		Commands.ResetAllTransforms,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleResetTransform, false));

	CommandList->MapAction(
		Commands.SetInitialTransformFromClosestBone,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleSetInitialTransformFromClosestBone),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsControlOrNullSelected, false));

	CommandList->MapAction(
		Commands.SetInitialTransformFromCurrentTransform,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleSetInitialTransformFromCurrentTransform),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsMultiSelected, false));

	CommandList->MapAction(
		Commands.SetShapeTransformFromCurrent,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleSetShapeTransformFromCurrent),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsControlSelected, false));

	CommandList->MapAction(
		Commands.FrameSelection,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleFrameSelection),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsMultiSelected, true));

	CommandList->MapAction(
		Commands.ControlBoneTransform,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleControlBoneOrSpaceTransform),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsSingleBoneSelected, false),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(this, &SRigHierarchy::IsSingleBoneSelected, false)
		);

	CommandList->MapAction(
		Commands.Unparent,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleUnparent),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsMultiSelected, false));

	CommandList->MapAction(
		Commands.FilteringFlattensHierarchy,
		FExecuteAction::CreateLambda([this]() { DisplaySettings.bFlattenHierarchyOnFilter = !DisplaySettings.bFlattenHierarchyOnFilter; RefreshTreeView(); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return DisplaySettings.bFlattenHierarchyOnFilter; }));

	CommandList->MapAction(
		Commands.HideParentsWhenFiltering,
		FExecuteAction::CreateLambda([this]() { DisplaySettings.bHideParentsOnFilter = !DisplaySettings.bHideParentsOnFilter; RefreshTreeView(); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return DisplaySettings.bHideParentsOnFilter; }));

	CommandList->MapAction(
		Commands.ShowImportedBones,
		FExecuteAction::CreateLambda([this]() { DisplaySettings.bShowImportedBones = !DisplaySettings.bShowImportedBones; RefreshTreeView(); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return DisplaySettings.bShowImportedBones; }));
	
	CommandList->MapAction(
		Commands.ShowBones,
		FExecuteAction::CreateLambda([this]() { DisplaySettings.bShowBones = !DisplaySettings.bShowBones; RefreshTreeView(); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return DisplaySettings.bShowBones; }));

	CommandList->MapAction(
		Commands.ShowControls,
		FExecuteAction::CreateLambda([this]() { DisplaySettings.bShowControls = !DisplaySettings.bShowControls; RefreshTreeView(); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return DisplaySettings.bShowControls; }));
	
	CommandList->MapAction(
		Commands.ShowNulls,
		FExecuteAction::CreateLambda([this]() { DisplaySettings.bShowNulls = !DisplaySettings.bShowNulls; RefreshTreeView(); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return DisplaySettings.bShowNulls; }));

	CommandList->MapAction(
		Commands.ShowRigidBodies,
		FExecuteAction::CreateLambda([this]() { DisplaySettings.bShowRigidBodies = !DisplaySettings.bShowRigidBodies; RefreshTreeView(); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return DisplaySettings.bShowRigidBodies; }));

	CommandList->MapAction(
		Commands.ShowReferences,
		FExecuteAction::CreateLambda([this]() { DisplaySettings.bShowReferences = !DisplaySettings.bShowReferences; RefreshTreeView(); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return DisplaySettings.bShowReferences; }));

	CommandList->MapAction(
		Commands.ToggleControlShapeTransformEdit,
		FExecuteAction::CreateLambda([this]()
		{
			ControlRigEditor.Pin()->GetEditMode()->ToggleControlShapeTransformEdit();
		}));

	CommandList->MapAction(
		Commands.SpaceSwitching,
		FExecuteAction::CreateSP(this, &SRigHierarchy::HandleTestSpaceSwitching),
		FCanExecuteAction::CreateSP(this, &SRigHierarchy::IsControlSelected, true));

	CommandList->MapAction(
		Commands.ShowIconColors,
		FExecuteAction::CreateLambda([this]() { DisplaySettings.bShowIconColors = !DisplaySettings.bShowIconColors; RefreshTreeView(); }),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]() { return DisplaySettings.bShowIconColors; }));
}

FReply SRigHierarchy::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

EVisibility SRigHierarchy::IsToolbarVisible() const
{
	if (URigHierarchy* Hierarchy = GetHierarchy())
	{
		if (Hierarchy->Num(ERigElementType::Bone) > 0)
		{
			return EVisibility::Collapsed;
		}
	}
	return EVisibility::Visible;
}

EVisibility SRigHierarchy::IsSearchbarVisible() const
{
	if (URigHierarchy* Hierarchy = GetHierarchy())
	{
		if ((Hierarchy->Num(ERigElementType::Bone) +
			Hierarchy->Num(ERigElementType::Null) +
			Hierarchy->Num(ERigElementType::Control)) > 0)
		{
			return EVisibility::Visible;
		}
	}
	return EVisibility::Collapsed;
}

FReply SRigHierarchy::OnImportSkeletonClicked()
{
	FRigHierarchyImportSettings Settings;
	TSharedPtr<FStructOnScope> StructToDisplay = MakeShareable(new FStructOnScope(FRigHierarchyImportSettings::StaticStruct(), (uint8*)&Settings));

	TSharedRef<SKismetInspector> KismetInspector = SNew(SKismetInspector);
	KismetInspector->ShowSingleStruct(StructToDisplay);

	SGenericDialogWidget::FArguments DialogArguments;
	DialogArguments.OnOkPressed_Lambda([&Settings, this] ()
	{
		if (Settings.Mesh != nullptr)
		{
			ImportHierarchy(FAssetData(Settings.Mesh));
		}
	});
	
	SGenericDialogWidget::OpenDialog(LOCTEXT("ControlRigHierarchyImport", "Import Hierarchy"), KismetInspector, DialogArguments, true);

	return FReply::Handled();
}

void SRigHierarchy::OnFilterTextChanged(const FText& SearchText)
{
	DisplaySettings.FilterText = SearchText;
	RefreshTreeView();
}

void SRigHierarchy::RefreshTreeView(bool bRebuildContent)
{
	bool bDummySuspensionFlag = false;
	bool* SuspensionFlagPtr = &bDummySuspensionFlag;
	if (ControlRigEditor.IsValid())
	{
		SuspensionFlagPtr = &ControlRigEditor.Pin()->bSuspendDetailsPanelRefresh;
	}
	TGuardValue<bool> SuspendDetailsPanelRefreshGuard(*SuspensionFlagPtr, true);
	TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);

	TreeView->RefreshTreeView(bRebuildContent);
}

TArray<FRigElementKey> SRigHierarchy::GetSelectedKeys() const
{
	TArray<TSharedPtr<FRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
	
	TArray<FRigElementKey> SelectedKeys;
	for (const TSharedPtr<FRigTreeElement>& SelectedItem : SelectedItems)
	{
		if(SelectedItem->Key.IsValid())
		{
			SelectedKeys.AddUnique(SelectedItem->Key);
		}
	}

	return SelectedKeys;
}

void SRigHierarchy::OnSelectionChanged(TSharedPtr<FRigTreeElement> Selection, ESelectInfo::Type SelectInfo)
{
	if (bIsChangingRigHierarchy)
	{
		return;
	}

	// an element to use for the control rig editor's detail panel
	FRigElementKey LastSelectedElement;

	URigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		URigHierarchyController* Controller = Hierarchy->GetController(true);
		check(Controller);
		
		TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);

		// flag to guard during selection changes.
		// in case there's no editor we'll use the local variable.
		bool bDummySuspensionFlag = false;
		bool* SuspensionFlagPtr = &bDummySuspensionFlag;
		if (ControlRigEditor.IsValid())
		{
			SuspensionFlagPtr = &ControlRigEditor.Pin()->bSuspendDetailsPanelRefresh;
		}

		TGuardValue<bool> SuspendDetailsPanelRefreshGuard(*SuspensionFlagPtr, true);
		
		const TArray<FRigElementKey> NewSelection = GetSelectedKeys();
		if(!Controller->SetSelection(NewSelection, true))
		{
			return;
		}

		if (NewSelection.Num() > 0)
		{
			if (ControlRigEditor.IsValid())
			{
				if (ControlRigEditor.Pin()->GetEventQueueComboValue() == 1)
				{
					HandleControlBoneOrSpaceTransform();
				}
			}

			LastSelectedElement = NewSelection.Last();
		}
	}

	if (ControlRigEditor.IsValid())
	{
		if(LastSelectedElement.IsValid())
		{
			ControlRigEditor.Pin()->SetDetailViewForRigElements();
		}
		else
		{
			ControlRigEditor.Pin()->ClearDetailObject();
		}
	}
}

void SRigHierarchy::OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
	if (!InElement)
	{
		return;
	}

	if(!ControlRigBlueprint.IsValid())
	{
		return;
	}
	
	if (ControlRigBlueprint->bSuspendAllNotifications)
	{
		return;
	}

	if (bIsChangingRigHierarchy || bIsConstructionEventRunning)
	{
		return;
	}

	if(InElement)
	{
		if(InElement->IsTypeOf(ERigElementType::Curve))
		{
			return;
		}
	}

	switch(InNotif)
	{
		case ERigHierarchyNotification::ElementAdded:
		{
			if(InElement)
			{
				if(TreeView->AddElement(InElement))
				{
					RefreshTreeView(false);
				}
			}
			break;
		}
		case ERigHierarchyNotification::ElementRemoved:
		{
			if(InElement)
			{
				if(TreeView->RemoveElement(InElement->GetKey()))
				{
					RefreshTreeView(false);
				}
			}
			break;
		}
		case ERigHierarchyNotification::ParentChanged:
		{
			check(InHierarchy);
			if(InElement)
			{
				const FRigElementKey ParentKey = InHierarchy->GetFirstParent(InElement->GetKey());
				if(TreeView->ReparentElement(InElement->GetKey(), ParentKey))
				{
					RefreshTreeView(false);
				}
			}
			break;
		}
		case ERigHierarchyNotification::ParentWeightsChanged:
		{
			if(URigHierarchy* Hierarchy = GetHierarchy())
			{
				if(InElement)
				{
					TArray<FRigElementWeight> ParentWeights = Hierarchy->GetParentWeightArray(InElement->GetKey());
					if(ParentWeights.Num() > 0)
					{
						TArray<FRigElementKey> ParentKeys = Hierarchy->GetParents(InElement->GetKey());
						check(ParentKeys.Num() == ParentWeights.Num());
						for(int32 ParentIndex=0;ParentIndex<ParentKeys.Num();ParentIndex++)
						{
							if(ParentWeights[ParentIndex].IsAlmostZero())
							{
								continue;
							}

							if(TreeView->ReparentElement(InElement->GetKey(), ParentKeys[ParentIndex]))
							{
								RefreshTreeView(false);
							}
							break;
						}
					}
				}
			}
			break;
		}
		case ERigHierarchyNotification::ElementRenamed:
		case ERigHierarchyNotification::HierarchyReset:
		{
			RefreshTreeView();
			break;
		}
		case ERigHierarchyNotification::ElementSelected:
		case ERigHierarchyNotification::ElementDeselected:
		{
			if(InElement)
			{
				const bool bSelected = (InNotif == ERigHierarchyNotification::ElementSelected); 
					
				for (int32 RootIndex = 0; RootIndex < TreeView->RootElements.Num(); ++RootIndex)
				{
					TSharedPtr<FRigTreeElement> Found = TreeView->FindElement(InElement->GetKey(), TreeView->RootElements[RootIndex]);
					if (Found.IsValid())
					{
						TreeView->SetItemSelection(Found, bSelected, ESelectInfo::OnNavigation);

						if(GetDefault<UPersonaOptions>()->bExpandTreeOnSelection && bSelected)
						{
							HandleFrameSelection();
						}

						if (ControlRigEditor.IsValid() && !GIsTransacting)
						{
							if (ControlRigEditor.Pin()->GetEventQueueComboValue() == 1)
							{
								TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
								HandleControlBoneOrSpaceTransform();
							}
						}
					}
				}
			}
			break;
		}
		case ERigHierarchyNotification::ControlSettingChanged:
		{
			// update color and other settings of the item
			if(InElement && InElement->GetType() == ERigElementType::Control)
			{
				for (int32 RootIndex = 0; RootIndex < TreeView->RootElements.Num(); ++RootIndex)
				{
					TSharedPtr<FRigTreeElement> TreeElement = TreeView->FindElement(InElement->GetKey(), TreeView->RootElements[RootIndex]);
					if (TreeElement.IsValid())
					{
						const FRigTreeDisplaySettings& Settings = TreeView->GetRigTreeDelegates().GetDisplaySettings();
						TreeElement->RefreshDisplaySettings(InHierarchy, Settings);
					}
				}
			}
			break;
		}
		default:
		{
			break;
		}
	}
}

void SRigHierarchy::OnHierarchyModified_AnyThread(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
	if(bIsChangingRigHierarchy)
	{
		return;
	}
	
	if(!ControlRigBeingDebuggedPtr.IsValid())
	{
		return;
	}

	if(InHierarchy != ControlRigBeingDebuggedPtr->GetHierarchy())
	{
		return;
	}

	if(bIsConstructionEventRunning)
	{
		return;
	}

	if(IsInGameThread())
	{
		OnHierarchyModified(InNotif, InHierarchy, InElement);
	}
	else
	{
		FRigElementKey Key;
		if(InElement)
		{
			Key = InElement->GetKey();
		}

		TWeakObjectPtr<URigHierarchy> WeakHierarchy = InHierarchy;

		FFunctionGraphTask::CreateAndDispatchWhenReady([this, InNotif, WeakHierarchy, Key]()
        {
            if(!WeakHierarchy.IsValid())
            {
                return;
            }
            const FRigBaseElement* Element = WeakHierarchy.Get()->Find(Key);
            OnHierarchyModified(InNotif, WeakHierarchy.Get(), Element);
			
        }, TStatId(), NULL, ENamedThreads::GameThread);
	}
}

void SRigHierarchy::HandleRefreshEditorFromBlueprint(UControlRigBlueprint* InBlueprint)
{
	if (bIsChangingRigHierarchy)
	{
		return;
	}
	RefreshTreeView();
}

void SRigHierarchy::HandleSetObjectBeingDebugged(UObject* InObject)
{
	if(ControlRigBeingDebuggedPtr.Get() == InObject)
	{
		return;
	}

	if(ControlRigBeingDebuggedPtr.IsValid())
	{
		if(UControlRig* ControlRigBeingDebugged = ControlRigBeingDebuggedPtr.Get())
		{
			if(!ControlRigBeingDebugged->HasAnyFlags(RF_BeginDestroyed))
			{
				ControlRigBeingDebugged->GetHierarchy()->OnModified().RemoveAll(this);
			}
		}
	}

	ControlRigBeingDebuggedPtr.Reset();
	
	if(UControlRig* ControlRig = Cast<UControlRig>(InObject))
	{
		ControlRigBeingDebuggedPtr = ControlRig;
		ControlRig->GetHierarchy()->OnModified().RemoveAll(this);
		ControlRig->GetHierarchy()->OnModified().AddSP(this, &SRigHierarchy::OnHierarchyModified_AnyThread);
		ControlRig->OnPreConstructionForUI_AnyThread().RemoveAll(this);
		ControlRig->OnPreConstructionForUI_AnyThread().AddSP(this, &SRigHierarchy::OnPreConstruction_AnyThread);
		ControlRig->OnPostConstruction_AnyThread().RemoveAll(this);
		ControlRig->OnPostConstruction_AnyThread().AddSP(this, &SRigHierarchy::OnPostConstruction_AnyThread);
		LastHierarchyHash = INDEX_NONE;
	}

	RefreshTreeView();
}

void SRigHierarchy::OnPreConstruction_AnyThread(UControlRig* InRig, const EControlRigState InState, const FName& InEventName)
{
	if(InRig != ControlRigBeingDebuggedPtr.Get())
	{
		return;
	}
	bIsConstructionEventRunning = true;
	SelectionBeforeConstruction = InRig->GetHierarchy()->GetSelectedKeys();
}

void SRigHierarchy::OnPostConstruction_AnyThread(UControlRig* InRig, const EControlRigState InState, const FName& InEventName)
{
	if(InRig != ControlRigBeingDebuggedPtr.Get())
	{
		return;
	}

	bIsConstructionEventRunning = false;

	const int32 HierarchyHash = InRig->GetHierarchy()->GetTopologyHash(false);
	if(LastHierarchyHash != HierarchyHash)
	{
		LastHierarchyHash = HierarchyHash;
		
		auto Task = [this]()
		{
			TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);

			RefreshTreeView(true);

			TreeView->ClearSelection();
			if(!SelectionBeforeConstruction.IsEmpty())
			{
				for (int32 RootIndex = 0; RootIndex < TreeView->RootElements.Num(); ++RootIndex)
				{
					for(const FRigElementKey& Key : SelectionBeforeConstruction)
					{
						TSharedPtr<FRigTreeElement> Found = TreeView->FindElement(Key, TreeView->RootElements[RootIndex]);
						if (Found.IsValid())
						{
							TreeView->SetItemSelection(Found, true, ESelectInfo::OnNavigation);
						}
					}
				}
			}
		};
				
		if(IsInGameThread())
		{
			Task();
		}
		else
		{
			FFunctionGraphTask::CreateAndDispatchWhenReady([Task]()
			{
				Task();
			}, TStatId(), NULL, ENamedThreads::GameThread);
		}
	}
}

void SRigHierarchy::ClearDetailPanel() const
{
	if(ControlRigEditor.IsValid())
	{
		ControlRigEditor.Pin()->ClearDetailObject();
	}
}

TSharedRef< SWidget > SRigHierarchy::CreateFilterMenu()
{
	const FControlRigHierarchyCommands& Actions = FControlRigHierarchyCommands::Get();

	const bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder(CloseAfterSelection, CommandList);

	MenuBuilder.BeginSection("FilterOptions", LOCTEXT("OptionsMenuHeading", "Options"));
	{
		MenuBuilder.AddMenuEntry(Actions.FilteringFlattensHierarchy);
		MenuBuilder.AddMenuEntry(Actions.HideParentsWhenFiltering);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("FilterBones", LOCTEXT("BonesMenuHeading", "Bones"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowImportedBones);
		MenuBuilder.AddMenuEntry(Actions.ShowBones);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection("FilterControls", LOCTEXT("ControlsMenuHeading", "Controls"));
	{
		MenuBuilder.AddMenuEntry(Actions.ShowControls);
		MenuBuilder.AddMenuEntry(Actions.ShowNulls);
		MenuBuilder.AddMenuEntry(Actions.ShowIconColors);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

TSharedPtr< SWidget > SRigHierarchy::CreateContextMenuWidget()
{
	UToolMenus* ToolMenus = UToolMenus::Get();

	if (UToolMenu* Menu = GetContextMenu())
	{
		return ToolMenus->GenerateWidget(Menu);
	}
	
	return SNullWidget::NullWidget;
}

void SRigHierarchy::OnItemClicked(TSharedPtr<FRigTreeElement> InItem)
{
	URigHierarchy* Hierarchy = GetHierarchy();
	check(Hierarchy);

	if (Hierarchy->IsSelected(InItem->Key))
	{
		if (ControlRigEditor.IsValid())
		{
			ControlRigEditor.Pin()->SetDetailViewForRigElements();
		}

		if (InItem->Key.Type == ERigElementType::Bone)
		{
			if(FRigBoneElement* BoneElement = Hierarchy->Find<FRigBoneElement>(InItem->Key))
			{
				if (BoneElement->BoneType == ERigBoneType::Imported)
				{
					return;
				}
			}
		}

		uint32 CurrentCycles = FPlatformTime::Cycles();
		double SecondsPassed = double(CurrentCycles - TreeView->LastClickCycles) * FPlatformTime::GetSecondsPerCycle();
		if (SecondsPassed > 0.5f)
		{
			RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float) {
				HandleRenameItem();
				return EActiveTimerReturnType::Stop;
			}));
		}

		TreeView->LastClickCycles = CurrentCycles;
	}
}

void SRigHierarchy::OnItemDoubleClicked(TSharedPtr<FRigTreeElement> InItem)
{
	if (TreeView->IsItemExpanded(InItem))
	{
		TreeView->SetExpansionRecursive(InItem, false, false);
	}
	else
	{
		TreeView->SetExpansionRecursive(InItem, false, true);
	}
}

void SRigHierarchy::OnSetExpansionRecursive(TSharedPtr<FRigTreeElement> InItem, bool bShouldBeExpanded)
{
	TreeView->SetExpansionRecursive(InItem, false, bShouldBeExpanded);
}

void SRigHierarchy::CreateDragDropMenu() const
{
	const FName MenuName = DragDropMenuName;
	UToolMenus* ToolMenus = UToolMenus::Get();

	if (!ensure(ToolMenus))
	{
		return;
	}

	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu(MenuName);

		Menu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				UToolMenus* ToolMenus = UToolMenus::Get();
				UControlRigContextMenuContext* MainContext = InMenu->FindContext<UControlRigContextMenuContext>();
				
				if (SRigHierarchy* RigHierarchyPanel = MainContext->GetRigHierarchyPanel())
				{
					FToolMenuEntry ParentEntry = FToolMenuEntry::InitMenuEntry(
                        TEXT("Parent"),
                        LOCTEXT("DragDropMenu_Parent", "Parent"),
                        LOCTEXT("DragDropMenu_Parent_ToolTip", "Parent Selected Items to the Target Item"),
                        FSlateIcon(),
                        FToolMenuExecuteAction::CreateSP(RigHierarchyPanel, &SRigHierarchy::HandleParent)
                    );
		
                    ParentEntry.InsertPosition.Position = EToolMenuInsertType::First;
                    InMenu->AddMenuEntry(NAME_None, ParentEntry);
		
                    UToolMenu* AlignMenu = InMenu->AddSubMenu(
                        ToolMenus->CurrentOwner(),
                        NAME_None,
                        TEXT("Align"),
                        LOCTEXT("DragDropMenu_Align", "Align"),
                        LOCTEXT("DragDropMenu_Align_ToolTip", "Align Selected Items' Transforms to Target Item's Transform")
                    );
		
                    if (FToolMenuSection* DefaultSection = InMenu->FindSection(NAME_None))
                    {
                        if (FToolMenuEntry* AlignMenuEntry = DefaultSection->FindEntry(TEXT("Align")))
                        {
                            AlignMenuEntry->InsertPosition.Name = ParentEntry.Name;
                            AlignMenuEntry->InsertPosition.Position = EToolMenuInsertType::After;
                        }
                    }
		
                    FToolMenuEntry AlignAllEntry = FToolMenuEntry::InitMenuEntry(
                        TEXT("All"),
                        LOCTEXT("DragDropMenu_Align_All", "All"),
                        LOCTEXT("DragDropMenu_Align_All_ToolTip", "Align Selected Items' Transforms to Target Item's Transform"),
                        FSlateIcon(),
                        FToolMenuExecuteAction::CreateSP(RigHierarchyPanel, &SRigHierarchy::HandleAlign)
                    );
                    AlignAllEntry.InsertPosition.Position = EToolMenuInsertType::First;
		
                    AlignMenu->AddMenuEntry(NAME_None, AlignAllEntry);	
				}
			})
		);
	}
}

UToolMenu* SRigHierarchy::GetDragDropMenu(const TArray<FRigElementKey>& DraggedKeys, FRigElementKey TargetKey)
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ensure(ToolMenus))
	{
		return nullptr;
	}
	
	const FName MenuName = DragDropMenuName;
	UControlRigContextMenuContext* MenuContext = NewObject<UControlRigContextMenuContext>();
	FControlRigMenuSpecificContext MenuSpecificContext;
	MenuSpecificContext.RigHierarchyDragAndDropContext = FControlRigRigHierarchyDragAndDropContext(DraggedKeys, TargetKey);
	MenuSpecificContext.RigHierarchyPanel = SharedThis(this);
	MenuContext->Init(ControlRigEditor, MenuSpecificContext);
	
	UToolMenu* Menu = ToolMenus->GenerateMenu(MenuName, FToolMenuContext(MenuContext));

	return Menu;
}

void SRigHierarchy::CreateContextMenu() const
{
	const FName MenuName = ContextMenuName;

	UToolMenus* ToolMenus = UToolMenus::Get();
	
	if (!ensure(ToolMenus))
	{
		return;
	}
	
	if (!ToolMenus->IsMenuRegistered(MenuName))
	{
		UToolMenu* Menu = ToolMenus->RegisterMenu(MenuName);
		
		Menu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateLambda([this](UToolMenu* InMenu)
			{
				UControlRigContextMenuContext* MainContext = InMenu->FindContext<UControlRigContextMenuContext>();
				
				if (SRigHierarchy* RigHierarchyPanel = MainContext->GetRigHierarchyPanel())
				{
					const FControlRigHierarchyCommands& Commands = FControlRigHierarchyCommands::Get(); 
				
					FToolMenuSection& ElementsSection = InMenu->AddSection(TEXT("Elements"), LOCTEXT("ElementsHeader", "Elements"));
					ElementsSection.AddSubMenu(TEXT("New"), LOCTEXT("New", "New"), LOCTEXT("New_ToolTip", "Create New Elements"),
						FNewToolMenuDelegate::CreateLambda([Commands, RigHierarchyPanel](UToolMenu* InSubMenu)
						{
							FToolMenuSection& DefaultSection = InSubMenu->AddSection(NAME_None);
							FRigElementKey SelectedKey;
							TArray<TSharedPtr<FRigTreeElement>> SelectedItems = RigHierarchyPanel->TreeView->GetSelectedItems();
							if (SelectedItems.Num() > 0)
							{
								SelectedKey = SelectedItems[0]->Key;
							}
							
							if (!SelectedKey || SelectedKey.Type == ERigElementType::Bone)
							{
								DefaultSection.AddMenuEntry(Commands.AddBoneItem);
							}
							DefaultSection.AddMenuEntry(Commands.AddControlItem);
							if(SelectedKey.Type == ERigElementType::Control)
							{
								DefaultSection.AddMenuEntry(Commands.AddAnimationChannelItem);
							}
							DefaultSection.AddMenuEntry(Commands.AddNullItem);
						})
					);
					
					ElementsSection.AddMenuEntry(Commands.DeleteItem);
					ElementsSection.AddMenuEntry(Commands.DuplicateItem);
					ElementsSection.AddMenuEntry(Commands.RenameItem);
					ElementsSection.AddMenuEntry(Commands.MirrorItem);

					if(RigHierarchyPanel->IsProceduralElementSelected() && ControlRigBlueprint.IsValid())
					{
						ElementsSection.AddMenuEntry(
							"SelectSpawnerNode",
							LOCTEXT("SelectSpawnerNode", "Select Spawner Node"),
							LOCTEXT("SelectSpawnerNode_Tooltip", "Selects the node that spawn / added this element."),
							FSlateIcon(),
							FUIAction(FExecuteAction::CreateLambda([this]() {
								const TArray<const FRigBaseElement*> Elements = GetHierarchy()->GetSelectedElements();
								for(const FRigBaseElement* Element : Elements)
								{
									if(Element->IsProcedural())
									{
										const int32 InstructionIndex = Element->GetCreatedAtInstructionIndex();
										if(UControlRig* ControlRig = Cast<UControlRig>(ControlRigBlueprint->GetObjectBeingDebugged()))
										{
											if(ControlRig->VM)
											{
												if(URigVMNode* Node = Cast<URigVMNode>(ControlRig->VM->GetByteCode().GetSubjectForInstruction(InstructionIndex)))
												{
													if(URigVMController* Controller = ControlRigBlueprint->GetController(Node->GetGraph()))
													{
														Controller->SelectNode(Node);
														Controller->RequestJumpToHyperlinkDelegate.ExecuteIfBound(Node);
													}
												}
											}
										}
									}
								}
							})
						));
					}

					if (RigHierarchyPanel->IsSingleBoneSelected(false) || RigHierarchyPanel->IsControlSelected(false))
					{
						FToolMenuSection& InteractionSection = InMenu->AddSection(TEXT("Interaction"), LOCTEXT("InteractionHeader", "Interaction"));
						if(RigHierarchyPanel->IsSingleBoneSelected(false))
						{
							InteractionSection.AddMenuEntry(Commands.ControlBoneTransform);
						}
						else if(RigHierarchyPanel->IsControlSelected(false))
						{
							InteractionSection.AddMenuEntry(Commands.SpaceSwitching);
						}
					}

					FToolMenuSection& CopyPasteSection = InMenu->AddSection(TEXT("Copy&Paste"), LOCTEXT("Copy&PasteHeader", "Copy & Paste"));
					CopyPasteSection.AddMenuEntry(Commands.CopyItems);
					CopyPasteSection.AddMenuEntry(Commands.PasteItems);
					CopyPasteSection.AddMenuEntry(Commands.PasteLocalTransforms);
					CopyPasteSection.AddMenuEntry(Commands.PasteGlobalTransforms);
					
					FToolMenuSection& TransformsSection = InMenu->AddSection(TEXT("Transforms"), LOCTEXT("TransformsHeader", "Transforms"));
					TransformsSection.AddMenuEntry(Commands.ResetTransform);
					TransformsSection.AddMenuEntry(Commands.ResetAllTransforms);

					{
						static const FString InitialKeyword = TEXT("Initial");
						static const FString OffsetKeyword = TEXT("Offset");
						static const FString InitialOffsetKeyword = TEXT("Initial / Offset");
						
						const FString* Keyword = &InitialKeyword;
						TArray<ERigElementType> SelectedTypes;;
						TArray<TSharedPtr<FRigTreeElement>> SelectedItems = RigHierarchyPanel->TreeView->GetSelectedItems();
						for(const TSharedPtr<FRigTreeElement>& SelectedItem : SelectedItems)
						{
							if(SelectedItem.IsValid())
							{
								SelectedTypes.AddUnique(SelectedItem->Key.Type);
							}
						}
						if(SelectedTypes.Contains(ERigElementType::Control))
						{
							// since it is unique this means it is only controls
							if(SelectedTypes.Num() == 1)
							{
								Keyword = &OffsetKeyword;
							}
							else
							{
								Keyword = &InitialOffsetKeyword;
							}
						}

						const FText FromCurrentLabel = FText::Format(LOCTEXT("SetTransformFromCurrentTransform", "Set {0} Transform from Current"), FText::FromString(*Keyword));
						const FText FromClosestBoneLabel = FText::Format(LOCTEXT("SetTransformFromClosestBone", "Set {0} Transform from Closest Bone"), FText::FromString(*Keyword));
						TransformsSection.AddMenuEntry(Commands.SetInitialTransformFromCurrentTransform, FromCurrentLabel);
						TransformsSection.AddMenuEntry(Commands.SetInitialTransformFromClosestBone, FromClosestBoneLabel);
					}
					
					TransformsSection.AddMenuEntry(Commands.SetShapeTransformFromCurrent);
					TransformsSection.AddMenuEntry(Commands.Unparent);

					FToolMenuSection& AssetsSection = InMenu->AddSection(TEXT("Assets"), LOCTEXT("AssetsHeader", "Assets"));
					AssetsSection.AddSubMenu(TEXT("Import"), LOCTEXT("ImportSubMenu", "Import"),
						LOCTEXT("ImportSubMenu_ToolTip", "Import hierarchy to the current rig. This only imports non-existing node. For example, if there is hand_r, it won't import hand_r. If you want to reimport whole new hiearchy, delete all nodes, and use import hierarchy."),
						FNewMenuDelegate::CreateSP(RigHierarchyPanel, &SRigHierarchy::CreateImportMenu)
					);
					
					AssetsSection.AddSubMenu(TEXT("Refresh"), LOCTEXT("RefreshSubMenu", "Refresh"),
						LOCTEXT("RefreshSubMenu_ToolTip", "Refresh the existing initial transform from the selected mesh. This only updates if the node is found."),
						FNewMenuDelegate::CreateSP(RigHierarchyPanel, &SRigHierarchy::CreateRefreshMenu)
					);	
				}
			})
		);
	}
}

UToolMenu* SRigHierarchy::GetContextMenu()
{
	const FName MenuName = ContextMenuName;
	UToolMenus* ToolMenus = UToolMenus::Get();

	if(!ensure(ToolMenus))
	{
		return nullptr;
	}

	// individual entries in this menu can access members of this context, particularly useful for editor scripting
	UControlRigContextMenuContext* ContextMenuContext = NewObject<UControlRigContextMenuContext>();
	FControlRigMenuSpecificContext MenuSpecificContext;
	MenuSpecificContext.RigHierarchyPanel = SharedThis(this);
	ContextMenuContext->Init(ControlRigEditor, MenuSpecificContext);

	FToolMenuContext MenuContext(CommandList);
	MenuContext.AddObject(ContextMenuContext);

	UToolMenu* Menu = ToolMenus->GenerateMenu(MenuName, MenuContext);

	return Menu;
}

TSharedPtr<FUICommandList> SRigHierarchy::GetContextMenuCommands() const
{
	return CommandList;
}

void SRigHierarchy::CreateRefreshMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddWidget(
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(3)
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("ControlRig.Hierarchy.Menu"))
			.Text(LOCTEXT("RefreshMesh_Title", "Select Mesh"))
			.ToolTipText(LOCTEXT("RefreshMesh_Tooltip", "Select Mesh to refresh transform from... It will refresh init transform from selected mesh. This doesn't change hierarchy. If you want to reimport hierarchy, please delete all nodes, and use import hierarchy."))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(3)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(USkeletalMesh::StaticClass())
			.OnObjectChanged(this, &SRigHierarchy::RefreshHierarchy)
		]
		,
		FText()
	);
}

void SRigHierarchy::RefreshHierarchy(const FAssetData& InAssetData)
{
	if (bIsChangingRigHierarchy)
	{
		return;
	}
	TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);

	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	ControlRigEditor.Pin()->ClearDetailObject();

	URigHierarchy* Hierarchy = GetDefaultHierarchy();
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(InAssetData.GetAsset());
	if (Mesh && Hierarchy)
	{
		TGuardValue<bool> SuspendBlueprintNotifs(ControlRigBlueprint->bSuspendAllNotifications, true);

		FScopedTransaction Transaction(LOCTEXT("HierarchyRefresh", "Refresh Transform"));

		// don't select bone if we are in construction mode.
		// we do this to avoid the editmode / viewport shapes to refresh recursively,
		// which can add an extreme slowdown depending on the number of bones (n^(n-1))
		bool bSelectBones = true;
		if (UControlRig* CurrentRig = ControlRigEditor.Pin()->ControlRig)
		{
			bSelectBones = !CurrentRig->IsConstructionModeEnabled();
		}

		const FReferenceSkeleton& RefSkeleton = Mesh->GetRefSkeleton();

		URigHierarchyController* Controller = Hierarchy->GetController(true);
		check(Controller);
		
		Controller->ImportBones(Mesh->GetSkeleton(), NAME_None, true, true, bSelectBones, true, true);
		Controller->ImportCurves(Mesh->GetSkeleton(), NAME_None, false, true, true);
	}

	ControlRigBlueprint->PropagateHierarchyFromBPToInstances();
	ControlRigEditor.Pin()->OnHierarchyChanged();
	ControlRigBlueprint->BroadcastRefreshEditor();
	RefreshTreeView();
	FSlateApplication::Get().DismissAllMenus();

	if (ControlRigEditor.IsValid() && 
		Mesh != nullptr)
	{
		ControlRigEditor.Pin()->GetPersonaToolkit()->SetPreviewMesh(Mesh, true);
	}

	if (ControlRigEditor.IsValid())
	{
		ControlRigEditor.Pin()->Compile();
	}
}

void SRigHierarchy::CreateImportMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddWidget(
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(3)
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("ControlRig.Hierarchy.Menu"))
			.Text(LOCTEXT("ImportMesh_Title", "Select Mesh"))
			.ToolTipText(LOCTEXT("ImportMesh_Tooltip", "Select Mesh to import hierarchy from... It will only import if the node doesn't exist in the current hierarchy."))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(3)
		[
			SNew(SObjectPropertyEntryBox)
			.AllowedClass(USkeletalMesh::StaticClass())
			.OnObjectChanged(this, &SRigHierarchy::ImportHierarchy)
		]
		,
		FText()
	);
}

void SRigHierarchy::ImportHierarchy(const FAssetData& InAssetData)
{
	if (bIsChangingRigHierarchy)
	{
		return;
	}
	TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);

	if (!ControlRigEditor.IsValid())
	{
		return;
	}

	URigHierarchy* Hierarchy = GetDefaultHierarchy();
	USkeletalMesh* Mesh = Cast<USkeletalMesh>(InAssetData.GetAsset());
	if (Mesh && Hierarchy)
	{
		// filter out meshes that don't contain a skeleton
		if(Mesh->GetSkeleton() == nullptr)
		{
			FNotificationInfo Info(LOCTEXT("SkeletalMeshHasNoSkeleton", "Chosen Skeletal Mesh has no assigned skeleton. This needs to fixed before the mesh can be used for a Control Rig."));
			Info.bUseSuccessFailIcons = true;
			Info.Image = FAppStyle::GetBrush(TEXT("MessageLog.Warning"));
			Info.bFireAndForget = true;
			Info.bUseThrobber = true;
			Info.FadeOutDuration = 2.f;
			Info.ExpireDuration = 8.f;;
			TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
			if (NotificationPtr)
			{
				NotificationPtr->SetCompletionState(SNotificationItem::CS_Fail);
			}
			return;
		}
		
		TGuardValue<bool> SuspendBlueprintNotifs(ControlRigBlueprint->bSuspendAllNotifications, true);

		FScopedTransaction Transaction(LOCTEXT("HierarchyImport", "Import Hierarchy"));

		// don't select bone if we are in construction mode.
		// we do this to avoid the editmode / viewport shapes to refresh recursively,
		// which can add an extreme slowdown depending on the number of bones (n^(n-1))
		bool bSelectBones = true;
		if (UControlRig* CurrentRig = ControlRigEditor.Pin()->ControlRig)
		{
			bSelectBones = !CurrentRig->IsConstructionModeEnabled();
		}

		URigHierarchyController* Controller = Hierarchy->GetController(true);
		check(Controller);

		TArray<FRigElementKey> ImportedBones = Controller->ImportBones(Mesh->GetSkeleton(), NAME_None, false, false, bSelectBones, true, true);
		Controller->ImportCurves(Mesh->GetSkeleton(), NAME_None, false, true);

		ControlRigBlueprint->SourceHierarchyImport = Mesh->GetSkeleton();
		ControlRigBlueprint->SourceCurveImport = Mesh->GetSkeleton();

		if(ImportedBones.Num() > 0)
		{
			ControlRigEditor.Pin()->GetEditMode()->FrameItems(ImportedBones);
		}
	}

	ControlRigBlueprint->PropagateHierarchyFromBPToInstances();
	ControlRigEditor.Pin()->OnHierarchyChanged();
	ControlRigBlueprint->BroadcastRefreshEditor();
	RefreshTreeView();
	FSlateApplication::Get().DismissAllMenus();

	if (ControlRigBlueprint->GetPreviewMesh() == nullptr &&
		ControlRigEditor.IsValid() && 
		Mesh != nullptr)
	{
		ControlRigEditor.Pin()->UpdateMeshInAnimInstance(Mesh);
		ControlRigEditor.Pin()->GetPersonaToolkit()->SetPreviewMesh(Mesh, true);
	}

	if (ControlRigEditor.IsValid())
	{
		ControlRigEditor.Pin()->Compile();
	}
}

bool SRigHierarchy::IsMultiSelected(bool bIncludeProcedural) const
{
	if(GetSelectedKeys().Num() > 0)
	{
		if(!bIncludeProcedural && IsProceduralElementSelected())
		{
			return false;
		}
		return true;
	}
	return false;
}

bool SRigHierarchy::IsSingleSelected(bool bIncludeProcedural) const
{
	if(GetSelectedKeys().Num() == 1)
	{
		if(!bIncludeProcedural && IsProceduralElementSelected())
		{
			return false;
		}
		return true;
	}
	return false;
}

bool SRigHierarchy::IsSingleBoneSelected(bool bIncludeProcedural) const
{
	if(!IsSingleSelected(bIncludeProcedural))
	{
		return false;
	}
	return GetSelectedKeys()[0].Type == ERigElementType::Bone;
}

bool SRigHierarchy::IsSingleNullSelected(bool bIncludeProcedural) const
{
	if(!IsSingleSelected(bIncludeProcedural))
	{
		return false;
	}
	return GetSelectedKeys()[0].Type == ERigElementType::Null;
}

bool SRigHierarchy::IsControlSelected(bool bIncludeProcedural) const
{
	if(!bIncludeProcedural && IsProceduralElementSelected())
	{
		return false;
	}
	
	TArray<FRigElementKey> SelectedKeys = GetSelectedKeys();
	for (const FRigElementKey& SelectedKey : SelectedKeys)
	{
		if (SelectedKey.Type == ERigElementType::Control)
		{
			return true;
		}
	}
	return false;
}

bool SRigHierarchy::IsControlOrNullSelected(bool bIncludeProcedural) const
{
	if(!bIncludeProcedural && IsProceduralElementSelected())
	{
		return false;
	}

	TArray<FRigElementKey> SelectedKeys = GetSelectedKeys();
	for (const FRigElementKey& SelectedKey : SelectedKeys)
	{
		if (SelectedKey.Type == ERigElementType::Control)
		{
			return true;
		}
		if (SelectedKey.Type == ERigElementType::Null)
		{
			return true;
		}
	}
	return false;
}

bool SRigHierarchy::IsProceduralElementSelected() const
{
	TArray<FRigElementKey> SelectedKeys = GetSelectedKeys();
	for (const FRigElementKey& SelectedKey : SelectedKeys)
	{
		if(!GetHierarchy()->IsProcedural(SelectedKey))
		{
			return false;
		}
	}
	return true;
}

bool SRigHierarchy::IsNonProceduralElementSelected() const
{
	TArray<FRigElementKey> SelectedKeys = GetSelectedKeys();
	for (const FRigElementKey& SelectedKey : SelectedKeys)
	{
		if(GetHierarchy()->IsProcedural(SelectedKey))
		{
			return false;
		}
	}
	return true;
}

void SRigHierarchy::HandleDeleteItem()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	if(!CanDeleteItem())
	{
		return;
	}

	URigHierarchy* Hierarchy = GetDefaultHierarchy();
 	if (Hierarchy)
 	{
		TArray<FRigElementKey> RemovedItems;

		ClearDetailPanel();
		FScopedTransaction Transaction(LOCTEXT("HierarchyTreeDeleteSelected", "Delete selected items from hierarchy"));

		// clear detail view display
		ControlRigEditor.Pin()->ClearDetailObject();

		bool bConfirmedByUser = false;
		bool bDeleteImportedBones = false;

 		URigHierarchyController* Controller = Hierarchy->GetController(true);
 		check(Controller);

 		TArray<FRigElementKey> SelectedKeys = GetSelectedKeys();

 		// clear selection early here to make sure ControlRigEditMode can react to this deletion
 		// it cannot react to it during Controller->RemoveElement() later because bSuspendAllNotifications is true
 		Controller->ClearSelection();

 		FRigHierarchyInteractionBracket InteractionBracket(Hierarchy);
 		
		for (const FRigElementKey& SelectedKey : SelectedKeys)
		{
			TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
			TGuardValue<bool> SuspendBlueprintNotifs(ControlRigBlueprint->bSuspendAllNotifications, true);

			if (SelectedKey.Type == ERigElementType::Bone)
			{
				if (FRigBoneElement* BoneElement = Hierarchy->Find<FRigBoneElement>(SelectedKey))
				{
					if (BoneElement->BoneType == ERigBoneType::Imported && BoneElement->ParentElement != nullptr)
					{
						if (!bConfirmedByUser)
						{
							FText ConfirmDelete = LOCTEXT("ConfirmDeleteBoneHierarchy", "Deleting imported(white) bones can cause issues with animation - are you sure ?");

							FSuppressableWarningDialog::FSetupInfo Info(ConfirmDelete, LOCTEXT("DeleteImportedBone", "Delete Imported Bone"), "DeleteImportedBoneHierarchy_Warning");
							Info.ConfirmText = LOCTEXT("DeleteImportedBoneHierarchy_Yes", "Yes");
							Info.CancelText = LOCTEXT("DeleteImportedBoneHierarchy_No", "No");

							FSuppressableWarningDialog DeleteImportedBonesInHierarchy(Info);
							bDeleteImportedBones = DeleteImportedBonesInHierarchy.ShowModal() != FSuppressableWarningDialog::Cancel;
							bConfirmedByUser = true;
						}

						if (!bDeleteImportedBones)
						{
							break;
						}
					}
				}
			}

			Controller->RemoveElement(SelectedKey, true, true);
			RemovedItems.Add(SelectedKey);
		}
	}

	ControlRigBlueprint->PropagateHierarchyFromBPToInstances();
	ControlRigEditor.Pin()->OnHierarchyChanged();
	RefreshTreeView();
	FSlateApplication::Get().DismissAllMenus();
}

bool SRigHierarchy::CanDeleteItem() const
{
	return IsMultiSelected(false);
}

/** Create Item */
void SRigHierarchy::HandleNewItem(ERigElementType InElementType, bool bIsAnimationChannel)
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	URigHierarchy* Hierarchy = GetDefaultHierarchy();
	if (Hierarchy)
	{
		// unselect current selected item
		ClearDetailPanel();

		URigHierarchyController* Controller = Hierarchy->GetController(true);
		check(Controller);

		FScopedTransaction Transaction(LOCTEXT("HierarchyTreeAdded", "Add new item to hierarchy"));

		FRigElementKey NewItemKey;
		FRigElementKey ParentKey;
		FTransform ParentTransform = FTransform::Identity;

		TArray<FRigElementKey> SelectedKeys = GetSelectedKeys();
		if (SelectedKeys.Num() > 0)
		{
			ParentKey = SelectedKeys[0];
			ParentTransform = Hierarchy->GetGlobalTransform(ParentKey);
		}

		// use bone's name as prefix if creating a control
		FString NewNameTemplate;
		const bool bIsParentABone = ParentKey.IsValid() && ParentKey.Type == ERigElementType::Bone;
		if( InElementType == ERigElementType::Control && bIsParentABone )
		{
			static const FString CtrlSuffix(TEXT("_ctrl"));
			NewNameTemplate = ParentKey.Name.ToString();
			NewNameTemplate += CtrlSuffix;
		}
		else
		{
			NewNameTemplate = FString::Printf(TEXT("New%s"), *StaticEnum<ERigElementType>()->GetNameStringByValue((int64)InElementType));

			if(bIsAnimationChannel)
			{
				static const FString NewAnimationChannel = TEXT("Channel");
				NewNameTemplate = NewAnimationChannel;
			}
		}
		
		const FName NewElementName = CreateUniqueName(*NewNameTemplate, InElementType);
		{
			TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
			switch (InElementType)
			{
				case ERigElementType::Bone:
				{
					NewItemKey = Controller->AddBone(NewElementName, ParentKey, ParentTransform, true, ERigBoneType::User, true, true);
					break;
				}
				case ERigElementType::Control:
				{
					FRigControlSettings Settings;

					if(bIsAnimationChannel)
					{
						Settings.AnimationType = ERigControlAnimationType::AnimationChannel;
						Settings.ControlType = ERigControlType::Float;
						Settings.MinimumValue = FRigControlValue::Make<float>(0.f);
						Settings.MaximumValue = FRigControlValue::Make<float>(1.f);
						Settings.DisplayName = Hierarchy->GetSafeNewDisplayName(ParentKey, *NewNameTemplate);

						NewItemKey = Controller->AddAnimationChannel(NewElementName, ParentKey, Settings, true, true);
					}
					else
					{
						Settings.ControlType = ERigControlType::EulerTransform;
						FEulerTransform Identity = FEulerTransform::Identity;
						FRigControlValue ValueToSet = FRigControlValue::Make<FEulerTransform>(Identity);
						Settings.MinimumValue = ValueToSet;
						Settings.MaximumValue = ValueToSet;

						NewItemKey = Controller->AddControl(NewElementName, ParentKey, Settings, Settings.GetIdentityValue(), FTransform::Identity, FTransform::Identity, true, true);
					}						
					break;
				}
				case ERigElementType::Null:
				{
					NewItemKey = Controller->AddNull(NewElementName, ParentKey, ParentTransform, true, true, true);
					break;
				}
				default:
				{
					return;
				}
			}

			if (ControlRigBlueprint.IsValid())
			{
				ControlRigBlueprint->BroadcastRefreshEditor();
			}
		}

		Controller->ClearSelection();
		Controller->SelectElement(NewItemKey);
	}

	FSlateApplication::Get().DismissAllMenus();
	RefreshTreeView();
}

/** Check whether we can deleting the selected item(s) */
bool SRigHierarchy::CanDuplicateItem() const
{
	return IsMultiSelected(false);
}

/** Duplicate Item */
void SRigHierarchy::HandleDuplicateItem()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	URigHierarchy* Hierarchy = GetDefaultHierarchy();
	if (Hierarchy)
	{
		ClearDetailPanel();
		{
			TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
			TGuardValue<bool> SuspendBlueprintNotifs(ControlRigBlueprint->bSuspendAllNotifications, true);

			FScopedTransaction Transaction(LOCTEXT("HierarchyTreeDuplicateSelected", "Duplicate selected items from hierarchy"));

			URigHierarchyController* Controller = Hierarchy->GetController(true);
			check(Controller);

			const TArray<FRigElementKey> KeysToDuplicate = GetSelectedKeys();
			Controller->DuplicateElements(KeysToDuplicate, true, true, true);
		}

		ControlRigBlueprint->PropagateHierarchyFromBPToInstances();
	}

	FSlateApplication::Get().DismissAllMenus();
	ControlRigEditor.Pin()->OnHierarchyChanged();
	{
		TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
		ControlRigBlueprint->BroadcastRefreshEditor();
	}
	RefreshTreeView();
}

/** Mirror Item */
void SRigHierarchy::HandleMirrorItem()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}
	
	URigHierarchy* Hierarchy = GetDefaultHierarchy();
	if (Hierarchy)
	{
		URigHierarchyController* Controller = Hierarchy->GetController(true);
		check(Controller);
		
		FRigMirrorSettings Settings;
		TSharedPtr<FStructOnScope> StructToDisplay = MakeShareable(new FStructOnScope(FRigMirrorSettings::StaticStruct(), (uint8*)&Settings));

		TSharedRef<SKismetInspector> KismetInspector = SNew(SKismetInspector);
		KismetInspector->ShowSingleStruct(StructToDisplay);

		TSharedRef<SCustomDialog> MirrorDialog = SNew(SCustomDialog)
			.Title(FText(LOCTEXT("ControlRigHierarchyMirror", "Mirror Selected Rig Elements")))
			.Content()
			[
				KismetInspector
			]
			.Buttons({
				SCustomDialog::FButton(LOCTEXT("OK", "OK")),
				SCustomDialog::FButton(LOCTEXT("Cancel", "Cancel"))
		});

		if (MirrorDialog->ShowModal() == 0)
		{
			ClearDetailPanel();
			{
				TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
				TGuardValue<bool> SuspendBlueprintNotifs(ControlRigBlueprint->bSuspendAllNotifications, true);

				FScopedTransaction Transaction(LOCTEXT("HierarchyTreeMirrorSelected", "Mirror selected items from hierarchy"));

				const TArray<FRigElementKey> KeysToMirror = GetSelectedKeys();
				const TArray<FRigElementKey> KeysToDuplicate = GetSelectedKeys();
				Controller->MirrorElements(KeysToDuplicate, Settings, true, true, true);
			}
			ControlRigBlueprint->PropagateHierarchyFromBPToInstances();
		}
	}

	FSlateApplication::Get().DismissAllMenus();
	ControlRigEditor.Pin()->OnHierarchyChanged();
	RefreshTreeView();
}

/** Check whether we can deleting the selected item(s) */
bool SRigHierarchy::CanRenameItem() const
{
	if(IsSingleSelected(false))
	{
		const FRigElementKey Key = GetSelectedKeys()[0];
		if(Key.Type == ERigElementType::RigidBody ||
			Key.Type == ERigElementType::Reference)
		{
			return false;
		}
		if(Key.Type == ERigElementType::Control)
		{
			if(URigHierarchy* DebuggedHierarchy = GetHierarchy())
			{
				if(FRigControlElement* ControlElement = DebuggedHierarchy->Find<FRigControlElement>(Key))
				{
					if(ControlElement->Settings.bIsTransientControl)
					{
						return false;
					}
				}
			}
		}
		return true;
	}
	return false;
}

/** Delete Item */
void SRigHierarchy::HandleRenameItem()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	if (!CanRenameItem())
	{
		return;
	}

	URigHierarchy* Hierarchy = GetDefaultHierarchy();
	if (Hierarchy)
	{
		FScopedTransaction Transaction(LOCTEXT("HierarchyTreeRenameSelected", "Rename selected item from hierarchy"));

		TArray<TSharedPtr<FRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
		if (SelectedItems.Num() == 1)
		{
			if (SelectedItems[0]->Key.Type == ERigElementType::Bone)
			{
				if(FRigBoneElement* BoneElement = Hierarchy->Find<FRigBoneElement>(SelectedItems[0]->Key))
				{
					if (BoneElement->BoneType == ERigBoneType::Imported)
					{
						FText ConfirmRename = LOCTEXT("RenameDeleteBoneHierarchy", "Renaming imported(white) bones can cause issues with animation - are you sure ?");

						FSuppressableWarningDialog::FSetupInfo Info(ConfirmRename, LOCTEXT("RenameImportedBone", "Rename Imported Bone"), "RenameImportedBoneHierarchy_Warning");
						Info.ConfirmText = LOCTEXT("RenameImportedBoneHierarchy_Yes", "Yes");
						Info.CancelText = LOCTEXT("RenameImportedBoneHierarchy_No", "No");

						FSuppressableWarningDialog RenameImportedBonesInHierarchy(Info);
						if (RenameImportedBonesInHierarchy.ShowModal() == FSuppressableWarningDialog::Cancel)
						{
							return;
						}
					}
				}
			}
			SelectedItems[0]->RequestRename();
		}
	}
}

bool SRigHierarchy::CanPasteItems() const
{
	return true;
}	

bool SRigHierarchy::CanCopyOrPasteItems() const
{
	return TreeView->GetNumItemsSelected() > 0;
}

void SRigHierarchy::HandleCopyItems()
{
	if (URigHierarchy* Hierarchy = GetHierarchy())
	{
		URigHierarchyController* Controller = Hierarchy->GetController(true);
		check(Controller);

		const TArray<FRigElementKey> Selection = GetHierarchy()->GetSelectedKeys();
		const FString Content = Controller->ExportToText(Selection);
		FPlatformApplicationMisc::ClipboardCopy(*Content);
	}
}

void SRigHierarchy::HandlePasteItems()
{
	if (URigHierarchy* Hierarchy = GetDefaultHierarchy())
	{
		TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
		TGuardValue<bool> SuspendBlueprintNotifs(ControlRigBlueprint->bSuspendAllNotifications, true);

		FString Content;
		FPlatformApplicationMisc::ClipboardPaste(Content);

		FScopedTransaction Transaction(LOCTEXT("HierarchyTreePastedRigElements", "Pasted rig elements."));

		URigHierarchyController* Controller = Hierarchy->GetController(true);
		check(Controller);

		Controller->ImportFromText(Content, false, true, true, true);
	}

	//ControlRigBlueprint->PropagateHierarchyFromBPToInstances();
	ControlRigEditor.Pin()->OnHierarchyChanged();
	{
		TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
		ControlRigBlueprint->BroadcastRefreshEditor();
	}
	RefreshTreeView();
}

class SRigHierarchyPasteTransformsErrorPipe : public FOutputDevice
{
public:

	int32 NumErrors;

	SRigHierarchyPasteTransformsErrorPipe()
		: FOutputDevice()
		, NumErrors(0)
	{
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		UE_LOG(LogControlRig, Error, TEXT("Error importing transforms to Hierarchy: %s"), V);
		NumErrors++;
	}
};

void SRigHierarchy::HandlePasteLocalTransforms()
{
	HandlePasteTransforms(ERigTransformType::CurrentLocal, true);
}

void SRigHierarchy::HandlePasteGlobalTransforms()
{
	HandlePasteTransforms(ERigTransformType::CurrentGlobal, false);
}

void SRigHierarchy::HandlePasteTransforms(ERigTransformType::Type InTransformType, bool bAffectChildren)
{
	if (URigHierarchy* Hierarchy = GetDefaultHierarchy())
	{
		FString Content;
		FPlatformApplicationMisc::ClipboardPaste(Content);

		FScopedTransaction Transaction(LOCTEXT("HierarchyTreePastedTransform", "Pasted transforms."));

		SRigHierarchyPasteTransformsErrorPipe ErrorPipe;
		FRigHierarchyCopyPasteContent Data;
		FRigHierarchyCopyPasteContent::StaticStruct()->ImportText(*Content, &Data, nullptr, EPropertyPortFlags::PPF_None, &ErrorPipe, FRigHierarchyCopyPasteContent::StaticStruct()->GetName(), true);
		if(ErrorPipe.NumErrors > 0)
		{
			return;
		}
		
		URigHierarchy* DebuggedHierarchy = GetHierarchy();

		const TArray<FRigElementKey> CurrentSelection = Hierarchy->GetSelectedKeys();
		const int32 Count = FMath::Min<int32>(CurrentSelection.Num(), Data.Elements.Num());
		for(int32 Index = 0; Index < Count; Index++)
		{
			const FRigHierarchyCopyPasteContentPerElement& PerElementData = Data.Elements[Index];
			const FTransform Transform =  PerElementData.Pose.Get(InTransformType);

			if(FRigTransformElement* TransformElement = Hierarchy->Find<FRigTransformElement>(CurrentSelection[Index]))
			{
				Hierarchy->SetTransform(TransformElement, Transform, InTransformType, bAffectChildren, true, false, true);
			}
			if(FRigBoneElement* BoneElement = Hierarchy->Find<FRigBoneElement>(CurrentSelection[Index]))
			{
				Hierarchy->SetTransform(BoneElement, Transform, ERigTransformType::MakeInitial(InTransformType), bAffectChildren, true, false, true);
			}
			
            if(DebuggedHierarchy && DebuggedHierarchy != Hierarchy)
            {
            	if(FRigTransformElement* TransformElement = DebuggedHierarchy->Find<FRigTransformElement>(CurrentSelection[Index]))
            	{
            		DebuggedHierarchy->SetTransform(TransformElement, Transform, InTransformType, bAffectChildren, true);
            	}
            	if(FRigBoneElement* BoneElement = DebuggedHierarchy->Find<FRigBoneElement>(CurrentSelection[Index]))
            	{
            		DebuggedHierarchy->SetTransform(BoneElement, Transform, ERigTransformType::MakeInitial(InTransformType), bAffectChildren, true);
            	}
            }
		}
	}
}

URigHierarchy* SRigHierarchy::GetHierarchy() const
{
	if (ControlRigBlueprint.IsValid())
	{
		if (UControlRig* DebuggedRig = ControlRigBeingDebuggedPtr.Get())
		{
			return DebuggedRig->GetHierarchy();
		}
	}
	if (ControlRigEditor.IsValid())
	{
		if (UControlRig* CurrentRig = ControlRigEditor.Pin()->ControlRig)
		{
			return CurrentRig->GetHierarchy();
		}
	}
	return GetDefaultHierarchy();
}

URigHierarchy* SRigHierarchy::GetDefaultHierarchy() const
{
	if (ControlRigBlueprint.IsValid())
	{
		return ControlRigBlueprint->Hierarchy;
	}
	return nullptr;
}


FName SRigHierarchy::CreateUniqueName(const FName& InBaseName, ERigElementType InElementType) const
{
	return GetHierarchy()->GetSafeNewName(InBaseName.ToString(), InElementType);
}

void SRigHierarchy::PostRedo(bool bSuccess) 
{
	if (bSuccess)
	{
		RefreshTreeView();
	}
}

void SRigHierarchy::PostUndo(bool bSuccess) 
{
	if (bSuccess)
	{
		RefreshTreeView();
	}
}

FReply SRigHierarchy::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TArray<FRigElementKey> DraggedElements = GetSelectedKeys();
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && DraggedElements.Num() > 0)
	{
		if (ControlRigEditor.IsValid())
		{
			TSharedRef<FRigElementHierarchyDragDropOp> DragDropOp = FRigElementHierarchyDragDropOp::New(MoveTemp(DraggedElements));
			DragDropOp->OnPerformDropToGraph.BindSP(ControlRigEditor.Pin().Get(), &FControlRigEditor::OnGraphNodeDropToPerform);
			return FReply::Handled().BeginDragDrop(DragDropOp);
		}
	}

	return FReply::Unhandled();
}

TOptional<EItemDropZone> SRigHierarchy::OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FRigTreeElement> TargetItem)
{
	TOptional<EItemDropZone> ReturnDropZone;

	TSharedPtr<FRigElementHierarchyDragDropOp> RigDragDropOp = DragDropEvent.GetOperationAs<FRigElementHierarchyDragDropOp>();
	if (RigDragDropOp.IsValid())
	{
		URigHierarchy* Hierarchy = GetHierarchy();
		if (Hierarchy)
		{
			for (const FRigElementKey& DraggedKey : RigDragDropOp->GetElements())
			{
				if (DraggedKey == TargetItem->Key)
				{
					return ReturnDropZone;
				}

				if(Hierarchy->IsProcedural(DraggedKey))
				{
					return ReturnDropZone;
				}

				if(Hierarchy->IsParentedTo(TargetItem->Key, DraggedKey))
				{
					return ReturnDropZone;
				}
			}
		}

		// don't allow dragging onto procedural items
		if(!GetDefaultHierarchy()->Contains(TargetItem->Key))
		{
			return ReturnDropZone;
		}

		switch (TargetItem->Key.Type)
		{
			case ERigElementType::Bone:
			{
				// bones can parent anything
				ReturnDropZone = EItemDropZone::OntoItem;
				break;
			}
			case ERigElementType::Control:
			case ERigElementType::Null:
			case ERigElementType::RigidBody:
			case ERigElementType::Reference:
			{
				for (const FRigElementKey& DraggedKey : RigDragDropOp->GetElements())
				{
					switch (DraggedKey.Type)
					{
						case ERigElementType::Control:
						case ERigElementType::Null:
						case ERigElementType::RigidBody:
						case ERigElementType::Reference:
						{
							break;
						}
						default:
						{
							return ReturnDropZone;
						}
					}
				}
				ReturnDropZone = EItemDropZone::OntoItem;
				break;
			}
			default:
			{
				ReturnDropZone = EItemDropZone::OntoItem;
				break;
			}
		}
	}

	return ReturnDropZone;
}

FReply SRigHierarchy::OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FRigTreeElement> TargetItem)
{
	bool bSummonDragDropMenu = DragDropEvent.GetModifierKeys().IsAltDown() && DragDropEvent.GetModifierKeys().IsShiftDown(); 
	bool bMatchTransforms = DragDropEvent.GetModifierKeys().IsAltDown();
	bool bReparentItems = !bMatchTransforms;

	TSharedPtr<FRigElementHierarchyDragDropOp> RigDragDropOp = DragDropEvent.GetOperationAs<FRigElementHierarchyDragDropOp>();
	if (RigDragDropOp.IsValid())
	{
		if (bSummonDragDropMenu)
		{
			const FVector2D& SummonLocation = DragDropEvent.GetScreenSpacePosition();

			// Get the context menu content. If NULL, don't open a menu.
			UToolMenu* DragDropMenu = GetDragDropMenu(RigDragDropOp->GetElements(), TargetItem->Key);
			const TSharedPtr<SWidget> MenuContent = UToolMenus::Get()->GenerateWidget(DragDropMenu);

			if (MenuContent.IsValid())
			{
				const FWidgetPath WidgetPath = DragDropEvent.GetEventPath() != nullptr ? *DragDropEvent.GetEventPath() : FWidgetPath();
				FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MenuContent.ToSharedRef(), SummonLocation, FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
			}
			
			return FReply::Handled();
		}
		else
		{
			FRigElementKey TargetKey;
			if (TargetItem.IsValid())
			{
				TargetKey = TargetItem->Key;
			}
			return ReparentOrMatchTransform(RigDragDropOp->GetElements(), TargetKey, bReparentItems);			
		}

	}

	return FReply::Unhandled();
}

FName SRigHierarchy::HandleRenameElement(const FRigElementKey& OldKey, const FString& NewName)
{
	ClearDetailPanel();

	// make sure there is no duplicate
	if (ControlRigBlueprint.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("HierarchyRename", "Rename Hierarchy Element"));

		URigHierarchy* Hierarchy = GetDefaultHierarchy();
		URigHierarchyController* Controller = Hierarchy->GetController(true);
		check(Controller);

		FString SanitizedNameStr = NewName;
		Hierarchy->SanitizeName(SanitizedNameStr);
		const FName SanitizedName = *SanitizedNameStr;
		FName ResultingName = NAME_None;

		bool bUseDisplayName = false;
		if(const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(OldKey))
		{
			if(ControlElement->IsAnimationChannel())
			{
				bUseDisplayName = true;
			}
		}

		if(bUseDisplayName)
		{
			ResultingName = Controller->SetDisplayName(OldKey, SanitizedName, true, true, true);
		}
		else
		{
			ResultingName = Controller->RenameElement(OldKey, SanitizedName, true, true, false).Name;
		}
		ControlRigBlueprint->PropagateHierarchyFromBPToInstances();
		return ResultingName;
	}

	return NAME_None;
}

bool SRigHierarchy::HandleVerifyNameChanged(const FRigElementKey& OldKey, const FString& NewName, FText& OutErrorMessage)
{
	bool bIsAnimationChannel = false;
	if (ControlRigBlueprint.IsValid())
	{
		URigHierarchy* Hierarchy = GetHierarchy();
		if(const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(OldKey))
		{
			if(ControlElement->IsAnimationChannel())
			{
				bIsAnimationChannel = true;

				if(ControlElement->GetDisplayName().ToString() == NewName)
				{
					return true;
				}
			}
		}
	}

	if(!bIsAnimationChannel)
	{
		if (OldKey.Name.ToString() == NewName)
		{
			return true;
		}
	}

	if (NewName.IsEmpty())
	{
		OutErrorMessage = FText::FromString(TEXT("Name is empty."));
		return false;
	}

	// make sure there is no duplicate
	if (ControlRigBlueprint.IsValid())
	{
		URigHierarchy* Hierarchy = GetHierarchy();

		if(bIsAnimationChannel)
		{
			if(const FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(OldKey))
			{
				if(const FRigBaseElement* ParentElement = Hierarchy->GetFirstParent(ControlElement))
				{
					FString OutErrorString;
					if (!Hierarchy->IsDisplayNameAvailable(ParentElement->GetKey(), NewName, &OutErrorString))
					{
						OutErrorMessage = FText::FromString(OutErrorString);
						return false;
					}
				}
			}
		}
		else
		{
			FString OutErrorString;
			if (!Hierarchy->IsNameAvailable(NewName, OldKey.Type, &OutErrorString))
			{
				OutErrorMessage = FText::FromString(OutErrorString);
				return false;
			}
		}
	}
	return true;
}

FReply SRigHierarchy::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// only allow drops onto empty space of the widget (when there's no target item under the mouse)
	TSharedPtr<FRigTreeElement> ItemAtMouse = TreeView->FindItemAtPosition(DragDropEvent.GetScreenSpacePosition());
	if(ItemAtMouse.IsValid())
	{
		return FReply::Unhandled();
	}
	
	return OnAcceptDrop(DragDropEvent, EItemDropZone::OntoItem, nullptr);
}

void SRigHierarchy::HandleResetTransform(bool bSelectionOnly)
{
	if ((IsMultiSelected(true) || !bSelectionOnly) && ControlRigEditor.IsValid())
	{
		if (UControlRigBlueprint* Blueprint = ControlRigEditor.Pin()->GetControlRigBlueprint())
		{
			if (URigHierarchy* DebuggedHierarchy = GetHierarchy())
			{
				FScopedTransaction Transaction(LOCTEXT("HierarchyResetTransforms", "Reset Transforms"));

				TArray<FRigElementKey> KeysToReset = GetSelectedKeys();
				if (!bSelectionOnly)
				{
					KeysToReset = DebuggedHierarchy->GetAllKeys(true, ERigElementType::Control);

					// Bone Transforms can also be modified, support reset for them as well
					KeysToReset.Append(DebuggedHierarchy->GetAllKeys(true, ERigElementType::Bone));
				}

				for (FRigElementKey Key : KeysToReset)
				{
					const FTransform InitialTransform = GetHierarchy()->GetInitialLocalTransform(Key);
					GetHierarchy()->SetLocalTransform(Key, InitialTransform, false, true, true, true);
					DebuggedHierarchy->SetLocalTransform(Key, InitialTransform, false, true, true);

					if (Key.Type == ERigElementType::Bone)
					{
						Blueprint->RemoveTransientControl(Key);
						ControlRigEditor.Pin()->RemoveBoneModification(Key.Name); 
					}
				}
			}
		}
	}
}

void SRigHierarchy::HandleSetInitialTransformFromCurrentTransform()
{
	if (IsMultiSelected(false))
	{
		if (UControlRigBlueprint* Blueprint = ControlRigEditor.Pin()->GetControlRigBlueprint())
		{
			if (URigHierarchy* DebuggedHierarchy = GetHierarchy())
			{
				FScopedTransaction Transaction(LOCTEXT("HierarchySetInitialTransforms", "Set Initial Transforms"));

				TArray<FRigElementKey> SelectedKeys = GetSelectedKeys();
				TMap<FRigElementKey, FTransform> GlobalTransforms;
				TMap<FRigElementKey, FTransform> ParentGlobalTransforms;

				for (const FRigElementKey& SelectedKey : SelectedKeys)
				{
					GlobalTransforms.FindOrAdd(SelectedKey) = DebuggedHierarchy->GetGlobalTransform(SelectedKey);
					ParentGlobalTransforms.FindOrAdd(SelectedKey) = DebuggedHierarchy->GetParentTransform(SelectedKey);
				}

				URigHierarchy* DefaultHierarchy = GetDefaultHierarchy();
				
				for (const FRigElementKey& SelectedKey : SelectedKeys)
				{
					FTransform GlobalTransform = GlobalTransforms[SelectedKey];
					FTransform LocalTransform = GlobalTransform.GetRelativeTransform(ParentGlobalTransforms[SelectedKey]);

					if (SelectedKey.Type == ERigElementType::Control)
					{
						if(FRigControlElement* ControlElement = DebuggedHierarchy->Find<FRigControlElement>(SelectedKey))
						{
							DebuggedHierarchy->SetControlOffsetTransform(ControlElement, LocalTransform, ERigTransformType::InitialLocal, true, true, false, true);
							DebuggedHierarchy->SetControlOffsetTransform(ControlElement, LocalTransform, ERigTransformType::CurrentLocal, true, true, false, true);
							DebuggedHierarchy->SetTransform(ControlElement, FTransform::Identity, ERigTransformType::InitialLocal, true, true, false, true);
							DebuggedHierarchy->SetTransform(ControlElement, FTransform::Identity, ERigTransformType::CurrentLocal, true, true, false, true);
						}

						if(DefaultHierarchy)
						{
							if(FRigControlElement* ControlElement = DefaultHierarchy->Find<FRigControlElement>(SelectedKey))
							{
								DefaultHierarchy->SetControlOffsetTransform(ControlElement, LocalTransform, ERigTransformType::InitialLocal, true, true);
								DefaultHierarchy->SetControlOffsetTransform(ControlElement, LocalTransform, ERigTransformType::CurrentLocal, true, true);
								DefaultHierarchy->SetTransform(ControlElement, FTransform::Identity, ERigTransformType::InitialLocal, true, true);
								DefaultHierarchy->SetTransform(ControlElement, FTransform::Identity, ERigTransformType::CurrentLocal, true, true);
							}
						}
					}
					else if (SelectedKey.Type == ERigElementType::Null ||
						SelectedKey.Type == ERigElementType::Bone)
					{
						FTransform InitialTransform = LocalTransform;
						if (ControlRigEditor.Pin()->PreviewInstance)
						{
							if (FAnimNode_ModifyBone* ModifyBone = ControlRigEditor.Pin()->PreviewInstance->FindModifiedBone(SelectedKey.Name))
							{
								InitialTransform.SetTranslation(ModifyBone->Translation);
								InitialTransform.SetRotation(FQuat(ModifyBone->Rotation));
								InitialTransform.SetScale3D(ModifyBone->Scale);
							}
						}

						if(FRigTransformElement* TransformElement = DebuggedHierarchy->Find<FRigTransformElement>(SelectedKey))
						{
							DebuggedHierarchy->SetTransform(TransformElement, LocalTransform, ERigTransformType::InitialLocal, true, true, false, true);
							DebuggedHierarchy->SetTransform(TransformElement, LocalTransform, ERigTransformType::CurrentLocal, true, true, false, true);
						}

						if(DefaultHierarchy)
						{
							if(FRigTransformElement* TransformElement = DefaultHierarchy->Find<FRigTransformElement>(SelectedKey))
							{
								DefaultHierarchy->SetTransform(TransformElement, LocalTransform, ERigTransformType::InitialLocal, true, true);
								DefaultHierarchy->SetTransform(TransformElement, LocalTransform, ERigTransformType::CurrentLocal, true, true);
							}
						}
					}
				}
			}
		}
	}
}

void SRigHierarchy::HandleFrameSelection()
{
	TArray<TSharedPtr<FRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
	for (TSharedPtr<FRigTreeElement> SelectedItem : SelectedItems)
	{
		TreeView->SetExpansionRecursive(SelectedItem, true, true);
	}

	if (SelectedItems.Num() > 0)
	{
		TreeView->RequestScrollIntoView(SelectedItems.Last());
	}
}

void SRigHierarchy::HandleControlBoneOrSpaceTransform()
{
	UControlRigBlueprint* Blueprint = ControlRigEditor.Pin()->GetControlRigBlueprint();
	if (Blueprint == nullptr)
	{
		return;
	}

	UControlRig* DebuggedControlRig = Cast<UControlRig>(Blueprint->GetObjectBeingDebugged());
	if(DebuggedControlRig == nullptr)
	{
		return;
	}

	TArray<FRigElementKey> SelectedKeys = GetSelectedKeys();
	if (SelectedKeys.Num() == 1)
	{
		if (SelectedKeys[0].Type == ERigElementType::Bone ||
			SelectedKeys[0].Type == ERigElementType::Null)
		{
			if(!DebuggedControlRig->GetHierarchy()->IsProcedural(SelectedKeys[0]))
			{
				Blueprint->AddTransientControl(SelectedKeys[0]);
			}
		}
	}
}

void SRigHierarchy::HandleUnparent()
{
	UControlRigBlueprint* Blueprint = ControlRigEditor.Pin()->GetControlRigBlueprint();
	if (Blueprint == nullptr)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("HierarchyTreeUnparentSelected", "Unparent selected items from hierarchy"));

	bool bUnparentImportedBones = false;
	bool bConfirmedByUser = false;

	TArray<FRigElementKey> SelectedKeys = GetSelectedKeys();
	TMap<FRigElementKey, FTransform> InitialTransforms;
	TMap<FRigElementKey, FTransform> GlobalTransforms;

	for (const FRigElementKey& SelectedKey : SelectedKeys)
	{
		URigHierarchy* Hierarchy = GetHierarchy();
		InitialTransforms.Add(SelectedKey, Hierarchy->GetInitialGlobalTransform(SelectedKey));
		GlobalTransforms.Add(SelectedKey, Hierarchy->GetGlobalTransform(SelectedKey));
	}

	for (const FRigElementKey& SelectedKey : SelectedKeys)
	{
		TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
		TGuardValue<bool> SuspendBlueprintNotifs(ControlRigBlueprint->bSuspendAllNotifications, true);

		URigHierarchy* Hierarchy = GetDefaultHierarchy();
		check(Hierarchy);

		URigHierarchyController* Controller = Hierarchy->GetController(true);
		check(Controller);

		const FTransform& InitialTransform = InitialTransforms.FindChecked(SelectedKey);
		const FTransform& GlobalTransform = GlobalTransforms.FindChecked(SelectedKey);

		switch (SelectedKey.Type)
		{
			case ERigElementType::Bone:
			{
				
				bool bIsImportedBone = false;
				if(FRigBoneElement* BoneElement = Hierarchy->Find<FRigBoneElement>(SelectedKey))
				{
					bIsImportedBone = BoneElement->BoneType == ERigBoneType::Imported;
				}
					
				if (bIsImportedBone && !bConfirmedByUser)
				{
					FText ConfirmUnparent = LOCTEXT("ConfirmUnparentBoneHierarchy", "Unparenting imported(white) bones can cause issues with animation - are you sure ?");

					FSuppressableWarningDialog::FSetupInfo Info(ConfirmUnparent, LOCTEXT("UnparentImportedBone", "Unparent Imported Bone"), "UnparentImportedBoneHierarchy_Warning");
					Info.ConfirmText = LOCTEXT("UnparentImportedBoneHierarchy_Yes", "Yes");
					Info.CancelText = LOCTEXT("UnparentImportedBoneHierarchy_No", "No");

					FSuppressableWarningDialog UnparentImportedBonesInHierarchy(Info);
					bUnparentImportedBones = UnparentImportedBonesInHierarchy.ShowModal() != FSuppressableWarningDialog::Cancel;
					bConfirmedByUser = true;
				}

				if (bUnparentImportedBones || !bIsImportedBone)
				{
					Controller->RemoveAllParents(SelectedKey, true, true, true);
				}
				break;
			}
			case ERigElementType::Null:
			case ERigElementType::Control:
			{
				Controller->RemoveAllParents(SelectedKey, true, true, true);
				break;
			}
			default:
			{
				break;
			}
		}

		Hierarchy->SetInitialGlobalTransform(SelectedKey, InitialTransform, true, true);
		Hierarchy->SetGlobalTransform(SelectedKey, GlobalTransform, false, true, true);
	}

	ControlRigBlueprint->PropagateHierarchyFromBPToInstances();
	ControlRigEditor.Pin()->OnHierarchyChanged();
	RefreshTreeView();
	FSlateApplication::Get().DismissAllMenus();
}

bool SRigHierarchy::FindClosestBone(const FVector& Point, FName& OutRigElementName, FTransform& OutGlobalTransform) const
{
	if (URigHierarchy* DebuggedHierarchy = GetHierarchy())
	{
		float NearestDistance = BIG_NUMBER;

		DebuggedHierarchy->ForEach<FRigBoneElement>([&] (FRigBoneElement* Element) -> bool
		{
			const FTransform CurTransform = DebuggedHierarchy->GetTransform(Element, ERigTransformType::CurrentGlobal);
            const float CurDistance = FVector::Distance(CurTransform.GetLocation(), Point);
            if (CurDistance < NearestDistance)
            {
                NearestDistance = CurDistance;
                OutGlobalTransform = CurTransform;
                OutRigElementName = Element->GetName();
            }
            return true;
		});

		return (OutRigElementName != NAME_None);
	}
	return false;
}

void SRigHierarchy::HandleTestSpaceSwitching()
{
	if (FControlRigEditorEditMode* EditMode = ControlRigEditor.Pin()->GetEditMode())
	{
		/// toooo centralize code here
		EditMode->OpenSpacePickerWidget();
	}
}

void SRigHierarchy::HandleParent(const FToolMenuContext& Context)
{
	if (UControlRigContextMenuContext* MenuContext = Cast<UControlRigContextMenuContext>(Context.FindByClass(UControlRigContextMenuContext::StaticClass())))
	{
		const FControlRigRigHierarchyDragAndDropContext DragAndDropContext = MenuContext->GetRigHierarchyDragAndDropContext();
		ReparentOrMatchTransform(DragAndDropContext.DraggedElementKeys, DragAndDropContext.TargetElementKey, true);
	}
}

void SRigHierarchy::HandleAlign(const FToolMenuContext& Context)
{
	if (UControlRigContextMenuContext* MenuContext = Cast<UControlRigContextMenuContext>(Context.FindByClass(UControlRigContextMenuContext::StaticClass())))
	{
		const FControlRigRigHierarchyDragAndDropContext DragAndDropContext = MenuContext->GetRigHierarchyDragAndDropContext();
		ReparentOrMatchTransform(DragAndDropContext.DraggedElementKeys, DragAndDropContext.TargetElementKey, false);
	}
}

FReply SRigHierarchy::ReparentOrMatchTransform(const TArray<FRigElementKey>& DraggedKeys, FRigElementKey TargetKey, bool bReparentItems)
{
	bool bMatchTransforms = !bReparentItems;

	URigHierarchy* DebuggedHierarchy = GetHierarchy();
	URigHierarchy* Hierarchy = GetDefaultHierarchy();

	if (Hierarchy && ControlRigBlueprint.IsValid())
	{
		URigHierarchyController* Controller = Hierarchy->GetController(true);
		if(Controller == nullptr)
		{
			return FReply::Unhandled();
		}

		TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
		TGuardValue<bool> SuspendBlueprintNotifs(ControlRigBlueprint->bSuspendAllNotifications, true);
		FScopedTransaction Transaction(LOCTEXT("HierarchyDragAndDrop", "Drag & Drop"));
		FRigHierarchyInteractionBracket InteractionBracket(Hierarchy);

		FTransform TargetGlobalTransform = DebuggedHierarchy->GetGlobalTransform(TargetKey);

		for (const FRigElementKey& DraggedKey : DraggedKeys)
		{
			if (DraggedKey == TargetKey)
			{
				return FReply::Unhandled();
			}

			if (bReparentItems)
			{
				if(Hierarchy->IsParentedTo(TargetKey, DraggedKey))
				{
					return FReply::Unhandled();
				}
			}

			if (DraggedKey.Type == ERigElementType::Bone)
			{
				if(FRigBoneElement* BoneElement = Hierarchy->Find<FRigBoneElement>(DraggedKey))
				{
					if (BoneElement->BoneType == ERigBoneType::Imported && BoneElement->ParentElement != nullptr)
					{
						FText ConfirmText = bMatchTransforms ?
							LOCTEXT("ConfirmMatchTransform", "Matching transforms of imported(white) bones can cause issues with animation - are you sure ?") :
							LOCTEXT("ConfirmReparentBoneHierarchy", "Reparenting imported(white) bones can cause issues with animation - are you sure ?");

						FText TitleText = bMatchTransforms ?
							LOCTEXT("MatchTransformImportedBone", "Match Transform on Imported Bone") :
							LOCTEXT("ReparentImportedBone", "Reparent Imported Bone");

						FSuppressableWarningDialog::FSetupInfo Info(ConfirmText, TitleText, "SRigHierarchy_Warning");
						Info.ConfirmText = LOCTEXT("SRigHierarchy_Warning_Yes", "Yes");
						Info.CancelText = LOCTEXT("SRigHierarchy_Warning_No", "No");

						FSuppressableWarningDialog ChangeImportedBonesInHierarchy(Info);
						if (ChangeImportedBonesInHierarchy.ShowModal() == FSuppressableWarningDialog::Cancel)
						{
							return FReply::Unhandled();
						}
					}
				}
			}
		}

		for (const FRigElementKey& DraggedKey : DraggedKeys)
		{
			if (bMatchTransforms)
			{
				if (DraggedKey.Type == ERigElementType::Control)
				{
					int32 ControlIndex = DebuggedHierarchy->GetIndex(DraggedKey);
					if (ControlIndex == INDEX_NONE)
					{
						continue;
					}

					FTransform ParentTransform = DebuggedHierarchy->GetParentTransformByIndex(ControlIndex, false);
					FTransform OffsetTransform = TargetGlobalTransform.GetRelativeTransform(ParentTransform);

					Hierarchy->SetControlOffsetTransformByIndex(ControlIndex, OffsetTransform, ERigTransformType::InitialLocal, true, true, true);
					Hierarchy->SetControlOffsetTransformByIndex(ControlIndex, OffsetTransform, ERigTransformType::CurrentLocal, true, true, true);
					Hierarchy->SetLocalTransform(DraggedKey, FTransform::Identity, true, true, true, true);
					Hierarchy->SetInitialLocalTransform(DraggedKey, FTransform::Identity, true, true, true);
					DebuggedHierarchy->SetControlOffsetTransformByIndex(ControlIndex, OffsetTransform, ERigTransformType::InitialLocal, true, true);
					DebuggedHierarchy->SetControlOffsetTransformByIndex(ControlIndex, OffsetTransform, ERigTransformType::CurrentLocal, true, true);
					DebuggedHierarchy->SetLocalTransform(DraggedKey, FTransform::Identity, true, true, true);
					DebuggedHierarchy->SetInitialLocalTransform(DraggedKey, FTransform::Identity, true, true);
				}
				else
				{
					Hierarchy->SetInitialGlobalTransform(DraggedKey, TargetGlobalTransform, true, true);
					Hierarchy->SetGlobalTransform(DraggedKey, TargetGlobalTransform, false, true, true);
					DebuggedHierarchy->SetInitialGlobalTransform(DraggedKey, TargetGlobalTransform, true, true);
					DebuggedHierarchy->SetGlobalTransform(DraggedKey, TargetGlobalTransform, false, true, true);
				}
				continue;
			}

			FRigElementKey ParentKey = TargetKey;

			const FTransform InitialGlobalTransform = DebuggedHierarchy->GetInitialGlobalTransform(DraggedKey);
			const FTransform CurrentGlobalTransform = DebuggedHierarchy->GetGlobalTransform(DraggedKey);
			const FTransform InitialLocalTransform = DebuggedHierarchy->GetInitialLocalTransform(DraggedKey);
			const FTransform CurrentLocalTransform = DebuggedHierarchy->GetLocalTransform(DraggedKey);
			const FTransform CurrentGlobalOffsetTransform = DebuggedHierarchy->GetGlobalControlOffsetTransform(DraggedKey, false);

			if(ParentKey.IsValid())
			{
				Controller->SetParent(DraggedKey, ParentKey, true, true, true);
			}
			else
			{
				Controller->RemoveAllParents(DraggedKey, true, true, true);
			}

			if (DraggedKey.Type == ERigElementType::Control)
			{
				int32 ControlIndex = DebuggedHierarchy->GetIndex(DraggedKey);
				if (ControlIndex == INDEX_NONE)
				{
					continue;
				}

				const FTransform GlobalParentTransform = DebuggedHierarchy->GetGlobalTransform(ParentKey, false);
				const FTransform LocalOffsetTransform = CurrentGlobalOffsetTransform.GetRelativeTransform(GlobalParentTransform);

				Hierarchy->SetControlOffsetTransformByIndex(ControlIndex, LocalOffsetTransform, ERigTransformType::InitialLocal, true, true, true);
				Hierarchy->SetControlOffsetTransformByIndex(ControlIndex, LocalOffsetTransform, ERigTransformType::CurrentLocal, true, true, true);
				Hierarchy->SetLocalTransform(DraggedKey, CurrentLocalTransform, true, true, true, true);
				Hierarchy->SetInitialLocalTransform(DraggedKey, InitialLocalTransform, true, true, true);
				DebuggedHierarchy->SetControlOffsetTransformByIndex(ControlIndex, LocalOffsetTransform, ERigTransformType::InitialLocal, true, true);
				DebuggedHierarchy->SetControlOffsetTransformByIndex(ControlIndex, LocalOffsetTransform, ERigTransformType::CurrentLocal, true, true);
				DebuggedHierarchy->SetLocalTransform(DraggedKey, CurrentLocalTransform, true, true, true);
				DebuggedHierarchy->SetInitialLocalTransform(DraggedKey, InitialLocalTransform, true, true);
			}
			else
			{
				DebuggedHierarchy->SetInitialGlobalTransform(DraggedKey, InitialGlobalTransform, true, true);
				DebuggedHierarchy->SetGlobalTransform(DraggedKey, CurrentGlobalTransform, false, true, true);
				Hierarchy->SetInitialGlobalTransform(DraggedKey, InitialGlobalTransform, true, true);
				Hierarchy->SetGlobalTransform(DraggedKey, CurrentGlobalTransform, false, true, true);
			}
		}
	}

	ControlRigBlueprint->PropagateHierarchyFromBPToInstances();

	if(bReparentItems)
	{
		TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
		ControlRigBlueprint->BroadcastRefreshEditor();
		RefreshTreeView();
	}

		
	return FReply::Handled();

}

void SRigHierarchy::HandleSetInitialTransformFromClosestBone()
{
	if (IsControlOrNullSelected(false))
	{
		if (UControlRigBlueprint* Blueprint = ControlRigEditor.Pin()->GetControlRigBlueprint())
		{
			if (URigHierarchy* DebuggedHierarchy = GetHierarchy())
			{
				URigHierarchy* Hierarchy = GetDefaultHierarchy();
  				check(Hierarchy);

				FScopedTransaction Transaction(LOCTEXT("HierarchySetInitialTransforms", "Set Initial Transforms"));

				TArray<FRigElementKey> SelectedKeys = GetSelectedKeys();
				TMap<FRigElementKey, FTransform> ClosestTransforms;
				TMap<FRigElementKey, FTransform> ParentGlobalTransforms;

				for (const FRigElementKey& SelectedKey : SelectedKeys)
				{
					if (SelectedKey.Type == ERigElementType::Control || SelectedKey.Type == ERigElementType::Null)
					{
						const FTransform GlobalTransform = DebuggedHierarchy->GetGlobalTransform(SelectedKey);
						FTransform ClosestTransform;
						FName ClosestRigElement;

						if (!FindClosestBone(GlobalTransform.GetLocation(), ClosestRigElement, ClosestTransform))
						{
							continue;
						}

						ClosestTransforms.FindOrAdd(SelectedKey) = ClosestTransform;
						ParentGlobalTransforms.FindOrAdd(SelectedKey) = DebuggedHierarchy->GetParentTransform(SelectedKey);
					}
				}

				for (const FRigElementKey& SelectedKey : SelectedKeys)
				{
					if (!ClosestTransforms.Contains(SelectedKey))
					{
						continue;
					}
					FTransform GlobalTransform = ClosestTransforms[SelectedKey];
					FTransform LocalTransform = GlobalTransform.GetRelativeTransform(ParentGlobalTransforms[SelectedKey]);

					if (SelectedKey.Type == ERigElementType::Control)
					{
						if(FRigControlElement* ControlElement = Hierarchy->Find<FRigControlElement>(SelectedKey))
						{
							Hierarchy->SetControlOffsetTransform(ControlElement, LocalTransform, ERigTransformType::InitialLocal, true, true, false, true);
							Hierarchy->SetControlOffsetTransform(ControlElement, LocalTransform, ERigTransformType::CurrentLocal, true, true, false, true);
							Hierarchy->SetTransform(ControlElement, FTransform::Identity, ERigTransformType::InitialLocal, true, true, false, true);
							Hierarchy->SetTransform(ControlElement, FTransform::Identity, ERigTransformType::CurrentLocal, true, true, false, true);
						}
						if(FRigControlElement* ControlElement = DebuggedHierarchy->Find<FRigControlElement>(SelectedKey))
						{
							DebuggedHierarchy->SetControlOffsetTransform(ControlElement, LocalTransform, ERigTransformType::InitialLocal, true, true);
							DebuggedHierarchy->SetControlOffsetTransform(ControlElement, LocalTransform, ERigTransformType::CurrentLocal, true, true);
							DebuggedHierarchy->SetTransform(ControlElement, FTransform::Identity, ERigTransformType::InitialLocal, true, true);
							DebuggedHierarchy->SetTransform(ControlElement, FTransform::Identity, ERigTransformType::CurrentLocal, true, true);
						}
					}
					else if (SelectedKey.Type == ERigElementType::Null ||
                        SelectedKey.Type == ERigElementType::Bone)
					{
						FTransform InitialTransform = LocalTransform;

						if(FRigTransformElement* TransformElement = Hierarchy->Find<FRigTransformElement>(SelectedKey))
						{
							Hierarchy->SetTransform(TransformElement, LocalTransform, ERigTransformType::InitialLocal, true, true, false, true);
							Hierarchy->SetTransform(TransformElement, LocalTransform, ERigTransformType::CurrentLocal, true, true, false, true);
						}
						if(FRigTransformElement* TransformElement = DebuggedHierarchy->Find<FRigTransformElement>(SelectedKey))
						{
							DebuggedHierarchy->SetTransform(TransformElement, LocalTransform, ERigTransformType::InitialLocal, true, true);
							DebuggedHierarchy->SetTransform(TransformElement, LocalTransform, ERigTransformType::CurrentLocal, true, true);
						}
					}
				}
			}
		}
	}
}

void SRigHierarchy::HandleSetShapeTransformFromCurrent()
{
	if (IsControlSelected(false))
	{
		if (UControlRigBlueprint* Blueprint = ControlRigEditor.Pin()->GetControlRigBlueprint())
		{
			if (URigHierarchy* DebuggedHierarchy = GetHierarchy())
			{
				FScopedTransaction Transaction(LOCTEXT("HierarchySetShapeTransforms", "Set Shape Transforms"));

				FRigHierarchyInteractionBracket InteractionBracket(GetHierarchy());
				FRigHierarchyInteractionBracket DebuggedInteractionBracket(DebuggedHierarchy);

				TArray<TSharedPtr<FRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
				for (const TSharedPtr<FRigTreeElement>& SelectedItem : SelectedItems)
				{
					if(FRigControlElement* ControlElement = DebuggedHierarchy->Find<FRigControlElement>(SelectedItem->Key))
					{
						const FRigElementKey Key = ControlElement->GetKey();
						
						if (ControlElement->Settings.SupportsShape())
						{
							const FTransform OffsetGlobalTransform = DebuggedHierarchy->GetGlobalControlOffsetTransform(Key); 
							const FTransform ShapeGlobalTransform = DebuggedHierarchy->GetGlobalControlShapeTransform(Key);
							const FTransform ShapeLocalTransform = ShapeGlobalTransform.GetRelativeTransform(OffsetGlobalTransform);
							
							DebuggedHierarchy->SetControlShapeTransform(Key, ShapeLocalTransform, true, true);
							DebuggedHierarchy->SetControlShapeTransform(Key, ShapeLocalTransform, false, true);
							GetHierarchy()->SetControlShapeTransform(Key, ShapeLocalTransform, true, true);
							GetHierarchy()->SetControlShapeTransform(Key, ShapeLocalTransform, false, true);

							DebuggedHierarchy->SetLocalTransform(Key, FTransform::Identity, false, true, true);
							DebuggedHierarchy->SetLocalTransform(Key, FTransform::Identity, true, true, true);
							GetHierarchy()->SetLocalTransform(Key, FTransform::Identity, false, true, true, true);
							GetHierarchy()->SetLocalTransform(Key, FTransform::Identity, true, true, true, true);
						}

						if (FControlRigEditorEditMode* EditMode = ControlRigEditor.Pin()->GetEditMode())
						{
							EditMode->RequestToRecreateControlShapeActors();
						}
					}
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

