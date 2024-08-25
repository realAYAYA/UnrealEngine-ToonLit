// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonDataBag.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonSerializable.h"
#include "Serialization/JsonSerializerBase.h"
#include "Serialization/JsonSerializerReader.h"
#include "Serialization/JsonSerializerWriter.h"
#include "JsonGlobals.h"

/**
 * Macros used to generate a serialization function for a class derived from FJsonSerializable
 */
#define BEGIN_JSON_SERIALIZER \
	virtual void Serialize(FJsonSerializerBase& Serializer, bool bFlatObject) override \
	{ \
		if (!bFlatObject) { Serializer.StartObject(); }

#define END_JSON_SERIALIZER \
		if (!bFlatObject) { Serializer.EndObject(); } \
	}

#define JSON_SERIALIZE(JsonName, JsonValue) \
		Serializer.Serialize(TEXTVIEW(JsonName), JsonValue)

#define JSON_SERIALIZE_WITHDEFAULT(JsonName, JsonValue, DefaultJsonValue) \
		if (Serializer.IsLoading()) \
		{ \
			if (!Serializer.GetObject()->HasField(TEXTVIEW(JsonName))) \
           	{ \
           		JsonValue = DefaultJsonValue; \
           	} \
        } \
		Serializer.Serialize(TEXTVIEW(JsonName), JsonValue);
		
#define JSON_SERIALIZE_OPTIONAL(JsonName, OptionalJsonValue) \
		if (Serializer.IsLoading()) \
		{ \
			if (Serializer.GetObject()->HasField(TEXTVIEW(JsonName))) \
			{ \
				Serializer.Serialize(TEXTVIEW(JsonName), OptionalJsonValue.Emplace()); \
			} \
		} \
		else \
		{ \
			if (OptionalJsonValue.IsSet()) \
			{ \
				Serializer.Serialize(TEXTVIEW(JsonName), OptionalJsonValue.GetValue()); \
			} \
		}

#define JSON_SERIALIZE_ARRAY(JsonName, JsonArray) \
		Serializer.SerializeArray(TEXTVIEW(JsonName), JsonArray)
		
#define JSON_SERIALIZE_ARRAY_WITHDEFAULT(JsonName, JsonArray, DefaultArray) \
		if (Serializer.IsLoading()) \
		{ \
			if (!Serializer.GetObject()->HasField(TEXTVIEW(JsonName))) \
			{ \
				JsonArray = DefaultArray; \
			} \
		} \
		Serializer.SerializeArray(TEXTVIEW(JsonName), JsonArray);
		
#define JSON_SERIALIZE_OBJECT_WITHDEFAULT(JsonName, JsonObject, DefaultObject) \
		if (Serializer.IsLoading()) \
		{ \
			if (!Serializer.GetObject()->HasField(TEXTVIEW(JsonName))) \
			{ \
				JsonObject = DefaultObject; \
			} \
		} \
		Serializer.SerializeArray(TEXTVIEW(JsonName), JsonArray);

#define JSON_SERIALIZE_MAP(JsonName, JsonMap) \
		Serializer.SerializeMap(TEXTVIEW(JsonName), JsonMap)

#define JSON_SERIALIZE_SIMPLECOPY(JsonMap) \
		Serializer.SerializeSimpleMap(JsonMap)

#define JSON_SERIALIZE_MAP_SAFE(JsonName, JsonMap) \
		Serializer.SerializeMapSafe(TEXTVIEW(JsonName), JsonMap)

#define JSON_SERIALIZE_SERIALIZABLE(JsonName, JsonValue) \
		JsonValue.Serialize(Serializer, false)

#define JSON_SERIALIZE_RAW_JSON_STRING(JsonName, JsonValue) \
		if (Serializer.IsLoading()) \
		{ \
			if (Serializer.GetObject()->HasTypedField<EJson::Object>(TEXTVIEW(JsonName))) \
			{ \
				TSharedPtr<FJsonObject> JsonObject = Serializer.GetObject()->GetObjectField(TEXTVIEW(JsonName)); \
				if (JsonObject.IsValid()) \
				{ \
					auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JsonValue); \
					FJsonSerializer::Serialize(JsonObject.ToSharedRef(), Writer); \
				} \
			} \
			else \
			{ \
				JsonValue = FString(); \
			} \
		} \
		else \
		{ \
			if (!JsonValue.IsEmpty()) \
			{ \
				Serializer.WriteIdentifierPrefix(TEXTVIEW(JsonName)); \
				Serializer.WriteRawJSONValue(*JsonValue); \
			} \
		}

