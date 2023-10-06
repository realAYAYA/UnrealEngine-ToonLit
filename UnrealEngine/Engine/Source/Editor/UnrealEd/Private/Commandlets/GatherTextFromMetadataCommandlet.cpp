// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/GatherTextFromMetadataCommandlet.h"

#include "Commandlets/Commandlet.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/InternationalizationManifest.h"
#include "Internationalization/LocKeyFuncs.h"
#include "LocTextHelper.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Misc/AssertionMacros.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "SourceCodeNavigation.h"
#include "Templates/Casts.h"
#include "Templates/SharedPointer.h"
#include "Trace/Detail/Channel.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

class UObject;

DEFINE_LOG_CATEGORY_STATIC(LogGatherTextFromMetaDataCommandlet, Log, All);

//////////////////////////////////////////////////////////////////////////
//GatherTextFromMetaDataCommandlet

UGatherTextFromMetaDataCommandlet::UGatherTextFromMetaDataCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UGatherTextFromMetaDataCommandlet::ShouldRunInPreview(const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals) const
{
	const FString* GatherType = ParamVals.Find(UGatherTextCommandletBase::GatherTypeParam);
	// If the param is not specified, it is assumed that both source and assets are to be gathered 
	return !GatherType || *GatherType == TEXT("Metadata") || *GatherType == TEXT("All");
}

