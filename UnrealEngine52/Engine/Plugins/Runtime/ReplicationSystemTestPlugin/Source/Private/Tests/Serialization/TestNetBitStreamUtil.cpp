// Copyright Epic Games, Inc. All Rights Reserved.

#include "NetworkAutomationTest.h"
#include "NetworkAutomationTestMacros.h"
#include "Containers/StringConv.h"
#include "Iris/Serialization/NetBitStreamReader.h"
#include "Iris/Serialization/NetBitStreamWriter.h"
#include "Iris/Serialization/NetBitStreamUtil.h"

namespace UE::Net::Private
{

class FNetBitStreamUtilTest : public FNetworkAutomationTestSuiteFixture
{
protected:
	enum : unsigned
	{
		BitStreamBufferSize = 1024,
	};

	FNetBitStreamReader Reader;
	FNetBitStreamWriter Writer;
	uint32 BitStreamBuffer[BitStreamBufferSize];
};

//const char* EmptyString = "";
//const char* ANSIString = "Just a regular ANSI string";
//const char* UTF8EncodedString = ;

UE_NET_TEST_FIXTURE(FNetBitStreamUtilTest, TestEmptyString)
{
	const FString EmptyString("");

	Writer.InitBytes(BitStreamBuffer, BitStreamBufferSize);

	WriteString(&Writer, EmptyString);
	Writer.CommitWrites();
	UE_NET_ASSERT_FALSE(Writer.IsOverflown());
	UE_NET_ASSERT_GT(Writer.GetPosBits(), 0U);

	FString String;
	Reader.InitBits(BitStreamBuffer, Writer.GetPosBits());
	ReadString(&Reader, String);
	UE_NET_ASSERT_FALSE(Reader.IsOverflown());
	UE_NET_ASSERT_EQ(String, EmptyString);
}

UE_NET_TEST_FIXTURE(FNetBitStreamUtilTest, TestANSIString)
{
	const FString ANSIString(TEXT("An ANSI string"));

	Writer.InitBytes(BitStreamBuffer, BitStreamBufferSize);

	WriteString(&Writer, ANSIString);
	Writer.CommitWrites();
	UE_NET_ASSERT_FALSE(Writer.IsOverflown());
	UE_NET_ASSERT_GT(Writer.GetPosBits(), 0U);

	FString String;
	Reader.InitBits(BitStreamBuffer, Writer.GetPosBits());
	ReadString(&Reader, String);
	UE_NET_ASSERT_FALSE(Reader.IsOverflown());
	UE_NET_ASSERT_EQ(String, ANSIString);
}

UE_NET_TEST_FIXTURE(FNetBitStreamUtilTest, TestWideString)
{
	const FString WideString(UTF8_TO_TCHAR("\xf0\x9f\x98\x86\xf0\x9f\x80\x9c\xe2\x82\xac\xc2\xa9"));

	Writer.InitBytes(BitStreamBuffer, BitStreamBufferSize);

	WriteString(&Writer, WideString);
	Writer.CommitWrites();
	UE_NET_ASSERT_FALSE(Writer.IsOverflown());
	UE_NET_ASSERT_GT(Writer.GetPosBits(), 0U);

	FString String;
	Reader.InitBits(BitStreamBuffer, Writer.GetPosBits());
	ReadString(&Reader, String);
	UE_NET_ASSERT_FALSE(Reader.IsOverflown());
	UE_NET_ASSERT_EQ(String, WideString);
}

UE_NET_TEST_FIXTURE(FNetBitStreamUtilTest, TestTooLongStringWritesEmptyString)
{
	constexpr int32 VeryLongStringLength = 77777;

	FString VeryLongString;
	VeryLongString.Appendf(TEXT("%*c"), VeryLongStringLength, 'y');
	UE_NET_ASSERT_EQ(VeryLongString.Len(), VeryLongStringLength);

	TArray<uint8> VeryLargeBuffer;
	VeryLargeBuffer.SetNumUninitialized(Align(VeryLongStringLength + 1024, 4));
	Writer.InitBytes(VeryLargeBuffer.GetData(), VeryLargeBuffer.Num());
	WriteString(&Writer, VeryLongString);
	Writer.CommitWrites();
	UE_NET_ASSERT_FALSE(Writer.IsOverflown());
	UE_NET_ASSERT_GT(Writer.GetPosBits(), 0U);

	FString String;
	Reader.InitBits(VeryLargeBuffer.GetData(), Writer.GetPosBits());
	ReadString(&Reader, String);
	UE_NET_ASSERT_FALSE(Reader.IsOverflown());
	UE_NET_ASSERT_EQ(String, FString());

	// Reset Writer to avoid write after free.
	Writer = FNetBitStreamWriter();
}

}
