// Copyright Epic Games, Inc. All Rights Reserved.


#include "Editor/SRigCurveContainer.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Input/STextEntryPopup.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Commands/GenericCommands.h"
#include "Editor/RigCurveContainerCommands.h"
#include "Editor/ControlRigEditor.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "ControlRig.h"
#include "ControlRigBlueprint.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "SRigCurveContainer"

static const FName ColumnId_RigCurveNameLabel( "Curve" );
static const FName ColumnID_RigCurveValueLabel( "Value" );

//////////////////////////////////////////////////////////////////////////
// SRigCurveListRow

void SRigCurveListRow::Construct( const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	Item = InArgs._Item;
	OnTextCommitted = InArgs._OnTextCommitted;
	OnSetRigCurveValue = InArgs._OnSetRigCurveValue;
	OnGetRigCurveValue = InArgs._OnGetRigCurveValue;
	OnGetFilterText = InArgs._OnGetFilterText;

	check( Item.IsValid() );

	SMultiColumnTableRow< FDisplayedRigCurveInfoPtr >::Construct( FSuperRowType::FArguments(), InOwnerTableView );
}

TSharedRef< SWidget > SRigCurveListRow::GenerateWidgetForColumn( const FName& ColumnName )
{
	if ( ColumnName == ColumnId_RigCurveNameLabel )
	{
		return
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4)
			.VAlign(VAlign_Center)
			[
				SAssignNew(Item->EditableText, SInlineEditableTextBlock)
				.OnTextCommitted(OnTextCommitted)
				.ColorAndOpacity(this, &SRigCurveListRow::GetItemTextColor)
				.IsSelected(this, &SRigCurveListRow::IsSelected)
				.Text(this, &SRigCurveListRow::GetItemName)
				.HighlightText(this, &SRigCurveListRow::GetFilterText)
			];
	}
	else if ( ColumnName == ColumnID_RigCurveValueLabel )
	{
		// Encase the SSpinbox in an SVertical box so we can apply padding. Setting ItemHeight on the containing SListView has no effect :-(
		return
			SNew( SVerticalBox )

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0.0f, 1.0f )
			.VAlign( VAlign_Center )
			[
				SNew( SSpinBox<float> )
				.Value( this, &SRigCurveListRow::GetValue )
				.OnValueChanged( this, &SRigCurveListRow::OnRigCurveValueChanged )
				.OnValueCommitted( this, &SRigCurveListRow::OnRigCurveValueValueCommitted )
				.IsEnabled(false)
			];
	}

	return SNullWidget::NullWidget;
}

void SRigCurveListRow::OnRigCurveValueChanged( float NewValue )
{
	Item->Value = NewValue;

	OnSetRigCurveValue.ExecuteIfBound(Item->CurveName, NewValue);
}

void SRigCurveListRow::OnRigCurveValueValueCommitted( float NewValue, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus)
	{
		OnRigCurveValueChanged(NewValue);
	}
}


FText SRigCurveListRow::GetItemName() const
{
	return FText::FromName(Item->CurveName);
}

FText SRigCurveListRow::GetFilterText() const
{
	if (OnGetFilterText.IsBound())
	{
		return OnGetFilterText.Execute();
	}
	else
	{
		return FText::GetEmpty();
	}
}

FSlateColor SRigCurveListRow::GetItemTextColor() const
{
	// If row is selected, show text as black to make it easier to read
	if (IsSelected())
	{
		return FLinearColor(0, 0, 0);
	}

	return FLinearColor(1, 1, 1);
}

float SRigCurveListRow::GetValue() const 
{ 
	if (OnGetRigCurveValue.IsBound())
	{
		return OnGetRigCurveValue.Execute(Item->CurveName);
	}

	return 0.f;
}

//////////////////////////////////////////////////////////////////////////
// SRigCurveContainer

