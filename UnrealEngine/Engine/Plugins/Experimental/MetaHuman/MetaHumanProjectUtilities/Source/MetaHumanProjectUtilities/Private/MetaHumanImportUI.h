// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FSourceMetaHuman;
class FInstalledMetaHuman;
struct FAssetOperationPaths;
enum class EQualityLevel: int;

enum class EImportOperationUserResponse: int
{
	OK,
	Cancel,
	BulkImport
};

/** Display a warning dialog informing the user that upgrade may impact upon incompatible MetaHumans in the project
 * @param SourceMetaHuman The MetaHuman being imported
 * @param IncompatibleCharacters MetaHumans in the project that are incompatible with the proposed import
 * @param InstalledMetaHumans All MetaHumans installed in the project
 */
EImportOperationUserResponse DisplayUpgradeWarning(const FSourceMetaHuman& SourceMetaHuman, const TSet<FString>& IncompatibleCharacters, const TArray<FInstalledMetaHuman>& InstalledMetaHumans, const TSet<FString>& AvailableMetaHumans, const FAssetOperationPaths& AssetOperations);

bool DisplayQualityLevelChangeWarning(EQualityLevel Source, EQualityLevel Target);
