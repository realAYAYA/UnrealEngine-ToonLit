// Copyright Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSInputInterface.h"
#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopeLock.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/EmbeddedCommunication.h"

#import <AudioToolbox/AudioToolbox.h>

DECLARE_LOG_CATEGORY_EXTERN(LogIOSInput, Log, All);

#define APPLE_CONTROLLER_DEBUG 0

static TAutoConsoleVariable<float> CVarHapticsKickHeavy(TEXT("ios.VibrationHapticsKickHeavyValue"), 0.65f, TEXT("Vibation values higher than this will kick a haptics heavy Impact"));
static TAutoConsoleVariable<float> CVarHapticsKickMedium(TEXT("ios.VibrationHapticsKickMediumValue"), 0.5f, TEXT("Vibation values higher than this will kick a haptics medium Impact"));
static TAutoConsoleVariable<float> CVarHapticsKickLight(TEXT("ios.VibrationHapticsKickLightValue"), 0.3f, TEXT("Vibation values higher than this will kick a haptics light Impact"));
static TAutoConsoleVariable<float> CVarHapticsRest(TEXT("ios.VibrationHapticsRestValue"), 0.2f, TEXT("Vibation values lower than this will allow haptics to Kick again when going over ios.VibrationHapticsKickValue"));

uint32 TranslateGCKeyCodeToASCII(GCKeyCode KeyCode)
{
    uint32 c = '?';

    if ( KeyCode < GCKeyCodeSlash ) // Only implemented up to '/'
    {
        if (KeyCode == GCKeyCodeKeyA) { c = 'A';}
        else if (KeyCode == GCKeyCodeKeyB) { c = 'B';}
        else if (KeyCode == GCKeyCodeKeyC) { c = 'C';}
        else if (KeyCode == GCKeyCodeKeyD) { c = 'D';}
        else if (KeyCode == GCKeyCodeKeyE) { c = 'E';}
        else if (KeyCode == GCKeyCodeKeyF) { c = 'F';}
        else if (KeyCode == GCKeyCodeKeyG) { c = 'G';}
        else if (KeyCode == GCKeyCodeKeyH) { c = 'H';}
        else if (KeyCode == GCKeyCodeKeyI) { c = 'I';}
        else if (KeyCode == GCKeyCodeKeyJ) { c = 'J';}
        else if (KeyCode == GCKeyCodeKeyK) { c = 'K';}
        else if (KeyCode == GCKeyCodeKeyL) { c = 'L';}
        else if (KeyCode == GCKeyCodeKeyM) { c = 'M';}
        else if (KeyCode == GCKeyCodeKeyN) { c = 'N';}
        else if (KeyCode == GCKeyCodeKeyO) { c = 'O';}
        else if (KeyCode == GCKeyCodeKeyP) { c = 'P';}
        else if (KeyCode == GCKeyCodeKeyQ) { c = 'Q';}
        else if (KeyCode == GCKeyCodeKeyR) { c = 'R';}
        else if (KeyCode == GCKeyCodeKeyS) { c = 'S';}
        else if (KeyCode == GCKeyCodeKeyT) { c = 'T';}
        else if (KeyCode == GCKeyCodeKeyU) { c = 'U';}
        else if (KeyCode == GCKeyCodeKeyV) { c = 'V';}
        else if (KeyCode == GCKeyCodeKeyW) { c = 'W';}
        else if (KeyCode == GCKeyCodeKeyX) { c = 'X';}
        else if (KeyCode == GCKeyCodeKeyY) { c = 'Y';}
        else if (KeyCode == GCKeyCodeKeyZ) { c = 'Z';}
        else if (KeyCode == GCKeyCodeOne) { c = '1';}
        else if (KeyCode == GCKeyCodeTwo) { c = '2';}
        else if (KeyCode == GCKeyCodeThree) { c = '3';}
        else if (KeyCode == GCKeyCodeFour) { c = '4';}
        else if (KeyCode == GCKeyCodeFive) { c = '5';}
        else if (KeyCode == GCKeyCodeSix) { c = '6';}
        else if (KeyCode == GCKeyCodeSeven) { c = '7';}
        else if (KeyCode == GCKeyCodeEight) { c = '8';}
        else if (KeyCode == GCKeyCodeNine) { c = '9';}
        else if (KeyCode == GCKeyCodeZero) { c = '0';}
        else if (KeyCode == GCKeyCodeReturnOrEnter) { c = 10;}
        else if (KeyCode == GCKeyCodeEscape) { c = 27;}
        else if (KeyCode == GCKeyCodeBackslash) { c = 8;}
        else if (KeyCode == GCKeyCodeTab) { c = '\t';}
        else if (KeyCode == GCKeyCodeSpacebar) { c = ' ';}
        else if (KeyCode == GCKeyCodeHyphen) { c = '-';}
        else if (KeyCode == GCKeyCodeEqualSign) { c = '=';}
        else if (KeyCode == GCKeyCodeOpenBracket) { c = '{';}
        else if (KeyCode == GCKeyCodeCloseBracket) { c = '}';}
        else if (KeyCode == GCKeyCodeBackslash) { c = '\\';}
        else if (KeyCode == GCKeyCodeSemicolon) { c = ';';}
        else if (KeyCode == GCKeyCodeQuote) { c = '\"';}
        else if (KeyCode == GCKeyCodeGraveAccentAndTilde) { c = '~';}
        else if (KeyCode == GCKeyCodeComma) { c = ',';}
        else if (KeyCode == GCKeyCodePeriod) { c = '.';}
        else if (KeyCode == GCKeyCodeSlash) { c = '/';}
        
        UE_LOG(LogIOS, Log, TEXT("char: %c"), TCHAR(c));
    }
    return c;
}

