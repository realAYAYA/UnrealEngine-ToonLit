// Copyright Epic Games, Inc. All Rights Reserved.

#include "NNERuntimeRDG.h"

#include "NNECoreRuntimeFormat.h"
#include "NNEUtilsLogHelper.h"
#include "NNEUtilsModelOptimizer.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIGPUReadback.h"
#include "Serialization/MemoryReader.h"

namespace UE::NNERuntimeRDG::Private
{

//
//
//
bool AlwaysValidValidationFunction(
	const NNECore::FAttributeMap& AttributeMap, 
	TConstArrayView<ENNETensorDataType> InputTensorTypes,
	TConstArrayView<NNECore::FSymbolicTensorShape> InputShapes)
{
	return true;
}


//
//
//
FInputValidator::FInputValidator() : 
	NumRequiredInput(0), NumOptionalInput(0)
{
	TemplateTypes.SetNum(1);
}

bool FInputValidator::Validate(TConstArrayView<ENNETensorDataType> InputTypes)
{
	check(InputTemplateIndices.Num() == NumRequiredInput + NumOptionalInput);

	bool bAreInputValid = true;
	int32 NumInputsToValidate = FMath::Min(InputTemplateIndices.Num(), InputTypes.Num());
	
	if (InputTypes.Num() < NumRequiredInput)
	{
		UE_LOG(LogNNE, Warning, TEXT("Required '%d' inputs but found '%d'."), NumRequiredInput, InputTypes.Num());
		bAreInputValid = false;
	}
	if (InputTypes.Num() > NumRequiredInput+NumOptionalInput)
	{
		UE_LOG(LogNNE, Warning, TEXT("Got a total of '%d' inputs but should have '%d' maximum."), InputTypes.Num(), NumRequiredInput + NumOptionalInput);
		bAreInputValid = false;
	}
	
	for (int32 Idx = 0; Idx < NumInputsToValidate; ++Idx)
	{
		const int32 TemplateIdx = InputTemplateIndices[Idx];
		
		check(TemplateIdx < TemplateTypes.Num());
		if (INDEX_NONE == TemplateTypes[TemplateIdx].Find(InputTypes[Idx]))
		{
			FString TargetType = NNEUtils::Internal::GetTensorDataTypeName(InputTypes[Idx]);
			UE_LOG(LogNNE, Warning, TEXT("Input at index '%d' (from template T%d) is of type '%s' witch is not supported for that input."), Idx, TemplateIdx, *TargetType);
			bAreInputValid = false;
		}
	}
	return bAreInputValid;
}
void FInputValidator::SetTemplateCount(int TemplateCount)
{
	TemplateTypes.SetNum(TemplateCount);
}
void FInputValidator::AddSupportedType(ENNETensorDataType Type, int TemplateIdx)
{
	check(TemplateTypes.Num() > TemplateIdx);
	TemplateTypes[TemplateIdx].Add(Type);
}
void FInputValidator::AddOptional(int32 TemplateIdx)
{
	InputTemplateIndices.Add(TemplateIdx);
	++NumOptionalInput;
}

void FInputValidator::AddRequired(int32 TemplateIdx)
{
	checkf(NumOptionalInput==0, TEXT("All required attribute should be declared before the optional ones as they are referenced by indices"));
	InputTemplateIndices.Add(TemplateIdx);
	++NumRequiredInput;
}


//
//
//
void FAttributeValidator::AddOptional(const FString& Name, ENNEAttributeDataType Type)
{
	checkf(nullptr == OptionalAttributes.FindByPredicate([Name](const FEntry& Other) { return Other.Name == Name; }), TEXT("Attribute name should be unique"));
	checkf(nullptr == RequiredAttributes.FindByPredicate([Name](const FEntry& Other) { return Other.Name == Name; }), TEXT("Attribute name should be unique"));
	OptionalAttributes.Emplace(Name, Type);
}

void FAttributeValidator::AddRequired(const FString& Name, ENNEAttributeDataType Type)
{
	checkf(nullptr == OptionalAttributes.FindByPredicate([Name](const FEntry& Other) { return Other.Name == Name; }), TEXT("Attribute name should be unique"));
	checkf(nullptr == RequiredAttributes.FindByPredicate([Name](const FEntry& Other) { return Other.Name == Name; }), TEXT("Attribute name should be unique"));
	RequiredAttributes.Emplace(Name, Type);
}

bool FAttributeValidator::Validate(const NNECore::FAttributeMap& AttributesToValidate)
{
	bool bAreAttributesValid = true;

	//Verify all required attribute are matching specifications
	for (int32 Idx = 0; Idx < RequiredAttributes.Num(); ++Idx)
	{
		const FNNEAttributeValue* FoundAttribute = AttributesToValidate.GetAttributeValue(RequiredAttributes[Idx].Name);
		
		if (FoundAttribute == nullptr)
		{
			bAreAttributesValid = false;
			UE_LOG(LogNNE, Warning, TEXT("Required attribute '%s' not found."),
				*RequiredAttributes[Idx].Name);
		}
		else if (RequiredAttributes[Idx].Type != FoundAttribute->GetType())
		{
			bAreAttributesValid = false;
			UE_LOG(LogNNE, Warning, TEXT("Required attribute '%s' type '%d' does not match expected type '%d'."),
				*RequiredAttributes[Idx].Name,
				(int)FoundAttribute->GetType(),
				(int)RequiredAttributes[Idx].Type);
		}
	}

	//Verify all optional attribute are matching specifications
	for (int32 Idx = 0; Idx < OptionalAttributes.Num(); ++Idx)
	{
		const FNNEAttributeValue* FoundAttribute = AttributesToValidate.GetAttributeValue(OptionalAttributes[Idx].Name);
		
		if ((FoundAttribute != nullptr) && (OptionalAttributes[Idx].Type != FoundAttribute->GetType()))
		{
			bAreAttributesValid = false;
			UE_LOG(LogNNE, Warning, TEXT("Optional attribute '%s' type '%d' does not match expected type '%d'."),
				*OptionalAttributes[Idx].Name,
				(int)FoundAttribute->GetType(),
				(int)OptionalAttributes[Idx].Type);
		}
	}

	//Verify all attributes are either required or optional, otherwise they are unsupported
	for (int32 Idx = 0; Idx < AttributesToValidate.Num(); ++Idx)
	{
		const FString& Name = AttributesToValidate.GetName(Idx);
		const FEntry* OptionalAttribute = OptionalAttributes.FindByPredicate([Name](const FEntry& Other) { return Other.Name == Name; });
		const FEntry* RequiredAttribute = RequiredAttributes.FindByPredicate([Name](const FEntry& Other) { return Other.Name == Name; });
		
		if (OptionalAttribute == nullptr && RequiredAttribute == nullptr)
		{
			bAreAttributesValid = false;
			UE_LOG(LogNNE, Warning, TEXT("Found unsupported attribute '%s'."), *Name);
		}
	}

	return bAreAttributesValid;
}

} // UE::NNERuntimeRDG::Private
