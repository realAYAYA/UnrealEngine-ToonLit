// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveLinkDebugCurveNodeBase.h"

#include "LiveLinkDebuggerSettings.h"
#include "Styling/SlateColor.h"

	FLiveLinkDebugCurveNodeBase::FLiveLinkDebugCurveNodeBase(FName InCurveName, float InCurveValue)
		: CurveName(InCurveName)
		, CurveValue(InCurveValue)
	{}

	FSlateColor FLiveLinkDebugCurveNodeBase::GetCurveFillColor() const
	{
		const ULiveLinkDebuggerSettings* UISettings = GetDefault<ULiveLinkDebuggerSettings>(ULiveLinkDebuggerSettings::StaticClass());
		if (UISettings)
		{
			return UISettings->GetBarColorForCurveValue(CurveValue);
		}

		return FSlateColor(FLinearColor::Red);
	}