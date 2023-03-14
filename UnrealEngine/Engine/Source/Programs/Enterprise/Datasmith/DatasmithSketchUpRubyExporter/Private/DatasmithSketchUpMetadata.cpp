// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithSketchUpMetadata.h"

#include "DatasmithSketchUpString.h"
#include "DatasmithSketchUpSummary.h"

// SketchUp SDK.
#include "DatasmithSketchUpSDKBegins.h"
#include "SketchUpAPI/model/attribute_dictionary.h"
#include "SketchUpAPI/model/classification_attribute.h"
#include "SketchUpAPI/model/classification_info.h"
#include "SketchUpAPI/model/component_definition.h"
#include "SketchUpAPI/model/component_instance.h"
#include "SketchUpAPI/model/entity.h"
#include "SketchUpAPI/model/model.h"
#include "DatasmithSketchUpSDKCeases.h"

// Datasmith SDK.
#include "Containers/Array.h"
#include "DatasmithSceneFactory.h"

using namespace DatasmithSketchUp;


TSet<FString> const FMetadata::InterestingAttributeDictionarySet =
{
	FString(TEXT("SU_DefinitionSet")),
	FString(TEXT("SU_InstanceSet"))
};

FMetadata::FMetadata(
	SUModelRef InSModelRef
) :
	SketchupSourceID(MODEL_METADATA_ID)
{
	// Get the number of attribute dictionaries in the SketchUp model.
	size_t SAttributeDictionaryCount = 0;
	SUModelGetNumAttributeDictionaries(InSModelRef, &SAttributeDictionaryCount); // we can ignore the returned SU_RESULT

	if (SAttributeDictionaryCount > 0)
	{
		// Retrieve the attribute dictionaries in the SketchUp model.
		TArray<SUAttributeDictionaryRef> SAttributeDictionaries;
		SAttributeDictionaries.Init(SU_INVALID, SAttributeDictionaryCount);
		SUModelGetAttributeDictionaries(InSModelRef, SAttributeDictionaryCount, SAttributeDictionaries.GetData(), &SAttributeDictionaryCount); // we can ignore the returned SU_RESULT
		SAttributeDictionaries.SetNum(SAttributeDictionaryCount);

		// Retrieve the key-value pairs of the SketchUp attribute dictionaries.
		for (SUAttributeDictionaryRef SAttributeDictionaryRef : SAttributeDictionaries)
		{
			ScanAttributeDictionary(SAttributeDictionaryRef);
		}
	}
}

FMetadata::FMetadata(
	SUEntityRef InSEntityRef
)
{
	// Get the SketckUp metadata ID.
	SUEntityGetID(InSEntityRef, &SketchupSourceID); // we can ignore the returned SU_RESULT

	// Get the number of attribute dictionaries in the SketchUp entity.
	size_t SAttributeDictionaryCount = 0;
	SUEntityGetNumAttributeDictionaries(InSEntityRef, &SAttributeDictionaryCount); // we can ignore the returned SU_RESULT

	if (SAttributeDictionaryCount > 0)
	{
		// Retrieve the attribute dictionaries in the SketchUp entity.
		TArray<SUAttributeDictionaryRef> SAttributeDictionaries;
		SAttributeDictionaries.Init(SU_INVALID, SAttributeDictionaryCount);
		SUEntityGetAttributeDictionaries(InSEntityRef, SAttributeDictionaryCount, SAttributeDictionaries.GetData(), &SAttributeDictionaryCount); // we can ignore the returned SU_RESULT
		SAttributeDictionaries.SetNum(SAttributeDictionaryCount);

		// Retrieve the key-value pairs of the SketchUp attribute dictionaries.
		for (SUAttributeDictionaryRef SAttributeDictionaryRef : SAttributeDictionaries)
		{
			ScanAttributeDictionary(SAttributeDictionaryRef);
		}
	}

	if (SUEntityGetType(InSEntityRef) == SURefType::SURefType_ComponentInstance)
	{
		// Retrieve the classification info from the SketckUp component instance.
		SUClassificationInfoRef SClassificationInfoRef = SU_INVALID;
		SUResult SResult = SUComponentInstanceCreateClassificationInfo(SUComponentInstanceFromEntity(InSEntityRef), &SClassificationInfoRef);
		// Make sure the SketckUp component instance is classified (no SU_ERROR_NO_DATA).
		if (SResult == SU_ERROR_NONE)
		{
			// Get the number of schemas in the SketchUp classification info.
			size_t SSchemaCount = 0;
			SUClassificationInfoGetNumSchemas(SClassificationInfoRef, &SSchemaCount);

			TArray<FString> ClassificationSchemaTypes;
			ClassificationSchemaTypes.Reserve(SSchemaCount);

			for (int32 SSchemaNo = 0; SSchemaNo < SSchemaCount; SSchemaNo++)
			{
				SUStringRef ShemaTypeStringRef = SU_INVALID;
				SUStringCreate(&ShemaTypeStringRef);
				SUClassificationInfoGetSchemaType(SClassificationInfoRef, SSchemaNo, &ShemaTypeStringRef);
				FString SchemaType = SuConvertString(ShemaTypeStringRef);
				SUStringRelease(&ShemaTypeStringRef);
				ClassificationSchemaTypes.Add(SchemaType);


				// Retrieve the SketchUp classification schema attribute.
				SUClassificationAttributeRef SSchemaAttributeRef = SU_INVALID;
				SUClassificationInfoGetSchemaAttribute(SClassificationInfoRef, SSchemaNo, &SSchemaAttributeRef); // we can ignore the returned SU_RESULT

				// Retrieve the key-value pairs of the SketchUp classification schema.
				ScanClassificationSchema(SSchemaAttributeRef);
			}

			if (SSchemaCount > 0)
			{
				MetadataKeyValueMap.Add(TEXT("ClassificationSchemaTypes"), FString::Join(ClassificationSchemaTypes, TEXT(",")));
			}

			// Release the SketchUp component classification info.
			SUClassificationInfoRelease(&SClassificationInfoRef); // we can ignore the returned SU_RESULT
		}
	}
}

