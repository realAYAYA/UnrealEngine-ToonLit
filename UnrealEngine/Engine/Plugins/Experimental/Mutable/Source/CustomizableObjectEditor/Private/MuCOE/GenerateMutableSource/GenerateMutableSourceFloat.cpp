// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/GenerateMutableSource/GenerateMutableSourceFloat.h"

#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/EdGraphSchema_CustomizableObject.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceImage.h"
#include "MuCOE/GenerateMutableSource/GenerateMutableSourceTable.h"
#include "MuCOE/GraphTraversal.h"
#include "MuCOE/Nodes/CustomizableObjectNodeCurve.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEnumParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatConstant.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatParameter.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatSwitch.h"
#include "MuCOE/Nodes/CustomizableObjectNodeFloatVariation.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuT/NodeScalarConstant.h"
#include "MuT/NodeScalarCurve.h"
#include "MuT/NodeScalarSwitch.h"
#include "MuT/NodeScalarTable.h"
#include "MuT/NodeScalarVariation.h"

#define LOCTEXT_NAMESPACE "CustomizableObjectEditor"


void SetCurveNodeValues(mu::NodeScalarCurvePtr& CurveNode, const FRichCurve& Curve)
{
	if (Curve.HasAnyData())
	{
		CurveNode->SetDefaultValue(Curve.GetDefaultValue());
		CurveNode->SetKeyFrameCount(Curve.GetNumKeys());
		int32 KeyNum = 0;

		for (auto Itr = Curve.GetKeyIterator(); Itr; ++Itr)
		{
			CurveNode->SetKeyFrame(KeyNum,
				Itr->Time,
				Itr->Value,
				Itr->ArriveTangent,
				Itr->ArriveTangentWeight,
				Itr->LeaveTangent,
				Itr->LeaveTangentWeight,
				Itr->InterpMode,
				Itr->TangentMode,
				Itr->TangentWeightMode);

			KeyNum++;
		}
	}
	else
	{
		CurveNode->SetDefaultValue(0.0f); // FRichCurve default DefaultValue is MAX_flt
	}
}


