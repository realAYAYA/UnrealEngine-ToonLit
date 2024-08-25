// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Dom/JsonValue.h"
#include "HAL/Platform.h"
#include "JsonGlobals.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Serialization/JsonTypes.h"
#include "Templates/SharedPointer.h"
#include "Trace/Detail/Channel.h"

/**
 * A Json Object is a structure holding an unordered set of name/value pairs.
 * In a Json file, it is represented by everything between curly braces {}.
 */
class FJsonObject
{
public:

	TMap<FString, TSharedPtr<FJsonValue>> Values;

	template<EJson JsonType>
	TSharedPtr<FJsonValue> GetField(FStringView FieldName) const
	{
		return GetField(FieldName, JsonType);
	}

	TSharedPtr<FJsonValue> GetField(FStringView FieldName, EJson JsonType) const
	{
		const TSharedPtr<FJsonValue>* Field = Values.FindByHash(GetTypeHash(FieldName), FieldName);
		if (Field != nullptr && Field->IsValid())
		{
			if (JsonType == EJson::None || (*Field)->Type == JsonType)
			{
				return (*Field);
			}
			else
			{
				UE_LOG(LogJson, Warning, TEXT("Field %.*s is of the wrong type."), FieldName.Len(), FieldName.GetData());
			}
		}
		else
		{
			UE_LOG(LogJson, Warning, TEXT("Field %.*s was not found."), FieldName.Len(), FieldName.GetData());
		}

		return MakeShared<FJsonValueNull>();
	}

	/**
	 * Attempts to get the field with the specified name.
	 *
	 * @param FieldName The name of the field to get.
	 * @return A pointer to the field, or nullptr if the field doesn't exist.
	 */
	TSharedPtr<FJsonValue> TryGetField(FStringView FieldName) const
	{
		const TSharedPtr<FJsonValue>* Field = Values.FindByHash(GetTypeHash(FieldName), FieldName);
		return (Field != nullptr && Field->IsValid()) ? *Field : TSharedPtr<FJsonValue>();
	}

	/**
	 * Checks whether a field with the specified name exists in the object.
	 *
	 * @param FieldName The name of the field to check.
	 * @return true if the field exists, false otherwise.
	 */
	bool HasField(FStringView FieldName) const
	{
		const TSharedPtr<FJsonValue>* Field = Values.FindByHash(GetTypeHash(FieldName), FieldName);
		if(Field && Field->IsValid())
		{
			return true;
		}

		return false;
	}
	
	/**
	 * Checks whether a field with the specified name and type exists in the object.
	 *
	 * @tparam JsonType The type of the field to check.
	 * @param FieldName The name of the field to check.
	 * @return true if the field exists, false otherwise.
	 */
	template<EJson JsonType>
	bool HasTypedField(FStringView FieldName) const
	{
		return HasTypedField(FieldName, JsonType);
	}

	/**
	 * Checks whether a field with the specified name and type exists in the object.
	 *
	 * @param JsonType The type of the field to check.
	 * @param FieldName The name of the field to check.
	 * @return true if the field exists, false otherwise.
	 */
	bool HasTypedField(FStringView FieldName, EJson JsonType) const
	{
		const TSharedPtr<FJsonValue>* Field = Values.FindByHash(GetTypeHash(FieldName), FieldName);
		if (Field && Field->IsValid() && ((*Field)->Type == JsonType))
		{
			return true;
		}

		return false;
	}

	/**
	 * Sets the value of the field with the specified name.
	 *
	 * @param FieldName The name of the field to set.
	 * @param Value The value to set.
	 */
	JSON_API void SetField(FString&& FieldName, const TSharedPtr<FJsonValue>& Value);
	JSON_API void SetField(const FString& FieldName, const TSharedPtr<FJsonValue>& Value);

	/**
	 * Removes the field with the specified name.
	 *
	 * @param FieldName The name of the field to remove.
	 */
	JSON_API void RemoveField(FStringView FieldName);

	/**
	 * Gets the field with the specified name as a number.
	 *
	 * Ensures that the field is present and is of type Json number.
	 *
	 * @param FieldName The name of the field to get.
	 * @return The field's value as a number.
	 */
	JSON_API double GetNumberField(FStringView FieldName) const;

	/**
	 * Gets a numeric field and casts to an int32
	 */
	FORCEINLINE int32 GetIntegerField(FStringView FieldName) const
	{
		return (int32)GetNumberField(FieldName);
	}

	/** Get the field named FieldName as a number. Returns false if it doesn't exist or cannot be converted. */
	JSON_API bool TryGetNumberField(FStringView FieldName, float& OutNumber) const;

	/** Get the field named FieldName as a number. Returns false if it doesn't exist or cannot be converted. */
	JSON_API bool TryGetNumberField(FStringView FieldName, double& OutNumber) const;

	/** Get the field named FieldName as a number, and makes sure it's within int8 range. Returns false if it doesn't exist or cannot be converted. */
	JSON_API bool TryGetNumberField(FStringView FieldName, int8& OutNumber) const;

