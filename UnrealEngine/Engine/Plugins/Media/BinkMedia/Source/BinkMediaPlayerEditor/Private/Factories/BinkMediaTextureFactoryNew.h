// Copyright Epic Games Tools LLC
//   Licenced under the Unreal Engine EULA 

#pragma once

#include "Factories/Factory.h"
#include "BinkMediaTextureFactoryNew.generated.h"

UCLASS(hidecategories=Object, MinimalAPI)
class UBinkMediaTextureFactoryNew : public UFactory 
{
	GENERATED_UCLASS_BODY()
public:
	UPROPERTY()
	TObjectPtr<class UBinkMediaPlayer> InitialMediaPlayer;

	virtual UObject* FactoryCreateNew( UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn ) override;
	virtual bool ShouldShowInNewMenu() const override 
	{ 
		return true; 
	}
};
