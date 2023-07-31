// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_SetControlOffset.h"

#include "GeometryCollection/GeometryCollectionAlgo.h"
#include "Units/RigUnitContext.h"
#include "Math/ControlRigMathLibrary.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_SetControlOffset)

FRigUnit_SetControlOffset_Execute()
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
				if (!CachedControlIndex.UpdateCache(FRigElementKey(Control, ERigElementType::Control), Hierarchy))
				{
					UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Control '%s' is not valid."), *Control.ToString());
					return;
				}

				FRigControlElement* ControlElement = Hierarchy->Get<FRigControlElement>(CachedControlIndex);
				if (Space == EBoneGetterSetterMode::GlobalSpace)
				{
					Hierarchy->SetControlOffsetTransform(ControlElement, Offset, ERigTransformType::CurrentGlobal, true, false);
					Hierarchy->SetControlOffsetTransform(ControlElement, Offset, ERigTransformType::InitialGlobal, true, false);
				}
				else
				{
					Hierarchy->SetControlOffsetTransform(ControlElement, Offset, ERigTransformType::CurrentLocal, true, false);
					Hierarchy->SetControlOffsetTransform(ControlElement, Offset, ERigTransformType::InitialLocal, true, false);
				}
			}
			default:
			{
				break;
			}
		}
	}
}

FRigUnit_GetShapeTransform_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

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
				if (!CachedControlIndex.UpdateCache(FRigElementKey(Control, ERigElementType::Control), Hierarchy))
				{
					UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Control '%s' is not valid."), *Control.ToString());
					return;
				}

				const FRigControlElement* ControlElement = Hierarchy->Get<FRigControlElement>(CachedControlIndex);
				Transform = Hierarchy->GetControlShapeTransform((FRigControlElement*)ControlElement, ERigTransformType::CurrentLocal);
				break;
			}
			default:
			{
				break;
			}
		}
	}
}

FRigUnit_SetShapeTransform_Execute()
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
				if (!CachedControlIndex.UpdateCache(FRigElementKey(Control, ERigElementType::Control), Hierarchy))
				{
					UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Control '%s' is not valid."), *Control.ToString());
					return;
				}

				FRigControlElement* ControlElement = Hierarchy->Get<FRigControlElement>(CachedControlIndex);
				Hierarchy->SetControlShapeTransform(ControlElement, Transform, ERigTransformType::InitialLocal);
				break;
			}
			default:
			{
				break;
			}
		}
	}
}