constexpr EIOSEventType operator+(EIOSEventType type, int Index) { return (EIOSEventType)((int)type + Index); }
// protects the input stack used on 2 threads
static FCriticalSection CriticalSection;
static TArray<TouchInput> TouchInputStack;
static TArray<int32> KeyInputStack;

bool FIOSInputInterface::bKeyboardInhibited = false;

TSharedRef< FIOSInputInterface > FIOSInputInterface::Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
    return MakeShareable( new FIOSInputInterface( InMessageHandler ) );
}

FIOSInputInterface::FIOSInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
	: FAppleControllerInterface( InMessageHandler )
	, bAllowRemoteRotation(false)
	, bGameSupportsMultipleActiveControllers(false)
    , LastHapticValue(0.0f)
    , MouseDeltaX(0)
    , MouseDeltaY(0)
    , ScrollDeltaY(0)
    , bHaveMouse(false)
{
    SCOPED_BOOT_TIMING("FIOSInputInterface::FIOSInputInterface");

#if !PLATFORM_TVOS
    MotionManager = nil;
    ReferenceAttitude = nil;
#endif
    bPauseMotion = false;
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bDisableMotionData"), bPauseMotion, GEngineIni);
    
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bGameSupportsMultipleActiveControllers"), bGameSupportsMultipleActiveControllers, GEngineIni);
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bAllowRemoteRotation"), bAllowRemoteRotation, GEngineIni);
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bAllowControllers"), bAllowControllers, GEngineIni);
    GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bControllersBlockDeviceFeedback"), bControllersBlockDeviceFeedback, GEngineIni);
    
    
    NSNotificationCenter* notificationCenter = [NSNotificationCenter defaultCenter];
    NSOperationQueue* currentQueue = [NSOperationQueue currentQueue];
    
    [notificationCenter addObserverForName:GCMouseDidConnectNotification object:nil queue:currentQueue usingBlock:^(NSNotification* Notification)
     {
        HandleMouseConnection(Notification.object);
    }];
    
    [notificationCenter addObserverForName:GCMouseDidDisconnectNotification object:nil queue:currentQueue usingBlock:^(NSNotification* Notification)
     {
        HandleMouseDisconnect(Notification.object);
    }];
    
    [notificationCenter addObserverForName:GCKeyboardDidConnectNotification object:nil queue:currentQueue usingBlock:^(NSNotification* Notification)
     {
        HandleKeyboardConnection(Notification.object);
        
    }];
    
    [notificationCenter addObserverForName:GCKeyboardDidDisconnectNotification object:nil queue:currentQueue usingBlock:^(NSNotification* Notification)
     {
        HandleKeyboardDisconnect(Notification.object);
    }];
    
    if ( GCMouse.current )
    {
        HandleMouseConnection( GCMouse.current );
    }
    
    if ( GCKeyboard.coalescedKeyboard )
    {
        HandleKeyboardConnection( GCKeyboard.coalescedKeyboard );
    }
    if (!bGameSupportsMultipleActiveControllers)
    {
        [notificationCenter addObserverForName:GCControllerDidBecomeCurrentNotification object:nil queue:currentQueue usingBlock:^(NSNotification* Notification)
         {
            SignalEvent(EAppleControllerEventType::BecomeCurrent, Notification.object);
        }];
    }
    
    FEmbeddedDelegates::GetNativeToEmbeddedParamsDelegateForSubsystem(TEXT("iosinput")).AddLambda([this](const FEmbeddedCallParamsHelper& Message)
                                                                                                  {
        FString Error;
#if !PLATFORM_TVOS
        // execute any console commands
        if (Message.Command == TEXT("stopmotion"))
        {
            [MotionManager release];
            MotionManager = nil;
            
            bPauseMotion = true;
        }
        else if (Message.Command == TEXT("startmotion"))
        {
            bPauseMotion = false;
        }
        else
#endif
        {
            Error = TEXT("Unknown iosinput command ") + Message.Command;
        }
        
        Message.OnCompleteDelegate({}, Error);
    });

    
#if !PLATFORM_TVOS
    HapticFeedbackSupportLevel = [[[UIDevice currentDevice] valueForKey:@"_feedbackSupportLevel"] intValue];
#else
    HapticFeedbackSupportLevel = 0;
#endif
}

