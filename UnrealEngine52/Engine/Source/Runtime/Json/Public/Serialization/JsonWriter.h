// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/JsonTypes.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/MemoryWriter.h"

/**
 * Takes an input string and escapes it so it can be written as a valid Json string. Also adds the quotes.
 * Appends to a given string-like object to avoid reallocations.
 * String-like object must support operator+=(const TCHAR*) and operation+=(TCHAR)
 *
 * @param AppendTo the string to append to.
 * @param StringVal the string to escape
 * @return the AppendTo string for convenience.
 */
template<typename StringType>
inline StringType& AppendEscapeJsonString(StringType& AppendTo, const FString& StringVal)
{
	AppendTo += TEXT("\"");
	for (const TCHAR* Char = *StringVal; *Char != TCHAR('\0'); ++Char)
	{
		switch (*Char)
		{
		case TCHAR('\\'): AppendTo += TEXT("\\\\"); break;
		case TCHAR('\n'): AppendTo += TEXT("\\n"); break;
		case TCHAR('\t'): AppendTo += TEXT("\\t"); break;
		case TCHAR('\b'): AppendTo += TEXT("\\b"); break;
		case TCHAR('\f'): AppendTo += TEXT("\\f"); break;
		case TCHAR('\r'): AppendTo += TEXT("\\r"); break;
		case TCHAR('\"'): AppendTo += TEXT("\\\""); break;
		default:
			// Must escape control characters
			if (*Char >= TCHAR(32))
			{
				AppendTo += *Char;
			}
			else
			{
				AppendTo.Appendf(TEXT("\\u%04x"), *Char);
			}
		}
	}
	AppendTo += TEXT("\"");

	return AppendTo;
}

/**
 * Takes an input string and escapes it so it can be written as a valid Json string. Also adds the quotes.
 *
 * @param StringVal the string to escape
 * @return the given string, escaped to produce a valid Json string.
 */
inline FString EscapeJsonString(const FString& StringVal)
{
	FString Result;
	return AppendEscapeJsonString(Result, StringVal);
}

/**
 * Template for Json writers.
 *
 * @param CharType The type of characters to print, i.e. TCHAR or ANSICHAR.
 * @param PrintPolicy The print policy to use when writing the output string (default = TPrettyJsonPrintPolicy).
 */
template <class CharType = TCHAR, class PrintPolicy = TPrettyJsonPrintPolicy<CharType> >
class TJsonWriter
{
public:

	static TSharedRef< TJsonWriter > Create( FArchive* const Stream, int32 InitialIndentLevel = 0 )
	{
		return MakeShareable( new TJsonWriter< CharType, PrintPolicy >( Stream, InitialIndentLevel ) );
	}

public:

	virtual ~TJsonWriter() { }

	FORCEINLINE int32 GetIndentLevel() const { return IndentLevel; }

	bool CanWriteObjectStart() const
	{
		return CanWriteObjectWithoutIdentifier();
	}

	void WriteObjectStart()
	{
		check(CanWriteObjectWithoutIdentifier());
		if (PreviousTokenWritten != EJsonToken::None )
		{
			WriteCommaIfNeeded();
		}

		if ( PreviousTokenWritten != EJsonToken::None )
		{
			PrintPolicy::WriteLineTerminator(Stream);
			PrintPolicy::WriteTabs(Stream, IndentLevel);
		}

		PrintPolicy::WriteChar(Stream, CharType('{'));
		++IndentLevel;
		Stack.Push( EJson::Object );
		PreviousTokenWritten = EJsonToken::CurlyOpen;
	}

	template<typename IdentifierType>
	void WriteObjectStart(IdentifierType&& Identifier)
	{
		check( Stack.Top() == EJson::Object );
		WriteIdentifier(Forward<IdentifierType>(Identifier));

		PrintPolicy::WriteLineTerminator(Stream);
		PrintPolicy::WriteTabs(Stream, IndentLevel);
		PrintPolicy::WriteChar(Stream, CharType('{'));
		++IndentLevel;
		Stack.Push( EJson::Object );
		PreviousTokenWritten = EJsonToken::CurlyOpen;
	}

