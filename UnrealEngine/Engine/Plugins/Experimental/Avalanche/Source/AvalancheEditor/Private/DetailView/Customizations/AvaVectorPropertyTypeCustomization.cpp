// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaVectorPropertyTypeCustomization.h"
#include "AvaEditorStyle.h"
#include "AvaEditorSubsystem.h"
#include "AvaEditorViewportUtils.h"
#include "AvaViewportUtils.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "DynamicMeshes/AvaShape2DDynMeshBase.h"
#include "DynamicMeshes/AvaShape3DDynMeshBase.h"
#include "Editor.h"
#include "IDetailChildrenBuilder.h"
#include "Viewport/AvaViewportExtension.h"
#include "ViewportClient/IAvaViewportClient.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "AvaVectorPropertyTypeCustomization"

void FAvaVectorPropertyTypeCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle
	, FDetailWidgetRow& HeaderRow
	, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	VectorPropertyHandle = StructPropertyHandle;
	const FString Type = StructPropertyHandle->GetProperty()->GetCPPType();
	bIsVector3d = Type == "FVector";
	SelectedObjectNum = VectorPropertyHandle->GetNumPerObjectValues();

	if (SelectedObjectNum > 0)
	{
		TArray<UObject*> OuterObjects;
		VectorPropertyHandle->GetOuterObjects(OuterObjects);

		// All outer objects should have the same world? So just use the first
		if (OuterObjects.Num() > 0)
		{
			UAvaEditorSubsystem* AvaEditorSubsystem = UAvaEditorSubsystem::Get(OuterObjects[0]);
			if (AvaEditorSubsystem)
			{
				TSharedPtr<FAvaViewportExtension> ViewportExtension = AvaEditorSubsystem->FindExtension<FAvaViewportExtension>();
				if (ViewportExtension.IsValid())
				{
					TArray<TSharedPtr<IAvaViewportClient>> ViewportClients = ViewportExtension->GetViewportClients();
					if (ViewportClients.Num() > 0)
					{
						ViewportClient = ViewportClients[0];
					}
				}
			}
		}
	}

	bool bPreserveRatio = false;
	if (StructPropertyHandle->HasMetaData("AllowPreserveRatio"))
	{
		bPreserveRatio = true;
		
		static const FVector2D ImageSize(16.f);

		// fill available space
		HeaderRow.NameWidget.HorizontalAlignment = EHorizontalAlignment::HAlign_Fill;
		HeaderRow.NameWidget.VerticalAlignment = EVerticalAlignment::VAlign_Fill;

		auto ComboBoxButtonBuilder = [this](const ERatioMode ButtonMode)->TSharedRef<SButton>
		{
			const FSlateBrush* ButtonImage = GetComboButtonBrush(ButtonMode);
			const FText& ButtonText = GetComboButtonText(ButtonMode);
			
			return SNew(SButton)
					.HAlign(EHorizontalAlignment::HAlign_Fill)
					.VAlign(EVerticalAlignment::VAlign_Fill)
					.Cursor(EMouseCursor::Hand)
					.OnClicked(this, &FAvaVectorPropertyTypeCustomization::OnComboButtonClicked, ButtonMode)
					.ContentPadding(0.f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(0.25f)
						[
							SNew(SScaleBox)
							[
								SNew(SImage)
								.DesiredSizeOverride(ImageSize)
								.Image(ButtonImage)
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						[
							SNew(STextBlock)
							.Justification(ETextJustify::Center)
							.Text(ButtonText)
							.Margin(FMargin(5.f, 0.f))
						]
					];
		};

		TArray<ERatioMode> RatioModes {
			ERatioMode::None,
			ERatioMode::PreserveXY
		};
		
		if (bIsVector3d)
		{
			RatioModes.Add(ERatioMode::PreserveYZ);
			RatioModes.Add(ERatioMode::PreserveXZ);
			RatioModes.Add(ERatioMode::PreserveXYZ);
		}

		const TSharedRef<SVerticalBox> ButtonVerticalBox = SNew(SVerticalBox);
		for (const ERatioMode& Mode : RatioModes)
		{
			ButtonVerticalBox->AddSlot()
			.Padding(0.f)
			[
				ComboBoxButtonBuilder(Mode)
			];
		}
		
		HeaderRow.NameContent()[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				StructPropertyHandle->CreatePropertyNameWidget()
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			[
				SAssignNew(ComboButton, SComboButton)
				.HAlign(EHorizontalAlignment::HAlign_Fill)
				.VAlign(EVerticalAlignment::VAlign_Center)
				.Cursor(EMouseCursor::Hand)
				.Method(EPopupMethod::UseCurrentWindow)
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SScaleBox)
						[
							SNew(SImage)
							.DesiredSizeOverride(ImageSize)
							.Image(this, &FAvaVectorPropertyTypeCustomization::GetCurrentComboButtonBrush)
						]
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Justification(ETextJustify::Center)
						.Text(this, &FAvaVectorPropertyTypeCustomization::GetCurrentComboButtonText)
						.Margin(FMargin(5.f, 0.f))
					]
				]
				.MenuContent()
				[
					ButtonVerticalBox
				]
			]
		];
	}
	else
	{
		HeaderRow.NameContent()[StructPropertyHandle->CreatePropertyNameWidget()];
	}

	// fill available space
	HeaderRow.ValueWidget.HorizontalAlignment = EHorizontalAlignment::HAlign_Fill;
	HeaderRow.ValueWidget.VerticalAlignment = EVerticalAlignment::VAlign_Fill;
	RatioMode = ERatioMode::None;
	
	if (bIsVector3d)
	{
		bPixelSizeProperty = VectorPropertyHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAvaShape3DDynMeshBase, PixelSize3D);
		XPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVector, X));
		YPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVector, Y));
		ZPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVector, Z));

		if (bPreserveRatio)
		{
			if (const FString& Value = VectorPropertyHandle->GetMetaDataProperty()->GetMetaData(FName("VectorRatioMode")); !Value.IsEmpty())
			{
				RatioMode = static_cast<ERatioMode>(FCString::Atoi(*Value));
			}
			else
			{
				RatioMode = ERatioMode::PreserveXYZ;
			}
		}
		
		if (StructPropertyHandle->HasMetaData("ClampMin"))
		{
			MinVectorClamp = FVector(StructPropertyHandle->GetFloatMetaData("ClampMin"));
		}
		
		if (StructPropertyHandle->HasMetaData("ClampMax"))
		{
			MaxVectorClamp = FVector(StructPropertyHandle->GetFloatMetaData("ClampMax"));
		}

		float SpinDelta = 1.f;
		if (StructPropertyHandle->HasMetaData("Delta"))
		{
			SpinDelta = StructPropertyHandle->GetFloatMetaData("Delta");
		}
		// compute spin delta based on min and max value in percentage (100%)
		else if (MinVectorClamp.IsSet() && MaxVectorClamp.IsSet())
		{
			SpinDelta = (MaxVectorClamp->X - MinVectorClamp->X) / 100.f;
		}
		
		HeaderRow.ValueContent()
		[
			SNew(SNumericVectorInputBox3D)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.X(this, &FAvaVectorPropertyTypeCustomization::GetVectorComponent,  static_cast<uint8>(0)) // X
				.Y(this, &FAvaVectorPropertyTypeCustomization::GetVectorComponent,  static_cast<uint8>(1)) // Y
				.Z(this, &FAvaVectorPropertyTypeCustomization::GetVectorComponent,  static_cast<uint8>(2)) // Z
				.bColorAxisLabels(bPreserveRatio)
				.MinVector(MinVectorClamp)
				.MaxVector(MaxVectorClamp)
				.MinSliderVector(MinVectorClamp)
				.MaxSliderVector(MaxVectorClamp)
				.OnXChanged(this, &FAvaVectorPropertyTypeCustomization::SetVectorComponent,  static_cast<uint8>(0))
				.OnYChanged(this, &FAvaVectorPropertyTypeCustomization::SetVectorComponent,  static_cast<uint8>(1))
				.OnZChanged(this, &FAvaVectorPropertyTypeCustomization::SetVectorComponent,  static_cast<uint8>(2))
				.OnXCommitted(this, &FAvaVectorPropertyTypeCustomization::SetVectorComponent,  static_cast<uint8>(0))
				.OnYCommitted(this, &FAvaVectorPropertyTypeCustomization::SetVectorComponent,  static_cast<uint8>(1))
				.OnZCommitted(this, &FAvaVectorPropertyTypeCustomization::SetVectorComponent,  static_cast<uint8>(2))
				.AllowSpin(true)
				.SpinDelta(SpinDelta)
				.IsEnabled(this, &FAvaVectorPropertyTypeCustomization::CanEditValue)
				.OnBeginSliderMovement(this, &FAvaVectorPropertyTypeCustomization::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &FAvaVectorPropertyTypeCustomization::OnEndSliderMovement)
		];
	}
	else
	{
		bPixelSizeProperty = VectorPropertyHandle->GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(UAvaShape2DDynMeshBase, PixelSize2D);
		XPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVector2D, X));
		YPropertyHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FVector2D, Y));

		if (bPreserveRatio)
		{
			if (const FString& Value = VectorPropertyHandle->GetMetaDataProperty()->GetMetaData(FName("VectorRatioMode")); !Value.IsEmpty())
			{
				RatioMode = static_cast<ERatioMode>(FCString::Atoi(*Value));
			}
			else
			{
				RatioMode = ERatioMode::PreserveXY;
			}
		}
		
		if (StructPropertyHandle->HasMetaData("ClampMin"))
		{
			MinVector2DClamp = FVector2D(StructPropertyHandle->GetFloatMetaData("ClampMin"));
		}
		
		if (StructPropertyHandle->HasMetaData("ClampMax"))
		{
			MaxVector2DClamp = FVector2D(StructPropertyHandle->GetFloatMetaData("ClampMax"));
		}

		float SpinDelta = 1.f;
		if (StructPropertyHandle->HasMetaData("Delta"))
		{
			SpinDelta = StructPropertyHandle->GetFloatMetaData("Delta");
		}
		// compute spin delta based on min and max value in percentage (100%)
		else if (MinVector2DClamp.IsSet() && MaxVector2DClamp.IsSet())
		{
			SpinDelta = (MaxVector2DClamp->X - MinVector2DClamp->X) / 100.f;
		}

		HeaderRow.ValueContent()
		[
			SNew(SNumericVectorInputBox2D)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.X(this, &FAvaVectorPropertyTypeCustomization::GetVectorComponent, static_cast<uint8>(0)) // X
				.Y(this, &FAvaVectorPropertyTypeCustomization::GetVectorComponent,  static_cast<uint8>(1)) // Y
				.bColorAxisLabels(bPreserveRatio)
				.MinVector(MinVector2DClamp)
				.MaxVector(MaxVector2DClamp)
				.MinSliderVector(MinVector2DClamp)
				.MaxSliderVector(MaxVector2DClamp)
				.OnXChanged(this, &FAvaVectorPropertyTypeCustomization::SetVectorComponent,  static_cast<uint8>(0))
				.OnYChanged(this, &FAvaVectorPropertyTypeCustomization::SetVectorComponent,  static_cast<uint8>(1))
				.OnXCommitted(this, &FAvaVectorPropertyTypeCustomization::SetVectorComponent,  static_cast<uint8>(0))
				.OnYCommitted(this, &FAvaVectorPropertyTypeCustomization::SetVectorComponent,  static_cast<uint8>(1))
				.AllowSpin(true)
				.SpinDelta(SpinDelta)
				.IsEnabled(this, &FAvaVectorPropertyTypeCustomization::CanEditValue)
				.OnBeginSliderMovement(this, &FAvaVectorPropertyTypeCustomization::OnBeginSliderMovement)
				.OnEndSliderMovement(this, &FAvaVectorPropertyTypeCustomization::OnEndSliderMovement)
		];
	}
}

void FAvaVectorPropertyTypeCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle,
	IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

FReply FAvaVectorPropertyTypeCustomization::OnComboButtonClicked(const ERatioMode NewMode)
{
	RatioMode = NewMode;
	const FString MetaData = FString::FromInt(static_cast<int32>(RatioMode));
	VectorPropertyHandle->GetMetaDataProperty()->SetMetaData(FName("VectorRatioMode"), *MetaData);
	ComboButton->SetIsOpen(false);
	return FReply::Handled();
}

const FSlateBrush* FAvaVectorPropertyTypeCustomization::GetComboButtonBrush(const ERatioMode Mode) const
{
	if (Mode == ERatioMode::None)
	{
		return FAvaEditorStyle::Get().GetBrush("Icons.Unlock");
	}
	else if (Mode == ERatioMode::PreserveXY || Mode == ERatioMode::PreserveXZ || Mode == ERatioMode::PreserveYZ)
	{
		return FAvaEditorStyle::Get().GetBrush("Icons.Lock2d");
	}
	else
	{
		return FAvaEditorStyle::Get().GetBrush("Icons.Lock3d");
	}
}

FText FAvaVectorPropertyTypeCustomization::GetComboButtonText(const ERatioMode Mode) const
{
	switch (Mode)
	{
		case(ERatioMode::PreserveXY):
			return FText::FromString("XY ");
			break;
		case(ERatioMode::PreserveYZ):
			return FText::FromString("YZ ");
			break;
		case(ERatioMode::PreserveXZ):
			return FText::FromString("XZ ");
			break;
		case(ERatioMode::PreserveXYZ):
			return FText::FromString("XYZ");
			break;
		default:
			return FText::FromString("Free");
			break;
	}
}

