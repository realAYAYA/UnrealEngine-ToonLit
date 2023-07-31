// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditMode/ControlRigControlsProxy.h"
#include "EditorModeManager.h"
#include "EditMode/ControlRigEditMode.h"
#include "Sequencer/ControlRigSequence.h"
#include "Rigs/RigHierarchy.h"

#include "Components/SkeletalMeshComponent.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "MovieSceneCommonHelpers.h"
#include "PropertyHandle.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ControlRigControlsProxy)

#if WITH_EDITOR

#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "SEnumCombo.h"


void UControlRigControlsProxy::SetIsMultiple(bool bIsVal)
{ 
	bIsMultiple = bIsVal; 
	if (bIsMultiple)
	{
		FString DisplayString = TEXT("Multiple");
		FName DisplayName(*DisplayString);
		Name = DisplayName;
	}
	else
	{
		Name = ControlName;
	}
}


void FControlRigEnumControlProxyValueDetails::CustomizeHeader(TSharedRef<IPropertyHandle> InStructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<UObject*> Objects;
	InStructPropertyHandle->GetOuterObjects(Objects);
	ensure(Objects.Num() == 1); // This is in here to ensure we are only showing the modifier details in the blueprint editor

	for (UObject* Object : Objects)
	{
		if (Object->IsA<UControlRigEnumControlProxy>())
		{
			ProxyBeingCustomized = Cast<UControlRigEnumControlProxy>(Object);
		}
	}

	check(ProxyBeingCustomized);

	HeaderRow
	.NameContent()
	[
		InStructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SEnumComboBox, ProxyBeingCustomized->Enum.EnumType)
		.OnEnumSelectionChanged(SEnumComboBox::FOnEnumSelectionChanged::CreateSP(this, &FControlRigEnumControlProxyValueDetails::OnEnumValueChanged, InStructPropertyHandle))
		.CurrentValue(this, &FControlRigEnumControlProxyValueDetails::GetEnumValue)
		.Font(FAppStyle::GetFontStyle(TEXT("MenuItem.Font")))
	];
}

void FControlRigEnumControlProxyValueDetails::CustomizeChildren(TSharedRef<IPropertyHandle> InStructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
}

int32 FControlRigEnumControlProxyValueDetails::GetEnumValue() const
{
	if (ProxyBeingCustomized)
	{
		return ProxyBeingCustomized->Enum.EnumIndex;
	}
	return 0;
}

void FControlRigEnumControlProxyValueDetails::OnEnumValueChanged(int32 InValue, ESelectInfo::Type InSelectInfo, TSharedRef<IPropertyHandle> InStructHandle){
	if (ProxyBeingCustomized)
	{
		ProxyBeingCustomized->Enum.EnumIndex = InValue;
		InStructHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	}
}

#endif

void UControlRigControlsProxy::SelectionChanged(bool bInSelected)
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		Modify();
		const FName PropertyName("bSelected");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		Binding.CallFunction<bool>(*this, bInSelected);
	}
}

void UControlRigControlsProxy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigControlsProxy, bSelected))
	{
		FRigControlElement* ControlElement = GetControlElement();
		if (ControlElement && ControlRig.IsValid())
		{
			FControlRigInteractionScope InteractionScope(ControlRig.Get(), ControlElement->GetKey());
			ControlRig->SelectControl(ControlName, bSelected);
			ControlRig->Evaluate_AnyThread();
		}
	}
}

#if WITH_EDITOR
void UControlRigControlsProxy::PostEditUndo()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlRig.IsValid() && ControlRig->GetHierarchy()->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		ControlRig->SelectControl(ControlName, bSelected);
	}
}
#endif

FRigControlElement* UControlRigControlsProxy::GetControlElement() const
{
	if(ControlRig.IsValid())
	{
		return ControlRig->GetHierarchy()->Find<FRigControlElement>(FRigElementKey(ControlName, ERigElementType::Control));
	}
	return nullptr;
}

