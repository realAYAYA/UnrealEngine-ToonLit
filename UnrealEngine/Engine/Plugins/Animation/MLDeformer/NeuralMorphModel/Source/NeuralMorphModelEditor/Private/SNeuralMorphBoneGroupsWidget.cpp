// Copyright Epic Games, Inc. All Rights Reserved.

#include "SNeuralMorphBoneGroupsWidget.h"
#include "NeuralMorphModel.h"
#include "NeuralMorphEditorModel.h"
#include "NeuralMorphInputInfo.h"
#include "SNeuralMorphInputWidget.h"
#include "SMLDeformerBonePickerDialog.h"
#include "MLDeformerEditorStyle.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Engine/SkeletalMesh.h"

#define LOCTEXT_NAMESPACE "NeuralMorphBoneGroupsWidget"

namespace UE::NeuralMorphModel
{
	FNeuralMorphBoneGroupsCommands::FNeuralMorphBoneGroupsCommands()
		: TCommands<FNeuralMorphBoneGroupsCommands>
	(	"Neural Morph Bone Groups",
		NSLOCTEXT("NeuralMorphBoneGroupsWidget", "NeuralMorphBoneGroupsDesc", "Neural Morph Bone Groups"),
		NAME_None,
		FMLDeformerEditorStyle::Get().GetStyleSetName())
	{
	}

	void FNeuralMorphBoneGroupsCommands::RegisterCommands()
	{
		UI_COMMAND(CreateGroup, "Create New Group", "Create a new bone group.", EUserInterfaceActionType::Button, FInputChord(EKeys::Insert));
		UI_COMMAND(DeleteSelectedItems, "Delete Selected Items", "Deletes the selected bones and/or groups.", EUserInterfaceActionType::Button, FInputChord(EKeys::Delete));
		UI_COMMAND(ClearGroups, "Clear All Groups", "Clears the entire list of bone groups.", EUserInterfaceActionType::Button, FInputChord());
		UI_COMMAND(AddBoneToGroup, "Add Bones To Group", "Add new bones to the group.", EUserInterfaceActionType::Button, FInputChord());
	}

	void SNeuralMorphBoneGroupsWidget::BindCommands(TSharedPtr<FUICommandList> CommandList)
	{
		const FNeuralMorphBoneGroupsCommands& GroupCommands = FNeuralMorphBoneGroupsCommands::Get();
		CommandList->MapAction(GroupCommands.CreateGroup, FExecuteAction::CreateSP(this, &SNeuralMorphBoneGroupsWidget::OnCreateBoneGroup));
		CommandList->MapAction(GroupCommands.DeleteSelectedItems, FExecuteAction::CreateSP(this, &SNeuralMorphBoneGroupsWidget::OnDeleteSelectedItems));
		CommandList->MapAction(GroupCommands.ClearGroups, FExecuteAction::CreateSP(this, &SNeuralMorphBoneGroupsWidget::OnClearBoneGroups));
		CommandList->MapAction(GroupCommands.AddBoneToGroup, FExecuteAction::CreateSP(this, &SNeuralMorphBoneGroupsWidget::OnAddBoneToGroup));
	}

	void SNeuralMorphBoneGroupsWidget::Construct(const FArguments& InArgs)
	{
		EditorModel = InArgs._EditorModel;
		InputWidget = InArgs._InputWidget;

		STreeView<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>>::FArguments SuperArgs;
		SuperArgs.TreeItemsSource(&RootElements);
		SuperArgs.SelectionMode(ESelectionMode::Multi);
		SuperArgs.OnGenerateRow(this, &SNeuralMorphBoneGroupsWidget::MakeTableRowWidget);
		SuperArgs.OnGetChildren(this, &SNeuralMorphBoneGroupsWidget::HandleGetChildrenForTree);
		SuperArgs.OnSelectionChanged(this, &SNeuralMorphBoneGroupsWidget::OnSelectionChanged);
		SuperArgs.OnContextMenuOpening(this, &SNeuralMorphBoneGroupsWidget::CreateContextMenuWidget);
		SuperArgs.HighlightParentNodesForSelection(false);
		SuperArgs.AllowInvisibleItemSelection(true);  // Without this we deselect everything when we filter or we collapse.

		STreeView<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>>::Construct(SuperArgs);

		RefreshTree(false);
	}

