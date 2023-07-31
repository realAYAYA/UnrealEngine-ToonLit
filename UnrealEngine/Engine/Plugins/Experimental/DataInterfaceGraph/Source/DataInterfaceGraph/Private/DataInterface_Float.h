// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataInterfaceTypes.h"
#include "IDataInterface.h"
#include "DataInterfaceContext.h"
#include "DataInterface_Float.generated.h"

UCLASS()
class UDataInterfaceFloat : public UObject, public IDataInterface
{
	GENERATED_BODY()

	virtual FName GetReturnTypeNameImpl() const final override
	{
		static const FName Name = TNameOf<float>::GetName();
		return Name;
	}

	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const override
	{
		checkf(false, TEXT("UDataInterfaceFloat::GetDataImpl must be overridden"));
		return false;
	}
};

UCLASS()
class UDataInterface_Float_Literal : public UDataInterfaceFloat
{
	GENERATED_BODY()

	// IDataInterface interface
	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override
	{
		Context.SetResult(Value);
		return true;
	}
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	float Value;
};

UCLASS()
class UDataInterface_Float_Multiply : public UDataInterfaceFloat
{
	GENERATED_BODY()

	// IDataInterface interface
	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<TScriptInterface<IDataInterface>> Inputs;
};

UCLASS()
class UDataInterface_Float_InterpTo : public UDataInterfaceFloat
{
	GENERATED_BODY()

	// IDataInterface interface
	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IDataInterface> Current;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IDataInterface> Target;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IDataInterface> Speed;
};

UCLASS()
class UDataInterface_Float_DeltaTime : public UDataInterfaceFloat
{
	GENERATED_BODY()

	// IDataInterface interface
	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override
	{
		Context.SetResult(Context.GetDeltaTime());
		return true;
	}
};

UCLASS()
class UDataInterface_Float_SpringInterp : public UDataInterfaceFloat
{
	GENERATED_BODY()

	// IDataInterface interface
	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IDataInterface> TargetValue;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IDataInterface> TargetValueRate;
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IDataInterface> SmoothingTime;
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IDataInterface> DampingRatio;
};