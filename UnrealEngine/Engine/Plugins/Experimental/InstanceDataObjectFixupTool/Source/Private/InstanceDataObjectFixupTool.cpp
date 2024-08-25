// Copyright Epic Games, Inc. All Rights Reserved.

#include "InstanceDataObjectFixupTool.h"

#include "InstanceDataObjectFixupPanel.h"
#include "DetailTreeNode.h"
#include "SlateOptMacros.h"
#include "PropertyEditorModule.h"
#include "SDetailsSplitter.h"
#include "Widgets/Layout/LinkableScrollBar.h"
#include "Widgets/Input/SButton.h"


#define LOCTEXT_NAMESPACE "InstanceDataObjectFixupTool"

class FInstanceDataObjectFixupSpecification : public TTreeDiffSpecification<TWeakPtr<FDetailTreeNode>>
{
public:
	using Super = TTreeDiffSpecification<TWeakPtr<FDetailTreeNode>>;
	
	FInstanceDataObjectFixupSpecification(const TSharedPtr<FInstanceDataObjectFixupPanel>& InLeftPanel, const TSharedPtr<FInstanceDataObjectFixupPanel>& InRightPanel)
		: LeftPanel(InLeftPanel)
		, RightPanel(InRightPanel)
	{}

	// when diffing, use the redirects to match properties so that renames are respected
	virtual bool AreMatching(const TWeakPtr<FDetailTreeNode>& TreeNodeA, const TWeakPtr<FDetailTreeNode>& TreeNodeB) const override
	{
		const TSharedPtr<FDetailTreeNode> PinnedTreeNodeA = TreeNodeA.Pin();
		const TSharedPtr<FDetailTreeNode> PinnedTreeNodeB = TreeNodeB.Pin();
		if (!PinnedTreeNodeA || !PinnedTreeNodeB)
		{
			return PinnedTreeNodeA == PinnedTreeNodeB;
		}

		const TSharedPtr<IPropertyHandle> PropertyHandleA = PinnedTreeNodeA->CreatePropertyHandle();
		const TSharedPtr<IPropertyHandle> PropertyHandleB = PinnedTreeNodeB->CreatePropertyHandle();
		if (!PropertyHandleA || !PropertyHandleB)
		{
			// top level category nodes
			return PinnedTreeNodeA->GetNodeName() == PinnedTreeNodeB->GetNodeName();
		}
		
		if (PropertyHandleA->IsCategoryHandle() || PropertyHandleB->IsCategoryHandle())
		{
			// category nodes
			return PropertyHandleA->GetPropertyDisplayName().ToString() == PropertyHandleB->GetPropertyDisplayName().ToString();
		}
		
		return PropertyHandleA->GetProperty() == PropertyHandleB->GetProperty();
	}

	virtual bool ShouldMatchByValue(const TWeakPtr<FDetailTreeNode>& TreeNode) const override
	{
		return false;
	}

	virtual bool ShouldInheritEqualFromChildren(const TWeakPtr<FDetailTreeNode>& TreeNodeA, const TWeakPtr<FDetailTreeNode>& TreeNodeB) const override
	{
		return true;
	}
	
	const TWeakPtr<FInstanceDataObjectFixupPanel> LeftPanel;
	const TWeakPtr<FInstanceDataObjectFixupPanel> RightPanel;
};

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SInstanceDataObjectFixupTool::Construct(const FArguments& InArgs)
{
	Panels[0] = MakeShared<FInstanceDataObjectFixupPanel>(InArgs._InstanceDataObjects, FInstanceDataObjectFixupPanel::EViewFlags::DefaultLeftPanel);
	Panels[1] = MakeShared<FInstanceDataObjectFixupPanel>(InArgs._InstanceDataObjects, FInstanceDataObjectFixupPanel::EViewFlags::DefaultRightPanel);
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			[
				SNew(SButton)
				.Text(LOCTEXT("AutoMarkForDeletion", "Mark remaining conflicts for deletion"))
				.OnClicked(this, &SInstanceDataObjectFixupTool::OnAutoMarkForDeletion)
			]
		]
		+SVerticalBox::Slot()
		[
			SAssignNew(Splitter, SDetailsSplitter)
			.RowHighlightColor_Static(&SInstanceDataObjectFixupTool::GetRowHighlightColor)
		]
		+SVerticalBox::Slot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.AutoHeight()
		[
			SNew(SButton)
			.Text(LOCTEXT("ConfirmButton","Confirm"))
			.OnClicked(this, &SInstanceDataObjectFixupTool::OnConfirmClicked)
			.IsEnabled(this, &SInstanceDataObjectFixupTool::IsResolved)
		]
	];
}

