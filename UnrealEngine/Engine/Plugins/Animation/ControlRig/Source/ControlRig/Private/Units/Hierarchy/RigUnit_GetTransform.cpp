// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_GetTransform.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_GetTransform)

FString FRigUnit_GetTransform::GetUnitLabel() const
{
	FString Initial = bInitial ? TEXT(" Initial") : FString();
	FString Type = StaticEnum<ERigElementType>()->GetDisplayNameTextByValue((int64)Item.Type).ToString();
	return FString::Printf(TEXT("Get Transform - %s%s"), *Type, *Initial);
}

FRigUnit_GetTransform_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if (URigHierarchy* Hierarchy = Context.Hierarchy)
	{
		switch (Context.State)
		{
			case EControlRigState::Init:
			{
				CachedIndex.Reset();
				// there is no "break;" here because an old node, Transform Constrtaint,
				// still caches data during init stage. Thus, if a it takes a GetTransform
				// as input, that GetTransform needs to execute and output a valid transform					
			}
			case EControlRigState::Update:
			{
				if (!CachedIndex.UpdateCache(Item, Hierarchy))
				{
					UE_CONTROLRIG_RIGUNIT_REPORT_WARNING(TEXT("Item '%s' is not valid."), *Item.ToString());
				}
				else
				{
					if(bInitial || Context.State == EControlRigState::Init)
					{
						switch (Space)
						{
							case EBoneGetterSetterMode::GlobalSpace:
							{
								Transform = Hierarchy->GetInitialGlobalTransform(CachedIndex);
								break;
							}
							case EBoneGetterSetterMode::LocalSpace:
							{
								Transform = Hierarchy->GetInitialLocalTransform(CachedIndex);
								break;
							}
							default:
							{
								break;
							}
						}
					}
					else
					{
						switch (Space)
						{
							case EBoneGetterSetterMode::GlobalSpace:
							{
								Transform = Hierarchy->GetGlobalTransform(CachedIndex);
								break;
							}
							case EBoneGetterSetterMode::LocalSpace:
							{
								Transform = Hierarchy->GetLocalTransform(CachedIndex);
								break;
							}
							default:
							{
								break;
							}
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

FRigUnit_GetTransformArray_Execute()
{
	FRigUnit_GetTransformItemArray::StaticExecute(RigVMExecuteContext, Items.Keys, Space, bInitial, Transforms, CachedIndex, Context);
}

FRigVMStructUpgradeInfo FRigUnit_GetTransformArray::GetUpgradeInfo() const
{
	FRigUnit_GetTransformItemArray NewNode;
	NewNode.Items = Items.GetKeys();
	NewNode.Space = Space;
	NewNode.bInitial = bInitial;
	return FRigVMStructUpgradeInfo(*this, NewNode);
}

FRigUnit_GetTransformItemArray_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	if(CachedIndex.Num() != Items.Num())
	{
		CachedIndex.Reset();
		CachedIndex.SetNum(Items.Num());
	}

	Transforms.SetNum(Items.Num());
	for(int32 Index=0;Index<Items.Num();Index++)
	{
		FRigUnit_GetTransform::StaticExecute(RigVMExecuteContext, Items[Index], Space, bInitial, Transforms[Index], CachedIndex[Index], Context);

	}
}

