// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "DMXControlConsoleFactory.generated.h"

class UDMXControlConsole;
class UDMXControlConsoleData;


/** Factory for DMX Control Console */
UCLASS()
class UDMXControlConsoleFactory
	: public UFactory
{
	GENERATED_BODY()

public:
	/** Constructor */
	UDMXControlConsoleFactory();

	//~ Begin UFactory Interface
	virtual FText GetDisplayName() const override { return FText::FromString(TEXT("DMX Control Console")); }
	virtual FName GetNewAssetThumbnailOverride() const override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual uint32 GetMenuCategories() const override;
	//~ Begin UFactory Interface	

	/**
	 * Creates a new Control Console asset from provided source control console in desired package name.
	 *
	 * @param SavePackagePath				The package path
	 * @param SaveAssetName					The desired asset name
	 * @param SourceControlConsole			The control console copied into the new console
	 * @return								The newly created console, or nullptr if no console could be created
	 */
	UDMXControlConsole* CreateConsoleAssetFromData(const FString& SavePackagePath, const FString& SaveAssetName, UDMXControlConsoleData* SourceConsoleData) const;
};
