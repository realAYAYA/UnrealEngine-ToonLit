// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dom/JsonObject.h"

void FJsonObject::SetField(FString&& FieldName, const TSharedPtr<FJsonValue>& Value)
{
	this->Values.Add(MoveTemp(FieldName), Value);
}

void FJsonObject::SetField(const FString& FieldName, const TSharedPtr<FJsonValue>& Value)
{
	SetField(CopyTemp(FieldName), Value);
}

void FJsonObject::RemoveField(FStringView FieldName)
{
	this->Values.RemoveByHash(GetTypeHash(FieldName), FieldName);
}

double FJsonObject::GetNumberField(FStringView FieldName) const
{
	return GetField<EJson::None>(FieldName)->AsNumber();
}

bool FJsonObject::TryGetNumberField(FStringView FieldName, float& OutNumber) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(FStringView FieldName, double& OutNumber) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(FStringView FieldName, int8& OutNumber) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(FStringView FieldName, int16& OutNumber) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(FStringView FieldName, int32& OutNumber) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(FStringView FieldName, int64& OutNumber) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(FStringView FieldName, uint8& OutNumber) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(FStringView FieldName, uint16& OutNumber) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(FStringView FieldName, uint32& OutNumber) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

bool FJsonObject::TryGetNumberField(FStringView FieldName, uint64& OutNumber) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetNumber(OutNumber);
}

void FJsonObject::SetNumberField(FString&& FieldName, double Number)
{
	this->Values.Add(MoveTemp(FieldName), MakeShared<FJsonValueNumber>(Number));
}

void FJsonObject::SetNumberField(const FString& FieldName, double Number)
{
	SetNumberField(CopyTemp(FieldName), Number);
}

FString FJsonObject::GetStringField(FStringView FieldName) const
{
	return GetField<EJson::None>(FieldName)->AsString();
}

bool FJsonObject::TryGetStringField(FStringView FieldName, FString& OutString) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetString(OutString);
}

bool FJsonObject::TryGetStringArrayField(FStringView FieldName, TArray<FString>& OutArray) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);

	if (!Field.IsValid())
	{
		return false;
	}

	const TArray< TSharedPtr<FJsonValue> > *Array;

	if (!Field->TryGetArray(Array))
	{
		return false;
	}

	for (int Idx = 0; Idx < Array->Num(); Idx++)
	{
		FString Element;

		if (!(*Array)[Idx]->TryGetString(Element))
		{
			return false;
		}

		OutArray.Add(Element);
	}

	return true;
}

void FJsonObject::SetStringField(FString&& FieldName, FString&& StringValue)
{
	this->Values.Add(MoveTemp(FieldName), MakeShared<FJsonValueString>(MoveTemp(StringValue)));
}

void FJsonObject::SetStringField(FString&& FieldName, const FString& StringValue)
{
	SetStringField(MoveTemp(FieldName), CopyTemp(StringValue));
}

void FJsonObject::SetStringField(const FString& FieldName, FString&& StringValue)
{
	SetStringField(CopyTemp(FieldName), MoveTemp(StringValue));
}

void FJsonObject::SetStringField(const FString& FieldName, const FString& StringValue)
{
	SetStringField(CopyTemp(FieldName), CopyTemp(StringValue));
}

bool FJsonObject::GetBoolField(FStringView FieldName) const
{
	return GetField<EJson::None>(FieldName)->AsBool();
}

bool FJsonObject::TryGetBoolField(FStringView FieldName, bool& OutBool) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetBool(OutBool);
}

void FJsonObject::SetBoolField(FString&& FieldName, bool InValue)
{
	this->Values.Add(MoveTemp(FieldName), MakeShared<FJsonValueBoolean>(InValue));
}

void FJsonObject::SetBoolField(const FString& FieldName, bool InValue)
{
	SetBoolField(CopyTemp(FieldName), InValue);
}

const TArray<TSharedPtr<FJsonValue>>& FJsonObject::GetArrayField(FStringView FieldName) const
{
	return GetField<EJson::Array>(FieldName)->AsArray();
}

bool FJsonObject::TryGetArrayField(FStringView FieldName, const TArray<TSharedPtr<FJsonValue>>*& OutArray) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetArray(OutArray);
}

void FJsonObject::SetArrayField(FString&& FieldName, TArray<TSharedPtr<FJsonValue>>&& Array)
{
	this->Values.Add(MoveTemp(FieldName), MakeShared<FJsonValueArray>(MoveTemp(Array)));
}

void FJsonObject::SetArrayField(FString&& FieldName, const TArray<TSharedPtr<FJsonValue>>& Array)
{
	SetArrayField(MoveTemp(FieldName), CopyTemp(Array));
}

void FJsonObject::SetArrayField(const FString& FieldName, TArray<TSharedPtr<FJsonValue>>&& Array)
{
	SetArrayField(CopyTemp(FieldName), MoveTemp(Array));
}

void FJsonObject::SetArrayField(const FString& FieldName, const TArray<TSharedPtr<FJsonValue>>& Array)
{
	SetArrayField(CopyTemp(FieldName), CopyTemp(Array));
}

const TSharedPtr<FJsonObject>& FJsonObject::GetObjectField(FStringView FieldName) const
{
	return GetField<EJson::Object>(FieldName)->AsObject();
}

bool FJsonObject::TryGetObjectField(FStringView FieldName, const TSharedPtr<FJsonObject>*& OutObject) const
{
	TSharedPtr<FJsonValue> Field = TryGetField(FieldName);
	return Field.IsValid() && Field->TryGetObject(OutObject);
}

void FJsonObject::SetObjectField(FString&& FieldName, const TSharedPtr<FJsonObject>& JsonObject)
{
	if (JsonObject.IsValid())
	{
		this->Values.Add(MoveTemp(FieldName), MakeShared<FJsonValueObject>(JsonObject.ToSharedRef()));
	}
	else
	{
		this->Values.Add(MoveTemp(FieldName), MakeShared<FJsonValueNull>());
	}
}

void FJsonObject::SetObjectField(const FString& FieldName, const TSharedPtr<FJsonObject>& JsonObject)
{
	SetObjectField(CopyTemp(FieldName), JsonObject);
}

void FJsonObject::Duplicate(const TSharedPtr<const FJsonObject>& Source, const TSharedPtr<FJsonObject>& Dest)
{
	if (Source && Dest)
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Source->Values)
		{
			Dest->SetField(Pair.Key, FJsonValue::Duplicate(Pair.Value));
		}
	}
}

void FJsonObject::Duplicate(const TSharedPtr<FJsonObject>& Source, TSharedPtr<FJsonObject>& Dest)
{
	Duplicate(ConstCastSharedPtr<const FJsonObject>(Source), Dest);
}
