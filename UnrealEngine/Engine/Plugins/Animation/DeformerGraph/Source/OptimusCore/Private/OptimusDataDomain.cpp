// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataDomain.h"

#include "OptimusExpressionEvaluator.h"
#include "OptimusObjectVersion.h"
#include "String/ParseTokens.h"


namespace Optimus::DomainName
{
	const FName Singleton("Singleton");
	const FName Vertex("Vertex");
	const FName Triangle("Triangle");
	const FName Bone("Bone");
	const FName UVChannel("UVChannel");
	const FName Index0("Index0");
}

FString Optimus::FormatDimensionNames(const TArray<FName>& InNames)
{
	TArray<FString> NameParts;
	for (FName Name: InNames)
	{
		NameParts.Add(Name.ToString());
	}
	return FString::Join(NameParts, TEXT(" › "));
}


TOptional<int32> FOptimusDataDomain::GetElementCount(TMap<FName, int32> InDomainCounts) const
{
	switch(Type)
	{
	case EOptimusDataDomainType::Dimensional:
		{
			if (DimensionNames.IsEmpty())
			{
				return 1;
			}
			else if (DimensionNames.Num() == 1)
			{
				if (const int32* Value = InDomainCounts.Find(DimensionNames[0]))
				{
					return *Value;
				}
			}
		}
		break;
	case EOptimusDataDomainType::Expression:
		return Optimus::Expression::FEngine(InDomainCounts).Evaluate(Expression);
	}
	
	return {};
}


FString FOptimusDataDomain::ToString() const
{
	switch(Type)
	{
	case EOptimusDataDomainType::Dimensional:
		{
			if (DimensionNames.IsEmpty())
			{
				return TEXT("Empty");
			}
			else if (DimensionNames.Num() == 1)
			{
				FString Result;
				for (FName Name: DimensionNames)
				{
					Result += Name.ToString();
					Result += TEXT("/");
				}
				Result += FString::FromInt(Multiplier);
				return Result;
			}
		}
		break;
	case EOptimusDataDomainType::Expression:
		return FString::Printf(TEXT("'%s'"), *Expression);
	}
	
	return {};
}


FOptimusDataDomain FOptimusDataDomain::FromString(
	const FString& InString
	)
{
	FString String = InString.TrimStartAndEnd();
	if (String.StartsWith(TEXT("'")))
	{
		return FOptimusDataDomain(String.Mid(1, String.Len() - 2));
	}

	TArray<FStringView> Parts;
	UE::String::ParseTokens(InString, TEXT("/"), Parts);
	if (Parts.Num() < 2)
	{
		return {};
	}

	const int32 Multiplier = FCString::Atoi(*FString(Parts.Pop()));
	TArray<FName> Names;
	for (FStringView Part: Parts)
	{
		Names.Add(FName(Part));
	}
	return FOptimusDataDomain(Names, Multiplier);
}


bool FOptimusDataDomain::operator==(const FOptimusDataDomain& InOtherDomain) const
{
	if (Type != InOtherDomain.Type)
	{
		return false;
	}

	switch(Type)
	{
	case EOptimusDataDomainType::Dimensional:
		return DimensionNames == InOtherDomain.DimensionNames && Multiplier == InOtherDomain.Multiplier;
	case EOptimusDataDomainType::Expression:
		return TStringView(Expression).TrimStartAndEnd().Compare(TStringView(InOtherDomain.Expression).TrimStartAndEnd()) == 0;
	}

	return false;
}

void FOptimusDataDomain::PostSerialize(const FArchive& Ar)
{
	if (Ar.IsLoading() &&
		Ar.CustomVer(FOptimusObjectVersion::GUID) < FOptimusObjectVersion::DataDomainExpansion)
	{
		BackCompFixupLevels();
	}
}


void FOptimusDataDomain::BackCompFixupLevels()
{
	if (!LevelNames_DEPRECATED.IsEmpty())
	{
		DimensionNames = LevelNames_DEPRECATED;
		LevelNames_DEPRECATED.Reset();
	}
}
