// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IMetaHumanProjectUtilitiesAutomationHandler;
class IMetaHumanBulkImportHandler;
struct FMetaHumanAssetImportDescription;
/**
 * 
 */
class METAHUMANPROJECTUTILITIES_API FMetaHumanImport
{
public:
	void ImportAsset(const FMetaHumanAssetImportDescription& AssetImportDescription);
	void SetAutomationHandler(IMetaHumanProjectUtilitiesAutomationHandler* Handler);
	void SetBulkImportHandler(IMetaHumanBulkImportHandler* Handler);

	static TSharedPtr<FMetaHumanImport> Get();

private:
	FMetaHumanImport() = default;
	IMetaHumanProjectUtilitiesAutomationHandler* AutomationHandler{nullptr};
	IMetaHumanBulkImportHandler* BulkImportHandler{nullptr};
	static TSharedPtr<FMetaHumanImport> MetaHumanImportInst;
};
