// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Dom/JsonObject.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonSerializerReader.h"

/**
 * Base class for a JSON serializable object
 */
struct FJsonSerializable
{
	/**
	 *	Virtualize destructor as we provide overridable functions
	 */
	JSON_API virtual ~FJsonSerializable();

	/**
	 * Used to allow serialization of a const ref
	 *
	 * @return the corresponding json string
	 */
	JSON_API const FString ToJson(bool bPrettyPrint = true) const;
	/**
	 * Serializes this object to its JSON string form
	 *
	 * @param bPrettyPrint - If true, will use the pretty json formatter
	 * @return the corresponding json string
	 */
	JSON_API virtual const FString ToJson(bool bPrettyPrint=true);
	JSON_API virtual void ToJson(TSharedRef<TJsonWriter<> >& JsonWriter, bool bFlatObject) const;
	JSON_API virtual void ToJson(TSharedRef< TJsonWriter< TCHAR, TCondensedJsonPrintPolicy< TCHAR > > >& JsonWriter, bool bFlatObject) const;

	/**
	 * Serializes the contents of a JSON string into this object
	 *
	 * @param Json the JSON data to serialize from
	 */
	JSON_API virtual bool FromJson(const FString& Json);

	/**
	 * Serializes the contents of a JSON string into this object
	 *
	 * @param Json the JSON data to serialize from
	 */
	JSON_API virtual bool FromJson(FString&& Json);

	/**
	 * Serializes the contents of a JSON string into this object using FUtf8StringView
	 *
	 * @param JsonStringView the JSON data to serialize from
	 */
	JSON_API bool FromJsonStringView(FUtf8StringView JsonStringView);

	/**
	 * Serializes the contents of a JSON string into this object using FWideStringView
	 *
	 * @param JsonStringView the JSON data to serialize from
	 */
	JSON_API bool FromJsonStringView(FWideStringView JsonStringView);
 

	JSON_API virtual bool FromJson(TSharedPtr<FJsonObject> JsonObject);

	/**
	 * Abstract method that needs to be supplied using the macros
	 *
	 * @param Serializer the object that will perform serialization in/out of JSON
	 * @param bFlatObject if true then no object wrapper is used
	 */
	JSON_API virtual void Serialize(FJsonSerializerBase& Serializer, bool bFlatObject) = 0;
};
