// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UnrealEdMisc.h"

class FSceneOutlinerDelegates
{
public:
	/** Return a single FSceneOutlinerDelegates object */
	SCENEOUTLINER_API static FSceneOutlinerDelegates& Get()
	{
		// return the singleton object
		static FSceneOutlinerDelegates Singleton;
		return Singleton;
	}

	/**	Broadcasts whenever the current selection changes */
	FSimpleMulticastDelegate SelectionChanged;

	/** Broadcasts whenever a Component Selection Changes */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnComponentSelectionChanged, class UActorComponent*);
	FOnComponentSelectionChanged OnComponentSelectionChanged;

	/** Broadcasts whenever a Component has been modified */
	FSimpleMulticastDelegate OnComponentsUpdated;

};