void SInstanceDataObjectFixupTool::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	PanelDiff->Tick();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SInstanceDataObjectFixupTool::SetDockTab(const TSharedRef<SDockTab>& DockTab)
{
	OwningDockTab = DockTab;
}

FReply SInstanceDataObjectFixupTool::OnConfirmClicked() const
{
	if (const TSharedPtr<SDockTab> DockTab = OwningDockTab.Pin())
	{
		DockTab->RequestCloseTab();
	}
	return FReply::Handled();
}

void SInstanceDataObjectFixupTool::GenerateDetailsViews()
{
	constexpr int32 LeftIndex = 0;
	constexpr int32 RightIndex = 1;
	
	// generate the panels
	Panels[LeftIndex]->GenerateDetailsView(true);
	Panels[RightIndex]->GenerateDetailsView(false);

	// diff the panels
	PanelDiff = MakeShared<FAsyncDetailViewDiff>(
		Panels[LeftIndex]->DetailsView.ToSharedRef(), Panels[RightIndex]->DetailsView.ToSharedRef());
	PanelDiff->SetDiffSpecification<FInstanceDataObjectFixupSpecification>(Panels[LeftIndex], Panels[RightIndex]);
    
	SLinkableScrollBar::LinkScrollBars(Panels[LeftIndex]->LinkableScrollBar.ToSharedRef(), Panels[RightIndex]->LinkableScrollBar.ToSharedRef(),
		TAttribute<TArray<FVector2f>>::CreateRaw(PanelDiff.Get(), &FAsyncDetailViewDiff::GenerateScrollSyncRate)); // TODO: Make work w/ CreateShared
	Panels[LeftIndex]->SetDiffAgainstRight(PanelDiff);
	Panels[RightIndex]->SetDiffAgainstLeft(PanelDiff);

	// add the panels to the splitter
	for (TSharedPtr<FInstanceDataObjectFixupPanel> Panel : Panels)
	{
		Splitter->AddSlot(
			SDetailsSplitter::Slot()
			.DetailsView(Panel->DetailsView.ToSharedRef())
			.DifferencesWithRightPanel(Panel.Get(), &FInstanceDataObjectFixupPanel::GetDiffAgainstRight)
			.IsReadonly_Lambda([Panel = Panel.ToWeakPtr()](){return Panel.IsValid() ? Panel.Pin()->HasViewFlag(FInstanceDataObjectFixupPanel::EViewFlags::ReadonlyValues) : true;})
			.ShouldIgnoreRow(Panel.Get(), &FInstanceDataObjectFixupPanel::ShouldSplitterIgnoreRow)
		);
	}
}

FLinearColor SInstanceDataObjectFixupTool::GetRowHighlightColor(const TUniquePtr<FAsyncDetailViewDiff::DiffNodeType>& DiffNode)
{
	switch (DiffNode->DiffResult)
	{
	case ETreeDiffResult::MissingFromTree1:
		// adds are green
		return FLinearColor(0.f, 1.f, 0.f, .5f);
	case ETreeDiffResult::MissingFromTree2:
		// removes/conflicts are over-saturated red
		return FLinearColor(1.5f, 0.3f, 0.3f, 1.f);
	case ETreeDiffResult::DifferentValues:
		return FLinearColor(0.f, 1.f, 1.f, .8f);
	case ETreeDiffResult::Identical:
		return FLinearColor();
	default:
		check(false);
		return FLinearColor();
	}
}

bool SInstanceDataObjectFixupTool::IsResolved() const
{
	for (const TSharedPtr<FInstanceDataObjectFixupPanel>& Panel : Panels)
	{
		if (!Panel->AreAllConflictsRedirected())
		{
			return false;
		}
	}
	return true;
}

FReply SInstanceDataObjectFixupTool::OnAutoMarkForDeletion() const
{
	for (const TSharedPtr<FInstanceDataObjectFixupPanel>& Panel : Panels)
	{
		Panel->AutoApplyMarkDeletedActions();
	}
	return FReply::Handled();
}


#undef LOCTEXT_NAMESPACE