	void WriteObjectEnd()
	{
		check( Stack.Top() == EJson::Object );

		PrintPolicy::WriteLineTerminator(Stream);

		--IndentLevel;
		PrintPolicy::WriteTabs(Stream, IndentLevel);
		PrintPolicy::WriteChar(Stream, CharType('}'));
		Stack.Pop();
		PreviousTokenWritten = EJsonToken::CurlyClose;
	}

	void WriteArrayStart()
	{
		check(CanWriteValueWithoutIdentifier());
		if ( PreviousTokenWritten != EJsonToken::None )
		{
			WriteCommaIfNeeded();
		}

		if ( PreviousTokenWritten != EJsonToken::None )
		{
			PrintPolicy::WriteLineTerminator(Stream);
			PrintPolicy::WriteTabs(Stream, IndentLevel);
		}

		PrintPolicy::WriteChar(Stream, CharType('['));
		++IndentLevel;
		Stack.Push( EJson::Array );
		PreviousTokenWritten = EJsonToken::SquareOpen;
	}

	template<typename IdentifierType>
	void WriteArrayStart(IdentifierType&& Identifier)
	{
		check( Stack.Top() == EJson::Object );
		WriteIdentifier(Forward<IdentifierType>(Identifier));

		PrintPolicy::WriteSpace( Stream );
		PrintPolicy::WriteChar(Stream, CharType('['));
		++IndentLevel;
		Stack.Push( EJson::Array );
		PreviousTokenWritten = EJsonToken::SquareOpen;
	}

	void WriteArrayEnd()
	{
		check( Stack.Top() == EJson::Array );

		--IndentLevel;
		if ( PreviousTokenWritten == EJsonToken::SquareClose || PreviousTokenWritten == EJsonToken::CurlyClose || PreviousTokenWritten == EJsonToken::String )
		{
			PrintPolicy::WriteLineTerminator(Stream);
			PrintPolicy::WriteTabs(Stream, IndentLevel);
		}
		else if ( PreviousTokenWritten != EJsonToken::SquareOpen )
		{
			PrintPolicy::WriteSpace( Stream );
		}

		PrintPolicy::WriteChar(Stream, CharType(']'));
		Stack.Pop();
		PreviousTokenWritten = EJsonToken::SquareClose;
	}

	template <class FValue>
	void WriteValue(FValue Value)
	{
		check(CanWriteValueWithoutIdentifier());
		WriteCommaIfNeeded();

		if (PreviousTokenWritten == EJsonToken::SquareOpen || EJsonToken_IsShortValue(PreviousTokenWritten))
		{
			PrintPolicy::WriteSpace( Stream );
		}
		else
		{
			PrintPolicy::WriteLineTerminator(Stream);
			PrintPolicy::WriteTabs(Stream, IndentLevel);
		}

		PreviousTokenWritten = WriteValueOnly( Value );
	}

	void WriteValue(FStringView Value)
	{
		check(CanWriteValueWithoutIdentifier());
		WriteCommaIfNeeded();

		PrintPolicy::WriteLineTerminator(Stream);
		PrintPolicy::WriteTabs(Stream, IndentLevel);
		PreviousTokenWritten = WriteValueOnly(Value);
	}

	void WriteValue(const FString& Value)
	{
		check(CanWriteValueWithoutIdentifier());
		WriteCommaIfNeeded();

		PrintPolicy::WriteLineTerminator(Stream);
		PrintPolicy::WriteTabs(Stream, IndentLevel);
		PreviousTokenWritten = WriteValueOnly(Value);
	}

	template<class FValue, typename IdentifierType>
	void WriteValue(IdentifierType&& Identifier, FValue Value)
	{
		check( Stack.Top() == EJson::Object );
		WriteIdentifier(Forward<IdentifierType>(Identifier));

		PrintPolicy::WriteSpace(Stream);
		PreviousTokenWritten = WriteValueOnly(MoveTemp(Value));
	}

	template<class ElementType, typename IdentifierType>
	void WriteValue(IdentifierType&& Identifier, const TArray<ElementType>& Array)
	{
		WriteArrayStart(Forward<IdentifierType>(Identifier));
		for (int Idx = 0; Idx < Array.Num(); Idx++)
		{
			WriteValue(Array[Idx]);
		}
		WriteArrayEnd();
	}

	void WriteValue(FStringView Identifier, const TCHAR* Value)
	{
		WriteValue(Identifier, FStringView(Value));
	}

