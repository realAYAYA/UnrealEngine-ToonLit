// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDataInterface.h"
#include "DataInterfaceContext.h"
#include "DataInterface_Name.generated.h"

UCLASS()
class UDataInterface_Name_Literal : public UObject, public IDataInterface
{
	GENERATED_BODY()

	virtual FName GetReturnTypeNameImpl() const final override
	{
		return NAME_Name;
	}

	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override
	{
		Context.SetResult(Value);
		return true;
	}
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	FName Value;
};