void FMetadata::ScanAttributeDictionary(
	SUAttributeDictionaryRef InSAttributeDictionaryRef
)
{
	// Retrieve the SketchUp attribute dictionary name.
	FString SAttributeDictionaryName;
	SAttributeDictionaryName = SuGetString(SUAttributeDictionaryGetName, InSAttributeDictionaryRef);

	// Skip uninteresting SketchUp attribute dictionaries.
	if (SAttributeDictionaryName.IsEmpty() || !InterestingAttributeDictionarySet.Contains(SAttributeDictionaryName))
	{
		return;
	}

	// Get the number of keys in the SketchUp attribute dictionary.
	size_t SAttributeKeyCount = 0;
	SUAttributeDictionaryGetNumKeys(InSAttributeDictionaryRef, &SAttributeKeyCount); // we can ignore the returned SU_RESULT

	if (SAttributeKeyCount > 0)
	{
		// Retrieve the keys in the SketchUp attribute dictionary.
		TArray<SUStringRef> SAttributeKeys;
		SAttributeKeys.Init(SU_INVALID, SAttributeKeyCount);
		for (int32 SAttributeKeNo = 0; SAttributeKeNo < SAttributeKeyCount; SAttributeKeNo++)
		{
			SUStringCreate(&SAttributeKeys[SAttributeKeNo]); // we can ignore the returned SU_RESULT
		}
		SUAttributeDictionaryGetKeys(InSAttributeDictionaryRef, SAttributeKeyCount, SAttributeKeys.GetData(), &SAttributeKeyCount); // we can ignore the returned SU_RESULT
		SAttributeKeys.SetNum(SAttributeKeyCount);

		for (SUStringRef SAttributeKeyRef : SAttributeKeys)
		{
			FString SAttributeKey = SuConvertString(SAttributeKeyRef);

			// Retrieve the value associated with the key from the SketchUp attribute dictionary.
			SUTypedValueRef STypedValueRef = SU_INVALID;
			SUTypedValueCreate(&STypedValueRef); // we can ignore the returned SU_RESULT
			SUResult SResult = SUAttributeDictionaryGetValue(InSAttributeDictionaryRef, TCHAR_TO_UTF8(*SAttributeKey), &STypedValueRef);
			// Make sure there is a value associated with the given key (no SU_ERROR_NO_DATA).
			if (SResult == SU_ERROR_NONE)
			{
				FString SAttributeValue = GetAttributeValue(STypedValueRef);

				if (!SAttributeValue.IsEmpty())
				{					
					// Add the SketchUp attribute key-value pair to our metadata dictionary.
					MetadataKeyValueMap.Add(FString::Printf(TEXT("%ls:%ls"), *SAttributeDictionaryName, *SAttributeKey), SAttributeValue);
				}
			}

			// Release the SketchUp attribute key and value.
			SUStringRelease(&SAttributeKeyRef); // we can ignore the returned SU_RESULT
			SUTypedValueRelease(&STypedValueRef); // we can ignore the returned SU_RESULT
		}
	}
}

