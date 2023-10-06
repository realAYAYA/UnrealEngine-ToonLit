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

EConvertFromTypeResult FStrProperty::ConvertFromType(const FPropertyTag& Tag, FStructuredArchive::FSlot Slot, uint8* Data, UStruct* DefaultsStruct, const uint8* Defaults)
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

	if (!(PortFlags & PPF_Delimited))
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
