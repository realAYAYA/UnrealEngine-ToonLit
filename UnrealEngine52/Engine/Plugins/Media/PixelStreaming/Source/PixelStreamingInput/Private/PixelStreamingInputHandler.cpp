// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingInputHandler.h"
#include "InputStructures.h"
#include "PixelStreamingInputProtocol.h"
#include "JavaScriptKeyCodes.inl"
#include "Settings.h"
#include "Layout/ArrangedChildren.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Utils.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateUser.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Misc/CoreMiscDefines.h"
#include "Input/HittestGrid.h"
#include "PixelStreamingHMD.h"
#include "IPixelStreamingHMDModule.h"
#include "PixelStreamingInputEnums.h"
#include "PixelStreamingInputConversion.h"
#include "Widgets/Input/SEditableText.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "PixelStreamingInputDevice.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPixelStreamingInputHandler, Log, VeryVerbose);
DEFINE_LOG_CATEGORY(LogPixelStreamingInputHandler);

// TODO: Gesture recognition is moving to the browser, so add handlers for the gesture events.
// The gestures supported will be swipe, pinch,

namespace UE::PixelStreamingInput
{
	typedef EPixelStreamingInputAction Action;
	typedef EPixelStreamingXRSystem XRSystem;

	FPixelStreamingInputHandler::FPixelStreamingInputHandler(TSharedPtr<FPixelStreamingApplicationWrapper> InApplicationWrapper, const TSharedPtr<FGenericApplicationMessageHandler>& InTargetHandler)
		: TargetViewport(nullptr)
		, NumActiveTouches(0)
		, bIsMouseActive(false)
		, MessageHandler(InTargetHandler)
		, PixelStreamerApplicationWrapper(InApplicationWrapper)
		, FocusedPos(FVector2D(-1.0f, -1.0f))
		, UnfocusedPos(FVector2D(-1.0f, -1.0f))
	{
		// Register this input handler as an IMotionController. The module handles the registering as an IInputDevice
		IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);

		RegisterMessageHandler("KeyPress", [this](FMemoryReader Ar) { HandleOnKeyChar(Ar); });
		RegisterMessageHandler("KeyUp", [this](FMemoryReader Ar) { HandleOnKeyUp(Ar); });
		RegisterMessageHandler("KeyDown", [this](FMemoryReader Ar) { HandleOnKeyDown(Ar); });

		RegisterMessageHandler("TouchStart", [this](FMemoryReader Ar) { HandleOnTouchStarted(Ar); });
		RegisterMessageHandler("TouchMove", [this](FMemoryReader Ar) { HandleOnTouchMoved(Ar); });
		RegisterMessageHandler("TouchEnd", [this](FMemoryReader Ar) { HandleOnTouchEnded(Ar); });

		RegisterMessageHandler("GamepadConnected", [this](FMemoryReader Ar) { HandleOnControllerConnected(Ar); });
		RegisterMessageHandler("GamepadAnalog", [this](FMemoryReader Ar) { HandleOnControllerAnalog(Ar); });
		RegisterMessageHandler("GamepadButtonPressed", [this](FMemoryReader Ar) { HandleOnControllerButtonPressed(Ar); });
		RegisterMessageHandler("GamepadButtonReleased", [this](FMemoryReader Ar) { HandleOnControllerButtonReleased(Ar); });
		RegisterMessageHandler("GamepadDisconnected", [this](FMemoryReader Ar) { HandleOnControllerDisconnected(Ar); });

		RegisterMessageHandler("MouseEnter", [this](FMemoryReader Ar) { HandleOnMouseEnter(Ar); });
		RegisterMessageHandler("MouseLeave", [this](FMemoryReader Ar) { HandleOnMouseLeave(Ar); });
		RegisterMessageHandler("MouseUp", [this](FMemoryReader Ar) { HandleOnMouseUp(Ar); });
		RegisterMessageHandler("MouseDown", [this](FMemoryReader Ar) { HandleOnMouseDown(Ar); });
		RegisterMessageHandler("MouseMove", [this](FMemoryReader Ar) { HandleOnMouseMove(Ar); });
		RegisterMessageHandler("MouseWheel", [this](FMemoryReader Ar) { HandleOnMouseWheel(Ar); });
		RegisterMessageHandler("MouseDouble", [this](FMemoryReader Ar) { HandleOnMouseDoubleClick(Ar); });

		RegisterMessageHandler("XRHMDTransform", [this](FMemoryReader Ar) { HandleOnXRHMDTransform(Ar); });
		RegisterMessageHandler("XRControllerTransform", [this](FMemoryReader Ar) { HandleOnXRControllerTransform(Ar); });
		RegisterMessageHandler("XRButtonPressed", [this](FMemoryReader Ar) { HandleOnXRButtonPressed(Ar); });
		RegisterMessageHandler("XRButtonTouched", [this](FMemoryReader Ar) { HandleOnXRButtonTouched(Ar); });
		RegisterMessageHandler("XRButtonReleased", [this](FMemoryReader Ar) { HandleOnXRButtonReleased(Ar); });
		RegisterMessageHandler("XRAnalog", [this](FMemoryReader Ar) { HandleOnXRAnalog(Ar); });
		RegisterMessageHandler("XRSystem", [this](FMemoryReader Ar) { HandleOnXRSystem(Ar); });

		RegisterMessageHandler("Command", [this](FMemoryReader Ar) { HandleOnCommand(Ar); });
		RegisterMessageHandler("UIInteraction", [this](FMemoryReader Ar) { HandleUIInteraction(Ar); });
		RegisterMessageHandler("TextboxEntry", [this](FMemoryReader Ar) { HandleOnTextboxEntry(Ar); });

