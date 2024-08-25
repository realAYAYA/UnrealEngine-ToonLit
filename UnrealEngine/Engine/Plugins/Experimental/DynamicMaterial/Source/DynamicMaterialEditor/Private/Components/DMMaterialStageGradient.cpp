// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMMaterialStageGradient.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"

#define LOCTEXT_NAMESPACE "DMMaterialProperty"

TArray<TStrongObjectPtr<UClass>> UDMMaterialStageGradient::Gradients = {};

UDMMaterialStageGradient::UDMMaterialStageGradient()
	: UDMMaterialStageGradient(FText::GetEmpty())
{
}

UDMMaterialStageGradient::UDMMaterialStageGradient(const FText& InName)
	: UDMMaterialStageThroughput(InName)
{
	bAllowNestedInputs = true;

	InputConnectors.Add({0, LOCTEXT("UV", "UV"), EDMValueType::VT_Float2});

	OutputConnectors.Add({0, LOCTEXT("Value", "Value"), EDMValueType::VT_Float3_RGB});
}

UDMMaterialStage* UDMMaterialStageGradient::CreateStage(TSubclassOf<UDMMaterialStageGradient> InMaterialStageGradientClass, UDMMaterialLayerObject* InLayer)
{
	check(InMaterialStageGradientClass);

	GetAvailableGradients();
	check(Gradients.Contains(TStrongObjectPtr<UClass>(InMaterialStageGradientClass.Get())));

	const FDMUpdateGuard Guard;

	UDMMaterialStage* NewStage = UDMMaterialStage::CreateMaterialStage(InLayer);

	UDMMaterialStageGradient* SourceGradient = NewObject<UDMMaterialStageGradient>(
		NewStage, 
		InMaterialStageGradientClass.Get(), 
		NAME_None, 
		RF_Transactional
	);
	
	check(SourceGradient);

	NewStage->SetSource(SourceGradient);

	return NewStage;
}

const TArray<TStrongObjectPtr<UClass>>& UDMMaterialStageGradient::GetAvailableGradients()
{
	if (Gradients.IsEmpty())
	{
		GenerateGradientList();
	}

	return Gradients;
}

UDMMaterialStageGradient* UDMMaterialStageGradient::ChangeStageSource_Gradient(UDMMaterialStage* InStage, 
	TSubclassOf<UDMMaterialStageGradient> InGradientClass)
{
	check(InStage);

	if (!InStage->CanChangeSource())
	{
		return nullptr;
	}

	check(InGradientClass);
	check(!(InGradientClass->ClassFlags & (CLASS_Abstract | CLASS_Hidden | CLASS_Deprecated | CLASS_NewerVersionExists)));

	return InStage->ChangeSource<UDMMaterialStageGradient>(InGradientClass);
}

bool UDMMaterialStageGradient::CanChangeInputType(int32 InputIndex) const
{
	if (InputIndex == 0)
	{
		return false;
	}

	return Super::CanChangeInput(InputIndex);
}

void UDMMaterialStageGradient::GenerateGradientList()
{
	Gradients.Empty();

	const TArray<TStrongObjectPtr<UClass>>& SourceList = UDMMaterialStageSource::GetAvailableSourceClasses();

	for (const TStrongObjectPtr<UClass>& SourceClass : SourceList)
	{
		UDMMaterialStageGradient* StageGradientCDO = Cast<UDMMaterialStageGradient>(SourceClass->GetDefaultObject(true));

		if (!StageGradientCDO)
		{
			continue;
		}

		Gradients.Add(SourceClass);
	}
}

#undef LOCTEXT_NAMESPACE
