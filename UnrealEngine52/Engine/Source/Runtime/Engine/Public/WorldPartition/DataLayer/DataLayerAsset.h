// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"

#include "WorldPartition/DataLayer/DataLayerType.h" 

#include "DataLayerAsset.generated.h"

UCLASS(BlueprintType, editinlinenew)
class ENGINE_API UDataLayerAsset : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UDataLayerConversionInfo;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface

public:
	void SetType(EDataLayerType Type) { DataLayerType = Type; }
	void SetDebugColor(FColor InDebugColor) { DebugColor = InDebugColor; }
#endif

	UFUNCTION(Category = "Data Layer", BlueprintCallable)
	EDataLayerType GetType() const { return DataLayerType; }

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	bool IsRuntime() const { return DataLayerType == EDataLayerType::Runtime; }

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	FColor GetDebugColor() const { return DebugColor; }

private:
	/** Whether the Data Layer affects actor runtime loading */
	UPROPERTY(Category = "Data Layer", EditAnywhere)
	EDataLayerType DataLayerType;

	UPROPERTY(Category = "Data Layer|Runtime", EditAnywhere)
	FColor DebugColor;
};