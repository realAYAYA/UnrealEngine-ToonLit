// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMXTypes.h"

#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"

#include "DMXGDTFAssetImportData.generated.h"

class UDMXImportGDTF;


UCLASS()
class DMXRUNTIME_API UDMXGDTFAssetImportData 
	: public UAssetImportData
{
	GENERATED_BODY()

public:
	/** Returns the imported file path and name */
	FORCEINLINE const FString& GetFilePathAndName() const { return FilePathAndName; };

#if WITH_EDITOR
	/** Creates new asset GDTF Asset Import Data from the File. Returns the GDTF Asset Import Data or nullptr if not a valid GDTF File */
	void SetSourceFile(const FString& InFilePathAndName);

	/** Returns the source data the asset was generated from */
	FORCEINLINE const TArray64<uint8>& GetRawSourceData() const { return RawSourceData.ByteArray; }
#endif // WITH_EDITOR 

protected:
	//~ Begin UObject interface
	virtual void PostLoad() override;
	//~ End UObject interface

private:
	/** The imported file path and name */
	UPROPERTY()
	FString FilePathAndName;

#if WITH_EDITORONLY_DATA
	/** The raw GDTF zip file as byte array */
	UPROPERTY()
	FDMXByteArray64 RawSourceData;
#endif // WITH_EDITORONLY_DATA
};