void FMetadata::ScanClassificationSchema(
	SUClassificationAttributeRef InSSchemaAttributeRef
)
{
	// Retrieve the SketchUp schema attribute path (the attribute name as it should be displayed to the user).
	FString SAttributePath = SuGetString(SUClassificationAttributeGetPath, InSSchemaAttributeRef);

	// Retrieve the SketchUp schema attribute value.
	SUTypedValueRef STypedValueRef = SU_INVALID;
	SUTypedValueCreate(&STypedValueRef); // we can ignore the returned SU_RESULT
	SUClassificationAttributeGetValue(InSSchemaAttributeRef, &STypedValueRef); // we can ignore the returned SU_RESULT

	FString SAttributeValue = GetAttributeValue(STypedValueRef);

	if (!SAttributeValue.IsEmpty())
	{
		// Add the SketchUp attribute key-value pair to our metadata dictionary.
		MetadataKeyValueMap.Add(SAttributePath, SAttributeValue);
	}

	SUTypedValueRelease(&STypedValueRef); // we can ignore the returned SU_RESULT

	// Get the number of sub-attributes in the SketchUp schema attribute.
	size_t SSubAttributeCount = 0;
	SUClassificationAttributeGetNumChildren(InSSchemaAttributeRef, &SSubAttributeCount); // we can ignore the returned SU_RESULT

	for (int32 SSubAttributeNo = 0; SSubAttributeNo < SSubAttributeCount; SSubAttributeNo++)
	{
		// Retrieve the sub-attribute in the SketchUp schema attribute.
		SUClassificationAttributeRef SSubAttributeRef = SU_INVALID;
		SUClassificationAttributeGetChild(InSSchemaAttributeRef, SSubAttributeNo, &SSubAttributeRef); // we can ignore the returned SU_RESULT

		// Retrieve the key-value pairs of the SketchUp schema sub-attribute.
		ScanClassificationSchema(SSubAttributeRef);
	}
}

