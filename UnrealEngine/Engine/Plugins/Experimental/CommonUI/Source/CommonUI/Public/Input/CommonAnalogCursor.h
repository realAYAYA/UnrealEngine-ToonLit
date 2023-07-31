// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Application/AnalogCursor.h"
#include "Layout/Geometry.h"
#include "InputCoreTypes.h"

class UCommonUIActionRouterBase;
class UCommonInputSubsystem;
class SWidget;
class UWidget;
class UGameViewportClient;

struct FInputEvent;
enum class ECommonInputType : uint8;
enum EOrientation;

/**
 * Analog cursor preprocessor that tastefully hijacks things a bit to support controller navigation by moving a hidden cursor around based on focus.
 *
 * Introduces a separate focus-driven mode of operation, wherein the cursor is made invisible and automatically updated
 * to be centered over whatever widget is currently focused (except the game viewport - we completely hide it then)
 */
class COMMONUI_API FCommonAnalogCursor : public FAnalogCursor
{
public:
	template <typename AnalogCursorT = FCommonAnalogCursor>
	static TSharedRef<AnalogCursorT> CreateAnalogCursor(const UCommonUIActionRouterBase& InActionRouter)
	{
		TSharedRef<AnalogCursorT> NewCursor = MakeShareable(new AnalogCursorT(InActionRouter));
		NewCursor->Initialize();
		return NewCursor;
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override;
	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;
	virtual bool HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override;

	virtual bool CanReleaseMouseCapture() const;

	virtual bool HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent) override;
	virtual bool HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override;
	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& InPointerEvent) override;
	virtual bool HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& InPointerEvent) override;

	void SetCursorMovementStick(EAnalogStick InCursorMovementStick);

	virtual int32 GetOwnerUserIndex() const override;

	virtual void ShouldHandleRightAnalog(bool bInShouldHandleRightAnalog);

	virtual bool IsAnalogMovementEnabled() const { return bIsAnalogMovementEnabled; }

protected:
	FCommonAnalogCursor(const UCommonUIActionRouterBase& InActionRouter);
	virtual void Initialize();
	
	virtual EOrientation DetermineScrollOrientation(const UWidget& Widget) const;

	virtual bool IsRelevantInput(const FKeyEvent& KeyEvent) const override;
	virtual bool IsRelevantInput(const FAnalogInputEvent& AnalogInputEvent) const override;
	
	void SetNormalizedCursorPosition(const FVector2D& RelativeNewPosition);
	bool IsInViewport(const FVector2D& Position) const;
	FVector2D ClampPositionToViewport(const FVector2D& InPosition) const;
	void HideCursor();

	UGameViewportClient* GetViewportClient() const;
	
	/**
	 * A ridiculous function name, but we have this exact question in a few places.
	 * We don't care about input while our owning player's game viewport isn't involved in the focus path,
	 * but we also want to hold off doing anything while that game viewport has full capture.
	 * So we need that "relevant, but not exclusive" sweet spot.
	 */
	bool IsGameViewportInFocusPathWithoutCapture() const;

	virtual void RefreshCursorSettings();
	virtual void RefreshCursorVisibility();
	
	virtual void HandleInputMethodChanged(ECommonInputType NewInputMethod);
	
	bool IsUsingGamepad() const;


	bool ShouldHideCursor() const;



	// Knowingly unorthodox member reference to a UObject - ok because we are a subobject of the owning router and will never outlive it
	const UCommonUIActionRouterBase& ActionRouter;
	ECommonInputType ActiveInputMethod;

	bool bIsAnalogMovementEnabled = false;
	
	bool bShouldHandleRightAnalog = true;
	
private:
	//EAnalogStick GetScrollStick() const;
	
	/** The current set of pointer buttons being used as keys. */
	TSet<FKey> PointerButtonDownKeys;

	TWeakPtr<SWidget> LastCursorTarget;
	FSlateRenderTransform LastCursorTargetTransform;

	float TimeUntilScrollUpdate = 0.f;

	//@todo DanH: This actually needs to go up a layer into FAnalogCursor so it behaves as desired if we allow things to fall through
	//		We'll also need a CommonNavigationConfig that respects the stick assignment as well so it also maps to nav mode
	//EAnalogStick CursorMovementStick = EAnalogStick::Left;
	
#if !UE_BUILD_SHIPPING
	enum EShoulderButtonFlags
	{
		None = 0,
		LeftShoulder = 1 << 0,
		RightShoulder = 1 << 1,
		LeftTrigger = 1 << 2,
		RightTrigger = 1 << 3,
		All = 0x1111
	};
	int32 ShoulderButtonStatus = EShoulderButtonFlags::None;
#endif
};
