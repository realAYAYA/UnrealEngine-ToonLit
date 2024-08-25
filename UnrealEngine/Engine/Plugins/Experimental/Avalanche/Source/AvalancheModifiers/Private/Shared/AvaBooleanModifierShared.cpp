// Copyright Epic Games, Inc. All Rights Reserved.

#include "Shared/AvaBooleanModifierShared.h"

#include "Async/ParallelFor.h"
#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Modifiers/AvaBooleanModifier.h"

TSet<TWeakObjectPtr<UAvaBooleanModifier>> UAvaBooleanModifierShared::GetIntersectingModifiers(const UAvaBooleanModifier* InTargetModifier, FAvaBooleanModifierSharedChannelInfo* OutDesc)
{
	TSet<TWeakObjectPtr<UAvaBooleanModifier>> IntersectingModifiers;
	if (!IsValid(InTargetModifier)
		|| !InTargetModifier->GetMeshComponent()
		|| !InTargetModifier->PreModifierCachedMesh.IsSet())
	{
		return IntersectingModifiers;
	}

	const AActor* TargetActor = InTargetModifier->GetModifiedActor();
	if (!IsValid(TargetActor))
	{
		return IntersectingModifiers;
	}

	const int32 TargetChannel = InTargetModifier->GetChannel();
	const FAvaBooleanModifierSharedChannel* Channel = Channels.Find(TargetChannel);
	if (!Channel)
	{
		return IntersectingModifiers;
	}

	const bool bTargetBooleanMode = InTargetModifier->GetMode() != EAvaBooleanMode::None;
	const FTransform TargetTransform = InTargetModifier->GetMeshComponent()->GetComponentTransform();

	if (OutDesc)
	{
		OutDesc->ChannelModifierCount = Channel->ModifiersWeak.Num();
		OutDesc->ChannelToolCount = 0;
		OutDesc->ChannelTargetCount = 0;
		OutDesc->ChannelIntersectCount = 0;
	}
			
	for (const TWeakObjectPtr<UAvaBooleanModifier>& OtherModifierWeak : Channel->ModifiersWeak)
	{
		UAvaBooleanModifier* OtherModifier = OtherModifierWeak.Get();
		if (!IsValid(OtherModifier)
			|| !OtherModifier->GetMeshComponent()
			|| !OtherModifier->PreModifierCachedMesh.IsSet())
		{
			continue;
		}

		if (OutDesc)
		{
			if (OtherModifier->GetMode() == EAvaBooleanMode::None)
			{
				OutDesc->ChannelTargetCount++;
			}
			else
			{
				OutDesc->ChannelToolCount++;
			}
		}
		
		const AActor* OtherActor = OtherModifier->GetModifiedActor();
		if (!OtherActor || TargetActor == OtherActor)
		{
			continue;
		}

		const bool bOtherBooleanMode = OtherModifier->GetMode() != EAvaBooleanMode::None;
		if (bTargetBooleanMode == bOtherBooleanMode)
		{
			continue;
		}

		const FTransform OtherTransform = OtherModifier->GetMeshComponent()->GetComponentTransform();
		
		// Other is mask
		bool bIsIntersecting = false;
		if (bOtherBooleanMode)
		{
			const FDynamicMesh3& TargetMesh = InTargetModifier->PreModifierCachedMesh.GetValue();

			OtherModifier->GetMeshComponent()->ProcessMesh([this, &bIsIntersecting, &TargetMesh, &OtherTransform, &TargetTransform](const FDynamicMesh3& InOtherMesh)
			{
				bIsIntersecting = TestIntersection(InOtherMesh, OtherTransform, TargetMesh, TargetTransform);
			});
		}
		// Target is mask
		else
		{
			const FDynamicMesh3& OtherMesh = OtherModifier->PreModifierCachedMesh.GetValue();

			InTargetModifier->GetMeshComponent()->ProcessMesh([this, &bIsIntersecting, &OtherMesh, &OtherTransform, &TargetTransform](const FDynamicMesh3& InTargetMesh)
			{
				bIsIntersecting = TestIntersection(InTargetMesh, TargetTransform, OtherMesh, OtherTransform);
			});
		}
		
		if (bIsIntersecting)
		{
			IntersectingModifiers.Add(OtherModifier);
			
			if (OutDesc)
			{
				OutDesc->ChannelIntersectCount++;
			}
		}
	}

	if (OutDesc)
	{
		OutDesc->ChannelCount = GetChannelCount();
	}
	
	return IntersectingModifiers;
}

