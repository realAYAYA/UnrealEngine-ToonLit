// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLensInputInterface.h"
#include "Misc/CoreDelegates.h"
#include "GenericPlatform/GenericApplication.h"
#include "WindowsGamingInputInterface.h"

#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#include <Winuser.h>
#include "Microsoft/HideMicrosoftPlatformTypes.h"


const uint32 HoloLensKeyCount = 256;
bool HoloLensKeyState[HoloLensKeyCount];
bool HoloLensKeyLastState[HoloLensKeyCount];
bool HoloLensCharState[HoloLensKeyCount];
bool HoloLensCharLastState[HoloLensKeyCount];

void appHoloLensKeyEvent(uint32 InCode, bool bInWasPressed)
{
	if (InCode < HoloLensKeyCount)
	{
		HoloLensKeyState[InCode] = bInWasPressed;
	}
}

void appHoloLensCharEvent(uint32 InCode, bool bInWasPressed)
{
	if (InCode < HoloLensKeyCount)
	{
		HoloLensCharState[InCode] = bInWasPressed;
	}
}

TSharedRef< FHoloLensInputInterface > FHoloLensInputInterface::Create( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	return MakeShareable( new FHoloLensInputInterface( InMessageHandler ) );
}

FHoloLensInputInterface::FHoloLensInputInterface( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
	: MessageHandler( InMessageHandler )
	, GamingInput( WindowsGamingInputInterface::Create(InMessageHandler) )
{
	FMemory::Memzero(HoloLensKeyState, HoloLensKeyCount);
	FMemory::Memzero(HoloLensKeyLastState, HoloLensKeyCount);
	FMemory::Memzero(HoloLensCharState, HoloLensKeyCount);
	FMemory::Memzero(HoloLensCharLastState, HoloLensKeyCount);

	// register focus handling events
	FCoreDelegates::ApplicationHasReactivatedDelegate.AddRaw( this, &FHoloLensInputInterface::OnFocusGain );
	FCoreDelegates::ApplicationWillDeactivateDelegate.AddRaw( this, &FHoloLensInputInterface::OnFocusLost );
}

FHoloLensInputInterface::~FHoloLensInputInterface()
{
	// unregister focus handling events
	FCoreDelegates::ApplicationHasReactivatedDelegate.RemoveAll( this );
	FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll( this );
}

void FHoloLensInputInterface::SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler )
{
	MessageHandler = InMessageHandler;

	GamingInput->SetMessageHandler(InMessageHandler);
}

void FHoloLensInputInterface::Tick( float DeltaTime )
{
	ConditionalScanForKeyboardChanges(DeltaTime);
}

void FHoloLensInputInterface::SendControllerEvents()
{
	GamingInput->UpdateGamepads();
}

void FHoloLensInputInterface::SetForceFeedbackChannelValue(int32 ControllerId, FForceFeedbackChannelType ChannelType, float Value)
{
	FForceFeedbackValues Values;
	GamingInput->GetVibration(ControllerId, Values.LeftLarge, Values.RightLarge, Values.LeftSmall, Values.RightSmall);

	switch (ChannelType)
	{
	case FForceFeedbackChannelType::LEFT_LARGE:
		Values.LeftLarge = Value;
		break;

	case FForceFeedbackChannelType::LEFT_SMALL:
		Values.LeftSmall = Value;
		break;

	case FForceFeedbackChannelType::RIGHT_LARGE:
		Values.RightLarge = Value;
		break;

	case FForceFeedbackChannelType::RIGHT_SMALL:
		Values.RightSmall = Value;
		break;

	default:
		// Unknown channel, so ignore it
		break;
	}

	SetForceFeedbackChannelValues(ControllerId, Values);
}

void FHoloLensInputInterface::SetForceFeedbackChannelValues(int32 ControllerId, const FForceFeedbackValues &Values)
{
	GamingInput->SetVibration(ControllerId, Values.LeftLarge, Values.RightLarge, Values.LeftSmall, Values.RightSmall);
}

//Windows::Gaming::Input::Gamepad^ FHoloLensInputInterface::GetGamepadForUser(const int UserId)
//{
//	return GamingInput->GetGamepadFor(UserId);
//}

int FHoloLensInputInterface::GetUserIdForController(Windows::Gaming::Input::Gamepad^ Controller)
{
	// TODO: handle multi-user scenario in HoloLens
	return 0;
}

/** Platform-specific backdoor to get the FPlatformUserId from an XboxUserId */
//FPlatformUserId FHoloLensInputInterface::GetPlatformUserIdFromXboxUserId(const TCHAR* XboxUserId)
//{
//	// TODO: handle multi-user scenario in HoloLens
//	return 0;
//}


Windows::Gaming::Input::Gamepad^ FHoloLensInputInterface::GetGamepadForControllerId(int32 ControllerId)
{
	return GamingInput->GetGamepadForIndex(ControllerId);
}


void FHoloLensInputInterface::OnFocusLost()
{
	FMemory::Memzero(HoloLensKeyState);
	FMemory::Memzero(HoloLensCharState);

	GamingInput->DisableUpdate();
	ConditionalScanForKeyboardChanges(0.0f);
}