void UControlRigTransformControlProxy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if ((PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigTransformControlProxy, Transform))
		|| (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigTransformControlProxy, Transform)))
	{
		FRigControlElement* ControlElement = GetControlElement();
		if (ControlElement && ControlRig.IsValid())
		{
			//MUST set through ControlRig
			FControlRigInteractionScope InteractionScope(ControlRig.Get(), ControlElement->GetKey());
			const FTransform RealTransform = Transform.ToFTransform(); //Transform is FEulerTransform
			ControlRig->SetControlValue<FRigControlValue::FTransform_Float>(ControlName, RealTransform, true, EControlRigSetKey::DoNotCare,false);
			ControlRig->GetHierarchy()->SetControlPreferredRotator(ControlElement, Transform.Rotation);
			ControlRig->Evaluate_AnyThread();

		}
	}
}

void UControlRigTransformControlProxy::ValueChanged()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlRig.IsValid())
	{
		Modify();
		const FName PropertyName("Transform");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		const FTransform NewTransform = ControlRig.Get()->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FTransform_Float>().ToTransform();
		FEulerTransform EulerTransform(NewTransform);
		EulerTransform.Rotation = ControlRig->GetHierarchy()->GetControlPreferredRotator(ControlElement);
		Binding.CallFunction<FEulerTransform>(*this, EulerTransform);
	}
}

#if WITH_EDITOR
void UControlRigTransformControlProxy::PostEditUndo()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlRig.IsValid() && ControlRig->GetHierarchy()->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		ControlRig->SelectControl(ControlName, bSelected);
		const FTransform RealTransform = Transform.ToFTransform(); //Transform is FEulerTransform
		ControlRig->SetControlValue<FRigControlValue::FTransform_Float>(ControlName, RealTransform, true, EControlRigSetKey::Never,false);
		ControlRig->GetHierarchy()->SetControlPreferredRotator(ControlElement, Transform.Rotation);
	}
}
#endif

void UControlRigTransformControlProxy::SetKey(const IPropertyHandle& KeyedPropertyHandle)
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FEulerTransform, Location))
		{
			Context.KeyMask = (uint32)EControlRigContextChannelToKey::Translation;
		}
		else if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FEulerTransform, Rotation))
		{
			Context.KeyMask = (uint32)EControlRigContextChannelToKey::Rotation;
		}
		else if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FEulerTransform, Scale))
		{
			Context.KeyMask = (uint32)EControlRigContextChannelToKey::Scale;
		}
		const FTransform RealTransform = Transform.ToFTransform(); //Transform is FEulerTransform
		ControlRig->SetControlValue<FRigControlValue::FTransform_Float>(ControlName, RealTransform, true, Context, false);
		ControlRig->GetHierarchy()->SetControlPreferredRotator(ControlElement, Transform.Rotation);
	}
}

void UControlRigTransformNoScaleControlProxy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if ((PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigTransformNoScaleControlProxy, Transform))
		|| (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigTransformNoScaleControlProxy, Transform)))
	{
		FRigControlElement* ControlElement = GetControlElement();
		if (ControlElement && ControlRig.IsValid())
		{
			//MUST set through ControlRig
			FControlRigInteractionScope InteractionScope(ControlRig.Get(), ControlElement->GetKey());
			ControlRig->SetControlValue<FRigControlValue::FTransformNoScale_Float>(ControlName, Transform, true, EControlRigSetKey::DoNotCare,false);
			ControlRig->Evaluate_AnyThread();

		}
	}
}

void UControlRigTransformNoScaleControlProxy::ValueChanged()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		Modify();
		const FName PropertyName("Transform");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		const FTransformNoScale NewTransform = ControlRig.Get()->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FTransformNoScale_Float>().ToTransform();
		Binding.CallFunction<FTransformNoScale>(*this, NewTransform);
	}
}

#if WITH_EDITOR
void UControlRigTransformNoScaleControlProxy::PostEditUndo()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlRig.IsValid() && ControlRig->GetHierarchy()->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		ControlRig->SelectControl(ControlName, bSelected);
		ControlRig->SetControlValue<FRigControlValue::FTransformNoScale_Float>(ControlName, Transform, true, EControlRigSetKey::Never,false);
	}
}
#endif

