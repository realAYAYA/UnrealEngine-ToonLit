// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGraphSubstrateMaterial.h"
#include "Internationalization/Text.h"
#include "Rendering/RenderingCommon.h"
#include "Rendering/StrataMaterialShared.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionStrata.h"
#include "SGraphPin.h"
#include "Math/Color.h"
#include "Styling/StyleColors.h"
#include "StrataDefinitions.h"
#include "Widgets/SBoxPanel.h"

enum class ESubstrateWidgetOutputType : uint8
{
	Node,
	DetailPanel,
};

static EStyleColor GetSubstrateWidgetColor0() { return EStyleColor::AccentBlue;  }
static EStyleColor GetSubstrateWidgetColor1() { return EStyleColor::AccentGreen;  }

static const TSharedRef<SWidget> InternalProcessOperator(
	const FStrataMaterialCompilationOutput& CompilationOutput, 
	const FStrataOperator& Op, 
	ESubstrateWidgetOutputType OutputType,
	const FGuid& InGuid, 
	EStyleColor OverrideColor)
{
	const bool bIsCurrent = OutputType == ESubstrateWidgetOutputType::Node ? InGuid == Op.MaterialExpressionGuid : false;
	const EStyleColor Color0 = bIsCurrent ? GetSubstrateWidgetColor0() : OverrideColor;
	const EStyleColor Color1 = bIsCurrent ? GetSubstrateWidgetColor1() : OverrideColor;
	switch (Op.OperatorType)
	{
		case STRATA_OPERATOR_WEIGHT:
			return InternalProcessOperator(CompilationOutput, CompilationOutput.Operators[Op.LeftIndex], OutputType, InGuid, OverrideColor);
			break;
		case STRATA_OPERATOR_VERTICAL:
		{
			auto VerticalOperator = SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(0.0f, 0.0f, 1.0f, 1.0f)
				[
					InternalProcessOperator(CompilationOutput, CompilationOutput.Operators[Op.LeftIndex], OutputType, InGuid, Color0)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(0.0f, 0.0f, 1.0f, 1.0f)
				[
					InternalProcessOperator(CompilationOutput, CompilationOutput.Operators[Op.RightIndex], OutputType, InGuid, Color1)
				];
			return VerticalOperator->AsShared();
		}
		break;
		case STRATA_OPERATOR_HORIZONTAL:
		{
			auto HorizontalOperator = SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(0.0f, 0.0f, 1.0f, 1.0f)
				[
					InternalProcessOperator(CompilationOutput, CompilationOutput.Operators[Op.LeftIndex], OutputType, InGuid, Color0)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(0.0f, 0.0f, 1.0f, 1.0f)
				[
					InternalProcessOperator(CompilationOutput, CompilationOutput.Operators[Op.RightIndex], OutputType, InGuid, Color1)
				];
			return HorizontalOperator->AsShared();
		}
		break;
		case STRATA_OPERATOR_ADD:
		{
			auto HorizontalOperator = SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(0.0f, 0.0f, 1.0f, 1.0f)
				[
					InternalProcessOperator(CompilationOutput, CompilationOutput.Operators[Op.LeftIndex], OutputType, InGuid, Color0)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Fill)
				.HAlign(HAlign_Fill)
				.Padding(0.0f, 0.0f, 1.0f, 1.0f)
				[
					InternalProcessOperator(CompilationOutput, CompilationOutput.Operators[Op.RightIndex], OutputType, InGuid, Color1)
				];
			return HorizontalOperator->AsShared();
		}
		break;
		case STRATA_OPERATOR_BSDF_LEGACY:	// legacy BSDF should have been converted to BSDF already.
		case STRATA_OPERATOR_BSDF:
		{
			FString BSDFDesc = OutputType == ESubstrateWidgetOutputType::Node ? 
											 FString(TEXT("BSDF")) : 
											 FString::Printf(TEXT("BSDF (%s%s%s%s%s%s%s%s)")
											, Op.bBSDFHasEdgeColor ? TEXT("F90 ") : TEXT("")
											, Op.bBSDFHasSSS ? TEXT("SSS ") : TEXT("")
											, Op.bBSDFHasMFPPluggedIn ? TEXT("MFP ") : TEXT("")
											, Op.bBSDFHasAnisotropy ? TEXT("Ani ") : TEXT("")
											, Op.bBSDFHasSecondRoughnessOrSimpleClearCoat ? TEXT("2Ro ") : TEXT("")
											, Op.bBSDFHasFuzz ? TEXT("Fuz ") : TEXT("")
											, Op.bBSDFHasGlint ? TEXT("Gli ") : TEXT("")
											, Op.bBSDFHasSpecularProfile ? TEXT("Spc ") : TEXT("")
											);

			static FString ToolTip;
			if (OutputType == ESubstrateWidgetOutputType::DetailPanel && ToolTip.IsEmpty())
			{
				ToolTip += TEXT("SSS means the BSDF features subsurface profile or subsurface setup using MFP.\n");
				ToolTip += TEXT("MFP means the BSDF MFP is specified by the user.\n");
				ToolTip += TEXT("F90 means the BSDF edge specular color representing reflectivity at grazing angle is used.\n");
				ToolTip += TEXT("Fuz means the BSDF fuzz layer is enabled.\n");
				ToolTip += TEXT("2Ro means the BSDF either uses a second specular lob with a second roughness, or the legacy simple clear coat.\n");
				ToolTip += TEXT("Ani means the BSDF anisotropic specular lighting is used.\n");
				ToolTip += TEXT("Gli means the BSDF features glints.\n");
				ToolTip += TEXT("Spc means the BSDF features specular profile.\n");
			}

			const EStyleColor Color = OverrideColor != EStyleColor::MAX ? OverrideColor : (bIsCurrent ? EStyleColor::AccentGreen : EStyleColor::AccentGray);
			const FSlateColor SlateColor = OutputType == ESubstrateWidgetOutputType::Node ? FSlateColor(Color) : FSlateColor(FLinearColor(0.16, 0.015, 0.24));
			auto BSDF = SNew(SErrorText)
				.ErrorText(FText::FromString(BSDFDesc))
				.BackgroundColor(SlateColor)
				.ToolTipText(FText::FromString(ToolTip));
			return BSDF->AsShared();
		}
		break;
	}

	static FString NoVisualization = OutputType == ESubstrateWidgetOutputType::DetailPanel ? FString(TEXT("Tree Operator Error")) : FString();
	auto TreeOperatorError = SNew(SErrorText)
		.ErrorText(FText::FromString(NoVisualization))
		.BackgroundColor(FSlateColor(EStyleColor::AccentRed));
	return TreeOperatorError->AsShared();
}

const TSharedRef<SWidget> FSubstrateWidget::ProcessOperator(const FStrataMaterialCompilationOutput& CompilationOutput)
{
	return InternalProcessOperator(CompilationOutput, CompilationOutput.Operators[CompilationOutput.RootOperatorIndex], ESubstrateWidgetOutputType::DetailPanel, FGuid(), EStyleColor::MAX);
}

const TSharedRef<SWidget> FSubstrateWidget::ProcessOperator(const FStrataMaterialCompilationOutput& CompilationOutput, const FGuid& InGuid)
{
	return InternalProcessOperator(CompilationOutput, CompilationOutput.Operators[CompilationOutput.RootOperatorIndex], ESubstrateWidgetOutputType::Node, InGuid, EStyleColor::MAX);
}

void FSubstrateWidget::GetPinColor(TSharedPtr<SGraphPin>& Out, const UMaterialGraphNode* InNode)
{	
	const FLinearColor Color0 = USlateThemeManager::Get().GetColor(GetSubstrateWidgetColor0());
	const FLinearColor Color1 = USlateThemeManager::Get().GetColor(GetSubstrateWidgetColor1());

	FLinearColor ColorModifier;
	bool bHasColorModifier = false;
	// Strata operator override pin color to ease material topology visualization
	const FName PinName = Out->GetPinObj()->PinName;
	if (InNode->MaterialExpression->IsA(UMaterialExpressionStrataVerticalLayering::StaticClass()))
	{			
		if (PinName == InNode->MaterialExpression->GetInputName(0)) // Top
		{
			bHasColorModifier = true;
			ColorModifier = Color0;
		}
		else if (PinName == InNode->MaterialExpression->GetInputName(1)) // Base
		{
			bHasColorModifier = true;
			ColorModifier = Color1;
		}
	}
	else if (InNode->MaterialExpression->IsA(UMaterialExpressionStrataHorizontalMixing::StaticClass()))
	{
		if (PinName == InNode->MaterialExpression->GetInputName(1)) // Foreground
		{
			bHasColorModifier = true;
			ColorModifier = Color1;
		}
		else if (PinName == InNode->MaterialExpression->GetInputName(0)) // Background
		{
			bHasColorModifier = true;
			ColorModifier = Color0;
		}
	}
	else if (InNode->MaterialExpression->IsA(UMaterialExpressionStrataAdd::StaticClass()))
	{
		if (PinName == InNode->MaterialExpression->GetInputName(0)) // A
		{
			bHasColorModifier = true;
			ColorModifier = Color0;
		}
		else if (PinName == InNode->MaterialExpression->GetInputName(1)) // B
		{
			bHasColorModifier = true;
			ColorModifier = Color1;
		}
	}

	if (bHasColorModifier)
	{
		Out->SetPinColorModifier(ColorModifier);
	}
}