// Copyright Epic Games, Inc. All Rights Reserved.

#include "CineCameraRigRailDetails.h"
#include "CineCameraRigRail.h"
#include "CameraRig_Rail.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "PropertyHandle.h"
#include "UObject/PropertyIterator.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Internationalization/Internationalization.h"
#include "ScopedTransaction.h"
#include "Layout/Visibility.h"

#define LOCTEXT_NAMESPACE "FCineCameraRigRailDetails"

void FCineCameraRigRailDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.Num() != 1)
	{
		return;
	}

	ACineCameraRigRail* RigRailActor = Cast<ACineCameraRigRail>(Objects[0].Get());
	if(RigRailActor == nullptr)
	{
		return;
	}
	RigRailActorPtr = RigRailActor;

	TSharedRef<IPropertyHandle> AbsolutePositionPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ACineCameraRigRail, AbsolutePositionOnRail));
	TSharedRef<IPropertyHandle> UseAbsolutePositionPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ACineCameraRigRail, bUseAbsolutePosition));
	TSharedRef<IPropertyHandle> OrigPositionPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ACameraRig_Rail, CurrentPositionOnRail), ACameraRig_Rail::StaticClass());
	TSharedRef<IPropertyHandle> LockOrientationPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ACameraRig_Rail, bLockOrientationToRail), ACameraRig_Rail::StaticClass());
	TSharedRef<IPropertyHandle> ShowRailPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ACameraRig_Rail, bShowRailVisualization), ACameraRig_Rail::StaticClass());
	TSharedRef<IPropertyHandle> PreviewScalePropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ACameraRig_Rail, PreviewMeshScale), ACameraRig_Rail::StaticClass());



	const TAttribute<bool> EditCondition = TAttribute<bool>::Create([this, UseAbsolutePositionPropertyHandle]()
	{
		bool bCond = false;
		UseAbsolutePositionPropertyHandle->GetValue(bCond);
		return !bCond;
	});

	DetailBuilder.EditDefaultProperty(LockOrientationPropertyHandle)->Visibility(EVisibility::Hidden);

	IDetailCategoryBuilder& RailControlsCategory = DetailBuilder.EditCategory("Rail Controls");
	RailControlsCategory.AddProperty(UseAbsolutePositionPropertyHandle);
	IDetailPropertyRow& AbsolutePositionRow = RailControlsCategory.AddProperty(AbsolutePositionPropertyHandle);
	RailControlsCategory.AddProperty(OrigPositionPropertyHandle)
	.EditCondition(EditCondition, nullptr);

	AbsolutePositionRow.CustomWidget(false)
		.NameContent()
		[
			AbsolutePositionPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(125.0f)
		[
			SNew(SNumericEntryBox<float>)
			.ToolTipText(LOCTEXT("CurrentPostionToolTip", "Postion property using custom parameterization"))
			.AllowSpin(true)
			.MinSliderValue(this, &FCineCameraRigRailDetails::GetAbsolutePositionSliderMinValue)
			.MaxSliderValue(this, &FCineCameraRigRailDetails::GetAbsolutePositionSliderMaxValue)
			.Value(this, &FCineCameraRigRailDetails::GetAbsolutePosition)
			.OnValueChanged(this, &FCineCameraRigRailDetails::OnAbsolutePositionChanged)
			.OnValueCommitted(this, &FCineCameraRigRailDetails::OnAbsolutePositionCommitted)
			.OnBeginSliderMovement(this, &FCineCameraRigRailDetails::OnBeginAbsolutePositionSliderMovement)
			.OnEndSliderMovement(this, &FCineCameraRigRailDetails::OnEndAbsolutePositionSliderMovement)
			.Font(IDetailLayoutBuilder::GetDetailFont())
		];

	IDetailCategoryBuilder& VisualizationCategory = DetailBuilder.EditCategory("SplineVisualization");
	VisualizationCategory.AddProperty(ShowRailPropertyHandle);
	VisualizationCategory.AddProperty(PreviewScalePropertyHandle);
}

