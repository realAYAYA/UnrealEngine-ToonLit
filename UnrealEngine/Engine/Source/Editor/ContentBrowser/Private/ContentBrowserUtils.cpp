// Copyright Epic Games, Inc. All Rights Reserved.


#include "ContentBrowserUtils.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetData.h"
#include "Containers/Map.h"
#include "Containers/StringView.h"
#include "ContentBrowserConfig.h"
#include "ContentBrowserDataFilter.h"
#include "ContentBrowserDataSource.h"
#include "ContentBrowserDataSubsystem.h"
#include "ContentBrowserItem.h"
#include "ContentBrowserItemData.h"
#include "ContentBrowserSingleton.h"
#include "CoreGlobals.h"
#include "Framework/Application/IMenu.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "IContentBrowserDataModule.h"
#include "Input/Reply.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Layout/SlateRect.h"
#include "Layout/WidgetPath.h"
#include "Math/Color.h"
#include "Math/UnrealMathSSE.h"
#include "Math/Vector2D.h"
#include "Misc/AssertionMacros.h"
#include "Misc/Attribute.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Misc/NamePermissionList.h"
#include "Misc/Optional.h"
#include "Misc/Paths.h"
#include "SAssetView.h"
#include "SPathView.h"
#include "SlateOptMacros.h"
#include "SlotBase.h"
#include "SourceControlOperations.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Templates/UnrealTemplate.h"
#include "Types/SlateEnums.h"
#include "UObject/TopLevelAssetPath.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Text/STextBlock.h"
#include "Algo/Transform.h"

class SWidget;
struct FGeometry;
struct FPointerEvent;

#define LOCTEXT_NAMESPACE "ContentBrowser"

namespace ContentBrowserUtils
{
	/** Converts a virtual path such as /All/Plugins -> /Plugins or /All/Game -> /Game */
	FString ConvertVirtualPathToInvariantPathString(const FString& VirtualPath)
	{
		FName ConvertedPath;
		IContentBrowserDataModule::Get().GetSubsystem()->TryConvertVirtualPath(FName(VirtualPath), ConvertedPath);
		return ConvertedPath.ToString();
	}
}

class SContentBrowserPopup : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SContentBrowserPopup ){}

		SLATE_ATTRIBUTE( FText, Message )

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs )
	{
		ChildSlot
		[
			SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Menu.Background"))
			.Padding(10.f)
			.OnMouseButtonDown(this, &SContentBrowserPopup::OnBorderClicked)
			.BorderBackgroundColor(this, &SContentBrowserPopup::GetBorderBackgroundColor)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.f, 0.f, 4.f, 0.f)
				[
					SNew(SImage) .Image( FAppStyle::GetBrush("ContentBrowser.PopupMessageIcon") )
				]

				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(InArgs._Message)
					.WrapTextAt(450)
				]
			]
		];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	static void DisplayMessage( const FText& Message, const FSlateRect& ScreenAnchor, TSharedRef<SWidget> ParentContent )
	{
		TSharedRef<SContentBrowserPopup> PopupContent = SNew(SContentBrowserPopup) .Message(Message);

		const FVector2D ScreenLocation = FVector2D(ScreenAnchor.Left, ScreenAnchor.Top);
		const bool bFocusImmediately = true;
		const FVector2D SummonLocationSize = ScreenAnchor.GetSize();

		TSharedPtr<IMenu> Menu = FSlateApplication::Get().PushMenu(
			ParentContent,
			FWidgetPath(),
			PopupContent,
			ScreenLocation,
			FPopupTransitionEffect( FPopupTransitionEffect::TopMenu ),
			bFocusImmediately,
			SummonLocationSize);

		PopupContent->SetMenu(Menu);
	}

private:
	void SetMenu(const TSharedPtr<IMenu>& InMenu)
	{
		Menu = InMenu;
	}

	FReply OnBorderClicked(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
	{
		if (Menu.IsValid())
		{
			Menu.Pin()->Dismiss();
		}

		return FReply::Handled();
	}

	FSlateColor GetBorderBackgroundColor() const
	{
		return IsHovered() ? FLinearColor(0.5, 0.5, 0.5, 1) : FLinearColor::White;
	}

private:
	TWeakPtr<IMenu> Menu;
};

