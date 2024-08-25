// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataTableJSON.h"
#include "UObject/EnumProperty.h"
#include "Engine/DataTable.h"
#include "Serialization/JsonSerializer.h"


namespace
{
	const TCHAR* JSONTypeToString(const EJson InType)
	{
		switch(InType)
		{
		case EJson::None:
			return TEXT("None");
		case EJson::Null:
			return TEXT("Null");
		case EJson::String:
			return TEXT("String");
		case EJson::Number:
			return TEXT("Number");
		case EJson::Boolean:
			return TEXT("Boolean");
		case EJson::Array:
			return TEXT("Array");
		case EJson::Object:
			return TEXT("Object");
		default:
			return TEXT("Unknown");
		}
	}
#if WITH_EDITOR

	template <typename CharType>
	void WriteJSONObjectStartWithOptionalIdentifier(typename TDataTableExporterJSON<CharType>::FDataTableJsonWriter& InJsonWriter, const FString* InIdentifier)
	{
		if (InIdentifier)
		{
			InJsonWriter.WriteObjectStart(*InIdentifier);
		}
		else
		{
			InJsonWriter.WriteObjectStart();
		}
	}

	template <typename CharType, typename ValueType>
	void WriteJSONValueWithOptionalIdentifier(typename TDataTableExporterJSON<CharType>::FDataTableJsonWriter& InJsonWriter, const FString* InIdentifier, const ValueType InValue)
	{
		if (InIdentifier)
		{
			InJsonWriter.WriteValue(*InIdentifier, InValue);
		}
		else
		{
			InJsonWriter.WriteValue(InValue);
		}
	}

#endif	// WITH_EDITOR

}

FString DataTableJSONUtils::GetKeyFieldName(const UDataTable& InDataTable)
{
	FString ExplicitString = InDataTable.ImportKeyField;
	if (ExplicitString.IsEmpty())
	{
		return TEXT("Name");
	}
	else
	{
		return ExplicitString;
	}
}


#if WITH_EDITOR

FDataTableExporterJSON::FDataTableExporterJSON(const EDataTableExportFlags InDTExportFlags, FString& OutExportText)
	: TDataTableExporterJSON<TCHAR>(InDTExportFlags, TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&OutExportText))
{
	bJsonWriterNeedsClose = true;
}

template<typename CharType>
TDataTableExporterJSON<CharType>::TDataTableExporterJSON(const EDataTableExportFlags InDTExportFlags, TSharedRef<FDataTableJsonWriter> InJsonWriter)
	: DTExportFlags(InDTExportFlags)
	, JsonWriter(InJsonWriter)
	, bJsonWriterNeedsClose(false)
{
}

template<typename CharType>
TDataTableExporterJSON<CharType>::~TDataTableExporterJSON()
{
	if (bJsonWriterNeedsClose)
	{
		JsonWriter->Close();
	}
}

template<typename CharType>
bool TDataTableExporterJSON<CharType>::WriteTable(const UDataTable& InDataTable)
{
	if (!InDataTable.RowStruct)
	{
		return false;
	}

	FString KeyField = DataTableJSONUtils::GetKeyFieldName(InDataTable);
	JsonWriter->WriteArrayStart();

	// Iterate over rows
	for (auto RowIt = InDataTable.GetRowMap().CreateConstIterator(); RowIt; ++RowIt)
	{
		JsonWriter->WriteObjectStart();
		{
			// RowName
			const FName RowName = RowIt.Key();
			JsonWriter->WriteValue(KeyField, RowName.ToString());

			// Now the values
			uint8* RowData = RowIt.Value();
			WriteRow(InDataTable.RowStruct, RowData, &KeyField);
		}
		JsonWriter->WriteObjectEnd();
	}

	JsonWriter->WriteArrayEnd();

	return true;
}

