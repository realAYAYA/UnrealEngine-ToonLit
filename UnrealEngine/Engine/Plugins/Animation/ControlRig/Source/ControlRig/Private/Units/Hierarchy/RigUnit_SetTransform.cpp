// Copyright Epic Games, Inc. All Rights Reserved.

#include "Units/Hierarchy/RigUnit_SetTransform.h"
#include "RigUnit_GetTransform.h"
#include "Units/RigUnitContext.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Hierarchy/RigUnit_SetControlOffset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_SetTransform)

FString FRigUnit_SetTransform::GetUnitLabel() const
{
	FString Initial = bInitial ? TEXT(" Initial") : FString();
	FString Type = StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)Item.Type).ToString();
	return FString::Printf(TEXT("Set Transform - %s%s"), *Type, *Initial);
}

FRigUnit_SetTransform_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if (Weight < SMALL_NUMBER)
	{
		return;
	}

	if (URigHierarchy* Hierarchy = ExecuteContext.Hierarchy)
	{
		switch (Context.State)
		{
			case EControlRigState::Init:
			{
				CachedIndex.Reset();

				return;
			}
			case EControlRigState::Update:
			{
				if (!CachedIndex.UpdateCache(Item, Hierarchy))
				{
					UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Item '%s' is not valid."), *Item.ToString());
				}
				else
				{
#if WITH_EDITOR
					if(bInitial)
					{
						// provide some user feedback when changing initial transforms during forward solve
						if(RigVMExecuteContext.GetEventName() == FRigUnit_BeginExecution::EventName)
						{
							UE_CONTROLRIG_RIGUNIT_LOG_MESSAGE(TEXT("Changing initial transforms during %s is not recommended."), *RigVMExecuteContext.GetEventName().ToString());
						}
					}
#endif

					// for controls - set the control offset transform instead
					if(bInitial && (CachedIndex.GetKey().Type == ERigElementType::Control))
					{
						FTransform TransformMutable = Value;
						FRigUnit_SetControlOffset::StaticExecute(RigVMExecuteContext, CachedIndex.GetKey().Name, TransformMutable, Space, CachedIndex, ExecuteContext, Context);
						
						if (ExecuteContext.GetEventName() == FRigUnit_PrepareForExecution::EventName)
						{
							Hierarchy->SetLocalTransformByIndex(CachedIndex, FTransform::Identity, true, bPropagateToChildren);
							Hierarchy->SetLocalTransformByIndex(CachedIndex, FTransform::Identity, false, bPropagateToChildren);
						}
						return;
					}
					
					FTransform WeightedTransform = Value;
					if (Weight < 1.f - SMALL_NUMBER)
					{
						FTransform PreviousTransform = WeightedTransform;
						switch (Space)
						{
							case EBoneGetterSetterMode::GlobalSpace:
							{
								PreviousTransform = Hierarchy->GetGlobalTransformByIndex(CachedIndex, bInitial);
								break;
							}
							case EBoneGetterSetterMode::LocalSpace:
							{
								PreviousTransform = Hierarchy->GetLocalTransformByIndex(CachedIndex, bInitial);
								break;
							}
							default:
							{
								break;
							}
						}
						WeightedTransform = FControlRigMathLibrary::LerpTransform(PreviousTransform, WeightedTransform, Weight);
					}

					switch (Space)
					{
						case EBoneGetterSetterMode::GlobalSpace:
						{
							Hierarchy->SetGlobalTransformByIndex(CachedIndex, WeightedTransform, bInitial, bPropagateToChildren);

							if (bInitial && ExecuteContext.GetEventName() == FRigUnit_PrepareForExecution::EventName)
							{
								Hierarchy->SetGlobalTransformByIndex(CachedIndex, WeightedTransform, false, bPropagateToChildren);
							}
							break;
						}
						case EBoneGetterSetterMode::LocalSpace:
						{
							Hierarchy->SetLocalTransformByIndex(CachedIndex, WeightedTransform, bInitial, bPropagateToChildren);

							if (bInitial && ExecuteContext.GetEventName() == FRigUnit_PrepareForExecution::EventName)
							{
								Hierarchy->SetLocalTransformByIndex(CachedIndex, WeightedTransform, false, bPropagateToChildren);
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
			default:
			{
				break;
			}
		}
	}
}

FString FRigUnit_SetTranslation::GetUnitLabel() const
{
	FString Type = StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)Item.Type).ToString();
	return FString::Printf(TEXT("Set Translation - %s"), *Type);
}

FRigUnit_SetTranslation_Execute()
{
	FTransform Transform = FTransform::Identity;
	FRigUnit_GetTransform::StaticExecute(RigVMExecuteContext, Item, Space, bInitial, Transform, CachedIndex, Context);
	Transform.SetLocation(Value);
	FRigUnit_SetTransform::StaticExecute(RigVMExecuteContext, Item, Space, bInitial, Transform, Weight, bPropagateToChildren, CachedIndex, ExecuteContext, Context);
}

FString FRigUnit_SetRotation::GetUnitLabel() const
{
	FString Type = StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)Item.Type).ToString();
	return FString::Printf(TEXT("Set Rotation - %s"), *Type);
}