	// WARNING: THIS IS DANGEROUS. Use this only if you know for a fact that the Value is valid JSON!
	// Use this to insert the results of a different JSON Writer in.
	void WriteRawJSONValue( const FString& Identifier, const FString& Value )
	{
		check( Stack.Top() == EJson::Object );
		WriteIdentifier( Identifier );

		PrintPolicy::WriteSpace(Stream);
		PrintPolicy::WriteString(Stream, Value);
		PreviousTokenWritten = EJsonToken::String;
	}

	template<typename IdentifierType>
	void WriteNull(IdentifierType&& Identifier)
	{
		WriteValue(Forward<IdentifierType>(Identifier), nullptr);
	}

	void WriteValue(const TCHAR* Value)
	{
		WriteValue(FStringView(Value));
	}

	// WARNING: THIS IS DANGEROUS. Use this only if you know for a fact that the Value is valid JSON!
	// Use this to insert the results of a different JSON Writer in.
	void WriteRawJSONValue(const FString& Value)
	{
		check(CanWriteValueWithoutIdentifier());
		WriteCommaIfNeeded();

		if ( PreviousTokenWritten != EJsonToken::True && PreviousTokenWritten != EJsonToken::False && PreviousTokenWritten != EJsonToken::SquareOpen )
		{
			PrintPolicy::WriteLineTerminator(Stream);
			PrintPolicy::WriteTabs(Stream, IndentLevel);
		}
		else
		{
			PrintPolicy::WriteSpace( Stream );
		}

		PrintPolicy::WriteString(Stream, Value);
		PreviousTokenWritten = EJsonToken::String;
	}

	void WriteNull()
	{
		WriteValue(nullptr);
	}

	virtual bool Close()
	{
		return ( PreviousTokenWritten == EJsonToken::None ||
				 PreviousTokenWritten == EJsonToken::CurlyClose  ||
				 PreviousTokenWritten == EJsonToken::SquareClose )
				&& Stack.Num() == 0;
	}

	/**
	 * WriteValue("Foo", Bar) should be equivalent to WriteIdentifierPrefix("Foo"), WriteValue(Bar)
	 */
	template<typename IdentifierType>
	void WriteIdentifierPrefix(IdentifierType&& Identifier)
	{
		check(Stack.Top() == EJson::Object);
		WriteIdentifier(Forward<IdentifierType>(Identifier));
		PrintPolicy::WriteSpace(Stream);
		PreviousTokenWritten = EJsonToken::Identifier;
	}

protected:

	/**
	 * Creates and initializes a new instance.
	 *
	 * @param InStream An archive containing the input.
	 * @param InitialIndentLevel The initial indentation level.
	 */
	TJsonWriter( FArchive* const InStream, int32 InitialIndentLevel )
		: Stream( InStream )
		, Stack()
		, PreviousTokenWritten(EJsonToken::None)
		, IndentLevel(InitialIndentLevel)
	{ }

protected:

	FORCEINLINE bool CanWriteValueWithoutIdentifier() const
	{
		return Stack.Num() <= 0 || Stack.Top() == EJson::Array || PreviousTokenWritten == EJsonToken::Identifier;
	}

	FORCEINLINE bool CanWriteObjectWithoutIdentifier() const
	{
		return Stack.Num() <= 0 || Stack.Top() == EJson::Array || PreviousTokenWritten == EJsonToken::Identifier || PreviousTokenWritten == EJsonToken::Colon;
	}

	FORCEINLINE void WriteCommaIfNeeded()
	{
		if ( PreviousTokenWritten != EJsonToken::CurlyOpen && PreviousTokenWritten != EJsonToken::SquareOpen && PreviousTokenWritten != EJsonToken::Identifier)
		{
			PrintPolicy::WriteChar(Stream, CharType(','));
		}
	}

	void WriteIdentifier(const ANSICHAR* Identifier)
	{
		WriteCommaIfNeeded();
		PrintPolicy::WriteLineTerminator(Stream);

		PrintPolicy::WriteTabs(Stream, IndentLevel);
		WriteStringValue(FAnsiStringView(Identifier));
		PrintPolicy::WriteChar(Stream, CharType(':'));
	}
	
