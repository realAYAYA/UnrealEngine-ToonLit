// Copyright Epic Games, Inc. All Rights Reserved.

#include "AppleControllerInterface.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"

DEFINE_LOG_CATEGORY(LogAppleController);

#define APPLE_CONTROLLER_DEBUG 0

TSharedRef< FAppleControllerInterface > FAppleControllerInterface::Create(  const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	return MakeShareable( new FAppleControllerInterface( InMessageHandler ) );
}

FAppleControllerInterface::FAppleControllerInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
	: MessageHandler( InMessageHandler )
	, bAllowControllers(true)
{
	if(!IS_PROGRAM)
	{
		// Clear array and setup unset player index values
		FMemory::Memzero(Controllers, sizeof(Controllers));
		for (int32 ControllerIndex = 0; ControllerIndex < UE_ARRAY_COUNT(Controllers); ControllerIndex++)
		{
			FUserController& UserController = Controllers[ControllerIndex];
			UserController.PlayerIndex = PlayerIndex::PlayerUnset;
		}
		
		for (GCController* Cont in [GCController controllers])
		{
			HandleConnection(Cont);
		}
		
		NSNotificationCenter* notificationCenter = [NSNotificationCenter defaultCenter];
		
		// Not in an operation queue, [NSOperationQueue currentQueue] will return nil on macOS and iOS
		// Notification callback will always be on the app main thread - defer events for Unreal Engine update thread

		[notificationCenter addObserverForName:GCControllerDidDisconnectNotification object:nil queue:nil usingBlock:^(NSNotification* Notification)
		{
			SignalEvent(EAppleControllerEventType::Disconnect, Notification.object);
		}];

		[notificationCenter addObserverForName:GCControllerDidConnectNotification object:nil queue:nil usingBlock:^(NSNotification* Notification)
		{
			SignalEvent(EAppleControllerEventType::Connect, Notification.object);
		}];

		dispatch_async(dispatch_get_main_queue(), ^
		{
		   [GCController startWirelessControllerDiscoveryWithCompletionHandler:^{ }];
		});
	}
}

void FAppleControllerInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;
}

void FAppleControllerInterface::SignalEvent(EAppleControllerEventType InEventType, GCController* InController)
{
	FScopeLock Lock(&DeferredEventCS);
	DeferredEvents.Add(FDeferredAppleControllerEvent(InEventType, InController));
}

void FAppleControllerInterface::Tick( float DeltaTime )
{
	FScopeLock Lock(&DeferredEventCS);
	
	for(uint32_t Index = 0;Index < DeferredEvents.Num();++Index)
	{
		FDeferredAppleControllerEvent& Event = DeferredEvents[Index];
		switch(Event.EventType)
		{
			case EAppleControllerEventType::Connect:
			{
				HandleConnection(Event.Controller);
				break;
			}
			case EAppleControllerEventType::Disconnect:
			{
				HandleDisconnect(Event.Controller);
				break;
			}
			case EAppleControllerEventType::BecomeCurrent:
			{
				SetCurrentController(Event.Controller);
				break;
			}
			case EAppleControllerEventType::Invalid:
			default:
			{
				// NOP
				break;
			}
		}
	}
	
	DeferredEvents.Empty();
}

void FAppleControllerInterface::SetControllerType(uint32 ControllerIndex)
{
    GCController *Controller = Controllers[ControllerIndex].Controller;

    if ([Controller.productCategory isEqualToString:@"DualShock 4"])
    {
        Controllers[ControllerIndex].ControllerType = ControllerType::DualShockGamepad;
    }
    else if ([Controller.productCategory isEqualToString:@"Xbox One"])
    {
        Controllers[ControllerIndex].ControllerType = ControllerType::XboxGamepad;
    }
    else if ([Controller.productCategory isEqualToString:@"DualSense"])
    {
        Controllers[ControllerIndex].ControllerType = ControllerType::DualSenseGamepad;
    }

    else if (Controller.extendedGamepad != nil)
    {
        Controllers[ControllerIndex].ControllerType = ControllerType::ExtendedGamepad;
    }
    else if (Controller.microGamepad != nil)
    {
        Controllers[ControllerIndex].ControllerType = ControllerType::SiriRemote;
    }
    else
    {
        Controllers[ControllerIndex].ControllerType = ControllerType::Unassigned;
        UE_LOG(LogAppleController, Warning, TEXT("Controller type is not recognized"));
    }
}

