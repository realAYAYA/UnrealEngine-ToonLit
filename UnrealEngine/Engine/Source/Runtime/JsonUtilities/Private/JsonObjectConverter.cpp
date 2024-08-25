// Copyright Epic Games, Inc. All Rights Reserved.

#include "JsonObjectConverter.h"
#include "Internationalization/Culture.h"
#include "Misc/PackageName.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"
#include "UObject/PropertyPortFlags.h"
#include "UObject/Package.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "JsonObjectWrapper.h"

#define LOCTEXT_NAMESPACE "JsonObjectConverter"

FString FJsonObjectConverter::StandardizeCase(const FString &StringIn)
{
	// this probably won't work for all cases, consider downcasing the string fully
	FString FixedString = StringIn;
	FixedString[0] = FChar::ToLower(FixedString[0]); // our JSON classes/variable start lower case
	FixedString.ReplaceInline(TEXT("ID"), TEXT("Id"), ESearchCase::CaseSensitive); // Id is standard instead of ID, some of our fnames use ID
	return FixedString;
}


namespace
{
	const FString ObjectClassNameKey = "_ClassName";

	const FName NAME_DateTime(TEXT("DateTime"));

/** Convert property to JSON, assuming either the property is not an array or the value is an individual array element */
TSharedPtr<FJsonValue> ConvertScalarFPropertyToJsonValue(FProperty* Property, const void* Value, int64 CheckFlags, int64 SkipFlags, const FJsonObjectConverter::CustomExportCallback* ExportCb, FProperty* OuterProperty, EJsonObjectConversionFlags ConversionFlags)
{
	// See if there's a custom export callback first, so it can override default behavior
	if (ExportCb && ExportCb->IsBound())
	{
		TSharedPtr<FJsonValue> CustomValue = ExportCb->Execute(Property, Value);
		if (CustomValue.IsValid())
		{
			return CustomValue;
		}
		// fall through to default cases
	}

	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		// export enums as strings
		UEnum* EnumDef = EnumProperty->GetEnum();
		FString StringValue = EnumDef->GetAuthoredNameStringByValue(EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(Value));
		return MakeShared<FJsonValueString>(StringValue);
	}
	else if (FNumericProperty *NumericProperty = CastField<FNumericProperty>(Property))
	{
		// see if it's an enum
		UEnum* EnumDef = NumericProperty->GetIntPropertyEnum();
		if (EnumDef != NULL)
		{
			// export enums as strings
			FString StringValue = EnumDef->GetAuthoredNameStringByValue(NumericProperty->GetSignedIntPropertyValue(Value));
			return MakeShared<FJsonValueString>(StringValue);
		}

		// We want to export numbers as numbers
		if (NumericProperty->IsFloatingPoint())
		{
			return MakeShared<FJsonValueNumber>(NumericProperty->GetFloatingPointPropertyValue(Value));
		}
		else if (NumericProperty->IsInteger())
		{
			return MakeShared<FJsonValueNumber>(NumericProperty->GetSignedIntPropertyValue(Value));
		}

		// fall through to default
	}
	else if (FBoolProperty *BoolProperty = CastField<FBoolProperty>(Property))
	{
		// Export bools as bools
		return MakeShared<FJsonValueBoolean>(BoolProperty->GetPropertyValue(Value));
	}
	else if (FStrProperty *StringProperty = CastField<FStrProperty>(Property))
	{
		return MakeShared<FJsonValueString>(StringProperty->GetPropertyValue(Value));
	}
	else if (FTextProperty *TextProperty = CastField<FTextProperty>(Property))
	{
		return MakeShared<FJsonValueString>(TextProperty->GetPropertyValue(Value).ToString());
	}
	else if (FArrayProperty *ArrayProperty = CastField<FArrayProperty>(Property))
	{
		TArray< TSharedPtr<FJsonValue> > Out;
		FScriptArrayHelper Helper(ArrayProperty, Value);
		for (int32 i=0, n=Helper.Num(); i<n; ++i)
		{
			TSharedPtr<FJsonValue> Elem = FJsonObjectConverter::UPropertyToJsonValue(ArrayProperty->Inner, Helper.GetRawPtr(i), CheckFlags & ( ~CPF_ParmFlags ), SkipFlags, ExportCb, ArrayProperty);
			if ( Elem.IsValid() )
			{
				// add to the array
				Out.Push(Elem);
			}
		}
		return MakeShared<FJsonValueArray>(Out);
	}
	else if ( FSetProperty* SetProperty = CastField<FSetProperty>(Property) )
	{
		TArray< TSharedPtr<FJsonValue> > Out;
		FScriptSetHelper Helper(SetProperty, Value);
		for (FScriptSetHelper::FIterator It(Helper); It; ++It)
		{
			TSharedPtr<FJsonValue> Elem = FJsonObjectConverter::UPropertyToJsonValue(SetProperty->ElementProp, Helper.GetElementPtr(It), CheckFlags & (~CPF_ParmFlags), SkipFlags, ExportCb, SetProperty);
			if (Elem.IsValid())
			{
				// add to the array
				Out.Push(Elem);
			}
		}
		return MakeShared<FJsonValueArray>(Out);
	}
	else if ( FMapProperty* MapProperty = CastField<FMapProperty>(Property) )
	{
		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();

		FScriptMapHelper Helper(MapProperty, Value);
		for (FScriptMapHelper::FIterator It(Helper); It; ++It)
		{
			TSharedPtr<FJsonValue> KeyElement = FJsonObjectConverter::UPropertyToJsonValue(MapProperty->KeyProp, Helper.GetKeyPtr(It), CheckFlags & (~CPF_ParmFlags), SkipFlags, ExportCb, MapProperty, ConversionFlags);
			TSharedPtr<FJsonValue> ValueElement = FJsonObjectConverter::UPropertyToJsonValue(MapProperty->ValueProp, Helper.GetValuePtr(It), CheckFlags & (~CPF_ParmFlags), SkipFlags, ExportCb, MapProperty, ConversionFlags);
			if (KeyElement.IsValid() && ValueElement.IsValid())
			{
				FString KeyString;
				if (!KeyElement->TryGetString(KeyString))
				{
					MapProperty->KeyProp->ExportTextItem_Direct(KeyString, Helper.GetKeyPtr(It), nullptr, nullptr, 0);
					if (KeyString.IsEmpty())
					{
						UE_LOG(LogJson, Error, TEXT("Unable to convert key to string for property %s."), *MapProperty->GetAuthoredName())
						KeyString = FString::Printf(TEXT("Unparsed Key %d"), It.GetLogicalIndex());
					}
				}

				// Coerce camelCase map keys for Enum/FName properties
				if (CastField<FEnumProperty>(MapProperty->KeyProp) ||
					CastField<FNameProperty>(MapProperty->KeyProp))
				{
					if (!EnumHasAnyFlags(ConversionFlags, EJsonObjectConversionFlags::SkipStandardizeCase))
					{
						KeyString = FJsonObjectConverter::StandardizeCase(KeyString);
					}
				}
				Out->SetField(KeyString, ValueElement);
			}
		}

		return MakeShared<FJsonValueObject>(Out);
	}
	else if (FStructProperty *StructProperty = CastField<FStructProperty>(Property))
	{
		UScriptStruct::ICppStructOps* TheCppStructOps = StructProperty->Struct->GetCppStructOps();
		// Intentionally exclude the JSON Object wrapper, which specifically needs to export JSON in an object representation instead of a string
		if (StructProperty->Struct != FJsonObjectWrapper::StaticStruct() && TheCppStructOps && TheCppStructOps->HasExportTextItem())
		{
			FString OutValueStr;
			TheCppStructOps->ExportTextItem(OutValueStr, Value, nullptr, nullptr, PPF_None, nullptr);
			return MakeShared<FJsonValueString>(OutValueStr);
		}

		TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();
		if (FJsonObjectConverter::UStructToJsonObject(StructProperty->Struct, Value, Out, CheckFlags & (~CPF_ParmFlags), SkipFlags, ExportCb, ConversionFlags))
		{
			return MakeShared<FJsonValueObject>(Out);
		}
	}
	else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
	{
		// Instanced properties should be copied by value, while normal UObject* properties should output as asset references
		UObject* Object = ObjectProperty->GetObjectPropertyValue(Value);
		if (Object && (ObjectProperty->HasAnyPropertyFlags(CPF_PersistentInstance) || (OuterProperty && OuterProperty->HasAnyPropertyFlags(CPF_PersistentInstance))))
		{
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();

			Out->SetStringField(ObjectClassNameKey, Object->GetClass()->GetPathName());
			if (FJsonObjectConverter::UStructToJsonObject(ObjectProperty->GetObjectPropertyValue(Value)->GetClass(), Object, Out, CheckFlags, SkipFlags, ExportCb))
			{
				TSharedRef<FJsonValueObject> JsonObject = MakeShared<FJsonValueObject>(Out);
				JsonObject->Type = EJson::Object;
				return JsonObject;
			}
		}
		else
		{
			FString StringValue;
			Property->ExportTextItem_Direct(StringValue, Value, nullptr, nullptr, PPF_None);
			return MakeShared<FJsonValueString>(StringValue);
		}
	}
	else
	{
		// Default to export as string for everything else
		FString StringValue;
		Property->ExportTextItem_Direct(StringValue, Value, NULL, NULL, PPF_None);
		return MakeShared<FJsonValueString>(StringValue);
	}

