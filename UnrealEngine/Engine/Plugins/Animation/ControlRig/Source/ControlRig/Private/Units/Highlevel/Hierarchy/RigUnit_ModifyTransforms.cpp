// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_ModifyTransforms.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_ModifyTransforms)

FRigUnit_ModifyTransforms_Execute()
{
    DECLARE_SCOPE_HIERARCHICAL_COUNTER_RIGUNIT()

	TArray<FCachedRigElement>& CachedItems = WorkData.CachedItems;

	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;
	if (Hierarchy)
	{
		switch (Context.State)
		{
			case EControlRigState::Init:
			{
				CachedItems.Reset();
				return;
			}
			case EControlRigState::Update:
			{
				if (CachedItems.Num() == 0)
				{
					CachedItems.SetNum(ItemToModify.Num());
				}

				float Minimum = FMath::Min<float>(WeightMinimum, WeightMaximum);
				float Maximum = FMath::Max<float>(WeightMinimum, WeightMaximum);

				if (Weight <= Minimum + SMALL_NUMBER || FMath::IsNearlyEqual(Minimum, Maximum))
				{
					return;
				}

				if (CachedItems.Num() == ItemToModify.Num())
				{
					float T = FMath::Clamp<float>((Weight - Minimum) / (Maximum - Minimum), 0.f, 1.f);
					bool bNeedsBlend = T < 1.f - SMALL_NUMBER;

					int32 EntryIndex = 0;
					for (const FRigUnit_ModifyTransforms_PerItem& Entry : ItemToModify)
					{
						FCachedRigElement& CachedItem = CachedItems[EntryIndex];
						if (!CachedItem.UpdateCache(Entry.Item, Hierarchy))
						{
							continue;
						}

						FTransform Transform = Entry.Transform;

						switch (Mode)
						{
							case EControlRigModifyBoneMode::OverrideLocal:
							{
								if (bNeedsBlend)
								{
									Transform = FControlRigMathLibrary::LerpTransform(Hierarchy->GetLocalTransform(CachedItem), Transform, T);
								}
								Hierarchy->SetLocalTransform(CachedItem, Transform, true);
								break;
							}
							case EControlRigModifyBoneMode::OverrideGlobal:
							{
								if (bNeedsBlend)
								{
									Transform = FControlRigMathLibrary::LerpTransform(Hierarchy->GetGlobalTransform(CachedItem), Transform, T);
								}
								Hierarchy->SetGlobalTransform(CachedItem, Transform, true);
								break;
							}
							case EControlRigModifyBoneMode::AdditiveLocal:
							{
								if (bNeedsBlend)
								{
									Transform = FControlRigMathLibrary::LerpTransform(FTransform::Identity, Transform, T);
								}
								Transform = Transform * Hierarchy->GetLocalTransform(CachedItem);
								Hierarchy->SetLocalTransform(CachedItem, Transform, true);
								break;
							}
							case EControlRigModifyBoneMode::AdditiveGlobal:
							{
								if (bNeedsBlend)
								{
									Transform = FControlRigMathLibrary::LerpTransform(FTransform::Identity, Transform, T);
								}
								Transform = Hierarchy->GetGlobalTransform(CachedItem) * Transform;
								Hierarchy->SetGlobalTransform(CachedItem, Transform, true);
								break;
							}
							default:
							{
								break;
							}
						}
						EntryIndex++;
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

