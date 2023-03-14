// Copyright Epic Games, Inc.All Rights Reserved.

#include "SlateViewportInterfaceWrapper.h"

FSlateViewportInterfaceWrapper::FSlateViewportInterfaceWrapper(TSharedPtr<ISlateViewport> InSceneViewport, UInputRouter* InInputRouter)
	: LegacyViewportInterface(InSceneViewport)
	, InputRouter(InInputRouter)
{
}

void FSlateViewportInterfaceWrapper::OnDrawViewport(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, class FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled)
{
	LegacyViewportInterface->OnDrawViewport(AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
}

FIntPoint FSlateViewportInterfaceWrapper::GetSize() const
{
	return LegacyViewportInterface->GetSize();
}

FSlateShaderResource* FSlateViewportInterfaceWrapper::GetViewportRenderTargetTexture() const
{
	return LegacyViewportInterface->GetViewportRenderTargetTexture();
}

bool FSlateViewportInterfaceWrapper::IsViewportTextureAlphaOnly() const
{
	return LegacyViewportInterface->IsViewportTextureAlphaOnly();
}

void FSlateViewportInterfaceWrapper::Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float DeltaTime)
{
	LegacyViewportInterface->Tick(AllottedGeometry, InCurrentTime, DeltaTime);
}

bool FSlateViewportInterfaceWrapper::RequiresVsync() const
{
	return LegacyViewportInterface->RequiresVsync();
}

bool FSlateViewportInterfaceWrapper::AllowScaling() const
{
	return LegacyViewportInterface->AllowScaling();
}

FCursorReply FSlateViewportInterfaceWrapper::OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent)
{
	return LegacyViewportInterface->OnCursorQuery(MyGeometry, CursorEvent);
}

TOptional<TSharedRef<SWidget>> FSlateViewportInterfaceWrapper::OnMapCursor(const FCursorReply& CursorReply)
{
	return LegacyViewportInterface->OnMapCursor(CursorReply);
}

bool FSlateViewportInterfaceWrapper::IsSoftwareCursorVisible() const
{
	return LegacyViewportInterface->IsSoftwareCursorVisible();
}

FVector2D FSlateViewportInterfaceWrapper::GetSoftwareCursorPosition() const
{
	return LegacyViewportInterface->GetSoftwareCursorPosition();
}

FReply FSlateViewportInterfaceWrapper::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return LegacyViewportInterface->OnMouseButtonDown(MyGeometry, MouseEvent);
}

FReply FSlateViewportInterfaceWrapper::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return LegacyViewportInterface->OnMouseButtonUp(MyGeometry, MouseEvent);
}

void FSlateViewportInterfaceWrapper::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	LegacyViewportInterface->OnMouseEnter(MyGeometry, MouseEvent);
}

void FSlateViewportInterfaceWrapper::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	LegacyViewportInterface->OnMouseLeave(MouseEvent);
}

FReply FSlateViewportInterfaceWrapper::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return LegacyViewportInterface->OnMouseMove(MyGeometry, MouseEvent);
}

FReply FSlateViewportInterfaceWrapper::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return LegacyViewportInterface->OnMouseWheel(MyGeometry, MouseEvent);
}

FReply FSlateViewportInterfaceWrapper::OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent)
{
	return LegacyViewportInterface->OnMouseButtonDoubleClick(InMyGeometry, InMouseEvent);
}

FReply FSlateViewportInterfaceWrapper::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return LegacyViewportInterface->OnKeyDown(MyGeometry, InKeyEvent);
}

FReply FSlateViewportInterfaceWrapper::OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	return LegacyViewportInterface->OnKeyUp(MyGeometry, InKeyEvent);
}

FReply FSlateViewportInterfaceWrapper::OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent)
{
	return LegacyViewportInterface->OnAnalogValueChanged(MyGeometry, InAnalogInputEvent);
}

FReply FSlateViewportInterfaceWrapper::OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent)
{
	return LegacyViewportInterface->OnKeyChar(MyGeometry, InCharacterEvent);
}

FReply FSlateViewportInterfaceWrapper::OnFocusReceived(const FFocusEvent& InFocusEvent)
{
	return LegacyViewportInterface->OnFocusReceived(InFocusEvent);
}

FReply FSlateViewportInterfaceWrapper::OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return LegacyViewportInterface->OnTouchStarted(MyGeometry, InTouchEvent);
}

FReply FSlateViewportInterfaceWrapper::OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return LegacyViewportInterface->OnTouchMoved(MyGeometry, InTouchEvent);
}

FReply FSlateViewportInterfaceWrapper::OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return LegacyViewportInterface->OnTouchEnded(MyGeometry, InTouchEvent);
}

FReply FSlateViewportInterfaceWrapper::OnTouchForceChanged(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return LegacyViewportInterface->OnTouchForceChanged(MyGeometry, InTouchEvent);
}

FReply FSlateViewportInterfaceWrapper::OnTouchFirstMove(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent)
{
	return LegacyViewportInterface->OnTouchFirstMove(MyGeometry, InTouchEvent);
}

FReply FSlateViewportInterfaceWrapper::OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& InGestureEvent)
{
	return LegacyViewportInterface->OnTouchGesture(MyGeometry, InGestureEvent);
}

FReply FSlateViewportInterfaceWrapper::OnMotionDetected(const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent)
{
	return LegacyViewportInterface->OnMotionDetected(MyGeometry, InMotionEvent);
}

TOptional<bool> FSlateViewportInterfaceWrapper::OnQueryShowFocus(const EFocusCause InFocusCause) const
{
	return LegacyViewportInterface->OnQueryShowFocus(InFocusCause);
}

void FSlateViewportInterfaceWrapper::OnFinishedPointerInput()
{
	LegacyViewportInterface->OnFinishedPointerInput();
}

FPopupMethodReply FSlateViewportInterfaceWrapper::OnQueryPopupMethod() const
{
	return LegacyViewportInterface->OnQueryPopupMethod();
}

FNavigationReply FSlateViewportInterfaceWrapper::OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent)
{
	return LegacyViewportInterface->OnNavigation(MyGeometry, InNavigationEvent);
}

bool FSlateViewportInterfaceWrapper::HandleNavigation(const uint32 InUserIndex, TSharedPtr<SWidget> InDestination)
{
	return LegacyViewportInterface->HandleNavigation(InUserIndex, InDestination);
}

void FSlateViewportInterfaceWrapper::OnFocusLost(const FFocusEvent& InFocusEvent)
{
	LegacyViewportInterface->OnFocusLost(InFocusEvent);
}

FReply FSlateViewportInterfaceWrapper::OnRequestWindowClose()
{
	return LegacyViewportInterface->OnRequestWindowClose();
}

void FSlateViewportInterfaceWrapper::OnViewportClosed()
{
	LegacyViewportInterface->OnViewportClosed();
}

TWeakPtr<SWidget> FSlateViewportInterfaceWrapper::GetWidget()
{
	return LegacyViewportInterface->GetWidget();
}

FReply FSlateViewportInterfaceWrapper::OnViewportActivated(const FWindowActivateEvent& InActivateEvent)
{
	return LegacyViewportInterface->OnViewportActivated(InActivateEvent);
}

void FSlateViewportInterfaceWrapper::OnViewportDeactivated(const FWindowActivateEvent& InActivateEvent)
{
	return LegacyViewportInterface->OnViewportDeactivated(InActivateEvent);
}