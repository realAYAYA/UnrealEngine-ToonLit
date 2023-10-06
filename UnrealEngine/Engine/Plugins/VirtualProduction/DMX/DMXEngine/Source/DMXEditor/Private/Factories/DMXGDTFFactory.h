// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "EditorReimportHandler.h"
#include "UObject/StrongObjectPtr.h"
#include "DMXGDTFFactory.generated.h"

class UDMXGDTFImportUI;

UCLASS(hidecategories=Object)
class UDMXGDTFFactory
    : public UFactory
	, public FReimportHandler
{
	GENERATED_BODY()

public:
	UDMXGDTFFactory();

	/**  Set import batch **/
	void EnableShowOption() { bShowOption = true; }

	//~ Begin UObject Interface
	virtual void CleanUp() override;
	virtual bool ConfigureProperties() override;
	virtual void PostInitProperties() override;
	//~ End UObject Interface

	//~ Begin UFactory Interface
	virtual bool DoesSupportClass(UClass * Class) override;
	virtual UClass* ResolveSupportedClass() override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled);
	virtual bool FactoryCanImport(const FString& Filename) override;
	//~ End UFactory Interface

	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface

public:
	static const TCHAR* Extension;

private:
	bool bShowOption;

	/** true if the import operation was canceled. */
	bool bOperationCanceled;

	UPROPERTY(transient)
	TObjectPtr<UDMXGDTFImportUI> ImportUI;
};
