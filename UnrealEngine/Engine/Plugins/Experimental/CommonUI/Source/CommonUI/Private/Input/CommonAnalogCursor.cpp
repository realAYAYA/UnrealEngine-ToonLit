// Copyright Epic Games, Inc. All Rights Reserved.

#include "Input/CommonAnalogCursor.h"
#include "CommonUIPrivate.h"
#include "Slate/SObjectWidget.h"
#include "Blueprint/UserWidget.h"
#include "Input/CommonUIActionRouterBase.h"
#include "CommonInputSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateUser.h"
#include "Framework/Views/TableViewMetadata.h"
#include "Input/CommonUIInputSettings.h"
#include "Engine/Console.h"
#include "CommonInputBaseTypes.h"
#include "Widgets/SViewport.h"
#include "Engine/GameViewportClient.h"

#include "Components/ListView.h"
#include "Components/ScrollBar.h"
#include "Components/ScrollBox.h"
#include "Engine/Engine.h"
#include "Slate/SGameLayerManager.h"
#include "Types/ReflectionMetadata.h"

#define LOCTEXT_NAMESPACE "CommonAnalogCursor"


//@todo DanH: CVar for forcing analog movement to be enabled

//@todo DanH: Move to UCommonUIInputSettings
const float AnalogScrollUpdatePeriod = 0.1f;
const float ScrollDeadZone = 0.2f;

bool IsEligibleFakeKeyPointerEvent(const FPointerEvent& PointerEvent)
{
	FKey EffectingButton = PointerEvent.GetEffectingButton();
	return EffectingButton.IsMouseButton() 
		&& EffectingButton != EKeys::LeftMouseButton
		&& EffectingButton != EKeys::RightMouseButton
		&& EffectingButton != EKeys::MiddleMouseButton;
}

FCommonAnalogCursor::FCommonAnalogCursor(const UCommonUIActionRouterBase& InActionRouter)
	: ActionRouter(InActionRouter)
	, ActiveInputMethod(ECommonInputType::MouseAndKeyboard)
{}

void FCommonAnalogCursor::Initialize()
{
	RefreshCursorSettings();

	PointerButtonDownKeys = FSlateApplication::Get().GetPressedMouseButtons();
	PointerButtonDownKeys.Remove(EKeys::LeftMouseButton);
	PointerButtonDownKeys.Remove(EKeys::RightMouseButton);
	PointerButtonDownKeys.Remove(EKeys::MiddleMouseButton);

	UCommonInputSubsystem& InputSubsystem = ActionRouter.GetInputSubsystem();
	InputSubsystem.OnInputMethodChangedNative.AddSP(this, &FCommonAnalogCursor::HandleInputMethodChanged);
	HandleInputMethodChanged(InputSubsystem.GetCurrentInputType());
}

#if WITH_EDITOR
extern bool IsViewportWindowInFocusPath(const UCommonUIActionRouterBase& Router);
#endif

