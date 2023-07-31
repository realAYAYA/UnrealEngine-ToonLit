// Copyright Epic Games, Inc. All Rights Reserved.

#include "Serialization/CompactBinaryWriter.h"

#include "IO/IoHash.h"
#include "Misc/AutomationTest.h"
#include "Misc/DateTime.h"
#include "Misc/Guid.h"
#include "Misc/Timespan.h"
#include "Misc/StringBuilder.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Serialization/CompactBinaryValidation.h"

#if WITH_DEV_AUTOMATION_TESTS

static constexpr EAutomationTestFlags::Type CompactBinaryWriterTestFlags = EAutomationTestFlags::Type(EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterObjectTest, "System.Core.Serialization.CbWriter.Object", CompactBinaryWriterTestFlags)
bool FCbWriterObjectTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	// Test Empty Object
	{
		Writer.Reset();
		Writer.BeginObject();
		Writer.EndObject();
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Object, Empty) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestTrue(TEXT("FCbWriter(Object, Empty).IsObject()"), Field.IsObject());
			TestFalse(TEXT("FCbWriter(Object, Empty).AsObjectView()"), Field.AsObjectView().CreateViewIterator().HasValue());
		}
	}

	// Test Named Empty Object
	{
		Writer.Reset();
		Writer.SetName(ANSITEXTVIEW("Object"));
		Writer.BeginObject();
		Writer.EndObject();
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Object, Empty, Name) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestTrue(TEXT("FCbWriter(Object, Empty, Name).IsObject()"), Field.IsObject());
			TestFalse(TEXT("FCbWriter(Object, Empty, Name).AsObjectView()"), Field.AsObjectView().CreateViewIterator().HasValue());
		}
	}

	// Test Basic Object
	{
		Writer.Reset();
		Writer.BeginObject();
		Writer.SetName(ANSITEXTVIEW("Integer")).AddInteger(0);
		Writer.SetName(ANSITEXTVIEW("Float")).AddFloat(0.0f);
		Writer.EndObject();
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Object, Basic) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestTrue(TEXT("FCbWriter(Object, Basic).IsObject()"), Field.IsObject());
			FCbObjectView Object = Field.AsObjectView();
			TestTrue(TEXT("FCbWriter(Object, Basic).AsObjectView()[Integer]"), Object[ANSITEXTVIEW("Integer")].IsInteger());
			TestTrue(TEXT("FCbWriter(Object, Basic).AsObjectView()[Float]"), Object[ANSITEXTVIEW("Float")].IsFloat());
		}
	}

	// Test Uniform Object
	{
		Writer.Reset();
		Writer.BeginObject();
		Writer.SetName(ANSITEXTVIEW("Field1")).AddInteger(0);
		Writer.SetName(ANSITEXTVIEW("Field2")).AddInteger(1);
		Writer.EndObject();
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Object, Uniform) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestTrue(TEXT("FCbWriter(Object, Uniform).IsObject()"), Field.IsObject());
			FCbObjectView Object = Field.AsObjectView();
			TestTrue(TEXT("FCbWriter(Object, Uniform).AsObjectView()[Field1]"), Object[ANSITEXTVIEW("Field1")].IsInteger());
			TestTrue(TEXT("FCbWriter(Object, Uniform).AsObjectView()[Field2]"), Object[ANSITEXTVIEW("Field2")].IsInteger());
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterArrayTest, "System.Core.Serialization.CbWriter.Array", CompactBinaryWriterTestFlags)
bool FCbWriterArrayTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	// Test Empty Array
	{
		Writer.Reset();
		Writer.BeginArray();
		Writer.EndArray();
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Array, Empty) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestTrue(TEXT("FCbWriter(Array, Empty).IsArray()"), Field.IsArray());
			TestEqual(TEXT("FCbWriter(Array, Empty).AsArrayView()"), Field.AsArrayView().Num(), uint64(0));
		}
	}

	// Test Named Empty Array
	{
		Writer.Reset();
		Writer.SetName(ANSITEXTVIEW("Array"));
		Writer.BeginArray();
		Writer.EndArray();
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Array, Empty, Name) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestTrue(TEXT("FCbWriter(Array, Empty, Name).IsArray()"), Field.IsArray());
			TestEqual(TEXT("FCbWriter(Array, Empty, Name).AsArrayView()"), Field.AsArrayView().Num(), uint64(0));
		}
	}

	// Test Basic Array
	{
		Writer.Reset();
		Writer.BeginArray();
		Writer.AddInteger(0);
		Writer.AddFloat(0.0f);
		Writer.EndArray();
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Array, Basic) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestTrue(TEXT("FCbWriter(Array, Basic).IsArray()"), Field.IsArray());
			FCbFieldViewIterator Iterator = Field.AsArrayView().CreateViewIterator();
			TestTrue(TEXT("FCbWriter(Array, Basic).AsArrayView()[Integer]"), Iterator.IsInteger());
			++Iterator;
			TestTrue(TEXT("FCbWriter(Array, Basic).AsArrayView()[Float]"), Iterator.IsFloat());
			++Iterator;
			TestFalse(TEXT("FCbWriter(Array, Basic).AsArrayView()[End]"), Iterator.HasValue());
		}
	}

	// Test Uniform Array
	{
		Writer.Reset();
		Writer.BeginArray();
		Writer.AddInteger(0);
		Writer.AddInteger(1);
		Writer.EndArray();
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Array, Uniform) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestTrue(TEXT("FCbWriter(Array, Uniform).IsArray()"), Field.IsArray());
			FCbFieldViewIterator Iterator = Field.AsArrayView().CreateViewIterator();
			TestTrue(TEXT("FCbWriter(Array, Basic).AsArrayView()[Field1]"), Iterator.IsInteger());
			++Iterator;
			TestTrue(TEXT("FCbWriter(Array, Basic).AsArrayView()[Field2]"), Iterator.IsInteger());
			++Iterator;
			TestFalse(TEXT("FCbWriter(Array, Basic).AsArrayView()[End]"), Iterator.HasValue());
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterNullTest, "System.Core.Serialization.CbWriter.Null", CompactBinaryWriterTestFlags)
bool FCbWriterNullTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	// Test Null
	{
		Writer.Reset();
		Writer.AddNull();
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Null) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestFalse(TEXT("FCbWriter(Null).HasName()"), Field.HasName());
			TestTrue(TEXT("FCbWriter(Null).IsNull()"), Field.IsNull());
		}
	}

	// Test Null with Name
	{
		Writer.Reset();
		Writer.SetName(ANSITEXTVIEW("Null"));
		Writer.AddNull();
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Null, Name) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestEqual(TEXT("FCbWriter(Null, Name).GetName()"), Field.GetName(), UTF8TEXTVIEW("Null"));
			TestTrue(TEXT("FCbWriter(Null, Name).HasName()"), Field.HasName());
			TestTrue(TEXT("FCbWriter(Null, Name).IsNull()"), Field.IsNull());
		}
	}

	// Test Null Array/Object Uniformity
	{
		Writer.Reset();

		Writer.BeginArray();
		Writer.AddNull();
		Writer.AddNull();
		Writer.AddNull();
		Writer.EndArray();

		Writer.BeginObject();
		Writer.SetName(ANSITEXTVIEW("N1")).AddNull();
		Writer.SetName(ANSITEXTVIEW("N2")).AddNull();
		Writer.SetName(ANSITEXTVIEW("N3")).AddNull();
		Writer.EndObject();

		FCbFieldIterator Fields = Writer.Save();
		TestEqual(TEXT("FCbWriter(Null, Uniform) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None);
	}

	// Test Null with Save(Buffer)
	{
		Writer.Reset();
		constexpr int NullCount = 3;
		for (int Index = 0; Index < NullCount; ++Index)
		{
			Writer.AddNull();
		}
		uint8 Buffer[NullCount]{};
		FCbFieldViewIterator Fields = Writer.Save(MakeMemoryView(Buffer));
		if (TestEqual(TEXT("FCbWriter(Null, Memory) Validate"), ValidateCompactBinaryRange(MakeMemoryView(Buffer), ECbValidateMode::All), ECbValidateError::None))
		{
			for (int Index = 0; Index < NullCount; ++Index)
			{
				TestTrue(TEXT("FCbWriter(Null, Memory) IsNull"), Fields.IsNull());
				++Fields;
			}
			TestFalse(TEXT("FCbWriter(Null, Memory) HasValue"), Fields.HasValue());
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FCbWriterBinaryTestBase : public FAutomationTestBase
{
protected:
	using FAutomationTestBase::FAutomationTestBase;
	using FAutomationTestBase::TestEqual;

	template <typename T, typename Size>
	void TestEqual(const TCHAR* What, TArrayView<T, Size> Actual, TArrayView<T, Size> Expected)
	{
		TestTrue(What, Actual.Num() == Expected.Num() && CompareItems(Actual.GetData(), Expected.GetData(), Actual.Num()));
	}
};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FCbWriterBinaryTest, FCbWriterBinaryTestBase, "System.Core.Serialization.CbWriter.Binary", CompactBinaryWriterTestFlags)
bool FCbWriterBinaryTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	// Test Empty Binary
	{
		Writer.Reset();
		Writer.AddBinary(nullptr, 0);
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Binary, Empty) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestFalse(TEXT("FCbWriter(Binary, Empty).HasName()"), Field.HasName());
			TestTrue(TEXT("FCbWriter(Binary, Empty).IsBinary()"), Field.IsBinary());
			TestTrue(TEXT("FCbWriter(Binary, Empty).AsBinaryView()"), Field.AsBinaryView().IsEmpty());
		}
	}

	// Test Basic Binary
	{
		Writer.Reset();
		const uint8 BinaryValue[] = { 1, 2, 3, 4, 5, 6 };
		Writer.SetName(ANSITEXTVIEW("Binary"));
		Writer.AddBinary(BinaryValue, sizeof(BinaryValue));
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Binary, Array) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestEqual(TEXT("FCbWriter(Binary, Array).GetName()"), Field.GetName(), UTF8TEXTVIEW("Binary"));
			TestTrue(TEXT("FCbWriter(Binary, Array).HasName()"), Field.HasName());
			TestTrue(TEXT("FCbWriter(Binary, Array).IsBinary()"), Field.IsBinary());
			TestTrue(TEXT("FCbWriter(Binary, Array).AsBinaryView()"), Field.AsBinaryView().EqualBytes(MakeMemoryView(BinaryValue)));
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterStringTest, "System.Core.Serialization.CbWriter.String", CompactBinaryWriterTestFlags)
bool FCbWriterStringTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	// Test Empty Strings
	{
		Writer.Reset();
		Writer.AddString(FAnsiStringView());
		Writer.AddString(FWideStringView());
		FCbFieldIterator Fields = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(String, Empty) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			for (FCbFieldView Field : Fields)
			{
				TestFalse(TEXT("FCbWriter(String, Empty).HasName()"), Field.HasName());
				TestTrue(TEXT("FCbWriter(String, Empty).IsString()"), Field.IsString());
				TestTrue(TEXT("FCbWriter(String, Empty).AsString()"), Field.AsString().IsEmpty());
			}
		}
	}

	// Test Basic Strings
	{
		Writer.Reset();
		Writer.SetName(ANSITEXTVIEW("String")).AddString(ANSITEXTVIEW("Value"));
		Writer.SetName(ANSITEXTVIEW("String")).AddString(TEXTVIEW("Value"));
		FCbFieldIterator Fields = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(String, Basic) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			for (FCbFieldView Field : Fields)
			{
				TestEqual(TEXT("FCbWriter(String, Basic).GetName()"), Field.GetName(), UTF8TEXTVIEW("String"));
				TestTrue(TEXT("FCbWriter(String, Basic).HasName()"), Field.HasName());
				TestTrue(TEXT("FCbWriter(String, Basic).IsString()"), Field.IsString());
				TestEqual(TEXT("FCbWriter(String, Basic).AsString()"), Field.AsString(), UTF8TEXTVIEW("Value"));
			}
		}
	}

	// Test Long Strings
	{
		Writer.Reset();
		constexpr int DotCount = 256;
		TUtf8StringBuilder<DotCount + 1> Dots;
		for (int Index = 0; Index < DotCount; ++Index)
		{
			Dots << '.';
		}
		Writer.AddString(Dots);
		Writer.AddString(FString::ChrN(DotCount, TEXT('.')));
		FCbFieldIterator Fields = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(String, Long) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			for (FCbFieldView Field : Fields)
			{
				TestEqual(TEXT("FCbWriter(String, Long).AsString()"), Field.AsString(), Dots.ToView());
			}
		}
	}

	// Test Non-ASCII String
	{
		Writer.Reset();
		WIDECHAR Value[2] = { 0xd83d, 0xde00 };
		Writer.AddString(ANSITEXTVIEW("\xf0\x9f\x98\x80"));
		Writer.AddString(FWideStringView(Value, UE_ARRAY_COUNT(Value)));
		FCbFieldIterator Fields = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(String, Unicode) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			for (FCbFieldView Field : Fields)
			{
				TestEqual(TEXT("FCbWriter(String, Unicode).AsString()"), Field.AsString(), UTF8TEXTVIEW("\xf0\x9f\x98\x80"));
			}
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterIntegerTest, "System.Core.Serialization.CbWriter.Integer", CompactBinaryWriterTestFlags)
bool FCbWriterIntegerTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	auto TestInt32 = [this, &Writer](int32 Value)
	{
		Writer.Reset();
		Writer.AddInteger(Value);
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Integer, Int32) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestEqual(TEXT("FCbWriter(Integer, Int32) Value"), Field.AsInt32(), Value);
			TestFalse(TEXT("FCbWriter(Integer, Int32) Error"), Field.HasError());
		}
	};

	auto TestUInt32 = [this, &Writer](uint32 Value)
	{
		Writer.Reset();
		Writer.AddInteger(Value);
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Integer, UInt32) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestEqual(TEXT("FCbWriter(Integer, UInt32) Value"), Field.AsUInt32(), Value);
			TestFalse(TEXT("FCbWriter(Integer, UInt32) Error"), Field.HasError());
		}
	};

	auto TestInt64 = [this, &Writer](int64 Value)
	{
		Writer.Reset();
		Writer.AddInteger(Value);
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Integer, Int64) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestEqual(TEXT("FCbWriter(Integer, Int64) Value"), Field.AsInt64(), Value);
			TestFalse(TEXT("FCbWriter(Integer, Int64) Error"), Field.HasError());
		}
	};

	auto TestUInt64 = [this, &Writer](uint64 Value)
	{
		Writer.Reset();
		Writer.AddInteger(Value);
		FCbField Field = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Integer, UInt64) Validate"), ValidateCompactBinary(Field.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestEqual(TEXT("FCbWriter(Integer, UInt64) Value"), Field.AsUInt64(), Value);
			TestFalse(TEXT("FCbWriter(Integer, UInt64) Error"), Field.HasError());
		}
	};

	TestUInt32(uint32(0x00));
	TestUInt32(uint32(0x7f));
	TestUInt32(uint32(0x80));
	TestUInt32(uint32(0xff));
	TestUInt32(uint32(0x0100));
	TestUInt32(uint32(0x7fff));
	TestUInt32(uint32(0x8000));
	TestUInt32(uint32(0xffff));
	TestUInt32(uint32(0x0001'0000));
	TestUInt32(uint32(0x7fff'ffff));
	TestUInt32(uint32(0x8000'0000));
	TestUInt32(uint32(0xffff'ffff));

	TestUInt64(uint64(0x0000'0001'0000'0000));
	TestUInt64(uint64(0x7fff'ffff'ffff'ffff));
	TestUInt64(uint64(0x8000'0000'0000'0000));
	TestUInt64(uint64(0xffff'ffff'ffff'ffff));

	TestInt32(int32(0x01));
	TestInt32(int32(0x80));
	TestInt32(int32(0x81));
	TestInt32(int32(0x8000));
	TestInt32(int32(0x8001));
	TestInt32(int32(0x7fff'ffff));
	TestInt32(int32(0x8000'0000));
	TestInt32(int32(0x8000'0001));

	TestInt64(int64(0x0000'0001'0000'0000));
	TestInt64(int64(0x8000'0000'0000'0000));
	TestInt64(int64(0x7fff'ffff'ffff'ffff));
	TestInt64(int64(0x8000'0000'0000'0001));
	TestInt64(int64(0xffff'ffff'ffff'ffff));

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterFloatTest, "System.Core.Serialization.CbWriter.Float", CompactBinaryWriterTestFlags)
bool FCbWriterFloatTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	// Test Float32
	{
		Writer.Reset();
		constexpr float Values[] = { 0.0f, 1.0f, -1.0f, UE_PI };
		for (float Value : Values)
		{
			Writer.AddFloat(Value);
		}
		FCbFieldIterator Fields = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Float, Single) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			const float* CheckValue = Values;
			for (FCbFieldView Field : Fields)
			{
				TestEqual(TEXT("FCbWriter(Float, Single).AsFloat()"), Field.AsFloat(), *CheckValue++);
				TestFalse(TEXT("FCbWriter(Float, Single) Error"), Field.HasError());
			}
		}
	}

	// Test Float64
	{
		Writer.Reset();
		constexpr double Values[] = { 0.0f, 1.0f, -1.0f, UE_PI, 1.9999998807907104, 1.9999999403953552, 3.4028234663852886e38, 6.8056469327705771e38 };
		for (double Value : Values)
		{
			Writer.AddFloat(Value);
		}
		FCbFieldIterator Fields = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Float, Double) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			const double* CheckValue = Values;
			for (FCbFieldView Field : Fields)
			{
				TestEqual(TEXT("FCbWriter(Float, Double).AsDouble()"), Field.AsDouble(), *CheckValue++);
				TestFalse(TEXT("FCbWriter(Float, Double) Error"), Field.HasError());
			}
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterBoolTest, "System.Core.Serialization.CbWriter.Bool", CompactBinaryWriterTestFlags)
bool FCbWriterBoolTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	// Test Bool Values
	{
		Writer.AddBool(true);
		Writer.AddBool(false);

		FCbFieldIterator Fields = Writer.Save();
		if (TestEqual(TEXT("FCbWriter(Bool) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
		{
			TestTrue(TEXT("FCbWriter(Bool).AsBool()"), Fields.AsBool());
			TestFalse(TEXT("FCbWriter(Bool) Error"), Fields.HasError());
			++Fields;
			TestFalse(TEXT("FCbWriter(Bool).AsBool()"), Fields.AsBool());
			TestFalse(TEXT("FCbWriter(Bool) Error"), Fields.HasError());
		}
	}

	// Test Bool Array/Object Uniformity
	{
		Writer.Reset();

		Writer.BeginArray();
		Writer.AddBool(false);
		Writer.AddBool(false);
		Writer.AddBool(false);
		Writer.EndArray();

		Writer.BeginObject();
		Writer.SetName(ANSITEXTVIEW("B1")).AddBool(false);
		Writer.SetName(ANSITEXTVIEW("B2")).AddBool(false);
		Writer.SetName(ANSITEXTVIEW("B3")).AddBool(false);
		Writer.EndObject();

		FCbFieldIterator Fields = Writer.Save();
		TestEqual(TEXT("FCbWriter(Bool, Uniform) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None);
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterObjectAttachmentTest, "System.Core.Serialization.CbWriter.ObjectAttachment", CompactBinaryWriterTestFlags)
bool FCbWriterObjectAttachmentTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	const FIoHash::ByteArray ZeroBytes{};
	const FIoHash::ByteArray SequentialBytes{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

	const FIoHash Values[] = { FIoHash(ZeroBytes), FIoHash(SequentialBytes) };
	for (const FIoHash& Value : Values)
	{
		Writer.AddObjectAttachment(Value);
	}

	FCbFieldIterator Fields = Writer.Save();
	if (TestEqual(TEXT("FCbWriter(ObjectAttachment) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
	{
		const FIoHash* CheckValue = Values;
		for (FCbFieldView Field : Fields)
		{
			TestEqual(TEXT("FCbWriter(ObjectAttachment).AsObjectAttachment()"), Field.AsObjectAttachment(), *CheckValue++);
			TestFalse(TEXT("FCbWriter(ObjectAttachment) Error"), Field.HasError());
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterBinaryAttachmentTest, "System.Core.Serialization.CbWriter.BinaryAttachment", CompactBinaryWriterTestFlags)
bool FCbWriterBinaryAttachmentTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	const FIoHash::ByteArray ZeroBytes{};
	const FIoHash::ByteArray SequentialBytes{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

	const FIoHash Values[] = { FIoHash(ZeroBytes), FIoHash(SequentialBytes) };
	for (const FIoHash& Value : Values)
	{
		Writer.AddBinaryAttachment(Value);
	}

	FCbFieldIterator Fields = Writer.Save();
	if (TestEqual(TEXT("FCbWriter(BinaryAttachment) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
	{
		const FIoHash* CheckValue = Values;
		for (FCbFieldView Field : Fields)
		{
			TestEqual(TEXT("FCbWriter(BinaryAttachment).AsBinaryAttachment()"), Field.AsBinaryAttachment(), *CheckValue++);
			TestFalse(TEXT("FCbWriter(BinaryAttachment) Error"), Field.HasError());
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterHashTest, "System.Core.Serialization.CbWriter.Hash", CompactBinaryWriterTestFlags)
bool FCbWriterHashTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	const FIoHash::ByteArray ZeroBytes{};
	const FIoHash::ByteArray SequentialBytes{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};

	const FIoHash Values[] = { FIoHash(ZeroBytes), FIoHash(SequentialBytes) };
	for (const FIoHash& Value : Values)
	{
		Writer.AddHash(Value);
	}

	FCbFieldIterator Fields = Writer.Save();
	if (TestEqual(TEXT("FCbWriter(Hash) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
	{
		const FIoHash* CheckValue = Values;
		for (FCbFieldView Field : Fields)
		{
			TestEqual(TEXT("FCbWriter(Hash).AsHash()"), Field.AsHash(), *CheckValue++);
			TestFalse(TEXT("FCbWriter(Hash) Error"), Field.HasError());
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterUuidTest, "System.Core.Serialization.CbWriter.Uuid", CompactBinaryWriterTestFlags)
bool FCbWriterUuidTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	const FGuid Values[] = { FGuid(), FGuid::NewGuid() };
	for (const FGuid& Value : Values)
	{
		Writer.AddUuid(Value);
	}

	FCbFieldIterator Fields = Writer.Save();
	if (TestEqual(TEXT("FCbWriter(Uuid) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
	{
		const FGuid* CheckValue = Values;
		for (FCbFieldView Field : Fields)
		{
			TestEqual(TEXT("FCbWriter(Uuid).AsUuid()"), Field.AsUuid(), *CheckValue++);
			TestFalse(TEXT("FCbWriter(Uuid) Error"), Field.HasError());
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterDateTimeTest, "System.Core.Serialization.CbWriter.DateTime", CompactBinaryWriterTestFlags)
bool FCbWriterDateTimeTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	const FDateTime Values[] = { FDateTime(0), FDateTime(2020, 5, 13, 15, 10) };
	for (FDateTime Value : Values)
	{
		Writer.AddDateTime(Value);
	}

	FCbFieldIterator Fields = Writer.Save();
	if (TestEqual(TEXT("FCbWriter(DateTime) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
	{
		const FDateTime* CheckValue = Values;
		for (FCbFieldView Field : Fields)
		{
			TestEqual(TEXT("FCbWriter(DateTime).AsDateTime()"), Field.AsDateTime(), *CheckValue++);
			TestFalse(TEXT("FCbWriter(DateTime) Error"), Field.HasError());
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterTimeSpanTest, "System.Core.Serialization.CbWriter.TimeSpan", CompactBinaryWriterTestFlags)
bool FCbWriterTimeSpanTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	const FTimespan Values[] = { FTimespan(0), FTimespan(1, 2, 4, 8) };
	for (FTimespan Value : Values)
	{
		Writer.AddTimeSpan(Value);
	}

	FCbFieldIterator Fields = Writer.Save();
	if (TestEqual(TEXT("FCbWriter(TimeSpan) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
	{
		const FTimespan* CheckValue = Values;
		for (FCbFieldView Field : Fields)
		{
			TestEqual(TEXT("FCbWriter(TimeSpan).AsTimeSpan()"), Field.AsTimeSpan(), *CheckValue++);
			TestFalse(TEXT("FCbWriter(TimeSpan) Error"), Field.HasError());
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterObjectIdTest, "System.Core.Serialization.CbWriter.ObjectId", CompactBinaryWriterTestFlags)
bool FCbWriterObjectIdTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	const FCbObjectId Values[] = { FCbObjectId(), FCbObjectId(MakeMemoryView<uint8>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12})) };
	for (const FCbObjectId& Value : Values)
	{
		Writer.AddObjectId(Value);
	}

	FCbFieldIterator Fields = Writer.Save();
	if (TestEqual(TEXT("FCbWriter(ObjectId) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
	{
		const FCbObjectId* CheckValue = Values;
		for (FCbFieldView Field : Fields)
		{
			TestEqual(TEXT("FCbWriter(ObjectId).AsObjectId()"), Field.AsObjectId(), *CheckValue++);
			TestFalse(TEXT("FCbWriter(ObjectId) Error"), Field.HasError());
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterCustomByIdTest, "System.Core.Serialization.CbWriter.CustomById", CompactBinaryWriterTestFlags)
bool FCbWriterCustomByIdTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	struct FCustomValue
	{
		uint64 Type;
		TArray<uint8, TInlineAllocator<16>> Bytes;
	};
	const FCustomValue Values[] =
	{
		{ 1, {1, 2, 3} },
		{ MAX_uint64, {4, 5, 6} },
	};

	for (const FCustomValue& Value : Values)
	{
		Writer.AddCustom(Value.Type, MakeMemoryView(Value.Bytes));
	}

	FCbFieldIterator Fields = Writer.Save();
	if (TestEqual(TEXT("FCbWriter(CustomById) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
	{
		const FCustomValue* CheckValue = Values;
		for (FCbFieldView Field : Fields)
		{
			TestTrue(TEXT("FCbWriter(CustomById).AsCustom()"), Field.AsCustom(CheckValue->Type).EqualBytes(MakeMemoryView(CheckValue->Bytes)));
			TestFalse(TEXT("FCbWriter(CustomById) Error"), Field.HasError());
			++CheckValue;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterCustomByNameTest, "System.Core.Serialization.CbWriter.CustomByName", CompactBinaryWriterTestFlags)
bool FCbWriterCustomByNameTest::RunTest(const FString& Parameters)
{
	TCbWriter<256> Writer;

	struct FCustomValue
	{
		FUtf8StringView Type;
		TArray<uint8, TInlineAllocator<16>> Bytes;
	};
	const FCustomValue Values[] =
	{
		{ UTF8TEXTVIEW("Type1"), {1, 2, 3} },
		{ UTF8TEXTVIEW("Type2"), {4, 5, 6} },
	};

	for (const FCustomValue& Value : Values)
	{
		Writer.AddCustom(Value.Type, MakeMemoryView(Value.Bytes));
	}

	FCbFieldIterator Fields = Writer.Save();
	if (TestEqual(TEXT("FCbWriter(CustomByName) Validate"), ValidateCompactBinaryRange(Fields.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None))
	{
		const FCustomValue* CheckValue = Values;
		for (FCbFieldView Field : Fields)
		{
			TestTrue(TEXT("FCbWriter(CustomByName).AsCustom()"), Field.AsCustom(CheckValue->Type).EqualBytes(MakeMemoryView(CheckValue->Bytes)));
			TestFalse(TEXT("FCbWriter(CustomByName) Error"), Field.HasError());
			++CheckValue;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterComplexTest, "System.Core.Serialization.CbWriter.Complex", CompactBinaryWriterTestFlags)
bool FCbWriterComplexTest::RunTest(const FString& Parameters)
{
	FCbObject Object;
	FBufferArchive Archive;
	{
		FCbWriter Writer;
		Writer.BeginObject();

		const uint8 LocalField[] = { uint8(ECbFieldType::IntegerPositive | ECbFieldType::HasFieldName), 1, 'I', 42 };
		Writer.AddField(ANSITEXTVIEW("FieldViewCopy"), FCbFieldView(LocalField));
		Writer.AddField(ANSITEXTVIEW("FieldCopy"), FCbField(FSharedBuffer::Clone(MakeMemoryView(LocalField))));

		const uint8 LocalObject[] = { uint8(ECbFieldType::Object | ECbFieldType::HasFieldName), 1, 'O', 7,
			uint8(ECbFieldType::IntegerPositive | ECbFieldType::HasFieldName), 1, 'I', 42,
			uint8(ECbFieldType::Null | ECbFieldType::HasFieldName), 1, 'N' };
		Writer.AddObject(ANSITEXTVIEW("ObjectViewCopy"), FCbObjectView(LocalObject));
		Writer.AddObject(ANSITEXTVIEW("ObjectCopy"), FCbObject(FSharedBuffer::Clone(MakeMemoryView(LocalObject))));

		const uint8 LocalArray[] = { uint8(ECbFieldType::UniformArray | ECbFieldType::HasFieldName), 1, 'A', 4, 2,
			uint8(ECbFieldType::IntegerPositive), 42, 21 };
		Writer.AddArray(ANSITEXTVIEW("ArrayViewCopy"), FCbArrayView(LocalArray));
		Writer.AddArray(ANSITEXTVIEW("ArrayCopy"), FCbArray(FSharedBuffer::Clone(MakeMemoryView(LocalArray))));

		Writer.AddNull(ANSITEXTVIEW("Null"));

		Writer.BeginObject(ANSITEXTVIEW("Binary"));
		{
			Writer.AddBinary(ANSITEXTVIEW("Empty"), FMemoryView());
			Writer.AddBinary(ANSITEXTVIEW("Value"), MakeMemoryView("BinaryValue"));
			Writer.AddBinary(ANSITEXTVIEW("LargeViewValue"), MakeMemoryView(FString::ChrN(256, TEXT('.'))));
			Writer.AddBinary(ANSITEXTVIEW("LargeValue"), FSharedBuffer::Clone(MakeMemoryView(FString::ChrN(256, TEXT('!')))));
		}
		Writer.EndObject();

		Writer.BeginObject(ANSITEXTVIEW("Strings"));
		{
			Writer.AddString(ANSITEXTVIEW("AnsiString"), ANSITEXTVIEW("AnsiValue"));
			Writer.AddString(ANSITEXTVIEW("WideString"), FString::ChrN(256, TEXT('.')));
			Writer.AddString(ANSITEXTVIEW("EmptyAnsiString"), FAnsiStringView());
			Writer.AddString(ANSITEXTVIEW("EmptyWideString"), FWideStringView());
			Writer.AddString(UTF8TEXTVIEW("EmptyUtf8String"), FUtf8StringView());
			Writer.AddString("AnsiStringLiteral", "AnsiValue");
			Writer.AddString("WideStringLiteral", TEXT("AnsiValue"));
		}
		Writer.EndObject();

		Writer.BeginArray(ANSITEXTVIEW("Integers"));
		{
			Writer.AddInteger(int32(-1));
			Writer.AddInteger(int64(-1));
			Writer.AddInteger(uint32(1));
			Writer.AddInteger(uint64(1));
			Writer.AddInteger(MIN_int32);
			Writer.AddInteger(MAX_int32);
			Writer.AddInteger(MAX_uint32);
			Writer.AddInteger(MIN_int64);
			Writer.AddInteger(MAX_int64);
			Writer.AddInteger(MAX_uint64);
		}
		Writer.EndArray();

		Writer.BeginArray(ANSITEXTVIEW("UniformIntegers"));
		{
			Writer.AddInteger(0);
			Writer.AddInteger(MAX_int32);
			Writer.AddInteger(MAX_uint32);
			Writer.AddInteger(MAX_int64);
			Writer.AddInteger(MAX_uint64);
		}
		Writer.EndArray();

		Writer.AddFloat(ANSITEXTVIEW("Float32"), 1.0f);
		Writer.AddFloat(ANSITEXTVIEW("Float64as32"), 2.0);
		Writer.AddFloat(ANSITEXTVIEW("Float64"), 3.0e100);

		Writer.AddBool(ANSITEXTVIEW("False"), false);
		Writer.AddBool(ANSITEXTVIEW("True"), true);

		Writer.AddObjectAttachment(ANSITEXTVIEW("ObjectAttachment"), FIoHash());
		Writer.AddBinaryAttachment(ANSITEXTVIEW("BinaryAttachment"), FIoHash());
		Writer.AddAttachment(ANSITEXTVIEW("Attachment"), FCbAttachment());

		Writer.AddHash(ANSITEXTVIEW("Hash"), FIoHash());
		Writer.AddUuid(ANSITEXTVIEW("Uuid"), FGuid());

		Writer.AddDateTimeTicks(ANSITEXTVIEW("DateTimeZero"), 0);
		Writer.AddDateTime(ANSITEXTVIEW("DateTime2020"), FDateTime(2020, 5, 13, 15, 10));

		Writer.AddTimeSpanTicks(ANSITEXTVIEW("TimeSpanZero"), 0);
		Writer.AddTimeSpan(ANSITEXTVIEW("TimeSpan"), FTimespan(1, 2, 4, 8));

		Writer.AddObjectId(ANSITEXTVIEW("ObjectIdZero"), FCbObjectId());
		Writer.AddObjectId(ANSITEXTVIEW("ObjectId"), FCbObjectId(MakeMemoryView<uint8>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12})));

		Writer.BeginObject(ANSITEXTVIEW("NestedObjects"));
		{
			Writer.BeginObject(ANSITEXTVIEW("Empty"));
			Writer.EndObject();

			Writer.BeginObject(ANSITEXTVIEW("Null"));
			Writer.AddNull(ANSITEXTVIEW("Null"));
			Writer.EndObject();
		}
		Writer.EndObject();

		Writer.BeginArray(ANSITEXTVIEW("NestedArrays"));
		{
			Writer.BeginArray();
			Writer.EndArray();

			Writer.BeginArray();
			Writer.AddNull();
			Writer.AddNull();
			Writer.AddNull();
			Writer.EndArray();

			Writer.BeginArray();
			Writer.AddBool(false);
			Writer.AddBool(false);
			Writer.AddBool(false);
			Writer.EndArray();

			Writer.BeginArray();
			Writer.AddBool(true);
			Writer.AddBool(true);
			Writer.AddBool(true);
			Writer.EndArray();
		}
		Writer.EndArray();

		Writer.BeginArray(ANSITEXTVIEW("ArrayOfObjects"));
		{
			Writer.BeginObject();
			Writer.EndObject();

			Writer.BeginObject();
			Writer.AddNull(ANSITEXTVIEW("Null"));
			Writer.EndObject();
		}
		Writer.EndArray();

		Writer.BeginArray(ANSITEXTVIEW("LargeArray"));
		for (int Index = 0; Index < 256; ++Index)
		{
			Writer.AddInteger(Index - 128);
		}
		Writer.EndArray();

		Writer.BeginArray(ANSITEXTVIEW("LargeUniformArray"));
		for (int Index = 0; Index < 256; ++Index)
		{
			Writer.AddInteger(Index);
		}
		Writer.EndArray();

		Writer.BeginArray(ANSITEXTVIEW("NestedUniformArray"));
		for (int Index = 0; Index < 16; ++Index)
		{
			Writer.BeginArray();
			for (int Value = 0; Value < 4; ++Value)
			{
				Writer.AddInteger(Value);
			}
			Writer.EndArray();
		}
		Writer.EndArray();

		Writer.EndObject();
		Object = Writer.Save().AsObject();

		Writer.Save(Archive);
		TestEqual(TEXT("FCbWriter(Complex).Save(Ar)->Num()"), uint64(Archive.Num()), Writer.GetSaveSize());
	}

	TestEqual(TEXT("FCbWriter(Complex).Save()->Validate"),
		ValidateCompactBinary(Object.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None);

	TestEqual(TEXT("FCbWriter(Complex).Save(Ar)->Validate"),
		ValidateCompactBinary(MakeMemoryView(Archive), ECbValidateMode::All), ECbValidateError::None);

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterOwnedReadOnlyTest, "System.Core.Serialization.CbWriter.OwnedReadOnly", CompactBinaryWriterTestFlags)
bool FCbWriterOwnedReadOnlyTest::RunTest(const FString& Parameters)
{
	FCbWriter Writer;
	Writer.BeginObject();
	Writer.EndObject();
	FCbObject Object = Writer.Save().AsObject();
	TestTrue(TEXT("FCbWriter().Save().IsOwned()"), Object.IsOwned());

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterStreamTest, "System.Core.Serialization.CbWriter.Stream", CompactBinaryWriterTestFlags)
bool FCbWriterStreamTest::RunTest(const FString& Parameters)
{
	FCbObject Object;
	{
		FCbWriter Writer;
		Writer.BeginObject();

		const uint8 LocalField[] = { uint8(ECbFieldType::IntegerPositive | ECbFieldType::HasFieldName), 1, 'I', 42 };
		Writer << ANSITEXTVIEW("FieldCopy") << FCbFieldView(LocalField);

		const uint8 LocalObject[] = { uint8(ECbFieldType::Object | ECbFieldType::HasFieldName), 1, 'O', 7,
			uint8(ECbFieldType::IntegerPositive | ECbFieldType::HasFieldName), 1, 'I', 42,
			uint8(ECbFieldType::Null | ECbFieldType::HasFieldName), 1, 'N' };
		Writer << ANSITEXTVIEW("ObjectCopy") << FCbObjectView(LocalObject);

		const uint8 LocalArray[] = { uint8(ECbFieldType::UniformArray | ECbFieldType::HasFieldName), 1, 'A', 4, 2,
			uint8(ECbFieldType::IntegerPositive), 42, 21 };
		Writer << ANSITEXTVIEW("ArrayCopy") << FCbArrayView(LocalArray);

		Writer << ANSITEXTVIEW("Null") << nullptr;

		Writer << ANSITEXTVIEW("Strings");
		Writer.BeginObject();
		Writer
			<< ANSITEXTVIEW("AnsiString") << ANSITEXTVIEW("AnsiValue")
			<< ANSITEXTVIEW("AnsiStringLiteral") << "AnsiValue"
			<< ANSITEXTVIEW("WideString") << TEXTVIEW("WideValue")
			<< ANSITEXTVIEW("WideStringLiteral") << TEXT("WideValue");
		Writer.EndObject();

		Writer << ANSITEXTVIEW("Integers");
		Writer.BeginArray();
		Writer << int32(-1) << int64(-1) << uint32(1) << uint64(1);
		Writer.EndArray();

		Writer << ANSITEXTVIEW("Float32") << 1.0f;
		Writer << ANSITEXTVIEW("Float64") << 2.0;

		Writer << ANSITEXTVIEW("False") << false << ANSITEXTVIEW("True") << true;

		Writer << ANSITEXTVIEW("Attachment") << FCbAttachment();

		Writer << ANSITEXTVIEW("Hash") << FIoHash();
		Writer << ANSITEXTVIEW("Uuid") << FGuid();

		Writer << ANSITEXTVIEW("DateTime") << FDateTime(2020, 5, 13, 15, 10);
		Writer << ANSITEXTVIEW("TimeSpan") << FTimespan(1, 2, 4, 8);

		Writer << ANSITEXTVIEW("ObjectIdZero") << FCbObjectId();
		Writer << ANSITEXTVIEW("ObjectId") << FCbObjectId(MakeMemoryView<uint8>({1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12}));

		Writer << "LiteralName" << nullptr;

		Writer.EndObject();
		Object = Writer.Save().AsObject();
	}

	TestEqual(TEXT("FCbWriter(Stream) Validate"), ValidateCompactBinary(Object.GetOuterBuffer(), ECbValidateMode::All), ECbValidateError::None);

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCbWriterStateTest, "System.Core.Serialization.CbWriter.State", CompactBinaryWriterTestFlags)
bool FCbWriterStateTest::RunTest(const FString& Parameters)
{
	FCbWriter Writer;

	// Assert on saving an empty writer.
	//uint8 EmptyField[1];
	//Writer.Reset();
	//Writer.Save(MakeMemoryView(EmptyField));

	// Assert on under-sized save buffer.
	//uint8 ZeroFieldSmall[1];
	//Writer.Reset();
	//Writer.AddInteger(0);
	//Writer.Save(MakeMemoryView(ZeroFieldSmall));

	// Assert on over-sized save buffer.
	//uint8 ZeroFieldLarge[3];
	//Writer.Reset();
	//Writer.AddInteger(0);
	//Writer.Save(MakeMemoryView(ZeroFieldLarge));

	// Assert on empty name.
	//Writer.SetName(ANSITEXTVIEW(""));

	// Assert on name after name.
	//Writer.SetName(ANSITEXTVIEW("Field")).SetName(ANSITEXTVIEW("Field"));

	// Assert on missing name.
	//Writer.BeginObject();
	//Writer.AddNull();
	//Writer.EndObject();

	// Assert on name in array.
	//Writer.BeginArray();
	//Writer.SetName(ANSITEXTVIEW("Field"));
	//Writer.EndArray();

	// Assert on save in object.
	//uint8 InvalidObject[1];
	//Writer.Reset();
	//Writer.BeginObject();
	//Writer.Save(MakeMemoryView(InvalidObject));
	//Writer.EndObject();

	// Assert on save in array.
	//uint8 InvalidArray[1];
	//Writer.Reset();
	//Writer.BeginArray();
	//Writer.Save(MakeMemoryView(InvalidArray));
	//Writer.EndArray();

	// Assert on object end with no begin.
	//Writer.EndObject();

	// Assert on array end with no begin.
	//Writer.EndArray();

	// Assert on object end after name with no value.
	//Writer.BeginObject();
	//Writer.SetName(ANSITEXTVIEW("Field"));
	//Writer.EndObject();

	// Assert on writing a field with no value.
	//Writer.Field(FCbFieldView());

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // WITH_DEV_AUTOMATION_TESTS