template <EIOSEventType U, EIOSEventType V>
static inline void HandleButtons(GCControllerButtonInput* _Nonnull eventButton, float value, BOOL pressed, TArray<FDeferredIOSEvent>& DeferredEvents, FCriticalSection* EventsMutex)
{
    (void)eventButton;
    (void)value;
    FDeferredIOSEvent DeferredEvent;
    DeferredEvent.type = pressed ? U : V;
    FScopeLock Lock(EventsMutex);
    DeferredEvents.Add(DeferredEvent);
}

void FIOSInputInterface::HandleMouseConnection(GCMouse* Mouse)
{
    bHaveMouse = true;
    MouseDeltaX = 0.f;
    MouseDeltaY = 0.f;
    Mouse.mouseInput.mouseMovedHandler = ^(GCMouseInput * _Nonnull eventMouse, float deltaX, float deltaY) {
        (void)eventMouse;
        this->MouseDeltaX += deltaX;
        this->MouseDeltaY -= deltaY;
    };

    Mouse.mouseInput.leftButton.pressedChangedHandler = ^(GCControllerButtonInput* _Nonnull eventButton, float value, BOOL pressed) {
        HandleButtons< EIOSEventType::LeftMouseDown, EIOSEventType::LeftMouseUp >( eventButton, value, pressed, DeferredEvents, &EventsMutex );
    };

    Mouse.mouseInput.rightButton.pressedChangedHandler = ^(GCControllerButtonInput* _Nonnull eventButton, float value, BOOL pressed) {
        HandleButtons< EIOSEventType::RightMouseDown, EIOSEventType::RightMouseUp >( eventButton, value, pressed, DeferredEvents, &EventsMutex );
    };

    Mouse.mouseInput.middleButton.pressedChangedHandler = ^(GCControllerButtonInput* _Nonnull eventButton, float value, BOOL pressed) {
        HandleButtons< EIOSEventType::MiddleMouseDown, EIOSEventType::MiddleMouseUp >( eventButton, value, pressed, DeferredEvents, &EventsMutex );
    };

    NSArray<GCDeviceButtonInput*>* auxButtons = Mouse.mouseInput.auxiliaryButtons;
    if (auxButtons && auxButtons.count > 0)
    {
        if ( auxButtons.count < 2 )
        {
            auxButtons[0].pressedChangedHandler = ^(GCControllerButtonInput* _Nonnull eventButton, float value, BOOL pressed){
                HandleButtons< EIOSEventType::ThumbDown + 0, EIOSEventType::ThumbUp + 0 >( eventButton, value, pressed, DeferredEvents, &EventsMutex );
            };
        }
        if ( auxButtons.count < 3 )
        {
            auxButtons[0].pressedChangedHandler = ^(GCControllerButtonInput* _Nonnull eventButton, float value, BOOL pressed){
                HandleButtons< EIOSEventType::ThumbDown + 1, EIOSEventType::ThumbUp + 1 >( eventButton, value, pressed, DeferredEvents, &EventsMutex );
            };
        }
    }

    Mouse.mouseInput.scroll.valueChangedHandler = ^(GCControllerDirectionPad* _Nonnull dpad, float xValue, float yValue) {
        (void)xValue;
        ScrollDeltaY += yValue;
    };
}

