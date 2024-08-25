// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreFwd.h"

class UExternalDataLayerAsset;

/**
 * Pushes a copy of the existing context and overrides the current External Data Layer Asset.
 */
class ENGINE_API FScopedActorEditorContextSetExternalDataLayerAsset
{
public:
	FScopedActorEditorContextSetExternalDataLayerAsset(const UExternalDataLayerAsset* InExternalDataLayerAsset);
	~FScopedActorEditorContextSetExternalDataLayerAsset();
};

#endif