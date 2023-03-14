// Copyright Epic Games, Inc. All Rights Reserved.

#include "SWaveformEditorTimeRuler.h"

#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "WaveformEditorRenderData.h"
#include "WaveformEditorTransportCoordinator.h"


#define LOCTEXT_NAMESPACE "WaveformEditorTimeRuler"

void SWaveformEditorTimeRuler::Construct(const FArguments& InArgs, TSharedRef<FWaveformEditorTransportCoordinator> InTransportCoordinator, TSharedRef<FWaveformEditorRenderData> InRenderData)
{
	TransportCoordinator = InTransportCoordinator;
	RenderData = InRenderData;

	DisplayUnit = InArgs._DisplayUnit;

	Style = InArgs._Style;
	
	check(Style);
	HandleColor = Style->HandleColor;
	HandleWidth = Style->HandleWidth;
	HandleBrush = Style->HandleBrush;
	BackgroundColor = Style->BackgroundColor;
	BackgroundBrush = Style->BackgroundBrush;
	TicksColor = Style->TicksColor;
	TicksTextColor = Style->TicksTextColor;
	TicksTextFont = Style->TicksTextFont;
	DesiredWidth = Style->DesiredWidth;
	DesiredHeight = Style->DesiredHeight;
	TicksTextOffset = Style->TicksTextOffset;
}

FVector2D SWaveformEditorTimeRuler::ComputeDesiredSize(float) const
{
	return FVector2D(DesiredWidth, DesiredHeight);
}

int32 SWaveformEditorTimeRuler::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		++LayerId,
		AllottedGeometry.ToPaintGeometry(),
		&BackgroundBrush,
		ESlateDrawEffect::None,
		BackgroundColor.GetSpecifiedColor());
	
	DrawRulerTicks(AllottedGeometry, OutDrawElements, LayerId);
	DrawPlayheadHandle(AllottedGeometry, OutDrawElements, LayerId);

	return LayerId;
}

void SWaveformEditorTimeRuler::DrawRulerTicks(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	const double MinorGridXStep = GridMetrics.MajorGridXStep / GridMetrics.NumMinorGridDivisions;
	const double MajorTickY = AllottedGeometry.Size.Y * 0.25;
	const double MinorTickY = AllottedGeometry.Size.Y * 0.75;

	TArray<FVector2D> LinePoints;
	LinePoints.SetNumUninitialized(2);

	for (double CurrentMajorTickX = GridMetrics.FirstMajorTickX; CurrentMajorTickX < AllottedGeometry.Size.X; CurrentMajorTickX += GridMetrics.MajorGridXStep)
	{
		const double MajorTickX = CurrentMajorTickX;
		
		LinePoints[0] = FVector2D(MajorTickX, MajorTickY);
		LinePoints[1] = FVector2D(MajorTickX, AllottedGeometry.Size.Y);

		FSlateDrawElement::MakeLines(
			OutDrawElements,
			++LayerId,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			TicksColor.GetSpecifiedColor(),
			false);


		for (int32 MinorTickIndex = 1; MinorTickIndex < GridMetrics.NumMinorGridDivisions; ++MinorTickIndex)
		{
			const double MinorTickX = MajorTickX + MinorGridXStep * MinorTickIndex;
			
			LinePoints[0] = FVector2D(MinorTickX, MinorTickY);
			LinePoints[1] = FVector2D(MinorTickX, AllottedGeometry.Size.Y);

			FSlateDrawElement::MakeLines(
				OutDrawElements,
				++LayerId,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				ESlateDrawEffect::None,
				TicksColor.GetSpecifiedColor(),
				false);

		}

		float TickTimeSeconds = MajorTickX / GridMetrics.PixelsPerSecond + GridMetrics.StartTime;
		DrawTickTimeString(TickTimeSeconds, MajorTickX, MajorTickY, OutDrawElements, LayerId, AllottedGeometry);
	}
}

void SWaveformEditorTimeRuler::DrawTickTimeString(float TickTimeSeconds, const double TickX, const double TickY, FSlateWindowElementList& OutDrawElements, int32& LayerId, const FGeometry& AllottedGeometry) const
{
	FString TimeString;

	switch (DisplayUnit)
	{
	case EWaveformEditorDisplayUnit::Samples:
	{
		uint32 SampleTime = TickTimeSeconds * RenderData->GetSampleRate();
		TimeString = FString::Printf(TEXT("%d"), SampleTime);
	}
	break;
	case EWaveformEditorDisplayUnit::Seconds:
		TimeString = FString::Printf(TEXT("%.3f"), TickTimeSeconds);
		break;
	default:
		TimeString = FString::Printf(TEXT("%.3f"), TickTimeSeconds);
		break;
	}

	FVector2D TextOffset(TickX + TicksTextOffset, TickY);

	FSlateDrawElement::MakeText(
		OutDrawElements,
		++LayerId,
		AllottedGeometry.ToPaintGeometry(TextOffset, AllottedGeometry.Size),
		TimeString,
		TicksTextFont,
		ESlateDrawEffect::None,
		TicksTextColor.GetSpecifiedColor()
	);
}

