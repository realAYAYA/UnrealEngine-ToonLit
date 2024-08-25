// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR
#include "StateTreePropertyRefHelpers.h"
#include "StateTreePropertyRef.h"
#include "UObject/TextProperty.h"
#include "UObject/EnumProperty.h"
#include "UObject/Class.h"
#include "IPropertyAccessEditor.h"
#include "StateTreePropertyBindings.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"

namespace UE::StateTree::PropertyRefHelpers
{
	static const FName BoolName = TEXT("bool");
	static const FName ByteName = TEXT("byte");
	static const FName Int32Name = TEXT("int32");
	static const FName Int64Name = TEXT("int64");
	static const FName FloatName = TEXT("float");
	static const FName DoubleName = TEXT("double");
	static const FName NameName = TEXT("Name");
	static const FName StringName = TEXT("String");
	static const FName TextName = TEXT("Text");

	static const FName IsRefToArrayName = TEXT("IsRefToArray");
	static const FName RefTypeName = TEXT("RefType");

	bool IsPropertyRefCompatibleWithProperty(const FProperty& RefProperty, const FProperty& SourceProperty)
	{
		ensure(IsPropertyRef(RefProperty));

		const FProperty* TestProperty = &SourceProperty;
		const bool bIsTargetRefArray = RefProperty.HasMetaData(IsRefToArrayName);

		if (bIsTargetRefArray)
		{
			if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(TestProperty))
			{
				TestProperty = ArrayProperty->Inner;
			}
			else
			{
				return false;
			}
		}

		const FString& TargetTypeNameStr = RefProperty.GetMetaData(RefTypeName);
		if (TargetTypeNameStr.IsEmpty())
		{
			return false;
		}

		const FName TargetTypeName = FName(*TargetTypeNameStr);

		const FStructProperty* SourceStructProperty = CastField<FStructProperty>(TestProperty);

		// Compare properties metadata directly if SourceProperty is PropertyRef as well
		if (SourceStructProperty && SourceStructProperty->Struct == FStateTreePropertyRef::StaticStruct())
		{
			const FName SourceTypeName(SourceStructProperty->GetMetaData(RefTypeName));
			const bool bIsSourceRefArray = SourceStructProperty->GetBoolMetaData(IsRefToArrayName);

			return SourceTypeName == TargetTypeName && bIsSourceRefArray == bIsTargetRefArray;
		}

		if(TargetTypeName == BoolName)
		{
			return CastField<FBoolProperty>(TestProperty) != nullptr;
		}
		else if(TargetTypeName == ByteName)
		{
			return CastField<FByteProperty>(TestProperty) != nullptr;
		}
		else if(TargetTypeName == Int32Name)
		{
			return CastField<FIntProperty>(TestProperty) != nullptr;
		}
		else if(TargetTypeName == Int64Name)
		{
			return CastField<FInt64Property>(TestProperty) != nullptr;
		}
		else if(TargetTypeName == FloatName)
		{
			return CastField<FFloatProperty>(TestProperty) != nullptr;
		}
		else if(TargetTypeName == DoubleName)
		{
			return CastField<FDoubleProperty>(TestProperty) != nullptr;
		}
		else if(TargetTypeName == NameName)
		{
			return CastField<FNameProperty>(TestProperty) != nullptr;
		}
		else if(TargetTypeName == StringName)
		{
			return CastField<FStrProperty>(TestProperty) != nullptr;
		}
		else if(TargetTypeName == TextName)
		{
			return CastField<FTextProperty>(TestProperty) != nullptr;
		}
		else
		{
			UField* TargetRefField = UClass::TryFindTypeSlow<UField>(TargetTypeNameStr);
			if (!TargetRefField)
			{
				TargetRefField = LoadObject<UField>(nullptr, *TargetTypeNameStr);
			}

			if (SourceStructProperty)
			{
				return SourceStructProperty->Struct->IsChildOf(Cast<UStruct>(TargetRefField));
			}

			if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(TestProperty))
			{
				// Only referencing object of the same exact class should be allowed. Otherwise one could e.g assign UObject to AActor property through reference to UObject.
				return ObjectProperty->PropertyClass == Cast<UStruct>(TargetRefField);
			}

