// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "InterchangeFactoryBase.h"
#include "Misc/TVariant.h"
#include "Serialization/EditorBulkData.h"
#include "Texture/InterchangeBlockedTexturePayloadData.h"
#include "Texture/InterchangeSlicedTexturePayloadData.h"
#include "Texture/InterchangeTextureLightProfilePayloadData.h"
#include "Texture/InterchangeTextureLightProfilePayloadData.h"
#include "Texture/InterchangeTexturePayloadData.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeTextureFactory.generated.h"

class UInterchangeTextureFactoryNode;

namespace UE::Interchange::Private::InterchangeTextureFactory
{
	using FTexturePayloadVariant = TVariant<FEmptyVariantState
		, TOptional<FImportImage>
		, TOptional<FImportBlockedImage>
		, TOptional<FImportSlicedImage>
		, TOptional<FImportLightProfile>>;

	struct FProcessedPayload
	{
		FProcessedPayload() = default;
		FProcessedPayload(FProcessedPayload&&) = default;
		FProcessedPayload& operator=(FProcessedPayload&&) = default;

		FProcessedPayload(const FProcessedPayload&) = delete;
		FProcessedPayload& operator=(const FProcessedPayload&) = delete;

		FProcessedPayload& operator=(UE::Interchange::Private::InterchangeTextureFactory::FTexturePayloadVariant&& InPayloadVariant);

		bool IsValid() const;

		UE::Interchange::Private::InterchangeTextureFactory::FTexturePayloadVariant SettingsFromPayload;
		UE::Serialization::FEditorBulkData::FSharedBufferWithID PayloadAndId;
	};
}

UCLASS(BlueprintType, Experimental)
class INTERCHANGEIMPORT_API UInterchangeTextureFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()
public:

	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;
	virtual EInterchangeFactoryAssetType GetFactoryAssetType() override { return EInterchangeFactoryAssetType::Textures; }
	virtual UObject* CreateEmptyAsset(const FCreateAssetParams& Arguments) override;
	virtual UObject* CreateAsset(const FCreateAssetParams& Arguments) override;
	virtual void PreImportPreCompletedCallback(const FImportPreCompletedCallbackParams& Arguments) override;
	virtual bool GetSourceFilenames(const UObject* Object, TArray<FString>& OutSourceFilenames) const override;
	virtual bool SetSourceFilename(const UObject* Object, const FString& SourceFilename, int32 SourceIndex) const override;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////

private:
	/**
	 * Check for any invalid resolutions and remove those from the payload
	 */
	void CheckForInvalidResolutions(UE::Interchange::Private::InterchangeTextureFactory::FTexturePayloadVariant& InPayloadVariant, const UInterchangeSourceData* SourceData, const UInterchangeTextureFactoryNode* TextureFactoryNode);
	
	UE::Interchange::Private::InterchangeTextureFactory::FProcessedPayload ProcessedPayload;

#if WITH_EDITORONLY_DATA
	//  The data for the source files will be stored here during the import
	TArray<FAssetImportInfo::FSourceFile> SourceFiles;
#endif // WITH_EDITORONLY_DATA
};