void FIOSInputInterface::HandleMouseDisconnect(GCMouse* Mouse)
{
    bHaveMouse = false;
}

void FIOSInputInterface::HandleKeyboardConnection(GCKeyboard* Keyboard)
{
    Keyboard.keyboardInput.keyChangedHandler = ^(GCKeyboardInput * _Nonnull keyboard, GCControllerButtonInput * _Nonnull key, GCKeyCode keyCode, BOOL pressed) {
        if ( !(FIOSInputInterface::IsKeyboardInhibited()) )
        {
            FDeferredIOSEvent DeferredEvent;
            DeferredEvent.type = pressed ? EIOSEventType::KeyDown : EIOSEventType::KeyUp;
            DeferredEvent.keycode = TranslateGCKeyCodeToASCII(keyCode);
            
            FScopeLock Lock(&EventsMutex);
            DeferredEvents.Add(DeferredEvent);
        }
    };
}

void FIOSInputInterface::HandleKeyboardDisconnect(GCKeyboard* Keyboard)
{
}

#if !PLATFORM_TVOS
void ModifyVectorByOrientation(FVector& Vec, bool bIsRotation)
{
    switch (FIOSApplication::CachedOrientation)
    {
        case UIInterfaceOrientationPortrait:
            // this is the base orientation, so nothing to do
            break;

        case UIInterfaceOrientationPortraitUpsideDown:
            if (bIsRotation)
            {
                // negate roll and pitch
                Vec.X = -Vec.X;
                Vec.Z = -Vec.Z;
            }
            else
            {
                // negate x/y
                Vec.X = -Vec.X;
                Vec.Y = -Vec.Y;
            }
            break;

        case UIInterfaceOrientationLandscapeRight:
            if (bIsRotation)
            {
                // swap and negate (as needed) roll and pitch
                double Temp = Vec.X;
                Vec.X = -Vec.Z;
                Vec.Z = Temp;
                Vec.Y *= -1.0f;
            }
            else
            {
                // swap and negate (as needed) x and y
                double Temp = Vec.X;
                Vec.X = -Vec.Y;
                Vec.Y = Temp;
            }
            break;

        case UIInterfaceOrientationLandscapeLeft:
            if (bIsRotation)
            {
                // swap and negate (as needed) roll and pitch
                double Temp = Vec.X;
                Vec.X = -Vec.Z;
                Vec.Z = -Temp;
            }
            else
            {
                // swap and negate (as needed) x and y
                double Temp = Vec.X;
                Vec.X = Vec.Y;
                Vec.Y = -Temp;
            }
            break;
    }
}
#endif