	void SNeuralMorphBoneGroupsWidget::OnSelectionChanged(TSharedPtr<FNeuralMorphBoneGroupsTreeElement> Selection, ESelectInfo::Type SelectInfo)
	{
		const TSharedPtr<SNeuralMorphInputWidget> NeuralInputWidget = StaticCastSharedPtr<SNeuralMorphInputWidget>(InputWidget);
		if (NeuralInputWidget.IsValid())
		{
			NeuralInputWidget->OnSelectInputBoneGroup(Selection);
		}
	}

	TSharedPtr<SWidget> SNeuralMorphBoneGroupsWidget::CreateContextMenuWidget() const
	{
		const FNeuralMorphBoneGroupsCommands& Actions = FNeuralMorphBoneGroupsCommands::Get();

		TSharedPtr<FUICommandList> CommandList = StaticCastSharedPtr<SNeuralMorphInputWidget>(InputWidget)->GetBoneGroupsCommandList();
		FMenuBuilder Menu(true, CommandList);

		const TArray<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>> CurSelectedItems = GetSelectedItems();
		Menu.BeginSection("BoneGroupActions", LOCTEXT("BoneGroupActionsHeading", "Bone Group Actions"));
		{
			if (CurSelectedItems.Num() == 1 && CurSelectedItems[0]->IsGroup())
			{
				Menu.AddMenuEntry(Actions.AddBoneToGroup);
			}

			if (!CurSelectedItems.IsEmpty())
			{			
				Menu.AddMenuEntry(Actions.DeleteSelectedItems);
			}

			if (!GetRootItems().IsEmpty())
			{
				Menu.AddMenuEntry(Actions.ClearGroups);
			}
		}
		Menu.EndSection();

		// Add the bone mask settings.
		TSharedPtr<SNeuralMorphInputWidget> NeuralInputWidget = StaticCastSharedPtr<SNeuralMorphInputWidget>(InputWidget);
		if (NeuralInputWidget.IsValid())
		{
			NeuralInputWidget->AddInputBoneGroupsMenuItems(Menu);
		}

		return Menu.MakeWidget();
	}

	void SNeuralMorphBoneGroupsWidget::OnCreateBoneGroup()
	{
		using namespace UE::MLDeformer;

		USkeletalMesh* SkelMesh = EditorModel->GetModel()->GetSkeletalMesh();
		if (!SkelMesh)
		{
			return;
		}

		// Find the group we want to add something to.
		UNeuralMorphModel* NeuralMorphModel = EditorModel->GetNeuralMorphModel();

		const UNeuralMorphInputInfo* InputInfo = Cast<const UNeuralMorphInputInfo>(EditorModel->GetEditorInputInfo());
		check(InputInfo);

		TSharedPtr<SMLDeformerBonePickerDialog> Dialog = 
			SNew(SMLDeformerBonePickerDialog)
			.RefSkeleton(&SkelMesh->GetRefSkeleton())
			.AllowMultiSelect(true)
			.IncludeList(InputInfo->GetBoneNames())
			.ExtraWidget(InputWidget->GetExtraBonePickerWidget());

		Dialog->ShowModal();

		const TArray<FName>& BoneNames = Dialog->GetPickedBoneNames();
		if (BoneNames.IsEmpty())
		{
			return;
		}

		NeuralMorphModel->BoneGroups.AddDefaulted();
		FNeuralMorphBoneGroup& BoneGroup = NeuralMorphModel->BoneGroups.Last();
		BoneGroup.GroupName = EditorModel->GenerateUniqueBoneGroupName();
		for (const FName BoneName : BoneNames)
		{
			BoneGroup.BoneNames.AddUnique(BoneName);
		}

		RefreshTree(true);
		EditorModel->RebuildEditorMaskInfo();
	}