#define JSON_SERIALIZE_ARRAY_SERIALIZABLE(JsonName, JsonArray, ElementType) \
		if (Serializer.IsLoading()) \
		{ \
			if (Serializer.GetObject()->HasTypedField<EJson::Array>(TEXTVIEW(JsonName))) \
			{ \
				for (auto It = Serializer.GetObject()->GetArrayField(TEXTVIEW(JsonName)).CreateConstIterator(); It; ++It) \
				{ \
					ElementType& Obj = JsonArray.AddDefaulted_GetRef(); \
					Obj.FromJson((*It)->AsObject()); \
				} \
			} \
		} \
		else \
		{ \
			Serializer.StartArray(TEXTVIEW(JsonName)); \
			for (auto It = JsonArray.CreateIterator(); It; ++It) \
			{ \
				It->Serialize(Serializer, false); \
			} \
			Serializer.EndArray(); \
		}
		
#define JSON_SERIALIZE_ARRAY_SERIALIZABLE_WITHDEFAULT(JsonName, JsonArray, ElementType, DefaultArray) \
		if (Serializer.IsLoading()) \
		{ \
			if (Serializer.GetObject()->HasTypedField<EJson::Array>(TEXTVIEW(JsonName))) \
			{ \
				for (auto It = Serializer.GetObject()->GetArrayField(TEXTVIEW(JsonName)).CreateConstIterator(); It; ++It) \
				{ \
					ElementType& Obj = JsonArray.AddDefaulted_GetRef(); \
					Obj.FromJson((*It)->AsObject()); \
				} \
			} \
			else \
			{ \
				JsonArray = DefaultArray; \
			} \
		} \
		else \
		{ \
			Serializer.StartArray(TEXTVIEW(JsonName)); \
			for (auto It = JsonArray.CreateIterator(); It; ++It) \
			{ \
				It->Serialize(Serializer, false); \
			} \
			Serializer.EndArray(); \
		}

#define JSON_SERIALIZE_OPTIONAL_ARRAY_SERIALIZABLE(JsonName, OptionalJsonArray, ElementType) \
		if (Serializer.IsLoading()) \
		{ \
			if (Serializer.GetObject()->HasTypedField<EJson::Array>(TEXTVIEW(JsonName))) \
			{ \
				TArray<ElementType>& JsonArray = OptionalJsonArray.Emplace(); \
				for (auto It = Serializer.GetObject()->GetArrayField(TEXTVIEW(JsonName)).CreateConstIterator(); It; ++It) \
				{ \
					ElementType& Obj = JsonArray.AddDefaulted_GetRef(); \
					Obj.FromJson((*It)->AsObject()); \
				} \
			} \
		} \
		else \
		{ \
			if (OptionalJsonArray.IsSet()) \
			{ \
				Serializer.StartArray(TEXTVIEW(JsonName)); \
				for (auto It = OptionalJsonArray->CreateIterator(); It; ++It) \
				{ \
					It->Serialize(Serializer, false); \
				} \
				Serializer.EndArray(); \
			} \
		}

#define JSON_SERIALIZE_MAP_SERIALIZABLE(JsonName, JsonMap, ElementType) \
		if (Serializer.IsLoading()) \
		{ \
			if (Serializer.GetObject()->HasTypedField<EJson::Object>(TEXTVIEW(JsonName))) \
			{ \
				TSharedPtr<FJsonObject> JsonObj = Serializer.GetObject()->GetObjectField(TEXTVIEW(JsonName)); \
				for (auto MapIt = JsonObj->Values.CreateConstIterator(); MapIt; ++MapIt) \
				{ \
					ElementType NewEntry; \
					NewEntry.FromJson(MapIt.Value()->AsObject()); \
					JsonMap.Add(MapIt.Key(), NewEntry); \
				} \
			} \
		} \
		else \
		{ \
			Serializer.StartObject(TEXTVIEW(JsonName)); \
			for (auto It = JsonMap.CreateIterator(); It; ++It) \
			{ \
				Serializer.StartObject(MakeStringView(It.Key())); \
				It.Value().Serialize(Serializer, true); \
				Serializer.EndObject(); \
			} \
			Serializer.EndObject(); \
		}

#define JSON_SERIALIZE_OBJECT_SERIALIZABLE(JsonName, JsonSerializableObject) \
		/* Process the JsonName field differently because it is an object */ \
		if (Serializer.IsLoading()) \
		{ \
			/* Read in the value from the JsonName field */ \
			if (Serializer.GetObject()->HasTypedField<EJson::Object>(TEXTVIEW(JsonName))) \
			{ \
				TSharedPtr<FJsonObject> JsonObj = Serializer.GetObject()->GetObjectField(TEXTVIEW(JsonName)); \
				if (JsonObj.IsValid()) \
				{ \
					(JsonSerializableObject).FromJson(JsonObj); \
				} \
			} \
		} \
		else \
		{ \
			/* Write the value to the Name field */ \
			Serializer.StartObject(TEXTVIEW(JsonName)); \
			(JsonSerializableObject).Serialize(Serializer, true); \
			Serializer.EndObject(); \
		}
		