			if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(TestProperty))
			{
				return EnumProperty->GetEnum() == TargetRefField;
			}
		}

		return false;
	}

	bool IsPropertyAccessibleForPropertyRef(const FProperty& SourceProperty, FStateTreeBindableStructDesc SourceStruct, bool bIsOutput)
	{
		switch (SourceStruct.DataSource)
		{
		case EStateTreeBindableStructSource::Parameter:
		case EStateTreeBindableStructSource::State:
			return true;

		case EStateTreeBindableStructSource::Context:
		case EStateTreeBindableStructSource::Condition:
			return false;

		case EStateTreeBindableStructSource::GlobalTask:
		case EStateTreeBindableStructSource::Evaluator:
		case EStateTreeBindableStructSource::Task:
			return bIsOutput || IsPropertyRef(SourceProperty);
		default:
			checkNoEntry();
		}

		return false;
	}

	bool IsPropertyAccessibleForPropertyRef(TConstArrayView<FStateTreePropertyPathIndirection> SourcePropertyPathIndirections, FStateTreeBindableStructDesc SourceStruct)
	{
		bool bIsOutput = false;
		for (const FStateTreePropertyPathIndirection& Indirection : SourcePropertyPathIndirections)
		{
			if (UE::StateTree::GetUsageFromMetaData(Indirection.GetProperty()) == EStateTreePropertyUsage::Output)
			{
				bIsOutput = true;
				break;
			}
		}

		return IsPropertyAccessibleForPropertyRef(*SourcePropertyPathIndirections.Last().GetProperty(), SourceStruct, bIsOutput);
	}

	bool IsPropertyAccessibleForPropertyRef(const FProperty& SourceProperty, TConstArrayView<FBindingChainElement> BindingChain, FStateTreeBindableStructDesc SourceStruct)
	{
		bool bIsOutput = UE::StateTree::GetUsageFromMetaData(&SourceProperty) == EStateTreePropertyUsage::Output;
		for (const FBindingChainElement& ChainElement : BindingChain)
		{
			if (const FProperty* Property = ChainElement.Field.Get<FProperty>())
			{
				if (UE::StateTree::GetUsageFromMetaData(Property) == EStateTreePropertyUsage::Output)
				{
					bIsOutput = true;
					break;
				}
			}
		}

		return IsPropertyAccessibleForPropertyRef(SourceProperty, SourceStruct, bIsOutput);
	}

	FEdGraphPinType GetPropertyRefInternalTypeAsPin(const FProperty& RefProperty)
	{
		ensure(IsPropertyRef(RefProperty));

		FEdGraphPinType PinType;
		PinType.PinSubCategory = NAME_None;

		PinType.ContainerType = RefProperty.HasMetaData(IsRefToArrayName) ? EPinContainerType::Array : EPinContainerType::None;
		const FString& TargetTypeNameStr = RefProperty.GetMetaData(RefTypeName);
		const FName TargetTypeName = FName(*TargetTypeNameStr);

		if(TargetTypeName == BoolName)
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
		}
		else if(TargetTypeName == ByteName)
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Byte;
		}
		else if(TargetTypeName == Int32Name)
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Int;
		}
		else if(TargetTypeName == Int64Name)
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Int64;
		}
		else if(TargetTypeName == FloatName)
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
			PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
		}
		else if(TargetTypeName == DoubleName)
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
			PinType.PinSubCategory = UEdGraphSchema_K2::PC_Double;
		}
		else if(TargetTypeName == NameName)
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Name;
		}
		else if(TargetTypeName == StringName)
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_String;
		}
		else if(TargetTypeName == TextName)
		{
			PinType.PinCategory = UEdGraphSchema_K2::PC_Text;
		}
		else
		{
			UField* TargetRefField = UClass::TryFindTypeSlow<UField>(TargetTypeNameStr);
			if (!TargetRefField)
			{
				TargetRefField = LoadObject<UField>(nullptr, *TargetTypeNameStr);
			}

			if (UStruct* Struct = Cast<UStruct>(TargetRefField))
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
				PinType.PinSubCategoryObject = Struct;
			}
			else if (UObject* Object = Cast<UObject>(TargetRefField))
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
				PinType.PinSubCategoryObject = Object;
			}
			else if (UEnum* Enum = Cast<UEnum>(TargetRefField))
			{
				PinType.PinCategory = UEdGraphSchema_K2::PC_Enum;
				PinType.PinSubCategoryObject = Enum;
			}
			else
			{
				checkNoEntry();
			}
		}

		return PinType;
	}

	bool IsPropertyRef(const FProperty& Property)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
		{
			return StructProperty->Struct == FStateTreePropertyRef::StaticStruct();
		}

		return false;
	}
}
#endif