// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendLiteralBlueprintAccess.h"

namespace MetasoundFrontendLiteralBlueprintAccessPrivate
{
	template <typename TLiteralType>
	FMetasoundFrontendLiteral CreatePODMetaSoundLiteral(const TLiteralType& Value)
	{
		FMetasoundFrontendLiteral Literal;
		Literal.Set(Value);
		return Literal;
	}
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateBoolMetaSoundLiteral(bool Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateBoolArrayMetaSoundLiteral(
	const TArray<bool>& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateFloatMetaSoundLiteral(float Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateFloatArrayMetaSoundLiteral(
	const TArray<float>& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateIntMetaSoundLiteral(int32 Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateIntArrayMetaSoundLiteral(
	const TArray<int32>& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateObjectMetaSoundLiteral(UObject* Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateObjectArrayMetaSoundLiteral(
	const TArray<UObject*>& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateStringMetaSoundLiteral(const FString& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateStringArrayMetaSoundLiteral(
	const TArray<FString>& Value)
{
	return MetasoundFrontendLiteralBlueprintAccessPrivate::CreatePODMetaSoundLiteral(Value);
}

FMetasoundFrontendLiteral UMetasoundFrontendLiteralBlueprintAccess::CreateMetaSoundLiteralFromParam(
	const FAudioParameter& Param)
{
	return FMetasoundFrontendLiteral{ Param };
}
