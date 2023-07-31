// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "Editor/UnrealEd/Public/SViewportToolBar.h"

class FXcodeGPUDebuggerPluginModule;
class FExtensionBase;
class FExtensibilityManager;
class FExtender;
class FToolBarBuilder;

class FXcodeGPUDebuggerPluginEditorExtension
{
public:
	FXcodeGPUDebuggerPluginEditorExtension(FXcodeGPUDebuggerPluginModule* ThePlugin);
	~FXcodeGPUDebuggerPluginEditorExtension();

private:
	void Initialize(FXcodeGPUDebuggerPluginModule* ThePlugin);
	void OnEditorLoaded(SWindow& SlateWindow, void* ViewportRHIPtr);
	void AddToolbarExtension(FToolBarBuilder& ToolbarBuilder, FXcodeGPUDebuggerPluginModule* ThePlugin);

	FDelegateHandle LoadedDelegateHandle;
	TSharedPtr<const FExtensionBase> ToolbarExtension;
	TSharedPtr<FExtensibilityManager> ExtensionManager;
	TSharedPtr<FExtender> ToolbarExtender;
	bool IsEditorInitialized;
};

#endif // WITH_EDITOR
