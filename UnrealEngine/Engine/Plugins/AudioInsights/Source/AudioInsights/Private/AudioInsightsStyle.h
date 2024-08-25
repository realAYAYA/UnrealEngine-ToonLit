// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Textures/SlateIcon.h"

#define LOCTEXT_NAMESPACE "AudioInsights"

namespace UE::Audio::Insights
{
	class FSlateStyle final : public FSlateStyleSet
	{
	public:
		static FSlateStyle& Get()
		{
			static FSlateStyle InsightsStyle;
			return InsightsStyle;
		}

		static FName GetStyleName()
		{
			const FLazyName StyleName = "AudioInsights";
			return StyleName.Resolve();
		}

		const FNumberFormattingOptions* GetAmpFloatFormat()
		{
			static FNumberFormattingOptions FloatFormat;
			FloatFormat.MinimumIntegralDigits = 1;
			FloatFormat.MaximumIntegralDigits = 1;
			FloatFormat.MinimumFractionalDigits = 3;
			FloatFormat.MaximumFractionalDigits = 3;
			return &FloatFormat;
		};

		const FNumberFormattingOptions* GetDefaultFloatFormat()
		{
			static FNumberFormattingOptions FloatFormat;
			FloatFormat.MinimumIntegralDigits = 1;
			FloatFormat.MinimumFractionalDigits = 4;
			FloatFormat.MaximumFractionalDigits = 4;
			return &FloatFormat;
		};

		const FNumberFormattingOptions* GetFreqFloatFormat()
		{
			static FNumberFormattingOptions FloatFormat;
			FloatFormat.MinimumIntegralDigits = 1;
			FloatFormat.MaximumIntegralDigits = 5;
			FloatFormat.MinimumFractionalDigits = 0;
			FloatFormat.MaximumFractionalDigits = 2;
			return &FloatFormat;
		};

		const FNumberFormattingOptions* GetPitchFloatFormat()
		{
			static FNumberFormattingOptions FloatFormat;
			FloatFormat.MinimumIntegralDigits = 1;
			FloatFormat.MaximumIntegralDigits = 3;
			FloatFormat.MinimumFractionalDigits = 3;
			FloatFormat.MaximumFractionalDigits = 3;
			return &FloatFormat;
		};

		const FNumberFormattingOptions* GetTimeFormat()
		{
			static FNumberFormattingOptions FloatFormat;
			FloatFormat.MinimumIntegralDigits = 1;
			FloatFormat.MinimumFractionalDigits = 3;
			FloatFormat.MaximumFractionalDigits = 3;
			return &FloatFormat;
		};

		FText FormatSecondsAsTime(float InTimeSec)
		{
			return FText::Format(LOCTEXT("TimeInSecondsFormat", "{0}s"), FText::AsNumber(InTimeSec, GetTimeFormat()));
		}

		FText FormatMillisecondsAsTime(float InTimeMS)
		{
			return FText::Format(LOCTEXT("TimeInMillisecondsFormat", "{0}ms"), FText::AsNumber(InTimeMS, GetTimeFormat()));
		}

		FSlateIcon CreateIcon(FName InName)
		{
			return { GetStyleName(), InName};
		}

		const FSlateBrush& GetBrushEnsured(FName InName)
		{
			const ISlateStyle* AudioInsightsStyle = FSlateStyleRegistry::FindSlateStyle(GetStyleName());
			if (ensureMsgf(AudioInsightsStyle, TEXT("Missing slate style '%s'"), *GetStyleName().ToString()))
			{
				const FSlateBrush* Brush = AudioInsightsStyle->GetBrush(InName);
				if (ensureMsgf(Brush, TEXT("Missing brush '%s'"), *InName.ToString()))
				{
					return *Brush;
				}
			}

			if (const FSlateBrush* NoBrush = FAppStyle::GetBrush("NoBrush"))
			{
				return *NoBrush;
			}

			return *DefaultBrush;
		}

		FSlateStyle()
			: FSlateStyleSet(GetStyleName())
		{
			SetParentStyleName(FAppStyle::GetAppStyleSetName());

			const FString PluginsDir = FPaths::EnginePluginsDir();

			SetContentRoot(PluginsDir / TEXT("AudioInsights/Content"));
			SetCoreContentRoot(PluginsDir / TEXT("Slate"));

			Set("AudioInsights.Analyzers.BackgroundColor", FLinearColor(0.0075f, 0.0075f, 0.0075f, 1.0f));

			const FVector2D Icon16(16.0f, 16.0f);
			const FVector2D Icon20(20.0f, 20.0f);
			const FVector2D Icon40(40.0f, 40.0f);
			const FVector2D Icon64(64.0f, 64.0f);

			Set("AudioInsights.Icon", new IMAGE_BRUSH_SVG(TEXT("Icons/audio_insights_icon"), Icon16));
			Set("AudioInsights.Icon.Dashboard", new IMAGE_BRUSH_SVG(TEXT("Icons/audio_dashboard"), Icon16));
			Set("AudioInsights.Icon.Event", new IMAGE_BRUSH_SVG(TEXT("Icons/audio_event"), Icon16));
			Set("AudioInsights.Icon.Log", new IMAGE_BRUSH_SVG(TEXT("Icons/audio_log"), Icon16));
			Set("AudioInsights.Icon.Sources", new IMAGE_BRUSH_SVG(TEXT("Icons/audio_sources"), Icon16));
			Set("AudioInsights.Icon.Submix", new IMAGE_BRUSH_SVG(TEXT("Icons/audio_submix"), Icon16));
			Set("AudioInsights.Icon.VirtualLoop", new IMAGE_BRUSH_SVG(TEXT("Icons/audio_virtualloop"), Icon16));
			Set("AudioInsights.Icon.Viewport", new IMAGE_BRUSH_SVG(TEXT("Icons/viewport"), Icon16));
			Set("AudioInsights.Icon.Open", new IMAGE_BRUSH_SVG(TEXT("Icons/open"), Icon20));
			Set("AudioInsights.Icon.ContentBrowser", new IMAGE_BRUSH_SVG(TEXT("Icons/content_browser"), Icon20));
			Set("AudioInsights.Icon.Start.Active", new IMAGE_BRUSH_SVG(TEXT("Icons/start_active"), Icon20));
			Set("AudioInsights.Icon.Start.Inactive", new IMAGE_BRUSH_SVG(TEXT("Icons/start_inactive"), Icon20));
			Set("AudioInsights.Icon.Record.Active", new IMAGE_BRUSH_SVG(TEXT("Icons/record_active"), Icon20));
			Set("AudioInsights.Icon.Record.Inactive", new IMAGE_BRUSH_SVG(TEXT("Icons/record_inactive"), Icon20));
			Set("AudioInsights.Icon.Stop.Active", new IMAGE_BRUSH_SVG(TEXT("Icons/stop_active"), Icon20));
			Set("AudioInsights.Icon.Stop.Inactive", new IMAGE_BRUSH_SVG(TEXT("Icons/stop_inactive"), Icon20));

			Set("AudioInsights.Thumbnail", new IMAGE_BRUSH_SVG(TEXT("Icons/audio_insights"), Icon16));

			FSlateStyleRegistry::RegisterSlateStyle(*this);
		}
	};
} // namespace UE::Audio::Insights
#undef LOCTEXT_NAMESPACE
