// Copyright Epic Games, Inc. All Rights Reserved.


#include "SPackageReportDialog.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SButton.h"
#include "Styling/AppStyle.h"
#include "Interfaces/IMainFrameModule.h"

#define LOCTEXT_NAMESPACE "PackageReportDialog"

struct FCompareFPackageReportNodeByName
{
	FORCEINLINE bool operator()( TSharedPtr<FPackageReportNode> A, TSharedPtr<FPackageReportNode> B ) const
	{
		return A->NodeName < B->NodeName;
	}
};

FPackageReportNode::FPackageReportNode()
	: CheckedState(ECheckBoxState::Undetermined)
	, bShouldMigratePackage(nullptr)
	, bIsFolder(false)
	, Parent(nullptr)
{}

FPackageReportNode::FPackageReportNode(const FString& InNodeName, bool InIsFolder)
	: NodeName(InNodeName)
	, CheckedState(ECheckBoxState::Undetermined)
	, bShouldMigratePackage(nullptr)
	, bIsFolder(InIsFolder)
	, Parent(nullptr)
{}

void FPackageReportNode::AddPackage(const FString& PackageName, bool* bInShouldMigratePackage)
{
	TArray<FString> PathElements;
	PackageName.ParseIntoArray(PathElements, TEXT("/"), /*InCullEmpty=*/true);

	(void)AddPackage_Recursive(PathElements, bInShouldMigratePackage);
}

void FPackageReportNode::ExpandChildrenRecursively(const TSharedRef<PackageReportTree>& Treeview)
{
	for ( auto ChildIt = Children.CreateConstIterator(); ChildIt; ++ChildIt )
	{
		Treeview->SetItemExpansion(*ChildIt, (*ChildIt)->CheckedState != ECheckBoxState::Unchecked);
		(*ChildIt)->ExpandChildrenRecursively(Treeview);
	}
}

FPackageReportNode::FChildrenState FPackageReportNode::AddPackage_Recursive(TArray<FString>& PathElements, bool* bInShouldMigratePackage)
{
	FChildrenState ChildrenState;
	ChildrenState.bAnyChildIsChecked = false;
	ChildrenState.bAllChildrenAreChecked = true;

	if ( PathElements.Num() > 0 )
	{
		// Pop the bottom element
		FString ChildNodeName = PathElements[0];
		PathElements.RemoveAt(0);

		// Try to find a child which uses this folder name
		TSharedPtr<FPackageReportNode> Child;
		for ( auto ChildIt = Children.CreateConstIterator(); ChildIt; ++ChildIt )
		{
			if ( (*ChildIt)->NodeName == ChildNodeName )
			{
				Child = (*ChildIt);
				break;
			}
		}

		// If one was not found, create it
		if ( !Child.IsValid() )
		{
			const bool bIsAFolder = (PathElements.Num() > 0);
			int32 ChildIdx = Children.Add( MakeShareable(new FPackageReportNode(ChildNodeName, bIsAFolder)) );
			Child = Children[ChildIdx];
			Child.Get()->Parent = this;
			Children.Sort( FCompareFPackageReportNodeByName() );
		}

		if ( ensure(Child.IsValid()) )
		{
			FChildrenState ChildChildrenState = Child->AddPackage_Recursive(PathElements, bInShouldMigratePackage);
			ChildrenState.bAnyChildIsChecked |= ChildChildrenState.bAnyChildIsChecked;
			ChildrenState.bAllChildrenAreChecked &= ChildChildrenState.bAllChildrenAreChecked;
		}

		CheckedState = ChildrenState.bAllChildrenAreChecked ? ECheckBoxState::Checked : (ChildrenState.bAnyChildIsChecked ? ECheckBoxState::Undetermined : ECheckBoxState::Unchecked);
	}
	else
	{
		CheckedState = *bInShouldMigratePackage ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		ChildrenState.bAnyChildIsChecked = ChildrenState.bAllChildrenAreChecked = CheckedState == ECheckBoxState::Checked;
		bShouldMigratePackage = bInShouldMigratePackage;
	}

	return ChildrenState;
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SPackageReportDialog::Construct( const FArguments& InArgs, const FText& InReportMessage, TArray<ReportPackageData>& InPackageNames, const FOnReportConfirmed& InOnReportConfirmed )
{
	OnReportConfirmed = InOnReportConfirmed;
	FolderOpenBrush = FAppStyle::GetBrush("ContentBrowser.AssetTreeFolderOpen");
	FolderClosedBrush = FAppStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed");
	PackageBrush = FAppStyle::GetBrush("ContentBrowser.ColumnViewAssetIcon");

	ConstructNodeTree(InPackageNames);
	
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FAppStyle::GetBrush("Docking.Tab.ContentAreaBrush") )
		.Padding(FMargin(4, 8, 4, 4))
		[
			SNew(SVerticalBox)

			// Report Message
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(STextBlock)
				.Text(InReportMessage)
				.TextStyle( FAppStyle::Get(), "PackageMigration.DialogTitle" )
			]

			// Tree of packages in the report
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SBorder)
				.BorderImage( FAppStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SAssignNew( ReportTreeView, PackageReportTree )
					.TreeItemsSource(&PackageReportRootNode.Children)
					.ItemHeight(18)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow( this, &SPackageReportDialog::GenerateTreeRow )
					.OnGetChildren( this, &SPackageReportDialog::GetChildrenForTree )
				]
			]

			// Ok/Cancel buttons
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0,4,0,0)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding( FAppStyle::GetMargin("StandardDialog.ContentPadding") )
					.OnClicked(this, &SPackageReportDialog::OkClicked)
					.Text(LOCTEXT("OkButton", "OK"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding( FAppStyle::GetMargin("StandardDialog.ContentPadding") )
					.OnClicked(this, &SPackageReportDialog::CancelClicked)
					.Text(LOCTEXT("CancelButton", "Cancel"))
				]
			]
		]
	];

	if ( ensure(ReportTreeView.IsValid()) )
	{
		PackageReportRootNode.ExpandChildrenRecursively(ReportTreeView.ToSharedRef());
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SPackageReportDialog::OpenPackageReportDialog(const FText& ReportMessage, TArray<ReportPackageData>& PackageNames, const FOnReportConfirmed& InOnReportConfirmed)
{
	TSharedRef<SWindow> ReportWindow = SNew(SWindow)
		.Title(LOCTEXT("ReportWindowTitle", "Asset Report"))
		.ClientSize( FVector2D(600, 500) )
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		[
			SNew(SPackageReportDialog, ReportMessage, PackageNames, InOnReportConfirmed)
		];
		
	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	if ( MainFrameModule.GetParentWindow().IsValid() )
	{
		FSlateApplication::Get().AddWindowAsNativeChild(ReportWindow, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(ReportWindow);
	}
}

void SPackageReportDialog::CloseDialog()
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());

	if ( Window.IsValid() )
	{
		Window->RequestDestroyWindow();
	}
}

