// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownSubListDocumentTabFactory.h"

#include "Rundown/AvaRundownEditor.h"
#include "Rundown/Pages/Slate/SAvaRundownInstancedPageList.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "AvaRundownSubListDocumentTabFactory"

const FName FAvaRundownSubListDocumentTabFactory::FactoryId = "AvaSubListTabFactory";
const FString FAvaRundownSubListDocumentTabFactory::BaseTabName(TEXT("AvaSubListDocument"));

FName FAvaRundownSubListDocumentTabFactory::GetTabId(int32 InSubListIndex)
{
	return FName(BaseTabName + "_" + FString::FromInt(InSubListIndex));
}

FAvaRundownSubListDocumentTabFactory::FAvaRundownSubListDocumentTabFactory(const TSharedPtr<FAvaRundownEditor>& InRundownEditor)
	: FDocumentTabFactory(FactoryId, InRundownEditor)
	, RundownEditorWeak(InRundownEditor)
	, SubListIndex(UAvaRundown::InstancePageList.SubListIndex)
{
	TabIcon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "PlacementBrowser.Icons.All");
}

TSharedRef<SWidget> FAvaRundownSubListDocumentTabFactory::CreateTabBody(const FWorkflowTabSpawnInfo& InInfo) const
{
	if (SubListIndex <= UAvaRundown::InstancePageList.SubListIndex)
	{
		return SNullWidget::NullWidget;
	}

	return SNew(SAvaRundownInstancedPageList, RundownEditorWeak.Pin(), UAvaRundown::CreateSubListReference(SubListIndex));
}

TSharedRef<SDockTab> FAvaRundownSubListDocumentTabFactory::SpawnSubListTab(const FWorkflowTabSpawnInfo& InInfo, int32 InSubListIndex)
{
	SubListIndex = InSubListIndex;
	TabIdentifier = GetTabId(InSubListIndex);
	TabLabel = FText::Format(LOCTEXT("RundownSubListDocument_TabLabel", "Page View {0}"), FText::AsNumber(SubListIndex + 1));
	ViewMenuDescription = FText::Format(LOCTEXT("RundownSubListDocument_ViewMenu_Desc", "Page View {0}"), FText::AsNumber(SubListIndex + 1));
	ViewMenuTooltip = FText::Format(LOCTEXT("RundownSubListDocument_ViewMenu_ToolTip", "Page View {0}"), FText::AsNumber(SubListIndex + 1));

	TSharedRef<SDockTab> NewTab = SpawnTab(InInfo);

	TSharedRef<SAvaRundownInstancedPageList> PageList = StaticCastSharedRef<SAvaRundownInstancedPageList>(NewTab->GetContent());
	NewTab->SetOnTabActivated(SDockTab::FOnTabActivatedCallback::CreateSP(PageList, &SAvaRundownInstancedPageList::OnTabActivated));
	PageList->SetMyTab(NewTab);

	if (TSharedPtr<FAvaRundownEditor> RundownEditor = PageList->GetRundownEditor())
	{
		UAvaRundown* Rundown = RundownEditor->GetRundown();

		if (IsValid(Rundown) && Rundown->IsValidSubListIndex(InSubListIndex)
			&& !Rundown->GetSubList(InSubListIndex).Name.IsEmpty())
		{
			NewTab->SetLabel(Rundown->GetSubList(InSubListIndex).Name);
		}
	}

	return NewTab;
}

#undef LOCTEXT_NAMESPACE