void UControlRigTransformNoScaleControlProxy::SetKey(const IPropertyHandle& KeyedPropertyHandle)
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FTransformNoScale, Location))
		{
			Context.KeyMask = (uint32)EControlRigContextChannelToKey::Translation;
		}
		else if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FTransformNoScale, Rotation))
		{
			Context.KeyMask = (uint32)EControlRigContextChannelToKey::Rotation;
		}
		ControlRig->SetControlValue<FRigControlValue::FTransformNoScale_Float>(ControlName, Transform, true, Context, false);
	}
}


void UControlRigEulerTransformControlProxy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if ((PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigEulerTransformControlProxy, Transform))
		|| (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigEulerTransformControlProxy, Transform)))
	{
		FRigControlElement* ControlElement = GetControlElement();
		if (ControlElement && ControlRig.IsValid())
		{
			//MUST set through ControlRig
			FControlRigInteractionScope InteractionScope(ControlRig.Get(), ControlElement->GetKey());
			ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlName, Transform, true, EControlRigSetKey::DoNotCare,false);
			ControlRig->Evaluate_AnyThread();

		}
	}
}

void UControlRigEulerTransformControlProxy::ValueChanged()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		Modify();
		const FName PropertyName("Transform");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		const FEulerTransform NewTransform = ControlRig.Get()->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FRigControlValue::FEulerTransform_Float>().ToTransform();

		Binding.CallFunction<FEulerTransform>(*this, NewTransform);

	}
}

#if WITH_EDITOR
void UControlRigEulerTransformControlProxy::PostEditUndo()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlRig.IsValid() && ControlRig->GetHierarchy()->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		ControlRig->SelectControl(ControlName, bSelected);
		ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlName, Transform, true, EControlRigSetKey::Never,false);
	}
}
#endif

void UControlRigEulerTransformControlProxy::SetKey(const IPropertyHandle& KeyedPropertyHandle)
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FEulerTransform, Location))
		{
			Context.KeyMask = (uint32)EControlRigContextChannelToKey::Translation;
		}
		else if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FEulerTransform, Rotation))
		{
			Context.KeyMask = (uint32)EControlRigContextChannelToKey::Rotation;
		}
		else if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FEulerTransform, Scale))
		{
			Context.KeyMask = (uint32)EControlRigContextChannelToKey::Scale;
		}
		ControlRig->SetControlValue<FRigControlValue::FEulerTransform_Float>(ControlName, Transform, true, Context,false);
	}
}

void UControlRigFloatControlProxy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigFloatControlProxy, Float))
	{
		FRigControlElement* ControlElement = GetControlElement();
		if (ControlElement && ControlRig.IsValid())
		{
			//MUST set through ControlRig
			FControlRigInteractionScope InteractionScope(ControlRig.Get(), ControlElement->GetKey());
			ControlRig->SetControlValue<float>(ControlName, Float, true, EControlRigSetKey::DoNotCare,false);
			ControlRig->Evaluate_AnyThread();

		}
	}
}

void UControlRigFloatControlProxy::ValueChanged()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		Modify();
		const FName PropertyName("Float");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		const float Val = ControlRig.Get()->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<float>();
		Binding.CallFunction<float>(*this, Val);
	}
}

#if WITH_EDITOR
void UControlRigFloatControlProxy::PostEditUndo()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlRig.IsValid() && ControlRig->GetHierarchy()->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		ControlRig->SelectControl(ControlName, bSelected);
		ControlRig->SetControlValue<float>(ControlName, Float, true, EControlRigSetKey::Never,false);
	}
}
#endif


void UControlRigFloatControlProxy::SetKey(const IPropertyHandle& KeyedPropertyHandle)
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		ControlRig->SetControlValue<float>(ControlName, Float, true, EControlRigSetKey::Always,false);
	}
}

void UControlRigIntegerControlProxy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigIntegerControlProxy, Integer))
	{
		FRigControlElement* ControlElement = GetControlElement();
		if (ControlElement && ControlRig.IsValid())
		{
			//MUST set through ControlRig
			FControlRigInteractionScope InteractionScope(ControlRig.Get(), ControlElement->GetKey());
			ControlRig->SetControlValue<int32>(ControlName, Integer, true, EControlRigSetKey::DoNotCare,false);
			ControlRig->Evaluate_AnyThread();
		}
	}
}

