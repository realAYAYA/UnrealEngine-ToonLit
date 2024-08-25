// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UI/Window/ILiveLinkHubComponent.h"

class FSpawnTabArgs;
class SLiveLinkHubMainTabView;
class SWindow;

class FLiveLinkHubMainTabController : public ILiveLinkHubComponent, public TSharedFromThis<FLiveLinkHubMainTabController>
{
public:

	//~ Begin ILiveLinkHubComponent Interface
	virtual void Init(const FLiveLinkHubComponentInitParams& Params) override;
	//~ End ILiveLinkHubComponent Interface

	/** Spawn the main tab. */
	void OpenTab();

private:
	/** Manages the sub-tabs */
	TSharedPtr<SLiveLinkHubMainTabView> MainTabView;

	/** Spawn the main tab. */
	TSharedRef<SDockTab> SpawnMainTab(const FSpawnTabArgs& Args, TSharedPtr<SWindow> RootWindow);
};