const FSlateBrush* FAvaVectorPropertyTypeCustomization::GetCurrentComboButtonBrush() const
{
	return GetComboButtonBrush(RatioMode);
}

FText FAvaVectorPropertyTypeCustomization::GetCurrentComboButtonText() const
{
	return GetComboButtonText(RatioMode);
}

TOptional<double> FAvaVectorPropertyTypeCustomization::GetVectorComponent(const uint8 Component) const
{
	if (!VectorPropertyHandle.IsValid() || SelectedObjectNum == 0)
	{
		return TOptional<double>();
	}
	double OutValue = 0.f;
	switch(Component)
	{
		case 0:
			if (XPropertyHandle->GetValue(OutValue) != FPropertyAccess::Success)
			{
				return TOptional<double>();
			}
		break;
		case 1:
			if (YPropertyHandle->GetValue(OutValue) != FPropertyAccess::Success)
			{
				return TOptional<double>();
			}
		break;
		case 2:
			if (ZPropertyHandle->GetValue(OutValue) != FPropertyAccess::Success)
			{
				return TOptional<double>();
			}
		break;
		default:
			return TOptional<double>();
		break;
	}
	// handle specific case
	if (bPixelSizeProperty)
	{
		return MeshSizeToPixelSize(OutValue);
	}
	else
	{
		return OutValue;
	}
}

