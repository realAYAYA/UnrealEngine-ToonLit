// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IChooserColumn.h"
#include "IChooserParameterBool.h"
#include "ChooserPropertyAccess.h"
#include "InstancedStruct.h"
#include "OutputBoolColumn.generated.h"

USTRUCT()
struct CHOOSER_API FOutputBoolColumn : public FChooserColumnBase
{
	GENERATED_BODY()
	public:
	FOutputBoolColumn();
	virtual bool HasFilters() const override { return false; }
	virtual bool HasOutputs() const override { return true; }
	virtual void SetOutputs(FChooserEvaluationContext& Context, int RowIndex) const override;
	
	UPROPERTY(EditAnywhere, NoClear, Meta = (ExcludeBaseStruct, BaseStruct = "/Script/Chooser.ChooserParameterBoolBase"), Category = "Hidden")
	FInstancedStruct InputValue;

#if WITH_EDITOR
	mutable bool TestValue=false;
#endif
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category=Runtime);
	bool DefaultRowValue = 0;
#endif
	
	UPROPERTY(EditAnywhere, Category=Runtime);
	TArray<bool> RowValues; 

	CHOOSER_COLUMN_BOILERPLATE(FChooserParameterBoolBase);
};