void FCommonAnalogCursor::Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor)
{
	//@todo DanH: Cursor visibility was getting thrown off somehow on PS4 and P2 wound up permanently showing the cursor
	//		Will circle back on this, but for now refreshing a single bool each frame is relatively harmless.
	RefreshCursorVisibility();

	// Don't bother trying to do anything while the game viewport has capture
	if (IsUsingGamepad() && IsGameViewportInFocusPathWithoutCapture())
	{
		// The game viewport can't have been focused without a user, so we're quite safe to assume/enforce validity of the user here
		const TSharedRef<FSlateUser> SlateUser = SlateApp.GetUser(GetOwnerUserIndex()).ToSharedRef();

#if WITH_EDITOR
		// Instantly acknowledge any changes to our settings when we're in the editor
		RefreshCursorSettings();
		if (!IsViewportWindowInFocusPath(ActionRouter))
		{
			return;
		}
#endif
		if (bIsAnalogMovementEnabled)
		{
			FAnalogCursor::Tick(DeltaTime, SlateApp, Cursor);
		}
		else
		{
			TSharedPtr<SWidget> PinnedLastCursorTarget = LastCursorTarget.Pin();

			// By default the cursor target is the focused widget itself, unless we're working with a list view
			TSharedPtr<SWidget> CursorTarget = SlateUser->GetFocusedWidget();
			if (TSharedPtr<ITableViewMetadata> TableViewMetadata = CursorTarget ? CursorTarget->GetMetaData<ITableViewMetadata>() : nullptr)
			{
				//@todo DanH: When a list is focused but the selected row isn't visible, should we try to hide the cursor or anything?
				// A list view is currently focused, so we actually want to make sure we are centering the cursor over the currently selected row instead
				TArray<TSharedPtr<ITableRow>> SelectedRows = TableViewMetadata->GatherSelectedRows();
				if (SelectedRows.Num() > 0 && ensure(SelectedRows[0].IsValid()))
				{
					// Just pick the first selected entry in the list - it's awfully rare to have anything other than single-selection when using gamepad
					CursorTarget = SelectedRows[0]->AsWidget();
				}
			}

			// We want to update the cursor position when focus changes or the focused widget moves at all
			if (CursorTarget != PinnedLastCursorTarget || (CursorTarget && CursorTarget->GetCachedGeometry().GetAccumulatedRenderTransform() != LastCursorTargetTransform))
			{
#if !UE_BUILD_SHIPPING
				if (CursorTarget != PinnedLastCursorTarget)
				{
					UE_LOG(LogCommonUI, Verbose, TEXT("User[%d] cursor target changed to [%s]"), GetOwnerUserIndex(), *FReflectionMetaData::GetWidgetDebugInfo(CursorTarget.Get()));
				}
#endif

				// Release capture unless the focused widget is the captor
				if (PinnedLastCursorTarget != CursorTarget && SlateUser->HasCursorCapture() && !SlateUser->DoesWidgetHaveAnyCapture(CursorTarget))
				{
					UE_LOG(LogCommonUI, Log, TEXT("User[%d] focus changed while the cursor is captured - releasing now before moving cursor to focused widget."), GetOwnerUserIndex());
					SlateUser->ReleaseCursorCapture();
				}

				LastCursorTarget = CursorTarget;

				bool bHasValidCursorTarget = false;
				if (CursorTarget)
				{
					FGeometry TargetGeometry; 
					if (CursorTarget == GetViewportClient()->GetGameViewportWidget())
					{
						//@todo DanH: We reeeeally need the GameViewport stuff to be friendlier toward splitscreen scenarios and allow easier direct access to a player's actual "viewport" widget

						// When the target is the game viewport as a whole, we don't want to center blindly - we want to center in the geometry of our owner's widget host layer
						TSharedPtr<IGameLayerManager> GameLayerManager = GetViewportClient()->GetGameLayerManager();
						if (ensure(GameLayerManager))
						{
							TargetGeometry = GameLayerManager->GetPlayerWidgetHostGeometry(ActionRouter.GetLocalPlayerChecked());
						}
					}
					else
					{
						TargetGeometry = CursorTarget->GetCachedGeometry();
					}
					
					LastCursorTargetTransform = TargetGeometry.GetAccumulatedRenderTransform();
					if (TargetGeometry.GetLocalSize().SizeSquared() > SMALL_NUMBER)
					{
						bHasValidCursorTarget = true;
						
						const FVector2D AbsoluteWidgetCenter = TargetGeometry.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
						SlateUser->SetCursorPosition(AbsoluteWidgetCenter);

						UE_LOG(LogCommonUI, Verbose, TEXT("User[%d] moving cursor to target [%s] @ (%d, %d)"), GetOwnerUserIndex(), *FReflectionMetaData::GetWidgetDebugInfo(CursorTarget.Get()), (int32)AbsoluteWidgetCenter.X, (int32)AbsoluteWidgetCenter.Y);
					}
				}

				if (!bHasValidCursorTarget)
				{
					SetNormalizedCursorPosition(FVector2D::ZeroVector);
				}
			}
		}

		if (bShouldHandleRightAnalog)
		{
			TimeUntilScrollUpdate -= DeltaTime;
			if (TimeUntilScrollUpdate <= 0.0f && GetAnalogValues(EAnalogStick::Right).SizeSquared() > FMath::Square(ScrollDeadZone))
			{
				// Generate mouse wheel events over all widgets currently registered as scroll recipients
				const TArray<const UWidget*>& AnalogScrollRecipients = ActionRouter.GatherActiveAnalogScrollRecipients();
				if (AnalogScrollRecipients.Num() > 0)
				{
					const FCommonAnalogCursorSettings& CursorSettings = UCommonUIInputSettings::Get().GetAnalogCursorSettings();
					const auto GetScrollAmountFunc = [&CursorSettings](float AnalogValue)
					{
						const float AmountBeyondDeadZone = FMath::Abs(AnalogValue) - CursorSettings.ScrollDeadZone;
						if (AmountBeyondDeadZone <= 0.f)
						{
							return 0.f;
						}
						return (AmountBeyondDeadZone / (1.f - CursorSettings.ScrollDeadZone)) * -FMath::Sign(AnalogValue) * CursorSettings.ScrollMultiplier;
					};

					const FVector2D& RightStickValues = GetAnalogValues(EAnalogStick::Right);
					const FVector2D ScrollAmounts(GetScrollAmountFunc(RightStickValues.X), GetScrollAmountFunc(RightStickValues.Y));

					for (const UWidget* ScrollRecipient : AnalogScrollRecipients)
					{
						check(ScrollRecipient);
						if (ScrollRecipient->GetCachedWidget())
						{
							const EOrientation Orientation = DetermineScrollOrientation(*ScrollRecipient);
							const float ScrollAmount = Orientation == Orient_Vertical ? ScrollAmounts.Y : ScrollAmounts.X;
							if (FMath::Abs(ScrollAmount) > SMALL_NUMBER)
							{
								const FVector2D WidgetCenter = ScrollRecipient->GetCachedGeometry().GetAbsolutePositionAtCoordinates(FVector2D(.5f, .5f));
								if (IsInViewport(WidgetCenter))
								{
									FPointerEvent MouseEvent(
										SlateUser->GetUserIndex(),
										FSlateApplication::CursorPointerIndex,
										WidgetCenter,
										WidgetCenter,
										TSet<FKey>(),
										EKeys::MouseWheelAxis,
										ScrollAmount,
										FModifierKeysState());

									SlateApp.ProcessMouseWheelOrGestureEvent(MouseEvent, nullptr);
								}
							}
						}
					}
				}
			}
		}
	}
}

