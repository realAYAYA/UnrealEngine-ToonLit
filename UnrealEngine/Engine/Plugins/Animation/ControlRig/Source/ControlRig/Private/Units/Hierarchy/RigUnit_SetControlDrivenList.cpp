// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_SetControlDrivenList.h"

#include "Rigs/RigHierarchyController.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_SetControlDrivenList)

FRigUnit_GetControlDrivenList_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

    Driven.Reset();

	const URigHierarchy* Hierarchy = Context.Hierarchy;
	if (Hierarchy)
	{
		switch (Context.State)
		{
			case EControlRigState::Init:
			{
				CachedControlIndex.Reset();
				break;
			}
			case EControlRigState::Update:
			{
				if (CachedControlIndex.UpdateCache(FRigElementKey(Control, ERigElementType::Control), Hierarchy))
				{
					const FRigControlElement* ControlElement = Hierarchy->GetChecked<FRigControlElement>(CachedControlIndex);
					Driven = ControlElement->Settings.DrivenControls;
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

FRigUnit_SetControlDrivenList_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	if (Hierarchy)
	{
		switch (Context.State)
		{
			case EControlRigState::Init:
			{
				CachedControlIndex.Reset();
				break;
			}
			case EControlRigState::Update:
			{
				if (CachedControlIndex.UpdateCache(FRigElementKey(Control, ERigElementType::Control), Hierarchy))
				{
					FRigControlElement* ControlElement = Hierarchy->GetChecked<FRigControlElement>(CachedControlIndex);
					if(ControlElement->Settings.DrivenControls != Driven)
					{
						Swap(ControlElement->Settings.DrivenControls, ControlElement->Settings.PreviouslyDrivenControls);
						ControlElement->Settings.DrivenControls = Driven;
						Hierarchy->Notify(ERigHierarchyNotification::ControlDrivenListChanged, ControlElement);
					}
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

