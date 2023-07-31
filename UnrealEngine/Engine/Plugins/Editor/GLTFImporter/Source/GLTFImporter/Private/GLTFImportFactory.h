// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "GLTFImportFactory.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGLTF, Log, All);

class IGLTFImporterModule;

UCLASS(transient)
class UGLTFImportFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

public:
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename,
	                                   const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

	virtual void CleanUp() override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual TArray<FString> GetFormats() const override;

private:
	void UpdateMeshes() const;

private:
	IGLTFImporterModule* GLTFImporterModule;
};
