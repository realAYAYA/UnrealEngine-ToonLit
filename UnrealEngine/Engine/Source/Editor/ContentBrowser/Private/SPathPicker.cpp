// Copyright Epic Games, Inc. All Rights Reserved.


#include "SPathPicker.h"

#include "ContentBrowserDataFilter.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserItem.h"
#include "ContentBrowserItemData.h"
#include "ContentBrowserPluginFilters.h"
#include "Delegates/Delegate.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Views/ITypedTableView.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IContentBrowserDataModule.h"
#include "Internationalization/Text.h"
#include "Layout/Children.h"
#include "Misc/Paths.h"
#include "SourcesSearch.h"
#include "SPathView.h"
#include "SSearchToggleButton.h"
#include "Styling/AppStyle.h"
#include "Textures/SlateIcon.h"
#include "UObject/NameTypes.h"

#define LOCTEXT_NAMESPACE "ContentBrowser"

void SPathPicker::Construct( const FArguments& InArgs )
{
	for (auto DelegateIt = InArgs._PathPickerConfig.SetPathsDelegates.CreateConstIterator(); DelegateIt; ++DelegateIt)
	{
		if ((*DelegateIt) != nullptr)
		{
			(**DelegateIt) = FSetPathPickerPathsDelegate::CreateSP(this, &SPathPicker::SetPaths);
		}
	}

	OnPathSelected = InArgs._PathPickerConfig.OnPathSelected;
	OnGetFolderContextMenu = InArgs._PathPickerConfig.OnGetFolderContextMenu;
	OnGetPathContextMenuExtender = InArgs._PathPickerConfig.OnGetPathContextMenuExtender;
	bOnPathSelectedPassesVirtualPaths = InArgs._PathPickerConfig.bOnPathSelectedPassesVirtualPaths;

	ChildSlot
	[
		SAssignNew(PathViewPtr, SPathView)
		.InitialCategoryFilter(EContentBrowserItemCategoryFilter::IncludeAssets) // TODO: Allow this to be wholesale overridden via the picker config
		.OnItemSelectionChanged(this, &SPathPicker::OnItemSelectionChanged) // TODO: Allow this to be wholesale overridden via the picker config
		.OnGetItemContextMenu(this, &SPathPicker::GetItemContextMenu) // TODO: Allow this to be wholesale overridden via the picker config
		.FocusSearchBoxWhenOpened(InArgs._PathPickerConfig.bFocusSearchBoxWhenOpened)
		.AllowContextMenu(InArgs._PathPickerConfig.bAllowContextMenu)
		.AllowClassesFolder(InArgs._PathPickerConfig.bAllowClassesFolder)
		.AllowReadOnlyFolders(InArgs._PathPickerConfig.bAllowReadOnlyFolders)
		.SelectionMode(ESelectionMode::Single)
		.CustomFolderPermissionList(InArgs._PathPickerConfig.CustomFolderPermissionList)
		.ShowFavorites(InArgs._PathPickerConfig.bShowFavorites)
	];

	const FString& DefaultPath = InArgs._PathPickerConfig.DefaultPath;
	if ( !DefaultPath.IsEmpty() && PathViewPtr->InternalPathPassesBlockLists(*DefaultPath))
	{
		const FName VirtualPath = IContentBrowserDataModule::Get().GetSubsystem()->ConvertInternalPathToVirtual(*DefaultPath);
		if (InArgs._PathPickerConfig.bAddDefaultPath && !PathViewPtr->FindTreeItem(VirtualPath))
		{
			const FString DefaultPathLeafName = FPaths::GetPathLeaf(VirtualPath.ToString());
			PathViewPtr->AddFolderItem(FContentBrowserItemData(nullptr, EContentBrowserItemFlags::Type_Folder, VirtualPath, *DefaultPathLeafName, FText(), nullptr), /*bUserNamed*/false);
		}

		PathViewPtr->SetSelectedPaths({ VirtualPath.ToString() });

		if (InArgs._PathPickerConfig.bNotifyDefaultPathSelected)
		{
			if (bOnPathSelectedPassesVirtualPaths)
			{
				OnPathSelected.ExecuteIfBound(VirtualPath.ToString());
			}
			else
			{				
				OnPathSelected.ExecuteIfBound(DefaultPath);
			}
		}
	}
}

void SPathPicker::OnItemSelectionChanged(const FContentBrowserItem& SelectedItem, ESelectInfo::Type SelectInfo)
{
	FName SelectedPackagePath;
	if (SelectedItem.IsFolder())
	{
		if (bOnPathSelectedPassesVirtualPaths)
		{
			OnPathSelected.ExecuteIfBound(SelectedItem.GetVirtualPath().ToString());
		}
		else if (SelectedItem.Legacy_TryGetPackagePath(SelectedPackagePath))
		{
			OnPathSelected.ExecuteIfBound(SelectedPackagePath.ToString());
		}
	}
}

TSharedPtr<SWidget> SPathPicker::GetItemContextMenu(TArrayView<const FContentBrowserItem> SelectedItems)
{
	TArray<FString> SelectedPackagePaths;
	for (const FContentBrowserItem& SelectedItem : SelectedItems)
	{
		SelectedPackagePaths.Add(SelectedItem.GetVirtualPath().ToString());
	}

	if (SelectedPackagePaths.Num() == 0)
	{
		return nullptr;
	}

	FOnCreateNewFolder OnCreateNewFolder = FOnCreateNewFolder::CreateSP(PathViewPtr.Get(), &SPathView::NewFolderItemRequested);

	if (OnGetFolderContextMenu.IsBound())
	{
		return OnGetFolderContextMenu.Execute(SelectedPackagePaths, OnGetPathContextMenuExtender, OnCreateNewFolder);
	}

	return GetFolderContextMenu(SelectedPackagePaths, OnGetPathContextMenuExtender, OnCreateNewFolder);
}

