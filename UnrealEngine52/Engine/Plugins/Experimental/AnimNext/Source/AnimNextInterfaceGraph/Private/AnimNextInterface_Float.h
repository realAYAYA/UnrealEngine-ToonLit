// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextInterfaceTypes.h"
#include "IAnimNextInterface.h"
#include "AnimNextInterfaceContext.h"
#include "AnimNextInterface_Float.generated.h"

UCLASS()
class UAnimNextInterfaceFloat : public UObject, public IAnimNextInterface
{
	GENERATED_BODY()

	virtual FName GetReturnTypeNameImpl() const final override
	{
		static const FName Name = TNameOf<float>::GetName();
		return Name;
	}

	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const override
	{
		checkf(false, TEXT("UAnimNextInterfaceFloat::GetDataImpl must be overridden"));
		return false;
	}
};

UCLASS()
class UAnimNextInterface_Float_Literal : public UAnimNextInterfaceFloat
{
	GENERATED_BODY()

	// IAnimNextInterface interface
	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const final override
	{
		Context.SetResult(Value);
		return true;
	}
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	float Value;
};

UCLASS()
class UAnimNextInterface_Float_Multiply : public UAnimNextInterfaceFloat
{
	GENERATED_BODY()

	// IAnimNextInterface interface
	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const final override;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TArray<TScriptInterface<IAnimNextInterface>> Inputs;
};

UCLASS()
class UAnimNextInterface_Float_InterpTo : public UAnimNextInterfaceFloat
{
	GENERATED_BODY()

	// IAnimNextInterface interface
	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const final override;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IAnimNextInterface> Current;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IAnimNextInterface> Target;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IAnimNextInterface> Speed;
};

UCLASS()
class UAnimNextInterface_Float_DeltaTime : public UAnimNextInterfaceFloat
{
	GENERATED_BODY()

	// IAnimNextInterface interface
	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const final override
	{
		Context.SetResult(Context.GetDeltaTime());
		return true;
	}
};

UCLASS()
class UAnimNextInterface_Float_SpringInterp : public UAnimNextInterfaceFloat
{
	GENERATED_BODY()

	// IAnimNextInterface interface
	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const final override;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IAnimNextInterface> TargetValue;

	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IAnimNextInterface> TargetValueRate;
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IAnimNextInterface> SmoothingTime;
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TScriptInterface<IAnimNextInterface> DampingRatio;
};