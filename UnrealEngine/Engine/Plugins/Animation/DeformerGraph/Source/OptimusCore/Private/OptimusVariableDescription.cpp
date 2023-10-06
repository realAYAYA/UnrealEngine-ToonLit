// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusVariableDescription.h"

#include "OptimusDataTypeRegistry.h"
#include "OptimusDeformer.h"
#include "OptimusHelpers.h"
#include "OptimusValueContainer.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusVariableDescription)


bool UOptimusVariableDescription::EnsureValueContainer()
{
	bool bValueContainerChanged = false;
	
	// Check if the current default value storage matches, otherwise create a matching default value storage, otherwise
	// if the variable type changes, we end up with mismatch in storage vs type.
	const UClass* RequiredClass = UOptimusValueContainerGeneratorClass::GetClassForType(GetPackage(), DataType);

	bool bValueContainerNeedsZeroing = false;
	if (!DefaultValue || DefaultValue->GetClass() != RequiredClass)
	{
		DefaultValue = UOptimusValueContainer::MakeValueContainer(this, DataType);
		bValueContainerChanged = true;
		bValueContainerNeedsZeroing = true;
	}

	if (DataType->CanCreateProperty())
	{
		const FShaderValueType::FValue ShaderValue = DataType->MakeShaderValue();
		
		if (bValueContainerNeedsZeroing || ValueData.Num() != ShaderValue.ShaderValue.Num())
		{
			ValueData.SetNumZeroed(ShaderValue.ShaderValue.Num());
			bValueContainerChanged = true;
		}
	}
	else if (!ValueData.IsEmpty())
	{
		ValueData.Reset();
		bValueContainerChanged = true;
	}

	return bValueContainerChanged;
}


UOptimusDeformer* UOptimusVariableDescription::GetOwningDeformer() const
{
	const UOptimusVariableContainer* Container = CastChecked<UOptimusVariableContainer>(GetOuter());
	return Container ? CastChecked<UOptimusDeformer>(Container->GetOuter()) : nullptr;
}


int32 UOptimusVariableDescription::GetIndex() const
{
	const UOptimusVariableContainer* Container = CastChecked<UOptimusVariableContainer>(GetOuter());
	return Container->Descriptions.IndexOfByKey(this);
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
	}

	if (EnsureValueContainer())
	{
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