	void WriteIdentifier(const TCHAR* Identifier)
	{
		WriteCommaIfNeeded();
		PrintPolicy::WriteLineTerminator(Stream);

		PrintPolicy::WriteTabs(Stream, IndentLevel);
		WriteStringValue(FStringView(Identifier));
		PrintPolicy::WriteChar(Stream, CharType(':'));
	}

	void WriteIdentifier(FStringView Identifier)
	{
		WriteCommaIfNeeded();
		PrintPolicy::WriteLineTerminator(Stream);

		PrintPolicy::WriteTabs(Stream, IndentLevel);
		WriteStringValue(Identifier);
		PrintPolicy::WriteChar(Stream, CharType(':'));
	}

	void WriteIdentifier(const FText& Identifier)
	{
		WriteIdentifier(Identifier.ToString()); // Does not copy
	}

	FORCEINLINE void WriteIdentifier(const FString& Identifier)
	{
		WriteCommaIfNeeded();
		PrintPolicy::WriteLineTerminator(Stream);

		PrintPolicy::WriteTabs(Stream, IndentLevel);
		WriteStringValue(FStringView(Identifier));
		PrintPolicy::WriteChar(Stream, CharType(':'));
	}

	FORCEINLINE EJsonToken WriteValueOnly(bool Value)
	{
		PrintPolicy::WriteString(Stream, Value ? TEXTVIEW("true") : TEXTVIEW("false"));
		return Value ? EJsonToken::True : EJsonToken::False;
	}

	FORCEINLINE EJsonToken WriteValueOnly(float Value)
	{
		PrintPolicy::WriteFloat(Stream, Value);
		return EJsonToken::Number;
	}

	FORCEINLINE EJsonToken WriteValueOnly(double Value)
	{
		// Specify 17 significant digits, the most that can ever be useful from a double
		// In particular, this ensures large integers are written correctly
		PrintPolicy::WriteDouble(Stream, Value);
		return EJsonToken::Number;
	}

	FORCEINLINE EJsonToken WriteValueOnly(int32 Value)
	{
		return WriteValueOnly((int64)Value);
	}

	FORCEINLINE EJsonToken WriteValueOnly(int64 Value)
	{
		PrintPolicy::WriteString(Stream, WriteToString<32>(Value));
		return EJsonToken::Number;
	}

	EJsonToken WriteValueOnly(uint64 Value)
	{
		PrintPolicy::WriteString(Stream, WriteToString<32>(Value));
		return EJsonToken::Number;
	}

	FORCEINLINE EJsonToken WriteValueOnly(TYPE_OF_NULLPTR)
	{
		PrintPolicy::WriteString(Stream, TEXTVIEW("null"));
		return EJsonToken::Null;
	}

	FORCEINLINE EJsonToken WriteValueOnly(const TCHAR* Value)
	{
		WriteStringValue(FStringView(Value));
		return EJsonToken::String;
	}

	FORCEINLINE EJsonToken WriteValueOnly(FStringView Value)
	{
		WriteStringValue(Value);
		return EJsonToken::String;
	}

	virtual void WriteStringValue(FAnsiStringView String)
	{
		PrintPolicy::WriteChar(Stream, CharType('"'));
		WriteEscapedString(String);
		PrintPolicy::WriteChar(Stream, CharType('"'));
	}

	virtual void WriteStringValue(FStringView String)
	{
		PrintPolicy::WriteChar(Stream, CharType('"'));
		WriteEscapedString(String);
		PrintPolicy::WriteChar(Stream, CharType('"'));
	}

	virtual void WriteStringValue(const FString& String)
	{
		TJsonWriter::WriteStringValue(FStringView(String));
	}