void SRigCurveContainer::Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor)
{
	ControlRigEditor = InControlRigEditor;
	ControlRigBlueprint = InControlRigEditor.Get().GetControlRigBlueprint();
	bIsChangingRigHierarchy = false;

	ControlRigBlueprint->Hierarchy->OnModified().AddRaw(this, &SRigCurveContainer::OnHierarchyModified);
	ControlRigBlueprint->OnRefreshEditor().AddRaw(this, &SRigCurveContainer::HandleRefreshEditorFromBlueprint);

	// Register and bind all our menu commands
	FCurveContainerCommands::Register();
	BindCommands();

	ChildSlot
	[
		SNew( SVerticalBox )
		
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0,2)
		[
			SNew(SHorizontalBox)
			// Filter entry
			+SHorizontalBox::Slot()
			.FillWidth( 1 )
			[
				SAssignNew( NameFilterBox, SSearchBox )
				.SelectAllTextWhenFocused( true )
				.OnTextChanged( this, &SRigCurveContainer::OnFilterTextChanged )
				.OnTextCommitted( this, &SRigCurveContainer::OnFilterTextCommitted )
			]
		]

		+ SVerticalBox::Slot()
		.FillHeight( 1.0f )		// This is required to make the scrollbar work, as content overflows Slate containers by default
		[
			SAssignNew( RigCurveListView, SRigCurveListType )
			.ListItemsSource( &RigCurveList )
			.OnGenerateRow( this, &SRigCurveContainer::GenerateRigCurveRow )
			.OnContextMenuOpening( this, &SRigCurveContainer::OnGetContextMenuContent )
			.ItemHeight( 22.0f )
			.SelectionMode(ESelectionMode::Multi)
			.OnSelectionChanged( this, &SRigCurveContainer::OnSelectionChanged )
			.HeaderRow
			(
				SNew( SHeaderRow )
				+ SHeaderRow::Column( ColumnId_RigCurveNameLabel )
				.FillWidth(1.f)
				.DefaultLabel( LOCTEXT( "RigCurveNameLabel", "Curve" ) )

				+ SHeaderRow::Column( ColumnID_RigCurveValueLabel )
				.FillWidth(1.f)
				.DefaultLabel( LOCTEXT( "RigCurveValueLabel", "Value" ) )
			)
		]
	];

	CreateRigCurveList();

	if(ControlRigEditor.IsValid())
	{
		ControlRigEditor.Pin()->OnControlRigEditorClosed().AddSP(this, &SRigCurveContainer::OnEditorClose);
	}
}

SRigCurveContainer::~SRigCurveContainer()
{
	const FControlRigEditor* Editor = ControlRigEditor.IsValid() ? ControlRigEditor.Pin().Get() : nullptr;
	OnEditorClose(Editor, ControlRigBlueprint.Get());
}

FReply SRigCurveContainer::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (UICommandList.IsValid() && UICommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SRigCurveContainer::BindCommands()
{
	// This should not be called twice on the same instance
	check(!UICommandList.IsValid());

	UICommandList = MakeShareable(new FUICommandList);

	FUICommandList& CommandList = *UICommandList;

	// Grab the list of menu commands to bind...
	const FCurveContainerCommands& MenuActions = FCurveContainerCommands::Get();

	// ...and bind them all

	CommandList.MapAction(
		FGenericCommands::Get().Rename,
		FExecuteAction::CreateSP(this, &SRigCurveContainer::OnRenameClicked),
		FCanExecuteAction::CreateSP(this, &SRigCurveContainer::CanRename));

	CommandList.MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SRigCurveContainer::OnDeleteNameClicked),
		FCanExecuteAction::CreateSP(this, &SRigCurveContainer::CanDelete));

	CommandList.MapAction(
		MenuActions.AddCurve,
		FExecuteAction::CreateSP(this, &SRigCurveContainer::OnAddClicked),
		FCanExecuteAction());
}

void SRigCurveContainer::OnPreviewMeshChanged(class USkeletalMesh* OldPreviewMesh, class USkeletalMesh* NewPreviewMesh)
{
	RefreshCurveList();
}

void SRigCurveContainer::OnFilterTextChanged( const FText& SearchText )
{
	FilterText = SearchText;

	RefreshCurveList();
}

void SRigCurveContainer::OnFilterTextCommitted( const FText& SearchText, ETextCommit::Type CommitInfo )
{
	// Just do the same as if the user typed in the box
	OnFilterTextChanged( SearchText );
}

TSharedRef<ITableRow> SRigCurveContainer::GenerateRigCurveRow(FDisplayedRigCurveInfoPtr InInfo, const TSharedRef<STableViewBase>& OwnerTable)
{
	check( InInfo.IsValid() );

	return
		SNew( SRigCurveListRow, OwnerTable)
		.Item( InInfo )
		.OnTextCommitted(this, &SRigCurveContainer::OnNameCommitted, InInfo)
		.OnSetRigCurveValue(this, &SRigCurveContainer::SetCurveValue)
		.OnGetRigCurveValue(this, &SRigCurveContainer::GetCurveValue)
		.OnGetFilterText(this, &SRigCurveContainer::GetFilterText);
}

