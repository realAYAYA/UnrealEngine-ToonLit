// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/MaterialStageExpressions/DMMSETextureSampleEdgeColor.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialSubStage.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialStageInputs/DMMSIExpression.h"
#include "Components/MaterialStageInputs/DMMSIValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat2.h"
#include "DMPrivate.h"
#include "Materials/MaterialExpressionTextureSample.h"

#define LOCTEXT_NAMESPACE "DMMaterialStageExpressionTextureSample"

namespace UE::DynamicMaterialEditor::TextureSampleEdgeColor::Private
{
	constexpr float EdgeGap = 0.025;
}

UDMMaterialStageExpressionTextureSampleEdgeColor::UDMMaterialStageExpressionTextureSampleEdgeColor()
	: UDMMaterialStageExpression(
		LOCTEXT("EdgeColor", "Edge Color"),
		UMaterialExpressionTextureSample::StaticClass()
	)
{
	bInputRequired = true;
	bAllowNestedInputs = true;

	InputConnectors.Add({1, LOCTEXT("Texture", "Texture"), EDMValueType::VT_Texture});
	InputConnectors.Add({0, LOCTEXT("Location", "Location"), EDMValueType::VT_Float2});

	using namespace UE::DynamicMaterialEditor::TextureSampleEdgeColor::Private;

	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionTextureSampleEdgeColor, EdgeLocation));
	EdgeLocation = EDMEdgeLocation::TopLeft;
	
	OutputConnectors.Add({0, LOCTEXT("ColorRGB", "Color (RGB)"), EDMValueType::VT_Float3_RGB});
}

bool UDMMaterialStageExpressionTextureSampleEdgeColor::CanChangeInputType(int32 InInputIndex) const
{
	return false;
}

bool UDMMaterialStageExpressionTextureSampleEdgeColor::IsInputVisible(int32 InInputIndex) const
{
	if (InInputIndex == 1 && EdgeLocation != EDMEdgeLocation::Custom)
	{
		return false;
	}

	return Super::IsInputVisible(InInputIndex);
}

void UDMMaterialStageExpressionTextureSampleEdgeColor::AddDefaultInput(int32 InInputIndex) const
{
	UDMMaterialStage* Stage = GetStage();
	check(Stage);

	if (InInputIndex == 1)
	{
		using namespace UE::DynamicMaterialEditor::TextureSampleEdgeColor::Private;

		UDMMaterialStageInputValue* InputValue = UDMMaterialStageInputValue::ChangeStageInput_NewLocalValue(Stage, 
			1, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL,
			EDMValueType::VT_Float2, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL);
		check(InputValue);

		UDMMaterialValueFloat2* InputFloat2 = Cast<UDMMaterialValueFloat2>(InputValue->GetValue());
		check(InputFloat2);

		InputFloat2->SetDefaultValue(FVector2D(EdgeGap, EdgeGap));
		InputFloat2->ApplyDefaultValue();
		return;
	}

	return Super::AddDefaultInput(InInputIndex);
}

void UDMMaterialStageExpressionTextureSampleEdgeColor::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	static const FName EdgeLocationName = GET_MEMBER_NAME_CHECKED(UDMMaterialStageExpressionTextureSampleEdgeColor, EdgeLocation);

	if (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == EdgeLocationName)
	{
		OnEdgeLocationChanged();
	}
}

void UDMMaterialStageExpressionTextureSampleEdgeColor::OnEdgeLocationChanged()
{
	if (EdgeLocation == EDMEdgeLocation::Custom)
	{
		return;
	}

	using namespace UE::DynamicMaterialEditor::TextureSampleEdgeColor::Private;

	UDMMaterialStage* Stage = GetStage();
	check(Stage);
	check(Stage->GetInputs().IsValidIndex(1));

	UDMMaterialStageInputValue* InputValue = Cast<UDMMaterialStageInputValue>(Stage->GetInputs()[1]);
	check(InputValue);
	
	UDMMaterialValueFloat2* Float2 = Cast<UDMMaterialValueFloat2>(InputValue->GetValue());
	check(Float2);

	FVector2D Float2Value = FVector2D::ZeroVector;
	
	switch (EdgeLocation)
	{
		case EDMEdgeLocation::TopLeft:
			Float2Value.X = EdgeGap;
			Float2Value.Y = EdgeGap;
			break;

		case EDMEdgeLocation::Top:
			Float2Value.X = 0.5;
			Float2Value.Y = EdgeGap;
			break;

		case EDMEdgeLocation::TopRight:
			Float2Value.X = 1.f - EdgeGap;
			Float2Value.Y = EdgeGap;
			break;

		case EDMEdgeLocation::Left:
			Float2Value.X = EdgeGap;
			Float2Value.Y = 0.5;
			break;

		case EDMEdgeLocation::Center:
			Float2Value.X = 0.5;
			Float2Value.Y = 0.5;
			break;

		case EDMEdgeLocation::Right:
			Float2Value.X = 1.f - EdgeGap;
			Float2Value.Y = 0.5;
			break;

		case EDMEdgeLocation::BottomLeft:
			Float2Value.X = EdgeGap;
			Float2Value.Y = 1.f - EdgeGap;
			break;

		case EDMEdgeLocation::Bottom:
			Float2Value.X = 0.5;
			Float2Value.Y = 1.f - EdgeGap;
			break;

		case EDMEdgeLocation::BottomRight:
			Float2Value.X = 1.f - EdgeGap;
			Float2Value.Y = 1.f - EdgeGap;
			break;

		default:
			checkNoEntry();
			break;
	}

	Float2->SetValue(Float2Value);
}

#undef LOCTEXT_NAMESPACE
