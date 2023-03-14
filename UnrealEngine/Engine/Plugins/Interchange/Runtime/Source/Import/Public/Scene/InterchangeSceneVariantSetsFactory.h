// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InterchangeFactoryBase.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "Scene/InterchangeVariantSetPayloadInterface.h"

#include "InterchangeSceneVariantSetsFactory.generated.h"

class UVariantObjectBinding;

UCLASS(BlueprintType, Experimental)
class INTERCHANGEIMPORT_API UInterchangeSceneVariantSetsFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()
public:

	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;
	virtual UObject* CreateEmptyAsset(const FCreateAssetParams& Arguments) override;
	virtual UObject* CreateAsset(const FCreateAssetParams& Arguments) override;
	virtual void PreImportPreCompletedCallback(const FImportPreCompletedCallbackParams& Arguments) override;
	virtual bool CanExecuteOnAnyThread() const override
	{
		//Currently we cannot use the anim sequence controller outside of the game thread
		return false;
	}

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////

private:
	const UInterchangeTranslatorBase* Translator = nullptr;
};


