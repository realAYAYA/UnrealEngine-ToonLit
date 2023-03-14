// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusVariableDescription.h"

#include "OptimusDataTypeRegistry.h"
#include "OptimusDeformer.h"
#include "OptimusHelpers.h"
#include "OptimusValueContainer.h"


void UOptimusVariableDescription::ResetValueDataSize()
{
	DefaultValue = UOptimusValueContainer::MakeValueContainer(this, DataType);

	if (DataType->CanCreateProperty())
	{
		// Create a temporary property from the type so that we can get the size of the
		// type for properly resizing the storage.
		TUniquePtr<FProperty> TempProperty(DataType->CreateProperty(nullptr, NAME_None));
		
		if (ValueData.Num() != TempProperty->GetSize())
		{
			ValueData.SetNumZeroed(TempProperty->GetSize());
		}
	}
}


UOptimusDeformer* UOptimusVariableDescription::GetOwningDeformer() const
{
	const UOptimusVariableContainer* Container = CastChecked<UOptimusVariableContainer>(GetOuter());
	return Container ? CastChecked<UOptimusDeformer>(Container->GetOuter()) : nullptr;
}

void UOptimusVariableDescription::PostLoad()
{
	Super::PostLoad();

	// 32-bit float data type is not supported for variables although they were allowed before. Do an in-place upgrade here. 
	const FOptimusDataTypeHandle FloatDataType = FOptimusDataTypeRegistry::Get().FindType(*FFloatProperty::StaticClass());
	const FOptimusDataTypeHandle DoubleDataType = FOptimusDataTypeRegistry::Get().FindType(*FDoubleProperty::StaticClass());
	
	if (DataType == FloatDataType)
	{
		DataType = DoubleDataType;
		ResetValueDataSize();
		(void)MarkPackageDirty();
	}
}


#if WITH_EDITOR

void UOptimusVariableDescription::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UOptimusVariableDescription, VariableName))
	{
		UOptimusDeformer* Deformer = GetOwningDeformer();
		if (ensure(Deformer))
		{
			VariableName = Optimus::GetUniqueNameForScope(GetOuter(), VariableName);
			Rename(*VariableName.ToString(), nullptr);

			constexpr bool bForceChange = true;
			Deformer->RenameVariable(this, VariableName, bForceChange);
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FOptimusDataType, TypeName))
	{
		UOptimusDeformer* Deformer = GetOwningDeformer();
		if (ensure(Deformer))
		{
			// Set the variable type again, so that we can remove any links that are now type-incompatible.
			constexpr bool bForceChange = true;
			Deformer->SetVariableDataType(this, DataType, bForceChange);
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UOptimusVariableDescription, DefaultValue))
	{
		// Store the default shader value.
		FShaderValueType::FValue Value = DefaultValue->GetShaderValue();
		ValueData = MoveTemp(Value.ShaderValue);
	}
}


void UOptimusVariableDescription::PreEditUndo()
{
	UObject::PreEditUndo();

	VariableNameForUndo = VariableName;
}


void UOptimusVariableDescription::PostEditUndo()
{
	UObject::PostEditUndo();

	if (VariableNameForUndo != VariableName)
	{
		const UOptimusDeformer *Deformer = GetOwningDeformer();
		Deformer->Notify(EOptimusGlobalNotifyType::VariableRenamed, this);
	}
}
#endif
