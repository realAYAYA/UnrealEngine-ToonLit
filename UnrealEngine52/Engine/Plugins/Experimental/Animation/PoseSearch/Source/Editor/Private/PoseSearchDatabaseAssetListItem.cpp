// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseAssetListItem.h"
#include "PoseSearchDatabaseViewModel.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimComposite.h"
#include "Animation/BlendSpace.h"

#include "Misc/TransactionObjectEvent.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Misc/FeedbackContext.h"
#include "AssetSelection.h"
#include "ClassIconFinder.h"
#include "DetailColumnSizeData.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearchDatabaseAssetTree.h"

#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SPositiveActionButton.h"
#include "Styling/AppStyle.h"

#include "ScopedTransaction.h"
#include "Styling/StyleColors.h"

#define LOCTEXT_NAMESPACE "SDatabaseAssetListItem"

namespace UE::PoseSearch
{
	static constexpr FLinearColor DisabledColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.25f);
	
	void SDatabaseAssetListItem::Construct(
		const FArguments& InArgs,
		const TSharedRef<FDatabaseViewModel>& InEditorViewModel,
		const TSharedRef<STableViewBase>& OwnerTable,
		TSharedRef<FDatabaseAssetTreeNode> InAssetTreeNode,
		TSharedRef<FUICommandList> InCommandList,
		TSharedPtr<SDatabaseAssetTree> InHierarchy)
	{
		WeakAssetTreeNode = InAssetTreeNode;
		EditorViewModel = InEditorViewModel;
		SkeletonView = InHierarchy;

		if (InAssetTreeNode->SourceAssetType == ESearchIndexAssetType::Invalid)
		{
			ConstructGroupItem(OwnerTable);
		}
		else
		{
			ConstructAssetItem(OwnerTable);
		}
	}

	void SDatabaseAssetListItem::ConstructGroupItem(const TSharedRef<STableViewBase>& OwnerTable)
	{
		STableRow<TSharedPtr<FDatabaseAssetTreeNode>>::ChildSlot
		.Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			GenerateItemWidget()
		];

		STableRow<TSharedPtr<FDatabaseAssetTreeNode>>::ConstructInternal(
			STableRow<TSharedPtr<FDatabaseAssetTreeNode>>::FArguments()
			.Style(FAppStyle::Get(), "DetailsView.TreeView.TableRow")
			.OnCanAcceptDrop(SkeletonView.Pin().Get(), &SDatabaseAssetTree::OnCanAcceptDrop)
			.OnAcceptDrop(SkeletonView.Pin().Get(), &SDatabaseAssetTree::OnAcceptDrop)
			.ShowSelection(true),
			OwnerTable);
	}

	void SDatabaseAssetListItem::ConstructAssetItem(const TSharedRef<STableViewBase>& OwnerTable)
	{
		STableRow<TSharedPtr<FDatabaseAssetTreeNode>>::Construct(
			STableRow<TSharedPtr<FDatabaseAssetTreeNode>>::FArguments()
			.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row"))
			.OnCanAcceptDrop(SkeletonView.Pin().Get(), &SDatabaseAssetTree::OnCanAcceptDrop)
			.OnAcceptDrop(SkeletonView.Pin().Get(), &SDatabaseAssetTree::OnAcceptDrop)
			.ShowWires(false)
			.Content()
			[
				GenerateItemWidget()
			], OwnerTable);
	}

	void SDatabaseAssetListItem::OnAddSequence()
	{
		EditorViewModel.Pin()->AddSequenceToDatabase(nullptr);
		SkeletonView.Pin()->RefreshTreeView(false);
	}

	void SDatabaseAssetListItem::OnAddBlendSpace()
	{
		EditorViewModel.Pin()->AddBlendSpaceToDatabase(nullptr);
		SkeletonView.Pin()->RefreshTreeView(false);
	}

	void SDatabaseAssetListItem::OnAddAnimComposite()
	{
		EditorViewModel.Pin()->AddAnimCompositeToDatabase(nullptr);
		SkeletonView.Pin()->RefreshTreeView(false);
	}

	FText SDatabaseAssetListItem::GetName() const
	{
		TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();

		if (const UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase())
		{
			if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetAnimationAssetBase(Node->SourceAssetIdx))
			{
				return FText::FromString(DatabaseAnimationAsset->GetName());
			}

			return FText::FromString(Database->GetName());
		}

		return LOCTEXT("None", "None");
	}

	TSharedRef<SWidget> SDatabaseAssetListItem::GenerateItemWidget()
	{
		TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();
		UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase();
		
		TSharedPtr<SWidget> ItemWidget;
		const FDetailColumnSizeData& ColumnSizeData = SkeletonView.Pin()->GetColumnSizeData();
		
		if (Node->SourceAssetType == ESearchIndexAssetType::Invalid)
		{
			// it's a group
			SAssignNew(ItemWidget, SBorder)
			.BorderImage(this, &SDatabaseAssetListItem::GetGroupBackgroundImage)
			.Padding(FMargin(3.0f, 5.0f))
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(5.0f)
				.AutoWidth()
				[
					SNew(SExpanderArrow, STableRow<TSharedPtr<FDatabaseAssetTreeNode>>::SharedThis(this))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SRichTextBlock)
					.Text(this, &SDatabaseAssetListItem::GetName)
					.TransformPolicy(ETextTransformPolicy::ToUpper)
					.DecoratorStyleSet(&FAppStyle::Get())
					.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				.Padding(2, 0, 0, 0)
				[
					GenerateAddButtonWidget()
				]
			];
		}
		else
		{
			// Item Icon
			TSharedPtr<SImage> ItemIconWidget;
			if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetAnimationAssetBase(Node->SourceAssetIdx))
			{
				SAssignNew(ItemIconWidget, SImage)
					.Image(FSlateIconFinder::FindIconBrushForClass(DatabaseAnimationAsset->GetAnimationAssetStaticClass()));
			}

			// Setup table row to display 
			SAssignNew(ItemWidget, SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSplitter)
				.Style(FAppStyle::Get(), "FoliageEditMode.Splitter")
				.PhysicalSplitterHandleSize(1.0f)
				.HitDetectionSplitterHandleSize(5.0f)
				.HighlightedHandleIndex(ColumnSizeData.GetHoveredSplitterIndex())
				.MinimumSlotHeight(0.5f)
				
				// Asset Name with type icon
				+SSplitter::Slot()
				.Value(ColumnSizeData.GetNameColumnWidth())
				.MinSize(0.3f)
				.OnSlotResized(ColumnSizeData.GetOnNameColumnResized())
				[
					SNew(SHorizontalBox)
					.Clipping(EWidgetClipping::ClipToBounds)
					+ SHorizontalBox::Slot()
					.MaxWidth(18)
					.AutoWidth()
					.Padding(0.0f, 0.0f, 5.0f, 0.0f)
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						ItemIconWidget.ToSharedRef()
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(this, &SDatabaseAssetListItem::GetName)
						.ColorAndOpacity(this, &SDatabaseAssetListItem::GetNameTextColorAndOpacity)
					]
				]
				
				// Display information via icons
				+SSplitter::Slot()
				.Value(ColumnSizeData.GetValueColumnWidth())
				.MinSize(0.3f)
				.OnSlotResized(ColumnSizeData.GetOnValueColumnResized())
				[
					// Asset Info.

					// Looping
					SNew(SHorizontalBox)
					.Clipping(EWidgetClipping::ClipToBounds)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 1.0f)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("Graph.Node.Loop"))
						.ColorAndOpacity(this, &SDatabaseAssetListItem::GetLoopingColorAndOpacity)
						.ToolTipText(this, &SDatabaseAssetListItem::GetLoopingToolTip)
					]

					// Root Motion
					+ SHorizontalBox::Slot()
					.Padding(1.0f, 1.0f)
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("AnimGraph.Attribute.RootMotionDelta.Icon"))
						.ColorAndOpacity(this, &SDatabaseAssetListItem::GetRootMotionColorAndOpacity)
						.ToolTipText(this, &SDatabaseAssetListItem::GetRootMotionOptionToolTip)
					]
					
					// Mirror Type
					+ SHorizontalBox::Slot()
					.Padding(1.0f, 1.0f)
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(this, &SDatabaseAssetListItem::GetMirrorOptionSlateBrush)
						.ToolTipText(this, &SDatabaseAssetListItem::GetMirrorOptionToolTip)
					]
				]
			]
			
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.MaxWidth(18)
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.EyeDropper"))
					.Visibility_Raw(this, &SDatabaseAssetListItem::GetSelectedActorIconVisbility)
				]
				+ SHorizontalBox::Slot()
				.MaxWidth(16)
				.Padding(4.0f, 0.0f)
				.AutoWidth()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SCheckBox)
					.IsChecked(this, &SDatabaseAssetListItem::GetAssetEnabledChecked)
					.OnCheckStateChanged(const_cast<SDatabaseAssetListItem*>(this), &SDatabaseAssetListItem::OnAssetIsEnabledChanged)
					.ToolTipText(this, &SDatabaseAssetListItem::GetAssetEnabledToolTip)
					.CheckedImage(FAppStyle::Get().GetBrush("Icons.Visible"))
					.CheckedHoveredImage(FAppStyle::Get().GetBrush("Icons.Visible"))
					.CheckedPressedImage(FAppStyle::Get().GetBrush("Icons.Visible"))
					.UncheckedImage(FAppStyle::Get().GetBrush("Icons.Hidden"))
					.UncheckedHoveredImage(FAppStyle::Get().GetBrush("Icons.Hidden"))
					.UncheckedPressedImage(FAppStyle::Get().GetBrush("Icons.Hidden"))
				]
			];
		}

		return ItemWidget.ToSharedRef();
	}

	TSharedRef<SWidget> SDatabaseAssetListItem::GenerateAddButtonWidget()
	{
		FMenuBuilder AddOptions(true, nullptr);

		AddOptions.AddMenuEntry(
			LOCTEXT("AddSequence", "Add Sequence"),
			LOCTEXT("AddSequenceTooltip", "Add new sequence to this group"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SDatabaseAssetListItem::OnAddSequence)),
			NAME_None,
			EUserInterfaceActionType::Button);

		AddOptions.AddMenuEntry(
			LOCTEXT("AddBlendSpaceOption", "Add Blend Space"),
			LOCTEXT("AddBlendSpaceOptionTooltip", "Add new blend space to this group"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SDatabaseAssetListItem::OnAddBlendSpace)),
			NAME_None,
			EUserInterfaceActionType::Button);

		AddOptions.AddMenuEntry(
			LOCTEXT("AnimCompositeOption", "Add Anim Composite"),
			LOCTEXT("AddAnimCompositeToDefaultGroupTooltip", "Add new anim composite to this group"),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateSP(this, &SDatabaseAssetListItem::OnAddAnimComposite)),
			NAME_None,
			EUserInterfaceActionType::Button);

		TSharedPtr<SComboButton> AddButton;
		SAssignNew(AddButton, SComboButton)
		.ContentPadding(0)
		.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
		.HasDownArrow(false)
		.ButtonContent()
		[
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(FAppStyle::Get().GetBrush("Icons.PlusCircle"))
		]
		.MenuContent()
		[
			AddOptions.MakeWidget()
		];

		return AddButton.ToSharedRef();
	}


	const FSlateBrush* SDatabaseAssetListItem::GetGroupBackgroundImage() const
	{
		if (STableRow<TSharedPtr<FDatabaseAssetTreeNode>>::IsHovered())
		{
			return FAppStyle::Get().GetBrush("Brushes.Secondary");
		}
		else
		{
			return FAppStyle::Get().GetBrush("Brushes.Header");
		}
	}

	EVisibility SDatabaseAssetListItem::GetSelectedActorIconVisbility() const
	{
		TSharedPtr<FDatabaseViewModel> ViewModelPtr = EditorViewModel.Pin();
		TSharedPtr<FDatabaseAssetTreeNode> TreeNodePtr = WeakAssetTreeNode.Pin();
		if (const FPoseSearchIndexAsset* SelectedIndexAsset = ViewModelPtr->GetSelectedActorIndexAsset())
		{
			if (TreeNodePtr->SourceAssetType == ESearchIndexAssetType::Sequence &&
				TreeNodePtr->SourceAssetIdx == SelectedIndexAsset->SourceAssetIdx)
			{
				return EVisibility::Visible;
			}
		}

		return EVisibility::Hidden;
	}

	ECheckBoxState SDatabaseAssetListItem::GetAssetEnabledChecked() const
	{
		TSharedPtr<FDatabaseViewModel> ViewModelPtr = EditorViewModel.Pin();
		TSharedPtr<FDatabaseAssetTreeNode> TreeNodePtr = WeakAssetTreeNode.Pin();
		const UPoseSearchDatabase* Database = ViewModelPtr->GetPoseSearchDatabase();

		if (Database->AnimationAssets.IsValidIndex(TreeNodePtr->SourceAssetIdx))
		{
			if (ViewModelPtr->IsEnabled(TreeNodePtr->SourceAssetIdx))
			{
				return ECheckBoxState::Checked;
			}
		}

		return ECheckBoxState::Unchecked;
	}

	void SDatabaseAssetListItem::OnAssetIsEnabledChanged(ECheckBoxState NewCheckboxState)
	{
		const FScopedTransaction Transaction(LOCTEXT("EnableChangedForAssetInPoseSearchDatabase", "Update enabled flag for item from Pose Search Database"));

		const TSharedPtr<FDatabaseViewModel> ViewModelPtr = EditorViewModel.Pin();
		const TSharedPtr<FDatabaseAssetTreeNode> TreeNodePtr = WeakAssetTreeNode.Pin();

		ViewModelPtr->GetPoseSearchDatabase()->Modify();
		
		ViewModelPtr->SetIsEnabled(TreeNodePtr->SourceAssetIdx, NewCheckboxState == ECheckBoxState::Checked ? true : false);

		SkeletonView.Pin()->RefreshTreeView(false, true);
		ViewModelPtr->BuildSearchIndex();
	}

	FSlateColor SDatabaseAssetListItem::GetNameTextColorAndOpacity() const
	{
		return GetAssetEnabledChecked() == ECheckBoxState::Checked ? FLinearColor::White : DisabledColor;
	}

	FSlateColor SDatabaseAssetListItem::GetLoopingColorAndOpacity() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		return Node->IsLooping() ? FLinearColor::White : DisabledColor;
	}

	FText SDatabaseAssetListItem::GetLoopingToolTip() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		return Node->IsLooping() ? LOCTEXT("NodeLoopEnabledToolTip", "Looping") : LOCTEXT("NodeLoopDisabledToolTip", "Not looping");
	}

	FSlateColor SDatabaseAssetListItem::GetRootMotionColorAndOpacity() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		return Node->IsRootMotionEnabled() ? FLinearColor::White : DisabledColor;
	}

	FText SDatabaseAssetListItem::GetRootMotionOptionToolTip() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		return Node->IsRootMotionEnabled() ? LOCTEXT("NodeRootMotionEnabledToolTip", "Root motion enabled") : LOCTEXT("NodeRootMotionDisabledToolTip", "No root motion enabled");

	}
	const FSlateBrush* SDatabaseAssetListItem::GetMirrorOptionSlateBrush() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();

		// TODO: Update icons when appropriate assets become available.
		switch (Node->GetMirrorOption())
		{
			case EPoseSearchMirrorOption::UnmirroredOnly: 
				return FAppStyle::Get().GetBrush("Icons.Minus");
			
			case EPoseSearchMirrorOption::MirroredOnly: 
				return FAppStyle::Get().GetBrush("Icons.Plus");
			
			case EPoseSearchMirrorOption::UnmirroredAndMirrored:
				return FAppStyle::Get().GetBrush("Icons.X");
			
			default:
				return nullptr;
		}
	}

	FText SDatabaseAssetListItem::GetMirrorOptionToolTip() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		return FText::FromString(LOCTEXT("ToolTipMirrorOption", "Mirror Option: ").ToString() + (Node ? UEnum::GetDisplayValueAsText(Node->GetMirrorOption()).ToString() : LOCTEXT("ToolTipMirrorOption_Invalid", "Invalid").ToString()));
	}
	
	FText SDatabaseAssetListItem::GetAssetEnabledToolTip() const
	{
		if (GetAssetEnabledChecked() == ECheckBoxState::Checked)
		{
			return LOCTEXT("DisableAssetTooltip", "Disable this asset in the Pose Search Database.");
		}
		
		return LOCTEXT("EnableAssetTooltip", "Enable this asset in the Pose Search Database.");
	}
}

#undef LOCTEXT_NAMESPACE