template<typename CharType>
bool TDataTableExporterJSON<CharType>::WriteTableAsObject(const UDataTable& InDataTable)
{
	if (!InDataTable.RowStruct)
	{
		return false;
	}

	JsonWriter->WriteObjectStart(InDataTable.GetName());

	// Iterate over rows
	for (auto RowIt = InDataTable.GetRowMap().CreateConstIterator(); RowIt; ++RowIt)
	{
		// RowName
		const FName RowName = RowIt.Key();
		JsonWriter->WriteObjectStart(RowName.ToString());
		{
			// Now the values
			uint8* RowData = RowIt.Value();
			WriteRow(InDataTable.RowStruct, RowData);
		}
		JsonWriter->WriteObjectEnd();
	}
	JsonWriter->WriteObjectEnd();

	return true;
}

template<typename CharType>
bool TDataTableExporterJSON<CharType>::WriteRow(const UScriptStruct* InRowStruct, const void* InRowData, const FString* FieldToSkip)
{
	if (!InRowStruct)
	{
		return false;
	}

	return WriteStruct(InRowStruct, InRowData, FieldToSkip);
}

template<typename CharType>
bool TDataTableExporterJSON<CharType>::WriteStruct(const UScriptStruct* InStruct, const void* InStructData, const FString* FieldToSkip)
{
	for (TFieldIterator<const FProperty> It(InStruct); It; ++It)
	{
		const FProperty* BaseProp = *It;
		check(BaseProp);

		const FString Identifier = DataTableUtils::GetPropertyExportName(BaseProp, DTExportFlags);
		if (FieldToSkip && *FieldToSkip == Identifier)
		{
			// Skip this field
			continue;
		}
 
		if (BaseProp->ArrayDim == 1)
		{
			const void* Data = BaseProp->ContainerPtrToValuePtr<void>(InStructData, 0);
			WriteStructEntry(InStructData, BaseProp, Data);
		}
		else
		{
			JsonWriter->WriteArrayStart(Identifier);

			for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < BaseProp->ArrayDim; ++ArrayEntryIndex)
			{
				const void* Data = BaseProp->ContainerPtrToValuePtr<void>(InStructData, ArrayEntryIndex);
				WriteContainerEntry(BaseProp, Data);
			}

			JsonWriter->WriteArrayEnd();
		}
	}

	return true;
}

