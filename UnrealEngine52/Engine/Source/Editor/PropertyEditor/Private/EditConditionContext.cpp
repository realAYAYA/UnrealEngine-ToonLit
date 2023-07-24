// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditConditionContext.h"
#include "EditConditionParser.h"

#include "ObjectPropertyNode.h"
#include "PropertyNode.h"
#include "PropertyEditorHelpers.h"
#include "PropertyPathHelpers.h"

DEFINE_LOG_CATEGORY(LogEditCondition);

FEditConditionContext::FEditConditionContext(FPropertyNode& InPropertyNode)
{
	PropertyNode = InPropertyNode.AsShared();
}

FName FEditConditionContext::GetContextName() const
{
	TSharedPtr<FPropertyNode> PinnedNode = PropertyNode.Pin();
	if (!PinnedNode.IsValid())
	{
		return FName();
	}

	return PinnedNode->GetProperty()->GetOwnerStruct()->GetFName();
}

const FBoolProperty* FEditConditionContext::GetSingleBoolProperty(const TSharedPtr<FEditConditionExpression>& Expression) const
{
	TSharedPtr<FPropertyNode> PinnedNode = PropertyNode.Pin();
	if (!PinnedNode.IsValid())
	{
		return nullptr;
	}

	const FProperty* Property = PinnedNode->GetProperty();
	if (Property == nullptr)
	{
		return nullptr;
	}

	const FBoolProperty* BoolProperty = nullptr;

	for (const FCompiledToken& Token : Expression->Tokens)
	{
		if (const EditConditionParserTokens::FPropertyToken* PropertyToken = Token.Node.Cast<EditConditionParserTokens::FPropertyToken>())
		{
			if (BoolProperty != nullptr)
			{
				// second property token in the same expression, this can't be a simple expression like "bValue == false"
				return nullptr;
			}

			BoolProperty = FindFProperty<FBoolProperty>(Property->GetOwnerStruct(), *PropertyToken->PropertyName);
			if (BoolProperty == nullptr)
			{
				return nullptr;
			}
		}
	}

	return BoolProperty;
}

const TWeakObjectPtr<UFunction> FEditConditionContext::GetFunction(const FString& FieldName) const
{
	TSharedPtr<FPropertyNode> PinnedNode = PropertyNode.Pin();
	if (!PinnedNode.IsValid())
	{
		return nullptr;
	}

	const FProperty* Property = PinnedNode->GetProperty();
	if (Property == nullptr)
	{
		return nullptr;
	}

	TWeakObjectPtr<UFunction> Function = FindUField<UFunction>(Property->GetOwnerStruct(), *FieldName);

	if (Function == nullptr && FieldName.Contains(TEXT(".")))
	{
		// Function not found in struct, try to see if this is a static function
		Function = FindObject<UFunction>(nullptr, *FieldName, true);

		if (Function.IsValid() && !Function->HasAnyFunctionFlags(EFunctionFlags::FUNC_Static))
		{
			Function = nullptr;
		}
	}

	return Function;
}

static TSet<TPair<FName, FString>> AlreadyLogged;

/**
 * Attempt to property within the node & log if not found.
 * 
 * @param PropertyNode		Property owning the field to search for, ex: a UObject
 * @param FieldNam			Name of field to search for
 * @return					Field for given fieldname if found, nullptr otherwise 
 */
template<typename T>
const T* FindTypedField(const TSharedPtr<FPropertyNode>& PropertyNode, const FString& FieldName)
{
	if (!PropertyNode.IsValid())
	{
		return nullptr;
	}

	const FProperty* Property = PropertyNode->GetProperty();
	if (Property == nullptr)
	{
		return nullptr;
	}

	const FProperty* Field = FindFProperty<FProperty>(Property->GetOwnerStruct(), *FieldName);
	if (Field == nullptr)
	{
		TPair<FName, FString> FieldKey(Property->GetOwnerStruct()->GetFName(), FieldName);
		if (!AlreadyLogged.Find(FieldKey))
		{
			AlreadyLogged.Add(FieldKey);
			UE_LOG(LogEditCondition, Error, TEXT("EditCondition parsing failed: Field name \"%s\" was not found in class \"%s\"."), *FieldName, *Property->GetOwnerStruct()->GetName());
		}

		return nullptr;
	}

	return CastField<T>(Field);
}

