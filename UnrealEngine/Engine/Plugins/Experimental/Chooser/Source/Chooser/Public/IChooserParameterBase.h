// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IHasContext.h"
#include "IObjectChooser.h"
#include "IChooserParameterBase.generated.h"

struct FAssetRegistryTag;

USTRUCT()
struct FChooserParameterBase
{
	GENERATED_BODY()

	virtual FString GetDebugName() const
	{
		FText Name;
		GetDisplayName(Name);
		return Name.ToString();
	}
	
	virtual void GetDisplayName(FText& OutName) const { }
	virtual void AddSearchNames(FStringBuilderBase& Builder) const { }
	virtual void ReplaceString(FStringView FindString, ESearchCase::Type, bool MatchWholeWord, FStringView ReplaceString) { }

	virtual void PostLoad() {};
	virtual void Compile(IHasContextClass* Owner, bool bForce) {};

	virtual ~FChooserParameterBase() {}
};