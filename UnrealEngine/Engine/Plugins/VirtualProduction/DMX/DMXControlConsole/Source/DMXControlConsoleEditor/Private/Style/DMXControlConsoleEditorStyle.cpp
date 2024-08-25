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
#define EDITOR_IMAGE_BRUSH( RelativePath, ... ) FSlateImageBrush(FPaths::EngineContentDir() / "Editor/Slate" / RelativePath + TEXT(".png"), __VA_ARGS__ )

FDMXControlConsoleEditorStyle::FDMXControlConsoleEditorStyle()
	: FSlateStyleSet("DMXControlConsoleEditorStyle")
{
	const FVector2D Icon16x16(16.f, 16.f);
	const FVector2D Icon51x30(51.f, 30.f);
	const FVector2D Icon51x31(51.f, 31.f);
	const FVector2D Icon40x40(40.f, 40.f);

	const FSlateColor SelectorColor = FAppStyle::GetSlateColor("SelectorColor");

	const TCHAR* DMXControlConsolePluginName = TEXT("DMXControlConsole");
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(DMXControlConsolePluginName);
	if (ensureMsgf(Plugin.IsValid(), TEXT("Cannot find Plugin 'DMXControlConsole' hence cannot set the DMXControlConsoleEditorStyle root directory.")))
	{
		SetContentRoot(Plugin->GetBaseDir() / TEXT("Content/Slate"));
	}
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	// Color brushes
	{
		Set("DMXControlConsole.WhiteBrush", new FSlateColorBrush(FLinearColor(1.f, 1.f, 1.f, 1.f)));
		Set("DMXControlConsole.BlackBrush", new FSlateColorBrush(FLinearColor(0.f, 0.f, 0.f, 1.f)));
		Set("DMXControlConsole.DefaultBrush", new FSlateColorBrush(FLinearColor(.0075f, .0075f, .0075f, 1.f)));
		Set("DMXControlConsole.Rounded.WhiteBrush", new FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 1.f), 4.f));
		Set("DMXControlConsole.Rounded.WhiteBrush_Tansparent", new FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.3f), 4.f));
		Set("DMXControlConsole.Rounded.BlackBrush", new FSlateRoundedBoxBrush(FLinearColor(0.f, 0.f, 0.f, 1.f), 4.f));
		Set("DMXControlConsole.Rounded.DefaultBrush", new FSlateRoundedBoxBrush(FLinearColor(.03f, .03f, .03f, 1.f), 4.f));
	}

	// Icons
	{
		Set("DMXControlConsole.TabIcon", new IMAGE_BRUSH_SVG("DMXControlConsole_16", Icon16x16));
		Set("DMXControlConsole.DMXLibrary", new IMAGE_BRUSH_SVG("DMXLibrary_16", Icon16x16));
		Set("DMXControlConsole.ResetToDefault", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/Reset", Icon16x16));
		Set("DMXControlConsole.ResetToZero", new EDITOR_IMAGE_BRUSH("UMG/Slider", Icon16x16));
		Set("DMXControlConsole.Clear", new EDITOR_IMAGE_BRUSH("Icons/GeneralTools/Erase_40x", Icon16x16));
		Set("DMXControlConsole.Group", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/GroupActors", Icon16x16));
		Set("DMXControlConsole.Ungroup", new EDITOR_IMAGE_BRUSH_SVG("Starship/Common/DataLayers", Icon16x16));
		Set("DMXControlConsole.ControlMode", new EDITOR_IMAGE_BRUSH_SVG("Starship/MainToolbar/select", Icon16x16));
		Set("DMXControlConsole.Fader.Mute", new CORE_IMAGE_BRUSH("Common/SmallCheckBox", Icon16x16));
		Set("DMXControlConsole.Fader.Unmute", new CORE_IMAGE_BRUSH("Common/SmallCheckBox_Checked", Icon16x16));
	}

	// Fader Groups
	{
		Set("DMXControlConsole.Rounded.FaderGroupTag", new FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 1.f), 2.f));

		Set("DMXControlConsole.FaderGroup_Hovered", new FSlateColorBrush(FLinearColor(.05f, .05f, .05f, 1.f)));
		Set("DMXControlConsole.FaderGroup_Selected", new FSlateColorBrush(FLinearColor(.15f, .15f, .15f, 1.f)));
		Set("DMXControlConsole.FaderGroup_Highlighted", new FSlateColorBrush(FLinearColor(.2f, .2f, .2f, 1.f)));
		Set("DMXControlConsole.Rounded.FaderGroup_Selected", new FSlateRoundedBoxBrush(FLinearColor(.05f, .05f, .05f, 1.f), 4.f));
		Set("DMXControlConsole.Rounded.FaderGroup_Highlighted", new FSlateRoundedBoxBrush(FLinearColor(.06f, .06f, .06f, 1.f), 4.f));
		Set("DMXControlConsole.Rounded.FaderGroupBorder_Hovered", new FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.3f), 4.f));
		Set("DMXControlConsole.Rounded.FaderGroupBorder_Selected", new FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 1.f), 4.f));

		Set("DMXControlConsole.FaderGroupToolbar", FTableRowStyle()
			.SetEvenRowBackgroundBrush(FSlateRoundedBoxBrush(FLinearColor(.01f, .01f, .01f, 1.f), 2.f))
			.SetOddRowBackgroundBrush(FSlateRoundedBoxBrush(FLinearColor(.02f, .02f, .02f, 1.f), 2.f))
			.SetEvenRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(FLinearColor(.05f, .05f, .05f, 1.f), 2.f))
			.SetOddRowBackgroundHoveredBrush(FSlateRoundedBoxBrush(FLinearColor(.06f, .06f, .06f, 1.f), 2.f))
			.SetActiveBrush(FSlateRoundedBoxBrush(FLinearColor(.08f, .08f, .08f, 1.f), 2.f))
			.SetActiveHoveredBrush(FSlateRoundedBoxBrush(FLinearColor(.09f, .09f, .09f, 1.f), 2.f))
			.SetSelectorFocusedBrush(CORE_BORDER_BRUSH("Common/Selector", FMargin(4.f / 16.f), FLinearColor(1.f, 1.f, 1.f, .7f)))
		);
	}

	// Faders
	{
		static const FLinearColor DefaultFaderBackColor = FLinearColor::FromSRGBColor(FColor::FromHex("191919"));
		static const FLinearColor DefaultSingleFaderFillColor = FLinearColor::FromSRGBColor(FColor::FromHex("38a4fc"));
		static const FLinearColor DefeaultFaderInactiveColor = FLinearColor::FromSRGBColor(FColor::FromHex("#8f8f8f"));
		static const FLinearColor DefeaultFaderForeColor = FLinearColor::FromSRGBColor(FColor::FromHex("ffffff"));

		static const FSpinBoxStyle SingleFaderSpinBoxStyle = FSpinBoxStyle()
			.SetBackgroundBrush(CORE_BOX_BRUSH("Common/Spinbox", FMargin(4.f / 16.f), DefaultFaderBackColor))
			.SetHoveredBackgroundBrush(CORE_BOX_BRUSH("Common/Spinbox", FMargin(4.f / 16.f), DefaultFaderBackColor))
			.SetActiveFillBrush(CORE_BOX_BRUSH("Common/Spinbox_Fill", FMargin(4.f / 16.f), DefaultSingleFaderFillColor))
			.SetInactiveFillBrush(CORE_BOX_BRUSH("Common/Spinbox_Fill", FMargin(4.f / 16.f), DefeaultFaderInactiveColor))
			.SetForegroundColor(DefeaultFaderForeColor)
			.SetArrowsImage(FSlateNoResource());

		static const FLinearColor DefaultMultiFaderFillColor = FLinearColor::FromSRGBColor(FColor::FromHex("025de6"));
		static const FSpinBoxStyle MultiFaderSpinBoxStyle = FSpinBoxStyle(SingleFaderSpinBoxStyle)
			.SetActiveFillBrush(CORE_BOX_BRUSH("Common/Spinbox_Fill", FMargin(4.f / 16.f), DefaultMultiFaderFillColor));

		Set("DMXControlConsole.SingleFader", SingleFaderSpinBoxStyle);
		Set("DMXControlConsole.MultiFader", MultiFaderSpinBoxStyle);

		Set("DMXControlConsole.Fader", new FSlateColorBrush(FLinearColor(.2f, .2f, .2f, .2f)));
		Set("DMXControlConsole.Fader_Highlighted", new FSlateColorBrush(FLinearColor(.4f, .4f, .4f, .4f)));
		Set("DMXControlConsole.Rounded.Fader", new FSlateRoundedBoxBrush(FLinearColor(.015f, .015f, .015f, 1.f), 4.f));
		Set("DMXControlConsole.Rounded.Fader_Hovered", new FSlateRoundedBoxBrush(FLinearColor(.1f, .1f, .1f, 1.f), 4.f));
		Set("DMXControlConsole.Rounded.Fader_Selected", new FSlateRoundedBoxBrush(FLinearColor(.12f, .12f, .12f, 1.f), 4.f));
		Set("DMXControlConsole.Rounded.Fader_Highlighted", new FSlateRoundedBoxBrush(FLinearColor(.14f, .14f, .14f, 1.f), 4.f));

		Set("DMXControlConsole.Rounded.SpinBoxBorder", new FSlateRoundedBoxBrush(FLinearColor(.03f, .03f, .03f, 1.f), 4.f));
		Set("DMXControlConsole.Rounded.SpinBoxBorder_SingleHovered", new FSlateRoundedBoxBrush(DefaultSingleFaderFillColor, 4.f));
		Set("DMXControlConsole.Rounded.SpinBoxBorder_MultiHovered", new FSlateRoundedBoxBrush(DefaultMultiFaderFillColor, 4.f));
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
#undef EDITOR_IMAGE_BRUSH
