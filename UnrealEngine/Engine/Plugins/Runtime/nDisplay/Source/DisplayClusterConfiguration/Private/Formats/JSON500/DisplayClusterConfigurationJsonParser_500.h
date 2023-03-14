// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Formats/IDisplayClusterConfigurationDataParser.h"
#include "Formats/JSON500/DisplayClusterConfigurationJsonTypes_500.h"

class UDisplayClusterConfigurationData;


namespace JSON500
{
	/**
	 * Config parser for JSON based config files
	 */
	class FDisplayClusterConfigurationJsonParser
		: public IDisplayClusterConfigurationDataParser
	{
	public:
		FDisplayClusterConfigurationJsonParser()  = default;
		~FDisplayClusterConfigurationJsonParser() = default;

	public:
		// Load data from a specified file
		virtual UDisplayClusterConfigurationData* LoadData(const FString& FilePath, UObject* Owner = nullptr) override;

		// Save data to a specified file
		virtual bool SaveData(const UDisplayClusterConfigurationData* ConfigData, const FString& FilePath) override;

		// Convert configuration to string
		virtual bool AsString(const UDisplayClusterConfigurationData* ConfigData, FString& OutString) override;

	protected:
		// [import data] Fill generic data container with parsed information
		UDisplayClusterConfigurationData* ConvertDataToInternalTypes();
		// [export data] Extract data from generic container to specific containers
		bool ConvertDataToExternalTypes(const UDisplayClusterConfigurationData* InData);

	private:
		// Intermediate temporary data (Json types)
		FDisplayClusterConfigurationJsonContainer_500 JsonData;
		// Source file
		FString ConfigFile;
		// Owner for config data UObject we'll create on success
		UObject* ConfigDataOwner = nullptr;
	};
}
