// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterfaceTypes.h"
#include "IDataInterface.h"
#include "DataInterfaceContext.h"
#include "DataInterface_Bool.generated.h"

UCLASS()
class UDataInterface_Bool_Literal : public UObject, public IDataInterface
{
	GENERATED_BODY()
	DATA_INTERFACE_RETURN_TYPE(bool)

	// IDataInterface interface
	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override
	{
		Context.SetResult(Value);
		return true;
	}
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	bool Value;
};

UCLASS()
class UDataInterface_Bool_And : public UObject, public IDataInterface
{
	GENERATED_BODY()
	DATA_INTERFACE_RETURN_TYPE(bool)

	// IDataInterface interface
	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override;

	UPROPERTY(EditAnywhere, Meta=(DataInterfaceType="bool"), Category = "Parameters")
	TArray<TScriptInterface<IDataInterface>> Inputs;
};


UCLASS()
class UDataInterface_Bool_Not : public UObject, public IDataInterface
{
	GENERATED_BODY()
	DATA_INTERFACE_RETURN_TYPE(bool)

	// IDataInterface interface
	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override;

	UPROPERTY(EditAnywhere, Meta=(DataInterfaceType="bool"), Category = "Parameters")
	TScriptInterface<IDataInterface> Input;
};