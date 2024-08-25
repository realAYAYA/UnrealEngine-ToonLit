// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMMaterialEffectStack.h"
#include "Components/DMMaterialEffect.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialSlot.h"
#include "DMComponentPath.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "DMMaterialEffectStack"

const FString UDMMaterialEffectStack::EffectsPathToken = FString(TEXT("Effect"));

UDMMaterialEffectStack* UDMMaterialEffectStack::CreateEffectStack(UDMMaterialSlot* InSlot)
{
	check(InSlot);

	return NewObject<UDMMaterialEffectStack>(InSlot, NAME_None, RF_Transactional);
}

UDMMaterialEffectStack* UDMMaterialEffectStack::CreateEffectStack(UDMMaterialLayerObject* InLayer)
{
	check(InLayer);

	return NewObject<UDMMaterialEffectStack>(InLayer, NAME_None, RF_Transactional);
}

UDMMaterialEffectStack::UDMMaterialEffectStack()
	: bEnabled(true)
{
	EditableProperties.Add(GET_MEMBER_NAME_CHECKED(UDMMaterialEffectStack, Effects));
}

UDMMaterialSlot* UDMMaterialEffectStack::GetSlot() const
{
	return Cast<UDMMaterialSlot>(GetOuterSafe());
}

UDMMaterialLayerObject* UDMMaterialEffectStack::GetLayer() const
{
	return Cast<UDMMaterialLayerObject>(GetOuterSafe());
}

bool UDMMaterialEffectStack::IsEnabled() const
{
	return bEnabled;
}

bool UDMMaterialEffectStack::SetEnabled(bool bInIsEnabled)
{
	if (bEnabled == bInIsEnabled)
	{
		return false;
	}

	bEnabled = bInIsEnabled;

	Update(EDMUpdateType::Structure);

	return true;
}

UDMMaterialEffect* UDMMaterialEffectStack::GetEffect(int32 InIndex) const
{
	if (Effects.IsValidIndex(InIndex))
	{
		return Effects[InIndex];
	}

	return nullptr;
}

TArray<UDMMaterialEffect*> UDMMaterialEffectStack::BP_GetEffects() const
{
	TArray<UDMMaterialEffect*> EffectObjects;
	EffectObjects.Reserve(Effects.Num());

	for (const TObjectPtr<UDMMaterialEffect>& Effect : Effects)
	{
		EffectObjects.Add(Effect);
	}

	return EffectObjects;
}

const TArray<TObjectPtr<UDMMaterialEffect>>& UDMMaterialEffectStack::GetEffects() const
{
	return Effects;
}

bool UDMMaterialEffectStack::HasEffect(const UDMMaterialEffect* InEffect) const
{
	if (!IsValid(InEffect))
	{
		return false;
	}

	for (const TObjectPtr<UDMMaterialEffect>& EffectPtr : ReverseIterate(Effects))
	{
		if (UDMMaterialEffect* Effect = EffectPtr.Get())
		{
			if (Effect == InEffect)
			{
				return true;
			}
		}
	}

	return false;
}

bool UDMMaterialEffectStack::AddEffect(UDMMaterialEffect* InEffect)
{
	if (!IsValid(InEffect))
	{
		return false;
	}

	if (GUndo)
	{
		InEffect->Modify();
	}

	if (UDMMaterialEffectStack* OldStack = InEffect->GetEffectStack())
	{
		if (GUndo)
		{
			OldStack->Modify();
		}

		OldStack->RemoveEffect(InEffect);
	}

	Effects.Add(InEffect);

	InEffect->Rename(nullptr, this, UE::DynamicMaterial::RenameFlags);

	if (IsComponentAdded())
	{
		InEffect->SetComponentState(EDMComponentLifetimeState::Added);
	}

	InEffect->Update(EDMUpdateType::Structure);

	return true;
}

bool UDMMaterialEffectStack::SetEffect(int32 InIndex, UDMMaterialEffect* InEffect)
{
	if (!IsValid(InEffect))
	{
		return false;
	}

	if (GUndo)
	{
		InEffect->Modify();
	}

	if (UDMMaterialEffectStack* OldStack = InEffect->GetEffectStack())
	{
		if (GUndo)
		{
			OldStack->Modify();
		}

		OldStack->RemoveEffect(InEffect);
	}

	Effects.Add(InEffect);

	InEffect->Rename(nullptr, this, UE::DynamicMaterial::RenameFlags);

	if (IsComponentAdded())
	{
		InEffect->SetComponentState(EDMComponentLifetimeState::Added);
	}

	InEffect->Update(EDMUpdateType::Structure);

	return true;
}

bool UDMMaterialEffectStack::MoveEffect(int32 InIndex, int32 InNewIndex)
{
	if (!IsComponentValid())
	{
		return false;
	}

	check(Effects.IsValidIndex(InIndex));

	InNewIndex = FMath::Clamp(InNewIndex, 0, Effects.Num() - 1);

	if (InNewIndex == InIndex)
	{
		return false;
	}

	UDMMaterialEffect* MovedEffect = Effects[InIndex];

	Effects.RemoveAt(InIndex, 1, EAllowShrinking::No); // Don't allow shrinking.
	Effects.Insert(MovedEffect, InNewIndex);

	const int MinIndex = FMath::Min(InIndex, InNewIndex);

	Effects[MinIndex]->Update(EDMUpdateType::Structure);

	return true;
}

bool UDMMaterialEffectStack::MoveEffect(UDMMaterialEffect* InEffect, int32 InNewIndex)
{
	if (!IsValid(InEffect))
	{
		return false;
	}

	const int32 EffectIndex = Effects.IndexOfByPredicate(
		[InEffect](const TObjectPtr<UDMMaterialEffect>& InElement)
		{
			return InEffect == InElement;
		}
	);

	if (EffectIndex != INDEX_NONE)
	{
		return MoveEffect(EffectIndex, InNewIndex);
	}

	return false;
}