void FAppleControllerInterface::SetCurrentController(GCController* Controller)
{
    int32 ControllerIndex = 0;
    PlayerIndex PreviousIndex = PlayerIndex::PlayerUnset;

    for (ControllerIndex = 0; ControllerIndex < UE_ARRAY_COUNT(Controllers); ControllerIndex++)
    {
        if (Controllers[ControllerIndex].Controller == Controller)
        {
            if (Controllers[ControllerIndex].PlayerIndex == PlayerIndex::PlayerOne)
            {
                // Already set as CurrentController
                return;
            }
            PreviousIndex = Controllers[ControllerIndex].PlayerIndex;
            Controllers[ControllerIndex].PlayerIndex = PlayerIndex::PlayerOne;
        }
    }
    
    for (ControllerIndex = 0; ControllerIndex < UE_ARRAY_COUNT(Controllers); ControllerIndex++)
    {
        if (Controllers[ControllerIndex].PlayerIndex == PlayerIndex::PlayerOne && Controllers[ControllerIndex].Controller != Controller)
        {
            // The old PlayerOne, should swap place with the new PlayerOne
            Controllers[ControllerIndex].PlayerIndex = PreviousIndex;
        }
    }
}

void FAppleControllerInterface::HandleConnection(GCController* Controller)
{
	static_assert(GCControllerPlayerIndex1 == 0 && GCControllerPlayerIndex4 == 3, "Apple changed the player index enums");

	if (!bAllowControllers)
	{
		return;
	}
	
	static_assert(GCControllerPlayerIndex1 == 0 && GCControllerPlayerIndex4 == 3, "Apple changed the player index enums");

	// find a good controller index to use
	bool bFoundSlot = false;
	for (int32 ControllerIndex = 0; ControllerIndex < UE_ARRAY_COUNT(Controllers); ControllerIndex++)
	{
        if (Controllers[ControllerIndex].ControllerType != ControllerType::Unassigned)
        {
            continue;
        }
        
        Controllers[ControllerIndex].PlayerIndex = (PlayerIndex)ControllerIndex;
        Controllers[ControllerIndex].Controller = [Controller retain];
        SetControllerType(ControllerIndex);
        
        // Deprecated but buttonMenu behavior is unreliable since iOS/tvOS 14
		Controllers[ControllerIndex].bPauseWasPressed = false;
PRAGMA_DISABLE_DEPRECATION_WARNINGS
		Controller.controllerPausedHandler = ^(GCController* Cont)
		{
			Controllers[ControllerIndex].bPauseWasPressed = true;
		};
PRAGMA_ENABLE_DEPRECATION_WARNINGS
        
        bFoundSlot = true;
        
        IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
        FPlatformUserId UserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(Controllers[ControllerIndex].PlayerIndex);
        FInputDeviceId DeviceId = INPUTDEVICEID_NONE;
        DeviceMapper.RemapControllerIdToPlatformUserAndDevice(Controllers[ControllerIndex].PlayerIndex, OUT UserId, OUT DeviceId);
        DeviceMapper.Internal_MapInputDeviceToUser(DeviceId, UserId, EInputDeviceConnectionState::Connected);
        
        UE_LOG(LogAppleController, Log, TEXT("New %s controller inserted, assigned to playerIndex %d"),
               Controllers[ControllerIndex].ControllerType == ControllerType::ExtendedGamepad ||
               Controllers[ControllerIndex].ControllerType == ControllerType::XboxGamepad ||
               Controllers[ControllerIndex].ControllerType == ControllerType::DualShockGamepad
               ? TEXT("Gamepad") : TEXT("Remote"), Controllers[ControllerIndex].PlayerIndex);
        break;
	}
	checkf(bFoundSlot, TEXT("Used a fifth controller somehow!"));
}