TSharedRef<SWidget> SWaveformEditorTimeRuler::MakeContextMenu()
{
	const bool bCloseAfterSelection = false;
	const bool bCloseSelfOnly = false;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, NULL, TSharedPtr<FExtender>(), bCloseSelfOnly, &FCoreStyle::Get());
	MenuBuilder.BeginSection("Ruler Options", LOCTEXT("RulerOptionsHeading", "Options"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("TimeUnitsSubMenuLabel", "Time Unit"),
			LOCTEXT("TimeUnitsSubMenuLabelToolTip", "Select the time unit shown by the grid"),
			FNewMenuDelegate::CreateSP(this, &SWaveformEditorTimeRuler::MakeTimeUnitsSubMenu)
		);
	}
	MenuBuilder.EndSection();

	
	return MenuBuilder.MakeWidget();
}

void SWaveformEditorTimeRuler::MakeTimeUnitsSubMenu(FMenuBuilder& SubMenuBuilder)
{
	SubMenuBuilder.BeginSection(NAME_None);

	SubMenuBuilder.AddMenuEntry(
		LOCTEXT("TimeUnitsSubMenuEntry_Seconds", "Seconds"),
		LOCTEXT("TimeUnitsSubMenuEntry_Seconds", "Seconds"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SWaveformEditorTimeRuler::NotifyTimeUnitMenuSelection, EWaveformEditorDisplayUnit::Seconds),
			FCanExecuteAction::CreateLambda([this] { return DisplayUnit != EWaveformEditorDisplayUnit::Seconds; }),
			FIsActionChecked::CreateLambda([this] { return DisplayUnit == EWaveformEditorDisplayUnit::Seconds;})
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	SubMenuBuilder.AddMenuEntry(
		LOCTEXT("TimeUnitsSubMenuEntry_Frames", "Frames"),
		LOCTEXT("TimeUnitsSubMenuEntry_Frames", "Frames"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &SWaveformEditorTimeRuler::NotifyTimeUnitMenuSelection, EWaveformEditorDisplayUnit::Samples),
			FCanExecuteAction::CreateLambda([this] { return DisplayUnit != EWaveformEditorDisplayUnit::Samples; }),
			FIsActionChecked::CreateLambda([this] { return DisplayUnit == EWaveformEditorDisplayUnit::Samples; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	SubMenuBuilder.EndSection();
 }

void SWaveformEditorTimeRuler::NotifyTimeUnitMenuSelection(const EWaveformEditorDisplayUnit SelectedDisplayUnit) const
{
	if (SelectedDisplayUnit == DisplayUnit)
	{
		return;
	}

	if (OnTimeUnitMenuSelection.IsBound())
	{
		OnTimeUnitMenuSelection.Broadcast(SelectedDisplayUnit);
	}
}

void SWaveformEditorTimeRuler::UpdateGridMetrics(const FWaveEditorGridMetrics& InMetrics)
{
	GridMetrics = InMetrics;
}

void SWaveformEditorTimeRuler::UpdateDisplayUnit(const EWaveformEditorDisplayUnit InDisplayUnit)
{
	DisplayUnit = InDisplayUnit;
}

void SWaveformEditorTimeRuler::OnStyleUpdated(const FWaveformEditorWidgetStyleBase* UpdatedStyle)
{
	check(UpdatedStyle);
	check(Style);

	if (UpdatedStyle != Style)
	{
		return;
	}

	HandleColor = Style->HandleColor;
	HandleWidth = Style->HandleWidth;
	HandleBrush = Style->HandleBrush;
	BackgroundColor = Style->BackgroundColor;
	BackgroundBrush = Style->BackgroundBrush;
	TicksColor = Style->TicksColor;
	TicksTextColor = Style->TicksTextColor;
	TicksTextFont = Style->TicksTextFont;
	DesiredWidth = Style->DesiredWidth;
	DesiredHeight = Style->DesiredHeight;
	TicksTextOffset = Style->TicksTextOffset;
}

void SWaveformEditorTimeRuler::DrawPlayheadHandle(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	const float WindowWidth = AllottedGeometry.Size.X;
	const float	HandleStart = TransportCoordinator->GetPlayheadPosition() * WindowWidth - HandleWidth / 2;
	const float	HandleEnd = HandleStart + HandleWidth;
 	FPaintGeometry HandleGeometry = AllottedGeometry.ToPaintGeometry(FVector2D(HandleStart, 0), FVector2D(HandleEnd - HandleStart, AllottedGeometry.Size.Y));

	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId,
		HandleGeometry,
		&HandleBrush,
		ESlateDrawEffect::None,
		HandleColor.GetSpecifiedColor()
	);

	++LayerId;
}

FReply SWaveformEditorTimeRuler::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return TransportCoordinator->ReceiveMouseButtonDown(*this, MyGeometry, MouseEvent);
}

FReply SWaveformEditorTimeRuler::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const bool HandleRightMouseButton = MouseEvent.GetEffectingButton() == EKeys::RightMouseButton;

	if (HandleRightMouseButton)
	{
		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, MakeContextMenu(), FSlateApplication::Get().GetCursorPos(), FPopupTransitionEffect::ContextMenu);
		return FReply::Handled();
	}

	return TransportCoordinator->ReceiveMouseButtonUp(*this, MyGeometry, MouseEvent);
}

FReply SWaveformEditorTimeRuler::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return TransportCoordinator->ReceiveMouseMove(*this, MyGeometry, MouseEvent);
}

#undef LOCTEXT_NAMESPACE