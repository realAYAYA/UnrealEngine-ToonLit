// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "InputMappingContext.h"

#include "VCamObjectWithInputFactory.generated.h"

UCLASS(hidecategories = Object)
class VCAMCOREEDITOR_API UVCamObjectWithInputFactory : public UFactory
{
	GENERATED_BODY()
public:	
	// The parent class of the created blueprint
	UPROPERTY(EditAnywhere, Category="BlueprintVirtualSubjectFactory", meta=(AllowAbstract = "", BlueprintBaseOnly = ""))
	TSubclassOf<UObject> ParentClass;

	UPROPERTY(BlueprintReadWrite, Category = "VCam Modifier Factory")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(BlueprintReadWrite, Category = "VCam Modifier Factory")
	int32 MappingPriority;

	// Begin UFactory Interface
	virtual bool ConfigureProperties() override;
	// End UFactory Interface
};