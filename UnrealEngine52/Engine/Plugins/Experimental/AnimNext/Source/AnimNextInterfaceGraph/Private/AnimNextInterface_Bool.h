// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextInterfaceTypes.h"
#include "IAnimNextInterface.h"
#include "AnimNextInterfaceContext.h"
#include "AnimNextInterface_Bool.generated.h"

UCLASS()
class UAnimNextInterface_Bool_Literal : public UObject, public IAnimNextInterface
{
	GENERATED_BODY()
	ANIM_NEXT_INTERFACE_RETURN_TYPE(bool)

	// IAnimNextInterface interface
	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const final override
	{
		Context.SetResult(Value);
		return true;
	}
	
	UPROPERTY(EditAnywhere, Category = "Parameters")
	bool Value;
};

UCLASS()
class UAnimNextInterface_Bool_And : public UObject, public IAnimNextInterface
{
	GENERATED_BODY()
	ANIM_NEXT_INTERFACE_RETURN_TYPE(bool)

	// IAnimNextInterface interface
	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const final override;

	UPROPERTY(EditAnywhere, Meta=(AnimNextInterfaceType="bool"), Category = "Parameters")
	TArray<TScriptInterface<IAnimNextInterface>> Inputs;
};


UCLASS()
class UAnimNextInterface_Bool_Not : public UObject, public IAnimNextInterface
{
	GENERATED_BODY()
	ANIM_NEXT_INTERFACE_RETURN_TYPE(bool)

	// IAnimNextInterface interface
	virtual bool GetDataImpl(const UE::AnimNext::Interface::FContext& Context) const final override;

	UPROPERTY(EditAnywhere, Meta=(AnimNextInterfaceType="bool"), Category = "Parameters")
	TScriptInterface<IAnimNextInterface> Input;
};