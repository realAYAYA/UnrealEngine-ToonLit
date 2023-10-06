// Copyright Epic Games, Inc. All Rights Reserved.

#include "CurveEditorSettings.h"

#include "HAL/PlatformCrt.h"

UCurveEditorSettings::UCurveEditorSettings()
{
	bAutoFrameCurveEditor = true;
	bShowBars = true;
	FrameInputPadding = 50;
	FrameOutputPadding = 50;
	bShowBufferedCurves = true;
	bShowCurveEditorCurveToolTips = true;
	TangentVisibility = ECurveEditorTangentVisibility::SelectedKeys;
	ZoomPosition = ECurveEditorZoomPosition::CurrentTime;
	bSnapTimeToSelection = false;

	SelectionColor = FLinearColor::White;
	ParentSpaceCustomColor = FLinearColor(.93, .31, .19); //pastel orange
	WorldSpaceCustomColor = FLinearColor(.198, .610, .558); //pastel teal

	TreeViewWidth = 0.3f;
}

bool UCurveEditorSettings::GetAutoFrameCurveEditor() const
{
	return bAutoFrameCurveEditor;
}

void UCurveEditorSettings::SetAutoFrameCurveEditor(bool InbAutoFrameCurveEditor)
{
	if (bAutoFrameCurveEditor != InbAutoFrameCurveEditor)
	{
		bAutoFrameCurveEditor = InbAutoFrameCurveEditor;
		SaveConfig();
	}
}

bool UCurveEditorSettings::GetShowBars() const
{
	return bShowBars;
}

void UCurveEditorSettings::SetShowBars(bool InValue)
{
	if (bShowBars != InValue)
	{
		bShowBars = InValue;
		SaveConfig();
	}
}

int32 UCurveEditorSettings::GetFrameInputPadding() const
{
	return FrameInputPadding;
}

void UCurveEditorSettings::SetFrameInputPadding(int32 InFrameInputPadding)
{
	if (FrameInputPadding != InFrameInputPadding)
	{
		FrameInputPadding = InFrameInputPadding;
		SaveConfig();
	}
}

int32 UCurveEditorSettings::GetFrameOutputPadding() const
{
	return FrameOutputPadding;
}

void UCurveEditorSettings::SetFrameOutputPadding(int32 InFrameOutputPadding)
{
	if (FrameOutputPadding != InFrameOutputPadding)
	{
		FrameOutputPadding = InFrameOutputPadding;
		SaveConfig();
	}
}

bool UCurveEditorSettings::GetShowBufferedCurves() const
{
	return bShowBufferedCurves;
}

void UCurveEditorSettings::SetShowBufferedCurves(bool InbShowBufferedCurves)
{
	if (bShowBufferedCurves != InbShowBufferedCurves)
	{
		bShowBufferedCurves = InbShowBufferedCurves;
		SaveConfig();
	}
}

bool UCurveEditorSettings::GetShowCurveEditorCurveToolTips() const
{
	return bShowCurveEditorCurveToolTips;
}

void UCurveEditorSettings::SetShowCurveEditorCurveToolTips(bool InbShowCurveEditorCurveToolTips)
{
	if (bShowCurveEditorCurveToolTips != InbShowCurveEditorCurveToolTips)
	{
		bShowCurveEditorCurveToolTips = InbShowCurveEditorCurveToolTips;
		SaveConfig();
	}
}

ECurveEditorTangentVisibility UCurveEditorSettings::GetTangentVisibility() const
{
	return TangentVisibility;
}

void UCurveEditorSettings::SetTangentVisibility(ECurveEditorTangentVisibility InTangentVisibility)
{
	if (TangentVisibility != InTangentVisibility)
	{
		TangentVisibility = InTangentVisibility;
		SaveConfig();
	}
}

ECurveEditorZoomPosition UCurveEditorSettings::GetZoomPosition() const
{
	return ZoomPosition;
}

void UCurveEditorSettings::SetZoomPosition(ECurveEditorZoomPosition InZoomPosition)
{
	if (ZoomPosition != InZoomPosition)
	{
		ZoomPosition = InZoomPosition;
		SaveConfig();
	}
}

bool UCurveEditorSettings::GetSnapTimeToSelection() const
{
	return bSnapTimeToSelection;
}

void UCurveEditorSettings::SetSnapTimeToSelection(bool bInSnapTimeToSelection)
{
	if (bSnapTimeToSelection != bInSnapTimeToSelection)
	{
		bSnapTimeToSelection = bInSnapTimeToSelection;
		SaveConfig();
	}
}

FLinearColor UCurveEditorSettings::GetSelectionColor() const
{
	return SelectionColor;
}

void UCurveEditorSettings::SetSelectionColor(const FLinearColor& InColor)
{
	if (SelectionColor != InColor)
	{
		SelectionColor = InColor;
		SaveConfig();
	}
}

