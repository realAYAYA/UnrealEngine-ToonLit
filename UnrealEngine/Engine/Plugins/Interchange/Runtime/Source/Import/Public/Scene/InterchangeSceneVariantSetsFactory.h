// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeFactoryBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "Scene/InterchangeVariantSetPayloadInterface.h"

#include "InterchangeSceneVariantSetsFactory.generated.h"

class ULevelVariantSets;
class UVariantObjectBinding;

UCLASS(BlueprintType, Experimental)
class INTERCHANGEIMPORT_API UInterchangeSceneVariantSetsFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()
public:

	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;
	virtual FImportAssetResult BeginImportAsset_GameThread(const FImportAssetObjectParams& Arguments) override;
	
	virtual void SetupObject_GameThread(const FSetupObjectParams& Arguments) override;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////

private:
	UObject* ImportObjectSourceData(const FImportAssetObjectParams& Arguments, ULevelVariantSets* LevelVariantSets);
	const UInterchangeTranslatorBase* Translator = nullptr;
};


