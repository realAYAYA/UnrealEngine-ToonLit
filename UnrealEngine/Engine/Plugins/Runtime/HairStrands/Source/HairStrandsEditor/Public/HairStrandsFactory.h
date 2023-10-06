// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "UObject/ObjectMacros.h"

#include "HairStrandsFactory.generated.h"

class IGroomTranslator;

/**
 * Implements a factory for UHairStrands objects.
 */
UCLASS(hidecategories = Object)
class UHairStrandsFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

public:

	//~ UFactory Interface
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
		const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual void GetSupportedFileExtensions(TArray<FString>& OutExtensions) const override;

protected:
	void InitTranslators();

	TSharedPtr<IGroomTranslator> GetTranslator(const FString& Filename);

	class UGroomImportOptions* ImportOptions;
	class UGroomCacheImportOptions* GroomCacheImportOptions;

private:
	TArray<TSharedPtr<IGroomTranslator>> Translators;
};