	/** Get the field named FieldName as a number, and makes sure it's within int16 range. Returns false if it doesn't exist or cannot be converted. */
	JSON_API bool TryGetNumberField(FStringView FieldName, int16& OutNumber) const;

	/** Get the field named FieldName as a number, and makes sure it's within int32 range. Returns false if it doesn't exist or cannot be converted. */
	JSON_API bool TryGetNumberField(FStringView FieldName, int32& OutNumber) const;

	/** Get the field named FieldName as a number. Returns false if it doesn't exist or cannot be converted. */
	JSON_API bool TryGetNumberField(FStringView FieldName, int64& OutNumber) const;

	/** Get the field named FieldName as a number, and makes sure it's within uint8 range. Returns false if it doesn't exist or cannot be converted. */
	JSON_API bool TryGetNumberField(FStringView FieldName, uint8& OutNumber) const;

	/** Get the field named FieldName as a number, and makes sure it's within uint16 range. Returns false if it doesn't exist or cannot be converted. */
	JSON_API bool TryGetNumberField(FStringView FieldName, uint16& OutNumber) const;

	/** Get the field named FieldName as a number, and makes sure it's within uint32 range. Returns false if it doesn't exist or cannot be converted.  */
	JSON_API bool TryGetNumberField(FStringView FieldName, uint32& OutNumber) const;

	/** Get the field named FieldName as a number, and makes sure it's within uint64 range. Returns false if it doesn't exist or cannot be converted.  */
	JSON_API bool TryGetNumberField(FStringView FieldName, uint64& OutNumber) const;

	/** Add a field named FieldName with Number as value */
	JSON_API void SetNumberField(FString&& FieldName, double Number);
	JSON_API void SetNumberField(const FString& FieldName, double Number);

	/** Get the field named FieldName as a string. */
	JSON_API FString GetStringField(FStringView FieldName) const;

	/** Get the field named FieldName as a string. Returns false if it doesn't exist or cannot be converted. */
	JSON_API bool TryGetStringField(FStringView FieldName, FString& OutString) const;

	/** Get the field named FieldName as an array of strings. Returns false if it doesn't exist or any member cannot be converted. */
	JSON_API bool TryGetStringArrayField(FStringView FieldName, TArray<FString>& OutArray) const;

	/** Get the field named FieldName as an array of enums. Returns false if it doesn't exist or any member is not a string. */
	template<typename TEnum>
	bool TryGetEnumArrayField(FStringView FieldName, TArray<TEnum>& OutArray) const
	{
		TArray<FString> Strings;
		if (!TryGetStringArrayField(FieldName, Strings))
		{
			return false;
		}

		OutArray.Empty();
		for (const FString& String : Strings)
		{
			TEnum Value;
			if (LexTryParseString(Value, *String))
			{
				OutArray.Add(Value);
			}
		}
		return true;
	}

	/** Add a field named FieldName with value of StringValue */
	JSON_API void SetStringField(FString&& FieldName, FString&& StringValue);
	JSON_API void SetStringField(FString&& FieldName, const FString& StringValue);
	JSON_API void SetStringField(const FString& FieldName, FString&& StringValue);
	JSON_API void SetStringField(const FString& FieldName, const FString& StringValue);

	/**
	 * Gets the field with the specified name as a boolean.
	 *
	 * Ensures that the field is present and is of type Json number.
	 *
	 * @param FieldName The name of the field to get.
	 * @return The field's value as a boolean.
	 */
	JSON_API bool GetBoolField(FStringView FieldName) const;

	/** Get the field named FieldName as a string. Returns false if it doesn't exist or cannot be converted. */
	JSON_API bool TryGetBoolField(FStringView FieldName, bool& OutBool) const;

	/** Set a boolean field named FieldName and value of InValue */
	JSON_API void SetBoolField(FString&& FieldName, bool InValue);
	JSON_API void SetBoolField(const FString& FieldName, bool InValue);

	/** Get the field named FieldName as an array. */
	JSON_API const TArray<TSharedPtr<FJsonValue>>& GetArrayField(FStringView FieldName) const;

	/** Try to get the field named FieldName as an array, or return false if it's another type */
	JSON_API bool TryGetArrayField(FStringView FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutArray) const;

	/** Set an array field named FieldName and value of Array */
	JSON_API void SetArrayField(FString&& FieldName, TArray<TSharedPtr<FJsonValue>>&& Array);
	JSON_API void SetArrayField(FString&& FieldName, const TArray<TSharedPtr<FJsonValue>>& Array);
	JSON_API void SetArrayField(const FString& FieldName, TArray<TSharedPtr<FJsonValue>>&& Array);
	JSON_API void SetArrayField(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>& Array);

	/**
	 * Gets the field with the specified name as a Json object.
	 *
	 * Ensures that the field is present and is of type Json object.
	 *
	 * @param FieldName The name of the field to get.
	 * @return The field's value as a Json object.
	 */
	JSON_API const TSharedPtr<FJsonObject>& GetObjectField(FStringView FieldName) const;

	/** Try to get the field named FieldName as an object, or return false if it's another type */
	JSON_API bool TryGetObjectField(FStringView FieldName, const TSharedPtr<FJsonObject>*& OutObject) const;

