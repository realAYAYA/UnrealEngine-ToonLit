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
	inline static const FString DefaultDestination = TEXT("/Game");

	FString CharacterPath; // The file path to the source unique assets for this import operation
	FString CommonPath; // The file path to the source common assets for this import operation
	FString CharacterName; // The name of the MetaHuman to import (expected to match the final part of CharacterPath)
	FString QuixelId; // The ID of the character being imported
	bool bIsBatchImport; // If this is part of a batch import
	FString DestinationPath = DefaultDestination; // The asset path to install the MetaHuman to in the project
	TArray<FQuixelAccountMetaHumanEntry> AccountMetaHumans; // All the MetaHumans that are included in the user's account. Used to show which MetaHumans can be upgraded
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
	static void MEGASCANSPLUGIN_API EnableAutomation(IMetaHumanProjectUtilitiesAutomationHandler* Handler);
	// Disable UI and enable automation of user input for headless testing
	static void MEGASCANSPLUGIN_API SetBulkImportHandler(IMetaHumanBulkImportHandler* Handler);
	// Main entry-point used by Quixel Bridge
	static void MEGASCANSPLUGIN_API ImportAsset(const FMetaHumanAssetImportDescription& AssetImportDescription);
};
