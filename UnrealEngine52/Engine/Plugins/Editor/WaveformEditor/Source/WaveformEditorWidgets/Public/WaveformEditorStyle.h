// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateStyle.h"
#include "WaveformEditorSlateTypes.h"

class UWaveformEditorWidgetsSettings;

class FWaveformEditorStyle
	: public FSlateStyleSet

{
public:
	FWaveformEditorStyle();
	~FWaveformEditorStyle();

	static FName StyleName;

	/** Access the singleton instance for this style set */
	static FWaveformEditorStyle& Get();
	static void Init();

	template< typename SlateWidgetStyle >
	static TSharedRef<SlateWidgetStyle> GetRegisteredWidgetStyle(const FName& InStyleName)
	{
		TSharedRef<FSlateWidgetStyle>* WidgetStyleBase = StyleInstance->WidgetStyleValues.Find(InStyleName);
		check(WidgetStyleBase);
		TSharedRef<SlateWidgetStyle> WidgetStyle = StaticCastSharedRef<SlateWidgetStyle>(*WidgetStyleBase);
		return WidgetStyle;
	}

	
private:
	static TUniquePtr<FWaveformEditorStyle> StyleInstance;

	static const UWaveformEditorWidgetsSettings* GetWidgetsSettings();
	static void  OnWidgetSettingsUpdated(const FName& PropertyName, const UWaveformEditorWidgetsSettings* Settings);
};