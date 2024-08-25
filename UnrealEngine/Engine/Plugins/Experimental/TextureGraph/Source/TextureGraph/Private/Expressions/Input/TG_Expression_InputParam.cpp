// Copyright Epic Games, Inc. All Rights Reserved.

#include "expressions/Input/TG_Expression_InputParam.h"


#if WITH_EDITOR
void UTG_Expression_InputParam::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// if Graph changes catch it first
	if (PropertyChangedEvent.GetPropertyName() == FName(TEXT("IsConstant")))
	{
		UE_LOG(LogTextureGraph, Log, TEXT("InputParam  Expression Parameter/Constant PostEditChangeProperty."));
		NotifySignatureChanged();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UTG_Expression_InputParam::SetbIsConstant(bool InIsConstant)
{
	if (bIsConstant != InIsConstant)
	{
		Modify();
		bIsConstant = InIsConstant;
		NotifySignatureChanged();
	}
}

void UTG_Expression_InputParam::ToggleIsConstant()
{
	Modify();
	if (bIsConstant)
	{
		bIsConstant = false;
	}
	else {
		bIsConstant = true;
	}
	NotifySignatureChanged();
}