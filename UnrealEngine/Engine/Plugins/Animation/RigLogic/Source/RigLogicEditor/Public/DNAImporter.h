// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Misc/SecureHash.h"
#include "DNAAssetImportUI.h"
#include "Logging/TokenizedMessage.h"
#include "MeshBuild.h"

class USkeletalMesh;

struct FDNAAssetImportOptions
{
	// General options
	bool bCanShowDialog;	
	// Skeletal Mesh options
	USkeletalMesh* SkeletalMesh;

	static void ResetOptions(FDNAAssetImportOptions *OptionsToReset)
	{
		check(OptionsToReset != nullptr);
		*OptionsToReset = FDNAAssetImportOptions();
	}
};

FDNAAssetImportOptions* GetImportOptions(class FDNAImporter* DNAImporter, UDNAAssetImportUI* ImportUI, bool bShowOptionDialog, bool bIsAutomated, const FString& FullPath, bool& OutOperationCanceled, const FString& InFilename);
void ApplyImportUIToImportOptions(UDNAAssetImportUI* ImportUI, FDNAAssetImportOptions& InOutImportOptions);

class FDNAImporter
{
public:
	~FDNAImporter();
	/**
	 * Returns the importer singleton. It will be created on the first request.
	 */
	static FDNAImporter* GetInstance();
	static void DeleteInstance();

	FDNAAssetImportOptions* ImportOptions;
	/**
	 * Get the object of import options
	 *
	 * @return FDNAAssetImportOptions
	 */
	FDNAAssetImportOptions* GetImportOptions() const;
	void PartialCleanUp();
	FString GetDNAFileVersion() { return DNAFileVersion; }
	FString GetDNAFileCreator() { return DNAFileCreator; }
protected:
	enum IMPORTPHASE
	{
		NOTSTARTED,
		FILEOPENED,
		IMPORTED,
		FIXEDANDCONVERTED,
	};

	static TSharedPtr<FDNAImporter> StaticInstance;
	static TSharedPtr<FDNAImporter> StaticPreviewInstance;

	// scene management
	IMPORTPHASE CurPhase;
	FString ErrorMessage;
	// base path of DNA file
	FString FileBasePath;
	TWeakObjectPtr<UObject> Parent;
	FString DNAFileVersion;
	FString DNAFileCreator;

	FDNAImporter();

	/**
	 * Clean up to reset the import.
	 */
	void CleanUp();
};