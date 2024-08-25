// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "Misc/CString.h"
#include "Serialization/JsonTypes.h"
#include "Templates/SharedPointer.h"

class FJsonObject;

/**
 * A Json Value is a structure that can be any of the Json Types.
 * It should never be used on its, only its derived types should be used.
 */
class FJsonValue
{
public:

	/** Returns this value as a double, logging an error and returning zero if this is not an Json Number */
	JSON_API double AsNumber() const;

	/** Returns this value as a string, logging an error and returning an empty string if not possible */
	JSON_API FString AsString() const;

	/** Returns this value as a boolean, logging an error and returning false if not possible */
	JSON_API bool AsBool() const;

	/** Returns this value as an array, logging an error and returning an empty array reference if not possible */
	JSON_API const TArray< TSharedPtr<FJsonValue> >& AsArray() const;

	/** Returns this value as an object, throwing an error if this is not an Json Object */
	JSON_API virtual const TSharedPtr<FJsonObject>& AsObject() const;

	/** Tries to convert this value to a number, returning false if not possible */
	virtual bool TryGetNumber(double& OutNumber) const { return false; }

	/** Tries to convert this value to a number, returning false if not possible */
	JSON_API virtual bool TryGetNumber(float& OutNumber) const;

	/** Tries to convert this value to a number, returning false if not possible */
	JSON_API virtual bool TryGetNumber(int8& OutNumber) const;

	/** Tries to convert this value to a number, returning false if not possible */
	JSON_API virtual bool TryGetNumber(int16& OutNumber) const;

	/** Tries to convert this value to a number, returning false if not possible */
	JSON_API virtual bool TryGetNumber(int32& OutNumber) const;

	/** Tries to convert this value to a number, returning false if not possible */
	JSON_API virtual bool TryGetNumber(int64& OutNumber) const;

	/** Tries to convert this value to a number, returning false if not possible */
	JSON_API virtual bool TryGetNumber(uint8& OutNumber) const;

	/** Tries to convert this value to a number, returning false if not possible */
	JSON_API virtual bool TryGetNumber(uint16& OutNumber) const;

	/** Tries to convert this value to a number, returning false if not possible */
	JSON_API virtual bool TryGetNumber(uint32& OutNumber) const;

	/** Tries to convert this value to a number, returning false if not possible */
	JSON_API virtual bool TryGetNumber(uint64& OutNumber) const;

	/** Tries to convert this value to a string, returning false if not possible */
	virtual bool TryGetString(FString& OutString) const { return false; }

	/** Tries to convert this value to a bool, returning false if not possible */
	virtual bool TryGetBool(bool& OutBool) const { return false; }

	/** Tries to convert this value to an array, returning false if not possible */
	virtual bool TryGetArray(const TArray< TSharedPtr<FJsonValue> >*& OutArray) const { return false; }
	
	/** Tries to convert this value to an array, returning false if not possible */
	virtual bool TryGetArray(TArray< TSharedPtr<FJsonValue> >*& OutArray) { return false; }

	/** Tries to convert this value to an object, returning false if not possible */
	virtual bool TryGetObject(const TSharedPtr<FJsonObject>*& Object) const { return false; }

	/** Tries to convert this value to an object, returning false if not possible */
	virtual bool TryGetObject(TSharedPtr<FJsonObject>*& Object) { return false; }

	/** Returns whether or not a caller should prefer a string representation of the value, rather than the natural JSON type */
	virtual bool PreferStringRepresentation() const { return false; }

	/** Returns true if this value is a 'null' */
	bool IsNull() const { return Type == EJson::Null || Type == EJson::None; }

	/** Get a field of the same type as the argument */
	void AsArgumentType(double                          & Value) { Value = AsNumber(); }
	void AsArgumentType(FString                         & Value) { Value = AsString(); }
	void AsArgumentType(bool                            & Value) { Value = AsBool  (); }
	void AsArgumentType(TArray< TSharedPtr<FJsonValue> >& Value) { Value = AsArray (); }
	void AsArgumentType(TSharedPtr<FJsonObject>         & Value) { Value = AsObject(); }

	EJson Type;

	static JSON_API TSharedPtr<FJsonValue> Duplicate(const TSharedPtr<const FJsonValue>& Src);
	static JSON_API TSharedPtr<FJsonValue> Duplicate(const TSharedPtr<FJsonValue>& Src);

	static JSON_API bool CompareEqual(const FJsonValue& Lhs, const FJsonValue& Rhs);

protected:

	FJsonValue() : Type(EJson::None) {}
	virtual ~FJsonValue() {}

	virtual FString GetType() const = 0;

	JSON_API void ErrorMessage(const FString& InType) const;

	friend inline bool operator==(const FJsonValue& Lhs, const FJsonValue& Rhs)
	{
		return FJsonValue::CompareEqual(Lhs, Rhs);
	}

	friend inline bool operator!=(const FJsonValue& Lhs, const FJsonValue& Rhs)
	{
		return !FJsonValue::CompareEqual(Lhs, Rhs);
	}
};


/** A Json String Value. */
class FJsonValueString : public FJsonValue
{
public:
	FJsonValueString(const FString& InString) : Value(InString) {Type = EJson::String;}
	FJsonValueString(FString&& InString) : Value(MoveTemp(InString)) {Type = EJson::String;}

