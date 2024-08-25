// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Widgets/SCompoundWidget.h"
#include "UObject/NameTypes.h"

class FName;
class FLiveLinkClient;

/**
 * Utility class to display timecode status on the menu bar. Provides user with
 * a drop down of supported time code values including subjects that are in the
 * Live Link session.
 */
class SLiveLinkTimecode : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SLiveLinkTimecode) {}
		/** The tab's content */
	SLATE_END_ARGS()

	/**
	 * @param InArgs
	 */
	void Construct(const FArguments& InArgs);

private:
	/** Indicate the status of the time code send status */
	FSlateColor GetTimecodeStatusColor() const;

	/** Gets the tooltip text for the time code widget */
	FText GetTimecodeTooltip() const;

	/** Makes the combo button menu for Timecode source */
	TSharedRef<SWidget> MakeMenu();

	/** Invoked by slate when the time code setting is enabled. */
	void OnEnableTimecodeToggled();

	/** Specifies the source for timecode */
	void SetTimecodeSource(const FName SourceId);

	/** Sends the current timecode settings to the connected editor(s). */
	void SendUpdatedTimecodeToEditor();

	/** Do we have timecode source enabled */
	bool bIsTimecodeSource = false;

	/** Name of the active timecode source. */
	FName ActiveTimecodeSource;

	/** Cached value for the FLiveLinkClient */
	FLiveLinkClient* WorkingClient = nullptr;
};
