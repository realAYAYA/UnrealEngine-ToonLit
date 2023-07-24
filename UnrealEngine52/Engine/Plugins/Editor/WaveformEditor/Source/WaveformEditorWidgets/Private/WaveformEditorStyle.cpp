// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorStyle.h"

#include "Styling/SlateStyleRegistry.h"
#include "WaveformEditorSlateTypes.h"
#include "WaveformEditorWidgetsSettings.h"
#include "WaveformEditorWidgetsSettings.h"

static FLazyName WaveformViewerStyleName("WaveformViewer.Style");
static FLazyName WaveformViewerOverlayStyleName("WaveformViewerOverlay.Style");
static FLazyName WaveformEditorRulerStyleName("WaveformEditorRuler.Style");

FName FWaveformEditorStyle::StyleName("WaveformEditorStyle");
TUniquePtr<FWaveformEditorStyle> FWaveformEditorStyle::StyleInstance = nullptr;

FWaveformEditorStyle::FWaveformEditorStyle()
	: FSlateStyleSet(StyleName)
{
}

FWaveformEditorStyle::~FWaveformEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

FWaveformEditorStyle& FWaveformEditorStyle::Get()
{
	check(StyleInstance);
	return *StyleInstance;
}

void FWaveformEditorStyle::Init()
{
	if (StyleInstance == nullptr)
	{
		StyleInstance = MakeUnique<FWaveformEditorStyle>();
	}

	const UWaveformEditorWidgetsSettings* Settings = GetWidgetsSettings();
	
	check(Settings);
	Settings->OnSettingChanged().AddStatic(&FWaveformEditorStyle::OnWidgetSettingsUpdated);

	StyleInstance->SetParentStyleName("CoreStyle");

	//Waveform Viewer style
	FWaveformViewerStyle WaveViewerStyle = FWaveformViewerStyle()
		.SetWaveformColor(Settings->WaveformColor)
		.SetBackgroundColor(Settings->WaveformBackgroundColor)
		.SetWaveformLineThickness(Settings->WaveformLineThickness)
		.SetSampleMarkersSize(Settings->SampleMarkersSize)
		.SetMajorGridLineColor(Settings->MajorGridColor)
		.SetMinorGridLineColor(Settings->MinorGridColor)
		.SetZeroCrossingLineColor(Settings->ZeroCrossingLineColor)
		.SetZeroCrossingLineThickness(Settings->ZeroCrossingLineThickness);

	StyleInstance->Set(WaveformViewerStyleName, WaveViewerStyle);

	//Waveform Viewer Overlay style
	FWaveformViewerOverlayStyle WaveViewerOverlayStyle = FWaveformViewerOverlayStyle().SetPlayheadColor(Settings->PlayheadColor);
	StyleInstance->Set(WaveformViewerOverlayStyleName, WaveViewerOverlayStyle);

	//Time Ruler style 
	FWaveformEditorTimeRulerStyle TimeRulerStyle = FWaveformEditorTimeRulerStyle()
		.SetHandleColor(Settings->PlayheadColor)
		.SetTicksColor(Settings->RulerTicksColor)
		.SetTicksTextColor(Settings->RulerTextColor)
		.SetHandleColor(Settings->PlayheadColor)
		.SetFontSize(Settings->RulerFontSize);

	StyleInstance->Set(WaveformEditorRulerStyleName, TimeRulerStyle);

	FSlateStyleRegistry::RegisterSlateStyle(StyleInstance->Get());

}

const UWaveformEditorWidgetsSettings* FWaveformEditorStyle::GetWidgetsSettings() 
{
	const UWaveformEditorWidgetsSettings* WaveformEditorWidgetsSettings = GetDefault<UWaveformEditorWidgetsSettings>();
	check(WaveformEditorWidgetsSettings);

	return WaveformEditorWidgetsSettings;
}

void FWaveformEditorStyle::OnWidgetSettingsUpdated(const FName& PropertyName, const UWaveformEditorWidgetsSettings* Settings)
{
	TSharedRef<FWaveformViewerStyle> WaveformViewerStyle = GetRegisteredWidgetStyle<FWaveformViewerStyle>(WaveformViewerStyleName);
	TSharedRef<FWaveformViewerOverlayStyle> WaveformViewerOverlayStyle = GetRegisteredWidgetStyle<FWaveformViewerOverlayStyle>(WaveformViewerOverlayStyleName);
	TSharedRef<FWaveformEditorTimeRulerStyle> WaveformEditorTimeRulerStyle = GetRegisteredWidgetStyle<FWaveformEditorTimeRulerStyle>(WaveformEditorRulerStyleName);

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, WaveformColor))
	{
		WaveformViewerStyle->SetWaveformColor(Settings->WaveformColor);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, WaveformBackgroundColor))
	{
		WaveformViewerStyle->SetBackgroundColor(Settings->WaveformBackgroundColor);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, WaveformLineThickness))
	{
		WaveformViewerStyle->SetWaveformLineThickness(Settings->WaveformLineThickness);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, SampleMarkersSize))
	{
		WaveformViewerStyle->SetSampleMarkersSize(Settings->SampleMarkersSize);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, MajorGridColor))
	{
		WaveformViewerStyle->SetMajorGridLineColor(Settings->MajorGridColor);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, MinorGridColor))
	{
		WaveformViewerStyle->SetMinorGridLineColor(Settings->MinorGridColor);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, ZeroCrossingLineColor))
	{
		WaveformViewerStyle->SetZeroCrossingLineColor(Settings->ZeroCrossingLineColor);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, ZeroCrossingLineThickness))
	{
		WaveformViewerStyle->SetZeroCrossingLineThickness(Settings->ZeroCrossingLineThickness);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, PlayheadColor))
	{
		WaveformViewerOverlayStyle->SetPlayheadColor(Settings->PlayheadColor);
		WaveformEditorTimeRulerStyle->SetHandleColor(Settings->PlayheadColor);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, RulerBackgroundColor))
	{
		WaveformEditorTimeRulerStyle->SetBackgroundColor(Settings->RulerBackgroundColor);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, RulerTicksColor))
	{
		WaveformEditorTimeRulerStyle->SetTicksColor(Settings->RulerTicksColor);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, RulerTextColor))
	{
		WaveformEditorTimeRulerStyle->SetTicksTextColor(Settings->RulerTextColor);
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UWaveformEditorWidgetsSettings, RulerFontSize))
	{
		WaveformEditorTimeRulerStyle->SetFontSize(Settings->RulerFontSize);
	}
}