	virtual bool TryGetString(FString& OutString) const override	{ OutString = Value; return true; }
	virtual bool TryGetNumber(double& OutDouble) const override		{ if (Value.IsNumeric()) { OutDouble = FCString::Atod(*Value); return true; } else { return false; } }
	virtual bool TryGetNumber(int32& OutValue) const override		{ LexFromString(OutValue, *Value); return true; }
	virtual bool TryGetNumber(uint32& OutValue) const override		{ LexFromString(OutValue, *Value); return true; }
	virtual bool TryGetNumber(int64& OutValue) const override		{ LexFromString(OutValue, *Value); return true; }
	virtual bool TryGetNumber(uint64& OutValue) const override		{ LexFromString(OutValue, *Value); return true; }
	virtual bool TryGetBool(bool& OutBool) const override			{ OutBool = Value.ToBool(); return true; }

	// Way to check if string value is empty without copying the string 
	bool IsEmpty() const { return Value.IsEmpty(); }

protected:
	FString Value;

	virtual FString GetType() const override {return TEXT("String");}
};


/** A Json Number Value. */
class FJsonValueNumber : public FJsonValue
{
public:
	FJsonValueNumber(double InNumber) : Value(InNumber) {Type = EJson::Number;}
	virtual bool TryGetNumber(double& OutNumber) const override		{ OutNumber = Value; return true; }
	virtual bool TryGetBool(bool& OutBool) const override			{ OutBool = (Value != 0.0); return true; }
	virtual bool TryGetString(FString& OutString) const override	{ OutString = FString::SanitizeFloat(Value, 0); return true; }
	
protected:

	double Value;

	virtual FString GetType() const override {return TEXT("Number");}
};


/** A Json Number Value, stored internally as a string so as not to lose precision */
class FJsonValueNumberString : public FJsonValue
{
public:
	FJsonValueNumberString(const FString& InString) : Value(InString) { Type = EJson::Number; }
	FJsonValueNumberString(FString&& InString) : Value(MoveTemp(InString)) { Type = EJson::Number; }

	virtual bool TryGetString(FString& OutString) const override { OutString = Value; return true; }
	virtual bool TryGetNumber(double& OutDouble) const override { return LexTryParseString(OutDouble, *Value); }
	virtual bool TryGetNumber(float &OutDouble) const override { return LexTryParseString(OutDouble, *Value); }
	virtual bool TryGetNumber(int8& OutValue) const override { return LexTryParseString(OutValue, *Value); }
	virtual bool TryGetNumber(int16& OutValue) const override { return LexTryParseString(OutValue, *Value); }
	virtual bool TryGetNumber(int32& OutValue) const override { return LexTryParseString(OutValue, *Value); }
	virtual bool TryGetNumber(int64& OutValue) const override { return LexTryParseString(OutValue, *Value); }
	virtual bool TryGetNumber(uint8& OutValue) const override { return LexTryParseString(OutValue, *Value); }
	virtual bool TryGetNumber(uint16& OutValue) const override { return LexTryParseString(OutValue, *Value); }
	virtual bool TryGetNumber(uint32& OutValue) const override { return LexTryParseString(OutValue, *Value); }
	virtual bool TryGetNumber(uint64& OutValue) const override { return LexTryParseString(OutValue, *Value); }
	virtual bool TryGetBool(bool& OutBool) const override { OutBool = Value.ToBool(); return true; }
	virtual bool PreferStringRepresentation() const override { return true; }

protected:
	FString Value;

	virtual FString GetType() const override { return TEXT("NumberString"); }
};


/** A Json Boolean Value. */
class FJsonValueBoolean : public FJsonValue
{
public:
	FJsonValueBoolean(bool InBool) : Value(InBool) {Type = EJson::Boolean;}
	virtual bool TryGetNumber(double& OutNumber) const override		{ OutNumber = Value ? 1 : 0; return true; }
	virtual bool TryGetBool(bool& OutBool) const override			{ OutBool = Value; return true; }
	virtual bool TryGetString(FString& OutString) const override	{ OutString = Value ? TEXT("true") : TEXT("false"); return true; }
	
protected:
	bool Value;

	virtual FString GetType() const override {return TEXT("Boolean");}
};


/** A Json Array Value. */
class FJsonValueArray : public FJsonValue
{
public:
	FJsonValueArray(const TArray< TSharedPtr<FJsonValue> >& InArray) : Value(InArray) {Type = EJson::Array;}
	FJsonValueArray(TArray< TSharedPtr<FJsonValue> >&& InArray) : Value(MoveTemp(InArray)) {Type = EJson::Array;}
	virtual bool TryGetArray(const TArray< TSharedPtr<FJsonValue> >*& OutArray) const override	{ OutArray = &Value; return true; }
	virtual bool TryGetArray(TArray< TSharedPtr<FJsonValue> >*& OutArray) override				{ OutArray = &Value; return true; }
	
protected:
	TArray< TSharedPtr<FJsonValue> > Value;

	virtual FString GetType() const override {return TEXT("Array");}
};


/** A Json Object Value. */
class FJsonValueObject : public FJsonValue
{
public:
	FJsonValueObject(TSharedPtr<FJsonObject> InObject) : Value(MoveTemp(InObject)) {Type = EJson::Object;}
	virtual bool TryGetObject(const TSharedPtr<FJsonObject>*& OutObject) const override	{ OutObject = &Value; return true; }
	virtual bool TryGetObject(TSharedPtr<FJsonObject>*& OutObject) override				{ OutObject = &Value; return true; }
	
protected:
	TSharedPtr<FJsonObject> Value;

	virtual FString GetType() const override {return TEXT("Object");}
};


/** A Json Null Value. */
class FJsonValueNull : public FJsonValue
{
public:
	FJsonValueNull() {Type = EJson::Null;}

protected:
	virtual FString GetType() const override {return TEXT("Null");}
};
