// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXControlConsoleEditorStyle.h"

#include "Engine/Font.h"
#include "Framework/Application/SlateApplication.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateTypes.h"
#include "Styling/StyleColors.h"


#define EDITOR_IMAGE_BRUSH_SVG( RelativePath, ... ) FSlateVectorImageBrush(FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".svg"), __VA_ARGS__ )

FDMXControlConsoleEditorStyle::FDMXControlConsoleEditorStyle()
	: FSlateStyleSet("DMXControlConsoleEditorStyle")
{
	static const FVector2D Icon16x16(16.f, 16.f);
	static const FVector2D Icon51x30(51.f, 30.f);
	static const FVector2D Icon51x31(51.f, 31.f);
	static const FVector2D Icon40x40(40.f, 40.f);

	const FSlateColor SelectorColor = FAppStyle::GetSlateColor("SelectorColor");

	const TSharedPtr<FSlateStyleSet> Style = MakeShared<FSlateStyleSet>(GetStyleSetName());

	static const TCHAR* DMXEnginePluginName = TEXT("DMXEngine");
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(DMXEnginePluginName);
	if (ensureMsgf(Plugin.IsValid(), TEXT("Cannot find Plugin 'DMXEngine' hence cannot set the DMXControlConsoleEditorStyle root directory.")))
	{
		SetContentRoot(Plugin->GetBaseDir() / TEXT("Content/Slate"));
	}
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	// Color brushes
	{
		Set("DMXControlConsole.WhiteBrush", new FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 1.f)));
		Set("DMXControlConsole.BlackBrush", new FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 1.f)));
	}

	// Icons
	{ 
		Set("DMXControlConsole.TabIcon", new IMAGE_BRUSH_SVG("DMXControlConsole_16", Icon16x16));
		Set("DMXControlConsole.PlayDMX", new IMAGE_BRUSH("icon_DMXPixelMappingEditor_PlayDMX_40x", Icon40x40));
		Set("DMXControlConsole.StopPlayingDMX", new IMAGE_BRUSH("icon_DMXPixelMappingEditor_StopPlayingDMX_40x", Icon40x40));
	}

	// Fader Groups
	{
		Set("DMXControlConsole.FaderGroup_Hovered", new FSlateColorBrush(FLinearColor(.05f, .05f, .05f, 1.f)));
		Set("DMXControlConsole.FaderGroup_Selected", new FSlateColorBrush(FLinearColor(.15f, .15f, .15f, 1.f)));
		Set("DMXControlConsole.FaderGroup_Highlighted", new FSlateColorBrush(FLinearColor(.2f, .2f, .2f, 1.f)));
	}

	// Faders
	{
		Set("DMXControlConsole.Fader", new FSlateColorBrush(FLinearColor(.2f, .2f, .2f, .2f)));
		Set("DMXControlConsole.Fader_Highlighted", new FSlateColorBrush(FLinearColor(.4f, .4f, .4f, .4f)));

		static const FLinearColor DefaultFaderBackColor = FLinearColor::FromSRGBColor(FColor::FromHex("191919"));
		static const FLinearColor DefaultFaderFillColor = FLinearColor::FromSRGBColor(FColor::FromHex("00aeef"));
		static const FLinearColor DefeaultFaderForeColor = FLinearColor::FromSRGBColor(FColor::FromHex("ffffff"));

		Set("DMXControlConsole.Fader", FSpinBoxStyle(FAppStyle::GetWidgetStyle<FSpinBoxStyle>("SpinBox"))
			.SetBackgroundBrush(CORE_BOX_BRUSH("Common/Spinbox", FMargin(4.f / 16.f), DefaultFaderBackColor))
			.SetHoveredBackgroundBrush(CORE_BOX_BRUSH("Common/Spinbox", FMargin(4.f / 16.f), DefaultFaderBackColor))
			.SetActiveFillBrush(CORE_BOX_BRUSH("Common/Spinbox_Fill", FMargin(4.f / 16.f), DefaultFaderFillColor))
			.SetInactiveFillBrush(CORE_BOX_BRUSH("Common/Spinbox_Fill", FMargin(4.f / 16.f), DefaultFaderFillColor))
			.SetForegroundColor(DefeaultFaderForeColor)
			.SetArrowsImage(FSlateNoResource()));
	}

	// Macros
	{
		Set("DMXControlConsole.MacroSineWave", new IMAGE_BRUSH("icon_MacroSineWave_51x31", Icon51x31));
		Set("DMXControlConsole.MacroMin", new IMAGE_BRUSH("icon_MacroMin_51x30", Icon51x30));
		Set("DMXControlConsole.MacroMax", new IMAGE_BRUSH("icon_MacroMax_51x30", Icon51x30));
	}

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FDMXControlConsoleEditorStyle::~FDMXControlConsoleEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

const FDMXControlConsoleEditorStyle& FDMXControlConsoleEditorStyle::Get()
{
	static const FDMXControlConsoleEditorStyle Inst;
	return Inst;
}

#undef EDITOR_IMAGE_BRUSH_SVG