/** A miniture confirmation popup for quick yes/no questions */
class SContentBrowserConfirmPopup :  public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SContentBrowserConfirmPopup ) {}
			
		/** The text to display */
		SLATE_ARGUMENT(FText, Prompt)

		/** The Yes Button to display */
		SLATE_ARGUMENT(FText, YesText)

		/** The No Button to display */
		SLATE_ARGUMENT(FText, NoText)

		/** Invoked when yes is clicked */
		SLATE_EVENT(FOnClicked, OnYesClicked)

		/** Invoked when no is clicked */
		SLATE_EVENT(FOnClicked, OnNoClicked)

	SLATE_END_ARGS()

	BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
	void Construct( const FArguments& InArgs )
	{
		OnYesClicked = InArgs._OnYesClicked;
		OnNoClicked = InArgs._OnNoClicked;

		ChildSlot
		[
			SNew(SBorder)
			. BorderImage(FAppStyle::GetBrush("Menu.Background"))
			. Padding(10.f)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 0.f, 0.f, 5.f)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
						.Text(InArgs._Prompt)
				]

				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(3.f)
					+ SUniformGridPanel::Slot(0, 0)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(InArgs._YesText)
						.OnClicked( this, &SContentBrowserConfirmPopup::YesClicked )
					]

					+ SUniformGridPanel::Slot(1, 0)
					.HAlign(HAlign_Fill)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.Text(InArgs._NoText)
						.OnClicked( this, &SContentBrowserConfirmPopup::NoClicked )
					]
				]
			]
		];
	}
	END_SLATE_FUNCTION_BUILD_OPTIMIZATION

	/** Opens the popup using the specified component as its parent */
	void OpenPopup(const TSharedRef<SWidget>& ParentContent)
	{
		// Show dialog to confirm the delete
		Menu = FSlateApplication::Get().PushMenu(
			ParentContent,
			FWidgetPath(),
			SharedThis(this),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect( FPopupTransitionEffect::TopMenu )
			);
	}

private:
	/** The yes button was clicked */
	FReply YesClicked()
	{
		if ( OnYesClicked.IsBound() )
		{
			OnYesClicked.Execute();
		}

		if (Menu.IsValid())
		{
			Menu.Pin()->Dismiss();
		}

		return FReply::Handled();
	}

	/** The no button was clicked */
	FReply NoClicked()
	{
		if ( OnNoClicked.IsBound() )
		{
			OnNoClicked.Execute();
		}

		if (Menu.IsValid())
		{
			Menu.Pin()->Dismiss();
		}

		return FReply::Handled();
	}

	/** The IMenu prepresenting this popup */
	TWeakPtr<IMenu> Menu;

	/** Delegates for button clicks */
	FOnClicked OnYesClicked;
	FOnClicked OnNoClicked;
};


void ContentBrowserUtils::DisplayMessage(const FText& Message, const FSlateRect& ScreenAnchor, const TSharedRef<SWidget>& ParentContent)
{
	SContentBrowserPopup::DisplayMessage(Message, ScreenAnchor, ParentContent);
}

void ContentBrowserUtils::DisplayConfirmationPopup(const FText& Message, const FText& YesString, const FText& NoString, const TSharedRef<SWidget>& ParentContent, const FOnClicked& OnYesClicked, const FOnClicked& OnNoClicked)
{
	TSharedRef<SContentBrowserConfirmPopup> Popup = 
		SNew(SContentBrowserConfirmPopup)
		.Prompt(Message)
		.YesText(YesString)
		.NoText(NoString)
		.OnYesClicked( OnYesClicked )
		.OnNoClicked( OnNoClicked );

	Popup->OpenPopup(ParentContent);
}

FString ContentBrowserUtils::GetItemReferencesText(const TArray<FContentBrowserItem>& Items)
{
	TArray<FContentBrowserItem> SortedItems = Items;
	SortedItems.Sort([](const FContentBrowserItem& One, const FContentBrowserItem& Two)
	{
		return One.GetVirtualPath().Compare(Two.GetVirtualPath()) < 0;
	});

	FString Result;
	for (const FContentBrowserItem& Item : SortedItems)
	{
		if (ensure(!Item.IsFolder()))
		{
			Item.AppendItemReference(Result);
		}
	}

	return Result;
}

