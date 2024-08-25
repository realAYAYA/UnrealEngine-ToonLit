// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseAssetListItem.h"

#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "AnimPreviewInstance.h"
#include "AssetSelection.h"
#include "ClassIconFinder.h"
#include "DetailColumnSizeData.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Editor.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IAnimationEditor.h"
#include "IPersonaToolkit.h"
#include "Misc/FeedbackContext.h"
#include "Misc/TransactionObjectEvent.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearchDatabaseAssetTree.h"
#include "PoseSearchDatabaseViewModel.h"
#include "ScopedTransaction.h"
#include "SPositiveActionButton.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SRichTextBlock.h"

#define LOCTEXT_NAMESPACE "SDatabaseAssetListItem"

namespace UE::PoseSearch
{
	static constexpr FLinearColor DisabledColor = FLinearColor(1.f, 1.f, 1.f, 0.25f);
	
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

		if (InAssetTreeNode->SourceAssetIdx == INDEX_NONE)
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

	void SDatabaseAssetListItem::OnAddAnimMontage()
	{
		EditorViewModel.Pin()->AddAnimMontageToDatabase(nullptr);
		SkeletonView.Pin()->RefreshTreeView(false);
	}

	FReply SDatabaseAssetListItem::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
	{
		TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();
		if (const UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase())
		{
			if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetAnimationAssetBase(Node->SourceAssetIdx))
			{
				if (UAssetEditorSubsystem* AssetEditorSS = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
				{
					if (UObject* AnimationAsset = DatabaseAnimationAsset->GetAnimationAsset())
					{
						AssetEditorSS->OpenEditorForAsset(AnimationAsset);

						if (IAssetEditorInstance* Editor = AssetEditorSS->FindEditorForAsset(AnimationAsset, true))
						{
							if (Editor->GetEditorName() == "AnimationEditor")
							{
								float AnimationAssetTime = 0.f;
								FVector AnimationAssetBlendParameters = FVector::ZeroVector;
								ViewModel->GetAnimationTime(Node->SourceAssetIdx, AnimationAssetTime, AnimationAssetBlendParameters);

								const IAnimationEditor* AnimationEditor = static_cast<IAnimationEditor*>(Editor);
								const UDebugSkelMeshComponent* PreviewComponent = AnimationEditor->GetPersonaToolkit()->GetPreviewMeshComponent();

								// Open asset paused and at specific time as seen on the pose search debugger.
								PreviewComponent->PreviewInstance->SetPosition(AnimationAssetTime);
								PreviewComponent->PreviewInstance->SetPlaying(false);
								PreviewComponent->PreviewInstance->SetBlendSpacePosition(AnimationAssetBlendParameters);
							}
						}
					}
				}
			}
		}

		return STableRow<TSharedPtr<FDatabaseAssetTreeNode>>::OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
	}

	FText SDatabaseAssetListItem::GetName() const
	{
		TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();

		if (const UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase())
		{
			TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
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
		
		TSharedPtr<SWidget> ItemWidget;
		const FDetailColumnSizeData& ColumnSizeData = SkeletonView.Pin()->GetColumnSizeData();
		
		if (Node->SourceAssetIdx == INDEX_NONE)
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
			];
		}
		else
		{
			// Item Icon
			TSharedPtr<SImage> ItemIconWidget;
			TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();
			if (UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase())
			{
				if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetAnimationAssetBase(Node->SourceAssetIdx))
				{
					SAssignNew(ItemIconWidget, SImage)
						.Image(FSlateIconFinder::FindIconBrushForClass(DatabaseAnimationAsset->GetAnimationAssetStaticClass()));
				}
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
					.Padding(1.0f, 2.0f)
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(FAppStyle::Get().GetBrush("AnimGraph.Attribute.RootMotionDelta.Icon"))
						.DesiredSizeOverride(FVector2D{16.f, 16.f})
						.ColorAndOpacity(this, &SDatabaseAssetListItem::GetRootMotionColorAndOpacity)
						.ToolTipText(this, &SDatabaseAssetListItem::GetRootMotionOptionToolTip)
					]
					
