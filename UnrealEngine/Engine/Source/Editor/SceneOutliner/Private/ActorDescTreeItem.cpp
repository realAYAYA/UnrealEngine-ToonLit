// Copyright Epic Games, Inc. All Rights Reserved.

#include "ActorDescTreeItem.h"
#include "WorldPartition/ActorDescContainer.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/WorldPartitionActorDesc.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor.h"
#include "ScopedTransaction.h"
#include "SceneOutlinerPublicTypes.h"
#include "SceneOutlinerDragDrop.h"
#include "SceneOutlinerStandaloneTypes.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ActorEditorUtils.h"
#include "ClassIconFinder.h"
#include "ISceneOutliner.h"
#include "ISceneOutlinerMode.h"
#include "Logging/MessageLog.h"
#include "SSocketChooser.h"
#include "ToolMenus.h"
#include "LevelEditorViewport.h"
#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "SceneOutliner_ActorDescTreeItem"

const FSceneOutlinerTreeItemType FActorDescTreeItem::Type(&IActorBaseTreeItem::Type);

struct SActorDescTreeLabel : FSceneOutlinerCommonLabelData, public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SActorDescTreeLabel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, FActorDescTreeItem& ActorDescItem, ISceneOutliner& SceneOutliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow)
	{
		WeakSceneOutliner = StaticCastSharedRef<ISceneOutliner>(SceneOutliner.AsShared());

		TreeItemPtr = StaticCastSharedRef<FActorDescTreeItem>(ActorDescItem.AsShared());

		HighlightText = SceneOutliner.GetFilterHighlightText();

		TSharedPtr<SInlineEditableTextBlock> InlineTextBlock;

		auto MainContent = SNew(SHorizontalBox)
			// Main actor desc label
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineTextBlock, SInlineEditableTextBlock)
				.Text(this, &SActorDescTreeLabel::GetDisplayText)
				.ToolTipText(this, &SActorDescTreeLabel::GetTooltipText)
				.HighlightText(HighlightText)
				.ColorAndOpacity(this, &SActorDescTreeLabel::GetForegroundColor)
				.OnTextCommitted(this, &SActorDescTreeLabel::OnLabelCommitted)
				.OnVerifyTextChanged(this, &SActorDescTreeLabel::OnVerifyItemLabelChanged)
				.IsSelected(FIsSelected::CreateSP(&InRow, &STableRow<FSceneOutlinerTreeItemPtr>::IsSelectedExclusively))
				.IsReadOnly_Lambda([Item = ActorDescItem.AsShared(), this]()
			{
				return !CanExecuteRenameRequest(Item.Get());
			})
			]

		+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(0.0f, 0.f, 3.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SActorDescTreeLabel::GetTypeText)
				.Visibility(this, &SActorDescTreeLabel::GetTypeTextVisibility)
				.HighlightText(HighlightText)
			];

		if (WeakSceneOutliner.Pin()->GetMode()->IsInteractive())
		{
			ActorDescItem.RenameRequestEvent.BindSP(InlineTextBlock.Get(), &SInlineEditableTextBlock::EnterEditingMode);
		}

		ChildSlot
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(FSceneOutlinerDefaultTreeItemMetrics::IconPadding())
				[
					SNew(SBox)
					.WidthOverride(FSceneOutlinerDefaultTreeItemMetrics::IconSize())
					.HeightOverride(FSceneOutlinerDefaultTreeItemMetrics::IconSize())
					[
						SNew(SImage)
						.Image(this, &SActorDescTreeLabel::GetIcon)
					.ToolTipText(this, &SActorDescTreeLabel::GetIconTooltip)
					.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

			+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f)
				[
					MainContent
				]
			];
	}

