// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Landscape.h"

class ILandscapeEditorServices
{
public:
	virtual ~ILandscapeEditorServices() {}

	/**
	 * Attempts to find an edit layer named InEditLayerName in InTargetLandscape.
	 * Creates the layer if it does not exist.
	 * 
	 * @param InEditLayerName The name of the layer to search for and possibly create
	 * @param InTargetLandscape The landscape which should have a layer called InEditLayerName
	 * @return The index at which the edit layer named InEditLayerName exists
	 */
	virtual int32 GetOrCreateEditLayer(FName InEditLayerName, ALandscape* InTargetLandscape) = 0;
};

#if WITH_EDITOR
class FLandscapeEditorServices : public ILandscapeEditorServices
{
public:
	virtual ~FLandscapeEditorServices() {}

	// Implements pure virtual function. Insertion logic is left to the user through modal drag + drop dialog.
	virtual int32 GetOrCreateEditLayer(FName InEditLayerName, ALandscape* InTargetLandscape);
};
#endif // WITH_EDITOR