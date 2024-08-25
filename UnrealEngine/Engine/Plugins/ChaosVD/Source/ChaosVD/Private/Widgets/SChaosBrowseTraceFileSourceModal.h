// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"

class SChaosVDNameListPicker;
class FReply;

enum class EChaosVDBrowseFileModalResponse
{
	OpenLastFolder,
	OpenProfilingFolder,
	OpenTraceStore,
	Cancel
};

/**
 * 
 */
class SChaosBrowseTraceFileSourceModal : public SWindow
{
public:
	SLATE_BEGIN_ARGS(SChaosBrowseTraceFileSourceModal)
		{
		}

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/** Displays the dialog in a blocking fashion */
	EChaosVDBrowseFileModalResponse ShowModal();

protected:

	void PopulateLocationNamesList();

	void RegisterAvailableLocationOptions();

	void HandleSessionNameSelected(TSharedPtr<FName> SelectedName);
	
	FReply OnButtonClick();

	TSharedPtr<SChaosVDNameListPicker> NamePickerWidget;
	
	TMap<FName, EChaosVDBrowseFileModalResponse> LocationNameToResponseID;

	FName CurrentSelectedLocationName;

	EChaosVDBrowseFileModalResponse UserResponse = EChaosVDBrowseFileModalResponse::Cancel;

	FText CVDFilesLocationNames[3];
};