void FAppleControllerInterface::HandleDisconnect(GCController* Controller)
{
		// if we don't allow controllers, there could be unset player index here
	if (!bAllowControllers)
	{
        return;
	}
	
    for (int32 ControllerIndex = 0; ControllerIndex < UE_ARRAY_COUNT(Controllers); ControllerIndex++)
    {
		FUserController& UserController = Controllers[ControllerIndex];
        if (UserController.Controller == Controller)
        {
			// Player index of unset(-1) would indicate that it has become unset even though it is now trying to disconnect
			// This can occur on iOS when bGameSupportsMultipleActiveControllers is false
			UE_LOG(LogAppleController, Log, TEXT("Controller for playerIndex %d, controller Index %d removed"), UserController.PlayerIndex, ControllerIndex);
            
            IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
            FPlatformUserId UserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(UserController.PlayerIndex);
            FInputDeviceId DeviceId = INPUTDEVICEID_NONE;
            DeviceMapper.RemapControllerIdToPlatformUserAndDevice(UserController.PlayerIndex, OUT UserId, OUT DeviceId);
            DeviceMapper.Internal_MapInputDeviceToUser(DeviceId, UserId, EInputDeviceConnectionState::Disconnected);
			
			[UserController.Controller release];
			[UserController.PreviousExtendedGamepad release];
			
            FMemory::Memzero(&UserController, sizeof(Controllers[ControllerIndex]));
            UserController.PlayerIndex = PlayerIndex::PlayerUnset;
            
            return;
        }
    }
}

void FAppleControllerInterface::SendControllerEvents()
{
	@autoreleasepool{
    for(int32 i = 0; i < UE_ARRAY_COUNT(Controllers); ++i)
 	{
		FUserController& Controller = Controllers[i];
		
		// make sure the connection handler has run on this
		if (Controller.PlayerIndex == PlayerIndex::PlayerUnset)
		{
            continue;
		}
		
		GCController* ControllerImpl = Controller.Controller;
		
		IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
		FPlatformUserId UserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(Controller.PlayerIndex);
		FInputDeviceId DeviceId = DeviceMapper.GetPrimaryInputDeviceForUser(UserId);
		
        GCExtendedGamepad* ExtendedGamepad = [ControllerImpl capture].extendedGamepad;
		GCMotion* Motion = ControllerImpl.motion;
		
		// Workaround for unreliable buttonMenu behavior since iOS/tvOS 14
		if (Controller.bPauseWasPressed)
        {
            MessageHandler->OnControllerButtonPressed(FGamepadKeyNames::SpecialRight, UserId, DeviceId, false);
            MessageHandler->OnControllerButtonReleased(FGamepadKeyNames::SpecialRight, UserId, DeviceId, false);

            Controller.bPauseWasPressed = false;
        }
        
		if (ExtendedGamepad != nil)
		{
            const GCExtendedGamepad* PreviousExtendedGamepad = Controller.PreviousExtendedGamepad;

            HandleButtonGamepad(FGamepadKeyNames::FaceButtonBottom, i);
            HandleButtonGamepad(FGamepadKeyNames::FaceButtonLeft, i);
            HandleButtonGamepad(FGamepadKeyNames::FaceButtonRight, i);
            HandleButtonGamepad(FGamepadKeyNames::FaceButtonTop, i);
            HandleButtonGamepad(FGamepadKeyNames::LeftShoulder, i);
            HandleButtonGamepad(FGamepadKeyNames::RightShoulder, i);
            HandleButtonGamepad(FGamepadKeyNames::LeftTriggerThreshold, i);
            HandleButtonGamepad(FGamepadKeyNames::RightTriggerThreshold, i);
            HandleButtonGamepad(FGamepadKeyNames::DPadUp, i);
            HandleButtonGamepad(FGamepadKeyNames::DPadDown, i);
            HandleButtonGamepad(FGamepadKeyNames::DPadRight, i);
            HandleButtonGamepad(FGamepadKeyNames::DPadLeft, i);
            HandleButtonGamepad(FGamepadKeyNames::SpecialRight, i);
            HandleButtonGamepad(FGamepadKeyNames::SpecialLeft, i);
            
            HandleAnalogGamepad(FGamepadKeyNames::LeftAnalogX, i);
            HandleAnalogGamepad(FGamepadKeyNames::LeftAnalogY, i);
            HandleAnalogGamepad(FGamepadKeyNames::RightAnalogX, i);
            HandleAnalogGamepad(FGamepadKeyNames::RightAnalogY, i);
            HandleAnalogGamepad(FGamepadKeyNames::RightTriggerAnalog, i);
            HandleAnalogGamepad(FGamepadKeyNames::LeftTriggerAnalog, i);


            HandleVirtualButtonGamepad(FGamepadKeyNames::LeftStickRight, FGamepadKeyNames::LeftStickLeft, i);
            HandleVirtualButtonGamepad(FGamepadKeyNames::LeftStickDown, FGamepadKeyNames::LeftStickUp, i);
            HandleVirtualButtonGamepad(FGamepadKeyNames::RightStickLeft, FGamepadKeyNames::RightStickRight, i);
            HandleVirtualButtonGamepad(FGamepadKeyNames::RightStickDown, FGamepadKeyNames::RightStickUp, i);
            HandleButtonGamepad(FGamepadKeyNames::LeftThumb, i);
            HandleButtonGamepad(FGamepadKeyNames::RightThumb, i);

            [Controller.PreviousExtendedGamepad release];
            Controller.PreviousExtendedGamepad = ExtendedGamepad;
            [Controller.PreviousExtendedGamepad retain];
		}
	}
	} //@autoreleasepool
}

