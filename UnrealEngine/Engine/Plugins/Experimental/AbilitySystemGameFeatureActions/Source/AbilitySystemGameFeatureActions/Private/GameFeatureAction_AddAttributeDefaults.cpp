// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameFeatureAction_AddAttributeDefaults.h"
#include "AbilitySystemGlobals.h"
#include "HAL/IConsoleManager.h"
#include "Misc/PackageName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(GameFeatureAction_AddAttributeDefaults)

#define LOCTEXT_NAMESPACE "GameFeatures"

namespace GameFeatureAction_AddAttributeDefaults
{
	static TAutoConsoleVariable<bool> CVarAllowRemoveAttributeDefaultTables(TEXT("GameFeatureAction_AddAttributeDefaults.AllowRemoveAttributeDefaultTables"), true, TEXT("Removes hard references when unregistering"));
}

//////////////////////////////////////////////////////////////////////
// UGameFeatureAction_AddAttributeDefaults

void UGameFeatureAction_AddAttributeDefaults::OnGameFeatureRegistering()
{
	const TArray<FSoftObjectPath>* AttribDefaultTableNamesToAdd = &AttribDefaultTableNames;

#if WITH_EDITOR
	// Do a file exists check in editor builds since some folks do not sync all data in the editor. Ideally we don't need to load anything at GFD registration time, but for now we will do this.
	TArray<FSoftObjectPath> AttribDefaultTableNamesThatExist;
	AttribDefaultTableNamesToAdd = &AttribDefaultTableNamesThatExist;
	for (const FSoftObjectPath& Path : AttribDefaultTableNames)
	{
		if (FPackageName::DoesPackageExist(Path.GetLongPackageName()))
		{
			AttribDefaultTableNamesThatExist.Add(Path);
		}
	}
#endif // WITH_EDITOR

	if (!AttribDefaultTableNamesToAdd->IsEmpty())
	{
		FNameBuilder OwnerNameBuilder;
		GetPathName(nullptr, OwnerNameBuilder);
		AttributeDefaultTablesOwnerName = FName(OwnerNameBuilder.ToView());

		UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();
		AbilitySystemGlobals.AddAttributeDefaultTables(AttributeDefaultTablesOwnerName, *AttribDefaultTableNamesToAdd);
	}
}

void UGameFeatureAction_AddAttributeDefaults::OnGameFeatureUnregistering()
{
	if (!AttribDefaultTableNames.IsEmpty() && GameFeatureAction_AddAttributeDefaults::CVarAllowRemoveAttributeDefaultTables.GetValueOnAnyThread())
	{
		UAbilitySystemGlobals& AbilitySystemGlobals = UAbilitySystemGlobals::Get();
		AbilitySystemGlobals.RemoveAttributeDefaultTables(AttributeDefaultTablesOwnerName, AttribDefaultTableNames);
	}
}

//////////////////////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