	// invalid
	return TSharedPtr<FJsonValue>();
}
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS

TSharedPtr<FJsonValue> FJsonObjectConverter::ObjectJsonCallback(FProperty* Property, const void* Value)
{
	if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
	{
		if (!ObjectProperty->HasAnyFlags(RF_Transient)) // We are taking Transient to mean we don't want to serialize to JSON either (could make a new flag if necessary)
		{
			TSharedRef<FJsonObject> Out = MakeShared<FJsonObject>();

			CustomExportCallback CustomCB;
			CustomCB.BindStatic(FJsonObjectConverter::ObjectJsonCallback);

			void** PtrToValuePtr = (void**)Value;

			if (FJsonObjectConverter::UStructToJsonObject(ObjectProperty->PropertyClass, (*PtrToValuePtr), Out, 0, 0, &CustomCB)) //-V549
			{
				return MakeShared<FJsonValueObject>(Out);
			}
		}
	}

	// invalid
	return TSharedPtr<FJsonValue>();
}

PRAGMA_ENABLE_DEPRECATION_WARNINGS

TSharedPtr<FJsonValue> FJsonObjectConverter::UPropertyToJsonValue(FProperty* Property, const void* Value, int64 CheckFlags, int64 SkipFlags, const CustomExportCallback* ExportCb, FProperty* OuterProperty, EJsonObjectConversionFlags ConversionFlags)
{
	if (Property->ArrayDim == 1)
	{
		return ConvertScalarFPropertyToJsonValue(Property, Value, CheckFlags, SkipFlags, ExportCb, OuterProperty, ConversionFlags);
	}

	TArray< TSharedPtr<FJsonValue> > Array;
	for (int Index = 0; Index != Property->ArrayDim; ++Index)
	{
		Array.Add(ConvertScalarFPropertyToJsonValue(Property, (char*)Value + Index * Property->ElementSize, CheckFlags, SkipFlags, ExportCb, OuterProperty, ConversionFlags));
	}
	return MakeShared<FJsonValueArray>(Array);
}

bool FJsonObjectConverter::UStructToJsonObject(const UStruct* StructDefinition, const void* Struct, TSharedRef<FJsonObject> OutJsonObject, int64 CheckFlags, int64 SkipFlags, const CustomExportCallback* ExportCb, EJsonObjectConversionFlags ConversionFlags)
{
	return UStructToJsonAttributes(StructDefinition, Struct, OutJsonObject->Values, CheckFlags, SkipFlags, ExportCb, ConversionFlags);
}