TSharedPtr<SWidget> SRigCurveContainer::OnGetContextMenuContent() const
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder( bShouldCloseWindowAfterMenuSelection, UICommandList);

	const FCurveContainerCommands& Actions = FCurveContainerCommands::Get();

	MenuBuilder.BeginSection("RigCurveAction", LOCTEXT( "CurveAction", "Curve Actions" ) );

	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Rename, NAME_None, LOCTEXT("RenameSmartNameLabel", "Rename Curve"), LOCTEXT("RenameSmartNameToolTip", "Rename the selected curve"));
	MenuBuilder.AddMenuEntry(FGenericCommands::Get().Delete, NAME_None, LOCTEXT("DeleteSmartNameLabel", "Delete Curve"), LOCTEXT("DeleteSmartNameToolTip", "Delete the selected curve"));
	MenuBuilder.AddMenuEntry(Actions.AddCurve);
	MenuBuilder.AddMenuSeparator();
	MenuBuilder.AddSubMenu(
		LOCTEXT("ImportSubMenu", "Import"),
		LOCTEXT("ImportSubMenu_ToolTip", "Import curves to the current rig. This only imports non-existing curve."),
		FNewMenuDelegate::CreateSP(const_cast<SRigCurveContainer*>(this), &SRigCurveContainer::CreateImportMenu)
	);


	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SRigCurveContainer::OnEditorClose(const FControlRigEditor* InEditor, UControlRigBlueprint* InBlueprint)
{
	if (InBlueprint)
	{
		InBlueprint->Hierarchy->OnModified().RemoveAll(this);
		InBlueprint->OnRefreshEditor().RemoveAll(this);
	}

	ControlRigBlueprint.Reset();
	ControlRigEditor.Reset();
}

void SRigCurveContainer::OnRenameClicked()
{
	TArray< FDisplayedRigCurveInfoPtr > SelectedItems = RigCurveListView->GetSelectedItems();

	SelectedItems[0]->EditableText->EnterEditingMode();
}

bool SRigCurveContainer::CanRename()
{
	return RigCurveListView->GetNumItemsSelected() == 1;
}

void SRigCurveContainer::OnAddClicked()
{
	TSharedRef<STextEntryPopup> TextEntry =
		SNew(STextEntryPopup)
		.Label(LOCTEXT("NewSmartnameLabel", "New Name"))
		.OnTextCommitted(this, &SRigCurveContainer::CreateNewNameEntry);

	FSlateApplication& SlateApp = FSlateApplication::Get();
	SlateApp.PushMenu(
		AsShared(),
		FWidgetPath(),
		TextEntry,
		SlateApp.GetCursorPos(),
		FPopupTransitionEffect::TypeInPopup
		);
}


void SRigCurveContainer::CreateNewNameEntry(const FText& CommittedText, ETextCommit::Type CommitType)
{
	FSlateApplication::Get().DismissAllMenus();
	if (!CommittedText.IsEmpty() && CommitType == ETextCommit::OnEnter)
	{
		URigHierarchy* Hierarchy = GetHierarchy();
		if (Hierarchy)
		{
			TGuardValue<bool> GuardReentry(bIsChangingRigHierarchy, true);

			const FName NewName = FName(*CommittedText.ToString());

			if(URigHierarchyController* Controller = Hierarchy->GetController())
			{
				const FRigElementKey CurveKey = Controller->AddCurve(NewName, 0.f, true);
				Controller->ClearSelection();
				Controller->SelectElement(CurveKey);
			}
		}

		FSlateApplication::Get().DismissAllMenus();
		RefreshCurveList();
	}
}

void SRigCurveContainer::CreateRigCurveList( const FString& SearchText )
{
	if(bIsChangingRigHierarchy)
	{
		return;
	}
	TGuardValue<bool> GuardReentry(bIsChangingRigHierarchy, true);

	if(!ControlRigBlueprint.IsValid())
	{
		return;
	}
	
	URigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		RigCurveList.Reset();

		// Iterate through all curves..
		Hierarchy->ForEach<FRigCurveElement>([this](FRigCurveElement* CurveElement) -> bool
		{
			const FString CurveString = CurveElement->GetName().ToString();
			
			// See if we pass the search filter
            if (!FilterText.IsEmpty() && !CurveString.Contains(*FilterText.ToString()))
            {
                return true;
            }

            TSharedRef<FDisplayedRigCurveInfo> NewItem = FDisplayedRigCurveInfo::Make(CurveElement->GetName());
            RigCurveList.Add(NewItem);

			return true;
		});

		// Sort final list
		struct FSortNamesAlphabetically
		{
			bool operator()(const FDisplayedRigCurveInfoPtr& A, const FDisplayedRigCurveInfoPtr& B) const
			{
				return (A.Get()->CurveName.Compare(B.Get()->CurveName) < 0);
			}
		};

		RigCurveList.Sort(FSortNamesAlphabetically());
	}
	RigCurveListView->RequestListRefresh();

	if (Hierarchy)
	{
		TArray<FRigElementKey> SelectedCurveKeys = Hierarchy->GetSelectedKeys(ERigElementType::Curve);
		for (const FRigElementKey& SelectedCurveKey : SelectedCurveKeys)
		{
			if(FRigCurveElement* CurveElement = Hierarchy->Find<FRigCurveElement>(SelectedCurveKey))
			{
				OnHierarchyModified(ERigHierarchyNotification::ElementSelected, Hierarchy, CurveElement);
			}
		}
	}

}

