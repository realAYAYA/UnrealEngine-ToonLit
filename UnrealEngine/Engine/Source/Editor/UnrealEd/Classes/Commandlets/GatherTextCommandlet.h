// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/GatherTextCommandletBase.h"
#include "GatherTextCommandlet.generated.h"

namespace EOutputJson
{
	enum Format { Manifest, Archive };
}

/**
 *	UGatherTextCommandlet: One commandlet to rule them all. This commandlet loads a config file and then calls other localization commandlets. Allows localization system to be easily extendable and flexible. 
 */
UCLASS()
class UGatherTextCommandlet : public UGatherTextCommandletBase
{
    GENERATED_UCLASS_BODY()
public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

	int32 ProcessGatherConfig(const FString& GatherTextConfigPath, const TSharedPtr<FLocalizationSCC>& CommandletSourceControlInfo, const TArray<FString>& Tokens, const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals);

	// Helpler function to generate a changelist description
	FText GetChangelistDescription(const TArray<FString>& GatherTextConfigPaths);

	static const FString UsageText;
//~ Begin UGatherTextCommandletBase  Interface
	virtual bool ShouldRunInPreview(const TArray<FString>& Switches, const TMap<FString, FString>& ParamVals) const override
	{
		// This commandlet is the driver for other commandlet. This should always run even in preview
		return true;
	}
	//~ End UGatherTextCommandletBase  Interface
private:
	 
};