					// Mirror Type
					+ SHorizontalBox::Slot()
					.Padding(2.0f, 3.0f)
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(this, &SDatabaseAssetListItem::GetMirrorOptionSlateBrush)
						.ToolTipText(this, &SDatabaseAssetListItem::GetMirrorOptionToolTip)
						.OnMouseButtonDown(this, &SDatabaseAssetListItem::MirrorOptionOnMouseButtonDown)
					]

					// Disable Reselection
					+ SHorizontalBox::Slot()
					.Padding(4.0f, 1.0f)
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SCheckBox)
						.IsChecked(this, &SDatabaseAssetListItem::GetDisableReselectionChecked)
						.OnCheckStateChanged(const_cast<SDatabaseAssetListItem*>(this), &SDatabaseAssetListItem::OnDisableReselectionChanged)
						.ToolTipText(this, &SDatabaseAssetListItem::GetDisableReselectionToolTip)
						.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
						.CheckedImage(FAppStyle::Get().GetBrush("MotionMatchingEditor.DisablePoseReselection"))
						.CheckedHoveredImage(FAppStyle::Get().GetBrush("MotionMatchingEditor.DisablePoseReselection"))
						.CheckedPressedImage(FAppStyle::Get().GetBrush("MotionMatchingEditor.DisablePoseReselection"))
						.UncheckedImage(FAppStyle::Get().GetBrush("MotionMatchingEditor.EnablePoseReselection"))
						.UncheckedHoveredImage(FAppStyle::Get().GetBrush("MotionMatchingEditor.EnablePoseReselection"))
						.UncheckedPressedImage(FAppStyle::Get().GetBrush("MotionMatchingEditor.EnablePoseReselection"))
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
		if (const FSearchIndexAsset* SelectedIndexAsset = ViewModelPtr->GetSelectedActorIndexAsset())
		{
			if (TreeNodePtr->SourceAssetIdx == SelectedIndexAsset->GetSourceAssetIdx())
			{
				return EVisibility::Visible;
			}
		}

		return EVisibility::Hidden;
	}

	FText SDatabaseAssetListItem::GetDisableReselectionToolTip() const
	{
		if (GetDisableReselectionChecked() == ECheckBoxState::Checked)
		{
			return LOCTEXT("EnableReselectionToolTip", "Reselection of poses from the same asset is disabled.");
		}
		
		return LOCTEXT("DisableReselectionToolTip", "Reselection of poses from the same asset is enabled.");
	}

	ECheckBoxState SDatabaseAssetListItem::GetDisableReselectionChecked() const
	{
		TSharedPtr<FDatabaseViewModel> ViewModelPtr = EditorViewModel.Pin();
		if (const UPoseSearchDatabase* Database = ViewModelPtr->GetPoseSearchDatabase())
		{
			TSharedPtr<FDatabaseAssetTreeNode> TreeNodePtr = WeakAssetTreeNode.Pin();
			if (Database->GetAnimationAssets().IsValidIndex(TreeNodePtr->SourceAssetIdx))
			{
				if (ViewModelPtr->IsDisableReselection(TreeNodePtr->SourceAssetIdx))
				{
					return ECheckBoxState::Checked;
				}
			}
		}

		return ECheckBoxState::Unchecked;
	}

	void SDatabaseAssetListItem::OnDisableReselectionChanged(ECheckBoxState NewCheckboxState)
	{
		const TSharedPtr<FDatabaseViewModel> ViewModelPtr = EditorViewModel.Pin();
		if (UPoseSearchDatabase* PoseSearchDatabase = ViewModelPtr->GetPoseSearchDatabase())
		{
			const FScopedTransaction Transaction(LOCTEXT("EnableChangedForAssetInPoseSearchDatabase", "Update enabled flag for item from Pose Search Database"));

			const TSharedPtr<FDatabaseAssetTreeNode> TreeNodePtr = WeakAssetTreeNode.Pin();

			PoseSearchDatabase->Modify();

			ViewModelPtr->SetDisableReselection(TreeNodePtr->SourceAssetIdx, NewCheckboxState == ECheckBoxState::Checked ? true : false);

			SkeletonView.Pin()->RefreshTreeView(false, true);

			// no need to rebuild the SearchIndex (ViewModelPtr->BuildSearchIndex()), since bDisableReselection is a runtime only parameter
		}
	}

	ECheckBoxState SDatabaseAssetListItem::GetAssetEnabledChecked() const
	{
		TSharedPtr<FDatabaseViewModel> ViewModelPtr = EditorViewModel.Pin();
		if (const UPoseSearchDatabase* Database = ViewModelPtr->GetPoseSearchDatabase())
		{
			TSharedPtr<FDatabaseAssetTreeNode> TreeNodePtr = WeakAssetTreeNode.Pin();
			if (Database->GetAnimationAssets().IsValidIndex(TreeNodePtr->SourceAssetIdx))
			{
				if (ViewModelPtr->IsEnabled(TreeNodePtr->SourceAssetIdx))
				{
					return ECheckBoxState::Checked;
				}
			}
		}
		return ECheckBoxState::Unchecked;
	}

	void SDatabaseAssetListItem::OnAssetIsEnabledChanged(ECheckBoxState NewCheckboxState)
	{
		const FScopedTransaction Transaction(LOCTEXT("EnableChangedForAssetInPoseSearchDatabase", "Update enabled flag for item from Pose Search Database"));

		const TSharedPtr<FDatabaseViewModel> ViewModelPtr = EditorViewModel.Pin();
		const TSharedPtr<FDatabaseAssetTreeNode> TreeNodePtr = WeakAssetTreeNode.Pin();

		ViewModelPtr->SetIsEnabled(TreeNodePtr->SourceAssetIdx, NewCheckboxState == ECheckBoxState::Checked);

		SkeletonView.Pin()->RefreshTreeView(false, true);
		ViewModelPtr->BuildSearchIndex();
	}

	FSlateColor SDatabaseAssetListItem::GetNameTextColorAndOpacity() const
	{
		TSharedPtr<FDatabaseViewModel> ViewModelPtr = EditorViewModel.Pin();
		if (const UPoseSearchDatabase* Database = ViewModelPtr->GetPoseSearchDatabase())
		{
			TSharedPtr<FDatabaseAssetTreeNode> TreeNodePtr = WeakAssetTreeNode.Pin();
			if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database->GetAnimationAssetBase(TreeNodePtr->SourceAssetIdx))
			{
				if (DatabaseAnimationAssetBase->IsEnabled())
				{
					if (DatabaseAnimationAssetBase->bSynchronizeWithExternalDependency)
					{
						return FColor::Turquoise;
					}

					return FLinearColor::White;
				}
			}
		}
		return DisabledColor;
	}

	FSlateColor SDatabaseAssetListItem::GetLoopingColorAndOpacity() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		return Node->IsLooping() ? FLinearColor::White : DisabledColor;
	}

	FText SDatabaseAssetListItem::GetLoopingToolTip() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		return Node->IsLooping() ? LOCTEXT("NodeLoopEnabledToolTip", "Looping (Read only)") : LOCTEXT("NodeLoopDisabledToolTip", "Not looping (Read only)");
	}

	FSlateColor SDatabaseAssetListItem::GetRootMotionColorAndOpacity() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		return Node->IsRootMotionEnabled() ? FLinearColor::White : DisabledColor;
	}

	FText SDatabaseAssetListItem::GetRootMotionOptionToolTip() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		return Node->IsRootMotionEnabled() ? LOCTEXT("NodeRootMotionEnabledToolTip", "Root motion enabled (Read only)") : LOCTEXT("NodeRootMotionDisabledToolTip", "No root motion enabled (Read only)");

	}
	const FSlateBrush* SDatabaseAssetListItem::GetMirrorOptionSlateBrush() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();

		// TODO: Update icons when appropriate assets become available.
		switch (Node->GetMirrorOption())
		{
			case EPoseSearchMirrorOption::UnmirroredOnly: 
				return FAppStyle::Get().GetBrush("GraphEditor.AlignNodesRight");
			
			case EPoseSearchMirrorOption::MirroredOnly: 
				return FAppStyle::Get().GetBrush("GraphEditor.AlignNodesLeft");
			
			case EPoseSearchMirrorOption::UnmirroredAndMirrored:
				return FAppStyle::Get().GetBrush("GraphEditor.AlignNodesCenter");
			
			default:
				return nullptr;
		}
	}

	FText SDatabaseAssetListItem::GetMirrorOptionToolTip() const
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		return FText::FromString(LOCTEXT("ToolTipMirrorOption", "Mirror Option: ").ToString() + (Node ? UEnum::GetDisplayValueAsText(Node->GetMirrorOption()).ToString() : LOCTEXT("ToolTipMirrorOption_Invalid", "Invalid").ToString()));
	}

	FReply SDatabaseAssetListItem::MirrorOptionOnMouseButtonDown(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
	{
		const TSharedPtr<FDatabaseAssetTreeNode> Node = WeakAssetTreeNode.Pin();
		const TSharedPtr<FDatabaseViewModel> ViewModel = EditorViewModel.Pin();

		if(InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			if (UPoseSearchDatabase* Database = ViewModel->GetPoseSearchDatabase())
			{
				if (FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = Database->GetMutableAnimationAssetBase(Node->SourceAssetIdx))
				{
					const FScopedTransaction Transaction(LOCTEXT("OnClickEditMirrorOptionPoseSearchDatabase", "Edit Mirror Option"));
				
					// Get next mirror option
					static const TArray<EPoseSearchMirrorOption> OptionArray = { EPoseSearchMirrorOption::UnmirroredOnly, EPoseSearchMirrorOption::MirroredOnly, EPoseSearchMirrorOption::UnmirroredAndMirrored };
					const int32 NextOption = (static_cast<int32>(DatabaseAnimationAsset->MirrorOption) + 1) % OptionArray.Num();

					// Modify asset (@todo: this should be done through the viewmodel).
					Database->Modify();
					DatabaseAnimationAsset->MirrorOption = OptionArray[NextOption];
				
					SkeletonView.Pin()->RefreshTreeView(false, true);
					ViewModel->BuildSearchIndex();
				
					return FReply::Handled();
				}
			}
		}
		
		return FReply::Unhandled();
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