FString ContentBrowserUtils::GetFolderReferencesText(const TArray<FContentBrowserItem>& Folders)
{
	TArray<FContentBrowserItem> SortedItems = Folders;
	SortedItems.Sort([](const FContentBrowserItem& One, const FContentBrowserItem& Two)
	{
		return One.GetVirtualPath().Compare(Two.GetVirtualPath()) < 0;
	});

	TStringBuilder<2048> Result;
	for (const FContentBrowserItem& Item : SortedItems)
	{
		if (ensure(Item.IsFolder()))
		{
			FName InternalPath = Item.GetInternalPath();
			if (!InternalPath.IsNone())
			{
				Result << InternalPath << LINE_TERMINATOR;
			}
		}
	}

	return Result.ToString();
}

void ContentBrowserUtils::CopyItemReferencesToClipboard(const TArray<FContentBrowserItem>& ItemsToCopy)
{
	FString Text = GetItemReferencesText(ItemsToCopy);
	if (!Text.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*Text);
	}
}

void ContentBrowserUtils::CopyFolderReferencesToClipboard(const TArray<FContentBrowserItem>& FoldersToCopy)
{
	FString Text = GetFolderReferencesText(FoldersToCopy);
	if (!Text.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*Text);
	}
}

void ContentBrowserUtils::CopyFilePathsToClipboard(const TArray<FContentBrowserItem>& ItemsToCopy)
{
	TArray<FContentBrowserItem> SortedItems = ItemsToCopy;
	SortedItems.Sort([](const FContentBrowserItem& One, const FContentBrowserItem& Two)
	{
		return One.GetVirtualPath().Compare(Two.GetVirtualPath()) < 0;
	});

	FString ClipboardText;
	for (const FContentBrowserItem& Item : SortedItems)
	{
		if (ClipboardText.Len() > 0)
		{
			ClipboardText += LINE_TERMINATOR;
		}

		FString ItemFilename;
		if (Item.GetItemPhysicalPath(ItemFilename) && FPaths::FileExists(ItemFilename))
		{
			ClipboardText += FPaths::ConvertRelativePathToFull(ItemFilename);
		}
		else
		{
			// Add a message for when a user tries to copy the path to a file that doesn't exist on disk of the form
			// <ItemName>: No file on disk
			ClipboardText += FString::Printf(TEXT("%s: No file on disk"), *Item.GetDisplayName().ToString());
		}
	}

	FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);
}

bool ContentBrowserUtils::IsItemDeveloperContent(const FContentBrowserItem& InItem)
{
	const FContentBrowserItemDataAttributeValue IsDeveloperAttributeValue = InItem.GetItemAttribute(ContentBrowserItemAttributes::ItemIsDeveloperContent);
	return IsDeveloperAttributeValue.IsValid() && IsDeveloperAttributeValue.GetValue<bool>();
}

bool ContentBrowserUtils::IsItemLocalizedContent(const FContentBrowserItem& InItem)
{
	const FContentBrowserItemDataAttributeValue IsLocalizedAttributeValue = InItem.GetItemAttribute(ContentBrowserItemAttributes::ItemIsLocalizedContent);
	return IsLocalizedAttributeValue.IsValid() && IsLocalizedAttributeValue.GetValue<bool>();
}

bool ContentBrowserUtils::IsItemEngineContent(const FContentBrowserItem& InItem)
{
	const FContentBrowserItemDataAttributeValue IsEngineAttributeValue = InItem.GetItemAttribute(ContentBrowserItemAttributes::ItemIsEngineContent);
	return IsEngineAttributeValue.IsValid() && IsEngineAttributeValue.GetValue<bool>();
}

bool ContentBrowserUtils::IsItemProjectContent(const FContentBrowserItem& InItem)
{
	const FContentBrowserItemDataAttributeValue IsProjectAttributeValue = InItem.GetItemAttribute(ContentBrowserItemAttributes::ItemIsProjectContent);
	return IsProjectAttributeValue.IsValid() && IsProjectAttributeValue.GetValue<bool>();
}