/** 
 * Get the parent to use as the context when evaluating the edit condition.
 * For normal properties inside a UObject, this is the UObject. 
 * For children of containers, this is the UObject the container is in. 
 * Note: We do not support nested containers.
 * The result can be nullptr in exceptional cases, eg. if the UI is getting rebuilt.
 */
static const FPropertyNode* GetEditConditionParentNode(const TSharedPtr<FPropertyNode>& PropertyNode)
{
	const FPropertyNode* ParentNode = PropertyNode->GetParentNode();
	if (ParentNode == nullptr)
	{
		return nullptr;
	}

	FFieldVariant PropertyOuter = PropertyNode->GetProperty()->GetOwnerVariant();

	if (PropertyOuter.Get<FArrayProperty>() != nullptr ||
		PropertyOuter.Get<FSetProperty>() != nullptr ||
		PropertyOuter.Get<FMapProperty>() != nullptr)
	{
		// in a dynamic container, parent is actually one level up
		return ParentNode->GetParentNode();
	}

	if (PropertyNode->GetProperty()->ArrayDim > 1 && PropertyNode->GetArrayIndex() != INDEX_NONE)
	{
		// in a fixed size container, parent node is just the header field
		return ParentNode->GetParentNode();
	}

	return ParentNode;
}

static const uint8* GetPropertyValuePtr(const FProperty* Property, const TSharedPtr<FPropertyNode>& PropertyNode, const FPropertyNode* ParentNode, const FComplexPropertyNode* ComplexParentNode, int32 Index)
{
	const uint8* ValuePtr = ComplexParentNode->GetValuePtrOfInstance(Index, Property, ParentNode);
	return ValuePtr;
}

TOptional<bool> FEditConditionContext::GetBoolValue(const FString& PropertyName, TWeakObjectPtr<UFunction> CachedFunction) const
{
	TSharedPtr<FPropertyNode> PinnedNode = PropertyNode.Pin();
	if (!PinnedNode.IsValid())
	{
		return TOptional<bool>();
	}

	if (const UFunction* Function = CachedFunction.Get())
	{
		if (CastField<FBoolProperty>(Function->GetReturnProperty()) == nullptr)
		{
			// Function return type not bool, return undefined
			return TOptional<bool>();
		}

		// Check for external static function references 
		if (Function->HasAnyFunctionFlags(EFunctionFlags::FUNC_Static))
		{
			FString StaticFunctionName = {};
			UObject* EditConditionExpressionCDO = Function->GetOuterUClass()->GetDefaultObject();
			Function->GetName(StaticFunctionName);

			{
				FEditorScriptExecutionGuard ScriptExecutionGuard;
				FCachedPropertyPath Path(StaticFunctionName);
				bool bResult = true;
				if (PropertyPathHelpers::GetPropertyValue(EditConditionExpressionCDO, Path, bResult))
				{
					return bResult;
				}
				else
				{
					// Execution failed, return undefined
					return TOptional<bool>();
				}
			}
		}

		// We might be selecting multiple objects, account for that
		TArray<UObject*> OutObjects;
		FObjectPropertyNode* ObjectNode = PinnedNode->FindObjectItemParent();
		if (ObjectNode)
		{
			int32 NumObjects = ObjectNode->GetNumObjects();
			for (int32 ObjectIndex = 0; ObjectIndex < NumObjects; ++ObjectIndex)
			{
				FEditorScriptExecutionGuard ScriptExecutionGuard;

				UObject* FunctionTarget = ObjectNode->GetUObject(ObjectIndex);
				FCachedPropertyPath FunctionPath(PropertyName);
				bool bResult = true;
				if (PropertyPathHelpers::GetPropertyValue(FunctionTarget, FunctionPath, bResult))
				{
					if (!bResult)
					{
						return false;
					}
				}
				else
				{
					// Execution failed, return undefined
					return TOptional<bool>();
				}
			}

			// Executed our target function over relevant objects with no condtion failures, return true
			if (NumObjects > 0)
			{
				return true;
			}
		}
	}

	const FBoolProperty* BoolProperty = FindTypedField<FBoolProperty>(PinnedNode, PropertyName);
	if (BoolProperty == nullptr)
	{
		return TOptional<bool>();
	}

	const FPropertyNode* ParentNode = GetEditConditionParentNode(PinnedNode);
	if (ParentNode == nullptr)
	{
		return TOptional<bool>();
	}

	const FComplexPropertyNode* ComplexParentNode = PinnedNode->FindComplexParent();
	if (ComplexParentNode == nullptr)
	{
		return TOptional<bool>();
	}

	TOptional<bool> Result;
	for (int32 Index = 0; Index < ComplexParentNode->GetInstancesNum(); ++Index)
	{
		const uint8* ValuePtr = GetPropertyValuePtr(BoolProperty, PinnedNode, ParentNode, ComplexParentNode, Index);
		if (ValuePtr == nullptr)
		{
			return TOptional<bool>();
		}

		bool bValue = BoolProperty->GetPropertyValue(ValuePtr);
		if (!Result.IsSet())
		{
			Result = bValue;
		}
		else if (Result.GetValue() != bValue)
		{
			// all values aren't the same...
			return TOptional<bool>();
		}
	}

	return Result;
}