void UControlRigIntegerControlProxy::ValueChanged()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		Modify();
		const FName PropertyName("Integer");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		const int32 Val = ControlRig.Get()->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<int32>();
		Binding.CallFunction<int32>(*this, Val);
	}
}

#if WITH_EDITOR
void UControlRigIntegerControlProxy::PostEditUndo()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlRig.IsValid() && ControlRig->GetHierarchy()->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		ControlRig->SelectControl(ControlName, bSelected);
		ControlRig->SetControlValue<int32>(ControlName, Integer, true, EControlRigSetKey::Never,false);
	}
}
#endif

void UControlRigIntegerControlProxy::SetKey(const IPropertyHandle& KeyedPropertyHandle)
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		ControlRig->SetControlValue<int32>(ControlName, Integer, true, EControlRigSetKey::Always,false);
	}
}

void UControlRigEnumControlProxy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigEnumControlProxy, Enum))
	{
		FRigControlElement* ControlElement = GetControlElement();
		if (ControlElement && ControlRig.IsValid())
		{
			//MUST set through ControlRig
			FControlRigInteractionScope InteractionScope(ControlRig.Get(), ControlElement->GetKey());
			ControlRig->SetControlValue<int32>(ControlName, Enum.EnumIndex, true, EControlRigSetKey::DoNotCare,false);
			ControlRig->Evaluate_AnyThread();

		}
	}
}

void UControlRigEnumControlProxy::ValueChanged()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		Modify();
		const FName PropertyName("Enum");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());

		FControlRigEnumControlProxyValue Val;
		Val.EnumType = ControlElement->Settings.ControlEnum;
		Val.EnumIndex = ControlRig.Get()->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<int32>();

		Binding.CallFunction<FControlRigEnumControlProxyValue>(*this, Val);
	}
}

#if WITH_EDITOR
void UControlRigEnumControlProxy::PostEditUndo()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlRig.IsValid() && ControlRig->GetHierarchy()->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		ControlRig->SelectControl(ControlName, bSelected);
		ControlRig->SetControlValue<int32>(ControlName, Enum.EnumIndex, true, EControlRigSetKey::Never,false);
	}
}
#endif

void UControlRigEnumControlProxy::SetKey(const IPropertyHandle& KeyedPropertyHandle)
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		ControlRig->SetControlValue<int32>(ControlName, Enum.EnumIndex, true, EControlRigSetKey::Always,false);
	}
}

void UControlRigVectorControlProxy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if ((PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigVectorControlProxy, Vector))
		|| (PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigVectorControlProxy, Vector)))
	{
		FRigControlElement* ControlElement = GetControlElement();
		if (ControlElement && ControlRig.IsValid())
		{
			//MUST set through ControlRig
			FControlRigInteractionScope InteractionScope(ControlRig.Get(), ControlElement->GetKey());
			ControlRig->SetControlValue<FVector3f>(ControlName, Vector, true, EControlRigSetKey::DoNotCare,false);
			ControlRig->Evaluate_AnyThread();

		}
	}
}

void UControlRigVectorControlProxy::ValueChanged()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		Modify();
		const FName PropertyName("Vector");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		const FVector3f Val = ControlRig.Get()->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FVector3f>();
		Binding.CallFunction<FVector3f>(*this, Val);
	}
}

#if WITH_EDITOR
void UControlRigVectorControlProxy::PostEditUndo()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlRig.IsValid() && ControlRig->GetHierarchy()->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		ControlRig->SelectControl(ControlName, bSelected);
		ControlRig->SetControlValue<FVector3f>(ControlName, (FVector3f)Vector, true, EControlRigSetKey::Never,false);
	}
}
#endif