bool ContentBrowserUtils::IsItemPluginContent(const FContentBrowserItem& InItem)
{
	const FContentBrowserItemDataAttributeValue IsPluginAttributeValue = InItem.GetItemAttribute(ContentBrowserItemAttributes::ItemIsPluginContent);
	return IsPluginAttributeValue.IsValid() && IsPluginAttributeValue.GetValue<bool>();
}

bool ContentBrowserUtils::IsItemPluginRootFolder(const FContentBrowserItem& InItem)
{
	if (!InItem.IsFolder())
	{
		return false;
	}
	FName InternalPath = InItem.GetInternalPath();
	if (InternalPath.IsNone())
	{
		return false;
	}
	FNameBuilder PathBuffer(InternalPath);
	FStringView Path = PathBuffer.ToView();
	if (int32 Index = 0; Path.RightChop(1).FindChar('/', Index) && Index != INDEX_NONE)
	{
		return false; // Contains a second slash, is not a root
	}
	return IsItemPluginContent(InItem);
}

bool ContentBrowserUtils::IsCollectionPath(const FString& InPath, FName* OutCollectionName, ECollectionShareType::Type* OutCollectionShareType)
{
	static const FString CollectionsRootPrefix = TEXT("/Collections");
	if (InPath.StartsWith(CollectionsRootPrefix))
	{
		TArray<FString> PathParts;
		InPath.ParseIntoArray(PathParts, TEXT("/"));
		check(PathParts.Num() > 2);

		// The second part of the path is the share type name
		if (OutCollectionShareType)
		{
			*OutCollectionShareType = ECollectionShareType::FromString(*PathParts[1]);
		}

		// The third part of the path is the collection name
		if (OutCollectionName)
		{
			*OutCollectionName = FName(*PathParts[2]);
		}

		return true;
	}
	return false;
}

void ContentBrowserUtils::CountPathTypes(const TArray<FString>& InPaths, int32& OutNumAssetPaths, int32& OutNumClassPaths)
{
	static const FString ClassesRootPrefix = TEXT("/Classes_");

	OutNumAssetPaths = 0;
	OutNumClassPaths = 0;

	for(const FString& Path : InPaths)
	{
		if(Path.StartsWith(ClassesRootPrefix))
		{
			++OutNumClassPaths;
		}
		else
		{
			++OutNumAssetPaths;
		}
	}
}

void ContentBrowserUtils::CountPathTypes(const TArray<FName>& InPaths, int32& OutNumAssetPaths, int32& OutNumClassPaths)
{
	static const FString ClassesRootPrefix = TEXT("/Classes_");

	OutNumAssetPaths = 0;
	OutNumClassPaths = 0;

	for(const FName& Path : InPaths)
	{
		if(Path.ToString().StartsWith(ClassesRootPrefix))
		{
			++OutNumClassPaths;
		}
		else
		{
			++OutNumAssetPaths;
		}
	}
}

void ContentBrowserUtils::CountItemTypes(const TArray<FAssetData>& InItems, int32& OutNumAssetItems, int32& OutNumClassItems)
{
	OutNumAssetItems = 0;
	OutNumClassItems = 0;

	const FTopLevelAssetPath ClassPath(TEXT("/Script/CoreUObject"), TEXT("Class"));
	for(const FAssetData& Item : InItems)
	{
		if(Item.AssetClassPath == ClassPath)
		{
			++OutNumClassItems;
		}
		else
		{
			++OutNumAssetItems;
		}
	}
}

FText ContentBrowserUtils::GetExploreFolderText()
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("FileManagerName"), FPlatformMisc::GetFileManagerName());
	return FText::Format(NSLOCTEXT("GenericPlatform", "ShowInFileManager", "Show in {FileManagerName}"), Args);
}

