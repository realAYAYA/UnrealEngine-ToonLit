// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAnimNextInterface.h"
#include "AnimNextInterfaceContext.h"
#include "AnimNextInterface_Name.generated.h"

UCLASS()
class UAnimNextInterface_Name_Literal : public UObject, public IAnimNextInterface
{
	GENERATED_BODY()

	virtual FName GetReturnTypeNameImpl() const final override
	{
		return NAME_Name;
	}

	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const final override
	{
		Context.SetResult(Value);
		return true;
	}
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	FName Value;
};