bool FCommonAnalogCursor::HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (IsRelevantInput(InKeyEvent))
	{
		const ULocalPlayer& LocalPlayer = *ActionRouter.GetLocalPlayerChecked();
		if (LocalPlayer.ViewportClient && LocalPlayer.ViewportClient->ViewportConsole && LocalPlayer.ViewportClient->ViewportConsole->ConsoleActive())
		{
			// Let everything through when the console is open
			return false;
		}

#if !UE_BUILD_SHIPPING
		const FKey& PressedKey = InKeyEvent.GetKey();
		if (PressedKey == EKeys::Gamepad_LeftShoulder) { ShoulderButtonStatus |= EShoulderButtonFlags::LeftShoulder; }
		if (PressedKey == EKeys::Gamepad_RightShoulder) { ShoulderButtonStatus |= EShoulderButtonFlags::RightShoulder; }
		if (PressedKey == EKeys::Gamepad_LeftTrigger) { ShoulderButtonStatus |= EShoulderButtonFlags::LeftTrigger; }
		if (PressedKey == EKeys::Gamepad_RightTrigger) { ShoulderButtonStatus |= EShoulderButtonFlags::RightTrigger; }

		if (ShoulderButtonStatus == EShoulderButtonFlags::All)
		{
			ShoulderButtonStatus = EShoulderButtonFlags::None;
			bIsAnalogMovementEnabled = !bIsAnalogMovementEnabled;
			//RefreshCursorVisibility();
		}
#endif

		// We support binding actions to the virtual accept key, so it's a special flower that gets processed right now
		const bool bIsVirtualAccept = InKeyEvent.GetKey() == EKeys::Virtual_Accept;
		const EInputEvent InputEventType = InKeyEvent.IsRepeat() ? IE_Repeat : IE_Pressed;
		if (bIsVirtualAccept && ActionRouter.ProcessInput(InKeyEvent.GetKey(), InputEventType) == ERouteUIInputResult::Handled)
		{
			return true;
		}
		else
		{
			//@todo DanH: This is a major bummer to have to flip this flag on the input subsystem here, but there is no awareness on a mouse event of whether it's real or not
			//		Though tbh, any place that cares should be able to just check the live input mode and infer from that whether this is a mouse click or virtual gamepad click
			UCommonInputSubsystem& InputSubsytem = ActionRouter.GetInputSubsystem();
			InputSubsytem.SetIsGamepadSimulatedClick(bIsVirtualAccept);
			bool bReturnValue = FAnalogCursor::HandleKeyDownEvent(SlateApp, InKeyEvent);
			InputSubsytem.SetIsGamepadSimulatedClick(false);

			return bReturnValue;
		}
	}
	return false;
}