	void SNeuralMorphBoneGroupsWidget::OnDeleteSelectedItems()
	{
		const TArray<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>> CurSelectedItems = GetSelectedItems();

		TArray<int32> GroupsToRemove;

		TArray<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>> SelectedGroups;
		TArray<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>> SelectedBones;

		// Check if the selection contains bones and/or groups.
		for (const TSharedPtr<FNeuralMorphBoneGroupsTreeElement>& SelectedItem : CurSelectedItems)
		{
			if (SelectedItem->IsGroup())
			{
				SelectedGroups.Add(SelectedItem);
				GroupsToRemove.Add(SelectedItem->GroupIndex);
			}
			else
			{
				SelectedBones.Add(SelectedItem);
			}
		}

		UNeuralMorphModel* NeuralMorphModel = EditorModel->GetNeuralMorphModel();

		// Remove all selected bones.
		if (!SelectedBones.IsEmpty())
		{
			for (const TSharedPtr<FNeuralMorphBoneGroupsTreeElement>& BoneItem : SelectedBones)
			{
				if (BoneItem.IsValid() && BoneItem->ParentGroup.IsValid())
				{
					const TSharedPtr<FNeuralMorphBoneGroupsTreeElement>& ParentGroup = BoneItem->ParentGroup.Pin();

					if (BoneItem->Name.IsNone())
					{
						// Remove all the none items.
						NeuralMorphModel->BoneGroups[ParentGroup->GroupIndex].BoneNames.RemoveAll(
							[](const FBoneReference& Item) 
							{
								return Item.BoneName.IsNone();
							});
					}
					else
					{
						NeuralMorphModel->BoneGroups[ParentGroup->GroupIndex].BoneNames.Remove(BoneItem->Name);
						FNeuralMorphMaskInfo* MaskInfo = NeuralMorphModel->BoneGroupMaskInfos.Find(ParentGroup->Name);
						if (MaskInfo)
						{
							MaskInfo->BoneNames.Remove(BoneItem->Name);
						}
					}
				}
			}
		}

		// Sort group indices big to small (back to front).
		if (!GroupsToRemove.IsEmpty())
		{
			GroupsToRemove.Sort([](const int32& A, const int32& B){ return A > B; });

			// Remove the items, back to front.
			for (const int32 Index : GroupsToRemove)
			{
				NeuralMorphModel->BoneGroupMaskInfos.Remove(NeuralMorphModel->BoneGroups[Index].GroupName);
				NeuralMorphModel->BoneGroups.RemoveAt(Index);
			}
		}

		RefreshTree(true);
		EditorModel->RebuildEditorMaskInfo();
	}

	void SNeuralMorphBoneGroupsWidget::OnClearBoneGroups()
	{
		UNeuralMorphModel* NeuralMorphModel = EditorModel->GetNeuralMorphModel();
		NeuralMorphModel->BoneGroups.Empty();
		NeuralMorphModel->BoneGroupMaskInfos.Empty();
		RefreshTree(true);
		EditorModel->RebuildEditorMaskInfo();
	}

