// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EditorReimportHandler.h"
#include "NeuralNetworkFactory.h"
#include "NeuralNetworkReimportFactory.generated.h"

UCLASS(MinimalAPI, collapsecategories)
class UNeuralNetworkReimportFactory : public UNeuralNetworkFactory, public FReimportHandler
{
	GENERATED_BODY()

public:
	UNeuralNetworkReimportFactory();
	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;
	virtual void PostImportCleanUp() { CleanUp(); }
	//~ End FReimportHandler Interface

	//~ Begin UFactory Interface
	virtual bool CanCreateNew() const override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual bool IsAutomatedImport() const override;
	//~ End UFactory Interface
};
