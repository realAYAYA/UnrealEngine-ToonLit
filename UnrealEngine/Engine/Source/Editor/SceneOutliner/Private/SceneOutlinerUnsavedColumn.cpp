// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerUnsavedColumn.h"
#include "ActorTreeItem.h"
#include "ActorDescTreeItem.h"
#include "ActorFolderTreeItem.h"
#include "SourceControlHelpers.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "UnsavedAssetsTrackerModule.h"
#include "SortHelper.h"
#include "ISceneOutliner.h"
#include "ISceneOutlinerMode.h"
#include "SceneOutlinerHelpers.h"

#define LOCTEXT_NAMESPACE "SceneOutlinerActorUnsavedColumn"

class SUnsavedActorWidget : public SImage
{
public:
	SLATE_BEGIN_ARGS(SUnsavedActorWidget) {}
	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct(const FArguments& InArgs, const FString& InActorExternalPackageFilename);
	
	bool IsUnsaved() const;
	
private:
	void UpdateImage();
	void OnUnsavedAssetAdded(const FString& FileAbsPathname);
	void OnUnsavedAssetRemoved(const FString& FileAbsPathname);

private:
	FString ExternalPackageFilename;
	bool bIsUnsaved;
};

void SUnsavedActorWidget::Construct(const FArguments& InArgs, const FString& InActorExternalPackageFilename)
{
	ExternalPackageFilename = InActorExternalPackageFilename;

	SImage::Construct(
			SImage::FArguments()
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(FStyleDefaults::GetNoBrush()));
	
	FUnsavedAssetsTrackerModule& UnsavedAssetsTrackerModule = FUnsavedAssetsTrackerModule::Get();
	UnsavedAssetsTrackerModule.OnUnsavedAssetAdded.AddSP(this, &SUnsavedActorWidget::OnUnsavedAssetAdded);
	UnsavedAssetsTrackerModule.OnUnsavedAssetRemoved.AddSP(this, &SUnsavedActorWidget::OnUnsavedAssetRemoved);

	bIsUnsaved = UnsavedAssetsTrackerModule.IsAssetUnsaved(ExternalPackageFilename);

	UpdateImage();
}

bool SUnsavedActorWidget::IsUnsaved() const
{
	return bIsUnsaved;
}

void SUnsavedActorWidget::OnUnsavedAssetAdded(const FString& FileAbsPathname)
{
	if (FileAbsPathname == ExternalPackageFilename)
	{
		// We should never be desynced, i.e if this item was added as an unsaved asset bIsUnsavedAsset MUST be false before
		check(!bIsUnsaved)
		bIsUnsaved = true;
		
		UpdateImage();
	}
}

void SUnsavedActorWidget::OnUnsavedAssetRemoved(const FString& FileAbsPathname)
{
	if (FileAbsPathname == ExternalPackageFilename)
	{
		// We should never be desynced, i.e if this item was removed from the unsaved asset list bIsUnsavedAsset MUST be true before
		check(bIsUnsaved)
		bIsUnsaved = false;

		UpdateImage();
	}
}

void SUnsavedActorWidget::UpdateImage()
{
	if (bIsUnsaved)
	{
		SetImage(FAppStyle::GetBrush("Icons.DirtyBadge"));
	}
	else
	{
		SetImage(nullptr);
	}
}


FName FSceneOutlinerActorUnsavedColumn::GetColumnID()
{
	return GetID();
}

SHeaderRow::FColumn::FArguments FSceneOutlinerActorUnsavedColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnID())
		.FixedWidth(24.f)
		.HAlignHeader(HAlign_Center)
		.VAlignHeader(VAlign_Center)
		.HAlignCell(HAlign_Center)
		.VAlignCell(VAlign_Center)
		.DefaultTooltip(FText::FromName(GetColumnID()))
		[
			SNew(SImage)
			.Image(FAppStyle::GetBrush("Icons.DirtyBadge"))
			.ColorAndOpacity(FSlateColor::UseForeground())
		];
}

const TSharedRef<SWidget> FSceneOutlinerActorUnsavedColumn::ConstructRowWidget(FSceneOutlinerTreeItemRef TreeItem, const STableRow<FSceneOutlinerTreeItemPtr>& Row)
{
	const FString ExternalPackageName = SceneOutliner::FSceneOutlinerHelpers::GetExternalPackageName(TreeItem.Get());
	const FString ExternalPackageFileName = !ExternalPackageName.IsEmpty() ? USourceControlHelpers::PackageFilename(ExternalPackageName) : FString();

	if (ExternalPackageFileName.IsEmpty())
	{
		return SNullWidget::NullWidget;
	}

	const TSharedRef<SUnsavedActorWidget> ActualWidget = SNew(SUnsavedActorWidget, ExternalPackageFileName);

	// Store a reference to the widget for this asset so we can grab the unsaved status for sorting
	UnsavedActorWidgets.Add(ExternalPackageFileName, ActualWidget);
	
	return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				ActualWidget
			];
}

void FSceneOutlinerActorUnsavedColumn::SortItems(TArray<FSceneOutlinerTreeItemPtr>& RootItems, const EColumnSortMode::Type SortMode) const
{
	FSceneOutlinerSortHelper<bool, SceneOutliner::FNumericStringWrapper>()
		/** Sort by unsaved first */
		.Primary([this](const ISceneOutlinerTreeItem& Item)
		{
			FString LongPackageName = SceneOutliner::FSceneOutlinerHelpers::GetExternalPackageName(Item);
			if (!LongPackageName.IsEmpty())
			{
				if (const TSharedRef<SUnsavedActorWidget>* FoundWidget = UnsavedActorWidgets.Find(USourceControlHelpers::PackageFilename(LongPackageName)))
				{
					// return the inverse because we want items with Unsaved = TRUE to show up on top in ascending sort (default)
					return !(*FoundWidget)->IsUnsaved();
				}
			}
			return true;
		}, SortMode)
		/** Then by type */
		.Secondary([this](const ISceneOutlinerTreeItem& Item){ return SceneOutliner::FNumericStringWrapper(Item.GetDisplayString()); }, SortMode)
		.Sort(RootItems);
}

#undef LOCTEXT_NAMESPACE