// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Docking/TabManager.h"

class ILiveLinkClient;
class FLiveLinkHubWindowController;

struct FLiveLinkHubComponentInitParams
{
	/** Manages the hub's slate application */
	TSharedRef<FLiveLinkHubWindowController> WindowController;

	/** The root area of the main window layout. Add tabs to this. */
	TSharedRef<FTabManager::FStack> MainStack;

	FLiveLinkHubComponentInitParams(TSharedRef<FLiveLinkHubWindowController> InWindowController, TSharedRef<FTabManager::FStack> InMainStack)
		: WindowController(MoveTemp(InWindowController))
		, MainStack(MoveTemp(InMainStack))
	{}
};

/** Provides the base interface for elements in the LiveLink Hub UI. */
class ILiveLinkHubComponent
{
public:
	virtual ~ILiveLinkHubComponent() = default;

	/** Initialises the component, e.g. registering tab spawners, etc. */
	virtual void Init(const FLiveLinkHubComponentInitParams& Params) {}
};
