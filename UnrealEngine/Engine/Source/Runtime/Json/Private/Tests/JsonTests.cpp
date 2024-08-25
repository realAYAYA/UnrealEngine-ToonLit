// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 * FJsonAutomationTest
 * Simple unit test that runs Json's in-built test cases
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FJsonAutomationTest, "System.Engine.FileSystem.JSON", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter )

typedef TJsonWriterFactory< TCHAR, TCondensedJsonPrintPolicy<TCHAR> > FCondensedJsonStringWriterFactory;
typedef TJsonWriter< TCHAR, TCondensedJsonPrintPolicy<TCHAR> > FCondensedJsonStringWriter;

typedef TJsonWriterFactory< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > FPrettyJsonStringWriterFactory;
typedef TJsonWriter< TCHAR, TPrettyJsonPrintPolicy<TCHAR> > FPrettyJsonStringWriter;

/** 
 * Execute the Json test cases
 *
 * @return	true if the test was successful, false otherwise
 */
bool FJsonAutomationTest::RunTest(const FString& Parameters)
{
	// Null Case
	{
		const FString InputString = TEXT("");
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InputString );

		TSharedPtr<FJsonObject> Object;
		verify( FJsonSerializer::Deserialize( Reader, Object ) == false );
		check( !Object.IsValid() );
	}

	// Empty Object Case
	{
		const FString InputString = TEXT("{}");
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InputString );

		TSharedPtr<FJsonObject> Object;
		verify( FJsonSerializer::Deserialize( Reader, Object ) );
		check( Object.IsValid() );

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create( &OutputString );
		verify( FJsonSerializer::Serialize( Object.ToSharedRef(), Writer ) );
		check( InputString == OutputString );
	}

	// Empty Array Case
	{
		const FString InputString = TEXT("[]");
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InputString );

		TArray< TSharedPtr<FJsonValue> > Array;
		verify( FJsonSerializer::Deserialize( Reader, Array ) );
		check( Array.Num() == 0 );

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create( &OutputString );
		verify( FJsonSerializer::Serialize( Array, Writer ) );
		check( InputString == OutputString );
	}

	// Empty Array with Empty Identifier Case
	{
		const FString ExpectedString = TEXT("[]");
		FString OutputString;
		TSharedRef<FJsonValueArray> EmptyValuesArray = MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>());
		TSharedRef<FCondensedJsonStringWriter> JsonWriter = FCondensedJsonStringWriterFactory::Create( &OutputString );
		verify(FJsonSerializer::Serialize(EmptyValuesArray, FString(), JsonWriter));
		check(ExpectedString == OutputString);
	}

	// Serializing Object Value with Empty Identifier Case
	{
		const FString ExpectedString = TEXT("{\"\":\"foo\"}");
		FString OutputString;
		TSharedRef<FJsonValue> FooValue = MakeShared<FJsonValueString>("foo");
		TSharedRef<FCondensedJsonStringWriter> JsonWriter = FCondensedJsonStringWriterFactory::Create(&OutputString);
		JsonWriter->WriteObjectStart();
		verify(FJsonSerializer::Serialize(FooValue, FString(), JsonWriter, false));
		JsonWriter->WriteObjectEnd();
		JsonWriter->Close();
		check(ExpectedString == OutputString);
	}

	// Simple Array Case
	{
		const FString InputString = 
			TEXT(
				"["
					"{"
						"\"Value\":\"Some String\""
					"}"
				"]"
			);

		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InputString );

		TArray< TSharedPtr<FJsonValue> > Array;
		bool bSuccessful = FJsonSerializer::Deserialize(Reader, Array);
		check(bSuccessful);
		check( Array.Num() == 1 );
		check( Array[0].IsValid() );

		TSharedPtr< FJsonObject > Object = Array[0]->AsObject();
		check( Object.IsValid() );
		check( Object->GetStringField( TEXT("Value") ) == TEXT("Some String") );

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create( &OutputString );
		verify( FJsonSerializer::Serialize( Array, Writer ) );
		check( InputString == OutputString );
	}

	// Object Array Case
	{
		const FString InputString =
			TEXT(
				"["
					"{"
						"\"Value\":\"Some String1\""
					"},"
					"{"
						"\"Value\":\"Some String2\""
					"},"
					"{"
						"\"Value\":\"Some String3\""
					"}"
				"]"
			);

		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(InputString);

		TArray< TSharedPtr<FJsonValue> > Array;

		bool bSuccessful = FJsonSerializer::Deserialize(Reader, Array);
		check(bSuccessful);
		check(Array.Num() == 3);
		check(Array[0].IsValid());
		check(Array[1].IsValid());
		check(Array[2].IsValid());

		TSharedPtr< FJsonObject > Object = Array[0]->AsObject();
		check(Object.IsValid());
		check(Object->GetStringField(TEXT("Value")) == TEXT("Some String1"));

		Object = Array[1]->AsObject();
		check(Object.IsValid());
		check(Object->GetStringField(TEXT("Value")) == TEXT("Some String2"));

		Object = Array[2]->AsObject();
		check(Object.IsValid());
		check(Object->GetStringField(TEXT("Value")) == TEXT("Some String3"));

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create(&OutputString);
		check(FJsonSerializer::Serialize(Array, Writer));
		check(InputString == OutputString);
	}

	// Number Array Case
	{
		const FString InputString =
			TEXT(
				"["
					"10,"
					"20,"
					"30,"
					"40"
				"]"
			);

		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(InputString);

		TArray< TSharedPtr<FJsonValue> > Array;
		bool bSuccessful = FJsonSerializer::Deserialize(Reader, Array);
		check(bSuccessful);
		check(Array.Num() == 4);
		check(Array[0].IsValid());
		check(Array[1].IsValid());
		check(Array[2].IsValid());
		check(Array[3].IsValid());

		double Number = Array[0]->AsNumber();
		check(Number == 10);

		Number = Array[1]->AsNumber();
		check(Number == 20);

		Number = Array[2]->AsNumber();
		check(Number == 30);

		Number = Array[3]->AsNumber();
		check(Number == 40);

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create(&OutputString);
		check(FJsonSerializer::Serialize(Array, Writer));
		check(InputString == OutputString);
	}

	// String Array Case
	{
		const FString InputString =
			TEXT(
				"["
					"\"Some String1\","
					"\"Some String2\","
					"\"Some String3\","
					"\"Some String4\""
				"]"
			);

		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(InputString);

		TArray< TSharedPtr<FJsonValue> > Array;
		bool bSuccessful = FJsonSerializer::Deserialize(Reader, Array);
		check(bSuccessful);
		check(Array.Num() == 4);
		check(Array[0].IsValid());
		check(Array[1].IsValid());
		check(Array[2].IsValid());
		check(Array[3].IsValid());

		FString Text = Array[0]->AsString();
		check(Text == TEXT("Some String1"));

		Text = Array[1]->AsString();
		check(Text == TEXT("Some String2"));

		Text = Array[2]->AsString();
		check(Text == TEXT("Some String3"));

		Text = Array[3]->AsString();
		check(Text == TEXT("Some String4"));

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create(&OutputString);
		check(FJsonSerializer::Serialize(Array, Writer));
		check(InputString == OutputString);
	}

	// Complex Array Case
	{
		const FString InputString =
			TEXT(
				"["
					"\"Some String1\","
					"10,"
					"{"
						"\"\":\"Empty Key\","
						"\"Value\":\"Some String3\""
					"},"
					"["
						"\"Some String4\","
						"\"Some String5\""
					"],"
					"true,"
					"null"
				"]"
			);

		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(InputString);

		TArray< TSharedPtr<FJsonValue> > Array;
		bool bSuccessful = FJsonSerializer::Deserialize(Reader, Array);
		check(bSuccessful);
		check(Array.Num() == 6);
		check(Array[0].IsValid());
		check(Array[1].IsValid());
		check(Array[2].IsValid());
		check(Array[3].IsValid());
		check(Array[4].IsValid());
		check(Array[5].IsValid());

		FString Text = Array[0]->AsString();
		check(Text == TEXT("Some String1"));

		double Number = Array[1]->AsNumber();
		check(Number == 10);

		TSharedPtr< FJsonObject > Object = Array[2]->AsObject();
		check(Object.IsValid());
		check(Object->GetStringField(TEXT("Value")) == TEXT("Some String3"));
		check(Object->GetStringField(TEXT("")) == TEXT("Empty Key"));

		const TArray<TSharedPtr< FJsonValue >>& InnerArray = Array[3]->AsArray();
		check(InnerArray.Num() == 2);
		check(Array[0].IsValid());
		check(Array[1].IsValid());

		Text = InnerArray[0]->AsString();
		check(Text == TEXT("Some String4"));

		Text = InnerArray[1]->AsString();
		check(Text == TEXT("Some String5"));

		bool Boolean = Array[4]->AsBool();
		check(Boolean == true);

		check(Array[5]->IsNull() == true);

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create(&OutputString);
		check(FJsonSerializer::Serialize(Array, Writer));
		check(InputString == OutputString);
	}

	// String Test
	{
		const FString InputString =
			TEXT(
				"{"
					"\"Value\":\"Some String, Escape Chars: \\\\, \\\", \\/, \\b, \\f, \\n, \\r, \\t, \\u002B\""
				"}"
			);
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InputString );

		TSharedPtr<FJsonObject> Object;
		bool bSuccessful = FJsonSerializer::Deserialize(Reader, Object);
		check(bSuccessful);
		check( Object.IsValid() );

		const TSharedPtr<FJsonValue>* Value = Object->Values.Find(TEXT("Value"));
		check(Value && (*Value)->Type == EJson::String);
		const FString String = (*Value)->AsString();
		check(String == TEXT("Some String, Escape Chars: \\, \", /, \b, \f, \n, \r, \t, +"));

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create( &OutputString );
		verify( FJsonSerializer::Serialize( Object.ToSharedRef(), Writer ) );

		const FString TestOutput =
			TEXT(
				"{"
					"\"Value\":\"Some String, Escape Chars: \\\\, \\\", /, \\b, \\f, \\n, \\r, \\t, +\""
				"}"
			);
		check(OutputString == TestOutput);
	}

	// String Test UTF8
	{
		// UTF8TEXT will prepend the first u8 literal specifier
		// UTF8TEXT does a cast so we can't add it each line and still get the literals to concatenate
		const UTF8CHAR* InputString = UTF8TEXT(
			"{"
				u8"\"Value\":\"Some String, Escape Chars: \\\\, \\\", \\/, \\b, \\f, \\n, \\r, \\t, \\u002B\\uD83D\\uDE10\","
				u8"\"Value1\":\"Greek String, Σὲ γνωρίζω ἀπὸ τὴν κόψη\","
				u8"\"Value2\":\"Thai String, สิบสองกษัตริย์ก่อนหน้าแลถัดไป\","
				u8"\"Value3\":\"Hello world, Καλημέρα κόσμε, コンニチハ\""
			u8"}"
		);

		TSharedRef< TJsonReader<UTF8CHAR> > Reader = TJsonReaderFactory<UTF8CHAR>::CreateFromView( InputString );

		TSharedPtr<FJsonObject> Object;
		bool bSuccessful = FJsonSerializer::Deserialize(Reader, Object);
		check(bSuccessful);
		check( Object.IsValid() );

		{
			const TSharedPtr<FJsonValue>* Value = Object->Values.Find(TEXT("Value"));
			check(Value && (*Value)->Type == EJson::String);
			const FString String = (*Value)->AsString();
			check(String == TEXT("Some String, Escape Chars: \\, \", /, \b, \f, \n, \r, \t, +😐"));
		}
		{
			const TSharedPtr<FJsonValue>* Value = Object->Values.Find(TEXT("Value1"));
			check(Value && (*Value)->Type == EJson::String);
			const FString String = (*Value)->AsString();
			check(String == TEXT("Greek String, Σὲ γνωρίζω ἀπὸ τὴν κόψη"));
		}
		{
			const TSharedPtr<FJsonValue>* Value = Object->Values.Find(TEXT("Value2"));
			check(Value && (*Value)->Type == EJson::String);
			const FString String = (*Value)->AsString();
			check(String == TEXT("Thai String, สิบสองกษัตริย์ก่อนหน้าแลถัดไป"));
		}
		{
			const TSharedPtr<FJsonValue>* Value = Object->Values.Find(TEXT("Value3"));
			check(Value && (*Value)->Type == EJson::String);
			const FString String = (*Value)->AsString();
			check(String == TEXT("Hello world, Καλημέρα κόσμε, コンニチハ"));
		}

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create( &OutputString );
		verify( FJsonSerializer::Serialize( Object.ToSharedRef(), Writer ) );

		// Note: The literal prefix for the string (u8, L), must be present for every contatenated string, not just the first one
		const FString TestOutput =
			TEXT("{")
				TEXT("\"Value\":\"Some String, Escape Chars: \\\\, \\\", /, \\b, \\f, \\n, \\r, \\t, +😐\",")
				TEXT("\"Value1\":\"Greek String, Σὲ γνωρίζω ἀπὸ τὴν κόψη\",")
				TEXT("\"Value2\":\"Thai String, สิบสองกษัตริย์ก่อนหน้าแลถัดไป\",")
				TEXT("\"Value3\":\"Hello world, Καλημέρα κόσμε, コンニチハ\"")
			TEXT("}");
		check(OutputString == TestOutput);
	}

	// Number Test
	{
		const FString InputString =
			TEXT(
				"{"
					"\"Value1\":2.544e+15,"
					"\"Value2\":-0.544E-2,"
					"\"Value3\":251e3,"
					"\"Value4\":-0.0,"
					"\"Value5\":843"
				"}"
			);
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InputString );

		TSharedPtr<FJsonObject> Object;
		bool bSuccessful = FJsonSerializer::Deserialize(Reader, Object);
		check(bSuccessful);
		check( Object.IsValid() );

		double TestValues[] = {2.544e+15, -0.544e-2, 251e3, -0.0, 843};
		for (int32 i = 0; i < 5; ++i)
		{
			const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FString::Printf(TEXT("Value%i"), i + 1));
			check(Value && (*Value)->Type == EJson::Number);
			const double Number = (*Value)->AsNumber();
			check(Number == TestValues[i]);
		}

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create( &OutputString );
		verify( FJsonSerializer::Serialize( Object.ToSharedRef(), Writer ) );

		// %g isn't standardized, so we use the same %g format that is used inside PrintJson instead of hardcoding the values here
		const FString TestOutput = FString::Printf(
			TEXT(
				"{"
					"\"Value1\":%.17g,"
					"\"Value2\":%.17g,"
					"\"Value3\":%.17g,"
					"\"Value4\":%.17g,"
					"\"Value5\":%.17g"
				"}"
			),
			TestValues[0], TestValues[1], TestValues[2], TestValues[3], TestValues[4]);
		check(OutputString == TestOutput);
	}

	// Boolean/Null Test
	{
		const FString InputString =
			TEXT(
				"{"
					"\"Value1\":true,"
					"\"Value2\":true,"
					"\"Value3\":faLsE,"
					"\"Value4\":null,"
					"\"Value5\":NULL"
				"}"
			);
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InputString );

		TSharedPtr<FJsonObject> Object;
		bool bSuccessful = FJsonSerializer::Deserialize(Reader, Object);
		check(bSuccessful);
		check( Object.IsValid() );

		bool TestValues[] = {true, true, false};
		for (int32 i = 0; i < 5; ++i)
		{
			const TSharedPtr<FJsonValue>* Value = Object->Values.Find(FString::Printf(TEXT("Value%i"), i + 1));
			check(Value);
			if (i < 3)
			{
				check((*Value)->Type == EJson::Boolean);
				const bool Bool = (*Value)->AsBool();
				check(Bool == TestValues[i]);
			}
			else
			{
				check((*Value)->Type == EJson::Null);
				check((*Value)->IsNull());
			}
		}

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create( &OutputString );
		verify( FJsonSerializer::Serialize( Object.ToSharedRef(), Writer ) );

		const FString TestOutput =
			TEXT(
				"{"
					"\"Value1\":true,"
					"\"Value2\":true,"
					"\"Value3\":false,"
					"\"Value4\":null,"
					"\"Value5\":null"
				"}"
			);
		check(OutputString == TestOutput);
	}

	// Object Test && extra whitespace test
	{
		const FString InputStringWithExtraWhitespace =
			TEXT(
				"		\n\r\n	   {"
					"\"Object\":"
					"{"
						"\"NestedValue\":null,"
						"\"NestedObject\":{}"
					"},"
					"\"Value\":true"
				"}		\n\r\n	   "
			);

		const FString InputString =
			TEXT(
				"{"
					"\"Object\":"
					"{"
						"\"NestedValue\":null,"
						"\"NestedObject\":{}"
					"},"
					"\"Value\":true"
				"}"
			);

		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InputStringWithExtraWhitespace );

		TSharedPtr<FJsonObject> Object;
		bool bSuccessful = FJsonSerializer::Deserialize(Reader, Object);
		check(bSuccessful);
		check( Object.IsValid() );

		const TSharedPtr<FJsonValue>* InnerValueFail = Object->Values.Find(TEXT("InnerValue"));
		check(!InnerValueFail);

		const TSharedPtr<FJsonValue>* ObjectValue = Object->Values.Find(TEXT("Object"));
		check(ObjectValue && (*ObjectValue)->Type == EJson::Object);
		const TSharedPtr<FJsonObject> InnerObject = (*ObjectValue)->AsObject();
		check(InnerObject.IsValid());

		{
			const TSharedPtr<FJsonValue>* NestedValueValue = InnerObject->Values.Find(TEXT("NestedValue"));
			check(NestedValueValue && (*NestedValueValue)->Type == EJson::Null);
			check((*NestedValueValue)->IsNull());

			const TSharedPtr<FJsonValue>* NestedObjectValue = InnerObject->Values.Find(TEXT("NestedObject"));
			check(NestedObjectValue && (*NestedObjectValue)->Type == EJson::Object);
			const TSharedPtr<FJsonObject> InnerInnerObject = (*NestedObjectValue)->AsObject();
			check(InnerInnerObject.IsValid());

			{
				const TSharedPtr<FJsonValue>* NestedValueValueFail = InnerInnerObject->Values.Find(TEXT("NestedValue"));
				check(!NestedValueValueFail);
			}
		}

		const TSharedPtr<FJsonValue>* ValueValue = Object->Values.Find(TEXT("Value"));
		check(ValueValue && (*ValueValue)->Type == EJson::Boolean);
		const bool Bool = (*ValueValue)->AsBool();
		check(Bool);

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create( &OutputString );
		verify( FJsonSerializer::Serialize( Object.ToSharedRef(), Writer ) );
		check(OutputString == InputString);
	}

	// Array Test
	{
		const FString InputString =
			TEXT(
				"{"
					"\"Array\":"
					"["
						"[],"
						"\"Some String\","
						"\"Another String\","
						"null,"
						"true,"
						"false,"
						"45,"
						"{}"
					"]"
				"}"
			);
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InputString );

		TSharedPtr<FJsonObject> Object;
		bool bSuccessful = FJsonSerializer::Deserialize(Reader, Object);
		check(bSuccessful);
		check( Object.IsValid() );

		const TSharedPtr<FJsonValue>* InnerValueFail = Object->Values.Find(TEXT("InnerValue"));
		check(!InnerValueFail);

		const TSharedPtr<FJsonValue>* ArrayValue = Object->Values.Find(TEXT("Array"));
		check(ArrayValue && (*ArrayValue)->Type == EJson::Array);
		const TArray< TSharedPtr<FJsonValue> > Array = (*ArrayValue)->AsArray();
		check(Array.Num() == 8);

		EJson ValueTypes[] = {EJson::Array, EJson::String, EJson::String, EJson::Null,
			EJson::Boolean, EJson::Boolean, EJson::Number, EJson::Object};
		for (int32 i = 0; i < Array.Num(); ++i)
		{
			const TSharedPtr<FJsonValue>& Value = Array[i];
			check(Value.IsValid());
			check(Value->Type == ValueTypes[i]);
		}

		const TArray< TSharedPtr<FJsonValue> >& InnerArray = Array[0]->AsArray();
		check(InnerArray.Num() == 0);
		check(Array[1]->AsString() == TEXT("Some String"));
		check(Array[2]->AsString() == TEXT("Another String"));
		check(Array[3]->IsNull());
		check(Array[4]->AsBool());
		check(!Array[5]->AsBool());
		check(FMath::Abs(Array[6]->AsNumber() - 45.f) < KINDA_SMALL_NUMBER);
		const TSharedPtr<FJsonObject> InnerObject = Array[7]->AsObject();
		check(InnerObject.IsValid());

		FString OutputString;
		TSharedRef< FCondensedJsonStringWriter > Writer = FCondensedJsonStringWriterFactory::Create( &OutputString );
		verify( FJsonSerializer::Serialize( Object.ToSharedRef(), Writer ) );
		check(OutputString == InputString);
	}

	// Pretty Print Test
	{
		const FString InputString =
			TEXT(
				"{"											LINE_TERMINATOR_ANSI
					"	\"Data1\": \"value\","				LINE_TERMINATOR_ANSI
					"	\"Data2\": \"value\","				LINE_TERMINATOR_ANSI
					"	\"Array\": ["						LINE_TERMINATOR_ANSI
					"		{"								LINE_TERMINATOR_ANSI
					"			\"InnerData1\": \"value\""	LINE_TERMINATOR_ANSI
					"		},"								LINE_TERMINATOR_ANSI
					"		[],"							LINE_TERMINATOR_ANSI
					"		[ 1, 2, 3, 4 ],"				LINE_TERMINATOR_ANSI
					"		{"								LINE_TERMINATOR_ANSI
					"		},"								LINE_TERMINATOR_ANSI
					"		\"value\","						LINE_TERMINATOR_ANSI
					"		\"value\""						LINE_TERMINATOR_ANSI
					"	],"									LINE_TERMINATOR_ANSI
					"	\"Object\":"						LINE_TERMINATOR_ANSI
					"	{"									LINE_TERMINATOR_ANSI
					"	}"									LINE_TERMINATOR_ANSI
				"}"
			);
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InputString );

		TSharedPtr<FJsonObject> Object;
		verify( FJsonSerializer::Deserialize( Reader, Object ) );
		check( Object.IsValid() );

		FString OutputString;
		TSharedRef< FPrettyJsonStringWriter > Writer = FPrettyJsonStringWriterFactory::Create( &OutputString );
		verify( FJsonSerializer::Serialize( Object.ToSharedRef(), Writer ) );
		check(OutputString == InputString);
	}
	  
	// Line and Character # test
	{
		const FString InputString =
			TEXT(
				"{"											LINE_TERMINATOR_ANSI
					"	\"Data1\": \"value\","				LINE_TERMINATOR_ANSI
					"	\"Array\":"							LINE_TERMINATOR_ANSI
					"	["									LINE_TERMINATOR_ANSI
					"		12345,"							LINE_TERMINATOR_ANSI
					"		True"							LINE_TERMINATOR_ANSI
					"	],"									LINE_TERMINATOR_ANSI
					"	\"Object\":"						LINE_TERMINATOR_ANSI
					"	{"									LINE_TERMINATOR_ANSI
					"	}"									LINE_TERMINATOR_ANSI
				"}"
			);
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( InputString );

		EJsonNotation Notation = EJsonNotation::Null;
		verify( Reader->ReadNext( Notation ) && Notation == EJsonNotation::ObjectStart );
		check( Reader->GetLineNumber() == 1 && Reader->GetCharacterNumber() == 1 );

		verify( Reader->ReadNext( Notation ) && Notation == EJsonNotation::String );
		check( Reader->GetLineNumber() == 2 && Reader->GetCharacterNumber() == 17 );

		verify( Reader->ReadNext( Notation ) && Notation == EJsonNotation::ArrayStart );
		check( Reader->GetLineNumber() == 4 && Reader->GetCharacterNumber() == 2 );

		verify( Reader->ReadNext( Notation ) && Notation == EJsonNotation::Number );
		check( Reader->GetLineNumber() == 5 && Reader->GetCharacterNumber() == 7 );

		verify( Reader->ReadNext( Notation ) && Notation == EJsonNotation::Boolean );
		check( Reader->GetLineNumber() == 6 && Reader->GetCharacterNumber() == 6 );
	}

	// Failure Cases
	TArray<FString> FailureInputs;

	// Unclosed Object
	FailureInputs.Add(
		TEXT("{"));

	// Values in Object without identifiers
	FailureInputs.Add(
		TEXT(
			"{"
				"\"Value1\","
				"\"Value2\","
				"43"
			"}"
		)
	);

	// Unexpected End Of Input Found
	FailureInputs.Add(
		TEXT(
			"{"
				"\"Object\":"
				"{"
					"\"NestedValue\":null,")
	);

	// Missing first brace
	FailureInputs.Add(
		TEXT(
			"\"Object\":"
			"{"
				"\"NestedValue\":null,"
				"\"NestedObject\":{}"
			"},"
		"\"Value\":true"
		"}"
		)
	);

	// Missing last character
	FailureInputs.Add(
		TEXT(
			"{"
				"\"Object\":"
				"{"
					"\"NestedValue\":null,"
					"\"NestedObject\":{}"
				"},"
				"\"Value\":true"
		)
	);

	// Missing curly brace
	FailureInputs.Add(TEXT("}"));

	// Missing bracket
	FailureInputs.Add(TEXT("]"));

	// Extra last character
	FailureInputs.Add(
		TEXT(
			"{"
				"\"Object\":"
				"{"
					"\"NestedValue\":null,"
					"\"NestedObject\":{}"
				"},"
				"\"Value\":true"
			"}0"
		)
	);

	// Missing comma
	FailureInputs.Add(
		TEXT(
			"{"
				"\"Value1\":null,"
				"\"Value2\":\"string\""
				"\"Value3\":65.3"
			"}"
		)
	);

	// Extra comma
	FailureInputs.Add(
		TEXT(
			"{"
				"\"Value1\":null,"
				"\"Value2\":\"string\","
				"\"Value3\":65.3,"
			"}"
		)
	);

	// Badly formed true/false/null
	FailureInputs.Add(TEXT("{\"Value\":tru}"));
	FailureInputs.Add(TEXT("{\"Value\":full}"));
	FailureInputs.Add(TEXT("{\"Value\":nulle}"));
	FailureInputs.Add(TEXT("{\"Value\":n%ll}"));

	// Floating Point Failures
	FailureInputs.Add(TEXT("{\"Value\":65.3e}"));
	FailureInputs.Add(TEXT("{\"Value\":65.}"));
	FailureInputs.Add(TEXT("{\"Value\":.7}"));
	FailureInputs.Add(TEXT("{\"Value\":+6}"));
	FailureInputs.Add(TEXT("{\"Value\":01}"));
	FailureInputs.Add(TEXT("{\"Value\":00.56}"));
	FailureInputs.Add(TEXT("{\"Value\":-1.e+4}"));
	FailureInputs.Add(TEXT("{\"Value\":2e+}"));

	// Bad Escape Characters
	FailureInputs.Add(TEXT("{\"Value\":\"Hello\\xThere\"}"));
	FailureInputs.Add(TEXT("{\"Value\":\"Hello\\u123There\"}"));
	FailureInputs.Add(TEXT("{\"Value\":\"Hello\\RThere\"}"));

	for (int32 i = 0; i < FailureInputs.Num(); ++i)
	{
		TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create( FailureInputs[i] );

		TSharedPtr<FJsonObject> Object;
		verify( FJsonSerializer::Deserialize( Reader, Object ) == false );
		check( !Object.IsValid() );
	}

	// TryGetNumber tests
	{
		auto JsonNumberToInt64 = [](double Val, int64& OutVal) -> bool
		{
			FJsonValueNumber JsonVal(Val);
			return ((FJsonValue&)JsonVal).TryGetNumber(OutVal);
		};

		auto JsonNumberToInt32 = [](double Val, int32& OutVal) -> bool
		{
			FJsonValueNumber JsonVal(Val);
			return ((FJsonValue&)JsonVal).TryGetNumber(OutVal);
		};

		auto JsonNumberToUInt32 = [](double Val, uint32& OutVal) -> bool
		{
			FJsonValueNumber JsonVal(Val);
			return ((FJsonValue&)JsonVal).TryGetNumber(OutVal);
		};
		
		// TryGetNumber-Int64 tests
		{
			int64 IntVal;
			bool bOk = JsonNumberToInt64(9007199254740991.0, IntVal);
			TestTrue(TEXT("TryGetNumber-Int64 Big Float64 succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int64 Big Float64"), IntVal, 9007199254740991LL);
		}

		{
			int64 IntVal;
			bool bOk = JsonNumberToInt64(-9007199254740991.0, IntVal);
			TestTrue(TEXT("TryGetNumber-Int64 Small Float64 succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int64 Small Float64"), IntVal, -9007199254740991LL);
		}

		{
			int64 IntVal;
			bool bOk = JsonNumberToInt64(0.4999999999999997, IntVal);
			TestTrue(TEXT("TryGetNumber-Int64 Lesser than near half succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int64 Lesser than near half rounds to zero"), IntVal, 0LL);
		}

		{
			int64 IntVal;
			bool bOk = JsonNumberToInt64(-0.4999999999999997, IntVal);
			TestTrue(TEXT("TryGetNumber-Int64 Greater than near negative half succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int64 Greater than near negative half rounds to zero"), IntVal, 0LL);
		}

		{
			int64 IntVal;
			bool bOk = JsonNumberToInt64(0.5, IntVal);
			TestTrue(TEXT("TryGetNumber-Int64 Half rounds to next integer succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int64 Half rounds to next integer"), IntVal, 1LL);
		}

		{
			int64 IntVal;
			bool bOk = JsonNumberToInt64(-0.5, IntVal);
			TestTrue(TEXT("TryGetNumber-Int64 Negative half rounds to next negative integer succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int64 Negative half rounds to next negative integer succeeds"), IntVal, -1LL);
		}

		// TryGetNumber-Int32 tests
		{
			int32 IntVal;
			bool bOk = JsonNumberToInt32(2147483647.000001, IntVal);
			TestFalse(TEXT("TryGetNumber-Int32 Number greater than max Int32 fails"), bOk);
		}

		{
			int32 IntVal;
			bool bOk = JsonNumberToInt32(-2147483648.000001, IntVal);
			TestFalse(TEXT("TryGetNumber-Int32 Number lesser than min Int32 fails"), bOk);
		}

		{
			int32 IntVal;
			bool bOk = JsonNumberToInt32(2147483647.0, IntVal);
			TestTrue(TEXT("TryGetNumber-Int32 Max Int32 succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int32 Max Int32"), IntVal, INT_MAX);
		}

		{
			int32 IntVal;
			bool bOk = JsonNumberToInt32(2147483646.5, IntVal);
			TestTrue(TEXT("TryGetNumber-Int32 Round up to max Int32 succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int32 Round up to max Int32"), IntVal, INT_MAX);
		}

		{
			int32 IntVal;
			bool bOk = JsonNumberToInt32(-2147483648.0, IntVal);
			TestTrue(TEXT("TryGetNumber-Int32 Min Int32 succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int32 Min Int32"), IntVal, INT_MIN);
		}

		{
			int32 IntVal;
			bool bOk = JsonNumberToInt32(-2147483647.5, IntVal);
			TestTrue(TEXT("TryGetNumber-Int32 Round down to min Int32 succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int32 Round down to min Int32"), IntVal, INT_MIN);
		}

		{
			int32 IntVal;
			bool bOk = JsonNumberToInt32(0.4999999999999997, IntVal);
			TestTrue(TEXT("TryGetNumber-Int32 Lesser than near half succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int32 Lesser than near half rounds to zero"), IntVal, 0);
		}

		{
			int32 IntVal;
			bool bOk = JsonNumberToInt32(-0.4999999999999997, IntVal);
			TestTrue(TEXT("TryGetNumber-Int32 Greater than near negative half succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int32 Greater than near negative half rounds to zero"), IntVal, 0);
		}

		{
			int32 IntVal;
			bool bOk = JsonNumberToInt32(0.5, IntVal);
			TestTrue(TEXT("TryGetNumber-Int32 Half rounds to next integer succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int32 Half rounds to next integer"), IntVal, 1);
		}

		{
			int32 IntVal;
			bool bOk = JsonNumberToInt32(-0.5, IntVal);
			TestTrue(TEXT("TryGetNumber-Int32 Negative half rounds to next negative integer succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-Int32 Negative half rounds to next negative integer succeeds"), IntVal, -1);
		}

		// TryGetNumber-UInt32 tests
		{
			uint32 IntVal;
			bool bOk = JsonNumberToUInt32(4294967295.000001, IntVal);
			TestFalse(TEXT("TryGetNumber-UInt32 Number greater than max Uint32 fails"), bOk);
		}

		{
			uint32 IntVal;
			bool bOk = JsonNumberToUInt32(-0.000000000000001, IntVal);
			TestFalse(TEXT("TryGetNumber-UInt32 Negative number fails"), bOk);
		}

		{
			uint32 IntVal;
			bool bOk = JsonNumberToUInt32(4294967295.0, IntVal);
			TestTrue(TEXT("TryGetNumber-UInt32 Max UInt32 succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-UInt32  Max UInt32"), IntVal, UINT_MAX);
		}

		{
			uint32 IntVal;
			bool bOk = JsonNumberToUInt32(4294967294.5, IntVal);
			TestTrue(TEXT("TryGetNumber-UInt32 Round up to max UInt32 succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-UInt32 Round up to max UInt32"), IntVal, UINT_MAX);
		}

		{
			uint32 IntVal;
			bool bOk = JsonNumberToUInt32(0.4999999999999997, IntVal);
			TestTrue(TEXT("TryGetNumber-UInt32 Lesser than near half succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-UInt32 Lesser than near half rounds to zero"), IntVal, 0U);
		}

		{
			uint32 IntVal;
			bool bOk = JsonNumberToUInt32(0.5, IntVal);
			TestTrue(TEXT("TryGetNumber-UInt32 Half rounds to next integer succeeds"), bOk);
			TestEqual(TEXT("TryGetNumber-UInt32 Half rounds to next integer"), IntVal, 1U);
		}
	}

	return true;
}

#endif //WITH_DEV_AUTOMATION_TESTS