	void SNeuralMorphBoneGroupsWidget::OnAddBoneToGroup()
	{
		using namespace UE::MLDeformer;

		USkeletalMesh* SkelMesh = EditorModel->GetModel()->GetSkeletalMesh();
		if (!SkelMesh)
		{
			return;
		}

		// Find the group we want to add something to.
		check(GetSelectedItems().Num() == 1);
		const int32 GroupIndex = GetSelectedItems()[0]->GroupIndex;
		check(GroupIndex != INDEX_NONE);
		UNeuralMorphModel* NeuralMorphModel = EditorModel->GetNeuralMorphModel();
		FNeuralMorphBoneGroup& BoneGroup = NeuralMorphModel->BoneGroups[GroupIndex];

		// Build the highlighted bone names list.
		TArray<FName> HighlightedBones;
		HighlightedBones.Reserve(BoneGroup.BoneNames.Num());
		for (const FBoneReference& BoneRef : BoneGroup.BoneNames)
		{
			HighlightedBones.Add(BoneRef.BoneName);
		}

		UNeuralMorphInputInfo* InputInfo = Cast<UNeuralMorphInputInfo>(EditorModel->GetEditorInputInfo());
		check(InputInfo);

		const FLinearColor HighlightColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.InputsWidget.HighlightColor");
		TSharedPtr<SMLDeformerBonePickerDialog> Dialog = 
			SNew(SMLDeformerBonePickerDialog)
			.RefSkeleton(&SkelMesh->GetRefSkeleton())
			.AllowMultiSelect(true)
			.HighlightBoneNames(HighlightedBones)
			.HighlightBoneNamesColor(HighlightColor)
			.IncludeList(InputInfo->GetBoneNames())
			.ExtraWidget(InputWidget->GetExtraBonePickerWidget());

		Dialog->ShowModal();

		const TArray<FName>& BoneNames = Dialog->GetPickedBoneNames();
		if (!BoneNames.IsEmpty())
		{
			for (const FName BoneName : BoneNames)
			{
				BoneGroup.BoneNames.AddUnique(BoneName);
			}

			USkeletalMesh* SkeletalMesh = NeuralMorphModel->GetSkeletalMesh();	
			const FReferenceSkeleton& RefSkel = SkeletalMesh->GetRefSkeleton();
			const TArray<FName>& EditorInfoBoneNames = InputInfo->GetBoneNames();
			const TArray<int32> VirtualParentTable = EditorModel->BuildVirtualParentTable(RefSkel, EditorInfoBoneNames);	
			const int32 HierarchyDepth = StaticCastSharedPtr<SNeuralMorphInputWidget>(InputWidget)->GetHierarchyDepth();

			// Build a list of bone indices that need to be added.
			// These can be more than just the picked bones, as we grow the mask based on a hierarchy depth as well.
			TArray<int32> NewMaskBones;
			for (const FName BoneName : BoneNames)
			{
				const int32 SkeletonBoneIndex = RefSkel.FindBoneIndex(BoneName);
				if (SkeletonBoneIndex == INDEX_NONE)
				{
					UE_LOG(LogNeuralMorphModel, Warning, TEXT("Bone '%s' cannot be found in the SkeletalMesh '%s', ignoring the bone inside the group's mask."), *BoneName.ToString(), *SkeletalMesh->GetName());
					continue;
				}

				EditorModel->RecursiveAddBoneToMaskUpwards(RefSkel, SkeletonBoneIndex, HierarchyDepth, VirtualParentTable, NewMaskBones);
				EditorModel->RecursiveAddBoneToMaskDownwards(RefSkel, SkeletonBoneIndex, HierarchyDepth, VirtualParentTable, NewMaskBones);
				EditorModel->AddRequiredBones(RefSkel, SkeletonBoneIndex, VirtualParentTable, NewMaskBones);
			}

			// Add the new bones to the mask.
			FNeuralMorphMaskInfo* MaskInfo = NeuralMorphModel->BoneGroupMaskInfos.Find(BoneGroup.GroupName);
			if (MaskInfo != nullptr)
			{
				for (const int32 BoneIndex : NewMaskBones)
				{
					const FName BoneName = RefSkel.GetBoneName(BoneIndex);
					if (BoneName.IsValid() && !BoneName.IsNone())
					{
						MaskInfo->BoneNames.AddUnique(BoneName);
					}
				}
			}

			RefreshTree(true);
			EditorModel->RebuildEditorMaskInfo();
		}
	}

	int32 SNeuralMorphBoneGroupsWidget::GetNumSelectedGroups() const
	{
		int32 NumSelectedGroups = 0;
		for (const TSharedPtr<FNeuralMorphBoneGroupsTreeElement>& Item : SelectedItems)
		{
			if (Item->IsGroup())
			{
				NumSelectedGroups++;
			}
		}
		return NumSelectedGroups;
	}