void FIOSInputInterface::ProcessTouchesAndKeys(uint32 ControllerId, const TArray<TouchInput>& InTouchInputStack, const TArray<int32>& InKeyInputStack)
{
    for(int i = 0; i < InTouchInputStack.Num(); ++i)
    {
        const TouchInput& Touch = InTouchInputStack[i];
        
        // send input to handler
        if (Touch.Type == TouchBegan)
        {
            MessageHandler->OnTouchStarted( NULL, Touch.Position, Touch.Force, Touch.Handle, ControllerId);
        }
        else if (Touch.Type == TouchEnded)
        {
            MessageHandler->OnTouchEnded(Touch.Position, Touch.Handle, ControllerId);
        }
        else if (Touch.Type == TouchMoved)
        {
            MessageHandler->OnTouchMoved(Touch.Position, Touch.Force, Touch.Handle, ControllerId);
        }
        else if (Touch.Type == ForceChanged)
        {
            MessageHandler->OnTouchForceChanged(Touch.Position, Touch.Force, Touch.Handle, ControllerId);
        }
        else if (Touch.Type == FirstMove)
        {
            MessageHandler->OnTouchFirstMove(Touch.Position, Touch.Force, Touch.Handle, ControllerId);
        }
    }
    
    // these come in pairs
    for(int32 KeyIndex = 0; KeyIndex < InKeyInputStack.Num(); KeyIndex+=2)
    {
        int32 KeyCode = InKeyInputStack[KeyIndex];
        int32 CharCode = InKeyInputStack[KeyIndex + 1];
        MessageHandler->OnKeyDown(KeyCode, CharCode, false);
        MessageHandler->OnKeyChar((TCHAR)CharCode,  false);
        MessageHandler->OnKeyUp  (KeyCode, CharCode, false);
    }
}

void FIOSInputInterface::ProcessDeferredEvents()
{
    TArray<FDeferredIOSEvent> EventsToProcess;

    EventsMutex.Lock();
    EventsToProcess.Append(DeferredEvents);
    DeferredEvents.Empty();
    EventsMutex.Unlock();

    for (uint32_t Index = 0; Index < EventsToProcess.Num(); ++Index)
    {
        ProcessEvent(EventsToProcess[Index]);
    }
}

void FIOSInputInterface::ProcessEvent(const FDeferredIOSEvent& Event)
{
    if (Event.type != EIOSEventType::Invalid)
    {
        switch (Event.type)
        {
            case EIOSEventType::KeyDown:
            {
                MessageHandler->OnKeyDown(Event.keycode, Event.keycode, false);
                break;
            }
            case EIOSEventType::KeyUp:
            {
                MessageHandler->OnKeyUp(Event.keycode, Event.keycode, false);
                break;
            }
            case EIOSEventType::LeftMouseDown:
            {
                MessageHandler->OnMouseDown(nullptr, EMouseButtons::Left);
                break;
            }
            case EIOSEventType::LeftMouseUp:
            {
                MessageHandler->OnMouseUp(EMouseButtons::Left);
                break;
            }
            case EIOSEventType::RightMouseDown:
            {
                MessageHandler->OnMouseDown(nullptr, EMouseButtons::Right);
                break;
            }
            case EIOSEventType::RightMouseUp:
            {
                MessageHandler->OnMouseUp(EMouseButtons::Right);
                break;
            }
            case EIOSEventType::MiddleMouseDown:
            {
                MessageHandler->OnMouseDown(nullptr, EMouseButtons::Middle);
                break;
            }
            case EIOSEventType::MiddleMouseUp:
            {
                MessageHandler->OnMouseUp(EMouseButtons::Middle);
                break;
            }
            case EIOSEventType::ThumbDown+0:
            {
                MessageHandler->OnMouseDown(nullptr, EMouseButtons::Thumb01);
                break;
            }
            case EIOSEventType::ThumbUp+0:
            {
                MessageHandler->OnMouseUp(EMouseButtons::Thumb01);
                break;
            }
            case EIOSEventType::ThumbDown+1:
            {
                MessageHandler->OnMouseDown(nullptr, EMouseButtons::Thumb02);
                break;
            }
            case EIOSEventType::ThumbUp+1:
            {
                MessageHandler->OnMouseUp(EMouseButtons::Thumb02);
                break;
            }
        }
    }
}

