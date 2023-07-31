// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"

#include "EditorReimportHandler.h"

#include "DMXLibraryFromMVRFactory.generated.h"

class FDMXZipper;
class UDMXImportGDTF;
class UDMXLibrary;
class UDMXMVRAssetImportData;
class UDMXMVRGeneralSceneDescription;


UCLASS()
class DMXEDITOR_API UDMXLibraryFromMVRFactory 
	: public UFactory
	, public FReimportHandler
{
	GENERATED_BODY()

public:
	UDMXLibraryFromMVRFactory();

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* Parent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled);
	virtual bool FactoryCanImport(const FString& Filename) override;
	//~ End UFactory Interface	

	/** File extention for MVR files */
	static const FString MVRFileExtension;

protected:
	//~ Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface

private:
	/** Creates a DMX Library asset. Returns nullptr if the library could not be created */
	UDMXLibrary* CreateDMXLibraryAsset(UObject* Parent, const FName& Name, EObjectFlags Flags, const FString& InFilename);

	/** Creates a General Scene Description from the MVR zip. Returns the General Scene Description or nullptr if not a valid MVR zip. */
	UDMXMVRGeneralSceneDescription* CreateGeneralSsceneDescription(UDMXLibrary& DMXLibrary, const TSharedRef<FDMXZipper>& Zip) const;

	/** Creates GDTF assets from the MVR */
	TArray<UDMXImportGDTF*> CreateGDTFAssets(UObject* Parent, EObjectFlags Flags, const TSharedRef<FDMXZipper>& Zip, const UDMXMVRGeneralSceneDescription& GeneralSceneDescription);

	/** Initializes the DMX Library from the General Scene Description and GDTF assets. Updates the library in case it already contains patches */
	void InitDMXLibrary(UDMXLibrary* DMXLibrary, const TArray<UDMXImportGDTF*>& GDTFAssets, UDMXMVRGeneralSceneDescription* GeneralSceneDescription) const;

	/** Returns the MVR Asset Import Data for the Object, or nullptr if it cannot be obtained */
	UDMXMVRAssetImportData* GetMVRAssetImportData(UObject* DMXLibraryObject) const;
};
