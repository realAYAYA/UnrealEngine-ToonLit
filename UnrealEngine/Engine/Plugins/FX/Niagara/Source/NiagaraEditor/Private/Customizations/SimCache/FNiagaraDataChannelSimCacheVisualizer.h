// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Customizations/NiagaraDataInterfaceSimCacheVisualizer.h"

/*
 * Provides a custom widget to show data channel writes stored in the sim cache in a table.
 */
class FNiagaraDataChannelSimCacheVisualizer : public INiagaraDataInterfaceSimCacheVisualizer
{
public:
	virtual ~FNiagaraDataChannelSimCacheVisualizer() override = default;
	
	virtual TSharedPtr<SWidget> CreateWidgetFor(UObject* CachedData, TSharedPtr<FNiagaraSimCacheViewModel> ViewModel) override;
};