void FHoloLensInputInterface::OnFocusGain()
{
	GamingInput->EnableUpdate();
}

uint32 LegacyMapVirtualKey(uint32 InVirtualKey)
{
	if ((InVirtualKey >= 48 && InVirtualKey <= 57)
		|| (InVirtualKey >= 65 && InVirtualKey <= 90)
		|| InVirtualKey == 8
		|| InVirtualKey == 9
		|| InVirtualKey == 13
		|| InVirtualKey == 27
		|| InVirtualKey == 32)
	{
		return InVirtualKey;
	}

	if (InVirtualKey >= 96 && InVirtualKey <= 105)
	{
		return InVirtualKey - 48;
	}

	if ((InVirtualKey == 106 || InVirtualKey == 107)
		|| (InVirtualKey >= 109 && InVirtualKey <= 111))
	{
		return InVirtualKey - 64;
	}

	// Special keys
	switch (InVirtualKey)
	{
	case 0x6A: return '*';
	case 0x6B: return '+';
	case 0x6D: return '-';
	case 0x6E: return '.';
	case 0x6F: return '/';

	case 0xBA: return ';';  // VK_OEM_1
	case 0xBB: return '=';  // VK_OEM_PLUS
	case 0xBC: return ',';  // VK_OEM_COMMA
	case 0xBD: return '_';  // VK_OEM_MINUS
	case 0xBE: return '.';  // VK_OEM_PERIOD
	case 0xBF: return '/';  // VK_OEM_2
	case 0xC0: return '`';  // VK_OEM_3

	case 0xDB: return '[';  // VK_OEM_4
	case 0xDC: return '\\'; // VK_OEM_5
	case 0xDD: return ']';  // VK_OEM_6
	case 0xDE: return '\''; // VK_OEM_7
	}

	return 0;
}

void FHoloLensInputInterface::ConditionalScanForKeyboardChanges( float DeltaTime )
{
	bool bShiftIsDown = (HoloLensKeyState[VK_LSHIFT] == true) | (HoloLensKeyState[VK_RSHIFT] == true);
	FModifierKeysState ModifierKeyState(
		(HoloLensKeyState[VK_LSHIFT] == true) ? true : false,
		(HoloLensKeyState[VK_RSHIFT] == true) ? true : false,
		(HoloLensKeyState[VK_LCONTROL] == true) ? true : false,
		(HoloLensKeyState[VK_RCONTROL] == true) ? true : false,
		false,
		false,
		false,
		false,
		false
		);

	for (uint32 KeyIndex = 0; KeyIndex < HoloLensKeyCount; KeyIndex++)
	{
		// Process key up/down
		{
			bool bKeyPressed = HoloLensKeyState[KeyIndex];
			bool bKeyReleased = ((HoloLensKeyState[KeyIndex] == false) && (HoloLensKeyLastState[KeyIndex] == true));

			if ((bKeyPressed == true) || (bKeyReleased == true))
			{
				bool bIsRepeat = ((bKeyPressed == true) && (HoloLensKeyLastState[KeyIndex] == true));
				
				// Get the character code from the virtual key pressed.  If 0, no translation from virtual key to character exists
				uint32 CharCode = LegacyMapVirtualKey(KeyIndex);

				//@todo.HoloLens: Put in a delay for registering repeats??
				if (bIsRepeat == false)
				{
					if (bKeyReleased == false)
					{
						MessageHandler->OnKeyDown( KeyIndex, CharCode, bIsRepeat );
					}
					else
					{
						MessageHandler->OnKeyUp( KeyIndex, CharCode, bIsRepeat );
					}
				}

				if (bKeyReleased == true)
				{
					uint32 CharKeyIndex = MapVirtualKeyToCharacter(KeyIndex, bShiftIsDown);
					// Mark the char as released...
					if (CharKeyIndex != 0)
					{
						HoloLensCharState[CharKeyIndex] = false;
						HoloLensCharLastState[CharKeyIndex] = false;
					}
				}
			}
		}

		// Process char
		bool bCharPressed = HoloLensCharState[KeyIndex];
		if (bCharPressed == true)
		{
			bool bIsRepeat = HoloLensCharLastState[KeyIndex];
			//@todo.HoloLens: Put in a delay for registering repeats??
			if (bIsRepeat == false)
			{
				MessageHandler->OnKeyChar( KeyIndex, bIsRepeat );
			}
		}
		
		HoloLensKeyLastState[KeyIndex] = HoloLensKeyState[KeyIndex];
		HoloLensCharLastState[KeyIndex] = HoloLensCharState[KeyIndex];
	}
}