int32 UGatherTextFromMetaDataCommandlet::Main( const FString& Params )
{
	// Parse command line - we're interested in the param vals
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	//Set config file
	const FString* ParamVal = ParamVals.Find(FString(TEXT("Config")));
	FString GatherTextConfigPath;
	
	if ( ParamVal )
	{
		GatherTextConfigPath = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromMetaDataCommandlet, Error, TEXT("No config specified."));
		return -1;
	}

	//Set config section
	ParamVal = ParamVals.Find(FString(TEXT("Section")));
	FString SectionName;

	if ( ParamVal )
	{
		SectionName = *ParamVal;
	}
	else
	{
		UE_LOG(LogGatherTextFromMetaDataCommandlet, Error, TEXT("No config section specified."));
		return -1;
	}

	//Modules to Preload
	TArray<FString> ModulesToPreload;
	GetStringArrayFromConfig(*SectionName, TEXT("ModulesToPreload"), ModulesToPreload, GatherTextConfigPath);

	for (const FString& ModuleName : ModulesToPreload)
	{
		FModuleManager::Get().LoadModule(*ModuleName);
	}

	// IncludePathFilters
	TArray<FString> IncludePathFilters;
	GetPathArrayFromConfig(*SectionName, TEXT("IncludePathFilters"), IncludePathFilters, GatherTextConfigPath);

	// IncludePaths (DEPRECATED)
	{
		TArray<FString> IncludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("IncludePaths"), IncludePaths, GatherTextConfigPath);
		if (IncludePaths.Num())
		{
			IncludePathFilters.Append(IncludePaths);
			UE_LOG(LogGatherTextFromMetaDataCommandlet, Warning, TEXT("IncludePaths detected in section %s. IncludePaths is deprecated, please use IncludePathFilters."), *SectionName);
		}
	}

	if (IncludePathFilters.Num() == 0)
	{
		UE_LOG(LogGatherTextFromMetaDataCommandlet, Error, TEXT("No include path filters in section %s."), *SectionName);
		return -1;
	}

	// ExcludePathFilters
	TArray<FString> ExcludePathFilters;
	GetPathArrayFromConfig(*SectionName, TEXT("ExcludePathFilters"), ExcludePathFilters, GatherTextConfigPath);

	// ExcludePaths (DEPRECATED)
	{
		TArray<FString> ExcludePaths;
		GetPathArrayFromConfig(*SectionName, TEXT("ExcludePaths"), ExcludePaths, GatherTextConfigPath);
		if (ExcludePaths.Num())
		{
			ExcludePathFilters.Append(ExcludePaths);
			UE_LOG(LogGatherTextFromMetaDataCommandlet, Warning, TEXT("ExcludePaths detected in section %s. ExcludePaths is deprecated, please use ExcludePathFilters."), *SectionName);
		}
	}

	FGatherTextDelegates::GetAdditionalGatherPaths.Broadcast(GatherManifestHelper->GetTargetName(), IncludePathFilters, ExcludePathFilters);

	// Get whether we should gather editor-only data. Typically only useful for the localization of UE itself.
	if (!GetBoolFromConfig(*SectionName, TEXT("ShouldGatherFromEditorOnlyData"), ShouldGatherFromEditorOnlyData, GatherTextConfigPath))
	{
		ShouldGatherFromEditorOnlyData = false;
	}

	// FieldTypesToInclude/FieldTypesToExclude
	{
		auto GetFieldTypesArrayFromConfig = [this, &SectionName, &GatherTextConfigPath](const TCHAR* InConfigKey, TArray<FFieldClassFilter>& OutFieldTypes)
		{
			TArray<FString> FieldTypeStrs;
			GetStringArrayFromConfig(*SectionName, InConfigKey, FieldTypeStrs, GatherTextConfigPath);

			if (FieldTypeStrs.Num() == 0)
			{
				return;
			}

			TArray<FFieldClassFilter> AllFieldTypes;
			// FField types
			{
				for (const FFieldClass* FieldClass : FFieldClass::GetAllFieldClasses())
				{
					AllFieldTypes.Emplace(FieldClass);
				}
			}
			// UField types
			{
				TArray<UClass*> AllUFieldClasses;
				AllUFieldClasses.Add(UField::StaticClass());
				GetDerivedClasses(UField::StaticClass(), AllUFieldClasses);
				for (const UClass* FieldClass : AllUFieldClasses)
				{
					AllFieldTypes.Emplace(FieldClass);
				}
			}

			for (const FString& FieldTypeStr : FieldTypeStrs)
			{
				const bool bIsWildcard = FieldTypeStr.GetCharArray().Contains(TEXT('*')) || FieldTypeStr.GetCharArray().Contains(TEXT('?'));
				if (bIsWildcard)
				{
					for (const FFieldClassFilter& FieldTypeFilter : AllFieldTypes)
					{
						if (FieldTypeFilter.GetName().MatchesWildcard(FieldTypeStr))
						{
							OutFieldTypes.Emplace(FieldTypeFilter);
						}
					}
				}
				else
				{
					const FFieldClass* FieldClass = FFieldClass::GetNameToFieldClassMap().FindRef(*FieldTypeStr);
					const UClass* UFieldClass = FindFirstObject<UClass>(*FieldTypeStr, EFindFirstObjectOptions::None, ELogVerbosity::Warning, TEXT("Looking for field types to include or exclude in GatherTextFromMetadata commandlet"));
					if (!FieldClass && !UFieldClass)
					{
						UE_LOG(LogGatherTextFromMetaDataCommandlet, Warning, TEXT("Field Type %s was not found (from %s in section %s). Did you forget a ModulesToPreload entry?"), *FieldTypeStr, InConfigKey, *SectionName);
						continue;
					}

					if (FieldClass)
					{
						OutFieldTypes.Emplace(FieldClass);
					}

					if (UFieldClass)
					{
						check(UFieldClass->IsChildOf<UField>());
						OutFieldTypes.Emplace(UFieldClass);
					}
				}
			}
		};

		GetFieldTypesArrayFromConfig(TEXT("FieldTypesToInclude"), FieldTypesToInclude);
		GetFieldTypesArrayFromConfig(TEXT("FieldTypesToExclude"), FieldTypesToExclude);
	}

	// FieldOwnerTypesToInclude/FieldOwnerTypesToExclude
	{
		auto GetFieldOwnerTypesArrayFromConfig = [this, &SectionName, &GatherTextConfigPath](const TCHAR* InConfigKey, TArray<const UStruct*>& OutFieldOwnerTypes)
		{
			TArray<FString> FieldOwnerTypeStrs;
			GetStringArrayFromConfig(*SectionName, InConfigKey, FieldOwnerTypeStrs, GatherTextConfigPath);

			TArray<const UStruct*> AllFieldOwnerClassTypes;
			TArray<const UStruct*> AllFieldOwnerScriptStructTypes;
			GetObjectsOfClass(UClass::StaticClass(), (TArray<UObject*>&)AllFieldOwnerClassTypes, false);
			GetObjectsOfClass(UScriptStruct::StaticClass(), (TArray<UObject*>&)AllFieldOwnerScriptStructTypes, false);

			for (const FString& FieldOwnerTypeStr : FieldOwnerTypeStrs)
			{
				const bool bIsWildcard = FieldOwnerTypeStr.GetCharArray().Contains(TEXT('*')) || FieldOwnerTypeStr.GetCharArray().Contains(TEXT('?'));
				if (bIsWildcard)
				{
					auto AddFieldOwnersMatchingWildcard = [&FieldOwnerTypeStr, &OutFieldOwnerTypes](const TArray<const UStruct*>& AllFieldOwnerTypes)
					{
						for (const UStruct* FieldOwnerType : AllFieldOwnerTypes)
						{
							if (FieldOwnerType->GetName().MatchesWildcard(FieldOwnerTypeStr))
							{
								OutFieldOwnerTypes.Add(FieldOwnerType);
							}
						}
					};
					AddFieldOwnersMatchingWildcard(AllFieldOwnerClassTypes);
					AddFieldOwnersMatchingWildcard(AllFieldOwnerScriptStructTypes);
				}
				else
				{
					const UStruct* FieldOwnerType = FindFirstObject<UStruct>(*FieldOwnerTypeStr, EFindFirstObjectOptions::EnsureIfAmbiguous);
					if (!FieldOwnerType)
					{
						UE_LOG(LogGatherTextFromMetaDataCommandlet, Warning, TEXT("Field Owner Type %s was not found (from %s in section %s). Did you forget a ModulesToPreload entry?"), *FieldOwnerTypeStr, InConfigKey, *SectionName);
						continue;
					}

					OutFieldOwnerTypes.Add(FieldOwnerType);
					if (const UClass* FieldOwnerClass = Cast<UClass>(FieldOwnerType))
					{
						GetDerivedClasses(FieldOwnerClass, (TArray<UClass*>&)OutFieldOwnerTypes);
					}
					if (FieldOwnerType == UScriptStruct::StaticClass())
					{
						// Structs don't have a catch-all base, so we allow ScriptStruct to mean "all struct types"
						OutFieldOwnerTypes.Append(AllFieldOwnerScriptStructTypes);
					}
				}
			}
		};

		GetFieldOwnerTypesArrayFromConfig(TEXT("FieldOwnerTypesToInclude"), FieldOwnerTypesToInclude);
		GetFieldOwnerTypesArrayFromConfig(TEXT("FieldOwnerTypesToExclude"), FieldOwnerTypesToExclude);
	}

	FGatherParameters Arguments;
	GetStringArrayFromConfig(*SectionName, TEXT("InputKeys"), Arguments.InputKeys, GatherTextConfigPath);
	GetStringArrayFromConfig(*SectionName, TEXT("OutputNamespaces"), Arguments.OutputNamespaces, GatherTextConfigPath);
	GetStringArrayFromConfig(*SectionName, TEXT("OutputKeys"), Arguments.OutputKeys, GatherTextConfigPath);

	// Execute gather.
	GatherTextFromUObjects(IncludePathFilters, ExcludePathFilters, Arguments);

	// Add any manifest dependencies if they were provided
	TArray<FString> ManifestDependenciesList;
	GetPathArrayFromConfig(*SectionName, TEXT("ManifestDependencies"), ManifestDependenciesList, GatherTextConfigPath);

	for (const FString& ManifestDependency : ManifestDependenciesList)
	{
		FText OutError;
		if (!GatherManifestHelper->AddDependency(ManifestDependency, &OutError))
		{
			UE_LOG(LogGatherTextFromMetaDataCommandlet, Error, TEXT("The GatherTextFromMetaData commandlet couldn't load the specified manifest dependency: '%s'. %s"), *ManifestDependency, *OutError.ToString());
			return -1;
		}
	}

	return 0;
}

