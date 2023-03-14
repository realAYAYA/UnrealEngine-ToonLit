// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "Curves/RealCurve.h"
#include "CurveTableFactory.generated.h"

class UCurveTable;

/* Creates a CurveTable with Rich Curves */

UCLASS(hidecategories=Object)
class UNREALED_API UCurveTableFactory : public UFactory
{
	GENERATED_UCLASS_BODY()

  public:
	// UFactory Interface
	virtual bool ConfigureProperties() override;
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;


protected:
	virtual UCurveTable* MakeNewCurveTable(UObject* InParent, FName Name, EObjectFlags Flags);

	ERichCurveInterpMode InterpMode;
};
