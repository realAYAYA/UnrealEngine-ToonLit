// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlockBindingReference.h"
#include "Param/AnimNextParameter.h"
#include "Param/AnimNextParameterLibrary.h"
#include "Param/AnimNextParameterBlock.h"

#define LOCTEXT_NAMESPACE "AnimNextParameterBlockBindingReference"

FAnimNextParamType UAnimNextParameterBlockBindingReference::GetParamType() const
{
	if(Library)
	{
		if(const UAnimNextParameter* Parameter = Library->FindParameter(ParameterName))
		{
			return Parameter->GetType();
		}
	}

	return FAnimNextParamType();
}

FName UAnimNextParameterBlockBindingReference::GetParameterName() const
{
	return ParameterName;
}

void UAnimNextParameterBlockBindingReference::SetParameterName(FName InName, bool bSetupUndoRedo)
{
	ParameterName = InName;
}

const UAnimNextParameter* UAnimNextParameterBlockBindingReference::GetParameter() const
{
	if(Library)
	{
		return Library->FindParameter(ParameterName);
	}

	return nullptr;
}

const UAnimNextParameterLibrary* UAnimNextParameterBlockBindingReference::GetLibrary() const
{
	return Library;
}

const UAnimNextParameterBlock* UAnimNextParameterBlockBindingReference::GetBlock() const
{
	return Block;
}

FText UAnimNextParameterBlockBindingReference::GetDisplayName() const
{
	return FText::FromName(ParameterName);
}

FText UAnimNextParameterBlockBindingReference::GetDisplayNameTooltip() const
{
	FTextBuilder TextBuilder;
	TextBuilder.AppendLine(FText::FromName(ParameterName));
	TextBuilder.AppendLine(Library ? FText::FromString(Library->GetPathName()) : LOCTEXT("MissingLibrary", "Missing parameter library"));
	return TextBuilder.ToText();
}

void UAnimNextParameterBlockBindingReference::GetEditedObjects(TArray<UObject*>& OutObjects) const
{
	if(Library)
	{
		OutObjects.Add(Library);
	}

	if(const UAnimNextParameter* Parameter = GetParameter())
	{
		OutObjects.Add(const_cast<UAnimNextParameter*>(Parameter));
	}
	
	if(Block)
	{
		OutObjects.Add(Block);
	}
}

#undef LOCTEXT_NAMESPACE