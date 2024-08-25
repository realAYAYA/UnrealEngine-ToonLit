// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGraphPinParamName.h"
#include "EditorUtils.h"
#include "SParameterPickerCombo.h"
#include "ScopedTransaction.h"
#include "UncookedOnlyUtils.h"
#include "Param/ParamCompatibility.h"
#include "Param/ParamUtils.h"
#include "Param/RigVMDispatch_GetLayerParameter.h"
#include "Param/RigVMDispatch_GetParameter.h"
#include "Param/RigVMDispatch_SetLayerParameter.h"

#define LOCTEXT_NAMESPACE "SGraphPinParamName"

namespace UE::AnimNext::Editor
{

void SGraphPinParamName::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	ModelPin = InArgs._ModelPin;
	Node = InArgs._GraphNode;
	FilterType = InArgs._FilterType;

	SGraphPin::Construct(SGraphPin::FArguments(), InPin);
}

TSharedRef<SWidget> SGraphPinParamName::GetDefaultValueWidget()
{
	FParameterPickerArgs Args;
	Args.bShowBlocks = false;
	Args.bMultiSelect = false;

	CachedType = UncookedOnly::FUtils::GetParameterTypeFromName(FName(*GraphPinObj->DefaultValue));

	// Check whether this is a Set/Get parameter from block node, and if so only show bound parameters
	if (ModelPin)
	{
		if (const URigVMTemplateNode* TemplateNode = Cast<URigVMTemplateNode>(ModelPin->GetOuter()))
		{
			const FRigVMDispatchFactory* GetLayerParameterFactory = FRigVMRegistry::Get().FindOrAddDispatchFactory(FRigVMDispatch_GetLayerParameter::StaticStruct());
			const FName GetLayerParameterNotation = GetLayerParameterFactory->GetTemplate()->GetNotation();

			const FRigVMDispatchFactory* SetLayerParameterFactory = FRigVMRegistry::Get().FindOrAddDispatchFactory(FRigVMDispatch_SetLayerParameter::StaticStruct());
			const FName SetLayerParameterNotation = SetLayerParameterFactory->GetTemplate()->GetNotation();

			if (TemplateNode->GetNotation() == GetLayerParameterNotation || TemplateNode->GetNotation() == SetLayerParameterNotation)
			{
				Args.bShowUnboundParameters = false;
			}
		}
	}
	
	Args.OnParameterPicked = FOnParameterPicked::CreateLambda([this](const FParameterBindingReference& InParameterBinding)
	{
		FScopedTransaction Transaction(LOCTEXT("SelectParameter", "Select Parameter"));
		GraphPinObj->Modify();
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, InParameterBinding.Parameter.ToString());

		CachedType = UncookedOnly::FUtils::GetParameterTypeFromName(FName(*GraphPinObj->DefaultValue));
	});
	
	Args.OnFilterParameterType = FOnFilterParameterType::CreateLambda([this](const FAnimNextParamType& InParamType)-> EFilterParameterResult
	{
		if(FilterType.IsValid())
		{
			return FParamUtils::GetCompatibility(FilterType, InParamType).IsCompatible() ? EFilterParameterResult::Include : EFilterParameterResult::Exclude;
		}
		else if(Node && ModelPin->IsLinked())
		{
			const FAnimNextParamType Type = FAnimNextParamType::FromRigVMTemplateArgument(ModelPin->GetTemplateArgumentType());
			return Type.IsValid() && FParamUtils::GetCompatibility(Type, InParamType).IsCompatible() ? EFilterParameterResult::Include : EFilterParameterResult::Exclude;
		}

		return EFilterParameterResult::Include;
	});
	Args.NewParameterType = FilterType;
	
	return SNew(SParameterPickerCombo)
		.PickerArgs(Args)
		.OnGetParameterName_Lambda([this]()
		{
			return FName(*GraphPinObj->DefaultValue);
		})
		.OnGetParameterType_Lambda([this]()
		{
			return CachedType;
		});
}

}

#undef LOCTEXT_NAMESPACE