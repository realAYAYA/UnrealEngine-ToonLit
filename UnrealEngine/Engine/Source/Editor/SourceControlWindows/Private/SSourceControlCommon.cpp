// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSourceControlCommon.h"

#include "Algo/Count.h"
#include "Algo/Find.h"
#include "Algo/Replace.h"
#include "AssetRegistry/AssetData.h"
#include "ActorFolder.h"
#include "ActorFolderDesc.h"
#include "AssetToolsModule.h"
#include "Styling/AppStyle.h"
#include "ISourceControlModule.h"
#include "SourceControlAssetDataCache.h"
#include "SourceControlHelpers.h"
#include "SSourceControlFileDialog.h"

#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Logging/MessageLog.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "SourceControlChangelist"

//////////////////////////////////////////////////////////////////////////

FChangelistTreeItemPtr IChangelistTreeItem::GetParent() const
{
	return Parent;
}

const TArray<FChangelistTreeItemPtr>& IChangelistTreeItem::GetChildren() const
{
	return Children;
}

void IChangelistTreeItem::AddChild(TSharedRef<IChangelistTreeItem> Child)
{
	Child->Parent = AsShared();
	Children.Add(MoveTemp(Child));
}

void IChangelistTreeItem::RemoveChild(const TSharedRef<IChangelistTreeItem>& Child)
{
	if (Children.Remove(Child))
	{
		Child->Parent = nullptr;
	}
}

void IChangelistTreeItem::RemoveAllChildren()
{
	for (TSharedPtr<IChangelistTreeItem>& Child : Children)
	{
		Child->Parent = nullptr;
	}
	Children.Reset();
}

static FString RetrieveAssetName(const FAssetData& InAssetData)
{
	static const FName NAME_ActorLabel(TEXT("ActorLabel"));

	if (InAssetData.FindTag(NAME_ActorLabel))
	{
		FString ResultAssetName;
		InAssetData.GetTagValue(NAME_ActorLabel, ResultAssetName);
		return ResultAssetName;
	}
	else if (InAssetData.FindTag(FPrimaryAssetId::PrimaryAssetDisplayNameTag))
	{
		FString ResultAssetName;
		InAssetData.GetTagValue(FPrimaryAssetId::PrimaryAssetDisplayNameTag, ResultAssetName);
		return ResultAssetName;
	}
	else if (InAssetData.AssetClassPath == UActorFolder::StaticClass()->GetClassPathName())
	{
		FString ActorFolderPath = UActorFolder::GetAssetRegistryInfoFromPackage(InAssetData.PackageName).GetDisplayName();
		if (!ActorFolderPath.IsEmpty())
		{
			return ActorFolderPath;
		}
	}

	return InAssetData.AssetName.ToString();
}

static FString RetrieveAssetPath(const FAssetData& InAssetData)
{
	int32 LastDot = -1;
	FString Path = InAssetData.GetObjectPathString();

	// Strip asset name from object path
	if (Path.FindLastChar('.', LastDot))
	{
		Path.LeftInline(LastDot);
	}

	return Path;
}

