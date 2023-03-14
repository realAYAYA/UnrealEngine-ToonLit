// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * DNA Asset Importer UI options.
 */

#pragma once

#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Factories/ImportSettings.h"
#include "DNAAssetImportUI.generated.h"

UCLASS(config = EditorPerProjectUserSettings, AutoExpandCategories = (Mesh), MinimalAPI)
class UDNAAssetImportUI : public UObject, public IImportSettingsParser
{
	GENERATED_UCLASS_BODY()

public:

	/** Skeletal mesh to use for imported DNA asset. When importing DNA, leaving this as "None" will generate new skeletal mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mesh, meta = (ImportType = "DNAAsset"))
		TObjectPtr<class USkeletalMesh> SkeletalMesh;

	UFUNCTION(BlueprintCallable, Category = Miscellaneous)
		void ResetToDefault();

	/** UObject Interface */
	virtual bool CanEditChange(const FProperty* InProperty) const override;

	/** IImportSettings Interface */
	virtual void ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson) override;

	/* Whether this UI is construct for a reimport */
	bool bIsReimport;

	/* When we are reimporting, we need the current object to preview skeletal mesh match issues. */
	UObject* ReimportMesh;


	//////////////////////////////////////////////////////////////////////////
		// DNA Asset file informations
		// Transient value that are set everytime we show the options dialog. These are information only and should be string.

	/* The DNA file version */
	UPROPERTY(VisibleAnywhere, Transient, Category = DNAFileInformation, meta = (ImportType = "DNAAsset", DisplayName = "File Version"))
		FString FileVersion;

	/* The file creator information */
	UPROPERTY(VisibleAnywhere, Transient, Category = DNAFileInformation, meta = (ImportType = "DNAAsset", DisplayName = "File Creator"))
		FString FileCreator;
};