TSharedPtr<SWidget> SPathPicker::GetFolderContextMenu(const TArray<FString>& SelectedPaths, FContentBrowserMenuExtender_SelectedPaths InMenuExtender, FOnCreateNewFolder InOnCreateNewFolder)
{
	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

	TSharedPtr<FExtender> Extender;
	if (InMenuExtender.IsBound())
	{
		// Code using extenders here currently expects internal paths
		Extender = InMenuExtender.Execute(ContentBrowserData->TryConvertVirtualPathsToInternal(SelectedPaths));
	}

	const bool bInShouldCloseWindowAfterSelection = true;
	const bool bCloseSelfOnly = true;
	FMenuBuilder MenuBuilder(bInShouldCloseWindowAfterSelection, nullptr, Extender, bCloseSelfOnly);

	// We can only create folders when we have a single path selected
	const bool bCanCreateNewFolder = SelectedPaths.Num() == 1 && ContentBrowserData->CanCreateFolder(*SelectedPaths[0], nullptr);

	FText NewFolderToolTip;
	if(SelectedPaths.Num() == 1)
	{
		if(bCanCreateNewFolder)
		{
			NewFolderToolTip = FText::Format(LOCTEXT("NewFolderTooltip_CreateIn", "Create a new folder in {0}."), FText::FromString(SelectedPaths[0]));
		}
		else
		{
			NewFolderToolTip = FText::Format(LOCTEXT("NewFolderTooltip_InvalidPath", "Cannot create new folders in {0}."), FText::FromString(SelectedPaths[0]));
		}
	}
	else
	{
		NewFolderToolTip = LOCTEXT("NewFolderTooltip_InvalidNumberOfPaths", "Can only create folders when there is a single path selected.");
	}

	// New Folder
	MenuBuilder.AddMenuEntry(
		LOCTEXT("NewFolder", "New Folder"),
		NewFolderToolTip,
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "ContentBrowser.NewFolderIcon"),
		FUIAction(
			FExecuteAction::CreateSP(this, &SPathPicker::CreateNewFolder, SelectedPaths.Num() > 0 ? SelectedPaths[0] : FString(), InOnCreateNewFolder),
			FCanExecuteAction::CreateLambda( [bCanCreateNewFolder] { return bCanCreateNewFolder; } )
			),
		"FolderContext"
		);

	return MenuBuilder.MakeWidget();
}

void SPathPicker::ExecuteRenameFolder()
{
	if (PathViewPtr.IsValid())
	{
		const TArray<FContentBrowserItem> SelectedItems = PathViewPtr->GetSelectedFolderItems();
		if (SelectedItems.Num() == 1)
		{
			PathViewPtr->RenameFolderItem(SelectedItems[0]);
		}
	}
}

void SPathPicker::ExecuteAddFolder()
{
	if (PathViewPtr.IsValid())
	{
		const TArray<FString> SelectedItems = PathViewPtr->GetSelectedPaths();
		if (SelectedItems.Num() == 1)
		{
			FOnCreateNewFolder OnCreateNewFolder = FOnCreateNewFolder::CreateSP(PathViewPtr.Get(), &SPathView::NewFolderItemRequested);
			CreateNewFolder(SelectedItems[0], OnCreateNewFolder);
		}
	}
}

void SPathPicker::CreateNewFolder(FString FolderPath, FOnCreateNewFolder InOnCreateNewFolder)
{
	const FText DefaultFolderBaseName = LOCTEXT("DefaultFolderName", "NewFolder");
	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

	// Create a valid base name for this folder
	FString DefaultFolderName = DefaultFolderBaseName.ToString();
	int32 NewFolderPostfix = 0;
	FName CombinedPathName;
	for (;;)
	{
		FString CombinedPathNameStr = FolderPath / DefaultFolderName;
		if (NewFolderPostfix > 0)
		{
			CombinedPathNameStr.AppendInt(NewFolderPostfix);
		}
		++NewFolderPostfix;

		CombinedPathName = *CombinedPathNameStr;

		const FContentBrowserItem ExistingFolder = ContentBrowserData->GetItemAtPath(CombinedPathName, EContentBrowserItemTypeFilter::IncludeFolders);
		if (!ExistingFolder.IsValid())
		{
			break;
		}
	}

	const FContentBrowserItemTemporaryContext NewFolderItem = ContentBrowserData->CreateFolder(CombinedPathName);
	if (NewFolderItem.IsValid())
	{
		InOnCreateNewFolder.ExecuteIfBound(NewFolderItem);
	}
}

void SPathPicker::RefreshPathView()
{
	PathViewPtr->Populate(true);
}

void SPathPicker::SetPaths(const TArray<FString>& NewPaths)
{
	PathViewPtr->SetSelectedPaths(NewPaths);
}

TArray<FString> SPathPicker::GetPaths() const
{
	return PathViewPtr->GetSelectedPaths();
}

const TSharedPtr<SPathView>& SPathPicker::GetPathView() const
{
	return PathViewPtr;
}

#undef LOCTEXT_NAMESPACE
