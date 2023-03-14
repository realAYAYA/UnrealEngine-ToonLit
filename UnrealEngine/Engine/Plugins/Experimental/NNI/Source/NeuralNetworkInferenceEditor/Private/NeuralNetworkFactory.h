// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "NeuralNetworkFactory.generated.h"

UCLASS(hidecategories=Object)
class UNeuralNetworkFactory : public UFactory
{
	GENERATED_BODY()

public:
	UNeuralNetworkFactory();
	//~ Begin UFactory Interface
	// UI creation (AssetTypeActions)
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, UObject* InContext, FFeedbackContext* InWarn) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags InFlags, const FString& InFilename,
		const TCHAR* Params, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual bool CanCreateNew() const override;
	// Dragging and importing ONNX files into the Content Browser
	virtual bool DoesSupportClass(UClass * Class) override;
	virtual UClass* ResolveSupportedClass() override;
	virtual bool FactoryCanImport(const FString& InFilename) override;
	virtual bool CanImportBeCanceled() const override;

protected:
	virtual bool IsValidFile(const FString& InFilename) const final;
};