void FAvaVectorPropertyTypeCustomization::SetVectorComponent(double NewValue, const uint8 Component)
{
	if (bMovingSlider)
	{
		SetVectorComponent(NewValue, ETextCommit::Default, Component);
	}
}

void FAvaVectorPropertyTypeCustomization::SetVectorComponent(double NewValue, ETextCommit::Type CommitType, const uint8 Component)
{
	const bool bFinalCommit = CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus;
	if (!bFinalCommit && CommitType != ETextCommit::Default)
	{
		return;
	}
	if (!VectorPropertyHandle.IsValid())
	{
		return;
	}
	// init here in case we only input the value without slider movement
	if ((!bIsVector3d && Begin2DValues.Num() == 0) ||
		(bIsVector3d && Begin3DValues.Num() == 0))
	{
		InitVectorValuesForRatio();
	}
	if (SelectedObjectNum == 0)
	{
		return;
	}
	// handle interactive debounce to avoid slow behaviour
	LastComponentValueSet = Component;
	if (DebounceValueSet > 0 && !bFinalCommit)
	{
		DebounceValueSet--;
		return;
	}
	DebounceValueSet = FMath::Clamp(SelectedObjectNum > 1 ? (SelectedObjectNum * MULTI_OBJECT_DEBOUNCE) : SINGLE_OBJECT_DEBOUNCE, 0, 255);
	// handle specific case for pixel size property
	if (bPixelSizeProperty)
	{
		NewValue = PixelSizeToMeshSize(NewValue);
	}
	if (!bMovingSlider)
	{
		GEditor->BeginTransaction(VectorPropertyHandle->GetPropertyDisplayName());
	}
	// update objects value for property, we handle transaction ourselves to batch properties changes together
	const EPropertyValueSetFlags::Type Flags = bMovingSlider ? EPropertyValueSetFlags::InteractiveChange : EPropertyValueSetFlags::NotTransactable;
	SetComponentValue(NewValue, Component, Flags);
	if (!bMovingSlider)
	{
		GEditor->EndTransaction();
		ResetVectorValuesForRatio();
	}
}

