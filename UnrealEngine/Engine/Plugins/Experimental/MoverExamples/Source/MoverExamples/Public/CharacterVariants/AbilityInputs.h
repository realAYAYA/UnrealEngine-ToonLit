// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoverTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AbilityInputs.generated.h"

// Data block containing extended ability inputs used by MoverExamples characters
USTRUCT(BlueprintType)
struct MOVEREXAMPLES_API FMoverExampleAbilityInputs : public FMoverDataStructBase
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	bool bIsDashJustPressed = false;

	UPROPERTY(BlueprintReadWrite, Category = Mover)
    bool bIsAimPressed = false;

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	bool bIsVaultJustPressed = false;

	UPROPERTY(BlueprintReadWrite, Category = Mover)
	bool bWantsToStartZiplining = false;


	// @return newly allocated copy of this FMoverExampleAbilityInputs. Must be overridden by child classes
	virtual FMoverDataStructBase* Clone() const override
	{
		// TODO: ensure that this memory allocation jives with deletion method
		FMoverExampleAbilityInputs* CopyPtr = new FMoverExampleAbilityInputs(*this);
		return CopyPtr;
	}

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override
	{
		Super::NetSerialize(Ar, Map, bOutSuccess);

		Ar.SerializeBits(&bIsDashJustPressed, 1);
		Ar.SerializeBits(&bIsAimPressed, 1);
		Ar.SerializeBits(&bIsVaultJustPressed, 1);
		Ar.SerializeBits(&bWantsToStartZiplining, 1);

		bOutSuccess = true;
		return true;
	}

	virtual UScriptStruct* GetScriptStruct() const override { return StaticStruct(); }

	virtual void ToString(FAnsiStringBuilderBase& Out) const override
	{
		Super::ToString(Out);
		Out.Appendf("bIsDashJustPressed: %i\n", bIsDashJustPressed);
		Out.Appendf("bIsAimPressed: %i\n", bIsAimPressed);
		Out.Appendf("bIsVaultJustPressed: %i\n", bIsVaultJustPressed);
		Out.Appendf("bWantsToStartZiplining: %i\n", bWantsToStartZiplining);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override { Super::AddReferencedObjects(Collector); }
};

UCLASS()
class MOVEREXAMPLES_API UMoverExampleAbilityInputsLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Mover|Input")
	static FMoverExampleAbilityInputs GetMoverExampleAbilityInputs(const FMoverDataCollection& FromCollection)
	{
		if (const FMoverExampleAbilityInputs* FoundInputs = FromCollection.FindDataByType<FMoverExampleAbilityInputs>())
		{
			return *FoundInputs;
		}

		return FMoverExampleAbilityInputs();
	}
};