static void RefreshAssetInformationInternal(const TArray<FAssetData>& Assets, const FString& InFilename, FString& OutAssetName, FString& OutAssetPath, FString& OutAssetType, FText& OutPackageName, FColor& OutAssetTypeColor)
{
	// Initialize display-related members
	FString Filename = InFilename;
	FString TempAssetName = SSourceControlCommon::GetDefaultAssetName().ToString();
	FString TempAssetPath = Filename;
	FString TempAssetType = SSourceControlCommon::GetDefaultAssetType().ToString();
	FString TempPackageName = Filename;
	FColor TempAssetColor = FColor(		// Copied from ContentBrowserCLR.cpp
		127 + FColor::Red.R / 2,	// Desaturate the colors a bit (GB colors were too.. much)
		127 + FColor::Red.G / 2,
		127 + FColor::Red.B / 2,
		200); // Opacity

	if (Assets.Num() > 0)
	{
		auto IsNotRedirector = [](const FAssetData& InAssetData) { return !InAssetData.IsRedirector(); };
		int32 NumUserFacingAsset = Algo::CountIf(Assets, IsNotRedirector);

		if (NumUserFacingAsset == 1)
		{
			const FAssetData& AssetData = *Algo::FindByPredicate(Assets, IsNotRedirector);

			TempAssetName = RetrieveAssetName(AssetData);
			TempAssetPath = RetrieveAssetPath(AssetData);
			TempAssetType = AssetData.AssetClassPath.ToString();

			const FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
			const TSharedPtr<IAssetTypeActions> AssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(AssetData.GetClass()).Pin();

			if (AssetTypeActions.IsValid())
			{
				TempAssetColor = AssetTypeActions->GetTypeColor();
			}
			else
			{
				TempAssetColor = FColor::White;
			}
		}
		else
		{
			TempAssetName = RetrieveAssetName(Assets[0]);
			TempAssetPath = RetrieveAssetPath(Assets[0]);

			for (int32 i = 1; i < Assets.Num(); ++i)
			{
				TempAssetName += TEXT(";") + RetrieveAssetName(Assets[i]);
			}

			TempAssetType = SSourceControlCommon::GetDefaultMultipleAsset().ToString();
			TempAssetColor = FColor::White;
		}

		// Beautify the package name
		TempPackageName = TempAssetPath + "." + TempAssetName;
	}
	else if (FPackageName::TryConvertFilenameToLongPackageName(Filename, TempPackageName))
	{
		// Fake asset name, asset path from the package name
		TempAssetPath = TempPackageName;

		int32 LastSlash = -1;
		if (TempPackageName.FindLastChar('/', LastSlash))
		{
			TempAssetName = TempPackageName;
			TempAssetName.RightChopInline(LastSlash + 1);
		}
	}
	else
	{
		TempAssetName = FPaths::GetCleanFilename(Filename);
		TempPackageName = Filename; // put back original package name if the try failed
		TempAssetType = FText::Format(SSourceControlCommon::GetDefaultUnknownAssetType(), FText::FromString(FPaths::GetExtension(Filename).ToUpper())).ToString();
	}

	// Finally, assign the temp variables to the member variables
	OutAssetName = TempAssetName;
	OutAssetPath = TempAssetPath;
	OutAssetType = TempAssetType;
	OutAssetTypeColor = TempAssetColor;
	OutPackageName = FText::FromString(TempPackageName);
}

//////////////////////////////////////////////////////////////////////////

FString IFileViewTreeItem::DefaultStrValue; // Default is an empty string.
FDateTime IFileViewTreeItem::DefaultDateTimeValue; // Default is FDateTime::MinValue().

void IFileViewTreeItem::SetLastModifiedDateTime(const FDateTime& Timestamp)
{
	if (Timestamp != LastModifiedDateTime) // Pay the text conversion only if needed.
	{
		LastModifiedDateTime = Timestamp;
		if (Timestamp != FDateTime::MinValue())
		{
			LastModifiedTimestampText = FText::AsDateTime(Timestamp, EDateTimeStyle::Short);
		}
		else
		{
			LastModifiedTimestampText = FText::GetEmpty();
		}
	}
}

//////////////////////////////////////////////////////////////////////////

FString FUnsavedAssetsTreeItem::GetDisplayString() const
{
	return "";
}