private:
	TWeakPtr<FActorDescTreeItem> TreeItemPtr;
	TAttribute<FText> HighlightText;

	FText GetDisplayText() const
	{
		if (TSharedPtr<FActorDescTreeItem> TreeItem = TreeItemPtr.Pin())
		{
			if (const FWorldPartitionActorDesc* ActorDesc = TreeItem->ActorDescHandle.Get())
			{
				FFormatNamedArguments Args;
				Args.Add(TEXT("ActorLabel"), FText::FromString(TreeItem->GetDisplayString()));
				
				UActorDescContainer* ActorDescContainer = ActorDesc->GetContainer();
				UWorld* World = ActorDescContainer != nullptr ? ActorDescContainer->GetWorld() : nullptr;
				UWorldPartition* WorldPartition = World != nullptr ? World->GetWorldPartition() : nullptr;
				if (WorldPartition && WorldPartition->IsActorPinned(ActorDesc->GetGuid()))
				{
					Args.Add(TEXT("UnloadState"), LOCTEXT("UnloadedDataLayer", "(Unloaded DataLayer)"));
				}
				else
				{
					Args.Add(TEXT("UnloadState"), LOCTEXT("UnloadedActorLabel", "(Unloaded)"));
				}

				return FText::Format(LOCTEXT("UnloadedActorDisplay", "{ActorLabel} {UnloadState}"), Args);
			}
		}
		return FText();
	}

	FText GetTooltipText() const
	{
		return FText();
	}

	FText GetTypeText() const
	{
		if (TSharedPtr<FActorDescTreeItem> TreeItem = TreeItemPtr.Pin())
		{
			if (const FWorldPartitionActorDesc* ActorDesc = TreeItem->ActorDescHandle.Get())
			{
				return FText::FromName(ActorDesc->GetDisplayClassName());
			}
		}

		return FText();
	}

	EVisibility GetTypeTextVisibility() const
	{
		return HighlightText.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
	}

	const FSlateBrush* GetIcon() const
	{
		TSharedPtr<FActorDescTreeItem> TreeItem = TreeItemPtr.Pin();

		if (TreeItem.IsValid() && WeakSceneOutliner.IsValid())
		{
			if (const FWorldPartitionActorDesc* ActorDesc = TreeItem->ActorDescHandle.Get())
			{
				const FName IconName = ActorDesc->GetDisplayClassName();

				const FSlateBrush* CachedBrush = WeakSceneOutliner.Pin()->GetCachedIconForClass(IconName);
				if (CachedBrush != nullptr)
				{
					return CachedBrush;
				}
				else if (IconName != NAME_None)
				{
					const FSlateBrush* FoundSlateBrush = FSlateIconFinder::FindIconForClass(ActorDesc->GetActorNativeClass()).GetIcon();
					WeakSceneOutliner.Pin()->CacheIconForClass(IconName, FoundSlateBrush);
					return FoundSlateBrush;
				}
			}
		}

		return nullptr;
	}

	const FSlateBrush* GetIconOverlay() const
	{
		return nullptr;
	}

	FText GetIconTooltip() const
	{
		return FText();
	}

	virtual FSlateColor GetForegroundColor() const override
	{
		if (const auto TreeItem = TreeItemPtr.Pin())
		{
			if (auto BaseColor = FSceneOutlinerCommonLabelData::GetForegroundColor(*TreeItem))
			{
				return BaseColor.GetValue();
			}
			
			if(WeakSceneOutliner.IsValid())
			{
				// Use the normal foreground color for selected items to make them readable
				if(WeakSceneOutliner.Pin()->GetTree().IsItemSelected(TreeItem))
				{
					return FSlateColor::UseSubduedForeground();
				}
			}
		}
		
		return FSceneOutlinerCommonLabelData::DarkColor;
	}

	bool OnVerifyItemLabelChanged(const FText&, FText&)
	{
		// don't allow label change for unloaded actor items
		return false;
	}

	void OnLabelCommitted(const FText& InLabel, ETextCommit::Type InCommitInfo)
	{
		// not supported.
	}
};

FActorDescTreeItem::FActorDescTreeItem(const FGuid& InActorGuid, UActorDescContainer* Container)
	: IActorBaseTreeItem(Type)
	, ActorDescHandle(Container, InActorGuid)
	, ActorGuid(InActorGuid)
{
	Initialize();
}