TOptional<int64> FEditConditionContext::GetIntegerValue(const FString& PropertyName, TWeakObjectPtr<UFunction> CachedFunction) const
{
	TSharedPtr<FPropertyNode> PinnedNode = PropertyNode.Pin();
	if (!PinnedNode.IsValid())
	{
		return TOptional<int64>();
	}

	if (const UFunction* Function = CachedFunction.Get())
	{ 
		// EditConditions Currently only support bool, see: UE-175891
		return TOptional<int64>();
	}

	const FProperty* Property = FindTypedField<FProperty>(PinnedNode, PropertyName);
	const FNumericProperty* NumericProperty = CastField<FNumericProperty>(Property);

	if (NumericProperty == nullptr)
	{
		// Retry with an enum and its underlying property
		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			NumericProperty = EnumProperty->GetUnderlyingProperty();
		}
	}

	if (NumericProperty == nullptr || !NumericProperty->IsInteger())
	{
		return TOptional<int64>();
	}

	const FPropertyNode* ParentNode = GetEditConditionParentNode(PinnedNode);
	if (ParentNode == nullptr)
	{
		return TOptional<int64>();
	}

	const FComplexPropertyNode* ComplexParentNode = PinnedNode->FindComplexParent();
	if (ComplexParentNode == nullptr)
	{
		return TOptional<int64>();
	}

	TOptional<int64> Result;
	for (int32 Index = 0; Index < ComplexParentNode->GetInstancesNum(); ++Index)
	{
		const uint8* ValuePtr = GetPropertyValuePtr(Property, PinnedNode, ParentNode, ComplexParentNode, Index);
		if (ValuePtr == nullptr)
		{
			return TOptional<int64>();
		}

		int64 Value = NumericProperty->GetSignedIntPropertyValue(ValuePtr);
		if (!Result.IsSet())
		{
			Result = Value;
		}
		else if (Result.GetValue() != Value)
		{
			// all values aren't the same...
			return TOptional<int64>();
		}
	}

	return Result;
}