FFileTreeItem::FFileTreeItem(FSourceControlStateRef InFileState, bool bBeautifyPaths, bool bIsShelvedFile)
	: IFileViewTreeItem(bIsShelvedFile ? IChangelistTreeItem::ShelvedFile : IChangelistTreeItem::File)
	, FileState(InFileState)
	, MinTimeBetweenUpdate(FTimespan::FromSeconds(5.f))
	, LastUpdateTime()
	, bAssetsUpToDate(false)
{
	CheckBoxState = ECheckBoxState::Checked;

	// Initialize asset data first

	if (bBeautifyPaths)
	{
		FSourceControlAssetDataCache& AssetDataCache = ISourceControlModule::Get().GetAssetDataCache();
		bAssetsUpToDate = AssetDataCache.GetAssetDataArray(FileState, Assets);
	}
	else
	{
		// We do not need to wait for AssetData from the cache.
		bAssetsUpToDate = true;
	}

	RefreshAssetInformation();
}

int32 FFileTreeItem::GetIconSortingPriority() const
{
	if (!FileState->IsCurrent())        { return 0; } // First if sorted in ascending order.
	if (FileState->IsUnknown())         { return 1; }
	if (FileState->IsConflicted())      { return 2; }
	if (FileState->IsCheckedOutOther()) { return 3; }
	if (FileState->IsCheckedOut())      { return 4; }
	if (FileState->IsDeleted())         { return 5; }
	if (FileState->IsAdded())           { return 6; }
	else                                { return 7; }
}

const FString& FFileTreeItem::GetCheckedOutBy() const
{
	CheckedOutBy.Reset();
	FileState->IsCheckedOutOther(&CheckedOutBy);
	return CheckedOutBy;
}

FText FFileTreeItem::GetCheckedOutByUser() const
{
	return FText::FromString(GetCheckedOutBy());
}

void FFileTreeItem::RefreshAssetInformation()
{
	// Initialize display-related members
	static TArray<FAssetData> NoAssets;
	RefreshAssetInformationInternal(Assets.IsValid() ? *Assets : NoAssets, FileState->GetFilename(), AssetNameStr, AssetPathStr, AssetTypeStr, PackageName, AssetTypeColor);
	AssetName = FText::FromString(AssetNameStr);
	AssetPath = FText::FromString(AssetPathStr);
	AssetType = FText::FromString(AssetTypeStr);
}

FText FFileTreeItem::GetAssetName() const
{
	return AssetName;
}

FText FFileTreeItem::GetAssetName()
{
	const FTimespan CurrentTime = FTimespan::FromSeconds(FPlatformTime::Seconds());

	if ((!bAssetsUpToDate) && ((CurrentTime - LastUpdateTime) > MinTimeBetweenUpdate))
	{
		FSourceControlAssetDataCache& AssetDataCache = ISourceControlModule::Get().GetAssetDataCache();
		LastUpdateTime = CurrentTime;

		if (AssetDataCache.GetAssetDataArray(FileState, Assets))
		{
			bAssetsUpToDate = true;
			RefreshAssetInformation();
		}
	}

	return AssetName;
}

//////////////////////////////////////////////////////////////////////////

FText FShelvedChangelistTreeItem::GetDisplayText() const
{
	return LOCTEXT("SourceControl_ShelvedFiles", "Shelved Items");
}

//////////////////////////////////////////////////////////////////////////

FOfflineFileTreeItem::FOfflineFileTreeItem(const FString& InFilename)
	: IFileViewTreeItem(IChangelistTreeItem::OfflineFile)
	, Assets()
	, Filename(InFilename)
	, PackageName(FText::FromString(InFilename)) 
	, AssetName(SSourceControlCommon::GetDefaultAssetName())
	, AssetPath()
	, AssetType(SSourceControlCommon::GetDefaultAssetType())
	, AssetTypeColor()
{
	FString TempString;

	USourceControlHelpers::GetAssetData(InFilename, Assets);

	RefreshAssetInformation();
}

void FOfflineFileTreeItem::RefreshAssetInformation()
{
	RefreshAssetInformationInternal(Assets, Filename, AssetNameStr, AssetPathStr, AssetTypeStr, PackageName, AssetTypeColor);
	AssetName = FText::FromString(AssetNameStr);
	AssetPath = FText::FromString(AssetPathStr);
	AssetType = FText::FromString(AssetTypeStr);
}

