// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_SetControlVisibility.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_SetControlVisibility)

FRigUnit_GetControlVisibility_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

    bVisible = false;

	const URigHierarchy* Hierarchy = Context.Hierarchy;
	if (Hierarchy)
	{
		switch (Context.State)
		{
			case EControlRigState::Init:
			{
				CachedControlIndex.Reset();
			}
			case EControlRigState::Update:
			{
				if (CachedControlIndex.UpdateCache(Item, Hierarchy))
				{
					if(const FRigControlElement* ControlElement = Hierarchy->Get<FRigControlElement>(CachedControlIndex))
					{
						bVisible = ControlElement->Settings.bShapeVisible;
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

FRigUnit_SetControlVisibility_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()
	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	if (Hierarchy)
	{
		switch (Context.State)
		{
			case EControlRigState::Init:
			{
				CachedControlIndices.Reset();
			}
			case EControlRigState::Update:
			{
				TArray<FRigElementKey> Keys;

				if (Item.IsValid())
				{
					if (Item.Type != ERigElementType::Control)
					{
						return;
					}

					Keys.Add(Item);
				}
				else if (!Pattern.IsEmpty())
				{
					Hierarchy->ForEach<FRigControlElement>([&Keys, Pattern](FRigControlElement* ControlElement) -> bool
					{
						if (ControlElement->GetName().ToString().Contains(Pattern, ESearchCase::CaseSensitive))
						{
							Keys.Add(ControlElement->GetKey());
						}
						return true;
					});
				}

				if (CachedControlIndices.Num() != Keys.Num())
				{
					CachedControlIndices.Reset();
					CachedControlIndices.SetNumZeroed(Keys.Num());
				}

				for (int32 Index = 0; Index < Keys.Num(); Index++)
				{
					CachedControlIndices[Index].UpdateCache(Keys[Index], Hierarchy);
				}

				for (const FCachedRigElement& CachedControlIndex : CachedControlIndices)
				{
					if (CachedControlIndex.IsValid())
					{
						Hierarchy->SetControlVisibility(CachedControlIndex, bVisible);
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