	/** Set an ObjectField named FieldName and value of JsonObject */
	JSON_API void SetObjectField(FString&& FieldName, const TSharedPtr<FJsonObject>& JsonObject);
	JSON_API void SetObjectField(const FString& FieldName, const TSharedPtr<FJsonObject>& JsonObject);

	static JSON_API void Duplicate(const TSharedPtr<const FJsonObject>& Source, const TSharedPtr<FJsonObject>& Dest);
	static JSON_API void Duplicate(const TSharedPtr<FJsonObject>& Source, TSharedPtr<FJsonObject>& Dest);

#if !PLATFORM_TCHAR_IS_UTF8CHAR

	template <EJson JsonType>
	UE_DEPRECATED(5.4, "Passing an ANSI string to GetField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	TSharedPtr<FJsonValue> GetField(FAnsiStringView FieldName) const
	{
		return GetField<JsonType>(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()));
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	TSharedPtr<FJsonValue> TryGetField(FAnsiStringView FieldName) const
	{
		return TryGetField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()));
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to HasField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool HasField(FAnsiStringView FieldName) const
	{
		return HasField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()));
	}

	template <EJson JsonType>
	UE_DEPRECATED(5.4, "Passing an ANSI string to HasTypedField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool HasTypedField(FAnsiStringView FieldName) const
	{
		return HasTypedField<JsonType>(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()));
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to RemoveField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	void RemoveField(FAnsiStringView FieldName)
	{
		return RemoveField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()));
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to GetNumberField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	double GetNumberField(FAnsiStringView FieldName) const
	{
		return GetNumberField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()));
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to GetIntegerField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	int32 GetIntegerField(FAnsiStringView FieldName) const
	{
		return GetIntegerField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()));
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetNumberField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetNumberField(FAnsiStringView FieldName, float& OutNumber) const
	{
		return TryGetNumberField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutNumber);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetNumberField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetNumberField(FAnsiStringView FieldName, double& OutNumber) const
	{
		return TryGetNumberField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutNumber);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetNumberField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetNumberField(FAnsiStringView FieldName, int8& OutNumber) const
	{
		return TryGetNumberField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutNumber);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetNumberField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetNumberField(FAnsiStringView FieldName, int16& OutNumber) const
	{
		return TryGetNumberField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutNumber);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetNumberField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetNumberField(FAnsiStringView FieldName, int32& OutNumber) const
	{
		return TryGetNumberField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutNumber);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetNumberField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetNumberField(FAnsiStringView FieldName, int64& OutNumber) const
	{
		return TryGetNumberField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutNumber);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetNumberField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetNumberField(FAnsiStringView FieldName, uint8& OutNumber) const
	{
		return TryGetNumberField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutNumber);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetNumberField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetNumberField(FAnsiStringView FieldName, uint16& OutNumber) const
	{
		return TryGetNumberField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutNumber);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetNumberField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetNumberField(FAnsiStringView FieldName, uint32& OutNumber) const
	{
		return TryGetNumberField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutNumber);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetNumberField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetNumberField(FAnsiStringView FieldName, uint64& OutNumber) const
	{
		return TryGetNumberField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutNumber);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to GetStringField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	FString GetStringField(FAnsiStringView FieldName) const
	{
		return GetStringField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()));
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetStringField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetStringField(FAnsiStringView FieldName, FString& OutString) const
	{
		return TryGetStringField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutString);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetStringArrayField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetStringArrayField(FAnsiStringView FieldName, TArray<FString>& OutArray) const
	{
		return TryGetStringArrayField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutArray);
	}

	template <typename TEnum>
	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetEnumArrayField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetEnumArrayField(FAnsiStringView FieldName, TArray<TEnum>& OutArray) const
	{
		return TryGetEnumArrayField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutArray);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to GetBoolField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool GetBoolField(FAnsiStringView FieldName) const
	{
		return GetBoolField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()));
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetBoolField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetBoolField(FAnsiStringView FieldName, bool& OutBool) const
	{
		return TryGetBoolField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutBool);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to GetArrayField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	const TArray<TSharedPtr<FJsonValue>>& GetArrayField(FAnsiStringView FieldName) const
	{
		return GetArrayField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()));
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetArrayField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetArrayField(FAnsiStringView FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutArray) const
	{
		return TryGetArrayField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutArray);
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to GetObjectField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	const TSharedPtr<FJsonObject>& GetObjectField(FAnsiStringView FieldName) const
	{
		return GetObjectField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()));
	}

	UE_DEPRECATED(5.4, "Passing an ANSI string to TryGetObjectField has been deprecated outside of UTF-8 mode. Please use the overload that takes a TCHAR string.")
	bool TryGetObjectField(FAnsiStringView FieldName, const TSharedPtr<FJsonObject>*& OutObject) const
	{
		return TryGetObjectField(StringCast<TCHAR>(FieldName.GetData(), FieldName.Len()), OutObject);
	}

#endif // !PLATFORM_TCHAR_IS_UTF8CHAR

};