		// Populate map
		// Button indices found in: https://github.com/immersive-web/webxr-input-profiles/tree/master/packages/registry/profiles
		// HTC Vive - Left Hand
		// Buttons
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Left, 0, Action::Click), EKeys::Vive_Left_Trigger_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Left, 0, Action::Axis), EKeys::Vive_Left_Trigger_Axis);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Left, 1, Action::Click), EKeys::Vive_Left_Grip_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Left, 2, Action::Click), EKeys::Vive_Left_Trackpad_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Left, 2, Action::Touch), EKeys::Vive_Left_Trackpad_Touch);
		// Axes
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Left, 0, Action::X), EKeys::Vive_Left_Trackpad_X);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Left, 1, Action::Y), EKeys::Vive_Left_Trackpad_Y);
		// HTC Vive - Right Hand
		// Buttons
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Right, 0, Action::Click), EKeys::Vive_Right_Trigger_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Right, 0, Action::Axis), EKeys::Vive_Right_Trigger_Axis);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Right, 1, Action::Click), EKeys::Vive_Right_Grip_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Right, 2, Action::Click), EKeys::Vive_Right_Trackpad_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Right, 2, Action::Touch), EKeys::Vive_Right_Trackpad_Touch);
		// Axes
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Right, 0, Action::X), EKeys::Vive_Right_Trackpad_X);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::HTCVive, EControllerHand::Right, 1, Action::Y), EKeys::Vive_Right_Trackpad_Y);

		// Quest - Left Hand
		// Buttons
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 0, Action::Click), EKeys::OculusTouch_Left_Trigger_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 0, Action::Axis), EKeys::OculusTouch_Left_Trigger_Axis);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 0, Action::Touch), EKeys::OculusTouch_Left_Trigger_Touch);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 1, Action::Click), EKeys::OculusTouch_Left_Grip_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 1, Action::Axis), EKeys::OculusTouch_Left_Grip_Axis);
		// Index 1 (grip) touch not supported in UE
		// Index 2 not supported by WebXR
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 3, Action::Click), EKeys::OculusTouch_Left_Thumbstick_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 3, Action::Touch), EKeys::OculusTouch_Left_Thumbstick_Touch);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 4, Action::Click), EKeys::OculusTouch_Left_X_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 4, Action::Touch), EKeys::OculusTouch_Left_X_Touch);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 5, Action::Click), EKeys::OculusTouch_Left_Y_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 5, Action::Touch), EKeys::OculusTouch_Left_Y_Touch);
		// Index 6 (thumbrest) not supported in UE

		// Axes
		// Indices 0 and 1 not supported in WebXR
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 2, Action::X), EKeys::OculusTouch_Left_Thumbstick_X);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Left, 3, Action::Y), EKeys::OculusTouch_Left_Thumbstick_Y);

		// Quest - Right Hand
		// Buttons
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 0, Action::Click), EKeys::OculusTouch_Right_Trigger_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 0, Action::Axis), EKeys::OculusTouch_Right_Trigger_Axis);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 0, Action::Touch), EKeys::OculusTouch_Right_Trigger_Touch);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 1, Action::Click), EKeys::OculusTouch_Right_Grip_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 1, Action::Axis), EKeys::OculusTouch_Right_Grip_Axis);
		// Index 1 (grip) touch not supported in UE
		// Index 2 not supported by WebXR
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 3, Action::Click), EKeys::OculusTouch_Right_Thumbstick_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 3, Action::Touch), EKeys::OculusTouch_Right_Thumbstick_Touch);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 4, Action::Click), EKeys::OculusTouch_Right_A_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 4, Action::Touch), EKeys::OculusTouch_Right_A_Touch);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 5, Action::Click), EKeys::OculusTouch_Right_B_Click);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 5, Action::Touch), EKeys::OculusTouch_Right_B_Touch);
		// Index 6 (thumbrest) not supported in UE

		// Axes
		// Indices 0 and 1 not supported in WebXR
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 2, Action::X), EKeys::OculusTouch_Right_Thumbstick_X);
		FPixelStreamingInputConverter::XRInputToFKey.Add(MakeTuple(XRSystem::Quest, EControllerHand::Right, 3, Action::Y), EKeys::OculusTouch_Right_Thumbstick_Y);

		// Gamepad Axes
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(1, Action::Axis), EKeys::Gamepad_LeftX);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(2, Action::Axis), EKeys::Gamepad_LeftY);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(3, Action::Axis), EKeys::Gamepad_RightX);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(4, Action::Axis), EKeys::Gamepad_RightY);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(5, Action::Axis), EKeys::Gamepad_LeftTriggerAxis);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(6, Action::Axis), EKeys::Gamepad_RightTriggerAxis);
		// Gamepad Buttons
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(0, Action::Click), EKeys::Gamepad_FaceButton_Bottom);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(1, Action::Click), EKeys::Gamepad_FaceButton_Right);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(2, Action::Click), EKeys::Gamepad_FaceButton_Left);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(3, Action::Click), EKeys::Gamepad_FaceButton_Top);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(4, Action::Click), EKeys::Gamepad_LeftShoulder);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(5, Action::Click), EKeys::Gamepad_RightShoulder);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(6, Action::Click), EKeys::Gamepad_LeftTrigger);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(7, Action::Click), EKeys::Gamepad_RightTrigger);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(8, Action::Click), EKeys::Gamepad_Special_Left);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(9, Action::Click), EKeys::Gamepad_Special_Right);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(10, Action::Click), EKeys::Gamepad_LeftThumbstick);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(11, Action::Click), EKeys::Gamepad_RightThumbstick);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(12, Action::Click), EKeys::Gamepad_DPad_Up);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(13, Action::Click), EKeys::Gamepad_DPad_Down);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(14, Action::Click), EKeys::Gamepad_DPad_Left);
		FPixelStreamingInputConverter::GamepadInputToFKey.Add(MakeTuple(15, Action::Click), EKeys::Gamepad_DPad_Right);

		PopulateDefaultCommandHandlers();
	}

	FPixelStreamingInputHandler::~FPixelStreamingInputHandler()
	{
	}

	void FPixelStreamingInputHandler::RegisterMessageHandler(const FString& MessageType, const TFunction<void(FMemoryReader)>& Handler)
	{
		DispatchTable.Add(FPixelStreamingInputProtocol::ToStreamerProtocol.Find(MessageType)->GetID(), Handler);
	}

	TFunction<void(FMemoryReader)> FPixelStreamingInputHandler::FindMessageHandler(const FString& MessageType)
	{
		return DispatchTable.FindRef(FPixelStreamingInputProtocol::ToStreamerProtocol.Find(MessageType)->GetID());
	}

	FName FPixelStreamingInputHandler::GetMotionControllerDeviceTypeName() const
	{
		return FName(TEXT("PixelStreamingXRController"));
	}

	bool FPixelStreamingInputHandler::GetControllerOrientationAndPosition(const int32 ControllerIndex, const FName MotionSource, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
	{
		if (FPixelStreamingHMD* HMD = IPixelStreamingHMDModule::Get().GetPixelStreamingHMD(); (HMD == nullptr || ControllerIndex == INDEX_NONE))
		{
			return false;
		}

		EControllerHand DeviceHand;
		if (GetHandEnumForSourceName(MotionSource, DeviceHand))
		{
			return GetControllerOrientationAndPosition(ControllerIndex, DeviceHand, OutOrientation, OutPosition, WorldToMetersScale);
		}
		return false;
	}

	bool FPixelStreamingInputHandler::GetControllerOrientationAndPosition(const int32 ControllerIndex, const EControllerHand DeviceHand, FRotator& OutOrientation, FVector& OutPosition, float WorldToMetersScale) const
	{
		if (FPixelStreamingHMD* HMD = IPixelStreamingHMDModule::Get().GetPixelStreamingHMD(); (HMD == nullptr || ControllerIndex == INDEX_NONE))
		{
			return false;
		}

		FPixelStreamingXRController Controller = XRControllers.FindRef(DeviceHand);
		OutOrientation = Controller.Transform.Rotator();
		OutPosition = Controller.Transform.GetTranslation();
		return true;
	}

	ETrackingStatus FPixelStreamingInputHandler::GetControllerTrackingStatus(const int32 ControllerIndex, const EControllerHand DeviceHand) const
	{
		const FPixelStreamingXRController* Controller = XRControllers.Find(DeviceHand);
		return (Controller != nullptr) ? ETrackingStatus::Tracked : ETrackingStatus::NotTracked;
	}

	void FPixelStreamingInputHandler::EnumerateSources(TArray<FMotionControllerSource>& SourcesOut) const
	{
		SourcesOut.Add(FName(TEXT("AnyHand")));
		SourcesOut.Add(FName(TEXT("Left")));
		SourcesOut.Add(FName(TEXT("Right")));
		SourcesOut.Add(FName(TEXT("LeftGrip")));
		SourcesOut.Add(FName(TEXT("RightGrip")));
		SourcesOut.Add(FName(TEXT("LeftAim")));
		SourcesOut.Add(FName(TEXT("RightAim")));
	}

	void FPixelStreamingInputHandler::Tick(const float InDeltaTime)
	{
		TouchIndicesProcessedThisFrame.Reset();

		FMessage Message;
		while (Messages.Dequeue(Message))
		{
			FMemoryReader Ar(Message.Data);
			(*Message.Handler)(Ar);
		}
		
		ProcessLatestAnalogInputFromThisTick();
		BroadcastActiveTouchMoveEvents();
	}

	void FPixelStreamingInputHandler::OnMessage(TArray<uint8> Buffer)
	{
		uint8 MessageType = Buffer[0];
		// Remove the message type. The remaining data in the buffer is now purely
		// the message data
		Buffer.RemoveAt(0);

		TFunction<void(FMemoryReader)>* Handler = DispatchTable.Find(MessageType);
		if (Handler != nullptr)
		{
			FMessage Message = {
				Handler, // The function to call
				Buffer	 // The message data
			};
			Messages.Enqueue(Message);
		}
		else
		{
			UE_LOG(LogPixelStreamingInputHandler, Warning, TEXT("No handler registered for message with id %d"), MessageType);
		}
	}

	void FPixelStreamingInputHandler::SetTargetWindow(TWeakPtr<SWindow> InWindow)
	{
		TargetWindow = InWindow;
		PixelStreamerApplicationWrapper->SetTargetWindow(InWindow);
	}

	TWeakPtr<SWindow> FPixelStreamingInputHandler::GetTargetWindow()
	{
		return TargetWindow;
	}

	void FPixelStreamingInputHandler::SetTargetScreenSize(TWeakPtr<FIntPoint> InScreenSize)
	{
		TargetScreenSize = InScreenSize;
	}

	TWeakPtr<FIntPoint> FPixelStreamingInputHandler::GetTargetScreenSize()
	{
		return TargetScreenSize;
	}

	void FPixelStreamingInputHandler::SetTargetScreenRect(TWeakPtr<FIntRect> InScreenRect)
	{
		TargetScreenRect = InScreenRect;
	}

	TWeakPtr<FIntRect> FPixelStreamingInputHandler::GetTargetScreenRect()
	{
		return TargetScreenRect;
	}

	void FPixelStreamingInputHandler::SetTargetViewport(TWeakPtr<SViewport> InViewport)
	{
		TargetViewport = InViewport;
	}

	TWeakPtr<SViewport> FPixelStreamingInputHandler::GetTargetViewport()
	{
		return TargetViewport;
	}

	void FPixelStreamingInputHandler::SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InTargetHandler)
	{
		MessageHandler = InTargetHandler;
	}

	bool FPixelStreamingInputHandler::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
	{
		return GEngine->Exec(InWorld, Cmd, Ar);
	}

	void FPixelStreamingInputHandler::SetChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
	{
		// TODO: Implement FFB
	}

	void FPixelStreamingInputHandler::SetChannelValues(int32 ControllerId, const FForceFeedbackValues& values)
	{
		// TODO: Implement FFB
	}

	void FPixelStreamingInputHandler::HandleOnKeyChar(FMemoryReader Ar)
	{
		TPayloadOneParam<TCHAR> Payload(Ar);
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("KEY_PRESSED: Character = '%c'"), Payload.Param1);
		// A key char event is never repeated, so set it to false. It's value
		// ultimately doesn't matter as this paramater isn't used later
		MessageHandler->OnKeyChar(Payload.Param1, false);
	}

	void FPixelStreamingInputHandler::HandleOnKeyDown(FMemoryReader Ar)
	{
		TPayloadTwoParam<uint8, uint8> Payload(Ar);

		bool bIsRepeat = Payload.Param2 != 0;
		const FKey* AgnosticKey = JavaScriptKeyCodeToFKey[Payload.Param1];
		if (FilterKey(*AgnosticKey))
		{
			const uint32* KeyPtr;
			const uint32* CharacterPtr;
			FInputKeyManager::Get().GetCodesFromKey(*AgnosticKey, KeyPtr, CharacterPtr);
			uint32 Key = KeyPtr ? *KeyPtr : 0;
			uint32 Character = CharacterPtr ? *CharacterPtr : 0;

			UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("KEY_DOWN: Key = %d; Character = %d; IsRepeat = %s"), Key, Character, bIsRepeat ? TEXT("True") : TEXT("False"));
			MessageHandler->OnKeyDown((int32)Key, (int32)Character, bIsRepeat);
		}
	}

	void FPixelStreamingInputHandler::HandleOnKeyUp(FMemoryReader Ar)
	{
		TPayloadOneParam<uint8> Payload(Ar);
		const FKey* AgnosticKey = JavaScriptKeyCodeToFKey[Payload.Param1];
		if (FilterKey(*AgnosticKey))
		{
			const uint32* KeyPtr;
			const uint32* CharacterPtr;
			FInputKeyManager::Get().GetCodesFromKey(*AgnosticKey, KeyPtr, CharacterPtr);
			uint32 Key = KeyPtr ? *KeyPtr : 0;
			uint32 Character = CharacterPtr ? *CharacterPtr : 0;

			UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("KEY_UP: Key = %d; Character = %d"), Key, Character);
			MessageHandler->OnKeyUp((int32)Key, (int32)Character, false);
		}
	}

	void FPixelStreamingInputHandler::HandleOnTouchStarted(FMemoryReader Ar)
	{
		TPayloadOneParam<uint8> Payload(Ar);

		uint8 NumTouches = Payload.Param1;
		for (uint8 TouchIdx = 0; TouchIdx < NumTouches; TouchIdx++)
		{
			//                 PosX    PoxY    IDX   Force  Valid
			TPayloadFiveParam<uint16, uint16, uint8, uint8, uint8> Touch(Ar);
			// If Touch is valid
			if (Touch.Param5 != 0)
			{
				//                                                                           convert range from 0,65536 -> 0,1
				FVector2D TouchLocation = ConvertFromNormalizedScreenLocation(FVector2D(Touch.Param1 / uint16_MAX, Touch.Param2 / uint16_MAX));
				const int32 TouchIndex = Touch.Param3;
				const float TouchForce = Touch.Param4 / 255.0f;
				UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("TOUCH_START: TouchIndex = %d; Pos = (%d, %d); CursorPos = (%d, %d); Force = %.3f"), TouchIndex, Touch.Param1, Touch.Param2, static_cast<int>(TouchLocation.X), static_cast<int>(TouchLocation.Y), TouchForce);

				if (InputType == EPixelStreamingInputType::RouteToWidget)
				{
					// TouchLocation = TouchLocation - TargetViewport.Pin()->GetCachedGeometry().GetAbsolutePosition();
					FWidgetPath WidgetPath = FindRoutingMessageWidget(TouchLocation);

					if (WidgetPath.IsValid())
					{
						FScopedSwitchWorldHack SwitchWorld(WidgetPath);
						FPointerEvent PointerEvent(0, TouchIndex, TouchLocation, TouchLocation, TouchForce, true);
						FSlateApplication::Get().RoutePointerDownEvent(WidgetPath, PointerEvent);
					}
				}
				else if (InputType == EPixelStreamingInputType::RouteToWindow)
				{
					if (NumActiveTouches == 0 && !bIsMouseActive)
					{
						FSlateApplication::Get().OnCursorSet();
						// Make sure the application is active.
						FSlateApplication::Get().ProcessApplicationActivationEvent(true);

						FVector2D OldCursorLocation = PixelStreamerApplicationWrapper->WrappedApplication->Cursor->GetPosition();
						PixelStreamerApplicationWrapper->Cursor->SetPosition(OldCursorLocation.X, OldCursorLocation.Y);
						FSlateApplication::Get().OverridePlatformApplication(PixelStreamerApplicationWrapper);
					}

					// We must update the user cursor position explicitly before updating the application cursor position
					// as if there's a delta between them, when the touch event is started it will trigger a move
					// resulting in a large 'drag' across the screen
					TSharedPtr<FSlateUser> User = FSlateApplication::Get().GetCursorUser();
					User->SetCursorPosition(TouchLocation);
					PixelStreamerApplicationWrapper->Cursor->SetPosition(TouchLocation.X, TouchLocation.Y);
					PixelStreamerApplicationWrapper->WrappedApplication->Cursor->SetPosition(TouchLocation.X, TouchLocation.Y);

					MessageHandler->OnTouchStarted(PixelStreamerApplicationWrapper->GetWindowUnderCursor(), TouchLocation, TouchForce, TouchIndex, 0); // TODO: ControllerId?
				}

				NumActiveTouches++;
			}
		}

		FindFocusedWidget();
	}

	void FPixelStreamingInputHandler::HandleOnTouchMoved(FMemoryReader Ar)
	{
		TPayloadOneParam<uint8> Payload(Ar);

		uint8 NumTouches = Payload.Param1;
		for (uint8 TouchIdx = 0; TouchIdx < NumTouches; TouchIdx++)
		{
			//                 PosX    PoxY    IDX   Force  Valid
			TPayloadFiveParam<uint16, uint16, uint8, uint8, uint8> Touch(Ar);
			// If Touch is valid
			if (Touch.Param5 != 0)
			{
				//                                                                           convert range from 0,65536 -> 0,1
				FVector2D TouchLocation = ConvertFromNormalizedScreenLocation(FVector2D(Touch.Param1 / uint16_MAX, Touch.Param2 / uint16_MAX));
				const int32 TouchIndex = Touch.Param3;
				const float TouchForce = Touch.Param4 / 255.0f;
				UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("TOUCH_MOVE: TouchIndex = %d; Pos = (%d, %d); CursorPos = (%d, %d); Force = %.3f"), TouchIndex, Touch.Param1, Touch.Param2, static_cast<int>(TouchLocation.X), static_cast<int>(TouchLocation.Y), TouchForce);

				FCachedTouchEvent& TouchEvent = CachedTouchEvents.FindOrAdd(TouchIndex);
				TouchEvent.Force = TouchForce;
				TouchEvent.ControllerIndex = 0;

				if (InputType == EPixelStreamingInputType::RouteToWidget)
				{
					// TouchLocation = TouchLocation - TargetViewport.Pin()->GetCachedGeometry().GetAbsolutePosition();
					TouchEvent.Location = TouchLocation;
					FWidgetPath WidgetPath = FindRoutingMessageWidget(TouchLocation);

					if (WidgetPath.IsValid())
					{
						FScopedSwitchWorldHack SwitchWorld(WidgetPath);
						FPointerEvent PointerEvent(0, TouchIndex, TouchLocation, LastTouchLocation, TouchForce, true);
						FSlateApplication::Get().RoutePointerMoveEvent(WidgetPath, PointerEvent, false);
					}

					LastTouchLocation = TouchLocation;
				}
				else if (InputType == EPixelStreamingInputType::RouteToWindow)
				{
					TouchEvent.Location = TouchLocation;
					MessageHandler->OnTouchMoved(TouchEvent.Location, TouchEvent.Force, TouchIndex, TouchEvent.ControllerIndex); // TODO: ControllerId?
				}
				TouchIndicesProcessedThisFrame.Add(TouchIndex);
			}
		}
	}

	void FPixelStreamingInputHandler::HandleOnTouchEnded(FMemoryReader Ar)
	{
		TPayloadOneParam<uint8> Payload(Ar);
		uint8 NumTouches = Payload.Param1;
		for (uint8 TouchIdx = 0; TouchIdx < NumTouches; TouchIdx++)
		{
			//                 PosX    PoxY    IDX   Force  Valid
			TPayloadFiveParam<uint16, uint16, uint8, uint8, uint8> Touch(Ar);
			// Always allowing the "up" events regardless of in or outside the valid region so
			// states aren't stuck "down". Might want to uncomment this if it causes other issues.
			// if(Touch.Param5 != 0)
			{
				//                                                                           convert range from 0,65536 -> 0,1
				FVector2D TouchLocation = ConvertFromNormalizedScreenLocation(FVector2D(Touch.Param1 / uint16_MAX, Touch.Param2 / uint16_MAX));
				const int32 TouchIndex = Touch.Param3;
				UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("TOUCH_END: TouchIndex = %d; Pos = (%d, %d); CursorPos = (%d, %d)"), Touch.Param3, Touch.Param1, Touch.Param2, static_cast<int>(TouchLocation.X), static_cast<int>(TouchLocation.Y));

				if (InputType == EPixelStreamingInputType::RouteToWidget)
				{
					// TouchLocation = TouchLocation - TargetViewport.Pin()->GetCachedGeometry().GetAbsolutePosition();
					FWidgetPath WidgetPath = FindRoutingMessageWidget(TouchLocation);

					if (WidgetPath.IsValid())
					{
						FScopedSwitchWorldHack SwitchWorld(WidgetPath);
						float TouchForce = 0.0f;
						FPointerEvent PointerEvent(0, TouchIndex, TouchLocation, TouchLocation, TouchForce, true);
						FSlateApplication::Get().RoutePointerUpEvent(WidgetPath, PointerEvent);
					}
				}
				else if (InputType == EPixelStreamingInputType::RouteToWindow)
				{
					MessageHandler->OnTouchEnded(TouchLocation, TouchIndex, 0); // TODO: ControllerId?
				}

				CachedTouchEvents.Remove(TouchIndex);
				NumActiveTouches = (NumActiveTouches > 0) ? NumActiveTouches - 1 : NumActiveTouches;
			}
		}

		// If there's no remaining touches, and there is also no mouse over the player window
		// then set the platform application back to its default. We need to set it back to default
		// so that people using the editor (if editor streaming) can click on buttons outside the target window
		// and also have the correct cursor (pixel streaming forces default cursor)
		if (NumActiveTouches == 0 && !bIsMouseActive && InputType == EPixelStreamingInputType::RouteToWindow)
		{
			FVector2D OldCursorLocation = PixelStreamerApplicationWrapper->Cursor->GetPosition();
			PixelStreamerApplicationWrapper->WrappedApplication->Cursor->SetPosition(OldCursorLocation.X, OldCursorLocation.Y);
			FSlateApplication::Get().OverridePlatformApplication(PixelStreamerApplicationWrapper->WrappedApplication);
		}
	}

	void FPixelStreamingInputHandler::HandleOnControllerConnected(FMemoryReader Ar)
	{
		TSharedPtr<FPixelStreamingInputDevice> InputDevice = FPixelStreamingInputDevice::GetInputDevice();
		uint8 NextControllerId = InputDevice->OnControllerConnected();

		FString Descriptor = FString::Printf(TEXT("{ \"controllerId\": %d }"), NextControllerId);

		FBufferArchive Buffer;
		Buffer << Descriptor;
		TArray<uint8> Data(Buffer.GetData(), Buffer.Num());
		// Specific implementation for this method is handled per streamer
		OnSendMessage.Broadcast("GamepadResponse", FMemoryReader(Data));

		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("GAMEPAD_CONNECTED: ControllerId = %d"), NextControllerId);
	}

	void FPixelStreamingInputHandler::HandleOnControllerAnalog(FMemoryReader Ar)
	{
		const TPayloadThreeParam<uint8, uint8, double> Payload(Ar);
		const FInputDeviceId ControllerId = FInputDeviceId::CreateFromInternalId((int32)Payload.Param1);
		// Overwrite the last data: every tick only process the latest
		AnalogEventsReceivedThisTick.FindOrAdd(ControllerId).FindOrAdd(Payload.Param2) = Payload.Param3;
	}

	void FPixelStreamingInputHandler::HandleOnControllerButtonPressed(FMemoryReader Ar)
	{
		TPayloadThreeParam<uint8, uint8, uint8> Payload(Ar);
		FKey* ButtonPtr = FPixelStreamingInputConverter::GamepadInputToFKey.Find(MakeTuple(Payload.Param2, Action::Click));
		if (ButtonPtr == nullptr)
		{
			return;
		}

		FInputDeviceId ControllerId = FInputDeviceId::CreateFromInternalId((int32)Payload.Param1);
		bool bIsRepeat = Payload.Param3 != 0;
		FPlatformUserId UserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();

		FKeyEvent KeyEvent(
			*ButtonPtr,															  /* InKey */
			FSlateApplication::Get().GetPlatformApplication()->GetModifierKeys(), /* InModifierKeys */
			ControllerId,														  /* InDeviceId */
			bIsRepeat,															  /* bInIsRepeat */
			0,																	  /* InCharacterCode */
			0,																	  /* InKeyCode */
			// TODO (william.belcher): This user idx should be the playerId
			0 /* InUserIndex*/
		);
		FSlateApplication::Get().ProcessKeyDownEvent(KeyEvent);

		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("GAMEPAD_PRESSED: ControllerId = %d; KeyName = %s; IsRepeat = %s;"), ControllerId.GetId(), *ButtonPtr->ToString(), bIsRepeat ? TEXT("True") : TEXT("False"));
	}

	void FPixelStreamingInputHandler::HandleOnControllerButtonReleased(FMemoryReader Ar)
	{
		TPayloadTwoParam<uint8, uint8> Payload(Ar);
		FKey* ButtonPtr = FPixelStreamingInputConverter::GamepadInputToFKey.Find(MakeTuple(Payload.Param2, Action::Click));
		if (ButtonPtr == nullptr)
		{
			return;
		}

		FInputDeviceId ControllerId = FInputDeviceId::CreateFromInternalId((int32)Payload.Param1);
		FPlatformUserId UserId = IPlatformInputDeviceMapper::Get().GetPrimaryPlatformUser();

		FKeyEvent KeyEvent(
			*ButtonPtr,															  /* InKey */
			FSlateApplication::Get().GetPlatformApplication()->GetModifierKeys(), /* InModifierKeys */
			ControllerId,														  /* InDeviceId */
			false,																  /* bInIsRepeat */
			0,																	  /* InCharacterCode */
			0,																	  /* InKeyCode */
			// TODO (william.belcher): This user idx should be the playerId
			0 /* InUserIndex*/
		);
		FSlateApplication::Get().ProcessKeyUpEvent(KeyEvent);

		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("GAMEPAD_RELEASED: ControllerId = %d; KeyName = %s;"), ControllerId.GetId(), *ButtonPtr->ToString());
	}

	void FPixelStreamingInputHandler::HandleOnControllerDisconnected(FMemoryReader Ar)
	{
		TPayloadOneParam<uint8> Payload(Ar);

		uint8 DisconnectedControllerId = Payload.Param1;

		TSharedPtr<FPixelStreamingInputDevice> InputDevice = FPixelStreamingInputDevice::GetInputDevice();
		InputDevice->OnControllerDisconnected(DisconnectedControllerId);

		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("GAMEPAD_DISCONNECTED: ControllerId = %d"), DisconnectedControllerId);
	}

	/**
	 * Mouse events
	 */
	void FPixelStreamingInputHandler::HandleOnMouseEnter(FMemoryReader Ar)
	{
		if (NumActiveTouches == 0 && !bIsMouseActive)
		{
			FSlateApplication::Get().OnCursorSet();
			FSlateApplication::Get().OverridePlatformApplication(PixelStreamerApplicationWrapper);
			// Make sure the application is active.
			FSlateApplication::Get().ProcessApplicationActivationEvent(true);
		}

		bIsMouseActive = true;
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("MOUSE_ENTER"));
	}

	void FPixelStreamingInputHandler::HandleOnMouseLeave(FMemoryReader Ar)
	{
		if (NumActiveTouches == 0)
		{
			// Restore normal application layer if there is no active touches and MouseEnter hasn't been triggered
			FSlateApplication::Get().OverridePlatformApplication(PixelStreamerApplicationWrapper->WrappedApplication);
		}
		bIsMouseActive = false;
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("MOUSE_LEAVE"));
	}

	void FPixelStreamingInputHandler::HandleOnMouseUp(FMemoryReader Ar)
	{
		// Ensure we have wrapped the slate application at this point
		if (!bIsMouseActive)
		{
			HandleOnMouseEnter(Ar);
		}

		TPayloadThreeParam<uint8, uint16, uint16> Payload(Ar);

		EMouseButtons::Type Button = static_cast<EMouseButtons::Type>(Payload.Param1);
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("MOUSE_UP: Button = %d"), Button);

		if (InputType == EPixelStreamingInputType::RouteToWidget)
		{
			FSlateApplication& SlateApplication = FSlateApplication::Get();
			FWidgetPath WidgetPath = FindRoutingMessageWidget(SlateApplication.GetCursorPos());

			if (WidgetPath.IsValid())
			{
				FScopedSwitchWorldHack SwitchWorld(WidgetPath);
				FKey Key = TranslateMouseButtonToKey(Button);

				FPointerEvent MouseEvent(
					SlateApplication.GetUserIndexForMouse(),
					FSlateApplicationBase::CursorPointerIndex,
					SlateApplication.GetCursorPos(),
					SlateApplication.GetLastCursorPos(),
					SlateApplication.GetPressedMouseButtons(),
					Key,
					0,
					SlateApplication.GetPlatformApplication()->GetModifierKeys());

				SlateApplication.RoutePointerUpEvent(WidgetPath, MouseEvent);
			}
		}
		else if (InputType == EPixelStreamingInputType::RouteToWindow)
		{
			if (Button != EMouseButtons::Type::Invalid)
			{
				MessageHandler->OnMouseUp(Button);
			}
		}
	}

	void FPixelStreamingInputHandler::HandleOnMouseDown(FMemoryReader Ar)
	{
		// Ensure we have wrapped the slate application at this point
		if (!bIsMouseActive)
		{
			HandleOnMouseEnter(Ar);
		}

		TPayloadThreeParam<uint8, uint16, uint16> Payload(Ar);
		//                                                                           convert range from 0,65536 -> 0,1
		FVector2D ScreenLocation = ConvertFromNormalizedScreenLocation(FVector2D(Payload.Param2 / uint16_MAX, Payload.Param3 / uint16_MAX));
		EMouseButtons::Type Button = static_cast<EMouseButtons::Type>(Payload.Param1);
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("MOUSE_DOWN: Button = %d; Pos = (%.4f, %.4f)"), Button, ScreenLocation.X, ScreenLocation.Y);
		// Set cursor pos on mouse down - we may not have moved if this is the very first click
		FSlateApplication& SlateApplication = FSlateApplication::Get();
		SlateApplication.OnCursorSet();
		// Force window focus
		SlateApplication.ProcessApplicationActivationEvent(true);

		if (InputType == EPixelStreamingInputType::RouteToWidget)
		{
			FWidgetPath WidgetPath = FindRoutingMessageWidget(ScreenLocation);

			if (WidgetPath.IsValid())
			{
				FScopedSwitchWorldHack SwitchWorld(WidgetPath);

				FKey Key = TranslateMouseButtonToKey(Button);

				FPointerEvent MouseEvent(
					SlateApplication.GetUserIndexForMouse(),
					FSlateApplicationBase::CursorPointerIndex,
					ScreenLocation,
					SlateApplication.GetLastCursorPos(),
					SlateApplication.GetPressedMouseButtons(),
					Key,
					0,
					SlateApplication.GetPlatformApplication()->GetModifierKeys());

				SlateApplication.RoutePointerDownEvent(WidgetPath, MouseEvent);
			}
		}
		else if (InputType == EPixelStreamingInputType::RouteToWindow)
		{
			MessageHandler->OnMouseDown(PixelStreamerApplicationWrapper->GetWindowUnderCursor(), Button, ScreenLocation);
		}
	}

	void FPixelStreamingInputHandler::HandleOnMouseMove(FMemoryReader Ar)
	{
		TPayloadFourParam<uint16, uint16, int16, int16> Payload(Ar);
		//                                                                           convert range from 0,65536 -> 0,1
		FIntPoint ScreenLocation = ConvertFromNormalizedScreenLocation(FVector2D(Payload.Param1 / uint16_MAX, Payload.Param2 / uint16_MAX));
		//                                                                 convert range from -32,768 to 32,767 -> -1,1
		FIntPoint Delta = ConvertFromNormalizedScreenLocation(FVector2D(Payload.Param3 / int16_MAX, Payload.Param4 / int16_MAX), false);
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("MOUSE_MOVE: Pos = (%d, %d); Delta = (%d, %d)"), ScreenLocation.X, ScreenLocation.Y, Delta.X, Delta.Y);
		FSlateApplication& SlateApplication = FSlateApplication::Get();
		SlateApplication.OnCursorSet();
		PixelStreamerApplicationWrapper->Cursor->SetPosition(ScreenLocation.X, ScreenLocation.Y);

		if (InputType == EPixelStreamingInputType::RouteToWidget)
		{
			FWidgetPath WidgetPath = FindRoutingMessageWidget(ScreenLocation);

			if (WidgetPath.IsValid())
			{
				FScopedSwitchWorldHack SwitchWorld(WidgetPath);

				FPointerEvent MouseEvent(
					SlateApplication.GetUserIndexForMouse(),
					FSlateApplicationBase::CursorPointerIndex,
					SlateApplication.GetCursorPos(),
					SlateApplication.GetLastCursorPos(),
					FVector2D(Delta.X, Delta.Y),
					SlateApplication.GetPressedMouseButtons(),
					SlateApplication.GetPlatformApplication()->GetModifierKeys());

				SlateApplication.RoutePointerMoveEvent(WidgetPath, MouseEvent, false);
			}
		}
		else if (InputType == EPixelStreamingInputType::RouteToWindow)
		{
			MessageHandler->OnRawMouseMove(Delta.X, Delta.Y);
		}
	}

	void FPixelStreamingInputHandler::HandleOnMouseWheel(FMemoryReader Ar)
	{
		TPayloadThreeParam<int16, uint16, uint16> Payload(Ar);
		//                                                                           convert range from 0,65536 -> 0,1
		FIntPoint ScreenLocation = ConvertFromNormalizedScreenLocation(FVector2D(Payload.Param2 / uint16_MAX, Payload.Param3 / uint16_MAX));
		const float SpinFactor = 1 / 120.0f;
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("MOUSE_WHEEL: Delta = %d; Pos = (%d, %d)"), Payload.Param1, ScreenLocation.X, ScreenLocation.Y);

		if (InputType == EPixelStreamingInputType::RouteToWidget)
		{
			FWidgetPath WidgetPath = FindRoutingMessageWidget(ScreenLocation);

			if (WidgetPath.IsValid())
			{
				FScopedSwitchWorldHack SwitchWorld(WidgetPath);

				FSlateApplication& SlateApplication = FSlateApplication::Get();

				FPointerEvent MouseEvent(
					SlateApplication.GetUserIndexForMouse(),
					FSlateApplicationBase::CursorPointerIndex,
					SlateApplication.GetCursorPos(),
					SlateApplication.GetCursorPos(),
					SlateApplication.GetPressedMouseButtons(),
					EKeys::Invalid,
					Payload.Param1 * SpinFactor,
					SlateApplication.GetPlatformApplication()->GetModifierKeys());

				SlateApplication.RouteMouseWheelOrGestureEvent(WidgetPath, MouseEvent, nullptr);
			}
		}
		else if (InputType == EPixelStreamingInputType::RouteToWindow)
		{
			MessageHandler->OnMouseWheel(Payload.Param1 * SpinFactor, ScreenLocation);
		}
	}

	void FPixelStreamingInputHandler::HandleOnMouseDoubleClick(FMemoryReader Ar)
	{
		TPayloadThreeParam<uint8, uint16, uint16> Payload(Ar);
		//                                                                           convert range from 0,65536 -> 0,1
		FVector2D ScreenLocation = ConvertFromNormalizedScreenLocation(FVector2D(Payload.Param2 / uint16_MAX, Payload.Param3 / uint16_MAX));
		EMouseButtons::Type Button = static_cast<EMouseButtons::Type>(Payload.Param1);
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("MOUSE_DOWN: Button = %d; Pos = (%.4f, %.4f)"), Button, ScreenLocation.X, ScreenLocation.Y);
		// Force window focus
		FSlateApplication& SlateApplication = FSlateApplication::Get();
		SlateApplication.ProcessApplicationActivationEvent(true);

		if (InputType == EPixelStreamingInputType::RouteToWidget)
		{
			FWidgetPath WidgetPath = FindRoutingMessageWidget(ScreenLocation);

			if (WidgetPath.IsValid())
			{
				FScopedSwitchWorldHack SwitchWorld(WidgetPath);
				FKey Key = TranslateMouseButtonToKey(Button);

				FPointerEvent MouseEvent(
					SlateApplication.GetUserIndexForMouse(),
					FSlateApplicationBase::CursorPointerIndex,
					SlateApplication.GetCursorPos(),
					SlateApplication.GetLastCursorPos(),
					SlateApplication.GetPressedMouseButtons(),
					Key,
					0,
					SlateApplication.GetPlatformApplication()->GetModifierKeys());

				SlateApplication.RoutePointerDoubleClickEvent(WidgetPath, MouseEvent);
			}
		}
		else if (InputType == EPixelStreamingInputType::RouteToWindow)
		{
			MessageHandler->OnMouseDoubleClick(PixelStreamerApplicationWrapper->GetWindowUnderCursor(), Button, ScreenLocation);
		}
	}

	/**
	 * XR Handling
	 */
	void FPixelStreamingInputHandler::HandleOnXRHMDTransform(FMemoryReader Ar)
	{
		// The buffer contains the transform matrix stored as 16 floats
		FMatrix HMDMatrix;
		for (int32 Row = 0; Row < 4; ++Row)

		{
			float Col0, Col1, Col2, Col3;
			Ar << Col0 << Col1 << Col2 << Col3;
			HMDMatrix.M[Row][0] = Col0;
			HMDMatrix.M[Row][1] = Col1;
			HMDMatrix.M[Row][2] = Col2;
			HMDMatrix.M[Row][3] = Col3;
		}
		HMDMatrix.DiagnosticCheckNaN();
		/**
		 * Converts the 'Y up' 'right handed' WebXR coordinate system transform to Unreal's 'Z up'
		 * 'left handed' coordinate system.
		 *
		 * Ignores scale.
		 *
		 * HMD Coordinate space (https://developer.mozilla.org/en-US/docs/Web/API/WebXR_Device_API/Geometry)
		 */
		// Rows and columns are swapped between raw mat and FMat
		FMatrix UEMatrix = FMatrix(
			FPlane(HMDMatrix.M[0][0], HMDMatrix.M[1][0], HMDMatrix.M[2][0], HMDMatrix.M[3][0]),
			FPlane(HMDMatrix.M[0][1], HMDMatrix.M[1][1], HMDMatrix.M[2][1], HMDMatrix.M[3][1]),
			FPlane(HMDMatrix.M[0][2], HMDMatrix.M[1][2], HMDMatrix.M[2][2], HMDMatrix.M[3][2]),
			FPlane(HMDMatrix.M[0][3], HMDMatrix.M[1][3], HMDMatrix.M[2][3], HMDMatrix.M[3][3]));
		// Extract & convert translation
		FVector Translation = FVector(-UEMatrix.M[3][2], UEMatrix.M[3][0], UEMatrix.M[3][1]) * 100.0f;
		// Extract & convert rotation
		FQuat RawRotation(UEMatrix);
		FQuat Rotation(-RawRotation.Z, RawRotation.X, RawRotation.Y, -RawRotation.W);

		if (FPixelStreamingHMD* HMD = IPixelStreamingHMDModule::Get().GetPixelStreamingHMD(); HMD != nullptr)
		{
			HMD->SetTransform(FTransform(Rotation, Translation, FVector(HMDMatrix.GetScaleVector(1.0f))));
		}
	}

	void FPixelStreamingInputHandler::HandleOnXRControllerTransform(FMemoryReader Ar)
	{
		// The buffer contains the transform matrix stored as 16 floats
		FMatrix ControllerMatrix;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			float Col0, Col1, Col2, Col3;
			Ar << Col0 << Col1 << Col2 << Col3;
			// Rows and columns are swapped between raw mat and FMat
			ControllerMatrix.M[Row][0] = Col0;
			ControllerMatrix.M[Row][1] = Col1;
			ControllerMatrix.M[Row][2] = Col2;
			ControllerMatrix.M[Row][3] = Col3;
		}
		ControllerMatrix.DiagnosticCheckNaN();
		FMatrix UEMatrix = FMatrix(
			FPlane(ControllerMatrix.M[0][0], ControllerMatrix.M[1][0], ControllerMatrix.M[2][0], ControllerMatrix.M[3][0]),
			FPlane(ControllerMatrix.M[0][1], ControllerMatrix.M[1][1], ControllerMatrix.M[2][1], ControllerMatrix.M[3][1]),
			FPlane(ControllerMatrix.M[0][2], ControllerMatrix.M[1][2], ControllerMatrix.M[2][2], ControllerMatrix.M[3][2]),
			FPlane(ControllerMatrix.M[0][3], ControllerMatrix.M[1][3], ControllerMatrix.M[2][3], ControllerMatrix.M[3][3]));
		// Extract & convert translation
		FVector Translation = FVector(-UEMatrix.M[3][2], UEMatrix.M[3][0], UEMatrix.M[3][1]) * 100.0f;
		// Extract & convert rotation
		FQuat RawRotation(UEMatrix);
		FQuat Rotation(-RawRotation.Z, RawRotation.X, RawRotation.Y, -RawRotation.W);
		EControllerHand Handedness;
		Ar << Handedness;
		FPixelStreamingXRController Controller;
		Controller.Transform = FTransform(Rotation, Translation, FVector(ControllerMatrix.GetScaleVector(1.0f)));
		Controller.Handedness = Handedness;
		XRControllers.Add(Handedness, Controller);
	}

	void FPixelStreamingInputHandler::HandleOnXRButtonTouched(FMemoryReader Ar)
	{
		TPayloadThreeParam<uint8, uint8, uint8> Payload(Ar);
		IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
		FInputDeviceId ControllerId = DeviceMapper.GetDefaultInputDevice();

		XRSystem System = IPixelStreamingHMDModule::Get().GetActiveXRSystem();
		EControllerHand Handedness = static_cast<EControllerHand>(Payload.Param1);
		uint8 ButtonIdx = Payload.Param2;
		bool bIsRepeat = Payload.Param3 != 0;

		FKey* ButtonPtr = FPixelStreamingInputConverter::XRInputToFKey.Find(MakeTuple(System, Handedness, ButtonIdx, Action::Touch));
		if (ButtonPtr == nullptr)
		{
			return;
		}

		MessageHandler->OnControllerButtonPressed(ButtonPtr->GetFName(), DeviceMapper.GetPrimaryPlatformUser(), ControllerId, bIsRepeat);
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("XR_TOUCHED: ControllerId = %d; KeyName = %s; IsRepeat = %s;"), ControllerId.GetId(), *ButtonPtr->ToString(), bIsRepeat ? TEXT("True") : TEXT("False"));
	}

	void FPixelStreamingInputHandler::HandleOnXRButtonPressed(FMemoryReader Ar)
	{
		TPayloadThreeParam<uint8, uint8, uint8> Payload(Ar);
		IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
		FInputDeviceId ControllerId = DeviceMapper.GetDefaultInputDevice();

		XRSystem System = IPixelStreamingHMDModule::Get().GetActiveXRSystem();
		EControllerHand Handedness = static_cast<EControllerHand>(Payload.Param1);
		uint8 ButtonIdx = Payload.Param2;
		bool bIsRepeat = Payload.Param3 != 0;

		FKey* ButtonPtr = FPixelStreamingInputConverter::XRInputToFKey.Find(MakeTuple(System, Handedness, ButtonIdx, Action::Click));
		if (ButtonPtr == nullptr)
		{
			return;
		}
		MessageHandler->OnControllerButtonPressed(ButtonPtr->GetFName(), DeviceMapper.GetPrimaryPlatformUser(), ControllerId, bIsRepeat);
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("XR_PRESSED: ControllerId = %d; KeyName = %s; IsRepeat = %s;"), ControllerId.GetId(), *ButtonPtr->ToString(), bIsRepeat ? TEXT("True") : TEXT("False"));

		// Try and see if there is an axis associated with this button (usually the case for triggers)
		// if so, trigger an axis movement with value 1.0
		FKey* AxisPtr = FPixelStreamingInputConverter::XRInputToFKey.Find(MakeTuple(System, Handedness, ButtonIdx, Action::Axis));
		if (AxisPtr == nullptr)
		{
			return;
		}
		float AnalogValue = 1.0f;
		MessageHandler->OnControllerAnalog(AxisPtr->GetFName(), DeviceMapper.GetPrimaryPlatformUser(), ControllerId, AnalogValue);
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("XR_ANALOG: ControllerId = %d; KeyName = %s; AnalogValue = %.4f;"), ControllerId.GetId(), *AxisPtr->ToString(), AnalogValue);
	}

	void FPixelStreamingInputHandler::HandleOnXRButtonReleased(FMemoryReader Ar)
	{
		TPayloadThreeParam<uint8, uint8, uint8> Payload(Ar);
		IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
		FInputDeviceId ControllerId = DeviceMapper.GetDefaultInputDevice();

		XRSystem System = IPixelStreamingHMDModule::Get().GetActiveXRSystem();
		EControllerHand Handedness = static_cast<EControllerHand>(Payload.Param1);
		uint8 ButtonIdx = Payload.Param2;
		bool bIsRepeat = Payload.Param3 != 0;

		FKey* ButtonPtr = FPixelStreamingInputConverter::XRInputToFKey.Find(MakeTuple(System, Handedness, ButtonIdx, Action::Click));
		if (ButtonPtr == nullptr)
		{
			return;
		}
		MessageHandler->OnControllerButtonReleased(ButtonPtr->GetFName(), DeviceMapper.GetPrimaryPlatformUser(), ControllerId, bIsRepeat);
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("XR_RELEASED: ControllerId = %d; KeyName = %s; IsRepeat = %s;"), ControllerId.GetId(), *ButtonPtr->ToString(), bIsRepeat ? TEXT("True") : TEXT("False"));

		// Try and see if there is an axis associated with this button (usually the case for triggers)
		// if so, trigger an axis movement with value 0.0
		FKey* AxisPtr = FPixelStreamingInputConverter::XRInputToFKey.Find(MakeTuple(System, Handedness, ButtonIdx, Action::Axis));
		if (AxisPtr == nullptr)
		{
			return;
		}

		float AnalogValue = 0.0f;
		MessageHandler->OnControllerAnalog(AxisPtr->GetFName(), DeviceMapper.GetPrimaryPlatformUser(), ControllerId, AnalogValue);
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("XR_ANALOG: ControllerId = %d; KeyName = %s; AnalogValue = %.4f;"), ControllerId.GetId(), *AxisPtr->ToString(), AnalogValue);
	}

	void FPixelStreamingInputHandler::HandleOnXRAnalog(FMemoryReader Ar)
	{
		TPayloadThreeParam<uint8, uint8, double> Payload(Ar);
		IPlatformInputDeviceMapper& DeviceMapper = IPlatformInputDeviceMapper::Get();
		FInputDeviceId ControllerId = DeviceMapper.GetDefaultInputDevice();

		XRSystem System = IPixelStreamingHMDModule::Get().GetActiveXRSystem();
		EControllerHand Handedness = static_cast<EControllerHand>(Payload.Param1);
		Action InputAction = static_cast<Action>(Payload.Param2 % 2);
		FKey* ButtonPtr = FPixelStreamingInputConverter::XRInputToFKey.Find(MakeTuple(System, Handedness, Payload.Param2, InputAction));
		if (ButtonPtr == nullptr)
		{
			return;
		}

		float AnalogValue = (float)Payload.Param3;
		// UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("XR_ANALOG: ControllerId = %d; KeyName = %s; AnalogValue = %.4f;"), ControllerId.GetId(), *Button.ToString(), AnalogValue);
		MessageHandler->OnControllerAnalog(ButtonPtr->GetFName(), DeviceMapper.GetPrimaryPlatformUser(), ControllerId, AnalogValue);
	}

	void FPixelStreamingInputHandler::HandleOnXRSystem(FMemoryReader Ar)
	{
		uint8 ActiveSystem;
		Ar << ActiveSystem;
		IPixelStreamingHMDModule::Get().SetActiveXRSystem(static_cast<XRSystem>(ActiveSystem));
	}

	void FPixelStreamingInputHandler::SetCommandHandler(const FString& CommandName, const TFunction<void(FString, FString)>& Handler)
	{
		CommandHandlers.Add(CommandName, Handler);
	}

	void FPixelStreamingInputHandler::PopulateDefaultCommandHandlers()
	{

		// Execute console commands if passed "ConsoleCommand" and -PixelStreamingAllowConsoleCommands is on.
		CommandHandlers.Add(TEXT("ConsoleCommand"), [](FString Descriptor, FString ConsoleCommand) {
			if (!UE::PixelStreamingInput::Settings::CVarPixelStreamingInputAllowConsoleCommands.GetValueOnAnyThread())
			{
				return;
			}
			GEngine->Exec(GEngine->GetWorld(), *ConsoleCommand); });

		// Change width/height if sent { "Resolution.Width": 1920, "Resolution.Height": 1080 }
		CommandHandlers.Add(TEXT("Resolution.Width"), [](FString Descriptor, FString WidthString) {
			bool bSuccess = false;
			FString HeightString;
			UE::PixelStreamingInput::ExtractJsonFromDescriptor(Descriptor, TEXT("Resolution.Height"), HeightString, bSuccess);
			if (bSuccess)
			{
				int Width = FCString::Atoi(*WidthString);
				int Height = FCString::Atoi(*HeightString);
				if (Width < 1 || Height < 1)
				{
					return;
				}

				FString ChangeResCommand = FString::Printf(TEXT("r.SetRes %dx%d"), Width, Height);
				GEngine->Exec(GEngine->GetWorld(), *ChangeResCommand);
			} });

		// Response to "Stat.FPS" by calling "stat fps"
		CommandHandlers.Add(TEXT("Stat.FPS"), [](FString Descriptor, FString FPSCommand) {
			FString StatFPSCommand = FString::Printf(TEXT("stat fps"));
			GEngine->Exec(GEngine->GetWorld(), *StatFPSCommand); });
	}

	/**
	 * Command handling
	 */
	void FPixelStreamingInputHandler::HandleOnCommand(FMemoryReader Ar)
	{
		FString Res;
		Res.GetCharArray().SetNumUninitialized(Ar.TotalSize() / 2 + 1);
		Ar.Serialize(Res.GetCharArray().GetData(), Ar.TotalSize());

		FString Descriptor = Res.Mid(MessageHeaderOffset);
		UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("Command: %s"), *Descriptor);

		// Iterate each command handler and see if the command we got matches any of the bound command names.
		for (auto& CommandHandlersPair : CommandHandlers)
		{
			FString CommandValue;
			bool bSuccess = false;
			UE::PixelStreamingInput::ExtractJsonFromDescriptor(Descriptor, CommandHandlersPair.Key, CommandValue, bSuccess);
			if (bSuccess)
			{
				// Execute bound command handler with descriptor and parsed command value
				CommandHandlersPair.Value(Descriptor, CommandValue);
				return;
			}
		}
	}

	/**
	 * UI Interaction handling
	 */
	void FPixelStreamingInputHandler::HandleUIInteraction(FMemoryReader Ar)
	{
		// Actual implementation is in the Pixel Streaming module
	}

	/**
	 * Textbox Entry handling
	 */
	void FPixelStreamingInputHandler::HandleOnTextboxEntry(FMemoryReader Ar)
	{
		FString Res;
		Res.GetCharArray().SetNumUninitialized(Ar.TotalSize() / 2 + 1);
		Ar.Serialize(Res.GetCharArray().GetData(), Ar.TotalSize());
		FString Text = Res.Mid(1);

		FSlateApplication::Get().ForEachUser([this, Text](FSlateUser& User) {
			TSharedPtr<SWidget> FocusedWidget = User.GetFocusedWidget();

			bool bIsEditableTextType = FocusedWidget->GetType() == TEXT("SEditableText");
			bool bIsMultiLineEditableTextType = FocusedWidget->GetType() == TEXT("SMultiLineEditableText");
			bool bEditable = FocusedWidget && (bIsEditableTextType || bIsMultiLineEditableTextType);

			if (bEditable)
			{
				if (bIsEditableTextType)
				{
					SEditableText* TextBox = static_cast<SEditableText*>(FocusedWidget.Get());
					TextBox->SetText(FText::FromString(Text));
				}
				else if (bIsMultiLineEditableTextType)
				{
					SMultiLineEditableTextBox* TextBox = static_cast<SMultiLineEditableTextBox*>(FocusedWidget.Get());
					TextBox->SetText(FText::FromString(Text));
				}
			}
		});
	}

	FIntPoint FPixelStreamingInputHandler::ConvertFromNormalizedScreenLocation(const FVector2D& ScreenLocation, bool bIncludeOffset)
	{
		FIntPoint OutVector((int32)ScreenLocation.X, (int32)ScreenLocation.Y);

		if (TSharedPtr<SWindow> ApplicationWindow = TargetWindow.Pin())
		{
			FVector2D WindowOrigin = ApplicationWindow->GetPositionInScreen();
			if (TSharedPtr<SViewport> ViewportWidget = TargetViewport.Pin())
			{
				FGeometry InnerWindowGeometry = ApplicationWindow->GetWindowGeometryInWindow();

				// Find the widget path relative to the window
				FArrangedChildren JustWindow(EVisibility::Visible);
				JustWindow.AddWidget(FArrangedWidget(ApplicationWindow.ToSharedRef(), InnerWindowGeometry));

				FWidgetPath PathToWidget(ApplicationWindow.ToSharedRef(), JustWindow);
				if (PathToWidget.ExtendPathTo(FWidgetMatcher(ViewportWidget.ToSharedRef()), EVisibility::Visible))
				{
					FArrangedWidget ArrangedWidget = PathToWidget.FindArrangedWidget(ViewportWidget.ToSharedRef()).Get(FArrangedWidget::GetNullWidget());

					FVector2D WindowClientOffset = ArrangedWidget.Geometry.GetAbsolutePosition();
					FVector2D WindowClientSize = ArrangedWidget.Geometry.GetAbsoluteSize();

					FVector2D OutTemp = bIncludeOffset
						? (ScreenLocation * WindowClientSize) + WindowOrigin + WindowClientOffset
						: (ScreenLocation * WindowClientSize);
					UE_LOG(LogPixelStreamingInputHandler, Verbose, TEXT("%.4f, %.4f"), ScreenLocation.X, ScreenLocation.Y);
					OutVector = FIntPoint((int32)OutTemp.X, (int32)OutTemp.Y);
				}
			}
			else
			{
				FVector2D SizeInScreen = ApplicationWindow->GetSizeInScreen();
				FVector2D OutTemp = bIncludeOffset
					? (SizeInScreen * ScreenLocation) + ApplicationWindow->GetPositionInScreen()
					: (SizeInScreen * ScreenLocation);
				OutVector = FIntPoint((int32)OutTemp.X, (int32)OutTemp.Y);
			}
		}
		else if (TSharedPtr<FIntRect> ScreenRectPtr = TargetScreenRect.Pin())
		{
			FIntRect ScreenRect = *ScreenRectPtr;
			FIntPoint SizeInScreen = ScreenRect.Max - ScreenRect.Min;
			FVector2D OutTemp = FVector2D(SizeInScreen.X, SizeInScreen.Y) * ScreenLocation + (bIncludeOffset ? FVector2D(ScreenRect.Min.X, ScreenRect.Min.Y) : FVector2D(0, 0));
			OutVector = FIntPoint((int32)OutTemp.X, (int32)OutTemp.Y);
		}
		else if (TSharedPtr<FIntPoint> ScreenSize = TargetScreenSize.Pin())
		{
			UE_LOG(LogPixelStreamingInputHandler, Warning, TEXT("You're using deprecated functionality by setting a target screen size. This functionality will be removed in later versions. Please use SetTargetScreenRect instead!"));
			FIntPoint SizeInScreen = *ScreenSize;
			FVector2D OutTemp = FVector2D(SizeInScreen) * ScreenLocation;
			OutVector = FIntPoint((int32)OutTemp.X, (int32)OutTemp.Y);
		}

		return OutVector;
	}

	bool FPixelStreamingInputHandler::FilterKey(const FKey& Key)
	{
		for (auto&& FilteredKey : Settings::FilteredKeys)
		{
			if (FilteredKey == Key)
				return false;
		}
		return true;
	}

	void FPixelStreamingInputHandler::ProcessLatestAnalogInputFromThisTick()
	{
		for (auto AnalogInputIt = AnalogEventsReceivedThisTick.CreateConstIterator(); AnalogInputIt; ++AnalogInputIt)
		{
			for (auto FKeyIt = AnalogInputIt->Value.CreateConstIterator(); FKeyIt; ++FKeyIt)
			{
				ProcessAnalog(AnalogInputIt->Key, FKeyIt->Key, FKeyIt->Value);
			}
		}
		AnalogEventsReceivedThisTick.Reset();
	}

	void FPixelStreamingInputHandler::ProcessAnalog(const FInputDeviceId& ControllerId, FKeyId Key, FAnalogValue AnalogValue)
	{
		const FKey* AxisPtr = FPixelStreamingInputConverter::GamepadInputToFKey.Find(MakeTuple(Key, Action::Axis));
		if (!AxisPtr)
		{
			return;
		}
		
		FSlateApplication& SlateApplication = FSlateApplication::Get();
		const FAnalogInputEvent AnalogInputEvent(
			*AxisPtr,														/* InKey */
			SlateApplication.GetPlatformApplication()->GetModifierKeys(),	/* InModifierKeys */
			ControllerId,													/* InDeviceId */
			false,													/* bInIsRepeat*/
			0,													/* InCharacterCode */
			0,														/* InKeyCode */
			AnalogValue,													/* InAnalogValue */
			// TODO (william.belcher): This user idx should be the playerId
			0 /* InUserIndex */
		);
		FSlateApplication::Get().ProcessAnalogInputEvent(AnalogInputEvent);
	}

	void FPixelStreamingInputHandler::BroadcastActiveTouchMoveEvents()
	{
		if (!ensure(MessageHandler))
		{
			return;
		}

		for (TPair<int32, FCachedTouchEvent> CachedTouchEvent : CachedTouchEvents)
		{
			const int32& TouchIndex = CachedTouchEvent.Key;
			const FCachedTouchEvent& TouchEvent = CachedTouchEvent.Value;

			// Only broadcast events that haven't already been fired this frame
			if (!TouchIndicesProcessedThisFrame.Contains(TouchIndex))
			{
				if (InputType == EPixelStreamingInputType::RouteToWidget)
				{
					FWidgetPath WidgetPath = FindRoutingMessageWidget(TouchEvent.Location);

					if (WidgetPath.IsValid())
					{
						FScopedSwitchWorldHack SwitchWorld(WidgetPath);
						FPointerEvent PointerEvent(0, TouchIndex, TouchEvent.Location, LastTouchLocation, TouchEvent.Force, true);
						FSlateApplication::Get().RoutePointerMoveEvent(WidgetPath, PointerEvent, false);
					}
				}
				else if (InputType == EPixelStreamingInputType::RouteToWindow)
				{
					MessageHandler->OnTouchMoved(TouchEvent.Location, TouchEvent.Force, TouchIndex, TouchEvent.ControllerIndex);
				}
			}
		}
	}

	FKey FPixelStreamingInputHandler::TranslateMouseButtonToKey(const EMouseButtons::Type Button)
	{
		FKey Key = EKeys::Invalid;

		switch (Button)
		{
			case EMouseButtons::Left:
				Key = EKeys::LeftMouseButton;
				break;
			case EMouseButtons::Middle:
				Key = EKeys::MiddleMouseButton;
				break;
			case EMouseButtons::Right:
				Key = EKeys::RightMouseButton;
				break;
			case EMouseButtons::Thumb01:
				Key = EKeys::ThumbMouseButton;
				break;
			case EMouseButtons::Thumb02:
				Key = EKeys::ThumbMouseButton2;
				break;
		}

		return Key;
	}

	void FPixelStreamingInputHandler::FindFocusedWidget()
	{
		FSlateApplication::Get().ForEachUser([this](FSlateUser& User) {
			TSharedPtr<SWidget> FocusedWidget = User.GetFocusedWidget();

			static FName SEditableTextType(TEXT("SEditableText"));
			static FName SMultiLineEditableTextType(TEXT("SMultiLineEditableText"));
			bool bEditable = FocusedWidget && (FocusedWidget->GetType() == SEditableTextType || FocusedWidget->GetType() == SMultiLineEditableTextType);

			// Tell the browser that the focus has changed.
			TSharedPtr<FJsonObject> JsonObject = MakeShareable(new FJsonObject);
			JsonObject->SetStringField(TEXT("command"), TEXT("onScreenKeyboard"));
			JsonObject->SetBoolField(TEXT("showOnScreenKeyboard"), bEditable);

			if (bEditable)
			{
				FVector2D NormalizedLocation;
				TSharedPtr<SWindow> ApplicationWindow = TargetWindow.Pin();
				if (ApplicationWindow.IsValid())
				{
					FVector2D WindowOrigin = ApplicationWindow->GetPositionInScreen();
					FVector2D Pos = FocusedWidget->GetCachedGeometry().GetAbsolutePosition();
					if (TargetViewport.IsValid())
					{
						TSharedPtr<SViewport> ViewportWidget = TargetViewport.Pin();

						if (ViewportWidget.IsValid())
						{
							FGeometry InnerWindowGeometry = ApplicationWindow->GetWindowGeometryInWindow();

							// Find the widget path relative to the window
							FArrangedChildren JustWindow(EVisibility::Visible);
							JustWindow.AddWidget(FArrangedWidget(ApplicationWindow.ToSharedRef(), InnerWindowGeometry));

							FWidgetPath PathToWidget(ApplicationWindow.ToSharedRef(), JustWindow);
							if (PathToWidget.ExtendPathTo(FWidgetMatcher(ViewportWidget.ToSharedRef()), EVisibility::Visible))
							{
								FArrangedWidget ArrangedWidget = PathToWidget.FindArrangedWidget(ViewportWidget.ToSharedRef()).Get(FArrangedWidget::GetNullWidget());

								FVector2D WindowClientOffset = ArrangedWidget.Geometry.GetAbsolutePosition();
								FVector2D WindowClientSize = ArrangedWidget.Geometry.GetAbsoluteSize();

								Pos = Pos - WindowClientOffset;
								NormalizedLocation = FVector2D(Pos / WindowClientSize);
							}
						}
					}
					else
					{
						FVector2D SizeInScreen = ApplicationWindow->GetSizeInScreen();
						NormalizedLocation = FVector2D(Pos / SizeInScreen);
					}
				}
				else if (TSharedPtr<FIntRect> ScreenRectPtr = TargetScreenRect.Pin())
				{
					FIntRect ScreenRect = *ScreenRectPtr;
					FIntPoint SizeInScreen = ScreenRect.Max - ScreenRect.Min;
					NormalizedLocation = FocusedPos / SizeInScreen;
				}

				NormalizedLocation *= uint16_MAX;
				// ConvertToNormalizedScreenLocation(Pos, NormalizedLocation);
				JsonObject->SetNumberField(TEXT("x"), static_cast<uint16>(NormalizedLocation.X));
				JsonObject->SetNumberField(TEXT("y"), static_cast<uint16>(NormalizedLocation.Y));

				FText TextboxContents;
				if (FocusedWidget->GetType() == TEXT("SEditableText"))
				{
					SEditableText* TextBox = static_cast<SEditableText*>(FocusedWidget.Get());
					TextboxContents = TextBox->GetText();
				}
				else if (FocusedWidget->GetType() == TEXT("SMultiLineEditableText"))
				{
					SMultiLineEditableText* TextBox = static_cast<SMultiLineEditableText*>(FocusedWidget.Get());
					TextboxContents = TextBox->GetText();
				}

				JsonObject->SetStringField(TEXT("contents"), TextboxContents.ToString());
			}

			FString Descriptor;
			TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Descriptor);
			FJsonSerializer::Serialize(JsonObject.ToSharedRef(), JsonWriter);

			FBufferArchive Buffer;
			Buffer << Descriptor;
			TArray<uint8> Data(Buffer.GetData(), Buffer.Num());
			// Specific implementation for this method is handled per streamer
			OnSendMessage.Broadcast("Command", FMemoryReader(Data));
		});
	}

	FWidgetPath FPixelStreamingInputHandler::FindRoutingMessageWidget(const FVector2D& Location) const
	{
		if (TSharedPtr<SWindow> PlaybackWindowPinned = TargetWindow.Pin())
		{
			if (PlaybackWindowPinned->AcceptsInput())
			{
				bool bIgnoreEnabledStatus = false;
				TArray<FWidgetAndPointer> WidgetsAndCursors = PlaybackWindowPinned->GetHittestGrid().GetBubblePath(Location, FSlateApplication::Get().GetCursorRadius(), bIgnoreEnabledStatus);
				return FWidgetPath(MoveTemp(WidgetsAndCursors));
			}
		}
		return FWidgetPath();
	}
} // namespace UE::PixelStreamingInput
