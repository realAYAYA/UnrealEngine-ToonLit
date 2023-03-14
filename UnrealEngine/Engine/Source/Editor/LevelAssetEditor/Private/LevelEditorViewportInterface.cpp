// Copyright Epic Games, Inc.All Rights Reserved.

#include "LevelEditorViewportInterface.h"

#include "Math/IntPoint.h"

class FSlateRect;
class FSlateShaderResource;
class FWidgetStyle;
class SWidget;
struct FGeometry;

FLevelEditorViewportInterfaceWrapper::FLevelEditorViewportInterfaceWrapper(TSharedPtr<ISlateViewport> InSceneViewport, UInputRouter* InInputRouter)
	: LegacyViewportInterface(InSceneViewport)
	, InputRouter(InInputRouter)
{
}

void FLevelEditorViewportInterfaceWrapper::OnDrawViewport(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, class FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled)
{
	LegacyViewportInterface->OnDrawViewport(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

FIntPoint FLevelEditorViewportInterfaceWrapper::GetSize() const
{
	return LegacyViewportInterface->GetSize();
}

FSlateShaderResource* FLevelEditorViewportInterfaceWrapper::GetViewportRenderTargetTexture() const
{
	return LegacyViewportInterface->GetViewportRenderTargetTexture();
}

bool FLevelEditorViewportInterfaceWrapper::IsViewportTextureAlphaOnly() const
{
	return LegacyViewportInterface->IsViewportTextureAlphaOnly();
}

void FLevelEditorViewportInterfaceWrapper::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float DeltaTime)
{
	LegacyViewportInterface->Tick(AllottedGeometry, InCurrentTime, DeltaTime);
}

bool FLevelEditorViewportInterfaceWrapper::RequiresVsync() const
{
	return LegacyViewportInterface->RequiresVsync();
}

bool FLevelEditorViewportInterfaceWrapper::AllowScaling() const
{
	return LegacyViewportInterface->AllowScaling();
}

FCursorReply FLevelEditorViewportInterfaceWrapper::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent)
{
	return LegacyViewportInterface->OnCursorQuery(MyGeometry, CursorEvent);
}

TOptional<TSharedRef<SWidget>> FLevelEditorViewportInterfaceWrapper::OnMapCursor(const FCursorReply& CursorReply)
{
	return LegacyViewportInterface->OnMapCursor(CursorReply);
}

bool FLevelEditorViewportInterfaceWrapper::IsSoftwareCursorVisible() const
{
	return LegacyViewportInterface->IsSoftwareCursorVisible();
}

FVector2D FLevelEditorViewportInterfaceWrapper::GetSoftwareCursorPosition() const
{
	return LegacyViewportInterface->GetSoftwareCursorPosition();
}

FReply FLevelEditorViewportInterfaceWrapper::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return LegacyViewportInterface->OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return LegacyViewportInterface->OnMouseButtonUp(MyGeometry, MouseEvent);
}

void FLevelEditorViewportInterfaceWrapper::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	LegacyViewportInterface->OnMouseEnter(MyGeometry, MouseEvent);
}

void FLevelEditorViewportInterfaceWrapper::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	LegacyViewportInterface->OnMouseLeave(MouseEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return LegacyViewportInterface->OnMouseMove(MyGeometry, MouseEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return LegacyViewportInterface->OnMouseWheel(MyGeometry, MouseEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return LegacyViewportInterface->OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return LegacyViewportInterface->OnKeyDown(MyGeometry, InKeyEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return LegacyViewportInterface->OnKeyUp(MyGeometry, InKeyEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
	return LegacyViewportInterface->OnAnalogValueChanged(MyGeometry, InAnalogInputEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	return LegacyViewportInterface->OnKeyChar(MyGeometry, InCharacterEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnFocusReceived(const FFocusEvent& InFocusEvent)
{
	return LegacyViewportInterface->OnFocusReceived(InFocusEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return LegacyViewportInterface->OnTouchStarted(MyGeometry, InTouchEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return LegacyViewportInterface->OnTouchMoved(MyGeometry, InTouchEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return LegacyViewportInterface->OnTouchEnded(MyGeometry, InTouchEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnTouchForceChanged(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return LegacyViewportInterface->OnTouchForceChanged(MyGeometry, InTouchEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnTouchFirstMove(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return LegacyViewportInterface->OnTouchFirstMove(MyGeometry, InTouchEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& InGestureEvent)
{
	return LegacyViewportInterface->OnTouchGesture(MyGeometry, InGestureEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnMotionDetected(const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent)
{
	return LegacyViewportInterface->OnMotionDetected(MyGeometry, InMotionEvent);
}

TOptional<bool> FLevelEditorViewportInterfaceWrapper::OnQueryShowFocus(const EFocusCause InFocusCause) const
{
	return LegacyViewportInterface->OnQueryShowFocus(InFocusCause);
}

void FLevelEditorViewportInterfaceWrapper::OnFinishedPointerInput()
{
	LegacyViewportInterface->OnFinishedPointerInput();
}

FPopupMethodReply FLevelEditorViewportInterfaceWrapper::OnQueryPopupMethod() const
{
	return LegacyViewportInterface->OnQueryPopupMethod();
}

FNavigationReply FLevelEditorViewportInterfaceWrapper::OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent)
{
	return LegacyViewportInterface->OnNavigation(MyGeometry, InNavigationEvent);
}

bool FLevelEditorViewportInterfaceWrapper::HandleNavigation(const uint32 InUserIndex, TSharedPtr<SWidget> InDestination)
{
	return LegacyViewportInterface->HandleNavigation(InUserIndex, InDestination);
}

void FLevelEditorViewportInterfaceWrapper::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	LegacyViewportInterface->OnFocusLost(InFocusEvent);
}

FReply FLevelEditorViewportInterfaceWrapper::OnRequestWindowClose()
{
	return LegacyViewportInterface->OnRequestWindowClose();
}

void FLevelEditorViewportInterfaceWrapper::OnViewportClosed()
{
	LegacyViewportInterface->OnViewportClosed();
}

TWeakPtr<SWidget> FLevelEditorViewportInterfaceWrapper::GetWidget()
{
	return LegacyViewportInterface->GetWidget();
}

FReply FLevelEditorViewportInterfaceWrapper::OnViewportActivated(const FWindowActivateEvent& InActivateEvent)
{
	return LegacyViewportInterface->OnViewportActivated(InActivateEvent);
}

void FLevelEditorViewportInterfaceWrapper::OnViewportDeactivated(const FWindowActivateEvent& InActivateEvent)
{
	return LegacyViewportInterface->OnViewportDeactivated(InActivateEvent);
}