// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Factories/Factory.h"
#include "Factories/ImportSettings.h"
#include "EditorReimportHandler.h"
#include "DNAAsset.h"
#include "Engine/SkeletalMesh.h"
#include "DNAAssetImportFactory.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDNAImportFactory, Log, All);

/** Factory responsible for importing DNA file and attaching DNA data into SkeletalMesh
 */
UCLASS(transient)
class UDNAAssetImportFactory: public UFactory
{ 
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY()
		TObjectPtr<class UDNAAssetImportUI> ImportUI;

	/** Prevent garbage collection of original when overriding ImportUI property */
	UPROPERTY()
		TObjectPtr<class UDNAAssetImportUI> OriginalImportUI;

	/** UObject properties */
	virtual void PostInitProperties() override;

	/** UFactory interface */
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual void CleanUp() override;
};
