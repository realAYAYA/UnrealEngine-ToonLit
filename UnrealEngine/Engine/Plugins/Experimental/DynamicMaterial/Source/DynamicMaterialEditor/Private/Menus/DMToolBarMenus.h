// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

class SDMEditor;
class SWidget;

class FDMToolBarMenus
{
public:
	static TSharedRef<SWidget> MakeEditorLayoutMenu(const TSharedPtr<SDMEditor>& InEditorWidget = nullptr);
};