template<typename CharType>
bool TDataTableExporterJSON<CharType>::WriteStructEntry(const void* InRowData, const FProperty* InProperty, const void* InPropertyData)
{
	const FString Identifier = DataTableUtils::GetPropertyExportName(InProperty, DTExportFlags);

	if (const FEnumProperty* EnumProp = CastField<const FEnumProperty>(InProperty))
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(EnumProp, (uint8*)InRowData, DTExportFlags);
		JsonWriter->WriteValue(Identifier, PropertyValue);
	}
	else if (const FNumericProperty *NumProp = CastField<const FNumericProperty>(InProperty))
	{
		if (NumProp->IsEnum())
		{
			const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(InProperty, (uint8*)InRowData, DTExportFlags);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
		else if (NumProp->IsInteger())
		{
			const int64 PropertyValue = NumProp->GetSignedIntPropertyValue(InPropertyData);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
		else if (NumProp->IsA(FFloatProperty::StaticClass()))
		{
			const float PropertyValue = (float)NumProp->GetFloatingPointPropertyValue(InPropertyData);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
		else
		{
			const double PropertyValue = NumProp->GetFloatingPointPropertyValue(InPropertyData);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
	}
	else if (const FBoolProperty* BoolProp = CastField<const FBoolProperty>(InProperty))
	{
		const bool PropertyValue = BoolProp->GetPropertyValue(InPropertyData);
		JsonWriter->WriteValue(Identifier, PropertyValue);
	}
	else if (const FArrayProperty* ArrayProp = CastField<const FArrayProperty>(InProperty))
	{
		JsonWriter->WriteArrayStart(Identifier);

		FScriptArrayHelper ArrayHelper(ArrayProp, InPropertyData);
		for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < ArrayHelper.Num(); ++ArrayEntryIndex)
		{
			const uint8* ArrayEntryData = ArrayHelper.GetRawPtr(ArrayEntryIndex);
			WriteContainerEntry(ArrayProp->Inner, ArrayEntryData);
		}

		JsonWriter->WriteArrayEnd();
	}
	else if (const FSetProperty* SetProp = CastField<const FSetProperty>(InProperty))
	{
		JsonWriter->WriteArrayStart(Identifier);

		FScriptSetHelper SetHelper(SetProp, InPropertyData);
		for (FScriptSetHelper::FIterator It(SetHelper); It; ++It)
		{
			const uint8* SetEntryData = SetHelper.GetElementPtr(It);
			WriteContainerEntry(SetHelper.GetElementProperty(), SetEntryData);
		}

		JsonWriter->WriteArrayEnd();
	}
	else if (const FMapProperty* MapProp = CastField<const FMapProperty>(InProperty))
	{
		JsonWriter->WriteObjectStart(Identifier);

		FScriptMapHelper MapHelper(MapProp, InPropertyData);
		for (FScriptMapHelper::FIterator It(MapHelper); It; ++It)
		{
			const uint8* MapKeyData = MapHelper.GetKeyPtr(It);
			const uint8* MapValueData = MapHelper.GetValuePtr(It);

			// JSON object keys must always be strings
			const FString KeyValue = DataTableUtils::GetPropertyValueAsStringDirect(MapHelper.GetKeyProperty(), (uint8*)MapKeyData, DTExportFlags);
			WriteContainerEntry(MapHelper.GetValueProperty(), MapValueData, &KeyValue);
		}

		JsonWriter->WriteObjectEnd();
	}
	else if (const FStructProperty* StructProp = CastField<const FStructProperty>(InProperty))
	{
		if (!!(DTExportFlags & EDataTableExportFlags::UseJsonObjectsForStructs))
		{
			JsonWriter->WriteObjectStart(Identifier);
			WriteStruct(StructProp->Struct, InPropertyData);
			JsonWriter->WriteObjectEnd();
		}
		else
		{
			const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(InProperty, (uint8*)InRowData, DTExportFlags);
			JsonWriter->WriteValue(Identifier, PropertyValue);
		}
	}
	else
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsString(InProperty, (uint8*)InRowData, DTExportFlags);
		JsonWriter->WriteValue(Identifier, PropertyValue);
	}

	return true;
}

template<typename CharType>
bool TDataTableExporterJSON<CharType>::WriteContainerEntry(const FProperty* InProperty, const void* InPropertyData, const FString* InIdentifier)
{
	if (const FEnumProperty* EnumProp = CastField<const FEnumProperty>(InProperty))
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsStringDirect(InProperty, (uint8*)InPropertyData, DTExportFlags);
		WriteJSONValueWithOptionalIdentifier<CharType>(*JsonWriter, InIdentifier, PropertyValue);
	}
	else if (const FNumericProperty *NumProp = CastField<const FNumericProperty>(InProperty))
	{
		if (NumProp->IsEnum())
		{
			const FString PropertyValue = DataTableUtils::GetPropertyValueAsStringDirect(InProperty, (uint8*)InPropertyData, DTExportFlags);
			WriteJSONValueWithOptionalIdentifier<CharType>(*JsonWriter, InIdentifier, PropertyValue);
		}
		else if (NumProp->IsInteger())
		{
			const int64 PropertyValue = NumProp->GetSignedIntPropertyValue(InPropertyData);
			WriteJSONValueWithOptionalIdentifier<CharType>(*JsonWriter, InIdentifier, PropertyValue);
		}
		else
		{
			const double PropertyValue = NumProp->GetFloatingPointPropertyValue(InPropertyData);
			WriteJSONValueWithOptionalIdentifier<CharType>(*JsonWriter, InIdentifier, PropertyValue);
		}
	}
	else if (const FBoolProperty* BoolProp = CastField<const FBoolProperty>(InProperty))
	{
		const bool PropertyValue = BoolProp->GetPropertyValue(InPropertyData);
		WriteJSONValueWithOptionalIdentifier<CharType>(*JsonWriter, InIdentifier, PropertyValue);
	}
	else if (const FStructProperty* StructProp = CastField<const FStructProperty>(InProperty))
	{
		if (!!(DTExportFlags & EDataTableExportFlags::UseJsonObjectsForStructs))
		{
			WriteJSONObjectStartWithOptionalIdentifier<CharType>(*JsonWriter, InIdentifier);
			WriteStruct(StructProp->Struct, InPropertyData);
			JsonWriter->WriteObjectEnd();
		}
		else
		{
			const FString PropertyValue = DataTableUtils::GetPropertyValueAsStringDirect(InProperty, (uint8*)InPropertyData, DTExportFlags);
			WriteJSONValueWithOptionalIdentifier<CharType>(*JsonWriter, InIdentifier, PropertyValue);
		}
	}
	else if (const FArrayProperty* ArrayProp = CastField<const FArrayProperty>(InProperty))
	{
		// Cannot nest arrays
		return false;
	}
	else if (const FSetProperty* SetProp = CastField<const FSetProperty>(InProperty))
	{
		// Cannot nest sets
		return false;
	}
	else if (const FMapProperty* MapProp = CastField<const FMapProperty>(InProperty))
	{
		// Cannot nest maps
		return false;
	}
	else
	{
		const FString PropertyValue = DataTableUtils::GetPropertyValueAsStringDirect(InProperty, (uint8*)InPropertyData, DTExportFlags);
		WriteJSONValueWithOptionalIdentifier<CharType>(*JsonWriter, InIdentifier, PropertyValue);
	}

	return true;
}