bool UDMMaterialEffectStack::RemoveEffect(int32 InIndex)
{
	if (!Effects.IsValidIndex(InIndex))
	{
		return false;
	}

	UDMMaterialEffect* Effect = Effects[InIndex];

	if (GUndo)
	{
		Effect->Modify();
	}

	Effect->SetEnabled(false);
	Effect->Rename(nullptr, GetTransientPackage(), UE::DynamicMaterial::RenameFlags);
	Effect->SetComponentState(EDMComponentLifetimeState::Removed);

	Effects.RemoveAt(InIndex);

	Update(EDMUpdateType::Structure);

	return true;
}

bool UDMMaterialEffectStack::RemoveEffect(UDMMaterialEffect* InEffect)
{
	if (!IsValid(InEffect))
	{
		return false;
	}

	const int32 EffectIndex = Effects.IndexOfByPredicate(
		[InEffect](const TObjectPtr<UDMMaterialEffect>& InElement)
		{
			return InEffect == InElement;
		}
	);

	if (EffectIndex != INDEX_NONE)
	{
		return RemoveEffect(EffectIndex);
	}

	return false;
}

bool UDMMaterialEffectStack::ApplyEffects(const TSharedRef<FDMMaterialBuildState>& InBuildState, EDMMaterialEffectTarget InEffectTarget,
	TArray<UMaterialExpression*>& InOutStageExpressions, int32& InOutLastExpressionOutputChannel, int32& InOutLastExpressionOutputIndex) const
{
	bool bAppliedEffect = false;

	for (const TObjectPtr<UDMMaterialEffect>& EffectPtr : Effects)
	{
		if (UDMMaterialEffect* Effect = EffectPtr.Get())
		{
			if (IsValid(Effect) && InEffectTarget == Effect->GetEffectTarget() && Effect->IsEnabled())
			{
				Effect->ApplyTo(InBuildState, InOutStageExpressions, InOutLastExpressionOutputChannel, InOutLastExpressionOutputIndex);
				bAppliedEffect = true;
			}
		}
	}

	return bAppliedEffect;
}

UDMMaterialComponent* UDMMaterialEffectStack::GetParentComponent() const
{
	UObject* Outer = GetOuterSafe();

	if (UDMMaterialSlot* Slot = Cast<UDMMaterialSlot>(Outer))
	{
		return Slot;
	}

	if (UDMMaterialLayerObject* Layer = Cast<UDMMaterialLayerObject>(Outer))
	{
		return Layer;
	}

	return nullptr;
}

FString UDMMaterialEffectStack::GetComponentPathComponent() const
{
	return UDMMaterialLayerObject::EffectStackPathToken;
}

FText UDMMaterialEffectStack::GetComponentDescription() const
{
	static const FText Description = LOCTEXT("EffectStack", "Effect Stack");
	return Description;
}

void UDMMaterialEffectStack::Update(EDMUpdateType InUpdateType)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (HasComponentBeenRemoved())
	{
		return;
	}

	Super::Update(InUpdateType);

	if (UDMMaterialComponent* Parent = GetParentComponent())
	{
		Parent->Update(InUpdateType);
	}
}

void UDMMaterialEffectStack::PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent)
{
	Super::PostEditorDuplicate(InMaterialModel, InParent);

	if (GetOuter() != InParent)
	{
		Rename(nullptr, InParent, UE::DynamicMaterial::RenameFlags);
	}

	for (const TObjectPtr<UDMMaterialEffect>& Effect : Effects)
	{
		if (Effect)
		{
			Effect->PostEditorDuplicate(InMaterialModel, this);
		}
	}
}

bool UDMMaterialEffectStack::Modify(bool bInAlwaysMarkDirty)
{
	const bool bSaved = Super::Modify(bInAlwaysMarkDirty);

	for (const TObjectPtr<UDMMaterialEffect>& Effect : Effects)
	{
		if (Effect)
		{
			Effect->Modify(bInAlwaysMarkDirty);
		}
	}

	return bSaved;
}

void UDMMaterialEffectStack::PostEditUndo()
{
	Super::PostEditUndo();

	if (!IsComponentValid())
	{
		return;
	}

	UDMMaterialComponent* Parent = GetParentComponent();

	if (!Parent)
	{
		SetComponentState(EDMComponentLifetimeState::Removed);
		return;
	}

	MarkComponentDirty();

	Update(EDMUpdateType::Structure);
}

UDMMaterialComponent* UDMMaterialEffectStack::GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const
{
	if (InPathSegment.GetToken() == EffectsPathToken)
	{
		int32 EffectIndex;

		if (InPathSegment.GetParameter(EffectIndex))
		{
			if (Effects.IsValidIndex(EffectIndex))
			{
				return Effects[EffectIndex]->GetComponentByPath(InPath);
			}
		}
	}

	return Super::GetSubComponentByPath(InPath, InPathSegment);
}

void UDMMaterialEffectStack::OnComponentAdded()
{
	Super::OnComponentAdded();

	if (!IsComponentValid())
	{
		return;
	}

	for (const TObjectPtr<UDMMaterialEffect>& Effect : Effects)
	{
		if (Effect)
		{
			Effect->SetComponentState(EDMComponentLifetimeState::Added);
		}
	}
}

void UDMMaterialEffectStack::OnComponentRemoved()
{
	Super::OnComponentRemoved();

	for (const TObjectPtr<UDMMaterialEffect>& Effect : Effects)
	{
		if (Effect)
		{
			Effect->SetComponentState(EDMComponentLifetimeState::Removed);
		}
	}
}

#undef LOCTEXT_NAMESPACE
