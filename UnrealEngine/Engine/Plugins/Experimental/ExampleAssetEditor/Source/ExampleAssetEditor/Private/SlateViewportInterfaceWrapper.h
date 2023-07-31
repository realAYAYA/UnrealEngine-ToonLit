// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "Rendering/RenderingCommon.h"

class UInputRouter;

class FSlateViewportInterfaceWrapper
	: public ISlateViewport
{
public:
	FSlateViewportInterfaceWrapper(TSharedPtr<ISlateViewport> InSceneViewport, UInputRouter* InInputRouter);

	// Begin ISlateViewport interface
	virtual void OnDrawViewport(const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, class FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) override;
	virtual FIntPoint GetSize() const override;
	virtual class FSlateShaderResource* GetViewportRenderTargetTexture() const override;
	virtual bool IsViewportTextureAlphaOnly() const override;
	virtual void Tick(const FGeometry& AllottedGeometry, double InCurrentTime, float DeltaTime) override;
	virtual bool RequiresVsync() const override;
	virtual bool AllowScaling() const override;
	virtual FCursorReply OnCursorQuery(const FGeometry& MyGeometry, const FPointerEvent& CursorEvent) override;
	virtual TOptional<TSharedRef<SWidget>> OnMapCursor(const FCursorReply& CursorReply) override;
	virtual bool IsSoftwareCursorVisible() const override;
	virtual FVector2D GetSoftwareCursorPosition() const override;
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnAnalogValueChanged(const FGeometry& MyGeometry, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual FReply OnKeyChar(const FGeometry& MyGeometry, const FCharacterEvent& InCharacterEvent) override;
	virtual FReply OnFocusReceived(const FFocusEvent& InFocusEvent) override;
	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchMoved(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchForceChanged(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchFirstMove(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply OnTouchGesture(const FGeometry& MyGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply OnMotionDetected(const FGeometry& MyGeometry, const FMotionEvent& InMotionEvent) override;
	virtual TOptional<bool> OnQueryShowFocus(const EFocusCause InFocusCause) const override;
	virtual void OnFinishedPointerInput() override;
	virtual FPopupMethodReply OnQueryPopupMethod() const override;
	virtual FNavigationReply OnNavigation(const FGeometry& MyGeometry, const FNavigationEvent& InNavigationEvent) override;
	virtual bool HandleNavigation(const uint32 InUserIndex, TSharedPtr<SWidget> InDestination) override;
	virtual void OnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual FReply OnRequestWindowClose() override;
	virtual void OnViewportClosed() override;
	virtual TWeakPtr<SWidget> GetWidget() override;
	virtual FReply OnViewportActivated(const FWindowActivateEvent& InActivateEvent) override;
	virtual void OnViewportDeactivated(const FWindowActivateEvent& InActivateEvent) override;
	// End ISlateViewport interface

protected:
	TSharedPtr<ISlateViewport> LegacyViewportInterface;
	UInputRouter* InputRouter;
};