// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
//#include "EditorFramework/AssetImportData.h"
#include "InterchangeFactoryBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeDatasmithSceneFactory.generated.h"


UCLASS(BlueprintType, Experimental)
class DATASMITHINTERCHANGE_API UInterchangeDatasmithSceneFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()
public:

	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;
	virtual UObject* CreateEmptyAsset(const FCreateAssetParams& Arguments) override;
	virtual UObject* CreateAsset(const FCreateAssetParams& Arguments) override;
	virtual void PreImportPreCompletedCallback(const FImportPreCompletedCallbackParams& Arguments) override;
	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////

private:
	//UE::Interchange::Private::InterchangeTextureFactory::FProcessedPayload ProcessedPayload;

#if WITH_EDITORONLY_DATA
	// When importing a UDIM the data for the source files will be stored here
	//TArray<FAssetImportInfo::FSourceFile> SourceFiles;
#endif // WITH_EDITORONLY_DATA
};