TOptional<double> FEditConditionContext::GetNumericValue(const FString& PropertyName, TWeakObjectPtr<UFunction> CachedFunction) const
{
	TSharedPtr<FPropertyNode> PinnedNode = PropertyNode.Pin();
	if (!PinnedNode.IsValid())
	{
		return TOptional<double>();
	}

	if (const UFunction* Function = CachedFunction.Get())
	{
		// EditConditions Currently only support bool, see: UE-175891
		return TOptional<double>();
	}

	const FNumericProperty* NumericProperty = FindTypedField<FNumericProperty>(PinnedNode, PropertyName);
	if (NumericProperty == nullptr)
	{
		return TOptional<double>();
	}

	const FPropertyNode* ParentNode = GetEditConditionParentNode(PinnedNode);
	if (ParentNode == nullptr)
	{
		return TOptional<double>();
	}

	const FComplexPropertyNode* ComplexParentNode = PinnedNode->FindComplexParent();
	if (ComplexParentNode == nullptr)
	{
		return TOptional<double>();
	}

	TOptional<double> Result;
	for (int32 Index = 0; Index < ComplexParentNode->GetInstancesNum(); ++Index)
	{
		const uint8* ValuePtr = GetPropertyValuePtr(NumericProperty, PinnedNode, ParentNode, ComplexParentNode, Index);
		if (ValuePtr == nullptr)
		{
			return TOptional<double>();
		}

		double Value = 0;

		if (NumericProperty->IsInteger())
		{
			Value = (double) NumericProperty->GetSignedIntPropertyValue(ValuePtr);
		}
		else if (NumericProperty->IsFloatingPoint())
		{
			Value = NumericProperty->GetFloatingPointPropertyValue(ValuePtr);
		}

		if (!Result.IsSet())
		{
			Result = Value;
		}
		else if (!FMath::IsNearlyEqual(Result.GetValue(), Value))
		{
			// all values aren't the same...
			return TOptional<double>();
		}
	}

	return Result;
}

TOptional<FString> FEditConditionContext::GetEnumValue(const FString& PropertyName, TWeakObjectPtr<UFunction> CachedFunction) const
{
	TSharedPtr<FPropertyNode> PinnedNode = PropertyNode.Pin();
	if (!PinnedNode.IsValid())
	{
		return TOptional<FString>();
	}

	if (const UFunction* Function = CachedFunction.Get())
	{
		// EditConditions Currently only support bool, see: UE-175891
		return TOptional<FString>();
	}

	const FProperty* Property = FindTypedField<FProperty>(PinnedNode, PropertyName);
	if (Property == nullptr)
	{
		return TOptional<FString>();
	}

	const UEnum* EnumType = nullptr;
	const FNumericProperty* NumericProperty = nullptr;
	if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
	{
		NumericProperty = EnumProperty->GetUnderlyingProperty();
		EnumType = EnumProperty->GetEnum();
	}
	else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
	{
		NumericProperty = ByteProperty;
		EnumType = ByteProperty->GetIntPropertyEnum();
	}

	if (EnumType == nullptr || NumericProperty == nullptr || !NumericProperty->IsInteger())
	{
		return TOptional<FString>();
	}

	const FPropertyNode* ParentNode = GetEditConditionParentNode(PinnedNode);
	if (ParentNode == nullptr)
	{
		return TOptional<FString>();
	}

	const FComplexPropertyNode* ComplexParentNode = PinnedNode->FindComplexParent();
	if (ComplexParentNode == nullptr)
	{
		return TOptional<FString>();
	}

	TOptional<int64> Result;
	for (int32 Index = 0; Index < ComplexParentNode->GetInstancesNum(); ++Index)
	{
		// NOTE: this very intentionally fetches the value from Property, not NumericProperty, 
		// because the underlying property of an enum does not return a valid value
		const uint8* ValuePtr = GetPropertyValuePtr(Property, PinnedNode, ParentNode, ComplexParentNode, Index);
		if (ValuePtr == nullptr)
		{
			return TOptional<FString>();
		}

		const int64 Value = NumericProperty->GetSignedIntPropertyValue(ValuePtr);
		if (!Result.IsSet())
		{
			Result = Value;
		}
		else if (Result.GetValue() != Value)
		{
			// all values aren't the same...
			return TOptional<FString>();
		}
	}

	if (!Result.IsSet())
	{
		return TOptional<FString>();
	}

	return EnumType->GetNameStringByValue(Result.GetValue());
}

