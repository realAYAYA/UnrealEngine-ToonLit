// Copyright Epic Games, Inc. All Rights Reserved.

#include "SStylusInputDebugWidget.h"

#include "IStylusState.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SCheckBox.h"

#define LOCTEXT_NAMESPACE "StylusInputDebugWidget"

SStylusInputDebugWidget::SStylusInputDebugWidget()
{
}

SStylusInputDebugWidget::~SStylusInputDebugWidget()
{
	InputSubsystem->RemoveMessageHandler(*this);
}

void SStylusInputDebugWidget::Construct(const FArguments& InArgs, UStylusInputSubsystem& InSubsystem)
{
	InputSubsystem = &InSubsystem;
	InputSubsystem->AddMessageHandler(*this);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MostRecentIndex", "Most Recent Index"))
			]
			+ SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(this, &SStylusInputDebugWidget::GetIndexText)
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Position", "Position"))
			]
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(this, &SStylusInputDebugWidget::GetPositionText)
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NormalPressure", "Normal Pressure"))
			]
		+ SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(this, &SStylusInputDebugWidget::GetPressureText)
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
			[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("TangentPressure", "Tangent Pressure"))
			]
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(this, &SStylusInputDebugWidget::GetTangentPressureText)
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Z", "Z"))
			]
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(this, &SStylusInputDebugWidget::GetZText)
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Twist", "Twist"))
			]
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(this, &SStylusInputDebugWidget::GetTwistText)
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Tilt", "Tilt"))
			]
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(this, &SStylusInputDebugWidget::GetTiltText)
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Size", "Size"))
			]
			+SHorizontalBox::Slot()
			.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(this, &SStylusInputDebugWidget::GetSizeText)
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("IsTouching", "Is Touching?"))
			]
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SStylusInputDebugWidget::IsTouching)
			]
		]
		+SVerticalBox::Slot()
			.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("IsInverted", "Is Inverted?"))
			]
			+SHorizontalBox::Slot()
				.FillWidth(1)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SStylusInputDebugWidget::IsInverted)	
			]
		]
	];
}

FText SStylusInputDebugWidget::GetVector2Text(FVector2D Value)
{
	return FText::FromString(FString::Format(TEXT("{0}, {1}"), { Value.X, Value.Y }));
}

FText SStylusInputDebugWidget::GetFloatText(float Value)
{
	return FText::FromString(FString::Format(TEXT("{0}"), { Value }));
}

ECheckBoxState SStylusInputDebugWidget::IsTouching() const
{
	return State.IsStylusDown() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState SStylusInputDebugWidget::IsInverted() const
{
	return State.IsStylusInverted() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}


#undef LOCTEXT_NAMESPACE