template class TDataTableExporterJSON<TCHAR>;
template class TDataTableExporterJSON<ANSICHAR>;

#endif // WITH_EDITOR


FDataTableImporterJSON::FDataTableImporterJSON(UDataTable& InDataTable, const FString& InJSONData, TArray<FString>& OutProblems)
	: DataTable(&InDataTable)
	, JSONData(InJSONData)
	, ImportProblems(OutProblems)
{
}

FDataTableImporterJSON::~FDataTableImporterJSON()
{
}

bool FDataTableImporterJSON::ReadTable()
{
	if (JSONData.IsEmpty())
	{
		ImportProblems.Add(TEXT("Input data is empty."));
		return false;
	}

	// Check we have a RowStruct specified
	if (!DataTable->RowStruct)
	{
		ImportProblems.Add(TEXT("No RowStruct specified."));
		return false;
	}

	TArray< TSharedPtr<FJsonValue> > ParsedTableRows;
	{
		const TSharedRef< TJsonReader<TCHAR> > JsonReader = TJsonReaderFactory<TCHAR>::Create(JSONData);
		if (!FJsonSerializer::Deserialize(JsonReader, ParsedTableRows) || ParsedTableRows.Num() == 0)
		{
			ImportProblems.Add(FString::Printf(TEXT("Failed to parse the JSON data. Error: %s"), *JsonReader->GetErrorMessage()));
			return false;
		}
	}

	// Empty existing data
	DataTable->EmptyTable();

	// Iterate over rows
	for (int32 RowIdx = 0; RowIdx < ParsedTableRows.Num(); ++RowIdx)
	{
		const TSharedPtr<FJsonValue>& ParsedTableRowValue = ParsedTableRows[RowIdx];
		TSharedPtr<FJsonObject> ParsedTableRowObject = ParsedTableRowValue->AsObject();
		if (!ParsedTableRowObject.IsValid())
		{
			ImportProblems.Add(FString::Printf(TEXT("Row '%d' is not a valid JSON object."), RowIdx));
			continue;
		}

		ReadRow(ParsedTableRowObject.ToSharedRef(), RowIdx);
	}

	DataTable->Modify(true);

	return true;
}

