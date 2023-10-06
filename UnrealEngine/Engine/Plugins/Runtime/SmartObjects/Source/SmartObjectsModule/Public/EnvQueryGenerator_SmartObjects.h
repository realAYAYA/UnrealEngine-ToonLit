// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "SmartObjectSubsystem.h"
#include "EnvQueryGenerator_SmartObjects.generated.h"


/** Fetches Smart Object slots within QueryBoxExtent from locations given by QueryOriginContext, that match SmartObjectRequestFilter */
UCLASS(meta = (DisplayName = "Smart Objects"))
class SMARTOBJECTSMODULE_API UEnvQueryGenerator_SmartObjects : public UEnvQueryGenerator
{
	GENERATED_BODY()

public:
	UEnvQueryGenerator_SmartObjects();

protected:
	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;

	virtual FText GetDescriptionTitle() const override;
	virtual FText GetDescriptionDetails() const override;

	/** The context indicating the locations to be used as query origins */
	UPROPERTY(EditAnywhere, Category=Generator)
	TSubclassOf<UEnvQueryContext> QueryOriginContext;

	/** If set will be used to filter gathered results */
	UPROPERTY(EditAnywhere, Category=Generator)
	FSmartObjectRequestFilter SmartObjectRequestFilter;

	/** Combined with generator's origin(s) (as indicated by QueryOriginContext) determines query's volume */
	UPROPERTY(EditAnywhere, Category = Generator)
	FVector QueryBoxExtent;

	/** Determines whether only currently claimable slots are allowed */
	UPROPERTY(EditAnywhere, Category = Generator)
	bool bOnlyClaimable = true;
};
