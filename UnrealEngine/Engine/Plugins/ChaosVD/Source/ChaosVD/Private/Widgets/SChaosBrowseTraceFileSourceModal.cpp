// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SChaosBrowseTraceFileSourceModal.h"

#include "ChaosVDModule.h"
#include "Editor.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SChaosVDNameListPicker.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"


void SChaosBrowseTraceFileSourceModal::PopulateLocationNamesList()
{
	TArray<TSharedPtr<FName>> NewNameList;
	NewNameList.Reserve(LocationNameToResponseID.Num());
	Algo::Transform(LocationNameToResponseID, NewNameList, [](const TPair<FName, EChaosVDBrowseFileModalResponse>& TransformData){ return MakeShared<FName>(TransformData.Key);});
	NamePickerWidget->UpdateNameList(MoveTemp(NewNameList));
}

void SChaosBrowseTraceFileSourceModal::RegisterAvailableLocationOptions()
{
	// TODO: This FText to FName conversion can be avoided if we make SChaosVDNameListPicker accept FText as well
	// But doing so it is a larger change than the one we can do now. SChaosVDNameListPicker will need to take the type as a template and there are several places currently using it.
	// FText.ToString should return the already localized name so we should be good in that area

	CVDFilesLocationNames[0] = LOCTEXT("BrowseLastFolder", "Last Opened Folder");
	LocationNameToResponseID.Add(FName(CVDFilesLocationNames[0].ToString()), EChaosVDBrowseFileModalResponse::OpenLastFolder);
	
	CVDFilesLocationNames[1] = LOCTEXT("BrowseProfilingFolder", "Profiling Folder");
	LocationNameToResponseID.Add(FName(CVDFilesLocationNames[1].ToString()), EChaosVDBrowseFileModalResponse::OpenProfilingFolder);

	CVDFilesLocationNames[2] = LOCTEXT("BrowseTraceStore", "Trace Store Folder");
	LocationNameToResponseID.Add(FName(CVDFilesLocationNames[2].ToString()), EChaosVDBrowseFileModalResponse::OpenTraceStore);

	CurrentSelectedLocationName = FName(CVDFilesLocationNames[0].ToString());
}

void SChaosBrowseTraceFileSourceModal::Construct(const FArguments& InArgs)
{
	SWindow::Construct(SWindow::FArguments()
	.Title(LOCTEXT("SChaosVDBrowseFileModal_Title", "CVD Recording Source Selector"))
	.SupportsMinimize(false)
	.SupportsMaximize(false)
	.UserResizeBorder(0)
	.ClientSize(FVector2D(350, 80))
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(15)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SelectSourceMessage", "Where is the recording file located?"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 10))
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(5)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FAppStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FAppStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0, 0)
				[
					SAssignNew(NamePickerWidget, SChaosVDNameListPicker)
					.OnNameSleceted_Raw(this, &SChaosBrowseTraceFileSourceModal::HandleSessionNameSelected)
				]
				+SUniformGridPanel::Slot(1, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("OpenSelectedLocation", "Open"))
					.OnClicked(this, &SChaosBrowseTraceFileSourceModal::OnButtonClick)
				]
			]
	]);
	
	RegisterAvailableLocationOptions();

	PopulateLocationNamesList();
}

EChaosVDBrowseFileModalResponse SChaosBrowseTraceFileSourceModal::ShowModal()
{
	GEditor->EditorAddModalWindow(SharedThis(this));
	return UserResponse;
}

void SChaosBrowseTraceFileSourceModal::HandleSessionNameSelected(TSharedPtr<FName> SelectedName)
{
	if (SelectedName)
	{
		CurrentSelectedLocationName = *SelectedName;
	}
}

FReply SChaosBrowseTraceFileSourceModal::OnButtonClick()
{
	if (const EChaosVDBrowseFileModalResponse* SelectedLocationResponse = LocationNameToResponseID.Find(CurrentSelectedLocationName))
	{
		UserResponse = *SelectedLocationResponse;
	}
	else
	{
		UserResponse = EChaosVDBrowseFileModalResponse::OpenLastFolder;
		UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Failed to get selected response [%s] | Defaulting to opening the last opened folder..."), ANSI_TO_TCHAR(__FUNCTION__), *CurrentSelectedLocationName.ToString());
	}
	
	RequestDestroyWindow();
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE

