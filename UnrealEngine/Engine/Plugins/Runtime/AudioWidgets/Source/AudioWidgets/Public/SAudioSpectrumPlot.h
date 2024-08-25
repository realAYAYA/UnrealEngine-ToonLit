// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AudioSpectrumPlotStyle.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/SlateDelegates.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateWidgetStyleAsset.h"
#include "Widgets/SCompoundWidget.h"

UENUM(BlueprintType)
enum class EAudioSpectrumPlotFrequencyAxisScale : uint8
{
	Linear,
	Logarithmic,
};

UENUM(BlueprintType)
enum class EAudioSpectrumPlotFrequencyAxisPixelBucketMode : uint8
{
	Sample UMETA(ToolTip = "Plot one data point per frequency axis pixel bucket only, choosing the data point nearest the pixel center."),
	Peak UMETA(ToolTip = "Plot one data point per frequency axis pixel bucket only, choosing the data point with the highest sound level."),
	Average UMETA(ToolTip = "Plot the average of the data points in each frequency axis pixel bucket."),
};

/**
 * Utility class for converting between spectrum data and local/absolute screen space.
 */
class FAudioSpectrumPlotScaleInfo
{
public:
	FAudioSpectrumPlotScaleInfo(const FVector2f InWidgetSize, EAudioSpectrumPlotFrequencyAxisScale InFrequencyAxisScale, float InViewMinFrequency, float InViewMaxFrequency, float InViewMinSoundLevel, float InViewMaxSoundLevel)
		: WidgetSize(InWidgetSize)
		, FrequencyAxisScale(InFrequencyAxisScale)
		, TransformedViewMinFrequency(ForwardTransformFrequency(InViewMinFrequency))
		, TransformedViewMaxFrequency(ForwardTransformFrequency(InViewMaxFrequency))
		, TransformedViewFrequencyRange(TransformedViewMaxFrequency - TransformedViewMinFrequency)
		, PixelsPerTransformedHz((TransformedViewFrequencyRange > 0.0f) ? (InWidgetSize.X / TransformedViewFrequencyRange) : 0.0f)
		, ViewMinSoundLevel(InViewMinSoundLevel)
		, ViewMaxSoundLevel(InViewMaxSoundLevel)
		, ViewSoundLevelRange(InViewMaxSoundLevel - InViewMinSoundLevel)
		, PixelsPerDecibel((ViewSoundLevelRange > 0.0f) ? (InWidgetSize.Y / ViewSoundLevelRange) : 0.0f)
	{
		//
	}

	float LocalXToFrequency(float ScreenX) const
	{
		const float TransformedFrequency = (PixelsPerTransformedHz != 0.0f) ? ((ScreenX / PixelsPerTransformedHz) + TransformedViewMinFrequency) : 0.0f;
		return InverseTransformFrequency(TransformedFrequency);
	}

	float FrequencyToLocalX(float Frequency) const
	{
		return (ForwardTransformFrequency(Frequency) - TransformedViewMinFrequency) * PixelsPerTransformedHz;
	}

	float LocalYToSoundLevel(float ScreenY) const
	{
		return (PixelsPerDecibel != 0.0f) ? (ViewMaxSoundLevel - (ScreenY / PixelsPerDecibel)) : 0.0f;
	}

	float SoundLevelToLocalY(float SoundLevel) const
	{
		return (ViewMaxSoundLevel - SoundLevel) * PixelsPerDecibel;
	}

	FVector2f ToLocalPos(const FVector2f& FrequencyAndSoundLevel) const
	{
		return { FrequencyToLocalX(FrequencyAndSoundLevel.X), SoundLevelToLocalY(FrequencyAndSoundLevel.Y) };
	}

private:
	float ForwardTransformFrequency(float Frequency) const
	{
		return (FrequencyAxisScale == EAudioSpectrumPlotFrequencyAxisScale::Logarithmic) ? FMath::Loge(Frequency) : Frequency;
	}

	float InverseTransformFrequency(float TransformedFrequency) const
	{
		return (FrequencyAxisScale == EAudioSpectrumPlotFrequencyAxisScale::Logarithmic) ? FMath::Exp(TransformedFrequency) : TransformedFrequency;
	}

	const FVector2f WidgetSize;