void UControlRigVectorControlProxy::SetKey(const IPropertyHandle& KeyedPropertyHandle)
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FVector3f, X))
		{
			switch (ControlElement->Settings.ControlType)
			{
			case ERigControlType::Position:
				Context.KeyMask = (uint32)EControlRigContextChannelToKey::TranslationX;
				break;
			case ERigControlType::Rotator:	
				Context.KeyMask = (uint32)EControlRigContextChannelToKey::RotationX;
				break;
			case ERigControlType::Scale:
				Context.KeyMask = (uint32)EControlRigContextChannelToKey::ScaleX;
				break;
			}
		}
		else if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FVector3f, Y))
		{
			switch (ControlElement->Settings.ControlType)
			{
			case ERigControlType::Position:
				Context.KeyMask = (uint32)EControlRigContextChannelToKey::TranslationY;
				break;
			case ERigControlType::Rotator:
				Context.KeyMask = (uint32)EControlRigContextChannelToKey::RotationY;
				break;
			case ERigControlType::Scale:
				Context.KeyMask = (uint32)EControlRigContextChannelToKey::ScaleY;
				break;
			}
		}
		else if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FVector3f, Z))
		{
			switch (ControlElement->Settings.ControlType)
			{
			case ERigControlType::Position:
				Context.KeyMask = (uint32)EControlRigContextChannelToKey::TranslationZ;
				break;
			case ERigControlType::Rotator:
				Context.KeyMask = (uint32)EControlRigContextChannelToKey::RotationZ;
				break;
			case ERigControlType::Scale:
				Context.KeyMask = (uint32)EControlRigContextChannelToKey::ScaleZ;
				break;
			}
		}
		ControlRig->SetControlValue<FVector3f>(ControlName, (FVector3f)Vector, true, Context,false);
	}
}

void UControlRigVector2DControlProxy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if ((PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigVector2DControlProxy, Vector2D))
		|| ((PropertyChangedEvent.MemberProperty && PropertyChangedEvent.MemberProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigVector2DControlProxy, Vector2D))))
	{
		FRigControlElement* ControlElement = GetControlElement();
		if (ControlElement && ControlRig.IsValid())
		{
			//MUST set through ControlRig
			FControlRigInteractionScope InteractionScope(ControlRig.Get(), ControlElement->GetKey());
			ControlRig->SetControlValue<FVector3f>(ControlName, FVector3f(Vector2D.X, Vector2D.Y, 0.f), true, EControlRigSetKey::DoNotCare,false);
			ControlRig->Evaluate_AnyThread();

		}
	}
}

void UControlRigVector2DControlProxy::ValueChanged()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		Modify();
		const FName PropertyName("Vector2D");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		const FVector3f TempValue = ControlRig.Get()->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<FVector3f>();
		const FVector2D Val(TempValue.X, TempValue.Y);
		Binding.CallFunction<FVector2D>(*this, Val);
	}
}

#if WITH_EDITOR
void UControlRigVector2DControlProxy::PostEditUndo()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlRig.IsValid() && ControlRig->GetHierarchy()->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		ControlRig->SelectControl(ControlName, bSelected);
		ControlRig->SetControlValue<FVector3f>(ControlName, FVector3f(Vector2D.X, Vector2D.Y, 0.f), true, EControlRigSetKey::Never,false);
	}
}
#endif


void UControlRigVector2DControlProxy::SetKey(const IPropertyHandle& KeyedPropertyHandle)
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		FRigControlModifiedContext Context;
		Context.SetKey = EControlRigSetKey::Always;
		if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FVector2D, X))
		{
			Context.KeyMask = (uint32)EControlRigContextChannelToKey::TranslationX;
		}
		else if (KeyedPropertyHandle.GetProperty()->GetFName() == GET_MEMBER_NAME_CHECKED(FVector2D, Y))
		{
			Context.KeyMask = (uint32)EControlRigContextChannelToKey::TranslationY;
		}
		ControlRig->SetControlValue<FVector3f>(ControlName, FVector3f(Vector2D.X, Vector2D.Y, 0.f), true, Context,false);
	}
}


void UControlRigBoolControlProxy::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UControlRigBoolControlProxy, Bool))
	{
		FRigControlElement* ControlElement = GetControlElement();
		if (ControlElement && ControlRig.IsValid())
		{
			//MUST set through ControlRig
			FControlRigInteractionScope InteractionScope(ControlRig.Get(), ControlElement->GetKey());
			ControlRig->SetControlValue<bool>(ControlName, Bool, true, EControlRigSetKey::DoNotCare,false);
			ControlRig->Evaluate_AnyThread();
		}
	}
}