bool FJsonObjectConverter::UStructToJsonAttributes(const UStruct* StructDefinition, const void* Struct, TMap< FString, TSharedPtr<FJsonValue> >& OutJsonAttributes, int64 CheckFlags, int64 SkipFlags, const CustomExportCallback* ExportCb, EJsonObjectConversionFlags ConversionFlags)
{
	if (SkipFlags == 0)
	{
		// If we have no specified skip flags, skip deprecated, transient and skip serialization by default when writing
		SkipFlags |= CPF_Deprecated | CPF_Transient;
	}

	if (StructDefinition == FJsonObjectWrapper::StaticStruct())
	{
		// Just copy it into the object
		const FJsonObjectWrapper* ProxyObject = (const FJsonObjectWrapper *)Struct;

		if (ProxyObject->JsonObject.IsValid())
		{
			OutJsonAttributes = ProxyObject->JsonObject->Values;
		}
		return true;
	}

	for (TFieldIterator<FProperty> It(StructDefinition); It; ++It)
	{
		FProperty* Property = *It;

		// Check to see if we should ignore this property
		if (CheckFlags != 0 && !Property->HasAnyPropertyFlags(CheckFlags))
		{
			continue;
		}
		if (Property->HasAnyPropertyFlags(SkipFlags))
		{
			continue;
		}

		FString VariableName = Property->GetAuthoredName();
		if (!EnumHasAnyFlags(ConversionFlags, EJsonObjectConversionFlags::SkipStandardizeCase))
		{
			VariableName = StandardizeCase(VariableName);
		}

		const void* Value = Property->ContainerPtrToValuePtr<uint8>(Struct);

		// convert the property to a FJsonValue
		TSharedPtr<FJsonValue> JsonValue = UPropertyToJsonValue(Property, Value, CheckFlags, SkipFlags, ExportCb, nullptr, ConversionFlags);
		if (!JsonValue.IsValid())
		{
			FFieldClass* PropClass = Property->GetClass();
			UE_LOG(LogJson, Error, TEXT("UStructToJsonObject - Unhandled property type '%s': %s"), *PropClass->GetName(), *Property->GetPathName());
			return false;
		}

		// set the value on the output object
		OutJsonAttributes.Add(VariableName, JsonValue);
	}

	return true;
}

template<class CharType, class PrintPolicy>
bool UStructToJsonObjectStringInternal(const TSharedRef<FJsonObject>& JsonObject, FString& OutJsonString, int32 Indent)
{
	TSharedRef<TJsonWriter<CharType, PrintPolicy> > JsonWriter = TJsonWriterFactory<CharType, PrintPolicy>::Create(&OutJsonString, Indent);
	bool bSuccess = FJsonSerializer::Serialize(JsonObject, JsonWriter);
	JsonWriter->Close();
	return bSuccess;
}

bool FJsonObjectConverter::UStructToJsonObjectString(const UStruct* StructDefinition, const void* Struct, FString& OutJsonString, int64 CheckFlags, int64 SkipFlags, int32 Indent, const CustomExportCallback* ExportCb, bool bPrettyPrint)
{
	TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
	if (UStructToJsonObject(StructDefinition, Struct, JsonObject, CheckFlags, SkipFlags, ExportCb))
	{
		bool bSuccess = false;
		if (bPrettyPrint)
		{
			bSuccess = UStructToJsonObjectStringInternal<TCHAR, TPrettyJsonPrintPolicy<TCHAR> >(JsonObject, OutJsonString, Indent);
		}
		else
		{
			bSuccess = UStructToJsonObjectStringInternal<TCHAR, TCondensedJsonPrintPolicy<TCHAR> >(JsonObject, OutJsonString, Indent);
		}
		if (bSuccess)
		{
			return true;
		}
		else
		{
			UE_LOG(LogJson, Warning, TEXT("UStructToJsonObjectString - Unable to write out JSON"));
		}
	}

	return false;
}

//static
bool FJsonObjectConverter::GetTextFromObject(const TSharedRef<FJsonObject>& Obj, FText& TextOut)
{
	// get the prioritized culture name list
	FCultureRef CurrentCulture = FInternationalization::Get().GetCurrentCulture();
	TArray<FString> CultureList = CurrentCulture->GetPrioritizedParentCultureNames();

	// try to follow the fall back chain that the engine uses
	FString TextString;
	for (const FString& CultureCode : CultureList)
	{
		if (Obj->TryGetStringField(CultureCode, TextString))
		{
			TextOut = FText::FromString(TextString);
			return true;
		}
	}

	// try again but only search on the locale region (in the localized data). This is a common omission (i.e. en-US source text should be used if no en is defined)
	for (const FString& LocaleToMatch : CultureList)
	{
		int32 SeparatorPos;
		// only consider base language entries in culture chain (i.e. "en")
		if (!LocaleToMatch.FindChar('-', SeparatorPos))
		{
			for (const auto& Pair : Obj->Values)
			{
				// only consider coupled entries now (base ones would have been matched on first path) (i.e. "en-US")
				if (Pair.Key.FindChar('-', SeparatorPos))
				{
					if (Pair.Key.StartsWith(LocaleToMatch))
					{
						TextOut = FText::FromString(Pair.Value->AsString());
						return true;
					}
				}
			}
		}
	}

	// no luck, is this possibly an unrelated JSON object?
	return false;
}


namespace
{
	bool JsonValueToFPropertyWithContainer(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* OutValue, const UStruct* ContainerStruct, void* Container, int64 CheckFlags, int64 SkipFlags, const bool bStrictMode, FText* OutFailReason);
	bool JsonAttributesToUStructWithContainer(const TMap< FString, TSharedPtr<FJsonValue> >& JsonAttributes, const UStruct* StructDefinition, void* OutStruct, const UStruct* ContainerStruct, void* Container, int64 CheckFlags, int64 SkipFlags, const bool bStrictMode, FText* OutFailReason);

