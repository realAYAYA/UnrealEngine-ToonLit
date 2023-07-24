// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commandlets/Commandlet.h"
#include "CryptoKeysCommandlet.generated.h"

/**
* Commandlet used to configure project encryption settings
*/
UCLASS()
class CRYPTOKEYS_API UCryptoKeysCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

		//~ Begin UCommandlet Interface
		virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
