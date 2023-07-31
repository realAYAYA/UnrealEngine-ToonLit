// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeaderProvider.h"
#include "UnrealHeaderTool.h"
#include "UnrealTypeDefinitionInfo.h"
#include "ClassMaps.h"
#include "HeaderParser.h"
#include "StringUtils.h"

FHeaderProvider::FHeaderProvider(EHeaderProviderSourceType InType, FString&& InId)
	: Type(InType)
	, Id(MoveTemp(InId))
{
}

FHeaderProvider::FHeaderProvider(FUnrealTypeDefinitionInfo& InTypeDef)
	: Type(EHeaderProviderSourceType::TypeDef)
	, Id(InTypeDef.GetNameCPP())
	, TypeDef(&InTypeDef)
{
	check(TypeDef->HasSource());
}

FUnrealSourceFile* FHeaderProvider::Resolve(const FUnrealSourceFile& ParentSourceFile)
{
	if (!bResolved)
	{
		bResolved = true;

		switch (Type)
		{
		case EHeaderProviderSourceType::ClassName:
		{
			if (TSharedRef<FUnrealTypeDefinitionInfo>* Source = GTypeDefinitionInfoMap.FindByName(*Id))
			{
				Cache = (*Source)->HasSource() ? &(*Source)->GetUnrealSourceFile() : nullptr;
				// There is an edge case with interfaces.  If you define the UMyInterface and IMyInterface in the same
				// source file as a class that implements the interface, a HeaderProvider for IMyInterface is added 
				// at the pre-parse time that later (incorrectly) resolves to UMyInterface.  This results in
				// the include file thinking that it includes itself.
				if (Cache == &ParentSourceFile)
				{
					Cache = nullptr;
				}
			}
			break;
		}

		case EHeaderProviderSourceType::ScriptStructName:
		{
			if (!FUHTConfig::Get().StructsWithNoPrefix.Contains(Id))
			{
				if (TSharedRef<FUnrealTypeDefinitionInfo>* Source = GTypeDefinitionInfoMap.FindByName(*GetClassNameWithPrefixRemoved(Id)))
				{
					Cache = (*Source)->HasSource() ? &(*Source)->GetUnrealSourceFile() : nullptr;
				}
			}
			if (Cache == nullptr)
			{
				FName IdName(*Id, FNAME_Find);
				if (TSharedRef<FUnrealTypeDefinitionInfo>* Source = GTypeDefinitionInfoMap.FindByName(*Id))
				{
					Cache = (*Source)->HasSource() ? &(*Source)->GetUnrealSourceFile() : nullptr;
				}
			}
			break;
		}

		case EHeaderProviderSourceType::TypeDef:
		{
			Cache = &TypeDef->GetUnrealSourceFile();
			break;
		}

		case EHeaderProviderSourceType::FileName:
		{
			if (const TSharedRef<FUnrealSourceFile>* Source = GUnrealSourceFilesMap.Find(Id))
			{
				Cache = &Source->Get();
			}
			break;
		}

		default:
			check(false);
		}

		// There is questionable compatibility hack where a source file will always be exported
		// regardless of having types when it is being included by the SAME package.
		if (Cache && Cache->GetPackage() == ParentSourceFile.GetPackage())
		{
			Cache->MarkReferenced();
		}
	}

	return Cache;
}

FString FHeaderProvider::ToString() const
{
	switch (Type)
	{
	case EHeaderProviderSourceType::ClassName:
		return FString::Printf(TEXT("%s %s"), TEXT("class"), *Id);
	case EHeaderProviderSourceType::ScriptStructName:
		return FString::Printf(TEXT("%s %s"), TEXT("struct"), *Id);
	case EHeaderProviderSourceType::TypeDef:
		return FString::Printf(TEXT("%s %s"), TEXT("property type"), *Id);
	case EHeaderProviderSourceType::FileName:
		return FString::Printf(TEXT("%s %s"), TEXT("file"), *Id);
	default:
		check(false);
		return FString::Printf(TEXT("%s %s"), TEXT("unknown"), *Id);
	}
}

const FString& FHeaderProvider::GetId() const
{
	return Id;
}

bool operator==(const FHeaderProvider& A, const FHeaderProvider& B)
{
	return A.Type == B.Type && A.Id == B.Id && A.TypeDef == B.TypeDef;
}
