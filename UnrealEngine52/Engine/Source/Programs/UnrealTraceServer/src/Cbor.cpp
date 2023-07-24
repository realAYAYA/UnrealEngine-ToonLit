// Copyright Epic Games, Inc. All Rights Reserved.

#include "Pch.h"
#include "Cbor.h"
#include "Utils.h"

// {{{1 misc -------------------------------------------------------------------

enum ECborMajorType
{
	Integer			= 0 << 5,
	NegativeInteger	= 1 << 5,
	ByteString		= 2 << 5,
	TextString		= 3 << 5,
	Array			= 4 << 5,
	Map				= 5 << 5,
	Tag				= 6 << 5,
	Float_Simple	= 7 << 5,
};



// {{{1 context ----------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
void FCborContext::Reset()
{
	NextCursor = nullptr;
}

////////////////////////////////////////////////////////////////////////////////
ECborType FCborContext::GetType() const
{
	return Type;
}

////////////////////////////////////////////////////////////////////////////////
int64 FCborContext::GetLength() const
{
	return Param;
}

////////////////////////////////////////////////////////////////////////////////
FStringView	FCborContext::AsString() const
{
	if (GetType() != ECborType::String)
	{
		return FStringView();
	}

    return FStringView((const char*)(NextCursor - Param), std::size_t(Param));
}

////////////////////////////////////////////////////////////////////////////////
int64 FCborContext::AsInteger() const
{
    return Param;
}

////////////////////////////////////////////////////////////////////////////////
void FCborContext::SetError()
{
	Type = ECborType::Error;
	Param = -1;
	NextCursor = nullptr;
}



// {{{1 writer -----------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
FCborWriter::FCborWriter(FInlineBuffer& InBuffer)
: Buffer(InBuffer)
{
}

////////////////////////////////////////////////////////////////////////////////
void FCborWriter::OpenMap(int32 ItemCount)
{
	if (ItemCount != -1)
	{
		WriteParam(ECborMajorType::Map, ItemCount);
		return;
	}

	uint8 Initial = ECborMajorType::Map | 31;
	Buffer.Append(&Initial, sizeof(Initial));
}

////////////////////////////////////////////////////////////////////////////////
void FCborWriter::OpenArray(int32 ItemCount)
{
	if (ItemCount != -1)
	{
		WriteParam(ECborMajorType::Array, ItemCount);
		return;
	}

	uint8 Initial = ECborMajorType::Array | 31;
	Buffer.Append(&Initial, sizeof(Initial));
}

////////////////////////////////////////////////////////////////////////////////
void FCborWriter::Close()
{
	uint8 Initial = ECborMajorType::Float_Simple | 31;
	Buffer.Append(&Initial, sizeof(Initial));
}

////////////////////////////////////////////////////////////////////////////////
void FCborWriter::WriteString(const char* Value, int32 Length)
{
	Length = (Length < 0) ? int32(strlen(Value)) : Length;

	WriteParam(ECborMajorType::TextString, Length);
	Buffer.Append(Value, Length);
}

////////////////////////////////////////////////////////////////////////////////
void FCborWriter::WriteInteger(int64 Value)
{
	uint32 Negative = (Value < 0);
	if (Negative)
	{
		Value = ~Value;
	}

	uint32 MajorType = Negative ? ECborMajorType::NegativeInteger : ECborMajorType::Integer;
	WriteParam(MajorType, Value);
}