	const EAudioSpectrumPlotFrequencyAxisScale FrequencyAxisScale;
	const float TransformedViewMinFrequency;
	const float TransformedViewMaxFrequency;
	const float TransformedViewFrequencyRange;
	const float PixelsPerTransformedHz;

	const float ViewMinSoundLevel;
	const float ViewMaxSoundLevel;
	const float ViewSoundLevelRange;
	const float PixelsPerDecibel;
};

/**
 * The audio spectrum data to plot.
 */
struct FAudioPowerSpectrumData
{
	TConstArrayView<float> CenterFrequencies;
	TConstArrayView<float> SquaredMagnitudes;
};

DECLARE_DELEGATE_RetVal(FAudioPowerSpectrumData, FGetAudioSpectrumData);

/**
 * Slate Widget for plotting an audio power spectrum, with linear or log frequency scale, and decibels sound levels.
 */
class AUDIOWIDGETS_API SAudioSpectrumPlot : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAudioSpectrumPlot)
		: _Style(&FAudioSpectrumPlotStyle::GetDefault())
		, _ViewMinFrequency(20.0f)
		, _ViewMaxFrequency(20000.0f)
		, _ViewMinSoundLevel(-60.0f)
		, _ViewMaxSoundLevel(12.0f)
		, _DisplayFrequencyAxisLabels(true)
		, _DisplaySoundLevelAxisLabels(true)
		, _FrequencyAxisScale(EAudioSpectrumPlotFrequencyAxisScale::Logarithmic)
		, _FrequencyAxisPixelBucketMode(EAudioSpectrumPlotFrequencyAxisPixelBucketMode::Average)
		, _BackgroundColor(FSlateColor::UseStyle())
		, _GridColor(FSlateColor::UseStyle())
		, _AxisLabelColor(FSlateColor::UseStyle())
		, _SpectrumColor(FSlateColor::UseStyle())
		, _AllowContextMenu(true)
	{}
		SLATE_STYLE_ARGUMENT(FAudioSpectrumPlotStyle, Style)
		SLATE_ATTRIBUTE(float, ViewMinFrequency)
		SLATE_ATTRIBUTE(float, ViewMaxFrequency)
		SLATE_ATTRIBUTE(float, ViewMinSoundLevel)
		SLATE_ATTRIBUTE(float, ViewMaxSoundLevel)
		SLATE_ATTRIBUTE(bool, DisplayFrequencyAxisLabels)
		SLATE_ATTRIBUTE(bool, DisplaySoundLevelAxisLabels)
		SLATE_ATTRIBUTE(EAudioSpectrumPlotFrequencyAxisScale, FrequencyAxisScale)
		SLATE_ATTRIBUTE(EAudioSpectrumPlotFrequencyAxisPixelBucketMode, FrequencyAxisPixelBucketMode)
		SLATE_ATTRIBUTE(FSlateColor, BackgroundColor)
		SLATE_ATTRIBUTE(FSlateColor, GridColor)
		SLATE_ATTRIBUTE(FSlateColor, AxisLabelColor)
		SLATE_ATTRIBUTE(FSlateColor, SpectrumColor)
		SLATE_ATTRIBUTE(bool, AllowContextMenu)
		SLATE_EVENT(FOnContextMenuOpening, OnContextMenuOpening)
		SLATE_EVENT(FGetAudioSpectrumData, OnGetAudioSpectrumData)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	void SetViewMinFrequency(float InViewMinFrequency) { ViewMinFrequency = InViewMinFrequency; }
	void SetViewMaxFrequency(float InViewMaxFrequency) { ViewMaxFrequency = InViewMaxFrequency; }
	void SetViewMinSoundLevel(float InViewMinSoundLevel) { ViewMinSoundLevel = InViewMinSoundLevel; }
	void SetViewMaxSoundLevel(float InViewMaxSoundLevel) { ViewMaxSoundLevel = InViewMaxSoundLevel; }
	void SetDisplayFrequencyAxisLabels(bool bInDisplayFrequencyAxisLabels) { bDisplayFrequencyAxisLabels = bInDisplayFrequencyAxisLabels; }
	void SetDisplaySoundLevelAxisLabels(bool bInDisplaySoundLevelAxisLabels) { bDisplaySoundLevelAxisLabels = bInDisplaySoundLevelAxisLabels; }
	void SetFrequencyAxisScale(EAudioSpectrumPlotFrequencyAxisScale InFrequencyAxisScale) { FrequencyAxisScale = InFrequencyAxisScale; }
	void SetFrequencyAxisPixelBucketMode(EAudioSpectrumPlotFrequencyAxisPixelBucketMode InFrequencyAxisPixelBucketMode) { FrequencyAxisPixelBucketMode = InFrequencyAxisPixelBucketMode; }
	void SetAllowContextMenu(bool bInAllowContextMenu) { bAllowContextMenu = bInAllowContextMenu; }

	TSharedRef<const FExtensionBase> AddContextMenuExtension(EExtensionHook::Position HookPosition, const TSharedPtr<FUICommandList>& CommandList, const FMenuExtensionDelegate& MenuExtensionDelegate);
	void RemoveContextMenuExtension(const TSharedRef<const FExtensionBase>& Extension);

	// Begin SWidget overrides.
	virtual FReply OnMouseButtonDown(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	// End SWidget overrides.

	void UnbindOnGetAudioSpectrumData() { OnGetAudioSpectrumData.Unbind(); }

private:
	// Begin SWidget overrides.
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
	// End SWidget overrides.

	int32 DrawSolidBackgroundRectangle(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle) const;

	int32 DrawGridAndLabels(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, const FAudioSpectrumPlotScaleInfo& ScaleInfo) const;
	void GetGridLineSoundLevels(TArray<float>& GridLineSoundLevels) const;
	void GetGridLineFrequencies(TArray<float>& AllGridLineFrequencies, TArray<float>& MajorGridLineFrequencies) const;

	int32 DrawPowerSpectrum(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, const FAudioSpectrumPlotScaleInfo& ScaleInfo) const;
	FAudioPowerSpectrumData GetPowerSpectrum() const;

	// This is a function to reduce the given array of data points to a possibly shorter array of points that will form the line to be plotted.
	// Where multiple data points map to the same frequency axis pixel bucket, the given 'cost function' will be used to select the best data point (the data point with the lowest 'cost').
	static void GetSpectrumLinePoints(TArray<FVector2f>& OutLinePoints, TConstArrayView<FVector2f> DataPoints, const FAudioSpectrumPlotScaleInfo& ScaleInfo, TFunctionRef<float(const FVector2f& DataPoint)> CostFunction);

	FLinearColor GetBackgroundColor(const FWidgetStyle& InWidgetStyle) const;
	FLinearColor GetGridColor(const FWidgetStyle& InWidgetStyle) const;
	FLinearColor GetAxisLabelColor(const FWidgetStyle& InWidgetStyle) const;
	FLinearColor GetSpectrumColor(const FWidgetStyle& InWidgetStyle) const;	

	TSharedRef<SWidget> BuildDefaultContextMenu();
	void BuildFrequencyAxisScaleSubMenu(FMenuBuilder& SubMenu);
	void BuildFrequencyAxisPixelBucketModeSubMenu(FMenuBuilder& SubMenu);

	static FName ContextMenuExtensionHook;
	TSharedPtr<FExtender> ContextMenuExtender;

	const FAudioSpectrumPlotStyle* Style;
	TAttribute<float> ViewMinFrequency;
	TAttribute<float> ViewMaxFrequency;
	TAttribute<float> ViewMinSoundLevel;
	TAttribute<float> ViewMaxSoundLevel;
	TAttribute<bool> bDisplayFrequencyAxisLabels;
	TAttribute<bool> bDisplaySoundLevelAxisLabels;
	TAttribute<EAudioSpectrumPlotFrequencyAxisScale> FrequencyAxisScale;
	TAttribute<EAudioSpectrumPlotFrequencyAxisPixelBucketMode> FrequencyAxisPixelBucketMode;
	TAttribute<FSlateColor> BackgroundColor;
	TAttribute<FSlateColor> GridColor;
	TAttribute<FSlateColor> AxisLabelColor;
	TAttribute<FSlateColor> SpectrumColor;
	TAttribute<bool> bAllowContextMenu;
	FOnContextMenuOpening OnContextMenuOpening;
	FGetAudioSpectrumData OnGetAudioSpectrumData;
};