void ContentBrowserUtils::ExploreFolders(const TArray<FContentBrowserItem>& InItems, const TSharedRef<SWidget>& InParentContent)
{
	TArray<FString> ExploreItems;

	for (const FContentBrowserItem& SelectedItem : InItems)
	{
		FString ItemFilename;
		if (SelectedItem.GetItemPhysicalPath(ItemFilename))
		{
			const bool bExists = SelectedItem.IsFile() ? FPaths::FileExists(ItemFilename) : FPaths::DirectoryExists(ItemFilename);
			if (bExists)
			{
				ExploreItems.Add(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*ItemFilename));
			}
		}
	}

	const int32 BatchSize = 10;
	const FText FileManagerName = FPlatformMisc::GetFileManagerName();
	const bool bHasMultipleBatches = ExploreItems.Num() > BatchSize;
	for (int32 i = 0; i < ExploreItems.Num(); ++i)
	{
		bool bIsBatchBoundary = (i % BatchSize) == 0;
		if (bHasMultipleBatches && bIsBatchBoundary)
		{
			int32 RemainingCount = ExploreItems.Num() - i;
			int32 NextCount = FMath::Min(BatchSize, RemainingCount);
			FText Prompt = FText::Format(LOCTEXT("ExecuteExploreConfirm", "Show {0} {0}|plural(one=item,other=items) in {1}?\nThere {2}|plural(one=is,other=are) {2} remaining."), NextCount, FileManagerName, RemainingCount);
			if (FMessageDialog::Open(EAppMsgType::YesNo, Prompt) != EAppReturnType::Yes)
			{
				return;
			}
		}

		FPlatformProcess::ExploreFolder(*ExploreItems[i]);
	}
}

bool ContentBrowserUtils::CanExploreFolders(const TArray<FContentBrowserItem>& InItems)
{
	for (const FContentBrowserItem& SelectedItem : InItems)
	{
		FString ItemFilename;
		if (SelectedItem.GetItemPhysicalPath(ItemFilename))
		{
			const bool bExists = SelectedItem.IsFile() ? FPaths::FileExists(ItemFilename) : FPaths::DirectoryExists(ItemFilename);
			if (bExists)
			{
				return true;
			}
		}
	}

	return false;
}

template <typename OutputContainerType>
void ConvertLegacySelectionToVirtualPathsImpl(TArrayView<const FAssetData> InAssets, TArrayView<const FString> InFolders, const bool InUseFolderPaths, OutputContainerType& OutVirtualPaths)
{
	OutVirtualPaths.Reset();
	if (InAssets.Num() == 0 && InFolders.Num() == 0)
	{
		return;
	}

	UContentBrowserDataSubsystem* ContentBrowserData = IContentBrowserDataModule::Get().GetSubsystem();

	auto AppendVirtualPath = [&OutVirtualPaths](FName InPath)
	{
		OutVirtualPaths.Add(InPath);
		return true;
	};

	for (const FAssetData& Asset : InAssets)
	{
		ContentBrowserData->Legacy_TryConvertAssetDataToVirtualPaths(Asset, InUseFolderPaths, AppendVirtualPath);
	}

	for (const FString& Folder : InFolders)
	{
		ContentBrowserData->Legacy_TryConvertPackagePathToVirtualPaths(*Folder, AppendVirtualPath);
	}
}

void ContentBrowserUtils::ConvertLegacySelectionToVirtualPaths(TArrayView<const FAssetData> InAssets, TArrayView<const FString> InFolders, const bool InUseFolderPaths, TArray<FName>& OutVirtualPaths)
{
	ConvertLegacySelectionToVirtualPathsImpl(InAssets, InFolders, InUseFolderPaths, OutVirtualPaths);
}

void ContentBrowserUtils::ConvertLegacySelectionToVirtualPaths(TArrayView<const FAssetData> InAssets, TArrayView<const FString> InFolders, const bool InUseFolderPaths, TSet<FName>& OutVirtualPaths)
{
	ConvertLegacySelectionToVirtualPathsImpl(InAssets, InFolders, InUseFolderPaths, OutVirtualPaths);
}