void SRigCurveContainer::RefreshCurveList()
{
	CreateRigCurveList(FilterText.ToString());
}

void SRigCurveContainer::OnNameCommitted(const FText& InNewName, ETextCommit::Type CommitType, FDisplayedRigCurveInfoPtr Item)
{
	URigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		if (CommitType == ETextCommit::OnEnter)
		{
			if(URigHierarchyController* Controller = Hierarchy->GetController())
			{
				FName NewName = FName(*InNewName.ToString());
				FName OldName = Item->CurveName;
				Controller->RenameElement(FRigElementKey(OldName, ERigElementType::Curve), NewName, true, true);
			}
		}
	}
}

void SRigCurveContainer::OnDeleteNameClicked()
{
	URigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		TGuardValue<bool> SuspendBlueprintNotifs(ControlRigBlueprint->bSuspendAllNotifications, true);

		TArray< FDisplayedRigCurveInfoPtr > SelectedItems = RigCurveListView->GetSelectedItems();
		for (auto Item : SelectedItems)
		{
			if(URigHierarchyController* Controller = Hierarchy->GetController())
			{
				Controller->RemoveElement(FRigElementKey(Item->CurveName, ERigElementType::Curve), true, true);
			}
		}
	}

	ControlRigBlueprint->PropagateHierarchyFromBPToInstances();
	RefreshCurveList();
}

bool SRigCurveContainer::CanDelete()
{
	return RigCurveListView->GetNumItemsSelected() > 0;
}

void SRigCurveContainer::SetCurveValue(const FName& CurveName, float CurveValue)
{
	URigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		Hierarchy->SetCurveValue(FRigElementKey(CurveName, ERigElementType::Curve), CurveValue, true);
	}
}

float SRigCurveContainer::GetCurveValue(const FName& CurveName)
{
	URigHierarchy* Hierarchy = GetInstanceHierarchy();
	if (Hierarchy)
	{
		return Hierarchy->GetCurveValue(FRigElementKey(CurveName, ERigElementType::Curve));
	}
	return 0.f;
}

void SRigCurveContainer::ChangeCurveName(const FName& OldName, const FName& NewName)
{
	TGuardValue<bool> GuardReentry(bIsChangingRigHierarchy, true);

	URigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		if(URigHierarchyController* Controller = Hierarchy->GetController())
		{
			Controller->RenameElement(FRigElementKey(OldName, ERigElementType::Curve), NewName, true, true);
		}
	}
}

void SRigCurveContainer::OnSelectionChanged(FDisplayedRigCurveInfoPtr Selection, ESelectInfo::Type SelectInfo)
{
	if (bIsChangingRigHierarchy)
	{
		return;
	}

	URigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		URigHierarchyController* Controller = Hierarchy->GetController();
		if(Controller == nullptr)
		{
			return;
		}

		FScopedTransaction ScopedTransaction(LOCTEXT("SelectCurveTransaction", "Select Curve"), !GIsTransacting);

		TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);

		TArray<FRigElementKey> OldSelection = Hierarchy->GetSelectedKeys(ERigElementType::Curve);
		TArray<FRigElementKey> NewSelection;

		TArray<FDisplayedRigCurveInfoPtr> SelectedItems = RigCurveListView->GetSelectedItems();
		for (const FDisplayedRigCurveInfoPtr& SelectedItem : SelectedItems)
		{
			NewSelection.Add(FRigElementKey(SelectedItem->CurveName, ERigElementType::Curve));
		}

		for (const FRigElementKey& PreviouslySelected : OldSelection)
		{
			if (NewSelection.Contains(PreviouslySelected))
			{
				continue;
			}
			Controller->DeselectElement(PreviouslySelected);
		}

		for (const FRigElementKey& NewlySelected : NewSelection)
		{
			Controller->SelectElement(NewlySelected);
		}
	}
}