	/** Convert JSON to property, assuming either the property is not an array or the value is an individual array element */
	bool ConvertScalarJsonValueToFPropertyWithContainer(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* OutValue, const UStruct* ContainerStruct, void* Container, int64 CheckFlags, int64 SkipFlags, const bool bStrictMode, FText* OutFailReason)
	{
	if (FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		if (JsonValue->Type == EJson::String)
		{
			// see if we were passed a string for the enum
			const UEnum* Enum = EnumProperty->GetEnum();
			check(Enum);
			FString StrValue = JsonValue->AsString();
			int64 IntValue = Enum->GetValueByName(FName(*StrValue), EGetByNameFlags::CheckAuthoredName);
			if (IntValue == INDEX_NONE)
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import enum %s from string value %s for property %s"), *Enum->CppType, *StrValue, *Property->GetAuthoredName());
				if (OutFailReason)
				{
					*OutFailReason = FText::Format(LOCTEXT("FailImportEnumFromString", "Unable to import enum {0} from string value {1} for property {2}"), FText::FromString(Enum->CppType), FText::FromString(StrValue), FText::FromString(Property->GetAuthoredName()));
				}
				return false;
			}
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(OutValue, IntValue);
		}
		else
		{
			// AsNumber will log an error for completely inappropriate types (then give us a default)
			EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(OutValue, (int64)JsonValue->AsNumber());
		}
	}
	else if (FNumericProperty *NumericProperty = CastField<FNumericProperty>(Property))
	{
		if (NumericProperty->IsEnum() && JsonValue->Type == EJson::String)
		{
			// see if we were passed a string for the enum
			const UEnum* Enum = NumericProperty->GetIntPropertyEnum();
			check(Enum); // should be assured by IsEnum()
			FString StrValue = JsonValue->AsString();
			int64 IntValue = Enum->GetValueByName(FName(*StrValue), EGetByNameFlags::CheckAuthoredName);
			if (IntValue == INDEX_NONE)
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import enum %s from numeric value %s for property %s"), *Enum->CppType, *StrValue, *Property->GetAuthoredName());
				if (OutFailReason)
				{
					*OutFailReason = FText::Format(LOCTEXT("FailImportEnumFromNumeric", "Unable to import enum {0} from numeric value {1} for property {2}"), FText::FromString(Enum->CppType), FText::FromString(StrValue), FText::FromString(Property->GetAuthoredName()));
				}
				return false;
			}
			NumericProperty->SetIntPropertyValue(OutValue, IntValue);
		}
		else if (NumericProperty->IsFloatingPoint())
		{
			// AsNumber will log an error for completely inappropriate types (then give us a default)
			NumericProperty->SetFloatingPointPropertyValue(OutValue, JsonValue->AsNumber());
		}
		else if (NumericProperty->IsInteger())
		{
			if (JsonValue->Type == EJson::String)
			{
				// parse string -> int64 ourselves so we don't lose any precision going through AsNumber (aka double)
				NumericProperty->SetIntPropertyValue(OutValue, FCString::Atoi64(*JsonValue->AsString()));
			}
			else
			{
				// AsNumber will log an error for completely inappropriate types (then give us a default)
				NumericProperty->SetIntPropertyValue(OutValue, (int64)JsonValue->AsNumber());
			}
		}
		else
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import json value into %s numeric property %s"), *Property->GetClass()->GetName(), *Property->GetAuthoredName());
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailImportNumericProperty", "Unable to import json value into {0} numeric property {1}"), FText::FromString(Property->GetClass()->GetName()), FText::FromString(Property->GetAuthoredName()));
			}
			return false;
		}
	}
	else if (FBoolProperty *BoolProperty = CastField<FBoolProperty>(Property))
	{
		// AsBool will log an error for completely inappropriate types (then give us a default)
		BoolProperty->SetPropertyValue(OutValue, JsonValue->AsBool());
	}
	else if (FStrProperty *StringProperty = CastField<FStrProperty>(Property))
	{
		// AsString will log an error for completely inappropriate types (then give us a default)
		StringProperty->SetPropertyValue(OutValue, JsonValue->AsString());
	}
	else if (FArrayProperty *ArrayProperty = CastField<FArrayProperty>(Property))
	{
		if (JsonValue->Type == EJson::Array)
		{
			TArray< TSharedPtr<FJsonValue> > ArrayValue = JsonValue->AsArray();
			int32 ArrLen = ArrayValue.Num();

			// make the output array size match
			FScriptArrayHelper Helper(ArrayProperty, OutValue);
			Helper.Resize(ArrLen);

			// set the property values
			for (int32 i = 0; i < ArrLen; ++i)
			{
				const TSharedPtr<FJsonValue>& ArrayValueItem = ArrayValue[i];
				if (ArrayValueItem.IsValid() && !ArrayValueItem->IsNull())
				{
					if (!JsonValueToFPropertyWithContainer(ArrayValueItem, ArrayProperty->Inner, Helper.GetRawPtr(i), ContainerStruct, Container, CheckFlags & (~CPF_ParmFlags), SkipFlags, bStrictMode, OutFailReason))
					{
						UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import Array element %d for property %s"), i, *Property->GetAuthoredName());
						if (OutFailReason)
						{
							*OutFailReason = FText::Format(LOCTEXT("FailImportArrayElement", "Unable to import Array element {0} for property {1}\n{2}"), FText::AsNumber(i), FText::FromString(Property->GetAuthoredName()), *OutFailReason);
						}
						return false;
					}
				}
			}
		}
		else
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import non-array JSON value into Array property %s"), *Property->GetAuthoredName());
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailImportArray", "Unable to import non-array JSON value into Array property {0}"), FText::FromString(Property->GetAuthoredName()));
			}
			return false;
		}
	}
	else if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
	{
		if (JsonValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> ObjectValue = JsonValue->AsObject();

			FScriptMapHelper Helper(MapProperty, OutValue);

			check(ObjectValue);

			int32 MapSize = ObjectValue->Values.Num();
			Helper.EmptyValues(MapSize);

			// set the property values
			for (const auto& Entry : ObjectValue->Values)
			{
				if (Entry.Value.IsValid() && !Entry.Value->IsNull())
				{
					int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();

					TSharedPtr<FJsonValueString> TempKeyValue = MakeShared<FJsonValueString>(Entry.Key);

					if (!JsonValueToFPropertyWithContainer(TempKeyValue, MapProperty->KeyProp, Helper.GetKeyPtr(NewIndex), ContainerStruct, Container, CheckFlags & (~CPF_ParmFlags), SkipFlags, bStrictMode, OutFailReason))
					{
						UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import Map element %s key for property %s"), *Entry.Key, *Property->GetAuthoredName());
						if (OutFailReason)
						{
							*OutFailReason = FText::Format(LOCTEXT("FailImportMapElementKey", "Unable to import Map element {0} key for property {1}\n{2}"), FText::FromString(Entry.Key), FText::FromString(Property->GetAuthoredName()), *OutFailReason);
						}
						return false;
					}

					if (!JsonValueToFPropertyWithContainer(Entry.Value, MapProperty->ValueProp, Helper.GetValuePtr(NewIndex), ContainerStruct, Container, CheckFlags & (~CPF_ParmFlags), SkipFlags, bStrictMode, OutFailReason))
					{
						UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import Map element %s value for property %s"), *Entry.Key, *Property->GetAuthoredName());
						if (OutFailReason)
						{
							*OutFailReason = FText::Format(LOCTEXT("FailImportMapElementValue", "Unable to import Map element {0} value for property {1}\n{2}"), FText::FromString(Entry.Key), FText::FromString(Property->GetAuthoredName()), *OutFailReason);
						}
						return false;
					}
				}
			}

			Helper.Rehash();
		}
		else
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import non-object JSON value into Map property %s"), *Property->GetAuthoredName());
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailImportMap", "Unable to import non-object JSON value into Map property {0}"), FText::FromString(Property->GetAuthoredName()));
			}
			return false;
		}
	}
	else if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
	{
		if (JsonValue->Type == EJson::Array)
		{
			TArray< TSharedPtr<FJsonValue> > ArrayValue = JsonValue->AsArray();
			int32 ArrLen = ArrayValue.Num();

			FScriptSetHelper Helper(SetProperty, OutValue);
			Helper.EmptyElements(ArrLen);

			// set the property values
			for (int32 i = 0; i < ArrLen; ++i)
			{
				const TSharedPtr<FJsonValue>& ArrayValueItem = ArrayValue[i];
				if (ArrayValueItem.IsValid() && !ArrayValueItem->IsNull())
				{
					int32 NewIndex = Helper.AddDefaultValue_Invalid_NeedsRehash();
					if (!JsonValueToFPropertyWithContainer(ArrayValueItem, SetProperty->ElementProp, Helper.GetElementPtr(NewIndex), ContainerStruct, Container, CheckFlags & (~CPF_ParmFlags), SkipFlags, bStrictMode, OutFailReason))
					{
						UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import Set element %d for property %s"), i, *Property->GetAuthoredName());
						if (OutFailReason)
						{
							*OutFailReason = FText::Format(LOCTEXT("FailImportSetElement", "Unable to import Set element {0} for property {1}\n{2}"), FText::AsNumber(i), FText::FromString(Property->GetAuthoredName()), *OutFailReason);
						}
						return false;
					}
				}
			}

			Helper.Rehash();
		}
		else
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import non-array JSON value into Set property %s"), *Property->GetAuthoredName());
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailImportSet", "Unable to import non-array JSON value into Set property {0}"), FText::FromString(Property->GetAuthoredName()));
			}
			return false;
		}
	}
	else if (FTextProperty* TextProperty = CastField<FTextProperty>(Property))
	{
		if (JsonValue->Type == EJson::String)
		{
			// assume this string is already localized, so import as invariant
			TextProperty->SetPropertyValue(OutValue, FText::FromString(JsonValue->AsString()));
		}
		else if (JsonValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
			check(Obj.IsValid()); // should not fail if Type == EJson::Object

			// import the subvalue as a culture invariant string
			FText Text;
			if (!FJsonObjectConverter::GetTextFromObject(Obj.ToSharedRef(), Text))
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import JSON object with invalid keys into Text property %s"), *Property->GetAuthoredName());
				if (OutFailReason)
				{
					*OutFailReason = FText::Format(LOCTEXT("FailImportTextFromObject", "Unable to import JSON object with invalid keys into Text property {0}"), FText::FromString(Property->GetAuthoredName()));
				}
				return false;
			}
			TextProperty->SetPropertyValue(OutValue, Text);
		}
		else
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import JSON value that is neither string nor object into Text property %s"), *Property->GetAuthoredName());
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailImportText", "Unable to import JSON value that is neither string nor object into Text property {0}"), FText::FromString(Property->GetAuthoredName()));
			}
			return false;
		}
	}
	else if (FStructProperty *StructProperty = CastField<FStructProperty>(Property))
	{
		if (JsonValue->Type == EJson::Object)
		{
			TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
			check(Obj.IsValid()); // should not fail if Type == EJson::Object
			if (!JsonAttributesToUStructWithContainer(Obj->Values, StructProperty->Struct, OutValue, ContainerStruct, Container, CheckFlags & (~CPF_ParmFlags), SkipFlags, bStrictMode, OutFailReason))
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import JSON object into %s property %s"), *StructProperty->Struct->GetAuthoredName(), *Property->GetAuthoredName());
				if (OutFailReason)
				{
					*OutFailReason = FText::Format(LOCTEXT("FailImportStructFromObject", "Unable to import JSON object into {0} property {1}\n{2}"), FText::FromString(StructProperty->Struct->GetAuthoredName()), FText::FromString(Property->GetAuthoredName()), *OutFailReason);
				}
				return false;
			}
		}
		else if (JsonValue->Type == EJson::String && StructProperty->Struct->GetFName() == NAME_LinearColor)
		{
			FLinearColor& ColorOut = *(FLinearColor*)OutValue;
			FString ColorString = JsonValue->AsString();

			FColor IntermediateColor;
			IntermediateColor = FColor::FromHex(ColorString);

			ColorOut = IntermediateColor;
		}
		else if (JsonValue->Type == EJson::String && StructProperty->Struct->GetFName() == NAME_Color)
		{
			FColor& ColorOut = *(FColor*)OutValue;
			FString ColorString = JsonValue->AsString();

			ColorOut = FColor::FromHex(ColorString);
		}
		else if (JsonValue->Type == EJson::String && StructProperty->Struct->GetFName() == NAME_DateTime)
		{
			FString DateString = JsonValue->AsString();
			FDateTime& DateTimeOut = *(FDateTime*)OutValue;
			if (DateString == TEXT("min"))
			{
				// min representable value for our date struct. Actual date may vary by platform (this is used for sorting)
				DateTimeOut = FDateTime::MinValue();
			}
			else if (DateString == TEXT("max"))
			{
				// max representable value for our date struct. Actual date may vary by platform (this is used for sorting)
				DateTimeOut = FDateTime::MaxValue();
			}
			else if (DateString == TEXT("now"))
			{
				// this value's not really meaningful from JSON serialization (since we don't know timezone) but handle it anyway since we're handling the other keywords
				DateTimeOut = FDateTime::UtcNow();
			}
			else if (FDateTime::ParseIso8601(*DateString, DateTimeOut))
			{
				// ok
			}
			else if (FDateTime::Parse(DateString, DateTimeOut))
			{
				// ok
			}
			else
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import JSON string into DateTime property %s"), *Property->GetAuthoredName());
				if (OutFailReason)
				{
					*OutFailReason = FText::Format(LOCTEXT("FailImportDateTimeFromString", "Unable to import JSON string into DateTime property {0}"), FText::FromString(Property->GetAuthoredName()));
				}
				return false;
			}
		}
		else if (JsonValue->Type == EJson::String && StructProperty->Struct->GetCppStructOps() && StructProperty->Struct->GetCppStructOps()->HasImportTextItem())
		{
			UScriptStruct::ICppStructOps* TheCppStructOps = StructProperty->Struct->GetCppStructOps();

			FString ImportTextString = JsonValue->AsString();
			const TCHAR* ImportTextPtr = *ImportTextString;
			if (!TheCppStructOps->ImportTextItem(ImportTextPtr, OutValue, PPF_None, nullptr, (FOutputDevice*)GWarn))
			{
				// Fall back to trying the tagged property approach if custom ImportTextItem couldn't get it done
				if (Property->ImportText_Direct(ImportTextPtr, OutValue, nullptr, PPF_None) == nullptr)
				{
					UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import JSON string into %s property %s"), *StructProperty->Struct->GetAuthoredName(), *Property->GetAuthoredName());
					if (OutFailReason)
					{
						*OutFailReason = FText::Format(LOCTEXT("FailImportStructFromString", "Unable to import JSON string into {0} property {1}"), FText::FromString(StructProperty->Struct->GetAuthoredName()), FText::FromString(Property->GetAuthoredName()));
					}
					return false;
				}
			}
		}
		else if (JsonValue->Type == EJson::String)
		{
			FString ImportTextString = JsonValue->AsString();
			const TCHAR* ImportTextPtr = *ImportTextString;
			if (Property->ImportText_Direct(ImportTextPtr, OutValue, nullptr, PPF_None) == nullptr)
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import JSON string into %s property %s"), *StructProperty->Struct->GetAuthoredName(), *Property->GetAuthoredName());
				if (OutFailReason)
				{
					*OutFailReason = FText::Format(LOCTEXT("FailImportStructFromString", "Unable to import JSON string into {0} property {1}"), FText::FromString(StructProperty->Struct->GetAuthoredName()), FText::FromString(Property->GetAuthoredName()));
				}
				return false;
			}
		}
		else
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import JSON value that is neither string nor object into %s property %s"), *StructProperty->Struct->GetAuthoredName(), *Property->GetAuthoredName());
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailImportStruct", "Unable to import JSON value that is neither string nor object into {0} property {1}"), FText::FromString(StructProperty->Struct->GetAuthoredName()), FText::FromString(Property->GetAuthoredName()));
			}
			return false;
		}
	}
	else if (FObjectProperty *ObjectProperty = CastField<FObjectProperty>(Property))
		{
			if (JsonValue->Type == EJson::Object)
			{
				UObject* Outer = GetTransientPackage();
				if (ContainerStruct->IsChildOf(UObject::StaticClass()))
				{
					Outer = (UObject*)Container;
				}

				TSharedPtr<FJsonObject> Obj = JsonValue->AsObject();
				UClass* PropertyClass = ObjectProperty->PropertyClass;

				// If a specific subclass was stored in the JSON, use that instead of the PropertyClass
				FString ClassString = Obj->GetStringField(ObjectClassNameKey);
				Obj->RemoveField(ObjectClassNameKey);
				if (!ClassString.IsEmpty())
				{
					UClass* FoundClass = FPackageName::IsShortPackageName(ClassString) ? FindFirstObject<UClass>(*ClassString) : UClass::TryFindTypeSlow<UClass>(ClassString);
					if (FoundClass)
					{
						PropertyClass = FoundClass;
					}
				}

				UObject* createdObj = StaticAllocateObject(PropertyClass, Outer, NAME_None, EObjectFlags::RF_NoFlags, EInternalObjectFlags::None, false);
				(*PropertyClass->ClassConstructor)(FObjectInitializer(createdObj, PropertyClass->ClassDefaultObject, EObjectInitializerOptions::None));

				ObjectProperty->SetObjectPropertyValue(OutValue, createdObj);

				check(Obj.IsValid()); // should not fail if Type == EJson::Object
				if (!JsonAttributesToUStructWithContainer(Obj->Values, PropertyClass, createdObj, PropertyClass, createdObj, CheckFlags & (~CPF_ParmFlags), SkipFlags, bStrictMode, OutFailReason))
				{
					UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import JSON object into %s property %s"), *PropertyClass->GetAuthoredName(), *Property->GetAuthoredName());
					if (OutFailReason)
					{
						*OutFailReason = FText::Format(LOCTEXT("FailImportObjectFromObject", "Unable to import JSON object into {0} property {1}\n{2}"), FText::FromString(PropertyClass->GetAuthoredName()), FText::FromString(Property->GetAuthoredName()), *OutFailReason);
					}
					return false;
				}
			}
			else if (JsonValue->Type == EJson::String)
			{
				// Default to expect a string for everything else
				if (Property->ImportText_Direct(*JsonValue->AsString(), OutValue, nullptr, 0) == nullptr)
				{
					UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import JSON string into %s property %s"), *ObjectProperty->PropertyClass->GetAuthoredName(), *Property->GetAuthoredName());
					if (OutFailReason)
					{
						*OutFailReason = FText::Format(LOCTEXT("FailImportObjectFromString", "Unable to import JSON string into {0} property {1}"), FText::FromString(*ObjectProperty->PropertyClass->GetAuthoredName()), FText::FromString(Property->GetAuthoredName()));
					}
					return false;
				}
			}
		}
	else
	{
		// Default to expect a string for everything else
		if (Property->ImportText_Direct(*JsonValue->AsString(), OutValue, nullptr, 0) == nullptr)
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Unable to import JSON string into property %s"), *Property->GetAuthoredName());
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailImportFromString", "Unable to import JSON string into property {0}"), FText::FromString(Property->GetAuthoredName()));
			}
			return false;
		}
	}

	return true;
	}


	bool JsonValueToFPropertyWithContainer(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* OutValue, const UStruct* ContainerStruct, void* Container, int64 CheckFlags, int64 SkipFlags, const bool bStrictMode, FText* OutFailReason)
	{
		if (!JsonValue.IsValid())
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Invalid JSON value"));
			if (OutFailReason)
			{
				*OutFailReason = LOCTEXT("InvalidJsonValue", "Invalid JSON value");
			}
			return false;
		}

		const bool bArrayOrSetProperty = Property->IsA<FArrayProperty>() || Property->IsA<FSetProperty>();
		const bool bJsonArray = JsonValue->Type == EJson::Array;

		if (!bJsonArray)
		{
			if (bArrayOrSetProperty)
			{
				UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Expecting JSON array"));
				if (OutFailReason)
				{
					*OutFailReason = LOCTEXT("ExpectingJsonArray", "Expecting JSON array");
				}
				return false;
			}

			if (Property->ArrayDim != 1)
			{
				if (bStrictMode)
				{
					UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - Property %s is not an array but has %d elements"), *Property->GetAuthoredName(), Property->ArrayDim);
					if (OutFailReason)
					{
						*OutFailReason = FText::Format(LOCTEXT("InvalidDimensionOfNonArrayProperty", "Property {0} is not an array but has {1} elements"), FText::FromString(Property->GetAuthoredName()), FText::AsNumber(Property->ArrayDim));
					}
					return false;
				}
				
				UE_LOG(LogJson, Warning, TEXT("Ignoring excess properties when deserializing %s"), *Property->GetAuthoredName());
			}

			return ConvertScalarJsonValueToFPropertyWithContainer(JsonValue, Property, OutValue, ContainerStruct, Container, CheckFlags, SkipFlags, bStrictMode, OutFailReason);
		}

		// In practice, the ArrayDim == 1 check ought to be redundant, since nested arrays of FProperties are not supported
		if (bArrayOrSetProperty && Property->ArrayDim == 1)
		{
			// Read into TArray
			return ConvertScalarJsonValueToFPropertyWithContainer(JsonValue, Property, OutValue, ContainerStruct, Container, CheckFlags, SkipFlags, bStrictMode, OutFailReason);
		}

		// We're deserializing a JSON array
		const auto& ArrayValue = JsonValue->AsArray();

		if (bStrictMode && (Property->ArrayDim != ArrayValue.Num()))
		{
			UE_LOG(LogJson, Error, TEXT("JsonValueToUProperty - JSON array size is incorrect (has %d elements, but needs %d)"), ArrayValue.Num(), Property->ArrayDim);
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("IncorrectArraySize", "JSON array size is incorrect (has {0} elements, but needs {1})"), FText::AsNumber(ArrayValue.Num()), FText::AsNumber(Property->ArrayDim));
			}
			return false;
		}
		
		if (Property->ArrayDim < ArrayValue.Num())
		{
			UE_LOG(LogJson, Warning, TEXT("Ignoring excess properties when deserializing %s"), *Property->GetAuthoredName());
		}

		// Read into native array
		const int32 ItemsToRead = FMath::Clamp(ArrayValue.Num(), 0, Property->ArrayDim);
		for (int Index = 0; Index != ItemsToRead; ++Index)
		{
			if (!ConvertScalarJsonValueToFPropertyWithContainer(ArrayValue[Index], Property, static_cast<char*>(OutValue) + Index * Property->ElementSize, ContainerStruct, Container, CheckFlags, SkipFlags, bStrictMode, OutFailReason))
			{
				return false;
			}
		}
		return true;
	}

	bool JsonAttributesToUStructWithContainer(const TMap< FString, TSharedPtr<FJsonValue> >& JsonAttributes, const UStruct* StructDefinition, void* OutStruct, const UStruct* ContainerStruct, void* Container, int64 CheckFlags, int64 SkipFlags, const bool bStrictMode, FText* OutFailReason)
	{
		if (StructDefinition == FJsonObjectWrapper::StaticStruct())
		{
			// Just copy it into the object
			FJsonObjectWrapper* ProxyObject = (FJsonObjectWrapper*)OutStruct;
			ProxyObject->JsonObject = MakeShared<FJsonObject>();
			ProxyObject->JsonObject->Values = JsonAttributes;
			return true;
		}

		int32 NumUnclaimedProperties = JsonAttributes.Num();
		if (NumUnclaimedProperties <= 0)
		{
			return true;
		}

		// iterate over the struct properties
		for (TFieldIterator<FProperty> PropIt(StructDefinition); PropIt; ++PropIt)
		{
			FProperty* Property = *PropIt;

			// Check to see if we should ignore this property
			if (CheckFlags != 0 && !Property->HasAnyPropertyFlags(CheckFlags))
			{
				continue;
			}
			if (Property->HasAnyPropertyFlags(SkipFlags))
			{
				continue;
			}

			// find a JSON value matching this property name
			FString PropertyName = StructDefinition->GetAuthoredNameForField(Property);
			const TSharedPtr<FJsonValue>* JsonValue = JsonAttributes.Find(PropertyName);
			
			if (!JsonValue)
			{
				if (bStrictMode)
				{
					UE_LOG(LogJson, Error, TEXT("JsonObjectToUStruct - Missing JSON value named %s"), *PropertyName);
					if (OutFailReason)
					{
						*OutFailReason = FText::Format(LOCTEXT("MissingJsonField", "Missing JSON value named {0}"), FText::FromString(PropertyName));
					}
					return false;
				}
				
				// we allow values to not be found since this mirrors the typical UObject mantra that all the fields are optional when deserializing
				continue;
			}

			if (JsonValue->IsValid() && !(*JsonValue)->IsNull())
			{
				void* Value = Property->ContainerPtrToValuePtr<uint8>(OutStruct);
				if (!JsonValueToFPropertyWithContainer(*JsonValue, Property, Value, ContainerStruct, Container, CheckFlags, SkipFlags, bStrictMode, OutFailReason))
				{
					UE_LOG(LogJson, Error, TEXT("JsonObjectToUStruct - Unable to import JSON value into property %s"), *PropertyName);
					if (OutFailReason)
					{
						*OutFailReason = FText::Format(LOCTEXT("FailImportValueToProperty", "Unable to import JSON value into property {0}\n{1}"), FText::FromString(PropertyName), *OutFailReason);
					}
					return false;
				}
			}

			if (--NumUnclaimedProperties <= 0)
			{
				// Should we log a warning/error if we still have properties in the JSON data that aren't in the struct definition in strict mode?
				
				// If we found all properties that were in the JsonAttributes map, there is no reason to keep looking for more.
				break;
			}
		}

		return true;
	}
}