//////////////////////////////////////////////////////////////////////////
namespace SSourceControlCommon
{

TSharedRef<SWidget> GetSCCFileWidget(FSourceControlStateRef InFileState, bool bIsShelvedFile)
{
	const FSlateBrush* IconBrush = FAppStyle::GetBrush("ContentBrowser.ColumnViewAssetIcon");

	// Make icon overlays (eg, SCC and dirty status) a reasonable size in relation to the icon size (note: it is assumed this icon is square)
	const float ICON_SCALING_FACTOR = 0.7f;
	const float IconOverlaySize = IconBrush->ImageSize.X * ICON_SCALING_FACTOR;

	return SNew(SOverlay)
		// The actual icon
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(IconBrush)
			.ColorAndOpacity_Lambda([bIsShelvedFile]() -> FSlateColor {
				return FSlateColor(bIsShelvedFile ? FColor::Yellow : FColor::White);
			})
		]
		// Source control state
		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(IconOverlaySize)
			.HeightOverride(IconOverlaySize)
			[
				SNew(SLayeredImage, InFileState->GetIcon())
				.ToolTipText(InFileState->GetDisplayTooltip())
			]
		];
}

TSharedRef<SWidget> GetSCCFileWidget()
{
	const FSlateBrush* IconBrush = FAppStyle::GetBrush("ContentBrowser.ColumnViewAssetIcon");

	// Make icon overlays (eg, SCC and dirty status) a reasonable size in relation to the icon size (note: it is assumed this icon is square)
	const float ICON_SCALING_FACTOR = 0.7f;
	const float IconOverlaySize = IconBrush->ImageSize.X * ICON_SCALING_FACTOR;

	return SNew(SOverlay)
		// The actual icon
		+ SOverlay::Slot()
		[
			SNew(SImage)
			.Image(IconBrush)
		.ColorAndOpacity(FSlateColor(FColor::White))
		]
	// Source control state
	+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(IconOverlaySize)
		.HeightOverride(IconOverlaySize)
		];
}

FText GetDefaultAssetName()
{
	return LOCTEXT("SourceControl_DefaultAssetName", "Unavailable");
}

FText GetDefaultAssetType()
{
	return LOCTEXT("SourceControl_DefaultAssetType", "Unknown");
}

FText GetDefaultUnknownAssetType()
{
	return LOCTEXT("SourceControl_FileTypeDefault", "{0} File");
}

FText GetDefaultMultipleAsset()
{
	return LOCTEXT("SourceCOntrol_ManyAssetType", "Multiple Assets");
}

FText GetSingleLineChangelistDescription(const FText& InFullDescription, ESingleLineFlags Flags)
{
	FString DescriptionTextAsString = InFullDescription.ToString();
	DescriptionTextAsString.TrimStartAndEndInline();

	if ((Flags & ESingleLineFlags::Mask_NewlineBehavior) == ESingleLineFlags::NewlineConvertToSpace)
	{
		static constexpr TCHAR Replacer = TCHAR(' ');
		// Replace all non-space whitespace characters with space
		Algo::ReplaceIf(DescriptionTextAsString,
			[](TCHAR C) { return FChar::IsWhitespace(C) && C != Replacer; },
			Replacer);
	}
	else
	{
		int32 NewlineStartIndex = INDEX_NONE;
		DescriptionTextAsString.FindChar(TCHAR('\n'), NewlineStartIndex);
		if (NewlineStartIndex != INDEX_NONE)
		{
			DescriptionTextAsString.LeftInline(NewlineStartIndex);
		}

		// Trim any trailing carriage returns
		if (DescriptionTextAsString.EndsWith(TEXT("\r"), ESearchCase::CaseSensitive))
		{
			DescriptionTextAsString.LeftChopInline(1);
		}
	}

	return InFullDescription.IsCultureInvariant() ? FText::AsCultureInvariant(DescriptionTextAsString) : FText::FromString(DescriptionTextAsString);
}