void ContentBrowserUtils::AppendAssetFilterToContentBrowserFilter(const FARFilter& InAssetFilter, const TSharedPtr<FPathPermissionList>& InAssetClassPermissionList, const TSharedPtr<FPathPermissionList>& InFolderPermissionList, FContentBrowserDataFilter& OutDataFilter)
{
	if (InAssetFilter.SoftObjectPaths.Num() > 0 || InAssetFilter.TagsAndValues.Num() > 0 || InAssetFilter.bIncludeOnlyOnDiskAssets)
	{
		FContentBrowserDataObjectFilter& ObjectFilter = OutDataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataObjectFilter>();
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		// TODO: Modify this API to also use FSoftObjectPath with deprecation
		ObjectFilter.ObjectNamesToInclude = UE::SoftObjectPath::Private::ConvertSoftObjectPaths(InAssetFilter.SoftObjectPaths);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		ObjectFilter.TagsAndValuesToInclude = InAssetFilter.TagsAndValues;
		ObjectFilter.bOnDiskObjectsOnly = InAssetFilter.bIncludeOnlyOnDiskAssets;
	}

	if (InAssetFilter.PackageNames.Num() > 0 || InAssetFilter.PackagePaths.Num() > 0 || (InFolderPermissionList && InFolderPermissionList->HasFiltering()))
	{
		FContentBrowserDataPackageFilter& PackageFilter = OutDataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataPackageFilter>();
		PackageFilter.PackageNamesToInclude = InAssetFilter.PackageNames;
		PackageFilter.PackagePathsToInclude = InAssetFilter.PackagePaths;
		PackageFilter.bRecursivePackagePathsToInclude = InAssetFilter.bRecursivePaths;
		PackageFilter.PathPermissionList = InFolderPermissionList;
	}

	if (InAssetFilter.ClassPaths.Num() > 0 || (InAssetClassPermissionList && InAssetClassPermissionList->HasFiltering()))
	{
		FContentBrowserDataClassFilter& ClassFilter = OutDataFilter.ExtraFilters.FindOrAddFilter<FContentBrowserDataClassFilter>();
		for (FTopLevelAssetPath ClassPathName : InAssetFilter.ClassPaths)
		{
			ClassFilter.ClassNamesToInclude.Add(ClassPathName.ToString());
		}
		ClassFilter.bRecursiveClassNamesToInclude = InAssetFilter.bRecursiveClasses;
		if (InAssetFilter.bRecursiveClasses)
		{
			for (FTopLevelAssetPath ClassPathName : InAssetFilter.RecursiveClassPathsExclusionSet)
			{
				ClassFilter.ClassNamesToExclude.Add(ClassPathName.ToString());
			}
			ClassFilter.bRecursiveClassNamesToExclude = false;
		}
		ClassFilter.ClassPermissionList = InAssetClassPermissionList;
	}
}

TSharedPtr<FPathPermissionList> ContentBrowserUtils::GetCombinedFolderPermissionList(const TSharedPtr<FPathPermissionList>& FolderPermissionList, const TSharedPtr<FPathPermissionList>& WritableFolderPermissionList)
{
	TSharedPtr<FPathPermissionList> CombinedFolderPermissionList;

	const bool bHidingFolders = FolderPermissionList && FolderPermissionList->HasFiltering();
	const bool bHidingReadOnlyFolders = WritableFolderPermissionList && WritableFolderPermissionList->HasFiltering();
	if (bHidingFolders || bHidingReadOnlyFolders)
	{
		CombinedFolderPermissionList = MakeShared<FPathPermissionList>();

		if (bHidingReadOnlyFolders && bHidingFolders)
		{
			FPathPermissionList IntersectedFilter = FolderPermissionList->CombinePathFilters(*WritableFolderPermissionList.Get());
			CombinedFolderPermissionList->Append(IntersectedFilter);
		}
		else if (bHidingReadOnlyFolders)
		{
			CombinedFolderPermissionList->Append(*WritableFolderPermissionList);
		}
		else if (bHidingFolders)
		{
			CombinedFolderPermissionList->Append(*FolderPermissionList);
		}
	}

	return CombinedFolderPermissionList;
}

bool ContentBrowserUtils::CanDeleteFromAssetView(TWeakPtr<SAssetView> AssetView, FText* OutErrorMsg)
{
	if (TSharedPtr<SAssetView> AssetViewPin = AssetView.Pin())
	{
		const TArray<FContentBrowserItem> SelectedItems = AssetViewPin->GetSelectedItems();

		bool bCanDelete = false;
		for (const FContentBrowserItem& SelectedItem : SelectedItems)
		{
			bCanDelete |= SelectedItem.CanDelete(OutErrorMsg);
		}
		return bCanDelete;
	}
	return false;
}

