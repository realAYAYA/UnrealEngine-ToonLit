// Copyright Epic Games, Inc. All Rights Reserved.


#include "Graph/SControlRigGraphPinCurveFloat.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "ControlRigBlueprint.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "EdGraphSchema_K2.h"
#include "ScopedTransaction.h"
#include "DetailLayoutBuilder.h"
#include "PropertyPathHelpers.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMModel/RigVMController.h"

#include "IControlRigEditorModule.h"

void SControlRigGraphPinCurveFloat::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SControlRigGraphPinCurveFloat::GetDefaultValueWidget()
{
	UControlRigGraph* RigGraph = Cast<UControlRigGraph>(GraphPinObj->GetOwningNode()->GetGraph());

	// 360 is the minimum width required to display keys' values 
	TSharedRef<SWidget> Widget = SNew(SBox)
		.MinDesiredWidth(360)
		.MaxDesiredWidth(400)
		.MinDesiredHeight(175)
		.MaxDesiredHeight(300)
		[
			SAssignNew(CurveEditor, SCurveEditor)
			.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
			.ViewMinInput(0.f)
			.ViewMaxInput(1.f)
			.ViewMinOutput(0.f)
			.ViewMaxOutput(1.f)
			.TimelineLength(1.f)
			.DesiredSize(FVector2D(360, 200))
			.HideUI(true)
		];

	CurveEditor->SetCurveOwner(this);

	return Widget;
}

TArray<FRichCurveEditInfoConst> SControlRigGraphPinCurveFloat::GetCurves() const
{
	TArray<FRichCurveEditInfoConst> Curves;
	Curves.Add(Curve.GetRichCurveConst());
	return Curves;
}

TArray<FRichCurveEditInfo> SControlRigGraphPinCurveFloat::GetCurves()
{
	TArray<FRichCurveEditInfo> Curves;
	Curves.Add(UpdateAndGetCurve().GetRichCurve());
	return Curves;
}

FRuntimeFloatCurve& SControlRigGraphPinCurveFloat::UpdateAndGetCurve()
{
	if (UEdGraphPin* Pin = GetPinObj())
	{
		if (UControlRigGraphNode* RigNode = Cast<UControlRigGraphNode>(Pin->GetOwningNode()))
		{
			if (URigVMPin* ModelPin = RigNode->GetModelPinFromPinPath(Pin->GetName()))
			{
				FString TextToImport = ModelPin->GetDefaultValue();
				FRuntimeFloatCurve::StaticStruct()->ImportText(*TextToImport, &Curve, nullptr, EPropertyPortFlags::PPF_None, nullptr, FRuntimeFloatCurve::StaticStruct()->GetName(), true);
			}
		}
	}
	return Curve;
}


void SControlRigGraphPinCurveFloat::ModifyOwner()
{
	if (UEdGraphPin* Pin = GetPinObj())
	{
		Pin->Modify();
	}
}

TArray<const UObject*> SControlRigGraphPinCurveFloat::GetOwners() const
{
	TArray<const UObject*> Owners;
	if (UEdGraphPin* Pin = GetPinObj())
	{
		if (UEdGraphNode* Node = Pin->GetOwningNode())
		{
			Owners.Add(Node);
		}
	}
	return Owners;
}

void SControlRigGraphPinCurveFloat::MakeTransactional()
{
}

bool SControlRigGraphPinCurveFloat::IsValidCurve(FRichCurveEditInfo CurveInfo)
{
	if (UEdGraphPin* Pin = GetPinObj())
	{
		if (UControlRigGraphNode* Node = Cast<UControlRigGraphNode>(Pin->GetOwningNode()))
		{
			if (URigVMUnitNode* StructModelNode = Cast<URigVMUnitNode>(Node->GetModelNode()))
			{
				FString NodeName, PropertyName;
				Pin->PinName.ToString().Split(TEXT("."), &NodeName, &PropertyName);

				if (FProperty* CurveProperty = StructModelNode->GetScriptStruct()->FindPropertyByName(*PropertyName))
				{
					return true;
				}
			}
		}
	}
	return false;
}

void SControlRigGraphPinCurveFloat::OnCurveChanged(const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos)
{
	if (UEdGraphPin* Pin = GetPinObj())
	{
		if (UControlRigGraphNode* Node = Cast<UControlRigGraphNode>(Pin->GetOwningNode()))
		{
			FString ExportedText;
			FRuntimeFloatCurve DefaultCurve;
			FRuntimeFloatCurve::StaticStruct()->ExportText(ExportedText, &Curve, &DefaultCurve, nullptr, EPropertyPortFlags::PPF_None, nullptr, true);
			Node->GetController()->SetPinDefaultValue(Pin->GetName(), ExportedText, true, true, true);
		}
	}
	ModifyOwner();
}
