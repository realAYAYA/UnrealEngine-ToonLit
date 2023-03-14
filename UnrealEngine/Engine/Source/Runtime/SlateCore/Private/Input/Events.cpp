// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/Events.h"
#include "Layout/ArrangedWidget.h"
#include "Widgets/SWidget.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(Events)


/* Static initialization
 *****************************************************************************/

const FTouchKeySet FTouchKeySet::StandardSet(EKeys::LeftMouseButton);
const FTouchKeySet FTouchKeySet::EmptySet(EKeys::Invalid);

FGeometry FInputEvent::FindGeometry(const TSharedRef<SWidget>& WidgetToFind) const
{
	return EventPath->FindArrangedWidget(WidgetToFind).Get(FArrangedWidget::GetNullWidget()).Geometry;
}

TSharedRef<SWindow> FInputEvent::GetWindow() const
{
	return EventPath->GetWindow();
}

FText FInputEvent::ToText() const
{
	return NSLOCTEXT("Events", "Unimplemented", "Unimplemented");
}

bool FInputEvent::IsPointerEvent() const
{
	return false;
}

bool FInputEvent::IsKeyEvent() const
{
	return false;
}

FText FCharacterEvent::ToText() const
{
	return FText::Format( NSLOCTEXT("Events", "Char", "Char({0})"), FText::FromString(FString(1, &Character)) );
}

FText FKeyEvent::ToText() const
{
	return FText::Format( NSLOCTEXT("Events", "Key", "Key({0})"), Key.GetDisplayName() );
}

bool FKeyEvent::IsKeyEvent() const
{
	return true;
}

FText FAnalogInputEvent::ToText() const
{
	return FText::Format(NSLOCTEXT("Events", "AnalogInput", "AnalogInput(key:{0}, value:{1}"), GetKey().GetDisplayName(), AnalogValue);
}

FText FPointerEvent::ToText() const
{
	return FText::Format( NSLOCTEXT("Events", "Pointer", "Pointer(key:{0}, pos:{1}x{2}, delta:{3}x{4})"), EffectingButton.GetDisplayName(), ScreenSpacePosition.X, ScreenSpacePosition.Y, CursorDelta.X, CursorDelta.Y);
}

bool FPointerEvent::IsPointerEvent() const
{
	return true;
}

