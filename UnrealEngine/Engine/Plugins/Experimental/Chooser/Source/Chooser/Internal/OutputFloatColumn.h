// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterFloat.h"
#include "ChooserPropertyAccess.h"
#include "InstancedStruct.h"
#include "OutputFloatColumn.generated.h"

USTRUCT()
struct CHOOSER_API FOutputFloatColumn : public FChooserColumnBase
{
	GENERATED_BODY()
	public:
	FOutputFloatColumn();
	virtual bool HasFilters() const override { return false; }
	virtual bool HasOutputs() const override { return true; }
	virtual void SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const override;
	
	UPROPERTY(EditAnywhere, Meta = (ExcludeBaseStruct, NoClear, BaseStruct = "/Script/Chooser.ChooserParameterFloatBase"), Category = "Hidden")
	FInstancedStruct InputValue;

#if WITH_EDITOR
	mutable double TestValue=false;
#endif
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category=Runtime);
	double DefaultRowValue = 0.0;
#endif
	
	UPROPERTY(EditAnywhere, Category=Runtime);
	TArray<double> RowValues; 

	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterFloatBase);
};