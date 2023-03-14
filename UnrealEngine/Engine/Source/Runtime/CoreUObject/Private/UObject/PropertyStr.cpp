// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/UnrealType.h"
#include "Misc/StringBuilder.h"
#include "Misc/AsciiSet.h"

/*-----------------------------------------------------------------------------
	FStrProperty.
-----------------------------------------------------------------------------*/
IMPLEMENT_FIELD(FStrProperty)

FStrProperty::FStrProperty(FFieldVariant InOwner, const UECodeGen_Private::FStrPropertyParams& Prop)
	: FStrProperty_Super(InOwner, (const UECodeGen_Private::FPropertyParamsBaseWithOffset&)Prop)
{
}

EConvertFromTypeResult FStrProperty::ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct)
{
	// Convert serialized text to string.
	if (Tag.Type==NAME_TextProperty)
	{ 
		FText Text;
		Slot << Text;
		const FString String = FTextInspector::GetSourceString(Text) ? *FTextInspector::GetSourceString(Text) : TEXT("");
		SetPropertyValue_InContainer(Data, String, Tag.ArrayIndex);

		return EConvertFromTypeResult::Converted;
	}

	return EConvertFromTypeResult::UseSerializeItem;
}

FString FStrProperty::GetCPPTypeForwardDeclaration() const
{
	return FString();
}

// Necessary to fix Compiler Error C2026 and C1091
FString FStrProperty::ExportCppHardcodedText(const FString& InSource, const FString& Indent)
{
	constexpr FAsciiSet EscapableCharacters("\\\"\n\r\t");

	auto EstimateExportedStringLength = [&EscapableCharacters](const UTF16CHAR* InStr)
	{
		int32 EstimatedLen = 0;
		while (const UTF16CHAR Char = *InStr++)
		{
			if (EscapableCharacters.Contains(Char))
			{
				// Exported escaped
				EstimatedLen += 2;
			}
			else if (Char > 0x7f)
			{
				// Exported as a UTF-16 sequence
				EstimatedLen += 4;
			}
			else
			{
				// Exported verbatim
				EstimatedLen += 1;
			}
		}
		return EstimatedLen;
	};

	const int32 PreferredLineSize = 256;
	const int32 LinesPerString = 16;

	// Note: This is a no-op on platforms that are using a 16-bit TCHAR
	FTCHARToUTF16 UTF16SourceString(*InSource, InSource.Len() + 1); // include the null terminator

	const bool bUseSubStrings = EstimateExportedStringLength(UTF16SourceString.Get()) > (LinesPerString * PreferredLineSize);
	int32 LineNum = 0;

	TStringBuilder<1024> Result;

	if (bUseSubStrings)
	{
		Result << TEXT("*(FString(");
	}

	const UTF16CHAR* SourceBegin = UTF16SourceString.Get();
	const UTF16CHAR* SourceIt = SourceBegin;
	do
	{
		if (SourceIt > SourceBegin)
		{
			Result << TEXT("\n");
			Result << Indent;
		}

		++LineNum;
		if (bUseSubStrings && (LineNum % LinesPerString) == 0)
		{
			Result << TEXT(") + FString(");
		}

		Result << TEXT("TEXT(\"");
		{
			int32 ResultStartLen = Result.Len();
			while (*SourceIt && (Result.Len() - ResultStartLen) < PreferredLineSize)
			{
				if (EscapableCharacters.Contains(*SourceIt))
				{
					const TCHAR CharToEscape = (TCHAR)*SourceIt++;

					Result << TEXT('\\');
					switch (CharToEscape)
					{
					case TEXT('\n'):
						Result << TEXT('n');
						break;
					case TEXT('\r'):
						Result << TEXT('r');
						break;
					case TEXT('\t'):
						Result << TEXT('t');
						break;
					default:
						Result << CharToEscape;
						break;
					}
				}
				else if (*SourceIt > 0x7f)
				{
					// If this character is part of a surrogate pair, then combine them and write them as a single UTF-32 sequence
					// Otherwise just write out the character as a UTF-16 sequence
					if (StringConv::IsHighSurrogate(*SourceIt) && StringConv::IsLowSurrogate(*(SourceIt + 1)))
					{
						const UTF32CHAR Codepoint = StringConv::EncodeSurrogate(*SourceIt, *(SourceIt + 1));
						Result.Appendf(TEXT("\\U%08x"), Codepoint);
						SourceIt += 2;
					}
					else
					{
						Result.Appendf(TEXT("\\u%04x"), *SourceIt++);
					}
				}
				else
				{
					Result << (TCHAR)*SourceIt++;
				}
			}
		}
		Result << TEXT("\")");
	}
	while (*SourceIt);

	if (bUseSubStrings)
	{
		Result << TEXT("))");
	}

	return Result.ToString();
}

void FStrProperty::ExportText_Internal( FString& ValueStr, const void* PropertyValueOrContainer, EPropertyPointerType PropertyPointerType, const void* DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope ) const
{
	FString StringValue;
	if (PropertyPointerType == EPropertyPointerType::Container && HasGetter())
	{
		GetValue_InContainer(PropertyValueOrContainer, &StringValue);
	}
	else
	{
		StringValue = *(FString*)PointerToValuePtr(PropertyValueOrContainer, PropertyPointerType);
	}

	if (0 != (PortFlags & PPF_ExportCpp))
	{
		ValueStr += FString::Printf(TEXT("FString(%s)"), *ExportCppHardcodedText(StringValue, FString()));
	}
	else if (!(PortFlags & PPF_Delimited))
	{
		ValueStr += StringValue;
	}
	else if ( StringValue.Len() > 0 )
	{
		ValueStr += FString::Printf( TEXT("\"%s\""), *(StringValue.ReplaceCharWithEscapedChar()) );
	}
	else
	{
		ValueStr += TEXT("\"\"");
	}
}
const TCHAR* FStrProperty::ImportText_Internal( const TCHAR* Buffer, void* ContainerOrPropertyPtr, EPropertyPointerType PropertyPointerType, UObject* Parent, int32 PortFlags, FOutputDevice* ErrorText ) const
{
	FString ImportedText;
	if( !(PortFlags & PPF_Delimited) )
	{
		ImportedText = Buffer;

		// in order to indicate that the value was successfully imported, advance the buffer past the last character that was imported
		Buffer += FCString::Strlen(Buffer);
	}
	else
	{
		// require quoted string here
		if (*Buffer != TCHAR('"'))
		{
			ErrorText->Logf(TEXT("Missing opening '\"' in string property value: %s"), Buffer);
			return NULL;
		}
		const TCHAR* Start = Buffer;
		FString Temp;
		Buffer = FPropertyHelpers::ReadToken(Buffer, Temp);
		if (Buffer == NULL)
		{
			return NULL;
		}
		if (Buffer > Start && Buffer[-1] != TCHAR('"'))
		{
			ErrorText->Logf(TEXT("Missing terminating '\"' in string property value: %s"), Start);
			return NULL;
		}
		ImportedText = MoveTemp(Temp);
	}
	if (PropertyPointerType == EPropertyPointerType::Container && HasSetter())
	{
		SetValue_InContainer(ContainerOrPropertyPtr, ImportedText);
	}
	else
	{
		*(FString*)PointerToValuePtr(ContainerOrPropertyPtr, PropertyPointerType) = MoveTemp(ImportedText);
	}
	return Buffer;
}

uint32 FStrProperty::GetValueTypeHashInternal(const void* Src) const
{
	return GetTypeHash(*(const FString*)Src);
}
