// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FNiagaraSimCacheViewModel;
class SWidget;

/**
 * Implementations can be registered with the niagara editor module to provide custom visualizations for data interfaces stored in sim caches.
 * This is needed since data interfaces can store any custom uobject, so the sim cache editor has no knowledge how to display the data stored within.
 *
 * See FNiagaraDataChannelCacheVisualizer for an example implementation.
 */
class NIAGARAEDITOR_API INiagaraDataInterfaceSimCacheVisualizer
{
public:
	virtual ~INiagaraDataInterfaceSimCacheVisualizer() = default;
	
	virtual TSharedPtr<SWidget> CreateWidgetFor(UObject* CachedData, TSharedPtr<FNiagaraSimCacheViewModel> ViewModel);
};