TOptional<FLinearColor> UCurveEditorSettings::GetCustomColor(UClass* InClass, const FString& InPropertyName) const
{
	TOptional<FLinearColor> Color;
	for (const FCustomColorForChannel& CustomColor : CustomColors)
	{
		UClass* Class = CustomColor.Object.LoadSynchronous();
		if (Class == InClass && CustomColor.PropertyName == InPropertyName)
		{
			Color = CustomColor.Color;
			break;
		}
	}
	return Color;
}
void UCurveEditorSettings::SetCustomColor(UClass* InClass, const FString& InPropertyName, FLinearColor InColor)
{
	TOptional<FLinearColor> Color;
	for (FCustomColorForChannel& CustomColor : CustomColors)
	{
		UClass* Class = CustomColor.Object.LoadSynchronous();
		if (Class == InClass && CustomColor.PropertyName == InPropertyName)
		{
			CustomColor.Color = InColor;
			SaveConfig(); 
			return;
		}
	}
	FCustomColorForChannel NewColor;
	NewColor.Object = InClass;
	NewColor.PropertyName = InPropertyName;
	NewColor.Color = InColor;
	CustomColors.Add(NewColor);
	SaveConfig();
}

void UCurveEditorSettings::DeleteCustomColor(UClass* InClass, const FString& InPropertyName)
{
	TOptional<FLinearColor> Color;
	for (int32 Index = 0;Index < CustomColors.Num(); ++Index)
	{
		FCustomColorForChannel& CustomColor = CustomColors[Index];
		UClass* Class = CustomColor.Object.LoadSynchronous();
		if (Class == InClass && CustomColor.PropertyName == InPropertyName)
		{
			CustomColors.RemoveAt(Index);
			SaveConfig();
			return;
		}
	}
}

TOptional<FLinearColor> UCurveEditorSettings::GetSpaceSwitchColor(const FString& InControlName) const
{
	TOptional<FLinearColor> Color;

	if (InControlName == FString(TEXT("Parent")))
	{
		Color =  ParentSpaceCustomColor;
	}
	else if(InControlName == FString(TEXT("World")))
	{
		Color = WorldSpaceCustomColor;
	}
	else
	{
		for (const FCustomColorForSpaceSwitch& CustomColor : ControlSpaceCustomColors)
		{
			if (CustomColor.ControlName == InControlName)
			{
				Color = CustomColor.Color;
				break;
			}
		}
	}
	return Color;
}

void UCurveEditorSettings::SetSpaceSwitchColor(const FString& InControlName, FLinearColor InColor)
{
	if (InControlName == FString(TEXT("Parent")))
	{
		ParentSpaceCustomColor = InColor;
		SaveConfig();
	}
	else if (InControlName == FString(TEXT("World")))
	{
		WorldSpaceCustomColor = InColor;
		SaveConfig();
	}
	else
	{
		TOptional<FLinearColor> Color;
		for (FCustomColorForSpaceSwitch& CustomColor : ControlSpaceCustomColors)
		{
			if (CustomColor.ControlName == InControlName)
			{
				CustomColor.Color = InColor;
				SaveConfig();
				return;
			}
		}
		FCustomColorForSpaceSwitch NewColor;
		NewColor.ControlName = InControlName;
		NewColor.Color = InColor;
		ControlSpaceCustomColors.Add(NewColor);
		SaveConfig();
	}
}

void UCurveEditorSettings::DeleteSpaceSwitchColor(const FString& InControlName)
{
	TOptional<FLinearColor> Color;
	for (int32 Index = 0; Index < ControlSpaceCustomColors.Num(); ++Index)
	{
		FCustomColorForSpaceSwitch& CustomColor = ControlSpaceCustomColors[Index];
		if (CustomColor.ControlName == InControlName)
		{
			ControlSpaceCustomColors.RemoveAt(Index);
			SaveConfig();
			return;
		}
	}
}

FLinearColor UCurveEditorSettings::GetNextRandomColor()
{
	static TArray<FLinearColor> IndexedColor;
	static int32 NextIndex = 0;
	if (IndexedColor.Num() == 0)
	{
		IndexedColor.Add(FLinearColor(.904, .323, .539)); //pastel red
		IndexedColor.Add(FLinearColor(.552, .737, .328)); //pastel green
		IndexedColor.Add(FLinearColor(.947, .418, .219)); //pastel orange
		IndexedColor.Add(FLinearColor(.156, .624, .921)); //pastel blue
		IndexedColor.Add(FLinearColor(.921, .314, .337)); //pastel red 2
		IndexedColor.Add(FLinearColor(.361, .651, .332)); //pastel green 2
		IndexedColor.Add(FLinearColor(.982, .565, .254)); //pastel orange 2
		IndexedColor.Add(FLinearColor(.246, .223, .514)); //pastel purple
		IndexedColor.Add(FLinearColor(.208, .386, .687)); //pastel blue2
		IndexedColor.Add(FLinearColor(.223, .590, .337)); //pastel green 3
		IndexedColor.Add(FLinearColor(.230, .291, .591)); //pastel blue 3
	}
	FLinearColor Color = IndexedColor[NextIndex];
	++NextIndex;
	int32 NewIndex = (NextIndex % IndexedColor.Num());
	NextIndex = NewIndex;
	return Color;
}

void UCurveEditorSettings::SetTreeViewWidth(float InTreeViewWidth)
{
	if (InTreeViewWidth != TreeViewWidth)
	{
		TreeViewWidth = InTreeViewWidth;
		SaveConfig();
	}
}

void UCurveEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	FName MemberPropertyName = (PropertyChangedEvent.MemberProperty != NULL) ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(UCurveEditorSettings, CustomColors) ||
		MemberPropertyName == GET_MEMBER_NAME_CHECKED(UCurveEditorSettings, CustomColors))
	{
		OnCustomColorsChangedEvent.Broadcast();
	}
	
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