void UControlRigBoolControlProxy::ValueChanged()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		Modify();
		const FName PropertyName("Bool");
		FTrackInstancePropertyBindings Binding(PropertyName, PropertyName.ToString());
		const bool Val = ControlRig.Get()->GetHierarchy()->GetControlValue(ControlElement, ERigControlValueType::Current).Get<bool>();
		Binding.CallFunction<bool>(*this, Val);
	}
}

#if WITH_EDITOR
void UControlRigBoolControlProxy::PostEditUndo()
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement && ControlRig.IsValid() && ControlRig->GetHierarchy()->Contains(FRigElementKey(ControlName, ERigElementType::Control)))
	{
		ControlRig->SelectControl(ControlName, bSelected);
		ControlRig->SetControlValue<bool>(ControlName, Bool, true, EControlRigSetKey::Never,false);
	}
}
#endif


void UControlRigBoolControlProxy::SetKey(const IPropertyHandle& KeyedPropertyHandle)
{
	FRigControlElement* ControlElement = GetControlElement();
	if (ControlElement)
	{
		ControlRig->SetControlValue<bool>(ControlName, Bool, true, EControlRigSetKey::Always,false);
	}
}


//////UControlDetailPanelControlProxies////////

UControlRigControlsProxy* UControlRigDetailPanelControlProxies::FindProxy(UControlRig* ControlRig, const FName& Name) const
{
	const FControlToProxyMap* ControlRigProxies = AllProxies.Find(ControlRig);
	if (ControlRigProxies)
	{
		TObjectPtr<UControlRigControlsProxy> const* Proxy = ControlRigProxies->ControlToProxy.Find(Name);
		if (Proxy && Proxy[0])
		{
			return Proxy[0];
		}
	}
	return nullptr;
}

void UControlRigDetailPanelControlProxies::AddProxy(UControlRig* ControlRig, const FName& Name,  FRigControlElement* ControlElement)
{
	UControlRigControlsProxy* Proxy = FindProxy(ControlRig,Name);
	if (!Proxy && ControlElement != nullptr)
	{
		switch(ControlElement->Settings.ControlType)
		{
			case ERigControlType::Transform:
			{
				Proxy = NewObject<UControlRigTransformControlProxy>(GetTransientPackage(), NAME_None);
				break;

			}
			case ERigControlType::TransformNoScale:
			{
				Proxy = NewObject<UControlRigTransformNoScaleControlProxy>(GetTransientPackage(), NAME_None);
				break;

			}
			case ERigControlType::EulerTransform:
			{
				Proxy = NewObject<UControlRigEulerTransformControlProxy>(GetTransientPackage(), NAME_None);
				break;
			}
			case ERigControlType::Float:
			{
				Proxy = NewObject<UControlRigFloatControlProxy>(GetTransientPackage(), NAME_None);
				break;

			}
			case ERigControlType::Integer:
			{
				if(ControlElement->Settings.ControlEnum == nullptr)
				{
					Proxy = NewObject<UControlRigIntegerControlProxy>(GetTransientPackage(), NAME_None);
				}
				else
				{
					UControlRigEnumControlProxy* EnumProxy = NewObject<UControlRigEnumControlProxy>(GetTransientPackage(), NAME_None);
					EnumProxy->Enum.EnumType = ControlElement->Settings.ControlEnum;
					Proxy = EnumProxy;
				}
				break;

			}
			case ERigControlType::Position:
			case ERigControlType::Rotator:
			case ERigControlType::Scale:
			{
				Proxy = NewObject<UControlRigVectorControlProxy>(GetTransientPackage(), NAME_None);
				break;

			}
			case ERigControlType::Vector2D:
			{
				Proxy = NewObject<UControlRigVector2DControlProxy>(GetTransientPackage(), NAME_None);
				break;

			}
			case ERigControlType::Bool:
			{
				Proxy = NewObject<UControlRigBoolControlProxy>(GetTransientPackage(), NAME_None);
				break;

			}
			default:
				break;
		}
		if (Proxy)
		{
			Proxy->bIsIndividual =
				(ControlElement->IsAnimationChannel()) ||
				(ControlElement->Settings.AnimationType == ERigControlAnimationType::ProxyControl) ;
			Proxy->SetFlags(RF_Transactional);
			Proxy->SetName(Name);
			Proxy->ControlRig = ControlRig;
			Proxy->ValueChanged();

			FControlToProxyMap* ControlRigProxies = AllProxies.Find(ControlRig);
			if (ControlRigProxies)
			{
				ControlRigProxies->ControlToProxy.Add(Name, Proxy);
			}
			else
			{
				FControlToProxyMap NewControlRigProxies;
				NewControlRigProxies.ControlToProxy.Add(Name, Proxy);
				AllProxies.Add(ControlRig, NewControlRigProxies);
			}
		}
	}
}

