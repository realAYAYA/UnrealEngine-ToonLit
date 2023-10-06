// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

/**
 * Slate style set that defines all the styles for the take recorder UI
 */
class FNiagaraSimCachingEditorStyle
	: public FSlateStyleSet
{
public:
	static FName StyleName;

	/** Access the singleton instance for this style set */
	static FNiagaraSimCachingEditorStyle& Get();

private:

	FNiagaraSimCachingEditorStyle();
	virtual ~FNiagaraSimCachingEditorStyle() override;
};
