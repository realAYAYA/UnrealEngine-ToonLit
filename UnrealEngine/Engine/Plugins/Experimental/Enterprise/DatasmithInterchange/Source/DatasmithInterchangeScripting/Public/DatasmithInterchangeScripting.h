// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithImportOptions.h"

#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "InterchangeManager.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "DatasmithInterchangeScripting.generated.h"

class AActor;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDatasmithInterchangeImportEnded);

USTRUCT(BlueprintType, Experimental)
struct DATASMITHINTERCHANGESCRIPTING_API FDatasmithInterchangeImportContext
{
	GENERATED_BODY()
	
	FDatasmithInterchangeImportContext();

	/** Path of content folder where to store created assets */
	UPROPERTY(BlueprintReadWrite, Category = "Datasmith | Interchange")
	FString AssetPath;

	/** If not null, the resulting ADatasmithSceneActor will be parented to the Anchor */
	UPROPERTY(BlueprintReadWrite, Category = "Datasmith | Interchange")
	TObjectPtr<USceneComponent> Anchor;

	/** If true, the import will be asynchronous */
	UPROPERTY(BlueprintReadWrite, Category = "Datasmith | Interchange")
	bool bAsync = true;

	/**  */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "StaticMesh Options", meta = (ShowOnlyInnerProperties))
	FDatasmithStaticMeshImportOptions StaticMeshOptions;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import Options", meta = (ShowOnlyInnerProperties))
	TArray<TObjectPtr<UObject>> ImportOptions;
};

UCLASS(BlueprintType, Experimental)
class DATASMITHINTERCHANGESCRIPTING_API UDatasmithInterchangeImportResult : public UObject
{
	GENERATED_BODY()

	using FImportResultPtr = TSharedPtr< UE::Interchange::FImportResult, ESPMode::ThreadSafe >;

public:
	UDatasmithInterchangeImportResult() {}
	~UDatasmithInterchangeImportResult();

	bool Build(const FString& InFilePath, FImportResultPtr InAssetResult, FImportResultPtr InSceneResult, bool bAsync, USceneComponent* Anchor);

	bool IsValid() { return bIsValid; }

	/** Dynamic delegate used to trigger an event in the Game when the import is completed */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Datasmith | Interchange")
	TArray<TObjectPtr<UObject>> ImportedObjects;

	/** Dynamic delegate used to trigger an event in the Game when the import is completed */
	UPROPERTY(VisibleAnywhere, BlueprintAssignable, Category = "Datasmith | Interchange")
	FDatasmithInterchangeImportEnded OnImportEnded;

private:
	void OnImportComplete(USceneComponent* Anchor, bool bAsync);

private:
	FString FilePath;
	FImportResultPtr AssetResult;
	FImportResultPtr SceneResult;
	bool bIsValid = false;
};

UCLASS(MinimalAPI, Experimental, meta = (ScriptName = "DatasmithInterchangeScripting"))
class UDatasmithInterchangeScripting : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Utility method to load a specific file into the current level
	 * @param FilePath  : Path to the file to load.
	 * @param AssetPath	: .
	 * @param bAsync    : 
	 * @param Anchor    : 
	 */
	UFUNCTION(BlueprintCallable, Category = "Datasmith | Interchange")
	static UDatasmithInterchangeImportResult* LoadFile(const FString& FilePath, const FDatasmithInterchangeImportContext& Context);

	/**
	 * Fetch the typed options associated with this file
	 * @param FilePath	The path to the file to get the option for
	 * @param OptionType	Type of the option structure to get
	 * @return The option structure found
	 */
	UFUNCTION(BlueprintCallable, Category = "Datasmith | Interchange")
	void GetDatasmithOptionsForFile(const FString& FilePath, TArray<UObject*>& Options);

	/**
	 * Utility method to get list of supported extensions and string to pass to IDesktopPlatform::OpenFileDialog
	 * @param Extensions : ';' separated list of extensions supported by Datasmith thru Interchange
	 * @param FileTypes	 : The type filters to show in the dialog when using @see IDesktopPlatform::OpenFileDialog.
	 */
	UFUNCTION(BlueprintCallable, Category = "Datasmith | Interchange")
	static void GetDatasmithFormats(FString& Extensions, FString& FileTypes);
};