void FAvaVectorPropertyTypeCustomization::OnBeginSliderMovement()
{
	LastComponentValueSet = INVALID_COMPONENT_IDX;
	DebounceValueSet = 0;
	InitVectorValuesForRatio();
	bMovingSlider = true;
	GEditor->BeginTransaction(VectorPropertyHandle->GetPropertyDisplayName());
}

void FAvaVectorPropertyTypeCustomization::OnEndSliderMovement(double NewValue)
{
	DebounceValueSet = 0;
	bMovingSlider = false;
	// set final value like enter pressed
	if (LastComponentValueSet != INVALID_COMPONENT_IDX)
	{
		SetVectorComponent(NewValue, ETextCommit::OnEnter, LastComponentValueSet);
	}
	// end started transactions during process
	while(GEditor->IsTransactionActive())
	{
		GEditor->EndTransaction();
	}
	ResetVectorValuesForRatio();
}

bool FAvaVectorPropertyTypeCustomization::CanEditValue() const
{
	if (!VectorPropertyHandle.IsValid())
	{
		return false;
	}
	
	if (bPixelSizeProperty && SelectedObjectNum > 0 && ViewportClient.IsValid())
	{
		if (FAvaViewportUtils::IsValidViewportSize(ViewportClient.Pin()->GetVirtualViewportSize()))
		{
			return VectorPropertyHandle->IsEditable();
		}

		return false;
	}
	
	return VectorPropertyHandle->IsEditable();
}

void FAvaVectorPropertyTypeCustomization::InitVectorValuesForRatio()
{
	Begin2DValues.Empty();
	Begin3DValues.Empty();
	
	TArray<FString> OutValues;
	VectorPropertyHandle->GetPerObjectValues(OutValues);
	SelectedObjectNum = VectorPropertyHandle->GetNumPerObjectValues();
	
	if (SelectedObjectNum > 0)
	{
		for (const FString& Val : OutValues)
		{
			if (bIsVector3d)
			{
				FVector Vector;
				if (Vector.InitFromString(Val))
				{
					Begin3DValues.Add(Vector);
				}
				else
				{
					TOptional<FVector> OptionalVector;
					Begin3DValues.Add(OptionalVector);
				}
			}
			else
			{
				FVector2D Vector;
				if (Vector.InitFromString(Val))
				{
					Begin2DValues.Add(Vector);
				}
				else
				{
					TOptional<FVector2D> OptionalVector;
					Begin2DValues.Add(OptionalVector);
				}
			}
		}
	}
}

void FAvaVectorPropertyTypeCustomization::ResetVectorValuesForRatio()
{
	Begin2DValues.Empty();
	Begin3DValues.Empty();
}

