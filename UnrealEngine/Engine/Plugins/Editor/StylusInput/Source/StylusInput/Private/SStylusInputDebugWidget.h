// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

#include "IStylusInputModule.h"
#include "IStylusState.h"

class SStylusInputDebugWidget : public SCompoundWidget, 
	public IStylusMessageHandler
{
public:
	SStylusInputDebugWidget();
	virtual ~SStylusInputDebugWidget();

	SLATE_BEGIN_ARGS(SStylusInputDebugWidget)
	{}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, UStylusInputSubsystem& InSubsystem);
	void OnStylusStateChanged(const FStylusState& InState, int32 InIndex)
	{
		State = InState;
		LastIndex = InIndex;
	}

private:

	UStylusInputSubsystem* InputSubsystem;
	FStylusState State;
	int32 LastIndex;

	ECheckBoxState IsTouching() const;
	ECheckBoxState IsInverted() const;

	FText GetPositionText() const { return GetVector2Text(State.GetPosition()); }
	FText GetTiltText() const { return GetVector2Text(State.GetTilt()); }
	FText GetSizeText() const { return GetVector2Text(State.GetSize()); }

	FText GetIndexText() const { return FText::FromString(FString::FromInt(LastIndex)); }

	FText GetPressureText() const { return GetFloatText(State.GetPressure()); }
	FText GetTangentPressureText() const { return GetFloatText(State.GetTangentPressure()); }
	FText GetZText() const { return GetFloatText(State.GetZ()); }
	FText GetTwistText() const { return GetFloatText(State.GetTwist()); }

	static FText GetVector2Text(FVector2D Value);
	static FText GetFloatText(float Value);
};