bool FAppleControllerInterface::IsControllerAssignedToGamepad(int32 ControllerId) const
{
	return ControllerId < UE_ARRAY_COUNT(Controllers) &&
		(Controllers[ControllerId].ControllerType != ControllerType::Unassigned);
}

bool FAppleControllerInterface::IsGamepadAttached() const
{
	bool bIsAttached = false;
	for(int32 i = 0; i < UE_ARRAY_COUNT(Controllers); ++i)
	{
		bIsAttached |= IsControllerAssignedToGamepad(i);
	}
	return bIsAttached && bAllowControllers;
}

GCControllerButtonInput* FAppleControllerInterface::GetGCControllerButton(const FGamepadKeyNames::Type& ButtonKey, uint32 ControllerIndex)
{
    GCController* Cont = Controllers[ControllerIndex].Controller;
    
    GCExtendedGamepad* ExtendedGamepad = Cont.extendedGamepad;
    GCControllerButtonInput *ButtonToReturn = nullptr;

    if (ButtonKey == FGamepadKeyNames::FaceButtonBottom){ButtonToReturn = ExtendedGamepad.buttonA;}
    else if (ButtonKey == FGamepadKeyNames::FaceButtonRight){ButtonToReturn = ExtendedGamepad.buttonB;}
    else if (ButtonKey == FGamepadKeyNames::FaceButtonLeft){ButtonToReturn = ExtendedGamepad.buttonX;}
    else if (ButtonKey == FGamepadKeyNames::FaceButtonTop){ButtonToReturn = ExtendedGamepad.buttonY;}
    else if (ButtonKey == FGamepadKeyNames::LeftShoulder){ButtonToReturn = ExtendedGamepad.leftShoulder;}
    else if (ButtonKey == FGamepadKeyNames::RightShoulder){ButtonToReturn = ExtendedGamepad.rightShoulder;}
    else if (ButtonKey == FGamepadKeyNames::LeftTriggerThreshold){ButtonToReturn = ExtendedGamepad.leftTrigger;}
    else if (ButtonKey == FGamepadKeyNames::RightTriggerThreshold){ButtonToReturn = ExtendedGamepad.rightTrigger;}
    else if (ButtonKey == FGamepadKeyNames::LeftTriggerAnalog){ButtonToReturn = ExtendedGamepad.leftTrigger;}
    else if (ButtonKey == FGamepadKeyNames::RightTriggerAnalog){ButtonToReturn = ExtendedGamepad.rightTrigger;}
    else if (ButtonKey == FGamepadKeyNames::LeftThumb){ButtonToReturn = ExtendedGamepad.leftThumbstickButton;}
    else if (ButtonKey == FGamepadKeyNames::RightThumb){ButtonToReturn = ExtendedGamepad.rightThumbstickButton;}

    return ButtonToReturn;
}

const ControllerType FAppleControllerInterface::GetControllerType(uint32 ControllerIndex)
{
    if (Controllers[ControllerIndex].Controller != nullptr)
    {
        return Controllers[ControllerIndex].ControllerType;
    }
    return ControllerType::Unassigned;
}