bool FJsonObjectConverter::JsonValueToUProperty(const TSharedPtr<FJsonValue>& JsonValue, FProperty* Property, void* OutValue, int64 CheckFlags, int64 SkipFlags, const bool bStrictMode, FText* OutFailReason)
{
	return JsonValueToFPropertyWithContainer(JsonValue, Property, OutValue, nullptr, nullptr, CheckFlags, SkipFlags, bStrictMode, OutFailReason);
}

bool FJsonObjectConverter::JsonObjectToUStruct(const TSharedRef<FJsonObject>& JsonObject, const UStruct* StructDefinition, void* OutStruct, int64 CheckFlags, int64 SkipFlags, const bool bStrictMode, FText* OutFailReason)
{
	return JsonAttributesToUStruct(JsonObject->Values, StructDefinition, OutStruct, CheckFlags, SkipFlags, bStrictMode, OutFailReason);
}

bool FJsonObjectConverter::JsonAttributesToUStruct(const TMap< FString, TSharedPtr<FJsonValue> >& JsonAttributes, const UStruct* StructDefinition, void* OutStruct, int64 CheckFlags, int64 SkipFlags, const bool bStrictMode, FText* OutFailReason)
{
	return JsonAttributesToUStructWithContainer(JsonAttributes, StructDefinition, OutStruct, StructDefinition, OutStruct, CheckFlags, SkipFlags, bStrictMode, OutFailReason);
}