void UGatherTextFromMetaDataCommandlet::GatherTextFromUObjects(const TArray<FString>& IncludePaths, const TArray<FString>& ExcludePaths, const FGatherParameters& Arguments)
{
	const FFuzzyPathMatcher FuzzyPathMatcher = FFuzzyPathMatcher(IncludePaths, ExcludePaths);

	for (TObjectIterator<UField> It; It; ++It)
	{
		UField* Field = *It;

		const UPackage* FieldPackage = Field->GetOutermost();
		const FString FieldPackageName = FieldPackage->GetName();
		if (!FPackageName::IsScriptPackage(FieldPackageName) || FPackageName::IsMemoryPackage(FieldPackageName) || FPackageName::IsTempPackage(FieldPackageName))
		{
			continue;
		}

		FString SourceFilePath;
		if (!FSourceCodeNavigation::FindClassHeaderPath(Field, SourceFilePath))
		{
			continue;
		}
		SourceFilePath = FPaths::ConvertRelativePathToFull(SourceFilePath);
		check(!SourceFilePath.IsEmpty());

		const FFuzzyPathMatcher::EPathMatch PathMatch = FuzzyPathMatcher.TestPath(SourceFilePath);
		if (PathMatch != FFuzzyPathMatcher::EPathMatch::Included)
		{
			continue;
		}

		const FName MetaDataPlatformName = GetSplitPlatformNameFromPath(SourceFilePath);
		GatherTextFromField(Field, Arguments, MetaDataPlatformName);
	}
}