void FIOSInputInterface::SendControllerEvents()
{
    TArray<TouchInput> LocalTouchInputStack;
    TArray<int32> LocalKeyInputStack;
    {
        FScopeLock Lock(&CriticalSection);
        Exchange(LocalTouchInputStack, TouchInputStack);
        Exchange(LocalKeyInputStack, KeyInputStack);
    }
    
#if !PLATFORM_TVOS
    // on ios, touches always go go player 0
    ProcessTouchesAndKeys(0, LocalTouchInputStack, LocalKeyInputStack);
#endif
    ProcessDeferredEvents();

    
#if !PLATFORM_TVOS // @todo tvos: This needs to come from the Microcontroller rotation
    if (!bPauseMotion)
    {
        // Update motion controls.
        FVector Attitude;
        FVector RotationRate;
        FVector Gravity;
        FVector Acceleration;

        GetMovementData(Attitude, RotationRate, Gravity, Acceleration);

        // Fix-up yaw to match directions
        Attitude.Y = -Attitude.Y;
        RotationRate.Y = -RotationRate.Y;

        // munge the vectors based on the orientation
        ModifyVectorByOrientation(Attitude, true);
        ModifyVectorByOrientation(RotationRate, true);
        ModifyVectorByOrientation(Gravity, false);
        ModifyVectorByOrientation(Acceleration, false);

        MessageHandler->OnMotionDetected(Attitude, RotationRate, Gravity, Acceleration, 0);
    }
#endif
    
    if ( bHaveMouse )
    {
        MessageHandler->OnRawMouseMove(FMath::TruncToInt(MouseDeltaX), FMath::TruncToInt(MouseDeltaY));
        MouseDeltaX = 0.f;
        MouseDeltaY = 0.f;
        
        MessageHandler->OnMouseWheel( ScrollDeltaY );
        ScrollDeltaY = 0.f;
    }
    
    // Generic Controller update
    FAppleControllerInterface::SendControllerEvents();
    
#if PLATFORM_TVOS
    // tvOS specific controller enumeration to handle micro input if no gamepad
    for(int32 i = 0; i < UE_ARRAY_COUNT(Controllers); ++i)
    {
        FUserController& Controller = Controllers[i];
        if(Controller.PreviousExtendedGamepad == nil)
        {
            GCController* Cont = Controller.Controller;
            GCMicroGamepad* MicroGamepad = [Cont capture].microGamepad;
            if (MicroGamepad != nil && Controller.PlayerIndex != PlayerIndex::PlayerUnset)
            {
                const GCMicroGamepad* PreviousMicroGamepad = Controller.PreviousMicroGamepad;

                HandleButtonGamepad(FGamepadKeyNames::FaceButtonBottom, i);
                HandleButtonGamepad(FGamepadKeyNames::FaceButtonLeft, i);
                HandleButtonGamepad(FGamepadKeyNames::SpecialRight, i);
                
                HandleAnalogGamepad(FGamepadKeyNames::LeftAnalogX, i);
                HandleAnalogGamepad(FGamepadKeyNames::LeftAnalogY, i);

                HandleButtonGamepad(FGamepadKeyNames::LeftStickUp, i);
                HandleButtonGamepad(FGamepadKeyNames::LeftStickDown, i);
                HandleButtonGamepad(FGamepadKeyNames::LeftStickRight, i);
                HandleButtonGamepad(FGamepadKeyNames::LeftStickLeft, i);
                
                [Controller.PreviousMicroGamepad release];
                Controller.PreviousMicroGamepad = MicroGamepad;
                [Controller.PreviousMicroGamepad retain];
            }
        }
    }
#endif
}

void FIOSInputInterface::QueueTouchInput(const TArray<TouchInput>& InTouchEvents)
{
    FScopeLock Lock(&CriticalSection);
    
    TouchInputStack.Append(InTouchEvents);
}

void FIOSInputInterface::QueueKeyInput(int32 Key, int32 Char)
{
    FScopeLock Lock(&CriticalSection);
    
    // put the key and char into the array
    KeyInputStack.Add(Key);
    KeyInputStack.Add(Char);
}

void FIOSInputInterface::EnableMotionData(bool bEnable)
{
    bPauseMotion = !bEnable;
    
#if !PLATFORM_TVOS
    if (bPauseMotion && MotionManager != nil)
    {
        [ReferenceAttitude release];
        ReferenceAttitude = nil;
        
        [MotionManager release];
        MotionManager = nil;
    }
    // When enabled MotionManager will be initialized on first use
#endif
}

bool FIOSInputInterface::IsMotionDataEnabled() const
{
    return !bPauseMotion;
}