TSharedRef<ITableRow> SPackageReportDialog::GenerateTreeRow( TSharedPtr<FPackageReportNode> TreeItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(TreeItem.IsValid());

	const FSlateBrush* IconBrush = GetNodeIcon(TreeItem);

	return SNew( STableRow< TSharedPtr<FPackageReportNode> >, OwnerTable )
		[
			// Icon
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SCheckBox)
				.OnCheckStateChanged(this, &SPackageReportDialog::CheckBoxStateChanged, TreeItem, OwnerTable)
				.IsChecked(this, &SPackageReportDialog::GetEnabledCheckState, TreeItem)
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SImage).Image(IconBrush)
			]
			// Name
			+SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SNew(STextBlock).Text(FText::FromString(TreeItem->NodeName))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		];
}

ECheckBoxState SPackageReportDialog::GetEnabledCheckState(TSharedPtr<FPackageReportNode> TreeItem) const
{
	return TreeItem.Get()->CheckedState;
}

void SPackageReportDialog::SetStateRecursive(TSharedPtr<FPackageReportNode> TreeItem, bool bIsChecked)
{
	if (TreeItem.Get() == nullptr)
	{
		return;
	}

	TreeItem.Get()->CheckedState = bIsChecked ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

	if (TreeItem.Get()->bShouldMigratePackage)
	{
		*(TreeItem.Get()->bShouldMigratePackage) = bIsChecked;
	}

	TArray< TSharedPtr<FPackageReportNode> > Children;
	GetChildrenForTree(TreeItem, Children);
	for (int i = 0; i < Children.Num(); i++)
	{
		if (Children[i].Get() == nullptr)
		{
			continue;
		}

		SetStateRecursive(Children[i], bIsChecked);
	}
}

void SPackageReportDialog::CheckBoxStateChanged(ECheckBoxState InCheckBoxState, TSharedPtr<FPackageReportNode> TreeItem, TSharedRef<STableViewBase> OwnerTable)
{
	SetStateRecursive(TreeItem, InCheckBoxState == ECheckBoxState::Checked);

	FPackageReportNode* CurrentParent = TreeItem->Parent;
	while (CurrentParent != nullptr)
	{
		bool bAnyChildIsChecked = false;
		bool bAllChildrenAreChecked = true;
		for (int i = 0; i < CurrentParent->Children.Num(); i++)
		{
			bAnyChildIsChecked |= CurrentParent->Children[i]->CheckedState != ECheckBoxState::Unchecked;
			bAllChildrenAreChecked &= CurrentParent->Children[i]->CheckedState != ECheckBoxState::Unchecked;
		}

		CurrentParent->CheckedState = bAllChildrenAreChecked ? ECheckBoxState::Checked : (bAnyChildIsChecked ? ECheckBoxState::Undetermined : ECheckBoxState::Unchecked);
		CurrentParent = CurrentParent->Parent;
	}

	OwnerTable.Get().RebuildList();
}

void SPackageReportDialog::GetChildrenForTree( TSharedPtr<FPackageReportNode> TreeItem, TArray< TSharedPtr<FPackageReportNode> >& OutChildren )
{
	OutChildren = TreeItem->Children;
}

void SPackageReportDialog::ConstructNodeTree(TArray<ReportPackageData>& PackageNames)
{
	for (ReportPackageData& Package : PackageNames)
	{
		PackageReportRootNode.AddPackage(Package.Name, &Package.bShouldMigratePackage);
	}
}

const FSlateBrush* SPackageReportDialog::GetNodeIcon(const TSharedPtr<FPackageReportNode>& ReportNode) const
{
	if ( !ReportNode->bIsFolder )
	{
		return PackageBrush;
	}
	else if ( ReportTreeView->IsItemExpanded(ReportNode) )
	{
		return FolderOpenBrush;
	}
	else
	{
		return FolderClosedBrush;
	}
}

FReply SPackageReportDialog::OkClicked()
{
	CloseDialog();
	OnReportConfirmed.ExecuteIfBound();

	return FReply::Handled();
}

FReply SPackageReportDialog::CancelClicked()
{
	CloseDialog();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