uint32 FHoloLensInputInterface::MapVirtualKeyToCharacter(uint32 InVirtualKey, bool bShiftIsDown)
{
	uint32 TranslatedKeyCode = (uint32)InVirtualKey;

	switch (InVirtualKey)
	{
	case VK_LBUTTON:
	case VK_RBUTTON:
	case VK_MBUTTON:
	case VK_XBUTTON1:
	case VK_XBUTTON2:
	case VK_BACK:
	case VK_TAB:
	case VK_RETURN:
	case VK_PAUSE:
	case VK_CAPITAL:
	case VK_ESCAPE:
	case VK_END:
	case VK_HOME:
	case VK_LEFT:
	case VK_UP:
	case VK_RIGHT:
	case VK_DOWN:
	case VK_INSERT:
	case VK_DELETE:
		TranslatedKeyCode = 0;
		break;

	case VK_SPACE:
		TranslatedKeyCode = TEXT(' ');
		break;

	// VK_0 - VK_9 are the same as ASCII '0' - '9' (0x30 - 0x39)
	// 0x40 : unassigned
	// VK_A - VK_Z are the same as ASCII 'A' - 'Z' (0x41 - 0x5A)
	case 0x41:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('a') : TEXT('A');
		break;
	case 0x42:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('b') : TEXT('B');
		break;
	case 0x43:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('c') : TEXT('C');
		break;
	case 0x44:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('d') : TEXT('D');
		break;
	case 0x45:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('e') : TEXT('E');
		break;
	case 0x46:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('f') : TEXT('F');
		break;
	case 0x47:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('g') : TEXT('G');
		break;
	case 0x48:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('h') : TEXT('H');
		break;
	case 0x49:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('i') : TEXT('I');
		break;
	case 0x4A:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('j') : TEXT('J');
		break;
	case 0x4B:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('k') : TEXT('K');
		break;
	case 0x4C:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('l') : TEXT('L');
		break;
	case 0x4D:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('m') : TEXT('M');
		break;
	case 0x4E:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('n') : TEXT('N');
		break;
	case 0x4F:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('o') : TEXT('O');
		break;
	case 0x50:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('p') : TEXT('P');
		break;
	case 0x51:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('q') : TEXT('Q');
		break;
	case 0x52:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('r') : TEXT('R');
		break;
	case 0x53:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('s') : TEXT('S');
		break;
	case 0x54:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('t') : TEXT('T');
		break;
	case 0x55:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('u') : TEXT('U');
		break;
	case 0x56:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('v') : TEXT('V');
		break;
	case 0x57:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('w') : TEXT('W');
		break;
	case 0x58:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('x') : TEXT('X');
		break;
	case 0x59:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('y') : TEXT('Y');
		break;
	case 0x5A:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('z') : TEXT('Z');
		break;

	case 0x30:
	case VK_NUMPAD0:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('0') : TEXT(')'); 
		break;
	case 0x31:
	case VK_NUMPAD1:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('1') : TEXT('!'); 
		break;
	case 0x32:
	case VK_NUMPAD2:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('2') : TEXT('@'); 
		break;
	case 0x33:
	case VK_NUMPAD3:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('3') : TEXT('#'); 
		break;
	case 0x34:
	case VK_NUMPAD4:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('4') : TEXT('$'); 
		break;
	case 0x35:
	case VK_NUMPAD5:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('5') : TEXT('%'); 
		break;
	case 0x36:
	case VK_NUMPAD6:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('6') : TEXT('^'); 
		break;
	case 0x37:
	case VK_NUMPAD7:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('7') : TEXT('&'); 
		break;
	case 0x38:
	case VK_NUMPAD8:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('8') : TEXT('*'); 
		break;
	case 0x39:
	case VK_NUMPAD9:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('9') : TEXT('('); 
		break;

	case VK_MULTIPLY:	
		TranslatedKeyCode = TEXT('*'); 
		break;
	case VK_ADD:		
		TranslatedKeyCode = TEXT('+'); 
		break;
	case VK_SUBTRACT:	
		TranslatedKeyCode = TEXT('-'); 
		break;
	case VK_DECIMAL:	
		TranslatedKeyCode = TEXT('.'); 
		break;
	case VK_DIVIDE:		
		TranslatedKeyCode = TEXT('/'); 
		break;

	case VK_F1:
	case VK_F2:
	case VK_F3:
	case VK_F4:
	case VK_F5:
	case VK_F6:
	case VK_F7:
	case VK_F8:
	case VK_F9:
	case VK_F10:
	case VK_F11:
	case VK_F12:
	case VK_NUMLOCK:
	case VK_SCROLL:
	case VK_LSHIFT:
	case VK_RSHIFT:
	case VK_LCONTROL:
	case VK_RCONTROL:
	case VK_LMENU:
	case VK_RMENU:
		TranslatedKeyCode = 0;
		break;

	case 0xbb:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('=') : TEXT('+');
		break;
	case 0xbc:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT(',') : TEXT('<');
		break;
	case 0xbd:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('-') : TEXT('_');
		break;
	case 0xbe:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('.') : TEXT('>');
		break;
	case 0xbf:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('/') : TEXT('?');
		break;
	case 0xc0:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('`') : TEXT('~');
		break;
	case 0xdb:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('[') : TEXT('{');
		break;
	case 0xdc:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT('\\') : TEXT('|'); 
		break;
	case 0xdd:
		TranslatedKeyCode = (!bShiftIsDown) ? TEXT(']') : TEXT('}');
		break;
	}

	return TranslatedKeyCode;
}
