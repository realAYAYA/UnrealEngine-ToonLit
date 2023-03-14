// Copyright Epic Games, Inc. All Rights Reserved.

#include "Text3DCharacterTransform.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Text3DComponent.h"


UText3DCharacterTransform::UText3DCharacterTransform()
{
	bLocationEnabled = false;
	LocationProgress = 0.0f;
	LocationOrder = EText3DCharacterEffectOrder::Normal;
	LocationRange = 50.0f;
	LocationDistance = FVector(100.0f, 0.0f, 0.0f);

	bScaleEnabled = false;
	ScaleProgress = 0.0f;
	ScaleOrder = EText3DCharacterEffectOrder::Normal;
	ScaleRange = 50.0f;
	ScaleBegin = FVector(1.0f, 0.0f, 0.0f);
	ScaleEnd = FVector(1.0f);

	bRotateEnabled = false;
	RotateProgress = 0.0f;
	RotateOrder = EText3DCharacterEffectOrder::Normal;
	RotateRange = 50.0f;
	RotateBegin = FRotator(-90.0f, 0.0f, 0.0f);
	RotateEnd = FRotator(0.0f, 0.0f, 0.0f);

	bInitialized = false;
}

void UText3DCharacterTransform::OnRegister()
{
	Super::OnRegister();

	if (!bInitialized)
	{
		bInitialized = true;
		ProcessEffect();
	}

	if (UText3DComponent* Text3DComponent = GetText3DComponent())
	{
		Text3DComponent->OnTextGenerated().AddUObject(this, &UText3DCharacterTransform::ProcessEffect);
	}
}

void UText3DCharacterTransform::OnUnregister()
{
	if (UText3DComponent* Text3DComponent = GetText3DComponent())
	{
		Text3DComponent->OnTextGenerated().RemoveAll(this);
	}

	Super::OnUnregister();
}

#if WITH_EDITOR
void UText3DCharacterTransform::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName Name = PropertyChangedEvent.GetPropertyName();
	if (Name == GET_MEMBER_NAME_CHECKED(UText3DCharacterTransform, bLocationEnabled))
	{
		ResetLocation();
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UText3DCharacterTransform, bScaleEnabled))
	{
		ResetScale();
	}
	else if (Name == GET_MEMBER_NAME_CHECKED(UText3DCharacterTransform, bRotateEnabled))
	{
		ResetRotate();
	}

	ProcessEffect();
}
#endif

void UText3DCharacterTransform::ProcessEffect()
{
	UText3DComponent* Text3DComponent = GetText3DComponent();
	if (!Text3DComponent)
	{
		return;
	}

	int32 Count = Text3DComponent->GetGlyphCount();
	for (int32 Index = 0; Index < Count; Index++)
	{
		USceneComponent* GlyphComponent = Text3DComponent->GetGlyphMeshComponent(Index);
		if (!GlyphComponent)
		{
			continue;
		}

		if (bLocationEnabled)
		{
			float Effect = GetEffectValue(Index, Count, LocationOrder, LocationProgress, LocationRange);
			FVector Location = LocationDistance * Effect;
			GlyphComponent->SetRelativeLocation(Location);
		}

		if (bScaleEnabled)
		{
			float Effect = GetEffectValue(Index, Count, ScaleOrder, ScaleProgress, ScaleRange);
			FVector Scale = ScaleBegin + ((ScaleEnd - ScaleBegin) * Effect);
			GlyphComponent->SetRelativeScale3D(Scale);
		}

		if (bRotateEnabled)
		{
			float Effect = GetEffectValue(Index, Count, RotateOrder, RotateProgress, RotateRange);
			FRotator Rotator = RotateBegin + (RotateEnd - RotateBegin) * Effect;
			GlyphComponent->SetRelativeRotation(Rotator);
		}
	}
}

void UText3DCharacterTransform::ResetLocation()
{
	UText3DComponent* Text3DComponent = GetText3DComponent();
	if (!Text3DComponent)
	{
		return;
	}

	int32 Count = Text3DComponent->GetGlyphCount();
	for (int32 Index = 0; Index < Count; Index++)
	{
		USceneComponent* GlyphComponent = Text3DComponent->GetGlyphMeshComponent(Index);
		if (GlyphComponent)
		{
			GlyphComponent->SetRelativeLocation(FVector::ZeroVector);
		}
	}
}