FString FMetadata::GetAttributeValue(
	SUTypedValueRef InSTypedValueRef
)
{
	FString SAttributeValue;

	// Get the type of the SketchUp attribute value.
	SUTypedValueType SAttributeType;
	SUTypedValueGetType(InSTypedValueRef, &SAttributeType); // we can ignore the returned SU_RESULT

	// Convert the SketchUp attribute value into a string representation.
	switch (SAttributeType)
	{
		case SUTypedValueType::SUTypedValueType_Empty:
		{
			break;
		}
		case SUTypedValueType::SUTypedValueType_Byte:
		{
			char ByteValue = 0;
			SUTypedValueGetByte(InSTypedValueRef, &ByteValue); // we can ignore the returned SU_RESULT
			SAttributeValue = FString::Printf(TEXT("%hhd"), ByteValue);
			break;
		}
		case SUTypedValueType::SUTypedValueType_Short:
		{
			int16 Int16Value = 0;
			SUTypedValueGetInt16(InSTypedValueRef, &Int16Value); // we can ignore the returned SU_RESULT
			SAttributeValue = FString::Printf(TEXT("%hd"), Int16Value);
			break;
		}
		case SUTypedValueType::SUTypedValueType_Int32:
		{
			int32 Int32Value = 0;
			SUTypedValueGetInt32(InSTypedValueRef, &Int32Value); // we can ignore the returned SU_RESULT
			SAttributeValue = FString::Printf(TEXT("%d"), Int32Value);
			break;
		}
		case SUTypedValueType::SUTypedValueType_Float:
		{
			float FloatValue = 0.0;
			SUTypedValueGetFloat(InSTypedValueRef, &FloatValue); // we can ignore the returned SU_RESULT
			SAttributeValue = FString::Printf(TEXT("%f"), FloatValue);
			break;
		}
		case SUTypedValueType::SUTypedValueType_Double:
		{
			double DoubleValue = 0.0;
			SUTypedValueGetDouble(InSTypedValueRef, &DoubleValue); // we can ignore the returned SU_RESULT
			SAttributeValue = FString::Printf(TEXT("%lf"), DoubleValue);
			break;
		}
		case SUTypedValueType::SUTypedValueType_Bool:
		{
			bool BoolValue = false;
			SUTypedValueGetBool(InSTypedValueRef, &BoolValue); // we can ignore the returned SU_RESULT
			SAttributeValue = BoolValue ? TEXT("true") : TEXT("false");
			break;
		}
		case SUTypedValueType::SUTypedValueType_Color:
		{
			SUColor ColorValue = { 0, 0, 0, 0 };
			SUTypedValueGetColor(InSTypedValueRef, &ColorValue); // we can ignore the returned SU_RESULT
			SAttributeValue = FString::Printf(TEXT("(%hhu, %hhu, %hhu, %hhu)"), ColorValue.red, ColorValue.green, ColorValue.blue, ColorValue.alpha);
			break;
		}
		case SUTypedValueType::SUTypedValueType_Time:
		{
			int64 TimeValue = 0; // seconds since 1970-01-01
			SUTypedValueGetTime(InSTypedValueRef, &TimeValue); // we can ignore the returned SU_RESULT
			SAttributeValue = FString::Printf(TEXT("%lld"), TimeValue);
			break;
		}
		case SUTypedValueType::SUTypedValueType_String:
		{
			SAttributeValue = SuGetString(SUTypedValueGetString, InSTypedValueRef);
			break;
		}
		case SUTypedValueType::SUTypedValueType_Vector3D:
		{
			double Vector3DValue[3] = { 0.0, 0.0, 0.0 };
			SUTypedValueGetVector3d(InSTypedValueRef, Vector3DValue); // we can ignore the returned SU_RESULT
			SAttributeValue = FString::Printf(TEXT("(%lf, %lf, %lf)"), Vector3DValue[0], Vector3DValue[1], Vector3DValue[2]);
			break;
		}
		case SUTypedValueType::SUTypedValueType_Array:
		{
			// Get the number of sub-values in the SketchUp attribute value.
			size_t SSubValueCount = 0;
			SUTypedValueGetNumArrayItems(InSTypedValueRef, &SSubValueCount); // we can ignore the returned SU_RESULT

			if (SSubValueCount > 0)
			{
				// Retrieve the sub-values in the SketchUp attribute value.
				TArray<SUTypedValueRef> SSubValues;
				SSubValues.Init(SU_INVALID, SSubValueCount);
				SUTypedValueGetArrayItems(InSTypedValueRef, SSubValueCount, SSubValues.GetData(), &SSubValueCount); // we can ignore the returned SU_RESULT
				SSubValues.SetNum(SSubValueCount);

				// Combine the SketchUp attribute sub-values into one string representation.
				FString SCombinedSubValues;
				for (SUTypedValueRef SSubValueRef : SSubValues)
				{
					FString SSubAttributeValue = GetAttributeValue(SSubValueRef);

					if (!SSubAttributeValue.IsEmpty())
					{
						SCombinedSubValues.Append(SSubAttributeValue);
						SCombinedSubValues.Append(TEXT(", "));
					}
				}

				if (!SCombinedSubValues.IsEmpty())
				{
					SAttributeValue.AppendChar(TEXT('('));
					SAttributeValue.Append(SCombinedSubValues.LeftChop(2)); // remove the last ", "
					SAttributeValue.AppendChar(TEXT(')'));
				}
			}

			break;
		}
	}

	// Removes whitespaces from the start and end of the SketchUp attribute value string representation.
	SAttributeValue.TrimStartAndEndInline();

	return SAttributeValue;
}

void FMetadata::AddMetadata(
	TSharedPtr<IDatasmithMetaDataElement> IODMetaDataElementPtr
) const
{
	// Add the metadata key-value pairs into the Datasmith metadata element.
	for (auto const& MetadataKeyValueEntry : MetadataKeyValueMap)
	{
		// Create a Datasmith key-value property.
		TSharedPtr<IDatasmithKeyValueProperty> DKeyValuePtr = FDatasmithSceneFactory::CreateKeyValueProperty(*MetadataKeyValueEntry.Key);
		DKeyValuePtr->SetValue(*MetadataKeyValueEntry.Value);

		// Add the key-value property to the Datasmith metadata element.
		IODMetaDataElementPtr->AddProperty(DKeyValuePtr);

		// ADD_TRACE_LINE(TEXT("Actor %ls metadata: %ls = %ls"), IODMetaDataElementPtr->GetName(), *MetadataKeyValueEntry.Key, *MetadataKeyValueEntry.Value);
	}
}