#define JSON_SERIALIZE_OBJECT_SERIALIZABLE_WITHDEFAULT(JsonName, JsonSerializableObject, JsonSerializableObjectDefault) \
		/* Process the JsonName field differently because it is an object */ \
		if (Serializer.IsLoading()) \
		{ \
			/* Read in the value from the JsonName field */ \
			if (Serializer.GetObject()->HasTypedField<EJson::Object>(TEXTVIEW(JsonName))) \
			{ \
				TSharedPtr<FJsonObject> JsonObj = Serializer.GetObject()->GetObjectField(TEXTVIEW(JsonName)); \
				if (JsonObj.IsValid()) \
				{ \
					(JsonSerializableObject).FromJson(JsonObj); \
				} \
			} \
			else \
			{ \
				JsonSerializableObject = JsonSerializableObjectDefault; \
			} \
		} \
		else \
		{ \
			/* Write the value to the Name field */ \
			Serializer.StartObject(TEXTVIEW(JsonName)); \
			(JsonSerializableObject).Serialize(Serializer, true); \
			Serializer.EndObject(); \
		}

#define JSON_SERIALIZE_OPTIONAL_OBJECT_SERIALIZABLE(JsonName, JsonSerializableObject) \
		if (Serializer.IsLoading()) \
		{ \
			using ObjectType = TRemoveReference<decltype(JsonSerializableObject.GetValue())>::Type; \
			if (Serializer.GetObject()->HasTypedField<EJson::Object>(TEXTVIEW(JsonName))) \
			{ \
				TSharedPtr<FJsonObject> JsonObj = Serializer.GetObject()->GetObjectField(TEXTVIEW(JsonName)); \
				if (JsonObj.IsValid()) \
				{ \
					JsonSerializableObject = ObjectType{}; \
					JsonSerializableObject.GetValue().FromJson(JsonObj); \
				} \
			} \
		} \
		else \
		{ \
			if (JsonSerializableObject.IsSet()) \
			{ \
				Serializer.StartObject(TEXTVIEW(JsonName)); \
				(JsonSerializableObject.GetValue()).Serialize(Serializer, true); \
				Serializer.EndObject(); \
			} \
		}

#define JSON_SERIALIZE_DATETIME_UNIX_TIMESTAMP(JsonName, JsonDateTime) \
		if (Serializer.IsLoading()) \
		{ \
			int64 UnixTimestampValue; \
			Serializer.Serialize(TEXTVIEW(JsonName), UnixTimestampValue); \
			JsonDateTime = FDateTime::FromUnixTimestamp(UnixTimestampValue); \
		} \
		else \
		{ \
			int64 UnixTimestampValue = JsonDateTime.ToUnixTimestamp(); \
			Serializer.Serialize(TEXTVIEW(JsonName), UnixTimestampValue); \
		}

#define JSON_SERIALIZE_DATETIME_UNIX_TIMESTAMP_MILLISECONDS(JsonName, JsonDateTime) \
if (Serializer.IsLoading()) \
{ \
	int64 UnixTimestampValueInMilliseconds; \
	Serializer.Serialize(TEXTVIEW(JsonName), UnixTimestampValueInMilliseconds); \
	JsonDateTime = FDateTime::FromUnixTimestamp(UnixTimestampValueInMilliseconds / 1000); \
} \
else \
{ \
	int64 UnixTimestampValueInMilliseconds = JsonDateTime.ToUnixTimestamp() * 1000; \
	Serializer.Serialize(TEXTVIEW(JsonName), UnixTimestampValueInMilliseconds); \
}

#define JSON_SERIALIZE_ENUM(JsonName, JsonEnum) \
if (Serializer.IsLoading()) \
{ \
	FString JsonTextValue; \
	Serializer.Serialize(TEXTVIEW(JsonName), JsonTextValue); \
	LexFromString(JsonEnum, *JsonTextValue); \
} \
else \
{ \
	FString JsonTextValue = LexToString(JsonEnum); \
	Serializer.Serialize(TEXTVIEW(JsonName), JsonTextValue); \
}

#define JSON_SERIALIZE_ENUM_ARRAY(JsonName, JsonArray, EnumType) \
if (Serializer.IsLoading()) \
{ \
	if (Serializer.GetObject()->HasTypedField<EJson::Array>(TEXTVIEW(JsonName))) \
	{ \
		EnumType EnumValue; \
		for (auto It = Serializer.GetObject()->GetArrayField(TEXTVIEW(JsonName)).CreateConstIterator(); It; ++It) \
		{ \
			LexFromString(EnumValue, *(*It)->AsString()); \
			JsonArray.Add(EnumValue); \
		} \
	} \
} \
else \
{ \
	Serializer.StartArray(TEXTVIEW(JsonName)); \
	for (EnumType& EnumValue : JsonArray) \
	{ \
		FString JsonTextValue = LexToString(EnumValue); \
		Serializer.Serialize(TEXTVIEW(JsonName), JsonTextValue); \
	} \
	Serializer.EndArray(); \
}