void FAvaVectorPropertyTypeCustomization::SetComponentValue(const double NewValue, const uint8 Component, const EPropertyValueSetFlags::Type Flags)
{
	// check if we are preserving ratio for current component change
	bool bPreserveRatio = false;
	switch (Component)
	{
		case 0:
			bPreserveRatio = ((RatioMode & ERatioMode::X) != ERatioMode::None);
		break;
		case 1:
			bPreserveRatio = ((RatioMode & ERatioMode::Y) != ERatioMode::None);
		break;
		case 2:
			bPreserveRatio = ((RatioMode & ERatioMode::Z) != ERatioMode::None);
		break;
		default:
		break;
	}
	const TArray<bool> PreserveRatios
	{
		(RatioMode & ERatioMode::X) != ERatioMode::None && bPreserveRatio,
		(RatioMode & ERatioMode::Y) != ERatioMode::None && bPreserveRatio,
		(RatioMode & ERatioMode::Z) != ERatioMode::None && bPreserveRatio
	};
	// set property per object since we need to handle ratios for each objects
	const uint8 MaxComponentCount = bIsVector3d ? 3 : 2;
	for (int32 ObjIdx = 0; ObjIdx < SelectedObjectNum; ObjIdx++)
	{
		if (!bIsVector3d && !Begin2DValues.IsValidIndex(ObjIdx) && !Begin2DValues[ObjIdx].IsSet())
		{
			continue;
		}
		if (bIsVector3d && !Begin3DValues.IsValidIndex(ObjIdx) && !Begin3DValues[ObjIdx].IsSet())
		{
			continue;
		}
		// compute clamped ratio for value change
		const double ClampedRatio = GetClampedRatioValueChange(ObjIdx, NewValue, Component, PreserveRatios);
		// loop for each component (X,Y,Z)
		for (uint8 ComponentIdx = 0; ComponentIdx < MaxComponentCount; ComponentIdx++)
		{
			// only assign value to specific component, skip others
			if (!bPreserveRatio && ComponentIdx != Component)
			{
				continue;
			}
			// compute new component value
			const double NewComponentValue = GetClampedComponentValue(ObjIdx, NewValue, ClampedRatio, ComponentIdx, Component);
			switch(ComponentIdx)
			{
				case 0:
					if (PreserveRatios[0] || ComponentIdx == Component)
					{
						XPropertyHandle->SetPerObjectValue(ObjIdx, FString::SanitizeFloat(NewComponentValue), Flags);
					}
					break;
				case 1:
					if (PreserveRatios[1] || ComponentIdx == Component)
					{
						YPropertyHandle->SetPerObjectValue(ObjIdx, FString::SanitizeFloat(NewComponentValue), Flags);
					}
					break;
				case 2:
					if (PreserveRatios[2] || ComponentIdx == Component)
					{
						ZPropertyHandle->SetPerObjectValue(ObjIdx, FString::SanitizeFloat(NewComponentValue), Flags);
					}
					break;
				default:;
			}
		}
	}
}