FActorDescTreeItem::FActorDescTreeItem(const FGuid& InActorGuid, FActorDescContainerCollection* ContainerCollection)
	: IActorBaseTreeItem(Type)
	, ActorDescHandle(ContainerCollection, InActorGuid)
	, ActorGuid(InActorGuid)
{
	Initialize();
}

void FActorDescTreeItem::Initialize()
{
	ID = FSceneOutlinerTreeItemID(ActorGuid);

	if (const FWorldPartitionActorDesc* const ActorDesc = ActorDescHandle.Get())
	{
		DisplayString = ActorDesc->GetActorLabel().ToString();
	}
	else
	{
		DisplayString = LOCTEXT("ActorLabelForMissingActor", "(Deleted Actor)").ToString();
	}
}

FSceneOutlinerTreeItemID FActorDescTreeItem::GetID() const
{
	return ID;
}

FString FActorDescTreeItem::GetDisplayString() const
{
	return DisplayString;
}

bool FActorDescTreeItem::CanInteract() const
{
	return ActorDescHandle.IsValid();
}

TSharedRef<SWidget> FActorDescTreeItem::GenerateLabelWidget(ISceneOutliner& Outliner, const STableRow<FSceneOutlinerTreeItemPtr>& InRow)
{
	return SNew(SActorDescTreeLabel, *this, Outliner, InRow);
}

void FActorDescTreeItem::FocusActorBounds() const
{
	if (FWorldPartitionActorDesc const* ActorDesc = ActorDescHandle.Get())
	{
		const bool bActiveViewportOnly = true;
		GEditor->MoveViewportCamerasToBox(ActorDesc->GetBounds(), bActiveViewportOnly, 0.5f);
	}
}

void FActorDescTreeItem::CopyActorFilePathtoClipboard() const
{
	if (FWorldPartitionActorDesc const* ActorDesc = ActorDescHandle.Get())
	{
		FString PackageFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(ActorDesc->GetActorPackage().ToString(), PackageFilename, FPackageName::GetAssetPackageExtension()))
		{
			FString Result = FPaths::ConvertRelativePathToFull(PackageFilename);
			FPlatformApplicationMisc::ClipboardCopy(*Result);
		}
	}
}

void FActorDescTreeItem::GenerateContextMenu(UToolMenu* Menu, SSceneOutliner&)
{
	FToolMenuSection& Section = Menu->AddSection("Section");
	Section.AddMenuEntry("FocusActorBounds", LOCTEXT("FocusActorBounds", "Focus Actor Bounds"), FText(), FSlateIcon(), FUIAction(FExecuteAction::CreateSP(this, &FActorDescTreeItem::FocusActorBounds)));
	Section.AddMenuEntry("CopyActorFilePath", LOCTEXT("CopyActorFilePath", "Copy Actor File Path"), LOCTEXT("CopyActorFilePathTooltip", "Copy the file path where this actor is saved"), FSlateIcon(FAppStyle::GetAppStyleSetName(), "GenericCommands.Copy"), FUIAction(FExecuteAction::CreateSP(this, &FActorDescTreeItem::CopyActorFilePathtoClipboard)));
}

bool FActorDescTreeItem::GetVisibility() const
{
	return true;
}

bool FActorDescTreeItem::ShouldShowPinnedState() const
{
	if (ActorDescHandle.IsValid())
	{
		// Pinning of ActorDescs is only supported on the main world partition
		if (UActorDescContainer* Container = ActorDescHandle->GetContainer())
		{
			return Container->IsMainPartitionContainer();
		}
	}

	return false;
}

bool FActorDescTreeItem::GetPinnedState() const
{
	if (ActorDescHandle.IsValid())
	{
		UWorldPartition* WorldPartition = ActorDescHandle->GetContainer()->GetWorld()->GetWorldPartition();
		return WorldPartition ? WorldPartition->IsActorPinned(GetGuid()) : false;
	}
	return false;
}

#undef LOCTEXT_NAMESPACE
