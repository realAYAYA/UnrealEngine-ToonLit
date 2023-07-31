// Copyright Epic Games, Inc. All Rights Reserved.
#include "OSCMessagePacket.h"

#include "OSCBundle.h"
#include "OSCLog.h"
#include "OSCMessage.h"


FOSCMessagePacket::FOSCMessagePacket()
	: IOSCPacket()
{}

FOSCMessagePacket::~FOSCMessagePacket()
{}

const FOSCAddress& FOSCMessagePacket::GetAddress() const
{
	return Address;
}

void FOSCMessagePacket::SetAddress(const FOSCAddress& InAddress)
{
	Address = InAddress;
}

TArray<FOSCType>& FOSCMessagePacket::GetArguments()
{
	return Arguments;
}

bool FOSCMessagePacket::IsBundle()
{
	return false;
}

bool FOSCMessagePacket::IsMessage()
{
	return true;
}

void FOSCMessagePacket::WriteData(FOSCStream& Stream)
{
	if (!Address.IsValidPath())
	{
		UE_LOG(LogOSC, Warning, TEXT("Failed to write OSCMessagePacket. Invalid OSCAddress '%s'"), *Address.GetFullPath());
		return;
	}

	// Begin writing data 
	Stream.WriteString(Address.GetFullPath());

	// write type tags
	FString TagTypes = ",";
	for (int32 i = 0; i < Arguments.Num(); i++)
	{
		TagTypes += static_cast<ANSICHAR>(Arguments[i].GetTypeTag());
	}

	Stream.WriteString(TagTypes);

	for (FOSCType OSCType : Arguments)
	{
		switch (OSCType.GetTypeTag())
		{
		case EOSCTypeTag::OSC_CHAR:
			Stream.WriteChar(OSCType.GetChar());
			break;
		case EOSCTypeTag::OSC_INT32:
			Stream.WriteInt32(OSCType.GetInt32());
			break;
		case EOSCTypeTag::OSC_FLOAT:
			Stream.WriteFloat(OSCType.GetFloat());
			break;
		case EOSCTypeTag::OSC_DOUBLE:
			Stream.WriteDouble(OSCType.GetDouble());
			break;
		case EOSCTypeTag::OSC_INT64:
			Stream.WriteInt64(OSCType.GetInt64());
			break;
		case EOSCTypeTag::OSC_TIME:
			Stream.WriteUInt64(OSCType.GetTimeTag());
			break;
		case EOSCTypeTag::OSC_STRING:
			Stream.WriteString(OSCType.GetString());
			break;
		case EOSCTypeTag::OSC_BLOB:
		{
			TArray<uint8> blob = OSCType.GetBlob();
			Stream.WriteBlob(blob);
		}
		break;
		case EOSCTypeTag::OSC_COLOR:
#if PLATFORM_LITTLE_ENDIAN
			Stream.WriteInt32(OSCType.GetColor().ToPackedABGR());
#else
			Stream.WriteInt32(OSCType.GetColor().ToPackedRGBA());
#endif
			break;
		case EOSCTypeTag::OSC_TRUE:
		case EOSCTypeTag::OSC_FALSE:
		case EOSCTypeTag::OSC_NIL:
		case EOSCTypeTag::OSC_INFINITUM:
			// No values are written for these types
			break;
		default:
			// Argument is not supported 
			unimplemented();
			break;
		}
	}
}

void FOSCMessagePacket::ReadData(FOSCStream& Stream)
{
	// Read Address
	Address = Stream.ReadString();

	// Read string of tags
	const FString StreamString = Stream.ReadString();

	const TArray<TCHAR, FString::AllocatorType>& TagTypes = StreamString.GetCharArray();
	if(TagTypes.Num() == 0)
	{
		UE_LOG(LogOSC, Error, TEXT("Failed to read message packet with address '%s' from stream: Invalid (Empty) Type Tag"), *Address.GetFullPath());
		return;
	}

	// Skip the first argument which is ','
	for (int32 i = 1; i < TagTypes.Num(); i++)
	{
		const EOSCTypeTag Tag = static_cast<EOSCTypeTag>(TagTypes[i]);
		switch (Tag)
		{
		case EOSCTypeTag::OSC_CHAR:
			Arguments.Add(FOSCType(Stream.ReadChar()));
			break;
		case EOSCTypeTag::OSC_INT32:
			Arguments.Add(FOSCType(Stream.ReadInt32()));
			break;
		case EOSCTypeTag::OSC_FLOAT:
			Arguments.Add(FOSCType(Stream.ReadFloat()));
			break;
		case EOSCTypeTag::OSC_DOUBLE:
			Arguments.Add(FOSCType(Stream.ReadDouble()));
			break;
		case EOSCTypeTag::OSC_INT64:
			Arguments.Add(FOSCType(Stream.ReadInt64()));
			break;
		case EOSCTypeTag::OSC_TRUE:
			Arguments.Add(FOSCType(true));
			break;
		case EOSCTypeTag::OSC_FALSE:
			Arguments.Add(FOSCType(false));
			break;
		case EOSCTypeTag::OSC_NIL:
			Arguments.Add(FOSCType(EOSCTypeTag::OSC_NIL));
			break;
		case EOSCTypeTag::OSC_INFINITUM:
			Arguments.Add(FOSCType(EOSCTypeTag::OSC_INFINITUM));
			break;
		case EOSCTypeTag::OSC_TIME:
			Arguments.Add(FOSCType(Stream.ReadUInt64()));
			break;
		case EOSCTypeTag::OSC_STRING:
			Arguments.Add(FOSCType(Stream.ReadString()));
			break;
		case EOSCTypeTag::OSC_BLOB:
			Arguments.Add(FOSCType(Stream.ReadBlob()));
			break;
		case EOSCTypeTag::OSC_COLOR:
			Arguments.Add(FOSCType(FColor(Stream.ReadInt32())));
			break;
		case EOSCTypeTag::OSC_TERMINATE:
			// Return on first terminate found. FString GetCharArray can return
			// an array with multiple terminators.
			return;

		default:
			// Argument is not supported 
			unimplemented();
			break;
		}
	}
}