void FCineCameraRigRailDetails::OnAbsolutePositionChanged(float NewValue)
{
	if (ACineCameraRigRail* RigRailActor = RigRailActorPtr.Get())
	{
		RigRailActor->AbsolutePositionOnRail = NewValue;
		static FProperty* AbsolutePositionProperty = FindFProperty<FProperty>(ACineCameraRigRail::StaticClass(), GET_MEMBER_NAME_CHECKED(ACineCameraRigRail, AbsolutePositionOnRail));
		FPropertyChangedEvent SetValueEvent(AbsolutePositionProperty);
		RigRailActor->PostEditChangeProperty(SetValueEvent);
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
	}
}

void FCineCameraRigRailDetails::OnAbsolutePositionCommitted(float NewValue, ETextCommit::Type CommitType)
{
	if (ACineCameraRigRail* RigRailActor = RigRailActorPtr.Get())
	{
		const FScopedTransaction Transaction(LOCTEXT("SetAbsolutePosition", "Set rig rail custom position"));
		RigRailActor->SetFlags(RF_Transactional);
		RigRailActor->Modify();
		RigRailActor->AbsolutePositionOnRail = NewValue;
		static FProperty* AbsolutePositionProperty = FindFProperty<FProperty>(ACineCameraRigRail::StaticClass(), GET_MEMBER_NAME_CHECKED(ACineCameraRigRail, AbsolutePositionOnRail));
		FPropertyChangedEvent SetValueEvent(AbsolutePositionProperty);
		RigRailActor->PostEditChangeProperty(SetValueEvent);
		if (GEditor)
		{
			GEditor->RedrawLevelEditingViewports(true);
		}
	}
}

void FCineCameraRigRailDetails::OnBeginAbsolutePositionSliderMovement()
{
	if (!bAbsolutePositionSliderStartedTransaction)
	{
		if (GEditor)
		{
			bAbsolutePositionSliderStartedTransaction = true;
			GEditor->BeginTransaction(LOCTEXT("AbsolutePositionSliderTransaction", "Set rig rail custom position via slider"));
			if (ACineCameraRigRail* RigRailActor = RigRailActorPtr.Get())
			{
				RigRailActor->SetFlags(RF_Transactional);
				RigRailActor->Modify();
			}
		}
	}
}

void FCineCameraRigRailDetails::OnEndAbsolutePositionSliderMovement(float NewValue)
{
	if (bAbsolutePositionSliderStartedTransaction)
	{
		if (GEditor)
		{
			GEditor->EndTransaction();
			bAbsolutePositionSliderStartedTransaction = false;
		}
	}
}

TOptional<float> FCineCameraRigRailDetails::GetAbsolutePosition() const
{
	if (ACineCameraRigRail* RigRailActor = RigRailActorPtr.Get())
	{
		return RigRailActor->AbsolutePositionOnRail;
	}
	return 0.0f;
}

TOptional<float> FCineCameraRigRailDetails::GetAbsolutePositionSliderMinValue() const
{

	float MinValue = 1.0f;
	if (ACineCameraRigRail* RigRailActor = RigRailActorPtr.Get())
	{
		UCineSplineComponent* SplineComp = RigRailActor->GetCineSplineComponent();
		const UCineSplineMetadata* MetaData = Cast<UCineSplineMetadata>(SplineComp->GetSplinePointsMetadata());
		MinValue = MetaData->AbsolutePosition.Points[0].OutVal;
	}
	return MinValue;
}

TOptional<float> FCineCameraRigRailDetails::GetAbsolutePositionSliderMaxValue() const
{

	float MaxValue = 5.0f;
	if (ACineCameraRigRail* RigRailActor = RigRailActorPtr.Get())
	{
		UCineSplineComponent* SplineComp = RigRailActor->GetCineSplineComponent();
		const UCineSplineMetadata* MetaData = Cast<UCineSplineMetadata>(SplineComp->GetSplinePointsMetadata());
		int32 NumPoints = MetaData->AbsolutePosition.Points.Num();
		MaxValue = MetaData->AbsolutePosition.Points[NumPoints - 1].OutVal;
	}
	return MaxValue;
}
#undef LOCTEXT_NAMESPACE