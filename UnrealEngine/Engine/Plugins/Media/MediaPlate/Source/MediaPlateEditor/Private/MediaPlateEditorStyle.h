// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

/**
 * Implements the visual style of the media plate editor UI.
 */
class FMediaPlateEditorStyle : public FSlateStyleSet
{
public:
	/** Default constructor. */
	FMediaPlateEditorStyle();

	 /** Destructor. */
	~FMediaPlateEditorStyle();

	static TSharedRef<FMediaPlateEditorStyle> Get()
	{
		if (!Singleton.IsValid())
		{
			Singleton = MakeShareable(new FMediaPlateEditorStyle);
		}
		return Singleton.ToSharedRef();
	}

private:
	static TSharedPtr<FMediaPlateEditorStyle> Singleton;
};