void FIOSInputInterface::GetMovementData(FVector& Attitude, FVector& RotationRate, FVector& Gravity, FVector& Acceleration)
{
#if !PLATFORM_TVOS
    // initialize on first use
    if (MotionManager == nil)
    {
        // Look to see if we can create the motion manager
        MotionManager = [[CMMotionManager alloc] init];
        
        // Check to see if the device supports full motion (gyro + accelerometer)
        if (MotionManager.deviceMotionAvailable)
        {
            MotionManager.deviceMotionUpdateInterval = 0.02;
            
            // Start the Device updating motion
            [MotionManager startDeviceMotionUpdates];
        }
        else
        {
            [MotionManager startAccelerometerUpdates];
            CenterPitch = CenterPitch = 0;
            bIsCalibrationRequested = false;
        }
    }
    
    // do we have full motion data?
    if (MotionManager.deviceMotionActive)
    {
        // Grab the values
        CMAttitude* CurrentAttitude = MotionManager.deviceMotion.attitude;
        CMRotationRate CurrentRotationRate = MotionManager.deviceMotion.rotationRate;
        CMAcceleration CurrentGravity = MotionManager.deviceMotion.gravity;
        CMAcceleration CurrentUserAcceleration = MotionManager.deviceMotion.userAcceleration;
        
        // apply a reference attitude if we have been calibrated away from default
        if (ReferenceAttitude)
        {
            [CurrentAttitude multiplyByInverseOfAttitude : ReferenceAttitude];
        }
        
        // convert to Unreal coordinate system
        Attitude = FVector(float(CurrentAttitude.pitch), float(CurrentAttitude.yaw), float(CurrentAttitude.roll));
        RotationRate = FVector(float(CurrentRotationRate.x), float(CurrentRotationRate.y), float(CurrentRotationRate.z));
        Gravity = FVector(float(CurrentGravity.x), float(CurrentGravity.y), float(CurrentGravity.z));
        Acceleration = FVector(float(CurrentUserAcceleration.x), float(CurrentUserAcceleration.y), float(CurrentUserAcceleration.z));
    }
    else
    {
        // get the plain accleration
        CMAcceleration RawAcceleration = [MotionManager accelerometerData].acceleration;
        FVector NewAcceleration(RawAcceleration.x, RawAcceleration.y, RawAcceleration.z);
        
        // storage for keeping the accelerometer values over time (for filtering)
        static bool bFirstAccel = true;
        
        // how much of the previous frame's acceleration to keep
        const float VectorFilter = bFirstAccel ? 0.0f : 0.85f;
        bFirstAccel = false;
        
        // apply new accelerometer values to last frames
        FilteredAccelerometer = FilteredAccelerometer * VectorFilter + (1.0f - VectorFilter) * NewAcceleration;
        
        // create an normalized acceleration vector
        FVector FinalAcceleration = -FilteredAccelerometer.GetSafeNormal();
        
        // calculate Roll/Pitch
        float CurrentPitch = (float)FMath::Atan2(FinalAcceleration.Y, FinalAcceleration.Z);
        float CurrentRoll = -(float)FMath::Atan2(FinalAcceleration.X, FinalAcceleration.Z);
        
        // if we want to calibrate, use the current values as center
        if (bIsCalibrationRequested)
        {
            CenterPitch = CurrentPitch;
            CenterRoll = CurrentRoll;
            bIsCalibrationRequested = false;
        }
        
        CurrentPitch -= CenterPitch;
        CurrentRoll -= CenterRoll;
        
        Attitude = FVector(CurrentPitch, 0, CurrentRoll);
        RotationRate = FVector(LastPitch - CurrentPitch, 0, LastRoll - CurrentRoll);
        Gravity = FVector(0, 0, 0);
        
        // use the raw acceleration for acceleration
        Acceleration = NewAcceleration;
        
        // remember for next time (for rotation rate)
        LastPitch = CurrentPitch;
        LastRoll = CurrentRoll;
    }
#endif
}