void UGatherTextFromMetaDataCommandlet::GatherTextFromField(UField* Field, const FGatherParameters& Arguments, const FName InPlatformName)
{
	// For structs, also gather the new non-object field values.
	if (UStruct* Struct = Cast<UStruct>(Field))
	{
		for (TFieldIterator<FField> FieldIt(Struct, EFieldIterationFlags::None); FieldIt; ++FieldIt)
		{
			GatherTextFromField(*FieldIt, Arguments, InPlatformName);
		}
	}

	if (ShouldGatherFromField(Field, false))
	{
		// Gather for object.
		{
			EnsureFieldDisplayNameImpl(Field, false);
			GatherTextFromFieldImpl(Field, Arguments, InPlatformName);
		}

		// For enums, also gather for enum values.
		if (UEnum* Enum = Cast<UEnum>(Field))
		{
			const int32 ValueCount = Enum->NumEnums();
			for (int32 i = 0; i < ValueCount; ++i)
			{
				if (!Enum->HasMetaData(TEXT("DisplayName"), i))
				{
					Enum->SetMetaData(TEXT("DisplayName"), *FName::NameToDisplayString(Enum->GetNameStringByIndex(i), false), i);
				}

				for (int32 j = 0; j < Arguments.InputKeys.Num(); ++j)
				{
					FStringFormatNamedArguments PatternArguments;
					PatternArguments.Add(TEXT("FieldPath"), Enum->GetFullGroupName(false) + TEXT(".") + Enum->GetNameStringByIndex(i));

					if (Enum->HasMetaData(*Arguments.InputKeys[j], i))
					{
						const FString& MetaDataValue = Enum->GetMetaData(*Arguments.InputKeys[j], i);
						if (!MetaDataValue.IsEmpty())
						{
							PatternArguments.Add(TEXT("MetaDataValue"), MetaDataValue);

							const FString Namespace = Arguments.OutputNamespaces[j];
							FLocItem LocItem(MetaDataValue);
							FManifestContext Context;
							Context.Key = FString::Format(*Arguments.OutputKeys[j], PatternArguments);
							Context.SourceLocation = FString::Printf(TEXT("Meta-data for key %s of enum value %s of enum %s in %s"), *Arguments.InputKeys[j], *Enum->GetNameStringByIndex(i), *Enum->GetName(), *Enum->GetFullGroupName(true));
							Context.PlatformName = InPlatformName;
							GatherManifestHelper->AddSourceText(Namespace, LocItem, Context);
						}
					}
				}
			}
		}
	}
}

