// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDataInterface.h"
#include "DataInterface_Wrapper.generated.h"

UCLASS()
class UDataInterface_Wrapper : public UObject, public IDataInterface
{
	GENERATED_BODY()

private:
	// IAnimDataInterface interface
	virtual FName GetReturnTypeNameImpl() const final override;
	virtual const UScriptStruct* GetReturnTypeStructImpl() const final override;
	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override;

//	UPROPERTY(EditAnywhere, Category = "Parameters")
//	TArray<FDataInterfaceParameter> Inputs;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IDataInterface> Output;
};