void FAppleControllerInterface::HandleInputInternal(const FGamepadKeyNames::Type& UEButton, uint32 ControllerIndex, bool bIsPressed, bool bWasPressed)
{
    const double CurrentTime = FPlatformTime::Seconds();
    const float InitialRepeatDelay = 0.2f;
    const float RepeatDelay = 0.1f;
    GCController* Cont = Controllers[ControllerIndex].Controller;
    
	IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId UserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(Controllers[ControllerIndex].PlayerIndex);
    FInputDeviceId DeviceId = DeviceMapper.GetPrimaryInputDeviceForUser(UserId);

    if (bWasPressed != bIsPressed)
    {
#if APPLE_CONTROLLER_DEBUG
        NSLog(@"%@ button %s on controller %d", bIsPressed ? @"Pressed" : @"Released", TCHAR_TO_ANSI(*UEButton.ToString()), Controllers[ControllerIndex].PlayerIndex);
#endif
        bIsPressed ? MessageHandler->OnControllerButtonPressed(UEButton, UserId, DeviceId, false) : MessageHandler->OnControllerButtonReleased(UEButton,UserId, DeviceId, false);
        NextKeyRepeatTime.FindOrAdd(UEButton) = CurrentTime + InitialRepeatDelay;
    }
    else if(bIsPressed)
    {
        double* NextRepeatTime = NextKeyRepeatTime.Find(UEButton);
        if(NextRepeatTime && *NextRepeatTime <= CurrentTime)
        {
            MessageHandler->OnControllerButtonPressed(UEButton, UserId, DeviceId, true);
            *NextRepeatTime = CurrentTime + RepeatDelay;
        }
    }
    else
    {
        NextKeyRepeatTime.Remove(UEButton);
    }
}

void FAppleControllerInterface::HandleVirtualButtonGamepad(const FGamepadKeyNames::Type& UEButtonNegative, const FGamepadKeyNames::Type& UEButtonPositive, uint32 ControllerIndex)
{
    GCController* Cont = Controllers[ControllerIndex].Controller;
    GCExtendedGamepad *ExtendedGamepad = Cont.extendedGamepad;
    GCExtendedGamepad *ExtendedPreviousGamepad = Controllers[ControllerIndex].PreviousExtendedGamepad;;

    // Send controller events any time we are passed the given input threshold similarly to PC/Console (see: XInputInterface.cpp)
    const float RepeatDeadzone = 0.24f;
    
    bool bWasNegativePressed = false;
    bool bNegativePressed = false;
    bool bWasPositivePressed = false;
    bool bPositivePressed = false;
    
    if (UEButtonNegative == FGamepadKeyNames::LeftStickLeft && UEButtonPositive == FGamepadKeyNames::LeftStickRight)
    {
        bWasNegativePressed = ExtendedPreviousGamepad != nil && ExtendedPreviousGamepad.leftThumbstick.xAxis.value <= -RepeatDeadzone;
        bNegativePressed = ExtendedGamepad.leftThumbstick.xAxis.value <= -RepeatDeadzone;
        bWasPositivePressed = ExtendedPreviousGamepad != nil && ExtendedPreviousGamepad.leftThumbstick.xAxis.value >= RepeatDeadzone;
        bPositivePressed = ExtendedGamepad.leftThumbstick.xAxis.value >= RepeatDeadzone;

        HandleInputInternal(FGamepadKeyNames::LeftStickDown, ControllerIndex, bNegativePressed, bWasNegativePressed);
        HandleInputInternal(FGamepadKeyNames::LeftStickUp, ControllerIndex, bPositivePressed, bWasPositivePressed);
    }
    else if (UEButtonNegative == FGamepadKeyNames::LeftStickDown && UEButtonPositive == FGamepadKeyNames::LeftStickUp)
    {
        bWasNegativePressed = ExtendedPreviousGamepad != nil && ExtendedPreviousGamepad.leftThumbstick.yAxis.value <= -RepeatDeadzone;
        bNegativePressed = ExtendedGamepad.leftThumbstick.yAxis.value <= -RepeatDeadzone;
        bWasPositivePressed = ExtendedPreviousGamepad != nil && ExtendedPreviousGamepad.leftThumbstick.yAxis.value >= RepeatDeadzone;
        bPositivePressed = ExtendedGamepad.leftThumbstick.yAxis.value >= RepeatDeadzone;

        HandleInputInternal(FGamepadKeyNames::LeftStickDown, ControllerIndex, bNegativePressed, bWasNegativePressed);
        HandleInputInternal(FGamepadKeyNames::LeftStickUp, ControllerIndex, bPositivePressed, bWasPositivePressed);
    }
    else if (UEButtonNegative == FGamepadKeyNames::RightStickLeft && UEButtonPositive == FGamepadKeyNames::RightStickRight)
    {
        bWasNegativePressed = ExtendedPreviousGamepad != nil && ExtendedPreviousGamepad.rightThumbstick.xAxis.value <= -RepeatDeadzone;
        bNegativePressed = ExtendedGamepad.rightThumbstick.xAxis.value <= -RepeatDeadzone;
        bWasPositivePressed = ExtendedPreviousGamepad != nil && ExtendedPreviousGamepad.rightThumbstick.xAxis.value >= RepeatDeadzone;
        bPositivePressed = ExtendedGamepad.rightThumbstick.xAxis.value >= RepeatDeadzone;

        HandleInputInternal(FGamepadKeyNames::LeftStickDown, ControllerIndex, bNegativePressed, bWasNegativePressed);
        HandleInputInternal(FGamepadKeyNames::LeftStickUp, ControllerIndex, bPositivePressed, bWasPositivePressed);
    }
    else if (UEButtonNegative == FGamepadKeyNames::RightStickDown && UEButtonPositive == FGamepadKeyNames::RightStickUp)
    {
        bWasNegativePressed = ExtendedPreviousGamepad != nil && ExtendedPreviousGamepad.rightThumbstick.yAxis.value <= -RepeatDeadzone;
        bNegativePressed = ExtendedGamepad.rightThumbstick.yAxis.value <= -RepeatDeadzone;
        bWasPositivePressed = ExtendedPreviousGamepad != nil && ExtendedPreviousGamepad.rightThumbstick.yAxis.value >= RepeatDeadzone;
        bPositivePressed = ExtendedGamepad.rightThumbstick.yAxis.value >= RepeatDeadzone;

        HandleInputInternal(FGamepadKeyNames::LeftStickDown, ControllerIndex, bNegativePressed, bWasNegativePressed);
        HandleInputInternal(FGamepadKeyNames::LeftStickUp, ControllerIndex, bPositivePressed, bWasPositivePressed);
    }
}