mu::NodeScalarPtr GenerateMutableSourceFloat(const UEdGraphPin* Pin, FMutableGraphGenerationContext& GenerationContext)
{
	check(Pin)
	RETURN_ON_CYCLE(*Pin, GenerationContext)

	CheckNumOutputs(*Pin, GenerationContext);
	
	const UEdGraphSchema_CustomizableObject* Schema = GetDefault<UEdGraphSchema_CustomizableObject>();

	UCustomizableObjectNode* Node = CastChecked<UCustomizableObjectNode>(Pin->GetOwningNode());

	const FGeneratedKey Key(reinterpret_cast<void*>(&GenerateMutableSourceFloat), *Pin, *Node, GenerationContext);
	if (const FGeneratedData* Generated = GenerationContext.Generated.Find(Key))
	{
		return static_cast<mu::NodeScalar*>(Generated->Node.get());
	}

	bool bDoNotAddToGeneratedCache = false;

	mu::NodeScalarPtr Result;
	
	if (const UCustomizableObjectNodeFloatConstant* FloatConstantNode = Cast<UCustomizableObjectNodeFloatConstant>(Node))
	{
		mu::NodeScalarConstantPtr ScalarNode = new mu::NodeScalarConstant();
		Result = ScalarNode;

		ScalarNode->SetValue(FloatConstantNode->Value);
	}

	else if (const UCustomizableObjectNodeFloatParameter* FloatParameterNode = Cast<UCustomizableObjectNodeFloatParameter>(Node))
	{
		mu::NodeScalarParameterPtr ScalarNode = new mu::NodeScalarParameter();
		Result = ScalarNode;

		GenerationContext.AddParameterNameUnique(Node, FloatParameterNode->ParameterName);

		ScalarNode->SetName(StringCast<ANSICHAR>(*FloatParameterNode->ParameterName).Get());
		ScalarNode->SetUid(StringCast<ANSICHAR>(*GenerationContext.GetNodeIdUnique(Node).ToString()).Get());
		ScalarNode->SetDefaultValue(FloatParameterNode->DefaultValue);

		GenerationContext.ParameterUIDataMap.Add(FloatParameterNode->ParameterName, FParameterUIData(
			FloatParameterNode->ParameterName,
			FloatParameterNode->ParamUIMetadata,
			EMutableParameterType::Float));
	}

	else if (const UCustomizableObjectNodeEnumParameter* EnumParamNode = Cast<UCustomizableObjectNodeEnumParameter>(Node))
	{
		mu::NodeScalarEnumParameterPtr EnumParameterNode = new mu::NodeScalarEnumParameter;

		const int32 NumSelectors = EnumParamNode->Values.Num();

		int32 DefaultValue = FMath::Clamp(EnumParamNode->DefaultIndex, 0, NumSelectors - 1);

		GenerationContext.AddParameterNameUnique(Node, EnumParamNode->ParameterName);

		EnumParameterNode->SetName(StringCast<ANSICHAR>(*EnumParamNode->ParameterName).Get());
		EnumParameterNode->SetUid(StringCast<ANSICHAR>(*GenerationContext.GetNodeIdUnique(Node).ToString()).Get());
		EnumParameterNode->SetValueCount(NumSelectors);
		EnumParameterNode->SetDefaultValueIndex(DefaultValue);

		FParameterUIData ParameterUIData(EnumParamNode->ParameterName, EnumParamNode->ParamUIMetadata, EMutableParameterType::Int);
		ParameterUIData.IntegerParameterGroupType = ECustomizableObjectGroupType::COGT_ONE;

		for (int SelectorIndex = 0; SelectorIndex < NumSelectors; ++SelectorIndex)
		{
			EnumParameterNode->SetValue(SelectorIndex, (float)SelectorIndex, StringCast<ANSICHAR>(*EnumParamNode->Values[SelectorIndex].Name).Get());

			ParameterUIData.ArrayIntegerParameterOption.Add(FIntegerParameterUIData(
				EnumParamNode->Values[SelectorIndex].Name,
				EnumParamNode->Values[SelectorIndex].ParamUIMetadata));
		}

		Result = EnumParameterNode;

		GenerationContext.ParameterUIDataMap.Add(EnumParamNode->ParameterName, ParameterUIData);
	}

	else if (const UCustomizableObjectNodeFloatSwitch* TypedNodeFloatSwitch = Cast<UCustomizableObjectNodeFloatSwitch>(Node))
	{
		// Using a lambda so control flow is easier to manage.
		Result = [&]()
		{
			const UEdGraphPin* SwitchParameter = TypedNodeFloatSwitch->SwitchParameter();

			// Check Switch Parameter arity preconditions.
			if (const int32 NumParameters = FollowInputPinArray(*SwitchParameter).Num();
				NumParameters != 1)
			{
				const FText Message = NumParameters
					? LOCTEXT("NoEnumParamInSwitch", "Switch nodes must have an enum switch parameter. Please connect an enum and refesh the switch node.")
					: LOCTEXT("InvalidEnumInSwitch", "Switch nodes must have a single enum with all the options inside. Please remove all the enums but one and refresh the switch node.");

				GenerationContext.Compiler->CompilerLog(Message, Node);
				return Result;
			}

			const UEdGraphPin* EnumPin = FollowInputPin(*SwitchParameter);
			mu::NodeScalarPtr SwitchParam = GenerateMutableSourceFloat(EnumPin, GenerationContext);

			// Switch Param not generated
			if (!SwitchParam)
			{
				// Warn about a failure.
				if (EnumPin)
				{
					const FText Message = LOCTEXT("FailedToGenerateSwitchParam", "Could not generate switch enum parameter. Please refesh the switch node and connect an enum.");
					GenerationContext.Compiler->CompilerLog(Message, Node);
				}

				return Result;
			}

			if (SwitchParam->GetType() != mu::NodeScalarEnumParameter::GetStaticType())
			{
				const FText Message = LOCTEXT("WrongSwitchParamType", "Switch parameter of incorrect type.");
				GenerationContext.Compiler->CompilerLog(Message, Node);

				return Result;
			}

			const int32 NumSwitchOptions = TypedNodeFloatSwitch->GetNumElements();

			mu::NodeScalarEnumParameter* EnumParameter = static_cast<mu::NodeScalarEnumParameter*>(SwitchParam.get());
			if (NumSwitchOptions != EnumParameter->GetValueCount())
			{
				const FText Message = LOCTEXT("MismatchedSwitch", "Switch enum and switch node have different number of options. Please refresh the switch node to make sure the outcomes are labeled properly.");
				GenerationContext.Compiler->CompilerLog(Message, Node);
			}

			mu::NodeScalarSwitchPtr SwitchNode = new mu::NodeScalarSwitch;
			SwitchNode->SetParameter(SwitchParam);
			SwitchNode->SetOptionCount(NumSwitchOptions);

			for (int SelectorIndex = 0; SelectorIndex < NumSwitchOptions; ++SelectorIndex)
			{
				if (const UEdGraphPin* const FloatPin = TypedNodeFloatSwitch->GetElementPin(SelectorIndex))
				{
					if (const UEdGraphPin* ConnectedPin = FollowInputPin(*FloatPin))
					{
						SwitchNode->SetOption(SelectorIndex, GenerateMutableSourceFloat(ConnectedPin, GenerationContext));
					}
				}
			}

			Result = SwitchNode;
			return Result;
		}(); // invoke lambda.
	}

	else if (const UCustomizableObjectNodeCurve* TypedNodeCurve = Cast<UCustomizableObjectNodeCurve>(Node))
	{
		mu::NodeScalarCurvePtr CurveNode = new mu::NodeScalarCurve();
		Result = CurveNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeCurve->InputPin()))
		{
			mu::NodeScalarPtr ScalarNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			CurveNode->SetT(ScalarNode);
		}

		if (UCurveBase* CurveAsset = TypedNodeCurve->CurveAsset)
		{
			int32 PinIndex = -1;

			for (int32 i = 0; i < TypedNodeCurve->GetNumCurvePins(); ++i)
			{
				if (TypedNodeCurve->CurvePins(i) == Pin)
				{
					PinIndex = i;
					break;
				}
			}

			if (const UCurveLinearColor* const CurveColor = Cast<UCurveLinearColor>(CurveAsset))
			{
				if (PinIndex >= 0 && PinIndex <= 3)
				{
					const FRichCurve& Curve = CurveColor->FloatCurves[PinIndex];
					SetCurveNodeValues(CurveNode, Curve);
				}
			}
			else if (const UCurveVector* const CurveVector = Cast<UCurveVector>(CurveAsset))
			{
				if (PinIndex >= 0 && PinIndex <= 2)
				{
					const FRichCurve& Curve = CurveVector->FloatCurves[PinIndex];
					SetCurveNodeValues(CurveNode, Curve);
				}
			}
			else if (const UCurveFloat* const CurveFloat = Cast<UCurveFloat>(CurveAsset))
			{
				if (PinIndex == 0)
				{
					const FRichCurve& Curve = CurveFloat->FloatCurve;
					SetCurveNodeValues(CurveNode, Curve);
				}
			}
		}
	}

	else if (const UCustomizableObjectNodeFloatVariation* TypedNodeFloatVar = Cast<const UCustomizableObjectNodeFloatVariation>(Node))
	{
		mu::NodeScalarVariationPtr FloatNode = new mu::NodeScalarVariation();
		Result = FloatNode;

		if (const UEdGraphPin* ConnectedPin = FollowInputPin(*TypedNodeFloatVar->DefaultPin()))
		{
			mu::NodeScalarPtr ChildNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
			if (ChildNode)
			{
				FloatNode->SetDefaultScalar(ChildNode.get());
			}
			else
			{
				GenerationContext.Compiler->CompilerLog(LOCTEXT("FloatFailed", "Float generation failed."), Node);
			}
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("FloatVarMissingDef", "Float variation node requires a default value."), Node);
		}

		FloatNode->SetVariationCount(TypedNodeFloatVar->Variations.Num());
		for (int VariationIndex = 0; VariationIndex < TypedNodeFloatVar->Variations.Num(); ++VariationIndex)
		{
			UEdGraphPin* VariationPin = TypedNodeFloatVar->VariationPin(VariationIndex);
			if (!VariationPin) continue;

			FloatNode->SetVariationTag(VariationIndex, StringCast<ANSICHAR>(*TypedNodeFloatVar->Variations[VariationIndex].Tag).Get());
			if (const UEdGraphPin* ConnectedPin = FollowInputPin(*VariationPin))
			{
				mu::NodeScalarPtr ChildNode = GenerateMutableSourceFloat(ConnectedPin, GenerationContext);
				FloatNode->SetVariationScalar(VariationIndex, ChildNode.get());
			}
		}
	}

	else if (const UCustomizableObjectNodeTable* TypedNodeTable = Cast<UCustomizableObjectNodeTable>(Node))
	{
		//This node will add a default value in case of error
		mu::NodeScalarConstantPtr ConstantValue = new mu::NodeScalarConstant();
		ConstantValue->SetValue(1.0f);

		Result = ConstantValue;

		if (Pin->PinType.PinCategory == Schema->PC_MaterialAsset)
		{
			// Material pins have to skip the cache of nodes or they will return always the same column node
			bDoNotAddToGeneratedCache = true;
		}

		bool bSuccess = true;

		if (TypedNodeTable->Table)
		{
			FString ColumnName = Pin->PinFriendlyName.ToString();
			FProperty* Property = TypedNodeTable->Table->FindTableProperty(FName(*ColumnName));

			if (!Property)
			{
				FString Msg = FString::Printf(TEXT("Couldn't find the column [%s] in the data table's struct."), *ColumnName);
				GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);

				bSuccess = false;
			}

			if (bSuccess)
			{
				// Generating a new data table if not exists
				mu::TablePtr Table;
				Table = GenerateMutableSourceTable(TypedNodeTable->Table->GetName(), Pin, GenerationContext);

				if (Table)
				{
					mu::NodeScalarTablePtr ScalarTableNode = new mu::NodeScalarTable();

					if (Pin->PinType.PinCategory == Schema->PC_MaterialAsset)
					{
						// Materials use the parameter id as column names
						ColumnName = GenerationContext.CurrentMaterialTableParameterId;
					}

					// Generating a new Float column if not exists
					if (Table->FindColumn(StringCast<ANSICHAR>(*ColumnName).Get()) == INDEX_NONE)
					{
						int32 Dummy = -1; // TODO MTBL-1512
						bool Dummy2 = false;
						bSuccess = GenerateTableColumn(TypedNodeTable, Pin, Table, ColumnName, Property, Dummy, Dummy, GenerationContext.CurrentLOD, Dummy, Dummy2, GenerationContext);

						if (!bSuccess)
						{
							FString Msg = FString::Printf(TEXT("Failed to generate the mutable table column [%s]"), *ColumnName);
							GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);
						}
					}

					if (bSuccess)
					{
						Result = ScalarTableNode;

						ScalarTableNode->SetTable(Table);
						ScalarTableNode->SetColumn(StringCast<ANSICHAR>(*ColumnName).Get());
						ScalarTableNode->SetParameterName(StringCast<ANSICHAR>(*TypedNodeTable->ParameterName).Get());

						GenerationContext.AddParameterNameUnique(Node, TypedNodeTable->ParameterName);
					}
				}
				else
				{
					FString Msg = FString::Printf(TEXT("Couldn't generate a mutable table."), *ColumnName);
					GenerationContext.Compiler->CompilerLog(FText::FromString(Msg), Node);
				}
			}
		}
		else
		{
			GenerationContext.Compiler->CompilerLog(LOCTEXT("ScalarTableError", "Couldn't find the data table of the node."), Node);
		}
	}

	else
	{
		GenerationContext.Compiler->CompilerLog(LOCTEXT("UnimplementedNode", "Node type not implemented yet."), Node);
	}

	if (!bDoNotAddToGeneratedCache)
	{
		GenerationContext.Generated.Add(Key, FGeneratedData(Node, Result));
		GenerationContext.GeneratedNodes.Add(Node);
	}

	if (Result)
	{
		Result->SetMessageContext(Node);
	}

	return Result;
}

#undef LOCTEXT_NAMESPACE

