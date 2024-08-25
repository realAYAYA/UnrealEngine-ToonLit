// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

struct FQuixelAccountMetaHumanEntry
{
	FString Name; // Character name
	FString Id; // Quixel ID
	bool bIsLegacy; // Does this MetaHuman require an Upgrade before it can be used
	FString Version; // The version of MHC used to create this character
};

struct FMetaHumanAssetImportDescription
{
	inline static const FString DefaultDestinationPath = TEXT("/Game");

	FString CharacterPath; // The file path to the source unique assets for this import operation
	FString CommonPath; // The file path to the source common assets for this import operation
	FString CharacterName; // The name of the MetaHuman to import (expected to match the final part of CharacterPath)
	FString QuixelId; // The ID of the character being imported
	bool bIsBatchImport; // If this is part of a batch import
	FString SourcePath = DefaultDestinationPath; // The asset path that the exporter has written the assets out to
	FString DestinationPath = DefaultDestinationPath; // The asset path to install the MetaHuman to in the project
	TArray<FQuixelAccountMetaHumanEntry> AccountMetaHumans; // All the MetaHumans that are included in the user's account. Used to show which MetaHumans can be upgraded
	bool bForceUpdate = false; // Ignore asset version metadata and update all assets
	bool bWarnOnQualityChange = false; // Warn if the user is importing a MetaHuman at a different quality level to the existing MetaHuman in the scene.
};

class IMetaHumanProjectUtilitiesAutomationHandler
{
public:
	virtual ~IMetaHumanProjectUtilitiesAutomationHandler()
	{
	};
	virtual bool ShouldContinueWithBreakingMetaHumans(const TArray<FString>&, const TArray<FString>& UpdatedFiles) = 0;
};

class IMetaHumanBulkImportHandler
{
public:
	virtual ~IMetaHumanBulkImportHandler()
	{
	};

	// MetaHumanIds is a list of the Quixel IDs of the MetaHumans to
	// be imported. This is an asynchronous operation. This function returns
	// immediately and the import operation that called it will immediately terminate.
	virtual void DoBulkImport(const TArray<FString>& MetaHumanIds) = 0;
};

class FMetaHumanProjectUtilities
{
public:
	// Disable UI and enable automation of user input for headless testing
	static void METAHUMANPROJECTUTILITIES_API EnableAutomation(IMetaHumanProjectUtilitiesAutomationHandler* Handler);
	// Disable UI and enable automation of user input for headless testing
	static void METAHUMANPROJECTUTILITIES_API SetBulkImportHandler(IMetaHumanBulkImportHandler* Handler);
	// Main entry-point used by Quixel Bridge
	static void METAHUMANPROJECTUTILITIES_API ImportAsset(const FMetaHumanAssetImportDescription& AssetImportDescription);
	// Provide the Url for the versioning service to use
	static void METAHUMANPROJECTUTILITIES_API OverrideVersionServiceUrl(const FString& BaseUrl);
};
