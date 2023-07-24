// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OptimusDataType.h"
#include "OptimusValueContainer.generated.h"

UCLASS()
class OPTIMUSCORE_API UOptimusValueContainerGeneratorClass : public UClass
{
public:
	GENERATED_BODY()
	
	// DECLARE_WITHIN(UObject) is only kept for back-compat, please don't parent the class
	// to the asset object.
	// This class should be parented to the package, instead of the asset object
	// because the engine no longer asset object as UClass outer
	// however, since in the past we have parented the class to the asset object
	// this flag has to be kept such that we can load the old asset in the first place and
	// re-parent it back to the package in post load
	DECLARE_WITHIN(UObject)

	static FName ValuePropertyName;
	
	// UClass overrides
	void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	
	static UClass *GetClassForType(
		UPackage*InPackage,
		FOptimusDataTypeRef InDataType
		);

	static UClass *RefreshClassForType(
		UPackage*InPackage,
		FOptimusDataTypeRef InDataType
		);

	UPROPERTY()
	FOptimusDataTypeRef DataType;
};

UCLASS()
class OPTIMUSCORE_API UOptimusValueContainer : public UObject
{
public:
	GENERATED_BODY()

	void PostLoad() override;
	
	static UOptimusValueContainer* MakeValueContainer(UObject* InOwner, FOptimusDataTypeRef InDataTypeRef);

	FOptimusDataTypeRef GetValueType() const;
	FShaderValueType::FValue GetShaderValue() const;
};