////////////////////////////////////////////////////////////////////////////////
void FCborWriter::WriteParam(uint32 MajorType, uint64 Value)
{
	if (Value <= 23)
	{
		uint8 Initial = MajorType | uint8(Value);
		Buffer.Append(&Initial, 1);
		return;
	}

	uint32 BytesPow2 = 3;
	BytesPow2 -= int32(Value <= 0xffff'ffffu);
	BytesPow2 -= int32(Value <= 0x0000'ffffu);
	BytesPow2 -= int32(Value <= 0x0000'00ffu);

	Value = ((Value & 0xffff'ffff'0000'0000ull) >> 32) | ((Value & 0x0000'0000'ffff'ffffull) << 32);
	Value = ((Value & 0xffff'0000'ffff'0000ull) >> 16) | ((Value & 0x0000'ffff'0000'ffffull) << 16);
	Value = ((Value & 0xff00'ff00'ff00'ff00ull) >>  8) | ((Value & 0x00ff'00ff'00ff'00ffull) <<  8);

	Value >>= 64 - (8 << BytesPow2);

	struct {
		uint8	Initial;
		uint8	Payload[sizeof(Value)];
	} Packet = {
		uint8(MajorType | (24 + BytesPow2)),
	};

	memcpy(Packet.Payload, &Value, sizeof(Value));
	Buffer.Append(&Packet, 1 + (1 << BytesPow2));
}



// {{{1 reader -----------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
FCborReader::FCborReader(const uint8* Data, uint32 DataSize)
: Base(Data)
, DataEnd(Data + DataSize)
{
}

////////////////////////////////////////////////////////////////////////////////
int64 FCborReader::ReadParam(const uint8*& Cursor)
{
	uint32 Additional = *Cursor & 0b0001'1111;
	++Cursor;

	// [0-23] literal values
	if (uint32(23 - Additional) <= 23)
	{
		return Additional;
	}

	// [24-27] value in subsequent bytes
	else if (uint32(27 - Additional) <= 27)
	{
		uint32 Bytes = 1 << (Additional - 24);
		if (Cursor + Bytes > DataEnd)
		{
			return -2;
		}

		int64 Value = 0;
		do
		{
			Value <<= 8;
			Value |= *Cursor;
			++Cursor;
		}
		while (--Bytes);

		return Value;
	}

	// 31: indeterminate length
	else if (Additional == 31)
	{
		return -1;
	}

	// ...something went wrong
	return -2;
}

////////////////////////////////////////////////////////////////////////////////
bool FCborReader::ReadContainer(FCborContext& Context, const uint8* Cursor)
{
	bool bIsMap = (*Cursor >= ECborMajorType::Map);

	int64 Param = ReadParam(Cursor);
	if (Param <= -2)
	{
		Context.SetError();
		return false;
	}

	Context.Type = bIsMap ? ECborType::Map : ECborType::Array;
	Context.NextCursor = Cursor;
	Context.Param = Param;
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FCborReader::ReadInteger(FCborContext& Context, const uint8* Cursor)
{
	bool bIsNegative = (*Cursor >= ECborMajorType::NegativeInteger);

	int64 Param = ReadParam(Cursor);
	if (Param <= -1)
	{
		Context.SetError();
		return false;
	}

	Context.Type = ECborType::Integer;
	Context.NextCursor = Cursor;
	Context.Param = bIsNegative ? ~Param : Param;
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FCborReader::ReadString(FCborContext& Context, const uint8* Cursor)
{
	int64 Param = ReadParam(Cursor);
	if (Param <= -2)
	{
		Context.SetError();
		return false;
	}
	else if (Param >= 0)
	{
		if ((Cursor + Param) > DataEnd)
		{
			Context.SetError();
			return false;
		}

		Cursor += Param;
	}

	Context.Type = ECborType::String;
	Context.NextCursor = Cursor;
	Context.Param = Param;
	return true;
}

////////////////////////////////////////////////////////////////////////////////
bool FCborReader::ReadNext(FCborContext& Context)
{
	const uint8* Cursor = (Context.NextCursor == nullptr) ? Base : Context.NextCursor;
	if (Cursor >= DataEnd)
	{
		Context.Type = ECborType::Eof;
		Context.Param = -1;
		return false;
	}

	int64 Param;
	switch (*Cursor & 0b1110'0000)
	{
	case ECborMajorType::Integer:
	case ECborMajorType::NegativeInteger:
		return ReadInteger(Context, Cursor);

	case ECborMajorType::ByteString:
	case ECborMajorType::TextString:
		return ReadString(Context, Cursor);

	case ECborMajorType::Array:
	case ECborMajorType::Map:
		return ReadContainer(Context, Cursor);
	
	case ECborMajorType::Tag:
		/* unsupported */
		break;

	case ECborMajorType::Float_Simple:
		Param = *Cursor & 0b0001'1111;
		++Cursor;

		switch (Param)
		{
		case 24:
			if (Cursor >= DataEnd)
			{
				Context.SetError();
				return false;
			}

			switch (*Cursor)
			{
			case 20: // false
			case 21: // true
			case 22: // null
			case 23: // undefined
				break;
			}
			break;

		case 25: // half
		case 26: // float
		case 27: // double
			/* if (Cursor + float_size > DataEnd) "basil!" / Context.SetError() */
			break;

		case 31:
			Context.Type = ECborType::End;
			Context.NextCursor = Cursor;
			Context.Param = -1;
			return false; // "false" to facilitate ReadNext() in loops
		}
		// 31 = break
		break;
	}

	return false;
}



// {{{1 test-read --------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
#if defined(_MSC_VER)
#   define DEBUG_BREAK() do { __debugbreak(); } while (0)
#else
#   define DEBUG_BREAK() do { __builtin_trap(); } while (0)
#endif

#define REQUIRE(x) \
	do { \
		if (!(x)) \
			DEBUG_BREAK(); \
	} while (false)

////////////////////////////////////////////////////////////////////////////////
static void TestCborReader_Integer()
{
	struct
	{
		int64	Expected;
		uint32	TruthSize;
		uint8	Truth[9];
	} Cases[] = {
		{ 0,			1, { 0x00 } },
		{ 0,			2, { 0x18, 0x00 } },
		{ 0,			3, { 0x19, 0x00, 0x00 } },
		{ 0,			5, { 0x1a, 0x00, 0x00, 0x00, 0x00 } },
		{ 0,			9, { 0x1b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 } },
		{ 1,			1, { 0x01 } },
		{ 23,			1, { 0x17 } },
		{ 24,			2, { 0x18, 0x18 } },
		{ 255,			2, { 0x18, 0xff } },
		{ 256,			3, { 0x19, 0x01, 0x00 } },
		{ 65535,		3, { 0x19, 0xff, 0xff } },
		{ 65536,		5, { 0x1a, 0x00, 0x01, 0x00, 0x00 } },
		{ 123456,		5, { 0x1a, 0x00, 0x01, 0xe2, 0x40 } },
		{ 123456789,	5, { 0x1a, 0x07, 0x5b, 0xcd, 0x15 } },
		{ -1,			1, { 0x20 } },
		{ -2,			1, { 0x21 } },
		{ -10,			1, { 0x29 } },
		{ -24,			1, { 0x37 } },
		{ -25,			2, { 0x38, 0x18 } },
		{ -500,			3, { 0x39, 0x01, 0xf3 } },
		{ -67000,		5, { 0x3a, 0x00, 0x01, 0x05, 0xb7 } },
		{ -123456789,	5, { 0x3a, 0x07, 0x5b, 0xcd, 0x14 } },
	};

	for (const auto& Case : Cases)
	{
		FCborContext Context;
		FCborReader Reader(Case.Truth, Case.TruthSize);
		REQUIRE(Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::Integer);
		REQUIRE(Context.AsInteger() == Case.Expected);
		REQUIRE(!Reader.ReadNext(Context));
	}
}

////////////////////////////////////////////////////////////////////////////////
static void TestCborReader_String()
{
	// "string"
	{
		const uint8 Truth[] = { 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67 };
		const char* Expected = "string";

		FCborContext Context;
		FCborReader Reader(Truth, TS_ARRAY_COUNT(Truth));
		REQUIRE(Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::String);
		REQUIRE(Context.AsString().Compare(Expected) == 0);
		REQUIRE(!Reader.ReadNext(Context));
	}

	// ""
	{
		const uint8 Truth[] = { 0x60 };
		const char* Expected = "";

		FCborContext Context;
		FCborReader Reader(Truth, TS_ARRAY_COUNT(Truth));
		REQUIRE(Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::String);
		REQUIRE(Context.AsString().Compare(Expected) == 0);
		REQUIRE(!Reader.ReadNext(Context));
	}

	// "stringstringstringstringstringstringstringstringstringstring"
	{
		const uint8 Truth[] = {
			0x78, 0x3c,
			0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			0x73, 0x74, 0x72, 0x69, 0x6e, 0x67
		};
		const char* Expected = "stringstringstringstringstringstringstringstringstringstring";

		FCborContext Context;
		FCborReader Reader(Truth, TS_ARRAY_COUNT(Truth));
		REQUIRE(Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::String);
		REQUIRE(Context.AsString().Compare(Expected) == 0);
		REQUIRE(!Reader.ReadNext(Context));
	}
}

////////////////////////////////////////////////////////////////////////////////
static void TestCborReader_Map()
{
	// {}
	{
		const uint8 Truth[] = { 0xa0 };

		FCborContext Context;
		FCborReader Reader(Truth, TS_ARRAY_COUNT(Truth));
		REQUIRE(Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::Map);
		REQUIRE(Context.GetLength() == 0);
		REQUIRE(!Reader.ReadNext(Context));
	}

	// {...}
	{
		const uint8 Truth[] = { 0xbf, 0xff };

		FCborContext Context;
		FCborReader Reader(Truth, TS_ARRAY_COUNT(Truth));
		REQUIRE(Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::Map);
		REQUIRE(Context.GetLength() == -1);

		REQUIRE(!Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::End);

		REQUIRE(!Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::Eof);
	}

	// { "key":"value", 0:1 }
	{
		const uint8 Truth[] = {
			0xa2,
			0x63, 0x6b, 0x65, 0x79,
			0x65, 0x76, 0x61, 0x6c, 0x75, 0x65,
			0x00, 0x01
		};

		FCborContext Context;
		FCborReader Reader(Truth, TS_ARRAY_COUNT(Truth));

		REQUIRE(Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::Map);
		REQUIRE(Context.GetLength() == 2);

		REQUIRE(Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::String);
		REQUIRE(Context.AsString().Compare("key") == 0);

		REQUIRE(Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::String);
		REQUIRE(Context.AsString().Compare("value") == 0);

		REQUIRE(Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::Integer);
		REQUIRE(Context.AsInteger() == 0);

		REQUIRE(Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::Integer);
		REQUIRE(Context.AsInteger() == 1);

		REQUIRE(!Reader.ReadNext(Context));
		REQUIRE(Context.GetType() == ECborType::Eof);
	}
}

// {{{1 test-write -------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
static void TestCborWriter_Integer()
{
	struct
	{
		int64	Content;
		uint32	TruthSize;
		uint8	Truth[9];
	} Cases[] = {
		{ 0,			1, { 0x00 } },
		{ 1,			1, { 0x01 } },
		{ 23,			1, { 0x17 } },
		{ 24,			2, { 0x18, 0x18 } },
		{ 255,			2, { 0x18, 0xff } },
		{ 256,			3, { 0x19, 0x01, 0x00 } },
		{ 65535,		3, { 0x19, 0xff, 0xff } },
		{ 65536,		5, { 0x1a, 0x00, 0x01, 0x00, 0x00 } },
		{ 123456,		5, { 0x1a, 0x00, 0x01, 0xe2, 0x40 } },
		{ 123456789,	5, { 0x1a, 0x07, 0x5b, 0xcd, 0x15 } },
		{ -1,			1, { 0x20 } },
		{ -2,			1, { 0x21 } },
		{ -10,			1, { 0x29 } },
		{ -24,			1, { 0x37 } },
		{ -25,			2, { 0x38, 0x18 } },
		{ -500,			3, { 0x39, 0x01, 0xf3 } },
		{ -67000,		5, { 0x3a, 0x00, 0x01, 0x05, 0xb7 } },
		{ -123456789,	5, { 0x3a, 0x07, 0x5b, 0xcd, 0x14 } },
	};

	for (const auto& Case : Cases)
	{
		TInlineBuffer<4> Buffer;
		FCborWriter Writer(Buffer);
		Writer.WriteInteger(Case.Content);
		REQUIRE(Buffer->GetSize() == Case.TruthSize);
		REQUIRE(memcmp(Buffer->GetData(), Case.Truth, Case.TruthSize) == 0);
	}
}

////////////////////////////////////////////////////////////////////////////////
static void TestCborWriter_String()
{
	struct
	{
		const char*	Value;
		uint32		TruthSize;
		const uint8	Truth[64];
	} Cases[] = {
		{ "string", 7, { 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67 } },
		{ "",		1, { 0x60 } },
		{
			"stringstringstringstringstringstringstringstringstringstring",
			62,
			{ 0x78, 0x3c,
			  0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			  0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			  0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			  0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
			  0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67 },
		},
	};

	for (const auto& Case : Cases)
	{
		TInlineBuffer<8> Buffer;
		FCborWriter Writer(Buffer);
		Writer.WriteString(Case.Value);
		REQUIRE(Buffer->GetSize() == Case.TruthSize);
		REQUIRE(memcmp(Buffer->GetData(), Case.Truth, Case.TruthSize) == 0);
	}
}

////////////////////////////////////////////////////////////////////////////////
void TestCborWriter_Map()
{
	// {}
	{
		const uint8 Truth[] = { 0xa0 };

		TInlineBuffer<4> Buffer;
		FCborWriter Writer(Buffer);
		Writer.OpenMap(0);

		REQUIRE(memcmp(Buffer->GetData(), Truth, sizeof(Truth)) == 0);
	}

	// {...}
	{
		const uint8 Truth[] = { 0xbf, 0xff };

		TInlineBuffer<4> Buffer;
		FCborWriter Writer(Buffer);
		Writer.OpenMap();
		Writer.Close();

		REQUIRE(memcmp(Buffer->GetData(), Truth, sizeof(Truth)) == 0);
	}

	// { "key":"value", 0:1 }
	{
		const uint8 Truth[] = {
			0xa2,
			0x63, 0x6b, 0x65, 0x79,
			0x65, 0x76, 0x61, 0x6c, 0x75, 0x65,
			0x00, 0x01
		};

		TInlineBuffer<4> Buffer;
		FCborWriter Writer(Buffer);
		Writer.OpenMap(2);
		Writer.WriteString("key");
		Writer.WriteString("value", 5);
		Writer.WriteInteger(0);
		Writer.WriteInteger(1);

		REQUIRE(memcmp(Buffer->GetData(), Truth, sizeof(Truth)) == 0);
	}
}



// {{{1 test -------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
void TestCbor()
{
	TestCborReader_Integer();
	TestCborReader_String();
	TestCborReader_Map();

	TestCborWriter_Integer();
	TestCborWriter_String();
	TestCborWriter_Map();

	// [-1]
	//const uint8 Truth[] = { 0x81, 0x20 };

	// float
	//const uint8 Truth[] = { 0xfb 0x3f 0xb9 0x99 0x99 0x99 0x99 0x99 0x9a }; // primitive(4591870180066957722)
}

/* vim: set noexpandtab foldlevel=1 : */
