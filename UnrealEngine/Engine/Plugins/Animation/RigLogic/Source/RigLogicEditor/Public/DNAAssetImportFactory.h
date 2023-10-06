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
*	Also extends ReimportHandler for importing DNA file with the same name as SkeletalMesh
 */
UCLASS(transient)
class UDNAAssetImportFactory: public UFactory, public FReimportHandler
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

	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override
	{
		return SetReimportPaths(Obj, NewReimportPaths[0], 0);
	}
	virtual void SetReimportPaths(UObject* Obj, const FString& NewReimportPath, const int32 SourceIndex) override {};

	virtual EReimportResult::Type Reimport(UObject* Obj) override
	{
		return Reimport(Obj, INDEX_NONE);
	}
	virtual EReimportResult::Type Reimport(UObject* Obj, int32 SourceFileIndex);
	virtual int32 GetPriority() const override;
	virtual void PostImportCleanUp() { CleanUp(); }
	//~ End FReimportHandler Interface

	/** UFactory interface */
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual void CleanUp() override {};
};