//static 
bool FJsonObjectConverter::GetTextFromField(const FString& FieldName, const TSharedPtr<FJsonValue>& FieldValue, FText& TextOut)
{
	if (FieldValue.IsValid())
	{
		switch (FieldValue->Type)
		{
			case EJson::Number:
			{
				// number
				TextOut = FText::AsNumber(FieldValue->AsNumber());
				return true;
			}
			case EJson::String:
			{
				if (FieldName.StartsWith(TEXT("date-")))
				{
					FDateTime Dte;
					if (FDateTime::ParseIso8601(*FieldValue->AsString(), Dte))
					{
						TextOut = FText::AsDate(Dte);
						return true;
					}
				}
				else if (FieldName.StartsWith(TEXT("datetime-")))
				{
					FDateTime Dte;
					if (FDateTime::ParseIso8601(*FieldValue->AsString(), Dte))
					{
						TextOut = FText::AsDateTime(Dte);
						return true;
					}
				}
				else
				{
				// culture invariant string
					TextOut = FText::FromString(FieldValue->AsString());
					return true;
				}
				break;
			}
			case EJson::Object:
			{
				// localized string
				if (FJsonObjectConverter::GetTextFromObject(FieldValue->AsObject().ToSharedRef(), TextOut))
				{
					return true;
				}

				UE_LOG(LogJson, Error, TEXT("Unable to apply JSON parameter %s (could not parse object)"), *FieldName);
				break;
			}
			default:
			{
				UE_LOG(LogJson, Error, TEXT("Unable to apply JSON parameter %s (bad type)"), *FieldName);
				break;
			}
		}
	}
	return false;
}

FFormatNamedArguments FJsonObjectConverter::ParseTextArgumentsFromJson(const TSharedPtr<const FJsonObject>& JsonObject)
{
	FFormatNamedArguments NamedArgs;
	if (JsonObject.IsValid())
	{
		for (const auto& It : JsonObject->Values)
		{
			FText TextValue;
			if (GetTextFromField(It.Key, It.Value, TextValue))
			{
				NamedArgs.Emplace(It.Key, TextValue);
			}
		}
	}
	return NamedArgs;
}

const FJsonObjectConverter::CustomExportCallback FJsonObjectConverter::ExportCallback_WriteISO8601Dates = 
	FJsonObjectConverter::CustomExportCallback::CreateLambda(
		[](FProperty* Prop, const void* Data) -> TSharedPtr<FJsonValue>
		{
			if (FStructProperty* StructProperty = CastField<FStructProperty>(Prop))
			{
				checkSlow(StructProperty->Struct);
				if (StructProperty->Struct->GetFName() == NAME_DateTime)
				{
					return MakeShared<FJsonValueString>(static_cast<const FDateTime*>(Data)->ToIso8601());
				}
			}
			return {};
		});

#undef LOCTEXT_NAMESPACE
