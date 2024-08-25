// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

class IAvaEditor;

struct FAvaLevelEditorIntegration
{
	static TSharedRef<IAvaEditor> BuildEditor();
};