	void SNeuralMorphBoneGroupsWidget::AddElement(TSharedPtr<FNeuralMorphBoneGroupsTreeElement> Element, TSharedPtr<FNeuralMorphBoneGroupsTreeElement> ParentElement)
	{
		if (!ParentElement)
		{
			RootElements.Add(Element);
		}
		else
		{
			ParentElement->Children.Add(Element);
			Element->ParentGroup = ParentElement->AsWeak();
		}
	}

	TSharedRef<ITableRow> FNeuralMorphBoneGroupsTreeElement::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FNeuralMorphBoneGroupsTreeElement> InTreeElement, TSharedPtr<SNeuralMorphBoneGroupsWidget> InTreeWidget)
	{
		return SNew(SNeuralMorphBoneGroupsTreeRowWidget, InOwnerTable, InTreeElement, InTreeWidget);
	}

	TSharedRef<ITableRow> SNeuralMorphBoneGroupsWidget::MakeTableRowWidget(TSharedPtr<FNeuralMorphBoneGroupsTreeElement> InItem, const TSharedRef<STableViewBase>& OwnerTable)
	{
		return InItem->MakeTreeRowWidget(OwnerTable, InItem.ToSharedRef(), SharedThis(this));
	}

	void SNeuralMorphBoneGroupsWidget::HandleGetChildrenForTree(TSharedPtr<FNeuralMorphBoneGroupsTreeElement> InItem, TArray<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>>& OutChildren)
	{
		OutChildren = InItem.Get()->Children;
	}

	void SNeuralMorphBoneGroupsWidget::UpdateTreeElements()
	{
		RootElements.Reset();

		const FLinearColor ErrorColor = FMLDeformerEditorStyle::Get().GetColor("MLDeformer.InputsWidget.ErrorColor");

		UNeuralMorphModel* NeuralMorphModel = EditorModel->GetNeuralMorphModel();

		const int32 NumGroups = NeuralMorphModel->GetBoneGroups().Num();
		for (int32 BoneGroupIndex = 0; BoneGroupIndex < NumGroups; ++BoneGroupIndex)
		{
			const FNeuralMorphBoneGroup& BoneGroup = NeuralMorphModel->GetBoneGroups()[BoneGroupIndex];
			bool bGroupHasError = false;

			for (const FBoneReference& BoneRef : BoneGroup.BoneNames)
			{
				if (!BoneRef.BoneName.IsValid() || BoneRef.BoneName.IsNone())
				{
					bGroupHasError = true;
					break;
				}
			}

			// Add the group header.
			TSharedPtr<FNeuralMorphBoneGroupsTreeElement> GroupElement = MakeShared<FNeuralMorphBoneGroupsTreeElement>();
			GroupElement->Name = BoneGroup.GroupName;
			GroupElement->TextColor = bGroupHasError ? FSlateColor(ErrorColor) : FSlateColor::UseForeground();
			GroupElement->GroupIndex = BoneGroupIndex;
			AddElement(GroupElement, nullptr);
			SetItemExpansion(GroupElement, true);

			// Add the items in the group.
			for (int32 BoneIndex = 0; BoneIndex < BoneGroup.BoneNames.Num(); ++BoneIndex)
			{
				const FName BoneName = BoneGroup.BoneNames[BoneIndex].BoneName;
				const bool bBoneHasError = !EditorModel->GetEditorInputInfo()->GetBoneNames().Contains(BoneName);

				TSharedPtr<FNeuralMorphBoneGroupsTreeElement> ItemElement = MakeShared<FNeuralMorphBoneGroupsTreeElement>();
				ItemElement->Name = BoneName;
				ItemElement->TextColor = bBoneHasError ? FSlateColor(ErrorColor) : FSlateColor::UseForeground();
				ItemElement->GroupBoneIndex = BoneIndex;
				AddElement(ItemElement, GroupElement);

				bGroupHasError |= bBoneHasError;
			}

			if (bGroupHasError)
			{
				GroupElement->TextColor = FSlateColor(ErrorColor);
			}
		}
	}

	FReply SNeuralMorphBoneGroupsWidget::OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
	{
		TSharedPtr<FUICommandList> CommandList = StaticCastSharedPtr<SNeuralMorphInputWidget>(InputWidget)->GetBoneGroupsCommandList();
		if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
		{
			return FReply::Handled();
		}

		return STreeView<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>>::OnKeyDown(InGeometry, InKeyEvent);
	}

	void SNeuralMorphBoneGroupsWidget::RefreshTree(bool bBroadcastPropertyChanged)
	{
		if (bBroadcastPropertyChanged)
		{
			UNeuralMorphModel* NeuralMorphModel = EditorModel->GetNeuralMorphModel();
			BroadcastModelPropertyChanged(GET_MEMBER_NAME_CHECKED(UNeuralMorphModel, BoneGroups));
		}

		UpdateTreeElements();

		FNeuralMorphEditorModel* NeuralMorphEditorModel = static_cast<FNeuralMorphEditorModel*>(EditorModel);
		SectionTitle = FText::Format(FTextFormat(LOCTEXT("BoneGroupsSectionTitle", "Bone Groups ({0})")), NeuralMorphEditorModel->GetNeuralMorphModel()->BoneGroups.Num());

		// Update the slate widget.
		RequestTreeRefresh();
	}

	TSharedPtr<SWidget> SNeuralMorphBoneGroupsWidget::CreateContextWidget() const
	{
		return TSharedPtr<SWidget>();
	}

	void SNeuralMorphBoneGroupsTreeRowWidget::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FNeuralMorphBoneGroupsTreeElement> InTreeElement, TSharedPtr<SNeuralMorphBoneGroupsWidget> InTreeView)
	{
		WeakTreeElement = InTreeElement;

		STableRow<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>>::Construct
		(
			STableRow<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>>::FArguments()
			.ShowWires(true)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SNeuralMorphBoneGroupsTreeRowWidget::GetName)
				.ColorAndOpacity_Lambda
				(
					[this]()
					{
						return WeakTreeElement.IsValid() ? WeakTreeElement.Pin()->TextColor : FSlateColor::UseForeground();
					}
				)
			], 
			OwnerTable
		);
	}

	FText SNeuralMorphBoneGroupsTreeRowWidget::GetName() const
	{
		if (WeakTreeElement.IsValid())
		{
			return FText::FromName(WeakTreeElement.Pin()->Name);
		}
		return FText();
	}

	bool SNeuralMorphBoneGroupsWidget::BroadcastModelPropertyChanged(const FName PropertyName)
	{
		UMLDeformerModel* Model = EditorModel->GetModel();

		FProperty* Property = Model->GetClass()->FindPropertyByName(PropertyName);
		if (Property == nullptr)
		{
			UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to find property '%s' in class '%s'"), *PropertyName.ToString(), *Model->GetName());
			return false;
		}

		FPropertyChangedEvent Event(Property, EPropertyChangeType::ValueSet);
		Model->PostEditChangeProperty(Event);
		return true;
	}


	void SNeuralMorphBoneGroupsWidget::Refresh()
	{
		RefreshTree(false);
	}

	FText SNeuralMorphBoneGroupsWidget::GetSectionTitle() const
	{ 
		return SectionTitle;
	}

	TSharedPtr<UE::MLDeformer::SMLDeformerInputWidget> SNeuralMorphBoneGroupsWidget::GetInputWidget() const
	{
		return InputWidget;
	}

	const TArray<TSharedPtr<FNeuralMorphBoneGroupsTreeElement>>& SNeuralMorphBoneGroupsWidget::GetRootElements() const
	{
		return RootElements;
	}

}	// namespace UE::NeuralMorphModel

#undef LOCTEXT_NAMESPACE