	template<typename InCharType>
	void WriteEscapedString(TStringView<InCharType> InView)
	{
		auto NeedsEscaping = [](InCharType Char) -> bool
		{
			switch (Char)
			{
			case TCHAR('\\'): return true;
			case TCHAR('\n'): return true;
			case TCHAR('\t'): return true;
			case TCHAR('\b'): return true;
			case TCHAR('\f'): return true;
			case TCHAR('\r'): return true;
			case TCHAR('\"'): return true;
			default:
				// Must escape control characters
				if (Char >= TCHAR(32))
				{
					return false;
				}
				else
				{
					return true;
				}
			}
		};

		// Write successive runs of unescaped and escaped characters until the view is exhausted
		while (!InView.IsEmpty())
		{
			 // In case we are handed a very large string, avoid checking all of it at once without writing anything
			constexpr int32 LongestRun = 2048;
			int32 EndIndex = 0;
			for (; EndIndex < InView.Len() && EndIndex < LongestRun; ++EndIndex)
			{
				if (NeedsEscaping(InView[EndIndex]))
				{ 
					break;
				}
			}
			if (TStringView<InCharType> Blittable = InView.Left(EndIndex); !Blittable.IsEmpty())
			{
				PrintPolicy::WriteString(Stream, Blittable);
			}
			InView.RightChopInline(EndIndex);
			for (EndIndex = 0; EndIndex < InView.Len(); ++EndIndex)
			{
				TCHAR Char = InView[EndIndex]; 
				switch (Char)
				{
				case TCHAR('\\'): PrintPolicy::WriteString(Stream, TEXTVIEW("\\\\")); continue;
				case TCHAR('\n'): PrintPolicy::WriteString(Stream, TEXTVIEW("\\n")); continue;
				case TCHAR('\t'): PrintPolicy::WriteString(Stream, TEXTVIEW("\\t")); continue;
				case TCHAR('\b'): PrintPolicy::WriteString(Stream, TEXTVIEW("\\b")); continue;
				case TCHAR('\f'): PrintPolicy::WriteString(Stream, TEXTVIEW("\\f")); continue;
				case TCHAR('\r'): PrintPolicy::WriteString(Stream, TEXTVIEW("\\r")); continue;
				case TCHAR('\"'): PrintPolicy::WriteString(Stream, TEXTVIEW("\\\"")); continue;
				default: break;
				}

				// Must escape control characters
				if (Char >= TCHAR(32))
				{
					break;
				}
				else
				{
					TAnsiStringBuilder<8> Builder;
					Builder.Appendf("\\u%04x", Char);
					PrintPolicy::WriteString(Stream, Builder.ToView());
				}
			}
			InView.RightChopInline(EndIndex);
		}
	}

	FArchive* const Stream;
	TArray<EJson> Stack;
	EJsonToken PreviousTokenWritten;
	int32 IndentLevel;
};


template <class PrintPolicy = TPrettyJsonPrintPolicy<TCHAR>>
class TJsonStringWriter
	: public TJsonWriter<TCHAR, PrintPolicy>
{
public:

	static TSharedRef<TJsonStringWriter> Create( FString* const InStream, int32 InitialIndent = 0 )
	{
		return MakeShareable(new TJsonStringWriter(InStream, InitialIndent));
	}

public:

	virtual ~TJsonStringWriter()
	{
		check(this->Stream->Close());
		delete this->Stream;
	}

	virtual bool Close() override
	{
		FString Out;

		for (int32 i = 0; i < Bytes.Num(); i+=sizeof(TCHAR))
		{
			TCHAR* Char = static_cast<TCHAR*>(static_cast<void*>(&Bytes[i]));
			Out += *Char;
		}

		*OutString = Out;

		return TJsonWriter<TCHAR, PrintPolicy>::Close();
	}

protected:

	TJsonStringWriter( FString* const InOutString, int32 InitialIndent )
		: TJsonWriter<TCHAR, PrintPolicy>(new FMemoryWriter(Bytes), InitialIndent)
		, Bytes()
		, OutString(InOutString)
	{ }

private:

	TArray<uint8> Bytes;
	FString* OutString;
};


template <class CharType = TCHAR, class PrintPolicy = TPrettyJsonPrintPolicy<CharType>>
class TJsonWriterFactory
{
public:

	static TSharedRef<TJsonWriter<CharType, PrintPolicy>> Create( FArchive* const Stream, int32 InitialIndent = 0 )
	{
		return TJsonWriter< CharType, PrintPolicy >::Create(Stream, InitialIndent);
	}

	static TSharedRef<TJsonWriter<TCHAR, PrintPolicy>> Create( FString* const Stream, int32 InitialIndent = 0 )
	{
		return StaticCastSharedRef<TJsonWriter<TCHAR, PrintPolicy>>(TJsonStringWriter<PrintPolicy>::Create(Stream, InitialIndent));
	}
};