double FAvaVectorPropertyTypeCustomization::GetClampedRatioValueChange(const int32 ObjectIdx, const double NewValue, const uint8 Component, const TArray<bool>& PreserveRatios) const
{
	double Ratio = 1;
	// get pre change value for this component
	if (bIsVector3d)
	{
		FVector BeginValue = Begin3DValues[ObjectIdx].GetValue();
		if (BeginValue[Component] != 0)
		{
			Ratio = NewValue / BeginValue[Component];
		}
		// apply min/max clamp
		if (MinVectorClamp.IsSet() || MaxVectorClamp.IsSet())
		{
			for (int32 ComponentIdx = 0; ComponentIdx < 3; ComponentIdx++)
			{
				if (PreserveRatios[ComponentIdx] || ComponentIdx == Component)
				{
					const double EndValue = BeginValue[ComponentIdx] * Ratio;
					if (MinVectorClamp.IsSet())
					{
						const double MinValue = MinVectorClamp.GetValue()[ComponentIdx];
						if (EndValue < MinValue)
						{
							Ratio = MinValue / BeginValue[ComponentIdx];
						}
					}
					if (MaxVectorClamp.IsSet())
					{
						const double MaxValue = MaxVectorClamp.GetValue()[ComponentIdx];
						if (EndValue > MaxValue)
						{
							Ratio = MaxValue / BeginValue[ComponentIdx];
						}
					}
				}
			}	
		}
	}
	else
	{
		FVector2D BeginValue = Begin2DValues[ObjectIdx].GetValue();
		if (BeginValue[Component] != 0)
		{
			Ratio = NewValue / BeginValue[Component];
		}
		// apply min/max clamp
		if (MinVector2DClamp.IsSet() || MaxVector2DClamp.IsSet())
		{
			for (int32 ComponentIdx = 0; ComponentIdx < 2; ComponentIdx++)
			{
				if (PreserveRatios[ComponentIdx] || ComponentIdx == Component)
				{
					const double EndValue = BeginValue[ComponentIdx] * Ratio;
					if (MinVector2DClamp.IsSet())
					{
						const double MinValue = MinVector2DClamp.GetValue()[ComponentIdx];
						if (EndValue < MinValue)
						{
							Ratio = MinValue / BeginValue[ComponentIdx];
						}
					}
					if (MaxVector2DClamp.IsSet())
					{
						const double MaxValue = MaxVector2DClamp.GetValue()[ComponentIdx];
						if (EndValue > MaxValue)
						{
							Ratio = MaxValue / BeginValue[ComponentIdx];
						}
					}
				}
			}
		}
	}
	return Ratio;
}

double FAvaVectorPropertyTypeCustomization::GetClampedComponentValue(const int32 ObjectIdx, double NewValue, const double Ratio, const uint8 ComponentIdx, const uint8 OriginalComponent)
{
	const double OldValue = (bIsVector3d ? Begin3DValues[ObjectIdx].GetValue()[ComponentIdx] : Begin2DValues[ObjectIdx].GetValue()[ComponentIdx]);
	const double SliderOriginalValue = (bIsVector3d ? Begin3DValues[ObjectIdx].GetValue()[OriginalComponent] : Begin2DValues[ObjectIdx].GetValue()[OriginalComponent]);
	if (SliderOriginalValue == 0 && OldValue == 0)
	{
		if (bIsVector3d)
		{
			if (MinVectorClamp.IsSet())
			{
				NewValue = FMath::Max(NewValue, MinVectorClamp.GetValue()[ComponentIdx]);
			}
			if (MaxVectorClamp.IsSet())
			{
				NewValue = FMath::Min(NewValue, MaxVectorClamp.GetValue()[ComponentIdx]);
			}
		}
		else
		{
			if (MinVector2DClamp.IsSet())
			{
				NewValue = FMath::Max(NewValue, MinVector2DClamp.GetValue()[ComponentIdx]);
			}
			if (MaxVector2DClamp.IsSet())
			{
				NewValue = FMath::Min(NewValue, MaxVector2DClamp.GetValue()[ComponentIdx]);
			}
		}
		return NewValue;
	}
	return OldValue * Ratio;
}

double FAvaVectorPropertyTypeCustomization::MeshSizeToPixelSize(double MeshSize) const
{
	if (ViewportClient.IsValid())
	{
		double PixelSize;

		if (FAvaEditorViewportUtils::MeshSizeToPixelSize(ViewportClient.Pin().ToSharedRef(), MeshSize, PixelSize))
		{
			return PixelSize;
		}
	}

	return MeshSize;
}

double FAvaVectorPropertyTypeCustomization::PixelSizeToMeshSize(double PixelSize) const
{
	if (ViewportClient.IsValid())
	{
		double MeshSize;

		if (FAvaEditorViewportUtils::PixelSizeToMeshSize(ViewportClient.Pin().ToSharedRef(), PixelSize, MeshSize))
		{
			return MeshSize;
		}
	}

	return PixelSize;
}

#undef LOCTEXT_NAMESPACE