/** Wraps the execution of a changelist operations with a slow task. */
void ExecuteChangelistOperationWithSlowTaskWrapper(const FText& Message, const TFunction<void()>& ChangelistTask)
{
	// NOTE: This is a ugly workaround for P4 because the generic popup feedback operations in FScopedSourceControlProgress() was supressed for all synchrounous
	//       operations. For other source control providers, the popup still shows up and showing a slow task and the FScopedSourceControlProgress at the same
	//       time is a bad user experience. Until we fix source control popup situation in general in the Editor, this hack is in place to avoid the double popup.
	//       At the time of writing, the other source control provider that supports changelists is Plastic.
	if (ISourceControlModule::Get().GetProvider().GetName() == "Perforce")
	{
		FScopedSlowTask Progress(0.f, Message);
		Progress.MakeDialog();
		ChangelistTask();
	}
	else
	{
		ChangelistTask();
	}
}

/** Wraps the execution of an uncontrolled changelist operations with a slow task. */
void ExecuteUncontrolledChangelistOperationWithSlowTaskWrapper(const FText& Message, const TFunction<void()>& UncontrolledChangelistTask)
{
	ExecuteChangelistOperationWithSlowTaskWrapper(Message, UncontrolledChangelistTask);
}

/** Displays toast notification to report the status of task. */
void DisplaySourceControlOperationNotification(const FText& Message, SNotificationItem::ECompletionState CompletionState)
{
	if (Message.IsEmpty())
	{
		return;
	}

	FMessageLog("SourceControl").Message(CompletionState == SNotificationItem::ECompletionState::CS_Fail ? EMessageSeverity::Error : EMessageSeverity::Info, Message);

	FNotificationInfo NotificationInfo(Message);
	NotificationInfo.ExpireDuration = 6.0f;
	NotificationInfo.Hyperlink = FSimpleDelegate::CreateLambda([]() { FGlobalTabmanager::Get()->TryInvokeTab(FName("OutputLog")); });
	NotificationInfo.HyperlinkText = LOCTEXT("ShowOutputLogHyperlink", "Show Output Log");
	FSlateNotificationManager::Get().AddNotification(NotificationInfo)->SetCompletionState(CompletionState);
}

bool OpenConflictDialog(const TArray<FSourceControlStateRef>& InFilesConflicts)
{
	TSharedPtr<SWindow> Window;
	TSharedPtr<SSourceControlFileDialog> SourceControlFileDialog;

	Window = SNew(SWindow)
			 .Title(LOCTEXT("CheckoutPackagesDialogTitle", "Check Out Assets"))
			 .SizingRule(ESizingRule::UserSized)
			 .ClientSize(FVector2D(1024.0f, 512.0f))
			 .SupportsMaximize(false)
			 .SupportsMinimize(false)
			 [
			 	SNew(SBorder)
			 	.Padding(4.f)
			 	.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			 	[
			 		SAssignNew(SourceControlFileDialog, SSourceControlFileDialog)
			 		.Message(LOCTEXT("CheckoutPackagesDialogMessage", "Conflict detected in the following assets:"))
			 		.Warning(LOCTEXT("CheckoutPackagesWarnMessage", "Warning: These assets are locked or not at the head revision. You may lose your changes if you continue, as you will be unable to submit them to revision control."))
			 		.Files(InFilesConflicts)
			 	]
			 ];

	SourceControlFileDialog->SetWindow(Window);
	Window->SetWidgetToFocusOnActivate(SourceControlFileDialog);
	GEditor->EditorAddModalWindow(Window.ToSharedRef());

	return SourceControlFileDialog->IsProceedButtonPressed();
}


} // end of namespace SSourceControlCommon

#undef LOCTEXT_NAMESPACE
