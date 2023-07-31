// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"

class UPluginMetadataObject;
class IPlugin;
class SWindow;
class SWidget;

/**
 * Creates a window to edit a .uplugin file
 */
class FPluginDescriptorEditor
{
public:
	static void OpenEditorWindow(TSharedRef<IPlugin> PluginToEdit, TSharedPtr<SWidget> ParentWidget, FSimpleDelegate OnEditCommitted);

private:
	static FReply OnEditPluginFinished(UPluginMetadataObject* MetadataObject, TSharedPtr<IPlugin> Plugin, FSimpleDelegate OnEditCommitted, TWeakPtr<SWindow> WeakWindow);
};

