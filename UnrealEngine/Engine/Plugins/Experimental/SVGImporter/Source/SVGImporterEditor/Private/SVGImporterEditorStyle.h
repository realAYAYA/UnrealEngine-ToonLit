// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"

class FSVGImporterEditorStyle : public FSlateStyleSet
{
public:
	FSVGImporterEditorStyle();

	virtual ~FSVGImporterEditorStyle() override;

	static FSVGImporterEditorStyle& Get()
	{
		static FSVGImporterEditorStyle StyleSet;
		return StyleSet;
	}
};