bool ContentBrowserUtils::CanRenameFromAssetView(TWeakPtr<SAssetView> AssetView, FText* OutErrorMsg)
{
	if (TSharedPtr<SAssetView> AssetViewPin = AssetView.Pin())
	{
		const TArray<FContentBrowserItem> SelectedItems = AssetViewPin->GetSelectedItems();
		return SelectedItems.Num() == 1 && SelectedItems[0].CanRename(nullptr, OutErrorMsg) && !AssetViewPin->IsThumbnailEditMode();
	}
	return false;
}

bool ContentBrowserUtils::CanDeleteFromPathView(TWeakPtr<SPathView> PathView, FText* OutErrorMsg)
{
	if (TSharedPtr<SPathView> PathViewPin = PathView.Pin())
	{
		const TArray<FContentBrowserItem> SelectedItems = PathViewPin->GetSelectedFolderItems();

		bool bCanDelete = false;
		for (const FContentBrowserItem& SelectedItem : SelectedItems)
		{
			bCanDelete |= SelectedItem.CanDelete(OutErrorMsg);
		}
		return bCanDelete;
	}
	return false;
}

bool ContentBrowserUtils::CanRenameFromPathView(TWeakPtr<SPathView> PathView, FText* OutErrorMsg)
{
	if (TSharedPtr<SPathView> PathViewPin = PathView.Pin())
	{
		const TArray<FContentBrowserItem> SelectedItems = PathViewPin->GetSelectedFolderItems();
		return SelectedItems.Num() == 1 && SelectedItems[0].CanRename(nullptr, OutErrorMsg);
	}
	return false;
}

FName ContentBrowserUtils::GetInvariantPath(const FContentBrowserItemPath& ItemPath)
{
	if (!ItemPath.HasInternalPath())
	{
		FName InvariantPath;
		const EContentBrowserPathType AssetPathType = IContentBrowserDataModule::Get().GetSubsystem()->TryConvertVirtualPath(ItemPath.GetVirtualPathName(), InvariantPath);
		if (AssetPathType == EContentBrowserPathType::Virtual)
		{
			return InvariantPath;
		}
		else
		{
			return NAME_None;
		}
	}

	return ItemPath.GetInternalPathName();
}

EContentBrowserIsFolderVisibleFlags ContentBrowserUtils::GetIsFolderVisibleFlags(const bool bDisplayEmpty)
{
	return EContentBrowserIsFolderVisibleFlags::Default | (bDisplayEmpty ? EContentBrowserIsFolderVisibleFlags::None : EContentBrowserIsFolderVisibleFlags::HideEmptyFolders);
}

bool ContentBrowserUtils::IsFavoriteFolder(const FString& FolderPath)
{
	return IsFavoriteFolder(FContentBrowserItemPath(FolderPath, EContentBrowserPathType::Virtual));
}

bool ContentBrowserUtils::IsFavoriteFolder(const FContentBrowserItemPath& FolderPath)
{
	const FName InvariantPath = ContentBrowserUtils::GetInvariantPath(FolderPath);
	if (!InvariantPath.IsNone())
	{
		return FContentBrowserSingleton::Get().FavoriteFolderPaths.Contains(InvariantPath.ToString());
	}

	return false;
}

void ContentBrowserUtils::AddFavoriteFolder(const FString& FolderPath, bool bFlushConfig /*= true*/)
{
	AddFavoriteFolder(FContentBrowserItemPath(FolderPath, EContentBrowserPathType::Virtual));
}

