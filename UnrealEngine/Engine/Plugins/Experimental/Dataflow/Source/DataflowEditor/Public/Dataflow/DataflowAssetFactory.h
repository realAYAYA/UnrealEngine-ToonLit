// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "DataflowAssetFactory.generated.h"

UCLASS()
class UDataflowAssetFactory : public UFactory
{
	GENERATED_BODY()
public:

	UDataflowAssetFactory();

	/** UFactory Interface */
	bool     CanCreateNew() const override;
	bool     FactoryCanImport(const FString& Filename) override;
	UObject* FactoryCreateNew(UClass*           InClass,
							  UObject*          InParent,
							  FName             InName,
							  EObjectFlags      Flags,
							  UObject*          Context,
							  FFeedbackContext* Warn) override;

	bool ShouldShowInNewMenu() const override;
	bool ConfigureProperties() override;
	/** End UFactory Interface */

private:
};