bool FDataTableImporterJSON::ReadRow(const TSharedRef<FJsonObject>& InParsedTableRowObject, const int32 InRowIdx)
{
	// Get row name
	FString RowKey = DataTableJSONUtils::GetKeyFieldName(*DataTable);
	FName RowName = DataTableUtils::MakeValidName(InParsedTableRowObject->GetStringField(RowKey));

	// Check its not 'none'
	if (RowName.IsNone())
	{
		ImportProblems.Add(FString::Printf(TEXT("Row '%d' missing key field '%s'."), InRowIdx, *RowKey));
		return false;
	}

	// Check its not a duplicate
	if (!DataTable->AllowDuplicateRowsOnImport() && DataTable->GetRowMap().Find(RowName) != nullptr)
	{
		ImportProblems.Add(FString::Printf(TEXT("Duplicate row name '%s'."), *RowName.ToString()));
		return false;
	}

	// Detect any extra fields within the data for this row
	if (!DataTable->bIgnoreExtraFields)
	{
		TArray<FString> TempPropertyImportNames;
		for (const TPair<FString, TSharedPtr<FJsonValue>>& ParsedPropertyKeyValuePair : InParsedTableRowObject->Values)
		{
			if (ParsedPropertyKeyValuePair.Key == RowKey)
			{
				// Skip the row name, as that doesn't match a property
				continue;
			}

			FName PropName = DataTableUtils::MakeValidName(ParsedPropertyKeyValuePair.Key);
			FProperty* ColumnProp = FindFProperty<FProperty>(DataTable->RowStruct, PropName);
			for (TFieldIterator<FProperty> It(DataTable->RowStruct); It && !ColumnProp; ++It)
			{
				DataTableUtils::GetPropertyImportNames(*It, TempPropertyImportNames);
				ColumnProp = TempPropertyImportNames.Contains(ParsedPropertyKeyValuePair.Key) ? *It : nullptr;
			}

			if (!ColumnProp)
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' cannot be found in struct '%s'."), *PropName.ToString(), *RowName.ToString(), *DataTable->RowStruct->GetName()));
			}
		}
	}

	// Allocate data to store information, using UScriptStruct to know its size
	uint8* RowData = (uint8*)FMemory::Malloc(DataTable->RowStruct->GetStructureSize());
	DataTable->RowStruct->InitializeStruct(RowData);
	// And be sure to call DestroyScriptStruct later

	// Add to row map
	DataTable->AddRowInternal(RowName, RowData);

	return ReadStruct(InParsedTableRowObject, DataTable->RowStruct, RowName, RowData);
}

bool FDataTableImporterJSON::ReadStruct(const TSharedRef<FJsonObject>& InParsedObject, UScriptStruct* InStruct, const FName InRowName, void* InStructData)
{
	// Now read in each property
	TArray<FString> TempPropertyImportNames;
	for (TFieldIterator<FProperty> It(InStruct); It; ++It)
	{
		FProperty* BaseProp = *It;
		check(BaseProp);

		const FString ColumnName = DataTableUtils::GetPropertyExportName(BaseProp);

		TSharedPtr<FJsonValue> ParsedPropertyValue;
		DataTableUtils::GetPropertyImportNames(BaseProp, TempPropertyImportNames);
		for (const FString& PropertyName : TempPropertyImportNames)
		{
			ParsedPropertyValue = InParsedObject->TryGetField(PropertyName);
			if (ParsedPropertyValue.IsValid())
			{
				break;
			}
		}

		if (!ParsedPropertyValue.IsValid())
		{
#if WITH_EDITOR
			// If the structure has specified the property as optional for import (gameplay code likely doing a custom fix-up or parse of that property),
			// then avoid warning about it
			static const FName DataTableImportOptionalMetadataKey(TEXT("DataTableImportOptional"));
			if (BaseProp->HasMetaData(DataTableImportOptionalMetadataKey))
			{
				continue;
			}
#endif // WITH_EDITOR

			if (!DataTable->bIgnoreMissingFields)
			{
				ImportProblems.Add(FString::Printf(TEXT("Row '%s' is missing an entry for '%s'."), *InRowName.ToString(), *ColumnName));
			}

			continue;
		}

		if (BaseProp->ArrayDim == 1)
		{
			void* Data = BaseProp->ContainerPtrToValuePtr<void>(InStructData, 0);
			ReadStructEntry(ParsedPropertyValue.ToSharedRef(), InRowName, ColumnName, InStructData, BaseProp, Data);
		}
		else
		{
			const TCHAR* const ParsedPropertyType = JSONTypeToString(ParsedPropertyValue->Type);

			const TArray< TSharedPtr<FJsonValue> >* PropertyValuesPtr;
			if (!ParsedPropertyValue->TryGetArray(PropertyValuesPtr))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Array, got %s."), *ColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			if (BaseProp->ArrayDim != PropertyValuesPtr->Num())
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is a static sized array with %d elements, but we have %d values to import"), *ColumnName, *InRowName.ToString(), BaseProp->ArrayDim, PropertyValuesPtr->Num()));
			}

			for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < BaseProp->ArrayDim; ++ArrayEntryIndex)
			{
				if (PropertyValuesPtr->IsValidIndex(ArrayEntryIndex))
				{
					void* Data = BaseProp->ContainerPtrToValuePtr<void>(InStructData, ArrayEntryIndex);
					const TSharedPtr<FJsonValue>& PropertyValueEntry = (*PropertyValuesPtr)[ArrayEntryIndex];
					ReadContainerEntry(PropertyValueEntry.ToSharedRef(), InRowName, ColumnName, ArrayEntryIndex, BaseProp, Data);
				}
			}
		}
	}

	return true;
}