void ContentBrowserUtils::AddFavoriteFolder(const FContentBrowserItemPath& FolderPath)
{
	const FName InvariantPath = ContentBrowserUtils::GetInvariantPath(FolderPath);
	if (InvariantPath.IsNone())
	{
		return;
	}

	const FString InvariantFolder = InvariantPath.ToString();

	FContentBrowserSingleton::Get().FavoriteFolderPaths.AddUnique(InvariantFolder);

	if (UContentBrowserConfig* EditorConfig = UContentBrowserConfig::Get())
	{
		EditorConfig->Favorites.Add(InvariantFolder);

		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	FContentBrowserSingleton::Get().BroadcastFavoritesChanged();
}

void ContentBrowserUtils::RemoveFavoriteFolder(const FContentBrowserItemPath& FolderPath)
{
	const FName InvariantPath = ContentBrowserUtils::GetInvariantPath(FolderPath);
	if (InvariantPath.IsNone())
	{
		return;
	}

	FString InvariantFolder = InvariantPath.ToString();

	FContentBrowserSingleton::Get().FavoriteFolderPaths.Remove(InvariantFolder);

	if (UContentBrowserConfig* EditorConfig = UContentBrowserConfig::Get())
	{
		EditorConfig->Favorites.Remove(InvariantFolder);

		UContentBrowserConfig::Get()->SaveEditorConfig();
	}

	FContentBrowserSingleton::Get().BroadcastFavoritesChanged();
}

void ContentBrowserUtils::RemoveFavoriteFolder(const FString& FolderPath, bool bFlushConfig)
{
	RemoveFavoriteFolder(FContentBrowserItemPath(FolderPath, EContentBrowserPathType::Virtual));
}

const TArray<FString>& ContentBrowserUtils::GetFavoriteFolders()
{
	return FContentBrowserSingleton::Get().FavoriteFolderPaths;
}

void ContentBrowserUtils::AddShowPrivateContentFolder(const FStringView VirtualFolderPath, const FName Owner)
{
	FContentBrowserSingleton& ContentBrowserSingleton = FContentBrowserSingleton::Get();

	if (!ContentBrowserSingleton.IsFolderShowPrivateContentToggleable(VirtualFolderPath))
	{
		return;
	}

	FName InvariantPath;
	IContentBrowserDataModule::Get().GetSubsystem()->TryConvertVirtualPath(VirtualFolderPath, InvariantPath);

	const TSharedPtr<FPathPermissionList>& ShowPrivateContentPermissionList = ContentBrowserSingleton.GetShowPrivateContentPermissionList();

	ShowPrivateContentPermissionList->AddAllowListItem(Owner, InvariantPath);

	ContentBrowserSingleton.SetPrivateContentPermissionListDirty();
}

void ContentBrowserUtils::RemoveShowPrivateContentFolder(const FStringView VirtualFolderPath, const FName Owner)
{
	FContentBrowserSingleton& ContentBrowserSingleton = FContentBrowserSingleton::Get();

	if (!ContentBrowserSingleton.IsFolderShowPrivateContentToggleable(VirtualFolderPath))
	{
		return;
	}

	FName InvariantPath;
	IContentBrowserDataModule::Get().GetSubsystem()->TryConvertVirtualPath(VirtualFolderPath, InvariantPath);

	const TSharedPtr<FPathPermissionList>& ShowPrivateContentPermissionList = ContentBrowserSingleton.GetShowPrivateContentPermissionList();

	ShowPrivateContentPermissionList->RemoveAllowListItem(Owner, InvariantPath);

	ContentBrowserSingleton.SetPrivateContentPermissionListDirty();
}

FAutoConsoleVariable CVarShowCustomVirtualFolderIcon(
	TEXT("ContentBrowser.ShowCustomVirtualFolderIcon"),
	1,
	TEXT("Whether to show a special icon for custom virtual folders added for organizational purposes in the content browser. E.g. EditorCustomVirtualPath field in plugins"));

bool ContentBrowserUtils::ShouldShowCustomVirtualFolderIcon()
{
	return CVarShowCustomVirtualFolderIcon->GetBool();
}

FAutoConsoleVariable CVarShowPluginFolderIcon(
	TEXT("ContentBrowser.ShowPluginFolderIcon"),
	1,
	TEXT("Whether to show a special icon for plugin folders in the content browser."));

bool ContentBrowserUtils::ShouldShowPluginFolderIcon()
{
	return CVarShowPluginFolderIcon->GetBool();
}

#undef LOCTEXT_NAMESPACE