void FAppleControllerInterface::HandleButtonGamepad(const FGamepadKeyNames::Type& UEButton, uint32 ControllerIndex)
{
    GCController* Cont = Controllers[ControllerIndex].Controller;

    
    bool bWasPressed = false;
    bool bIsPressed = false;

    GCExtendedGamepad *ExtendedGamepad = nil;
    GCExtendedGamepad *ExtendedPreviousGamepad = nil;

    GCMicroGamepad *MicroGamepad = nil;
    GCMicroGamepad *MicroPreviousGamepad = nil;

#define SET_PRESSED(Gamepad, PreviousGamepad, GCButton, UEButton) \
{ \
bWasPressed = PreviousGamepad.GCButton.pressed; \
bIsPressed = Gamepad.GCButton.pressed; \
}
    switch (Controllers[ControllerIndex].ControllerType)
    {
        case ControllerType::ExtendedGamepad:
        case ControllerType::DualShockGamepad:
        case ControllerType::XboxGamepad:
        case ControllerType::DualSenseGamepad:
            
            ExtendedGamepad = Cont.extendedGamepad;
            ExtendedPreviousGamepad = Controllers[ControllerIndex].PreviousExtendedGamepad;
       
            if (UEButton == FGamepadKeyNames::FaceButtonLeft){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, buttonX, UEButton);}
            else if (UEButton == FGamepadKeyNames::FaceButtonBottom){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, buttonA, UEButton);}
            else if (UEButton == FGamepadKeyNames::FaceButtonRight){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, buttonB, UEButton);}
            else if (UEButton == FGamepadKeyNames::FaceButtonTop){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, buttonY, UEButton);}
            else if (UEButton == FGamepadKeyNames::LeftShoulder){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, leftShoulder, UEButton);}
            else if (UEButton == FGamepadKeyNames::RightShoulder){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, rightShoulder, UEButton);}
            else if (UEButton == FGamepadKeyNames::LeftTriggerThreshold){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, leftTrigger, UEButton);}
            else if (UEButton == FGamepadKeyNames::RightTriggerThreshold){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, rightTrigger, UEButton);}
            else if (UEButton == FGamepadKeyNames::DPadUp){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, dpad.up, UEButton);}
            else if (UEButton == FGamepadKeyNames::DPadDown){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, dpad.down, UEButton);}
            else if (UEButton == FGamepadKeyNames::DPadRight){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, dpad.right, UEButton);}
            else if (UEButton == FGamepadKeyNames::DPadLeft){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, dpad.left, UEButton);}
            else if (UEButton == FGamepadKeyNames::SpecialRight){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, buttonMenu, UEButton);}
            else if (UEButton == FGamepadKeyNames::SpecialLeft){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, buttonOptions, UEButton);}
            else if (UEButton == FGamepadKeyNames::LeftThumb){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, leftThumbstickButton, UEButton);}
            else if (UEButton == FGamepadKeyNames::RightThumb){SET_PRESSED(ExtendedGamepad, ExtendedPreviousGamepad, rightThumbstickButton, UEButton);}
            break;
        case ControllerType::SiriRemote:
            
            MicroGamepad = Cont.microGamepad;
            MicroPreviousGamepad = Controllers[ControllerIndex].PreviousMicroGamepad;

            if (UEButton == FGamepadKeyNames::LeftStickUp){SET_PRESSED(MicroGamepad, MicroPreviousGamepad, dpad.up, UEButton);}
            else if (UEButton == FGamepadKeyNames::LeftStickDown){SET_PRESSED(MicroGamepad, MicroPreviousGamepad, dpad.down, UEButton);}
            else if (UEButton == FGamepadKeyNames::LeftStickRight){SET_PRESSED(MicroGamepad, MicroPreviousGamepad, dpad.right, UEButton);}
            else if (UEButton == FGamepadKeyNames::LeftStickLeft){SET_PRESSED(MicroGamepad, MicroPreviousGamepad, dpad.left, UEButton);}
            else if (UEButton == FGamepadKeyNames::FaceButtonBottom){SET_PRESSED(MicroGamepad, MicroPreviousGamepad, buttonA, UEButton);}
            else if (UEButton == FGamepadKeyNames::FaceButtonLeft){SET_PRESSED(MicroGamepad, MicroPreviousGamepad, buttonX, UEButton);}
            else if (UEButton == FGamepadKeyNames::SpecialRight){SET_PRESSED(MicroGamepad, MicroPreviousGamepad, buttonMenu, UEButton);}
            break;
    }
    HandleInputInternal(UEButton, ControllerIndex, bIsPressed, bWasPressed);
}