void FIOSInputInterface::CalibrateMotion(uint32 PlayerIndex)
{
#if !PLATFORM_TVOS
    // If we are using the motion manager, grab a reference frame.  Note, once you set the Attitude Reference frame
    // all additional reference information will come from it
    if (MotionManager && MotionManager.deviceMotionActive)
    {
        ReferenceAttitude = [MotionManager.deviceMotion.attitude retain];
    }
    else
    {
        bIsCalibrationRequested = true;
    }
#endif
    
    if (PlayerIndex >= 0 && PlayerIndex < UE_ARRAY_COUNT(Controllers))
    {
        Controllers[PlayerIndex].bNeedsReferenceAttitude = true;
    }
}

bool FIOSInputInterface::Exec_Runtime(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
    // Keep track whether the command was handled or not.
    bool bHandledCommand = false;
    
    if (FParse::Command(&Cmd, TEXT("CALIBRATEMOTION")))
    {
        uint32 PlayerIndex = FCString::Atoi(Cmd);
        CalibrateMotion(PlayerIndex);
        bHandledCommand = true;
    }
    
    return bHandledCommand;
}

void FIOSInputInterface::SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
    if(IsGamepadAttached() && bControllersBlockDeviceFeedback)
    {
        Value = 0.0f;
    }
    
    if(HapticFeedbackSupportLevel >= 2)
    {
        // if we are at rest, then kick when we are over the Kick cutoff
        if (LastHapticValue == 0.0f && Value > 0.0f)
        {
            const float HeavyKickVal = CVarHapticsKickHeavy.GetValueOnGameThread();
            const float MediumKickVal = CVarHapticsKickMedium.GetValueOnGameThread();
            const float LightKickVal = CVarHapticsKickLight.GetValueOnGameThread();
            // once we get past the
            if (Value > LightKickVal)
            {
                if (Value > HeavyKickVal)
                {
                    FPlatformMisc::PrepareMobileHaptics(EMobileHapticsType::ImpactHeavy);
                }
                else if (Value > MediumKickVal)
                {
                    FPlatformMisc::PrepareMobileHaptics(EMobileHapticsType::ImpactMedium);
                }
                else
                {
                    FPlatformMisc::PrepareMobileHaptics(EMobileHapticsType::ImpactLight);
                }

                FPlatformMisc::TriggerMobileHaptics();
                
                // remember it to not kick again
                LastHapticValue = Value;
            }
        }
        else
        {
            const float RestVal = CVarHapticsRest.GetValueOnGameThread();

            if (Value >= RestVal)
            {
                // always remember the last value if we are over the Rest amount
                LastHapticValue = Value;
            }
            else
            {
                // release the haptics
                FPlatformMisc::ReleaseMobileHaptics();
                
                // rest
                LastHapticValue = 0.0f;
            }
        }
    }
    else
    {
        if(Value >= 0.3f)
        {
            AudioServicesPlaySystemSound(kSystemSoundID_Vibrate);
        }
    }
}

void FIOSInputInterface::SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values)
{
    // Use largest vibration state as value
    float MaxLeft = Values.LeftLarge > Values.LeftSmall ? Values.LeftLarge : Values.LeftSmall;
    float MaxRight = Values.RightLarge > Values.RightSmall ? Values.RightLarge : Values.RightSmall;
    float Value = MaxLeft > MaxRight ? MaxLeft : MaxRight;
    
    // the other function will just play, regardless of channel
    SetForceFeedbackChannelValue(ControllerId, FForceFeedbackChannelType::LEFT_LARGE, Value);
}

NSData* FIOSInputInterface::GetGamepadGlyphRawData(const FGamepadKeyNames::Type& ButtonKey, uint32 ControllerIndex)
{
    GCController* Cont = Controllers[ControllerIndex].Controller;
    GCExtendedGamepad* ExtendedGamepad = Cont.extendedGamepad;
    if (ExtendedGamepad == nil)
    {
        NSLog(@"Siri Remote is not compatible with glyphs.");
        return nullptr;
    }
    
    GCControllerButtonInput *ButtonToReturnGlyphOf = GetGCControllerButton(ButtonKey, ControllerIndex);
    
    UIImage* Image = nullptr;
    NSString *ButtonStringName = ButtonToReturnGlyphOf.sfSymbolsName;
    Image = [UIImage systemImageNamed:ButtonStringName];
    
    return UIImagePNGRepresentation(Image);
}