TOptional<UObject*> FEditConditionContext::GetPointerValue(const FString& PropertyName, TWeakObjectPtr<UFunction> CachedFunction) const
{
	TSharedPtr<FPropertyNode> PinnedNode = PropertyNode.Pin();
	if (!PinnedNode.IsValid())
	{
		return TOptional<UObject*>();
	}

	if (const UFunction* Function = CachedFunction.Get())
	{
		// EditConditions Currently only support bool, see: UE-175891
		return TOptional<UObject*>();
	}

	const FProperty* Property = FindTypedField<FProperty>(PinnedNode, PropertyName);
	if (Property == nullptr)
	{
		return TOptional<UObject*>();
	}

	const FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property);
	if (ObjectProperty == nullptr)
	{
		return TOptional<UObject*>();
	}

	const FPropertyNode* ParentNode = GetEditConditionParentNode(PinnedNode);
	if (ParentNode == nullptr)
	{
		return TOptional<UObject*>();
	}

	const FComplexPropertyNode* ComplexParentNode = PinnedNode->FindComplexParent();
	if (ComplexParentNode == nullptr)
	{
		return TOptional<UObject*>();
	}

	TOptional<UObject*> Result;
	for (int32 Index = 0; Index < ComplexParentNode->GetInstancesNum(); ++Index)
	{
		const uint8* ValuePtr = GetPropertyValuePtr(Property, PinnedNode, ParentNode, ComplexParentNode, Index);
		if (ValuePtr == nullptr)
		{
			return TOptional<UObject*>();
		}

		UObject* Value = ObjectProperty->GetObjectPropertyValue(ValuePtr);
		if (!Result.IsSet())
		{
			Result = Value;
		}
		else if (Result.GetValue() != Value)
		{
			// all values aren't the same
			return TOptional<UObject*>();
		}
	}

	return Result;
}

TOptional<FString> FEditConditionContext::GetTypeName(const FString& PropertyName, TWeakObjectPtr<UFunction> CachedFunction) const
{
	TSharedPtr<FPropertyNode> PinnedNode = PropertyNode.Pin();
	if (!PinnedNode.IsValid())
	{
		return TOptional<FString>();
	}

	auto GetPropertyName = [](const FProperty* Property) -> TOptional<FString>
	{
		if (Property == nullptr)
		{
			return TOptional<FString>();
		}

		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			return EnumProperty->GetEnum()->GetName();
		}
		else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			const UEnum* EnumType = ByteProperty->GetIntPropertyEnum();
			if (EnumType != nullptr)
			{
				return EnumType->GetName();
			}
		}

		return Property->GetCPPType();
	};

	if (const UFunction* Function = CachedFunction.Get())
	{
		return GetPropertyName(Function->GetReturnProperty());
	}

	const FProperty* Property = FindTypedField<FProperty>(PinnedNode, PropertyName);

	return GetPropertyName(Property);
}

TOptional<int64> FEditConditionContext::GetIntegerValueOfEnum(const FString& EnumTypeName, const FString& MemberName) const
{
	const UEnum* EnumType = UClass::TryFindTypeSlow<UEnum>(EnumTypeName, EFindFirstObjectOptions::ExactClass);
	if (EnumType == nullptr)
	{
		return TOptional<int64>();
	}

	const int64 EnumValue = EnumType->GetValueByName(FName(*MemberName));
	if (EnumValue == INDEX_NONE)
	{
		return TOptional<int64>();
	}

	return EnumValue;
}