void FAppleControllerInterface::HandleAnalogGamepad(const FGamepadKeyNames::Type& UEAxis, uint32 ControllerIndex)
{
    GCController* Cont = Controllers[ControllerIndex].Controller;
    
    IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
	FPlatformUserId UserId = FGenericPlatformMisc::GetPlatformUserForUserIndex(Controllers[ControllerIndex].PlayerIndex);
    FInputDeviceId DeviceId = DeviceMapper.GetPrimaryInputDeviceForUser(UserId);
    
    // Send controller events any time we are passed the given input threshold similarly to PC/Console (see: XInputInterface.cpp)
    const float RepeatDeadzone = 0.24f;
    bool bWasPositivePressed = false;
    bool bPositivePressed = false;
    bool bWasNegativePressed = false;
    bool bNegativePressed = false;
    float axisValue = 0;

    GCExtendedGamepad *ExtendedGamepad = Cont.extendedGamepad;
    GCExtendedGamepad *ExtendedPreviousGamepad = Controllers[ControllerIndex].PreviousExtendedGamepad;;

    GCMicroGamepad *MicroGamepad = Cont.microGamepad;
    GCMicroGamepad *MicroPreviousGamepad = Controllers[ControllerIndex].PreviousMicroGamepad;
    
    switch (Controllers[ControllerIndex].ControllerType)
    {
        case ControllerType::ExtendedGamepad:
        case ControllerType::DualShockGamepad:
        case ControllerType::XboxGamepad:
        case ControllerType::DualSenseGamepad:
            
            if (UEAxis == FGamepadKeyNames::LeftAnalogX){if ((ExtendedPreviousGamepad != nil && ExtendedGamepad.leftThumbstick.xAxis.value != ExtendedPreviousGamepad.leftThumbstick.xAxis.value) ||
                                                             (ExtendedGamepad.leftThumbstick.xAxis.value < -RepeatDeadzone || ExtendedGamepad.leftThumbstick.xAxis.value > RepeatDeadzone)){axisValue = ExtendedGamepad.leftThumbstick.xAxis.value;}}
            else if (UEAxis == FGamepadKeyNames::LeftAnalogY){if ((ExtendedPreviousGamepad != nil && ExtendedGamepad.leftThumbstick.yAxis.value != ExtendedPreviousGamepad.leftThumbstick.yAxis.value) ||
                                                                  (ExtendedGamepad.leftThumbstick.yAxis.value < -RepeatDeadzone || ExtendedGamepad.leftThumbstick.yAxis.value > RepeatDeadzone)){axisValue = ExtendedGamepad.leftThumbstick.yAxis.value;}}
            else if (UEAxis == FGamepadKeyNames::RightAnalogX){if ((ExtendedPreviousGamepad != nil && ExtendedGamepad.rightThumbstick.xAxis.value != ExtendedPreviousGamepad.rightThumbstick.xAxis.value) ||
                                                                   (ExtendedGamepad.rightThumbstick.xAxis.value < -RepeatDeadzone || ExtendedGamepad.rightThumbstick.xAxis.value > RepeatDeadzone)){axisValue = ExtendedGamepad.rightThumbstick.xAxis.value;}}
            else if (UEAxis == FGamepadKeyNames::RightAnalogY){if ((ExtendedPreviousGamepad != nil && ExtendedGamepad.rightThumbstick.yAxis.value != ExtendedPreviousGamepad.rightThumbstick.yAxis.value) ||
                                                                   (ExtendedGamepad.rightThumbstick.yAxis.value < -RepeatDeadzone || ExtendedGamepad.rightThumbstick.yAxis.value > RepeatDeadzone)){axisValue = ExtendedGamepad.rightThumbstick.yAxis.value;}}
            else if (UEAxis == FGamepadKeyNames::LeftTriggerAnalog){if ((ExtendedPreviousGamepad != nil && ExtendedGamepad.leftTrigger.value != ExtendedPreviousGamepad.leftTrigger.value) ||
                                                                        (ExtendedGamepad.leftTrigger.value < -RepeatDeadzone || ExtendedGamepad.leftTrigger.value > RepeatDeadzone)){axisValue = ExtendedGamepad.leftTrigger.value;}}
            else if (UEAxis == FGamepadKeyNames::RightTriggerAnalog){if ((ExtendedPreviousGamepad != nil && ExtendedGamepad.rightTrigger.value != ExtendedPreviousGamepad.rightTrigger.value) ||
                                                                         (ExtendedGamepad.rightTrigger.value < -RepeatDeadzone || ExtendedGamepad.rightTrigger.value > RepeatDeadzone)){axisValue = ExtendedGamepad.rightTrigger.value;}}
            break;
            
        case ControllerType::SiriRemote:
            
            if (UEAxis == FGamepadKeyNames::LeftAnalogX){if ((ExtendedPreviousGamepad != nil && MicroGamepad.dpad.xAxis.value != ExtendedPreviousGamepad.dpad.xAxis.value) ||
                                                             (MicroGamepad.dpad.xAxis.value < -RepeatDeadzone || MicroGamepad.dpad.xAxis.value > RepeatDeadzone)){axisValue = MicroGamepad.dpad.xAxis.value;}}
            else if (UEAxis == FGamepadKeyNames::LeftAnalogY){if ((MicroPreviousGamepad != nil && MicroGamepad.dpad.yAxis.value != MicroPreviousGamepad.dpad.yAxis.value) ||
                                                                  (MicroGamepad.dpad.yAxis.value < -RepeatDeadzone || MicroGamepad.dpad.yAxis.value > RepeatDeadzone)){axisValue = MicroGamepad.dpad.yAxis.value;}}
            break;
    }
#if APPLE_CONTROLLER_DEBUG
    NSLog(@"Axis %s is %f", TCHAR_TO_ANSI(*UEAxis.ToString()), axisValue);
#endif
    MessageHandler->OnControllerAnalog(UEAxis, UserId, DeviceId, axisValue);
}
