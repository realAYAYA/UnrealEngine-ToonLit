// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/TextProperty.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "KismetStringTableLibrary.generated.h"

UCLASS(meta=(BlueprintThreadSafe, ScriptName="StringTableLibrary"), MinimalAPI)
class UKismetStringTableLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	/** Returns true if the given table ID corresponds to a registered string table. */
	UFUNCTION(BlueprintPure, Category="Utilities|String Table", meta=(DisplayName="Is String Table Registered"))
	static ENGINE_API bool IsRegisteredTableId(const FName TableId);

	/** Returns true if the given table ID corresponds to a registered string table, and that table has. */
	UFUNCTION(BlueprintPure, Category="Utilities|String Table", meta=(DisplayName="Is String Table Entry Registered"))
	static ENGINE_API bool IsRegisteredTableEntry(const FName TableId, const FString& Key);

	/** Returns the namespace of the given string table. */
	UFUNCTION(BlueprintPure, Category="Utilities|String Table", meta=(DisplayName="Get String Table Namespace"))
	static ENGINE_API FString GetTableNamespace(const FName TableId);

	/** Returns the source string of the given string table entry (or an empty string). */
	UFUNCTION(BlueprintPure, Category="Utilities|String Table", meta=(DisplayName="Get String Table Entry Source String"))
	static ENGINE_API FString GetTableEntrySourceString(const FName TableId, const FString& Key);

	/** Returns the specified meta-data of the given string table entry (or an empty string). */
	UFUNCTION(BlueprintPure, Category="Utilities|String Table", meta=(DisplayName="Get String Table Entry Meta-Data"))
	static ENGINE_API FString GetTableEntryMetaData(const FName TableId, const FString& Key, const FName MetaDataId);

	/** Returns an array of all registered string table IDs */
	UFUNCTION(BlueprintPure, Category="Utilities|String Table", meta=(DisplayName="Get Registered String Tables"))
	static ENGINE_API TArray<FName> GetRegisteredStringTables();

	/** Returns an array of all keys within the given string table */
	UFUNCTION(BlueprintPure, Category="Utilities|String Table", meta=(DisplayName="Get Keys from String Table"))
	static ENGINE_API TArray<FString> GetKeysFromStringTable(const FName TableId);

	/** Returns an array of all meta-data IDs within the given string table entry */
	UFUNCTION(BlueprintPure, Category="Utilities|String Table", meta=(DisplayName="Get Meta-Data IDs from String Table Entry"))
	static ENGINE_API TArray<FName> GetMetaDataIdsFromStringTableEntry(const FName TableId, const FString& Key);
};
