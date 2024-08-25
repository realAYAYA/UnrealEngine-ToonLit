// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointerFwd.h"

class ILevelEditor;
class SDMEditor;
class SDockTab;
class UWorld;

class FDMLevelEditorIntegration
{
public:
	static void Initialize();

	static void Shutdown();

	static TSharedPtr<SDMEditor> GetEditorForWorld(UWorld* InWorld);

	static TSharedPtr<SDockTab> InvokeTabForWorld(UWorld* InWorld);
};