FRigUnit_SetRotation_Execute()
{
	FTransform Transform = FTransform::Identity;
	FRigUnit_GetTransform::StaticExecute(RigVMExecuteContext, Item, Space, bInitial, Transform, CachedIndex, Context);
	Transform.SetRotation(Value);
	FRigUnit_SetTransform::StaticExecute(RigVMExecuteContext, Item, Space, bInitial, Transform, Weight, bPropagateToChildren, CachedIndex, ExecuteContext, Context);
}

FString FRigUnit_SetScale::GetUnitLabel() const
{
	FString Type = StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)Item.Type).ToString();
	return FString::Printf(TEXT("Set Scale - %s"), *Type);
}

FRigUnit_SetScale_Execute()
{
	FTransform Transform = FTransform::Identity;
	FRigUnit_GetTransform::StaticExecute(RigVMExecuteContext, Item, Space, bInitial, Transform, CachedIndex, Context);
	Transform.SetScale3D(Scale);
	FRigUnit_SetTransform::StaticExecute(RigVMExecuteContext, Item, Space, bInitial, Transform, Weight, bPropagateToChildren, CachedIndex, ExecuteContext, Context);
}


FRigUnit_SetTransformArray_Execute()
{
	FRigUnit_SetTransformItemArray::StaticExecute(RigVMExecuteContext, Items.Keys, Space, bInitial, Transforms, Weight, bPropagateToChildren, CachedIndex, ExecuteContext, Context);
}

FRigVMStructUpgradeInfo FRigUnit_SetTransformArray::GetUpgradeInfo() const
{
	FRigUnit_SetTransformItemArray NewNode;
	NewNode.Items = Items.GetKeys();
	NewNode.Space = Space;
	NewNode.bInitial = bInitial;
	NewNode.Transforms = Transforms;
	NewNode.Weight = Weight;
	NewNode.bPropagateToChildren = bPropagateToChildren;

	return FRigVMStructUpgradeInfo(*this, NewNode);
}

FRigUnit_SetTransformItemArray_Execute()
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if(CachedIndex.Num() != Items.Num())
	{
		CachedIndex.Reset();
		CachedIndex.SetNum(Items.Num());
	}

	if(Transforms.Num() != Items.Num())
	{
		UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("The number of transforms (%d) doesn't match the size of the collection (%d)."), Transforms.Num(), Items.Num());
		return;
	}

	for(int32 Index=0;Index<Items.Num();Index++)
	{
		FRigUnit_SetTransform::StaticExecute(RigVMExecuteContext, Items[Index], Space, bInitial, Transforms[Index], Weight, bPropagateToChildren, CachedIndex[Index], ExecuteContext, Context);
	}
}