void UText3DCharacterTransform::ResetRotate()
{
	UText3DComponent* Text3DComponent = GetText3DComponent();
	if (!Text3DComponent)
	{
		return;
	}

	int32 Count = Text3DComponent->GetGlyphCount();
	for (int32 Index = 0; Index < Count; Index++)
	{
		USceneComponent* GlyphComponent = Text3DComponent->GetGlyphMeshComponent(Index);
		if (GlyphComponent)
		{
			GlyphComponent->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
}

void UText3DCharacterTransform::ResetScale()
{
	UText3DComponent* Text3DComponent = GetText3DComponent();
	if (!Text3DComponent)
	{
		return;
	}

	int32 Count = Text3DComponent->GetGlyphCount();
	for (int32 Index = 0; Index < Count; Index++)
	{
		USceneComponent* GlyphComponent = Text3DComponent->GetGlyphMeshComponent(Index);
		if (GlyphComponent)
		{
			GlyphComponent->SetRelativeScale3D(FVector(1.0f));
		}
	}
}

float UText3DCharacterTransform::GetEffectValue(int32 Index, int32 Total, EText3DCharacterEffectOrder Order, float Progress, float Range)
{
	float Effect = 0.0f, Strip = 1.0f;
	GetLineParameters(Range * 0.01f, Order, Total, Effect, Strip);

	int32 Position = GetEffectPosition(Index, Total, Order);
	return FMath::Clamp((Progress * 0.01f - Effect * Position) / Strip, 0.0f, 1.0f);
}

void UText3DCharacterTransform::GetLineParameters(float Range, EText3DCharacterEffectOrder Order, int32 Total, float & EffectOut, float & StripOut)
{
	float Segment = 1.0f / Total;
	EffectOut = (1.0f - Range) / Total;
	StripOut = FMath::Max(Range, Segment);

	if (Order == EText3DCharacterEffectOrder::FromCenter || Order == EText3DCharacterEffectOrder::ToCenter)
	{
		int32 Center = Total / 2.0f - 0.5f;
		EffectOut *= (float)Total / (Center + 1.0f);
	}

	if (FMath::IsNearlyZero(EffectOut))
	{
		EffectOut = 0.01f;
	}
}

int32 UText3DCharacterTransform::GetEffectPosition(int32 Index, int32 Total, EText3DCharacterEffectOrder Order)
{
	int32 Center = 0.5f * Total - 0.5f;
	switch (Order)
	{
	case EText3DCharacterEffectOrder::FromCenter:
	{
		if (Index > Center)
		{
			Index = Total - Index - 1;
		}

		return Center - Index;
	}

	case EText3DCharacterEffectOrder::ToCenter:
	{
		if (Index > Center)
		{
			return Total - Index - 1;
		}

		return Index;
	}

	case EText3DCharacterEffectOrder::Opposite:
	{
		return Total - Index - 1;
	}
	}

	return Index;
}

void UText3DCharacterTransform::SetLocationEnabled(bool bEnabled)
{
	if (bLocationEnabled != bEnabled)
	{
		bLocationEnabled = bEnabled;
		ResetLocation();
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetLocationProgress(float Progress)
{
	Progress = FMath::Clamp(Progress, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(LocationProgress, Progress))
	{
		LocationProgress = Progress;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetLocationOrder(EText3DCharacterEffectOrder Order)
{
	if (LocationOrder != Order)
	{
		LocationOrder = Order;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetLocationRange(float Range)
{
	Range = FMath::Clamp(Range, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(LocationRange, Range))
	{
		LocationRange = Range;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetLocationDistance(FVector Distance)
{
	if (!(LocationDistance - Distance).IsNearlyZero())
	{
		LocationDistance = Distance;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetScaleEnabled(bool bEnabled)
{
	if (bScaleEnabled != bEnabled)
	{
		bScaleEnabled = bEnabled;
		ResetScale();
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetScaleProgress(float Progress)
{
	Progress = FMath::Clamp(Progress, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(ScaleProgress, Progress))
	{
		ScaleProgress = Progress;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetScaleOrder(EText3DCharacterEffectOrder Order)
{
	if (ScaleOrder != Order)
	{
		ScaleOrder = Order;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetScaleRange(float Range)
{
	Range = FMath::Clamp(Range, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(ScaleRange, Range))
	{
		ScaleRange = Range;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetScaleBegin(FVector Value)
{
	if (!(ScaleBegin - Value).IsNearlyZero())
	{
		ScaleBegin = Value;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetScaleEnd(FVector Value)
{
	if (!(ScaleEnd - Value).IsNearlyZero())
	{
		ScaleEnd = Value;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetRotateEnabled(bool bEnabled)
{
	if (bRotateEnabled != bEnabled)
	{
		bRotateEnabled = bEnabled;
		ResetRotate();
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetRotateProgress(float Progress)
{
	Progress = FMath::Clamp(Progress, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(RotateProgress, Progress))
	{
		RotateProgress = Progress;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetRotateOrder(EText3DCharacterEffectOrder Order)
{
	if (RotateOrder != Order)
	{
		RotateOrder = Order;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetRotateRange(float Range)
{
	Range = FMath::Clamp(Range, 0.0f, 100.0f);
	if (!FMath::IsNearlyEqual(RotateRange, Range))
	{
		RotateRange = Range;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetRotateBegin(FRotator Value)
{
	if (!(RotateBegin - Value).IsNearlyZero())
	{
		RotateBegin = Value;
		ProcessEffect();
	}
}

void UText3DCharacterTransform::SetRotateEnd(FRotator Value)
{
	if (!(RotateEnd - Value).IsNearlyZero())
	{
		RotateEnd = Value;
		ProcessEffect();
	}
}

UText3DComponent* UText3DCharacterTransform::GetText3DComponent()
{
	UText3DComponent* Component = Cast<UText3DComponent>(GetAttachParent());
	if (Component)
	{
		return Component;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}
	
	return Owner->FindComponentByClass<UText3DComponent>();
}