void SRigCurveContainer::OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
	if (bIsChangingRigHierarchy)
	{
		return;
	}

	if(InElement)
	{
		if(!InElement->IsTypeOf(ERigElementType::Curve))
		{
			return;
		}
	}

	if (ControlRigBlueprint.IsValid())
	{
		if (ControlRigBlueprint->bSuspendAllNotifications)
		{
			return;
		}
	}

	switch(InNotif)
	{
		case ERigHierarchyNotification::ElementAdded:
		case ERigHierarchyNotification::ElementRemoved:
		case ERigHierarchyNotification::ElementRenamed:
		case ERigHierarchyNotification::HierarchyReset:
		{
			RefreshCurveList();
			break;
		}
		case ERigHierarchyNotification::ElementSelected:
    	case ERigHierarchyNotification::ElementDeselected:
		{
			if(InElement)
			{
				const bool bSelected = InNotif == ERigHierarchyNotification::ElementSelected;
				for(const FDisplayedRigCurveInfoPtr& Item : RigCurveList)
				{
					if (Item->CurveName == InElement->GetName())
					{
						RigCurveListView->SetItemSelection(Item, bSelected);
						break;
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

void SRigCurveContainer::HandleRefreshEditorFromBlueprint(UControlRigBlueprint* InBlueprint)
{
	RefreshCurveList();
}

void SRigCurveContainer::CreateImportMenu(FMenuBuilder& MenuBuilder)
{
	MenuBuilder.AddWidget(
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(3)
		[
			SNew(STextBlock)
			.Font(FAppStyle::GetFontStyle("ControlRig.Curve.Menu"))
			.Text(LOCTEXT("ImportMesh_Title", "Select Mesh"))
			.ToolTipText(LOCTEXT("ImportMesh_Tooltip", "Select Mesh to import Curve from... It will only import if the node doesn't exist in the current Curve."))
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(3)
		[
			SNew(SObjectPropertyEntryBox)
			//.ObjectPath_UObject(this, &SRigCurveContainer::GetCurrentHierarchy)
			.OnShouldFilterAsset(this, &SRigCurveContainer::ShouldFilterOnImport)
			.OnObjectChanged(this, &SRigCurveContainer::ImportCurve)
		]
		,
		FText()
		);
}

bool SRigCurveContainer::ShouldFilterOnImport(const FAssetData& AssetData) const
{
	return (AssetData.AssetClassPath != USkeletalMesh::StaticClass()->GetClassPathName() &&
		AssetData.AssetClassPath != USkeleton::StaticClass()->GetClassPathName());
}

void SRigCurveContainer::ImportCurve(const FAssetData& InAssetData)
{
	URigHierarchy* Hierarchy = GetHierarchy();
	if (Hierarchy)
	{
		TGuardValue<bool> SuspendBlueprintNotifs(ControlRigBlueprint->bSuspendAllNotifications, true);

		USkeleton* Skeleton = nullptr;
		if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(InAssetData.GetAsset()))
		{
			Skeleton = Mesh->GetSkeleton();
			ControlRigBlueprint->SourceCurveImport = Skeleton;
		}
		else 
		{
			Skeleton = Cast<USkeleton>(InAssetData.GetAsset());
			ControlRigBlueprint->SourceCurveImport = Skeleton;
		}

		if (Skeleton)
		{
			TGuardValue<bool> GuardReentry(bIsChangingRigHierarchy, true);

			FScopedTransaction Transaction(LOCTEXT("CurveImport", "Import Curve"));
			ControlRigBlueprint->Modify();
			
			if(URigHierarchyController* Controller = Hierarchy->GetController())
			{
				Controller->ClearSelection();
				Controller->ImportCurves(Skeleton, NAME_None, false, true, true);
			}

			FSlateApplication::Get().DismissAllMenus();
		}
	}

	RefreshCurveList();
	ControlRigBlueprint->PropagateHierarchyFromBPToInstances();
}

URigHierarchy* SRigCurveContainer::GetHierarchy() const
{
	if (ControlRigBlueprint.IsValid())
	{
		return ControlRigBlueprint->Hierarchy;
	}
	return nullptr;
}

URigHierarchy* SRigCurveContainer::GetInstanceHierarchy() const
{
	if (ControlRigEditor.IsValid())
	{
		UControlRig* ControlRig = ControlRigEditor.Pin()->GetInstanceRig();
		if (ControlRig)
		{
			return ControlRig->GetHierarchy();
		}
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