bool FDataTableImporterJSON::ReadStructEntry(const TSharedRef<FJsonValue>& InParsedPropertyValue, const FName InRowName, const FString& InColumnName, const void* InRowData, FProperty* InProperty, void* InPropertyData)
{
	const TCHAR* const ParsedPropertyType = JSONTypeToString(InParsedPropertyValue->Type);

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(InProperty))
	{
		FString EnumValue;
		if (InParsedPropertyValue->TryGetString(EnumValue))
		{
			FString Error = DataTableUtils::AssignStringToProperty(EnumValue, InProperty, (uint8*)InRowData);
			if (!Error.IsEmpty())
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' has invalid enum value: %s."), *InColumnName, *InRowName.ToString(), *EnumValue));
				return false;
			}
		}
		else
		{
			int64 PropertyValue = 0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Integer, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(InPropertyData, PropertyValue);
		}
	}
	else if (FNumericProperty *NumProp = CastField<FNumericProperty>(InProperty))
	{
		FString EnumValue;
		if (NumProp->IsEnum() && InParsedPropertyValue->TryGetString(EnumValue))
		{
			FString Error = DataTableUtils::AssignStringToProperty(EnumValue, InProperty, (uint8*)InRowData);
			if (!Error.IsEmpty())
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' has invalid enum value: %s."), *InColumnName, *InRowName.ToString(), *EnumValue));
				return false;
			}
		}
		else if (NumProp->IsInteger())
		{
			int64 PropertyValue = 0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Integer, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetIntPropertyValue(InPropertyData, PropertyValue);
		}
		else
		{
			double PropertyValue = 0.0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Double, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetFloatingPointPropertyValue(InPropertyData, PropertyValue);
		}
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(InProperty))
	{
		bool PropertyValue = false;
		if (!InParsedPropertyValue->TryGetBool(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Boolean, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		BoolProp->SetPropertyValue(InPropertyData, PropertyValue);
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(InProperty))
	{
		const TArray< TSharedPtr<FJsonValue> >* PropertyValuesPtr;
		if (!InParsedPropertyValue->TryGetArray(PropertyValuesPtr))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Array, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		FScriptArrayHelper ArrayHelper(ArrayProp, InPropertyData);
		ArrayHelper.EmptyValues();
		for (const TSharedPtr<FJsonValue>& PropertyValueEntry : *PropertyValuesPtr)
		{
			const int32 NewEntryIndex = ArrayHelper.AddValue();
			uint8* ArrayEntryData = ArrayHelper.GetRawPtr(NewEntryIndex);
			ReadContainerEntry(PropertyValueEntry.ToSharedRef(), InRowName, InColumnName, NewEntryIndex, ArrayProp->Inner, ArrayEntryData);
		}
	}
	else if (FSetProperty* SetProp = CastField<FSetProperty>(InProperty))
	{
		const TArray< TSharedPtr<FJsonValue> >* PropertyValuesPtr;
		if (!InParsedPropertyValue->TryGetArray(PropertyValuesPtr))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Array, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		FScriptSetHelper SetHelper(SetProp, InPropertyData);
		SetHelper.EmptyElements();
		for (const TSharedPtr<FJsonValue>& PropertyValueEntry : *PropertyValuesPtr)
		{
			const int32 NewEntryIndex = SetHelper.AddDefaultValue_Invalid_NeedsRehash();
			uint8* SetEntryData = SetHelper.GetElementPtr(NewEntryIndex);
			ReadContainerEntry(PropertyValueEntry.ToSharedRef(), InRowName, InColumnName, NewEntryIndex, SetHelper.GetElementProperty(), SetEntryData);
		}
		SetHelper.Rehash();
	}
	else if (FMapProperty* MapProp = CastField<FMapProperty>(InProperty))
	{
		const TSharedPtr<FJsonObject>* PropertyValue;
		if (!InParsedPropertyValue->TryGetObject(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected Object, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		FScriptMapHelper MapHelper(MapProp, InPropertyData);
		MapHelper.EmptyValues();
		for (const auto& PropertyValuePair : (*PropertyValue)->Values)
		{
			const int32 NewEntryIndex = MapHelper.AddDefaultValue_Invalid_NeedsRehash();
			uint8* MapKeyData = MapHelper.GetKeyPtr(NewEntryIndex);
			uint8* MapValueData = MapHelper.GetValuePtr(NewEntryIndex);

			// JSON object keys are always strings
			const FString KeyError = DataTableUtils::AssignStringToPropertyDirect(PropertyValuePair.Key, MapHelper.GetKeyProperty(), MapKeyData);
			if (KeyError.Len() > 0)
			{
				MapHelper.RemoveAt(NewEntryIndex);
				ImportProblems.Add(FString::Printf(TEXT("Problem assigning key '%s' to property '%s' on row '%s' : %s"), *PropertyValuePair.Key, *InColumnName, *InRowName.ToString(), *KeyError));
				return false;
			}

			if (!ReadContainerEntry(PropertyValuePair.Value.ToSharedRef(), InRowName, InColumnName, NewEntryIndex, MapHelper.GetValueProperty(), MapValueData))
			{
				MapHelper.RemoveAt(NewEntryIndex);
				return false;
			}
		}
		MapHelper.Rehash();
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(InProperty))
	{
		const TSharedPtr<FJsonObject>* PropertyValue = nullptr;
		if (InParsedPropertyValue->TryGetObject(PropertyValue))
		{
			return ReadStruct(PropertyValue->ToSharedRef(), StructProp->Struct, InRowName, InPropertyData);
		}
		else
		{
			// If the JSON does not contain a JSON object for this struct, we try to use the backwards-compatible string deserialization, same as the "else" block below
			FString PropertyValueString;
			if (!InParsedPropertyValue->TryGetString(PropertyValueString))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected String, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			const FString Error = DataTableUtils::AssignStringToProperty(PropertyValueString, InProperty, (uint8*)InRowData);
			if (Error.Len() > 0)
			{
				ImportProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to property '%s' on row '%s' : %s"), *PropertyValueString, *InColumnName, *InRowName.ToString(), *Error));
				return false;
			}

			return true;
		}
	}
	else
	{
		FString PropertyValue;
		if (!InParsedPropertyValue->TryGetString(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected String, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		const FString Error = DataTableUtils::AssignStringToProperty(PropertyValue, InProperty, (uint8*)InRowData);
		if(Error.Len() > 0)
		{
			ImportProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to property '%s' on row '%s' : %s"), *PropertyValue, *InColumnName, *InRowName.ToString(), *Error));
			return false;
		}
	}

	return true;
}

bool FDataTableImporterJSON::ReadContainerEntry(const TSharedRef<FJsonValue>& InParsedPropertyValue, const FName InRowName, const FString& InColumnName, const int32 InArrayEntryIndex, FProperty* InProperty, void* InPropertyData)
{
	const TCHAR* const ParsedPropertyType = JSONTypeToString(InParsedPropertyValue->Type);

	if (FEnumProperty* EnumProp = CastField<FEnumProperty>(InProperty))
	{
		FString EnumValue;
		if (InParsedPropertyValue->TryGetString(EnumValue))
		{
			FString Error = DataTableUtils::AssignStringToPropertyDirect(EnumValue, InProperty, (uint8*)InPropertyData);
			if (!Error.IsEmpty())
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' has invalid enum value: %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), *EnumValue));
				return false;
			}
		}
		else
		{
			int64 PropertyValue = 0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected Integer, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			EnumProp->GetUnderlyingProperty()->SetIntPropertyValue(InPropertyData, PropertyValue);
		}
	}
	else if (FNumericProperty *NumProp = CastField<FNumericProperty>(InProperty))
	{
		FString EnumValue;
		if (NumProp->IsEnum() && InParsedPropertyValue->TryGetString(EnumValue))
		{
			FString Error = DataTableUtils::AssignStringToPropertyDirect(EnumValue, InProperty, (uint8*)InPropertyData);
			if (!Error.IsEmpty())
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' has invalid enum value: %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), *EnumValue));
				return false;
			}
		}
		else if(NumProp->IsInteger())
		{
			int64 PropertyValue = 0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected Integer, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetIntPropertyValue(InPropertyData, PropertyValue);
		}
		else
		{
			double PropertyValue = 0.0;
			if (!InParsedPropertyValue->TryGetNumber(PropertyValue))
			{
				ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected Double, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			NumProp->SetFloatingPointPropertyValue(InPropertyData, PropertyValue);
		}
	}
	else if (FBoolProperty* BoolProp = CastField<FBoolProperty>(InProperty))
	{
		bool PropertyValue = false;
		if (!InParsedPropertyValue->TryGetBool(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected Boolean, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		BoolProp->SetPropertyValue(InPropertyData, PropertyValue);
	}
	else if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(InProperty))
	{
		// Cannot nest arrays
		return false;
	}
	else if (FSetProperty* SetProp = CastField<FSetProperty>(InProperty))
	{
		// Cannot nest sets
		return false;
	}
	else if (FMapProperty* MapProp = CastField<FMapProperty>(InProperty))
	{
		// Cannot nest maps
		return false;
	}
	else if (FStructProperty* StructProp = CastField<FStructProperty>(InProperty))
	{
		const TSharedPtr<FJsonObject>* PropertyValue = nullptr;
		if (InParsedPropertyValue->TryGetObject(PropertyValue))
		{
			return ReadStruct(PropertyValue->ToSharedRef(), StructProp->Struct, InRowName, InPropertyData);
		}
		else
		{
			// If the JSON does not contain a JSON object for this struct, we try to use the backwards-compatible string deserialization, same as the "else" block below
			FString PropertyValueString;
			if (!InParsedPropertyValue->TryGetString(PropertyValueString))
			{
				ImportProblems.Add(FString::Printf(TEXT("Property '%s' on row '%s' is the incorrect type. Expected String, got %s."), *InColumnName, *InRowName.ToString(), ParsedPropertyType));
				return false;
			}

			const FString Error = DataTableUtils::AssignStringToPropertyDirect(PropertyValueString, InProperty, (uint8*)InPropertyData);
			if (Error.Len() > 0)
			{
				ImportProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to entry %d on property '%s' on row '%s' : %s"), InArrayEntryIndex, *PropertyValueString, *InColumnName, *InRowName.ToString(), *Error));
				return false;
			}

			return true;
		}
	}
	else
	{
		FString PropertyValue;
		if (!InParsedPropertyValue->TryGetString(PropertyValue))
		{
			ImportProblems.Add(FString::Printf(TEXT("Entry %d on property '%s' on row '%s' is the incorrect type. Expected String, got %s."), InArrayEntryIndex, *InColumnName, *InRowName.ToString(), ParsedPropertyType));
			return false;
		}

		const FString Error = DataTableUtils::AssignStringToPropertyDirect(PropertyValue, InProperty, (uint8*)InPropertyData);
		if(Error.Len() > 0)
		{
			ImportProblems.Add(FString::Printf(TEXT("Problem assigning string '%s' to entry %d on property '%s' on row '%s' : %s"), InArrayEntryIndex, *PropertyValue, *InColumnName, *InRowName.ToString(), *Error));
			return false;
		}
	}

	return true;
}

