// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ChaosVDPlaybackControllerObserver.h"
#include "Widgets/SChaosVDNameListPicker.h"
#include "Templates/SharedPointer.h"

class FUICommandInfo;
class SChaosVDPlaybackViewport;
class FChaosVDPlaybackViewportClient;
struct FChaosVDTrackInfo;
struct FGuid;

/**
 * Widget containing the visualization controls for the Chaos Visual debugger tool to be shown in a Tab
 */
class SChaosVDVisualizationControls : public SCompoundWidget, public FChaosVDPlaybackControllerObserver
{
public:
	SLATE_BEGIN_ARGS( SChaosVDVisualizationControls ){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TWeakPtr<FChaosVDPlaybackController>& InPlaybackController, const TWeakPtr<SChaosVDPlaybackViewport>& InPlaybackViewport);

protected:

	void RegisterCommandsUI();

	TWeakPtr<SChaosVDPlaybackViewport> PlaybackViewport;

};