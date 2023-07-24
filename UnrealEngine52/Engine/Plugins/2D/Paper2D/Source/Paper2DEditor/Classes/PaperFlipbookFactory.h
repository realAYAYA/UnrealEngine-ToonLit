// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookFactory.generated.h"

/**
 * Factory for flipbooks
 */

UCLASS()
class PAPER2DEDITOR_API UPaperFlipbookFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

	TArray<FPaperFlipbookKeyFrame> KeyFrames;

	// UFactory interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	// End of UFactory interface
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#endif
