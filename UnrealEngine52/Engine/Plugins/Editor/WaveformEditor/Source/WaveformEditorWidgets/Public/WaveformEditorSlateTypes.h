// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Delegates/Delegate.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateWidgetStyle.h"

#include "WaveformEditorSlateTypes.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWidgetStyleUpdated, const FWaveformEditorWidgetStyleBase* /*Updated Widget Style*/);

USTRUCT()
struct FWaveformEditorWidgetStyleBase : public FSlateWidgetStyle
{
	GENERATED_USTRUCT_BODY()

	FWaveformEditorWidgetStyleBase() = default;
	virtual ~FWaveformEditorWidgetStyleBase() = default;

	virtual void BroadcastStyleUpdate() const { OnStyleUpdated.Broadcast(this); }

	FOnWidgetStyleUpdated OnStyleUpdated;
};

/**
 * Represents the appearance of a Waveform Viewer
 */
USTRUCT(BlueprintType)
struct FWaveformViewerStyle : public FWaveformEditorWidgetStyleBase
{
	GENERATED_USTRUCT_BODY()

	FWaveformViewerStyle();

	static const FWaveformViewerStyle& GetDefault();
	virtual const FName GetTypeName() const override { return TypeName; };
	virtual void GetResources(TArray< const FSlateBrush* >& OutBrushes) const override;

	static const FName TypeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor WaveformColor;
	FWaveformViewerStyle& SetWaveformColor(const FSlateColor InWaveformColor) { WaveformColor = InWaveformColor; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float WaveformLineThickness;
	FWaveformViewerStyle& SetWaveformLineThickness(const float InWaveformLineThickness) { WaveformLineThickness = InWaveformLineThickness; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor MajorGridLineColor;
	FWaveformViewerStyle& SetMajorGridLineColor(const FSlateColor InMajorGridLineColor) { MajorGridLineColor = InMajorGridLineColor; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor MinorGridLineColor;
	FWaveformViewerStyle& SetMinorGridLineColor(const FSlateColor InMinorGridLineColor) { MinorGridLineColor = InMinorGridLineColor; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor ZeroCrossingLineColor;
	FWaveformViewerStyle& SetZeroCrossingLineColor(const FSlateColor InZeroCrossingLineColor) { ZeroCrossingLineColor = InZeroCrossingLineColor; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float ZeroCrossingLineThickness;
	FWaveformViewerStyle& SetZeroCrossingLineThickness(const float InZeroCrossingLineThickness) { ZeroCrossingLineThickness = InZeroCrossingLineThickness; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float SampleMarkersSize;
	FWaveformViewerStyle& SetSampleMarkersSize(const float InSampleMarkersSize) { SampleMarkersSize = InSampleMarkersSize; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor WaveformBackgroundColor;
	FWaveformViewerStyle& SetBackgroundColor(const FSlateColor InBackgroundColor) { WaveformBackgroundColor = InBackgroundColor; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush BackgroundBrush;
	FWaveformViewerStyle& SetBackgroundBrush(const FSlateBrush InBackgroundBrush) { BackgroundBrush = InBackgroundBrush; BroadcastStyleUpdate(); return *this; }
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float DesiredWidth;
	FWaveformViewerStyle& SetDesiredWidth(const float InDesiredWidth) { DesiredWidth = InDesiredWidth; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float DesiredHeight;
	FWaveformViewerStyle& SetDesiredHeight(const float InDesiredHeight) { DesiredHeight = InDesiredHeight; BroadcastStyleUpdate(); return *this; }
};

/**
 * Represents the appearance of a Waveform Viewer Overlay style
 */
USTRUCT(BlueprintType)
struct FWaveformViewerOverlayStyle : public FWaveformEditorWidgetStyleBase
{
	GENERATED_USTRUCT_BODY()

	FWaveformViewerOverlayStyle();

	static const FWaveformViewerOverlayStyle& GetDefault();
	virtual const FName GetTypeName() const override { return TypeName; };

	static const FName TypeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor PlayheadColor;
	FWaveformViewerOverlayStyle& SetPlayheadColor(const FSlateColor InPlayheadColor) { PlayheadColor = InPlayheadColor; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float PlayheadWidth;
	FWaveformViewerOverlayStyle& SetPlayheadWidth(const float InPlayheadWidth) { PlayheadWidth = InPlayheadWidth; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float DesiredWidth;
	FWaveformViewerOverlayStyle& SetDesiredWidth(const float InDesiredWidth) { DesiredWidth = InDesiredWidth; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float DesiredHeight;
	FWaveformViewerOverlayStyle& SetDesiredHeight(const float InDesiredHeight) { DesiredHeight = InDesiredHeight; BroadcastStyleUpdate(); return *this; }
};

/**
 * Represents the appearance of a Waveform Editor Time Ruler
 */
USTRUCT(BlueprintType)
struct FWaveformEditorTimeRulerStyle : public FWaveformEditorWidgetStyleBase
{
	GENERATED_USTRUCT_BODY()

	FWaveformEditorTimeRulerStyle();

	static const FWaveformEditorTimeRulerStyle& GetDefault();
	virtual const FName GetTypeName() const override { return TypeName; };
	virtual void GetResources(TArray< const FSlateBrush* >& OutBrushes) const override;

	static const FName TypeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float HandleWidth;
	FWaveformEditorTimeRulerStyle& SetHandleWidth(const float InHandleWidth) { HandleWidth = InHandleWidth; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor HandleColor;
	FWaveformEditorTimeRulerStyle& SetHandleColor(const FSlateColor& InHandleColor) { HandleColor = InHandleColor; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush HandleBrush;
	FWaveformEditorTimeRulerStyle& SetHandleBrush(const FSlateBrush& InHandleBrush) { HandleBrush = InHandleBrush; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor TicksColor;
	FWaveformEditorTimeRulerStyle& SetTicksColor(const FSlateColor& InTicksColor) { TicksColor = InTicksColor; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor TicksTextColor;
	FWaveformEditorTimeRulerStyle& SetTicksTextColor(const FSlateColor& InTicksTextColor) { TicksTextColor = InTicksTextColor; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateFontInfo TicksTextFont;
	FWaveformEditorTimeRulerStyle& SetTicksTextFont(const FSlateFontInfo& InTicksTextFont) { TicksTextFont = InTicksTextFont; BroadcastStyleUpdate(); return *this; }
	FWaveformEditorTimeRulerStyle& SetFontSize(const float InFontSize) {TicksTextFont.Size = InFontSize; BroadcastStyleUpdate(); return *this;	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float TicksTextOffset;
	FWaveformEditorTimeRulerStyle& SetTicksTextOffset(const float InTicksTextOffset) { TicksTextOffset = InTicksTextOffset; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateColor BackgroundColor;
	FWaveformEditorTimeRulerStyle& SetBackgroundColor(const FSlateColor& InBackgroundColor) { BackgroundColor = InBackgroundColor; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	FSlateBrush BackgroundBrush;
	FWaveformEditorTimeRulerStyle& SetBackgroundBrush(const FSlateBrush& InBackgroundBrush) { BackgroundBrush = InBackgroundBrush; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float DesiredWidth;
	FWaveformEditorTimeRulerStyle& SetDesiredWidth(const float InDesiredWidth) { DesiredWidth = InDesiredWidth; BroadcastStyleUpdate(); return *this; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Appearance)
	float DesiredHeight;
	FWaveformEditorTimeRulerStyle& SetDesiredHeight(const float InDesiredHeight) { DesiredHeight = InDesiredHeight; BroadcastStyleUpdate(); return *this; }
};