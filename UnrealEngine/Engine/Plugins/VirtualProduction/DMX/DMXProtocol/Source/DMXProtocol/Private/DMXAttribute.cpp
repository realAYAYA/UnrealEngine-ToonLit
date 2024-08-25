// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXAttribute.h"

#include "DMXProtocolModule.h"
#include "DMXProtocolSettings.h"
#include "Modules/ModuleManager.h"


FDMXAttributeName::FDMXAttributeName()
{
	// This depends on the FDMXProtocolModule and can be called
	// on CDO creation, when the module might not be available yet.
	// So we first check if it is available.
	const IModuleInterface* DMXProtocolModule = FModuleManager::Get().GetModule("DMXProtocol");
	if (DMXProtocolModule != nullptr)
	{
		if (const UDMXProtocolSettings* DMXSettings = GetDefault<UDMXProtocolSettings>())
		{
			if (DMXSettings->Attributes.Num() > 0)
			{
				Name = DMXSettings->Attributes.begin()->Name;
			}
		}
	}
}

FDMXAttributeName::FDMXAttributeName(const FDMXAttribute& InAttribute)
{
	Name = InAttribute.Name;
}

FDMXAttributeName::FDMXAttributeName(const FName& NameAttribute)
{
	Name = NameAttribute;
}

void FDMXAttributeName::SetFromName(const FName& InName)
{
	*this = InName;
}

TArray<FName> FDMXAttributeName::GetPredefinedValues()
{
	TArray<FName> Result;
	const UDMXProtocolSettings* DMXSettings = GetDefault<UDMXProtocolSettings>();
	if (!DMXSettings)
	{
		return Result;
	}

	for (const FDMXAttribute& Attribute : DMXSettings->Attributes)
	{
		Result.Add(Attribute.Name);
	}
	return Result;
}

FString UDMXAttributeNameConversions::Conv_DMXAttributeToString(const FDMXAttributeName& InAttribute)
{
	return InAttribute.Name.ToString();
}

FName UDMXAttributeNameConversions::Conv_DMXAttributeToName(const FDMXAttributeName& InAttribute)
{
	return InAttribute.Name;
}

TArray<FString> FDMXAttribute::GetKeywords() const
{
	TArray<FString> CleanedKeywords;
	Keywords.ParseIntoArray(CleanedKeywords, TEXT(","));
	for (FString& CleanKeyword : CleanedKeywords)
	{
		CleanKeyword.TrimStartAndEndInline();
	}

	return CleanedKeywords;
}

void FDMXAttribute::CleanupKeywords()
{
	// support tabs too
	Keywords = Keywords.ConvertTabsToSpaces(1);
	Keywords.TrimStartAndEndInline();
	TArray<FString> KeywordsArray;
	Keywords.ParseIntoArray(KeywordsArray, TEXT(","), true);
	Keywords = FString::Join(KeywordsArray, TEXT(", "));
}