bool FCommonAnalogCursor::HandleKeyUpEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent)
{
	if (IsRelevantInput(InKeyEvent))
	{
#if !UE_BUILD_SHIPPING
		//@todo DanH: I'm not sure this'll actually work, may have been a dumb idea to try tracking this way
		const FKey& PressedKey = InKeyEvent.GetKey();
		if (PressedKey == EKeys::Gamepad_LeftShoulder) { ShoulderButtonStatus ^= EShoulderButtonFlags::LeftShoulder; }
		if (PressedKey == EKeys::Gamepad_RightShoulder) { ShoulderButtonStatus ^= EShoulderButtonFlags::RightShoulder; }
		if (PressedKey == EKeys::Gamepad_LeftTrigger) { ShoulderButtonStatus ^= EShoulderButtonFlags::LeftTrigger; }
		if (PressedKey == EKeys::Gamepad_RightTrigger) { ShoulderButtonStatus ^= EShoulderButtonFlags::RightTrigger; }
#endif

		// We support binding actions to the virtual accept key, so it's a special flower that gets processed right now
		const bool bIsVirtualAccept = InKeyEvent.GetKey() == EKeys::Virtual_Accept;
		if (bIsVirtualAccept && ActionRouter.ProcessInput(InKeyEvent.GetKey(), IE_Released) == ERouteUIInputResult::Handled)
		{
			return true;
		}
		else
		{
			return FAnalogCursor::HandleKeyUpEvent(SlateApp, InKeyEvent);
		}
	}
	return false;
}

bool FCommonAnalogCursor::CanReleaseMouseCapture() const
{
	EMouseCaptureMode MouseCapture = ActionRouter.GetActiveMouseCaptureMode();
	return MouseCapture == EMouseCaptureMode::CaptureDuringMouseDown || MouseCapture == EMouseCaptureMode::CapturePermanently_IncludingInitialMouseDown;
}

bool FCommonAnalogCursor::HandleAnalogInputEvent(FSlateApplication& SlateApp, const FAnalogInputEvent& InAnalogInputEvent)
{
	if (IsRelevantInput(InAnalogInputEvent))
	{
		bool bParentHandled = FAnalogCursor::HandleAnalogInputEvent(SlateApp, InAnalogInputEvent);
		if (bIsAnalogMovementEnabled)
		{
			return bParentHandled;
		}
	}
	
	return false;
}

bool FCommonAnalogCursor::HandleMouseMoveEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent)
{
#if WITH_EDITOR
	// We can leave editor cursor visibility in a bad state if the engine stops ticking to debug
	if (GIntraFrameDebuggingGameThread)
	{
		SlateApp.SetPlatformCursorVisibility(true);
		TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(GetOwnerUserIndex());
		if (ensure(SlateUser))
		{
			SlateUser->SetCursorVisibility(true);
		}
	}
#endif // WITH_EDITOR

	return FAnalogCursor::HandleMouseMoveEvent(SlateApp, MouseEvent);
}

