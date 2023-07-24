// Copyright Epic Games, Inc. All Rights Reserved.

#include "RewindDebuggerStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"

TSharedPtr< FRewindDebuggerStyle > FRewindDebuggerStyle::StyleInstance = nullptr;


FRewindDebuggerStyle::FRewindDebuggerStyle() :
    FSlateStyleSet("RewindDebuggerStyle")
{
	const FVector2D Icon24x24(24.0f, 24.0f);
	const FVector2D Icon16x16(16.0f, 16.0f);

	SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Animation/GameplayInsights/Content"));
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT(""));

	// Toolbar
	Set("RewindDebugger.StartRecording.small", new CORE_IMAGE_BRUSH("Editor/Slate/Sequencer/Transport_Bar/Record_24x", Icon24x24));
	Set("RewindDebugger.StopRecording.small", new CORE_IMAGE_BRUSH("Editor/Slate/Sequencer/Transport_Bar/Recording_24x", Icon24x24));
	Set("RewindDebugger.FirstFrame.small", new CORE_IMAGE_BRUSH("Editor/Slate/Sequencer/Transport_Bar/Go_To_Front_24x", Icon24x24));
	Set("RewindDebugger.PreviousFrame.small", new CORE_IMAGE_BRUSH("Editor/Slate/Sequencer/Transport_Bar/Step_Backwards_24x", Icon24x24));
	Set("RewindDebugger.ReversePlay.small", new CORE_IMAGE_BRUSH("Editor/Slate/Sequencer/Transport_Bar/Backwards_24x", Icon24x24));
	Set("RewindDebugger.Pause.small", new CORE_IMAGE_BRUSH("Editor/Slate/Sequencer/Transport_Bar/Pause_24x", Icon24x24));
	Set("RewindDebugger.Play.small", new CORE_IMAGE_BRUSH("Editor/Slate/Sequencer/Transport_Bar/Play_24x", Icon24x24));
	Set("RewindDebugger.NextFrame.small", new CORE_IMAGE_BRUSH("Editor/Slate/Sequencer/Transport_Bar/Step_Forward_24x", Icon24x24));
	Set("RewindDebugger.LastFrame.small", new CORE_IMAGE_BRUSH("Editor/Slate/Sequencer/Transport_Bar/Go_To_End_24x", Icon24x24));

	// Actor Picker
	Set("RewindDebugger.SelectActor", new CORE_IMAGE_BRUSH("Editor/Slate/Icons/eyedropper_16px", Icon16x16));

	// tab icon
	Set("RewindDebugger.RewindIcon", new IMAGE_BRUSH("Rewind_24x", Icon16x16));
	Set("RewindDebugger.RewindDetailsIcon", new IMAGE_BRUSH("RewindDetails_24x", Icon16x16));

	// menu icon
	Set("RewindDebugger.MenuIcon", new CORE_IMAGE_BRUSH_SVG("Slate/Starship/Common/menu", Icon16x16));
}

void FRewindDebuggerStyle::Initialize()
{
	if (!StyleInstance.IsValid())
	{
		StyleInstance = MakeShared<FRewindDebuggerStyle>();
		FSlateStyleRegistry::RegisterSlateStyle(*StyleInstance);
	}
}

void FRewindDebuggerStyle::Shutdown()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleInstance);
	ensure(StyleInstance.IsUnique());
	StyleInstance.Reset();
}

const ISlateStyle& FRewindDebuggerStyle::Get()
{
	return *StyleInstance;
}
