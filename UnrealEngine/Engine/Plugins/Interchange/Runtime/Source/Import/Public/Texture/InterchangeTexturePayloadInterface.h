// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "InterchangeSourceData.h"
#include "Texture/InterchangeTexturePayloadData.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeTexturePayloadInterface.generated.h"

UINTERFACE()
class INTERCHANGEIMPORT_API UInterchangeTexturePayloadInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * Texture payload interface. Derive from it if your payload can import texture
 */
class INTERCHANGEIMPORT_API IInterchangeTexturePayloadInterface
{
	GENERATED_BODY()
public:

	/**
	 * Once the translation is done, the import process need a way to retrieve payload data.
	 * This payload will be use by the factories to create the asset.
	 *
	 * @param PayloadSourceData - The source data containing the data to translate
	 * @param PayloadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return a PayloadData containing the import image data. The TOptional will not be set if there is an error.
	 */
	virtual TOptional<UE::Interchange::FImportImage> GetTexturePayloadData(const UInterchangeSourceData* PayloadSourceData, const FString& PayloadKey) const = 0;

	/**
	 * Does this translator support the request for a compressed source data?
	 */
	virtual bool SupportCompressedTexturePayloadData() const { return false; }

	/**
	 * Once the translation is done, the import process need a way to retrieve payload data.
	 * This payload will be use by the factories to create the asset.
	 *
	 * @param PayloadSourceData - The source data containing the data to translate
	 * @param PayloadKey - The key to retrieve the a particular payload contain into the specified source data.
	 * @return a PayloadData containing the import image data with image data being the original file format generally. The TOptional will not be set if there is an error.
	 */
	virtual TOptional<UE::Interchange::FImportImage> GetCompressedTexturePayloadData(const UInterchangeSourceData* PayloadSourceData, const FString& PayloadKey) const { return {}; }
};