void UGatherTextFromMetaDataCommandlet::GatherTextFromField(FField* Field, const FGatherParameters& Arguments, const FName InPlatformName)
{
	FProperty* Property = CastField<FProperty>(Field);
	if (ShouldGatherFromField(Field, Property && Property->HasAnyPropertyFlags(CPF_EditorOnly)))
	{
		EnsureFieldDisplayNameImpl(Field, Field->IsA(FBoolProperty::StaticClass()));
		GatherTextFromFieldImpl(Field, Arguments, InPlatformName);
	}
}

template <typename FieldType>
bool UGatherTextFromMetaDataCommandlet::ShouldGatherFromField(const FieldType* Field, const bool bIsEditorOnly)
{
	auto ShouldGatherFieldByType = [this, Field]()
	{
		if (FieldTypesToInclude.Num() == 0 && FieldTypesToExclude.Num() == 0)
		{
			return true;
		}

		const auto* FieldClass = Field->GetClass();
		auto TestClassFilter = [FieldClass](const TArray<FFieldClassFilter>& InFieldTypeFilters)
		{
			for (const FFieldClassFilter& FieldTypeFilter : InFieldTypeFilters)
			{
				if (FieldTypeFilter.TestClass(FieldClass))
				{
					return true;
				}
			}
			return false;
		};
		
		return (FieldTypesToInclude.Num() == 0 || TestClassFilter(FieldTypesToInclude))
			&& (FieldTypesToExclude.Num() == 0 || !TestClassFilter(FieldTypesToExclude));
	};

	auto ShouldGatherFieldByOwnerType = [this, Field]()
	{
		if (FieldOwnerTypesToInclude.Num() == 0 && FieldOwnerTypesToExclude.Num() == 0)
		{
			return true;
		}

		const UStruct* FieldOwnerType = Field->GetOwnerStruct();
		if (FieldOwnerType)
		{
			// Only properties and functions will have an owner struct type
			return (FieldOwnerTypesToInclude.Num() == 0 || FieldOwnerTypesToInclude.Contains(FieldOwnerType))
				&& (FieldOwnerTypesToExclude.Num() == 0 || !FieldOwnerTypesToExclude.Contains(FieldOwnerType));
		}

		return true;
	};

	return (!bIsEditorOnly || ShouldGatherFromEditorOnlyData) && ShouldGatherFieldByType() && ShouldGatherFieldByOwnerType();
}

template <typename FieldType>
void UGatherTextFromMetaDataCommandlet::GatherTextFromFieldImpl(FieldType* Field, const FGatherParameters& Arguments, const FName InPlatformName)
{
	for (int32 i = 0; i < Arguments.InputKeys.Num(); ++i)
	{
		FStringFormatNamedArguments PatternArguments;
		PatternArguments.Add(TEXT("FieldPath"), Field->GetFullGroupName(false));

		if (Field->HasMetaData(*Arguments.InputKeys[i]))
		{
			const FString& MetaDataValue = Field->GetMetaData(*Arguments.InputKeys[i]);
			if (!MetaDataValue.IsEmpty())
			{
				PatternArguments.Add(TEXT("MetaDataValue"), MetaDataValue);

				const UStruct* FieldOwnerType = Field->GetOwnerStruct();
				const FString Namespace = Arguments.OutputNamespaces[i];
				FLocItem LocItem(MetaDataValue);
				FManifestContext Context;
				Context.Key = FString::Format(*Arguments.OutputKeys[i], PatternArguments);
				Context.SourceLocation = FString::Printf(TEXT("Meta-data for key %s of member %s in %s (type: %s, owner: %s)"), *Arguments.InputKeys[i], *Field->GetName(), *Field->GetFullGroupName(true), *Field->GetClass()->GetName(), FieldOwnerType ? *FieldOwnerType->GetName() : TEXT("<null>"));
				Context.PlatformName = InPlatformName;
				GatherManifestHelper->AddSourceText(Namespace, LocItem, Context);
			}
		}
	}
}

template <typename FieldType>
void UGatherTextFromMetaDataCommandlet::EnsureFieldDisplayNameImpl(FieldType* Field, const bool bIsBool)
{
	if (!Field->HasMetaData(TEXT("DisplayName")))
	{
		Field->SetMetaData(TEXT("DisplayName"), *FName::NameToDisplayString(Field->GetName(), bIsBool));
	}
}