bool UAvaBooleanModifierShared::TestIntersection(const UE::Geometry::FDynamicMesh3& InToolMesh, const FTransform& InToolTransform, const UE::Geometry::FDynamicMesh3& InTargetMesh, const FTransform& InTargetTransform) const
{
	using namespace UE::Geometry;

	if (InToolMesh.TriangleCount() == 0)
	{
		return false;
	}
	
	FDynamicMeshAABBTree3 Spatials[2];
	ParallelFor(2, [&Spatials, &InToolMesh, &InTargetMesh](int32 k)
	{
		Spatials[k].SetMesh(k == 0 ? &InToolMesh : &InTargetMesh, true);
	}, EParallelForFlags::Unbalanced);

	const bool bIsIdentity1 = InToolTransform.Equals(FTransform::Identity, 0);
	FTransformSRT3d Transform1(InToolTransform);

	const bool bIsIdentity2 = InTargetTransform.Equals(FTransform::Identity, 0);
	FTransformSRT3d Transform2(InTargetTransform);
	
	bool bIsIntersecting = false;
	if (bIsIdentity1 && bIsIdentity2)
	{
		bIsIntersecting = Spatials[0].TestIntersection(Spatials[1]);
	} 
	else if (bIsIdentity1 || bIsIdentity2)
	{
		const FIndex2i Indices = (bIsIdentity1) ? FIndex2i(0,1) : FIndex2i(1,0);
		FTransformSRT3d UseTransform = (bIsIdentity2) ? Transform1 : Transform2;
		bIsIntersecting = Spatials[Indices.A].TestIntersection(Spatials[Indices.B],
			[&UseTransform](const FVector3d& Pos) { return UseTransform.TransformPosition(Pos); });
	}
	else
	{
		bIsIntersecting = Spatials[0].TestIntersection(Spatials[1],
			[&Transform1, &Transform2](const FVector3d& Pos) { return Transform1.InverseTransformPosition(Transform2.TransformPosition(Pos)); });
	}

	// We are not intersecting but target is perhaps contained inside tool
	if (!bIsIntersecting)
	{
		const FBox ToolBounds = static_cast<FBox>(InToolMesh.GetBounds(true));
		const FVector ActorOffset = InToolTransform.GetLocation() - InTargetTransform.GetLocation();
		
		if (ToolBounds.IsInside(ActorOffset))
		{
			bIsIntersecting = true;
		}
	}

	return bIsIntersecting;
}

void UAvaBooleanModifierShared::TrackModifierChannel(UAvaBooleanModifier* InModifier)
{
	if (!InModifier)
	{
		return;
	}
	
	FAvaBooleanModifierSharedChannel& Channel = Channels.FindOrAdd(InModifier->GetChannel());
	Channel.ModifiersWeak.Add(InModifier);
}

void UAvaBooleanModifierShared::UntrackModifierChannel(UAvaBooleanModifier* InModifier)
{
	if (!InModifier)
	{
		return;
	}
	
	for (TMap<uint8, FAvaBooleanModifierSharedChannel>::TIterator It(Channels); It; ++It)
	{
		It->Value.ModifiersWeak.Remove(InModifier);

		// Remove empty channel
		if (It->Value.ModifiersWeak.IsEmpty())
		{
			It.RemoveCurrent();
		}
	}
}

void UAvaBooleanModifierShared::UpdateModifierChannel(UAvaBooleanModifier* InModifier)
{
	if (!InModifier)
	{
		return;
	}
	
	const uint8 Channel = InModifier->GetChannel();
	for (TMap<uint8, FAvaBooleanModifierSharedChannel>::TIterator It(Channels); It; ++It)
	{
		if (Channel != It->Key)
		{
			It->Value.ModifiersWeak.Remove(InModifier);
			
			// Remove empty channel
			if (It->Value.ModifiersWeak.IsEmpty())
			{
				It.RemoveCurrent();
			}
		}
	}

	TrackModifierChannel(InModifier);
}

uint8 UAvaBooleanModifierShared::GetChannelCount() const
{
	return Channels.Num();
}

int32 UAvaBooleanModifierShared::GetChannelModifierCount(uint8 InChannel) const
{
	if (const FAvaBooleanModifierSharedChannel* Channel = Channels.Find(InChannel))
	{
		return Channel->ModifiersWeak.Num();
	}
	return 0;
}

int32 UAvaBooleanModifierShared::GetChannelModifierModeCount(uint8 InChannel, EAvaBooleanMode InMode) const
{
	int32 Count = 0;
	if (const FAvaBooleanModifierSharedChannel* Channel = Channels.Find(InChannel))
	{
		for (const TWeakObjectPtr<UAvaBooleanModifier>& ModifierWeak : Channel->ModifiersWeak)
		{
			const UAvaBooleanModifier* Modifier = ModifierWeak.Get();
			if (Modifier && Modifier->GetMode() == InMode)
			{
				Count++;
			}
		}
	}
	return Count;
}