void UControlRigDetailPanelControlProxies::RemoveProxy(UControlRig* ControlRig, const FName& Name)
{
	UControlRigControlsProxy* ExistingProxy = FindProxy(ControlRig,Name);
	if (ExistingProxy)
	{
		ExistingProxy->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders);
		ExistingProxy->MarkAsGarbage();
	}
	FControlToProxyMap* ControlRigProxies = AllProxies.Find(ControlRig);
	if (ControlRigProxies)
	{
		ControlRigProxies->ControlToProxy.Remove(Name);
	}
}

void UControlRigDetailPanelControlProxies::RemoveAllProxies(UControlRig* ControlRig)
{
	//no control rig remove all
	if (ControlRig == nullptr)
	{
		for (TPair<TObjectPtr<UControlRig>, FControlToProxyMap>& ControlRigProxies : AllProxies)
		{
			for (TPair<FName, TObjectPtr<UControlRigControlsProxy> >& Pair : ControlRigProxies.Value.ControlToProxy)
			{
				UControlRigControlsProxy* ExistingProxy = Pair.Value;
				if (ExistingProxy)
				{
					ExistingProxy->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders);
					ExistingProxy->MarkAsGarbage();
				}
			}
		}
		AllProxies.Empty();
		SelectedProxies.SetNum(0);
	}
	else
	{
		FControlToProxyMap* ControlRigProxies = AllProxies.Find(ControlRig);
		if (ControlRigProxies)
		{
			for (TPair<FName, TObjectPtr<UControlRigControlsProxy>>& Pair : ControlRigProxies->ControlToProxy)
			{
				UControlRigControlsProxy* ExistingProxy = Pair.Value;
				if (ExistingProxy)
				{
					SelectedProxies.Remove(ExistingProxy);
					ExistingProxy->Rename(nullptr, GetTransientPackage(), REN_ForceNoResetLoaders);
					ExistingProxy->MarkAsGarbage();
				}
			}

			AllProxies.Remove(ControlRig);
		}
	}
}

void UControlRigDetailPanelControlProxies::RecreateAllProxies(UControlRig* ControlRig)
{
	RemoveAllProxies(ControlRig);
	TArray<FRigControlElement*> Controls = ControlRig->AvailableControls();
	for (FRigControlElement* ControlElement : Controls)
	{
		if(ControlElement->Settings.AnimationType != ERigControlAnimationType::VisualCue)
		{
			AddProxy(ControlRig,ControlElement->GetName(), ControlElement);
		}
	}
}

void UControlRigDetailPanelControlProxies::ProxyChanged(UControlRig* ControlRig, const FName& Name)
{
	if (IsInGameThread())
	{
		UControlRigControlsProxy* Proxy = FindProxy(ControlRig,Name);
		if (Proxy)
		{
			Modify();
			Proxy->ValueChanged();
		}
	}
}

void UControlRigDetailPanelControlProxies::SelectProxy(UControlRig* ControlRig,const FName& Name, bool bSelected)
{
	UControlRigControlsProxy* Proxy = FindProxy(ControlRig,Name);
	if (Proxy)
	{
		Modify();
		if (bSelected)
		{
			if (!SelectedProxies.Contains(Proxy))
			{
				SelectedProxies.Add(Proxy);
			}
		}
		else
		{
			SelectedProxies.Remove(Proxy);
		}
		Proxy->SelectionChanged(bSelected);
	}
}

bool UControlRigDetailPanelControlProxies::IsSelected(UControlRig* InControlRig, const FName& Name) const
{
	if (UControlRigControlsProxy* Proxy = FindProxy(InControlRig, Name))
	{
		return SelectedProxies.Contains(Proxy);
	}
	return false;
}


