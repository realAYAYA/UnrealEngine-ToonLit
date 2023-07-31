// Copyright Epic Games, Inc. All Rights Reserved.

#include "CADKernel/Core/Parameters.h"

#include "CADKernel/UI/Message.h"

using namespace std;

namespace UE::CADKernel 
{ 

FString FParameterValue::ToString() const
{
	switch (Type)
	{
	case EValue::Double:
		return FString::Printf(TEXT("%f"), Value.Real);
		break;

	case EValue::Boolean:
		return Value.Boolean ? TEXT("true") : TEXT("false");
		break;

	case EValue::Integer:
		return FString::Printf(TEXT("%d"), Value.Integer);
		break;

	case EValue::String:
		return StringValue;
		break;
	}
	return TEXT("");
}

FString FParameter::GetTypeString() const
{
	switch (GetType())
	{
	case EValue::Double:
		return TEXT("Double");

	case EValue::Boolean:
		return TEXT("Boolean");

	case EValue::Integer:
		return TEXT("Integer");

	case EValue::String:
		return TEXT("String");

	default:
		break;
	}
	return TEXT("");
}

bool FParameterValue::operator==(const FParameterValue& Other) const
{
	if (Type != Other.Type) 
	{
		return false;
	}

	switch (Type)
	{
	case EValue::Double:
		return FMath::IsNearlyEqual(Value.Real, Other.Value.Real);

	case EValue::Boolean:
		return Value.Boolean == Other.Value.Boolean;

	case EValue::Integer:
		return Value.Integer == Other.Value.Integer;

	case EValue::String:
		return StringValue == Other.StringValue;

	default:
		break;
	}
	return false;
}

void FParameter::AddToParameterMap(FParameter& Parameter, FParameters& Parameters)
{
	Parameters.Add(Parameter);
}

void FParameter::SetFromString(const FString& String)
{
	switch (GetType())
	{
	case EValue::Double:
		{
			double TmpValue = FCString::Atod(*String);
			SetValue(TmpValue);
		}
		break;

	case EValue::Integer:
		{
			int32 TmpValue = FCString::Atoi(*String);
			SetValue(TmpValue);
		}
		break;

	case EValue::Boolean:
		{
			FString lowerStr = String.ToLower();

			if (lowerStr == TEXT("true"))
			{
				SetValue(true);
			}
			else if (lowerStr == TEXT("false"))
			{
				SetValue(false);
			}
			else if (lowerStr == TEXT("0"))
			{
				SetValue(false);
			}
			else {
				SetValue(true);
			}
		}
		break;

	case EValue::String:
		{
			SetValue(String);
		}
		break;

	default:
		break;
	}
}

void FParameters::SetFromString(const FString& ParameterStr)
{
	if (ParameterStr.Len() > 0 && ParameterStr[0] == TEXT('?')) 
	{
		PrintParameterList();
	}

	TArray<FString> ParameterArray;
	ParameterStr.ParseIntoArray(ParameterArray, TEXT(";"));

	for (FString& ParameterString : ParameterArray)
	{
		FString Name;
		FString Value;
		if (ParameterString.Split(TEXT("="), &Name, &Value))
		{
			FParameter* Parameter = GetByName(Name);
			if (Parameter == nullptr)
			{
				continue;
			}
			Parameter->SetFromString(Value);
		}
	}
}

void FParameters::PrintParameterList()
{
	FMessage::Printf(Log, TEXT("Extra parameters:\n"));
	for (const TPair<FString, FParameter*>& Parameter : Map)
	{
		FMessage::Printf(Log, TEXT(" - %s : %s (default = %s)\n"), *Parameter.Key, *Parameter.Value->GetValue().ToString(), *Parameter.Value->GetDefaultValue().ToString());
	}
}

FString FParameters::ToString(bool bOnlyChanged) const
{
	FString String;
	bool bIsFirst = true;
	for (const TPair<FString, FParameter*>& Pair : Map)
	{
		const FParameter* Parameter = Pair.Value;
		if (bOnlyChanged && Parameter->IsDefault()) 
		{
			continue;
		}

		if (!bIsFirst) 
		{
			String += TEXT(";");
		}

		String += Parameter->ToString();
		bIsFirst = false;
	}
	return String;
}

FString FParameter::ToString() const
{
	return GetName() + FString(TEXT("=")) + GetValue().ToString();
}

}