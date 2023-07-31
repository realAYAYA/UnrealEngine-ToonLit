// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AnimTimeline/AnimTimelineTrack.h"

class SAnimMontagePanel;
class SVerticalBox;

/** A timeline track that displays anim montage section labels */
class FAnimTimelineTrack_MontageSections : public FAnimTimelineTrack
{
	ANIMTIMELINE_DECLARE_TRACK(FAnimTimelineTrack_MontageSections, FAnimTimelineTrack);

public:
	FAnimTimelineTrack_MontageSections(const TSharedRef<FAnimModel>& InModel);

	/** FAnimTimelineTrack interface */
	virtual bool SupportsFiltering() const override { return false; }
};
