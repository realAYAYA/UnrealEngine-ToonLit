// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchMultiSequence.h"
#include "Animation/AnimSequenceBase.h"

bool UPoseSearchMultiSequence::IsLooping() const
{
	float CommonPlayLength = -1.f;
	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (Item.Sequence)
		{
			if (!Item.Sequence->bLoop)
			{
				return false;
			}

			if (CommonPlayLength < 0.f)
			{
				CommonPlayLength = Item.Sequence->GetPlayLength();
			}
			else if (!FMath::IsNearlyEqual(CommonPlayLength, Item.Sequence->GetPlayLength()))
			{
				return false;
			}
		}
	}
	return true;
}

const FString UPoseSearchMultiSequence::GetName() const
{
	FString Name;
	Name.Reserve(256);
	bool bAddComma = false;
	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (bAddComma)
		{
			Name += ", ";
		}
		else
		{
			bAddComma = true;
		}

		Name += "[";
		Name += Item.Role.ToString();
		Name += "] ";
		Name += GetNameSafe(Item.Sequence);
	}
	return Name;
}

bool UPoseSearchMultiSequence::HasRootMotion() const
{
	bool bHasAtLeastOneValidItem = false;
	bool bHasRootMotion = true;

	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (Item.Sequence)
		{
			bHasRootMotion &= Item.Sequence->HasRootMotion();
			bHasAtLeastOneValidItem = true;
		}
	}

	return bHasAtLeastOneValidItem && bHasRootMotion;
}

float UPoseSearchMultiSequence::GetPlayLength() const
{
	float PlayLength = 0.f;
	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (Item.Sequence)
		{
			PlayLength = FMath::Max(PlayLength, Item.Sequence->GetPlayLength());
		}
	}
	return PlayLength;
}

#if WITH_EDITOR
int32 UPoseSearchMultiSequence::GetFrameAtTime(float Time) const
{
	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (Item.Sequence)
		{
			return Item.Sequence->GetFrameAtTime(Time);
		}
	}
	return 0;
}
#endif // WITH_EDITOR

UAnimSequenceBase* UPoseSearchMultiSequence::GetSequence(const UE::PoseSearch::FRole& Role) const
{
	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (Item.Role == Role)
		{
			return Item.Sequence;
		}
	}
	return nullptr;
}

const FTransform& UPoseSearchMultiSequence::GetOrigin(const UE::PoseSearch::FRole& Role) const
{
	for (const FPoseSearchMultiSequenceItem& Item : Items)
	{
		if (Item.Role == Role)
		{
			return Item.Origin;
		}
	}
	return FTransform::Identity;
}