// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_ControlChannel.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_ControlChannel)

bool FRigUnit_GetAnimationChannelBase::UpdateCache(const URigHierarchy* InHierarchy, const FName& Control, const FName& Channel, FRigElementKey& Key, int32& Hash)
{
	if(!Key.IsValid())
	{
		Hash = INDEX_NONE;
	}

	const int32 ExpectedHash = (int32)HashCombine(GetTypeHash(InHierarchy->GetTopologyVersion()), HashCombine(GetTypeHash(Control), GetTypeHash(Channel)));
	if(ExpectedHash != Hash)
	{
		if(const FRigControlElement* ControlElement = InHierarchy->Find<FRigControlElement>(FRigElementKey(Control, ERigElementType::Control)))
		{
			FRigBaseElementChildrenArray Children = InHierarchy->GetChildren(ControlElement);
			for(const FRigBaseElement* Child : Children)
			{
				if(const FRigControlElement* ChildControl = Cast<FRigControlElement>(Child))
				{
					if(ChildControl->IsAnimationChannel())
					{
						if(ChildControl->GetDisplayName() == Channel)
						{
							Key = ChildControl->GetKey();
							Hash = ExpectedHash;
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	return true;
}

FRigUnit_GetBoolAnimationChannel_Execute()
{
	Value = false;
	
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Bool)
		{
			const FRigControlValue StoredValue = Context.Hierarchy->GetControlValueByIndex(ChannelElement->GetIndex(), bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
			Value = StoredValue.Get<bool>();
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a Bool."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_GetFloatAnimationChannel_Execute()
{
	Value = 0.f;
	
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Float)
		{
			const FRigControlValue StoredValue = Context.Hierarchy->GetControlValueByIndex(ChannelElement->GetIndex(), bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
			Value = StoredValue.Get<float>();
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a Float."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_GetIntAnimationChannel_Execute()
{
	Value = 0;
	
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Integer)
		{
			const FRigControlValue StoredValue = Context.Hierarchy->GetControlValueByIndex(ChannelElement->GetIndex(), bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
			Value = StoredValue.Get<int32>();
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not an Integer (or Enum)."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_GetVector2DAnimationChannel_Execute()
{
	Value = FVector2D::ZeroVector;
	
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Vector2D)
		{
			const FRigControlValue StoredValue = Context.Hierarchy->GetControlValueByIndex(ChannelElement->GetIndex(), bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
			Value = FVector2D(StoredValue.Get<FVector2f>());
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a Vector2D."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_GetVectorAnimationChannel_Execute()
{
	Value = FVector::ZeroVector;
	
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Position || ChannelElement->Settings.ControlType == ERigControlType::Scale)
		{
			const FRigControlValue StoredValue = Context.Hierarchy->GetControlValueByIndex(ChannelElement->GetIndex(), bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
			Value = FVector(StoredValue.Get<FVector3f>());
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a Vector (Position or Scale)."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_GetRotatorAnimationChannel_Execute()
{
	Value = FRotator::ZeroRotator;
	
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Rotator)
		{
			const FRigControlValue StoredValue = Context.Hierarchy->GetControlValueByIndex(ChannelElement->GetIndex(), bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
			Value = FRotator::MakeFromEuler(FVector(StoredValue.Get<FVector3f>()));
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a Rotator."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_GetTransformAnimationChannel_Execute()
{
	Value = FTransform::Identity;
	
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Transform)
		{
			const FRigControlValue StoredValue = Context.Hierarchy->GetControlValueByIndex(ChannelElement->GetIndex(), bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
			Value = StoredValue.Get<FRigControlValue::FTransform_Float>().ToTransform();
		}
		else if(ChannelElement->Settings.ControlType == ERigControlType::EulerTransform)
		{
			const FRigControlValue StoredValue = Context.Hierarchy->GetControlValueByIndex(ChannelElement->GetIndex(), bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
			Value = StoredValue.Get<FRigControlValue::FEulerTransform_Float>().ToTransform().ToFTransform();
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a EulerTransform / Transform."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_SetBoolAnimationChannel_Execute()
{
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Bool)
		{
			const FRigControlValue ValueToStore = FRigControlValue::Make<bool>(Value);
			Context.Hierarchy->SetControlValueByIndex(ChannelElement->GetIndex(), ValueToStore, bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a Bool."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_SetFloatAnimationChannel_Execute()
{
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Float)
		{
			const FRigControlValue ValueToStore = FRigControlValue::Make<float>(Value);
			Context.Hierarchy->SetControlValueByIndex(ChannelElement->GetIndex(), ValueToStore, bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a Float."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_SetIntAnimationChannel_Execute()
{
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Integer)
		{
			const FRigControlValue ValueToStore = FRigControlValue::Make<int32>(Value);
			Context.Hierarchy->SetControlValueByIndex(ChannelElement->GetIndex(), ValueToStore, bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not an Integer (or Enum)."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_SetVector2DAnimationChannel_Execute()
{
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Vector2D)
		{
			const FRigControlValue ValueToStore = FRigControlValue::Make<FVector2f>(FVector2f(Value));
			Context.Hierarchy->SetControlValueByIndex(ChannelElement->GetIndex(), ValueToStore, bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a Vector2D."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_SetVectorAnimationChannel_Execute()
{
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Position || ChannelElement->Settings.ControlType == ERigControlType::Scale)
		{
			const FRigControlValue ValueToStore = FRigControlValue::Make<FVector3f>(FVector3f(Value));
			Context.Hierarchy->SetControlValueByIndex(ChannelElement->GetIndex(), ValueToStore, bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a Vector (Position or Scale)."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_SetRotatorAnimationChannel_Execute()
{
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Rotator)
		{
			const FRigControlValue ValueToStore = FRigControlValue::Make<FVector3f>(FVector3f(Value.Euler()));
			Context.Hierarchy->SetControlValueByIndex(ChannelElement->GetIndex(), ValueToStore, bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a Rotator."), *CachedChannelKey.ToString());
		}
	}
}

FRigUnit_SetTransformAnimationChannel_Execute()
{
	if(!UpdateCache(Context.Hierarchy, Control, Channel, CachedChannelKey, CachedChannelHash))
	{
		return;
	}

	if(const FRigControlElement* ChannelElement = Context.Hierarchy->Find<FRigControlElement>(CachedChannelKey))
	{
		if(ChannelElement->Settings.ControlType == ERigControlType::Transform)
		{
			const FRigControlValue ValueToStore = FRigControlValue::Make<FRigControlValue::FTransform_Float>(Value);
			Context.Hierarchy->SetControlValueByIndex(ChannelElement->GetIndex(), ValueToStore, bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
		}
		else if(ChannelElement->Settings.ControlType == ERigControlType::EulerTransform)
		{
			const FRigControlValue ValueToStore = FRigControlValue::Make<FRigControlValue::FEulerTransform_Float>(FEulerTransform(Value));
			Context.Hierarchy->SetControlValueByIndex(ChannelElement->GetIndex(), ValueToStore, bInitial ? ERigControlValueType::Initial : ERigControlValueType::Current);
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Animation Channel %s is not a EulerTransform / Transform."), *CachedChannelKey.ToString());
		}
	}
}
