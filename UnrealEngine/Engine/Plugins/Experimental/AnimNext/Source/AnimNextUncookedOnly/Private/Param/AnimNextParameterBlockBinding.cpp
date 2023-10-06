// Copyright Epic Games, Inc. All Rights Reserved.

#include "Param/AnimNextParameterBlockBinding.h"
#include "Param/AnimNextParameter.h"
#include "Param/AnimNextParameterLibrary.h"

#define LOCTEXT_NAMESPACE "AnimNextParameterBlockBinding"

FAnimNextParamType UAnimNextParameterBlockBinding::GetParamType() const
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

FName UAnimNextParameterBlockBinding::GetParameterName() const
{
	return ParameterName;
}

void UAnimNextParameterBlockBinding::SetParameterName(FName InName, bool bSetupUndoRedo)
{
	if(bSetupUndoRedo)
	{
		Modify();
	}

	ParameterName = InName;

	BroadcastModified();
}

const UAnimNextParameter* UAnimNextParameterBlockBinding::GetParameter() const
{
	if(Library)
	{
		return Library->FindParameter(ParameterName);
	}

	return nullptr;
}

const UAnimNextParameterLibrary* UAnimNextParameterBlockBinding::GetLibrary() const
{
	return Library;
}

FText UAnimNextParameterBlockBinding::GetDisplayName() const
{
	return FText::FromName(ParameterName);
}

FText UAnimNextParameterBlockBinding::GetDisplayNameTooltip() const
{
	FTextBuilder TextBuilder;
	TextBuilder.AppendLine(FText::FromName(ParameterName));
	TextBuilder.AppendLine(Library ? FText::FromString(Library->GetPathName()) : LOCTEXT("MissingLibrary", "Missing parameter library"));
	return TextBuilder.ToText();
}

void UAnimNextParameterBlockBinding::GetEditedObjects(TArray<UObject*>& OutObjects) const
{
	if(Library)
	{
		OutObjects.Add(Library);
	}

	if(const UAnimNextParameter* Parameter = GetParameter())
	{
		OutObjects.Add(const_cast<UAnimNextParameter*>(Parameter));
	}
}

#undef LOCTEXT_NAMESPACE