bool FCommonAnalogCursor::HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& PointerEvent)
{
	if (FAnalogCursor::IsRelevantInput(PointerEvent))
	{
#if UE_COMMONUI_PLATFORM_SUPPORTS_TOUCH	
		// Some platforms don't register as switching its input type, so detect touch input here to hide the cursor.
		if (PointerEvent.IsTouchEvent() && ShouldHideCursor())
		{
			//ClearCenterWidget();
			HideCursor();
		}
#endif 

		//@todo DanH: May want to make the list of "mouse buttons to treat like keys" a settings thing
		// Mouse buttons other than the two primaries are fair game for binding as if they were normal keys
		const FKey EffectingButton = PointerEvent.GetEffectingButton();
		if (EffectingButton.IsMouseButton()
			&& EffectingButton != EKeys::LeftMouseButton
			&& EffectingButton != EKeys::RightMouseButton
			&& EffectingButton != EKeys::MiddleMouseButton)
		{
			UGameViewportClient* ViewportClient = GetViewportClient();
			if (TSharedPtr<SWidget> ViewportWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr)
			{
				//@todo DanH: Mouse buttons generally transfer focus to the application they're over, so shouldn't we be able to rely instead
				//		on a combination check that a) the SlateApp is active and b) the game viewport is in the focus path?
				//		Mousing over to a web browser and clicking the back button, for example, will shift OS focus to the browser
				//			Within the Slate app, clicking back will transfer focus just like LMB would
				const FWidgetPath WidgetsUnderCursor = SlateApp.LocateWindowUnderMouse(PointerEvent.GetScreenSpacePosition(), SlateApp.GetInteractiveTopLevelWindows());
				if (WidgetsUnderCursor.ContainsWidget(ViewportWidget.Get()))
				{
					//@todo DanH: Do we need to go through the whole process here of generating a false key down event?
					//		All we really want to do here is allow UI input bindings to mouse buttons right?
					//		95% sure we can/should simply be having the ActionRouter process input here instead of generating the event
					//			After all, a widget expecting to receive an OnKeyDown with a mouse button key is just plaing doing it wrong...
					FKeyEvent MouseKeyEvent(EffectingButton, PointerEvent.GetModifierKeys(), PointerEvent.GetUserIndex(), false, 0, 0);
					if (SlateApp.ProcessKeyDownEvent(MouseKeyEvent))
					{
						PointerButtonDownKeys.Add(EffectingButton);
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool FCommonAnalogCursor::HandleMouseButtonUpEvent(FSlateApplication& SlateApp, const FPointerEvent& PointerEvent)
{
	if (FAnalogCursor::IsRelevantInput(PointerEvent))
	{
		const bool bHadKeyDown = PointerButtonDownKeys.Remove(PointerEvent.GetEffectingButton()) > 0;
		if (bHadKeyDown
			|| (IsEligibleFakeKeyPointerEvent(PointerEvent) && !SlateApp.HasUserMouseCapture(PointerEvent.GetUserIndex())))
		{
			// Reprocess as a key if there was no mouse capture or it was previously pressed
			FKeyEvent MouseKeyEvent(PointerEvent.GetEffectingButton(), PointerEvent.GetModifierKeys(), PointerEvent.GetUserIndex(), false, 0, 0);
			bool bHandled = SlateApp.ProcessKeyUpEvent(MouseKeyEvent);
			if (bHadKeyDown)
			{
				//@todo DanH: What is this scenario? As it is we'll ignore the button down when its outside the app window, but still handle it when released. NONSENSE.
				//		Also if someone had it down when they activated the app, then released, there's also no reason to process that
				//		Now, if we were to have a mouse button hold action (ewwwww) and you have it down and leave the app, then release, I guess we'd still process it so long as it was down

				// Only block the mouse up if the mouse down was also blocked
				return bHandled;
			}
		}
	}

	return false;
}

int32 FCommonAnalogCursor::GetOwnerUserIndex() const
{
	return ActionRouter.GetLocalPlayerIndex();
}

void FCommonAnalogCursor::ShouldHandleRightAnalog(bool bInShouldHandleRightAnalog)
{
	bShouldHandleRightAnalog = bInShouldHandleRightAnalog;
}

//void FCommonAnalogCursor::SetCursorMovementStick(EAnalogStick InCursorMovementStick)
//{
//	const EAnalogStick NewStick = InCursorMovementStick == EAnalogStick::Max ? EAnalogStick::Left : InCursorMovementStick;
//	if (NewStick != CursorMovementStick)
//	{
//		ClearAnalogValues();
//		CursorMovementStick = NewStick;
//	}
//}

EOrientation FCommonAnalogCursor::DetermineScrollOrientation(const UWidget& Widget) const
{
	if (const UListView* AsListView = Cast<const UListView>(&Widget))
	{
		return AsListView->GetOrientation();
	}
	else if (const UScrollBar* AsScrollBar = Cast<const UScrollBar>(&Widget))
	{
		return AsScrollBar->Orientation;
	}
	else if (const UScrollBox* AsScrollBox = Cast<const UScrollBox>(&Widget))
	{
		return AsScrollBox->Orientation;
	}
	return EOrientation::Orient_Vertical;
}

bool FCommonAnalogCursor::IsRelevantInput(const FKeyEvent& KeyEvent) const
{
	return IsUsingGamepad() && FAnalogCursor::IsRelevantInput(KeyEvent) && (IsGameViewportInFocusPathWithoutCapture() || (KeyEvent.GetKey() == EKeys::Virtual_Accept && CanReleaseMouseCapture()));
}

bool FCommonAnalogCursor::IsRelevantInput(const FAnalogInputEvent& AnalogInputEvent) const
{
	return IsUsingGamepad() && FAnalogCursor::IsRelevantInput(AnalogInputEvent) && IsGameViewportInFocusPathWithoutCapture();
}

//EAnalogStick FCommonAnalogCursor::GetScrollStick() const
//{
//	// Scroll is on the right stick unless it conflicts with the cursor movement stick
//	return CursorMovementStick == EAnalogStick::Right ? EAnalogStick::Left : EAnalogStick::Right;
//}

UGameViewportClient* FCommonAnalogCursor::GetViewportClient() const
{
	return ActionRouter.GetLocalPlayerChecked()->ViewportClient;
}

bool FCommonAnalogCursor::IsGameViewportInFocusPathWithoutCapture() const
{
	if (const UGameViewportClient* ViewportClient = GetViewportClient())
	{
		if (TSharedPtr<SViewport> GameViewportWidget = ViewportClient->GetGameViewportWidget())
		{
			TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(GetOwnerUserIndex());
			if (ensure(SlateUser) && !SlateUser->DoesWidgetHaveCursorCapture(GameViewportWidget))
			{
#if PLATFORM_DESKTOP
				// Not captured - is it in the focus path?
				return SlateUser->IsWidgetInFocusPath(GameViewportWidget);
#endif
				// If we're not on desktop, focus on the viewport is irrelevant, as there aren't other windows around to care about
				return true;
			}
		}
	}
	return false;
}

void FCommonAnalogCursor::HandleInputMethodChanged(ECommonInputType NewInputMethod)
{
	ActiveInputMethod = NewInputMethod;
	if (IsUsingGamepad())
	{
		LastCursorTarget.Reset();
	}
	//RefreshCursorVisibility();
}

void FCommonAnalogCursor::RefreshCursorSettings()
{
	const FCommonAnalogCursorSettings& CursorSettings = UCommonUIInputSettings::Get().GetAnalogCursorSettings();
	Acceleration = CursorSettings.CursorAcceleration;
	MaxSpeed = CursorSettings.CursorMaxSpeed;
	DeadZone = CursorSettings.CursorDeadZone;
	StickySlowdown = CursorSettings.HoverSlowdownFactor;
	Mode = CursorSettings.bEnableCursorAcceleration ? AnalogCursorMode::Accelerated : AnalogCursorMode::Direct;
}

void FCommonAnalogCursor::RefreshCursorVisibility()
{
	FSlateApplication& SlateApp = FSlateApplication::Get();
	if (TSharedPtr<FSlateUser> SlateUser = SlateApp.GetUser(GetOwnerUserIndex()))
	{
		const bool bShowCursor = bIsAnalogMovementEnabled || ActionRouter.ShouldAlwaysShowCursor() || ActiveInputMethod == ECommonInputType::MouseAndKeyboard;

		if (!bShowCursor)
		{
			SlateApp.SetPlatformCursorVisibility(false);
		}
		SlateUser->SetCursorVisibility(bShowCursor);
	}
}

bool FCommonAnalogCursor::IsUsingGamepad() const
{
	return ActiveInputMethod == ECommonInputType::Gamepad;
}

bool FCommonAnalogCursor::ShouldHideCursor() const
{
	bool bUsingMouseForTouch = FSlateApplication::Get().IsFakingTouchEvents();
	const ULocalPlayer& LocalPlayer = *ActionRouter.GetLocalPlayerChecked();
	if (UGameViewportClient* GameViewportClient = LocalPlayer.ViewportClient)
	{
		bUsingMouseForTouch |= GameViewportClient->GetUseMouseForTouch();
	}

	return !bUsingMouseForTouch;
}

void FCommonAnalogCursor::HideCursor()
{
	const TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(GetOwnerUserIndex());
	const UWorld* World = ActionRouter.GetWorld();
	if (SlateUser && World && World->IsGameWorld())
	{
		UGameViewportClient* GameViewport = World->GetGameViewport();
		if (GameViewport && GameViewport->GetWindow().IsValid() && GameViewport->Viewport)
		{
			const FVector2D TopLeftPos = GameViewport->Viewport->ViewportToVirtualDesktopPixel(FVector2D(0.025f, 0.025f));
			SlateUser->SetCursorPosition(TopLeftPos);
			SlateUser->SetCursorVisibility(false);
		}
	}
}

void FCommonAnalogCursor::SetNormalizedCursorPosition(const FVector2D& RelativeNewPosition)
{
	TSharedPtr<FSlateUser> SlateUser = FSlateApplication::Get().GetUser(GetOwnerUserIndex());
	if (ensure(SlateUser))
	{
		const UGameViewportClient* ViewportClient = GetViewportClient();
		if (TSharedPtr<SViewport> ViewportWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr)
		{
			const FVector2D ClampedNewPosition(FMath::Clamp(RelativeNewPosition.X, 0.0f, 1.0f), FMath::Clamp(RelativeNewPosition.Y, 0.0f, 1.0f));
			const FVector2D AbsolutePosition = ViewportWidget->GetCachedGeometry().GetAbsolutePositionAtCoordinates(ClampedNewPosition);
			SlateUser->SetCursorPosition(AbsolutePosition);
		}
	}
}

bool FCommonAnalogCursor::IsInViewport(const FVector2D& Position) const
{
	if (const UGameViewportClient* ViewportClient = GetViewportClient())
	{
		TSharedPtr<SViewport> ViewportWidget = ViewportClient->GetGameViewportWidget();
		return ViewportWidget && ViewportWidget->GetCachedGeometry().GetLayoutBoundingRect().ContainsPoint(Position);
	}
	return false;
}

FVector2D FCommonAnalogCursor::ClampPositionToViewport(const FVector2D& InPosition) const
{
	const UGameViewportClient* ViewportClient = GetViewportClient();
	if (TSharedPtr<SViewport> ViewportWidget = ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr)
	{
		const FGeometry& ViewportGeometry = ViewportWidget->GetCachedGeometry();
		FVector2D LocalPosition = ViewportGeometry.AbsoluteToLocal(InPosition);
		LocalPosition.X = FMath::Clamp(LocalPosition.X, 1.0f, ViewportGeometry.GetLocalSize().X - 1.0f);
		LocalPosition.Y = FMath::Clamp(LocalPosition.Y, 1.0f, ViewportGeometry.GetLocalSize().Y - 1.0f);
		
		return ViewportGeometry.LocalToAbsolute(LocalPosition);
	}

	return InPosition;
}

#undef LOCTEXT_NAMESPACE
