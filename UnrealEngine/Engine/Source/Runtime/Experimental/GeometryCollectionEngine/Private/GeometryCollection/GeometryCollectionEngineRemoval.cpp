// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionEngineRemoval.h"
#include "GeometryCollection/GeometryCollection.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FGeometryCollectionRemoveOnBreakDynamicFacade::FGeometryCollectionRemoveOnBreakDynamicFacade(FManagedArrayCollection& InCollection)
	: BreakTimerAttribute(InCollection, "BreakTimer", FGeometryCollection::TransformGroup)
	, PostBreakDurationAttribute(InCollection, "PostBreakDuration", FGeometryCollection::TransformGroup)
	, BreakRemovalDurationAttribute(InCollection, "BreakRemovalDuration", FGeometryCollection::TransformGroup)
{
}

bool FGeometryCollectionRemoveOnBreakDynamicFacade::IsValid() const
{
	return BreakTimerAttribute.IsValid()
		&& PostBreakDurationAttribute.IsValid()
		&& BreakRemovalDurationAttribute.IsValid()
	;
}

void FGeometryCollectionRemoveOnBreakDynamicFacade::AddAttributes(const TManagedArray<FVector4f>& RemoveOnBreakAttribute, const TManagedArray<TSet<int32>>& ChildrenAttribute)
{
	BreakTimerAttribute.AddAndFill(DisabledBreakTimer);

	if (!PostBreakDurationAttribute.IsValid())
	{
		TManagedArray<float>& PostBreakDuration = PostBreakDurationAttribute.Add();
		for (int32 Idx = 0; Idx < PostBreakDuration.Num(); ++Idx)
		{
			const FRemoveOnBreakData RemoveOnBreakData{ RemoveOnBreakAttribute[Idx] };
			const FVector2f PostBreakTimer = RemoveOnBreakData.GetBreakTimer();
			const float MinTime = FMath::Max(0.0f, PostBreakTimer.X);
			const float MaxTime = FMath::Max(MinTime, PostBreakTimer.Y);
			PostBreakDuration[Idx] = RemoveOnBreakData.IsEnabled()? FMath::RandRange(MinTime, MaxTime): DisabledPostBreakDuration;
		}
	}

	if (!BreakRemovalDurationAttribute.IsValid())
	{
		TManagedArray<float>& BreakRemovalDuration = BreakRemovalDurationAttribute.Add();
		for (int32 Idx = 0; Idx < BreakRemovalDuration.Num(); ++Idx)
		{
			const FRemoveOnBreakData RemoveOnBreakData{ RemoveOnBreakAttribute[Idx] };
			const FVector2f RemovalTimer = RemoveOnBreakData.GetRemovalTimer();
			const float MinTime = FMath::Max(0.0f, RemovalTimer.X);
			const float MaxTime = FMath::Max(MinTime, RemovalTimer.Y);
			const bool bIsCluster = (ChildrenAttribute[Idx].Num() > 0);
			const bool bUseClusterCrumbling = (bIsCluster && RemoveOnBreakData.GetClusterCrumbling());
			BreakRemovalDuration[Idx] = bUseClusterCrumbling? CrumblingRemovalTimer: FMath::RandRange(MinTime, MaxTime);
		}
	}
}

bool FGeometryCollectionRemoveOnBreakDynamicFacade::IsRemovalActive(int32 TransformIndex) const
{
	return (PostBreakDurationAttribute.Get()[TransformIndex] > DisabledPostBreakDuration);
}

bool FGeometryCollectionRemoveOnBreakDynamicFacade::UseClusterCrumbling(int32 TransformIndex) const
{
	return (BreakRemovalDurationAttribute.Get()[TransformIndex] <= CrumblingRemovalTimer);
}

float FGeometryCollectionRemoveOnBreakDynamicFacade::UpdateBreakTimerAndComputeDecay(int32 TransformIndex, float DeltaTime)
{
	float& BreakTimerRef = BreakTimerAttribute.Modify()[TransformIndex];
	const float PostBreakDuration = PostBreakDurationAttribute.Get()[TransformIndex];
	const float BreakRemovalDuration = BreakRemovalDurationAttribute.Get()[TransformIndex];

	float UpdatedBreakDecay = 0;
	if (PostBreakDuration >= 0)
	{
		BreakTimerRef += DeltaTime;
	
		const bool bPostBreakTimeExpired = (BreakTimerRef >= PostBreakDuration) && (PostBreakDuration >= 0);
		const bool bZeroRemovalDuration = (BreakRemovalDuration < UE_SMALL_NUMBER);
		if (bPostBreakTimeExpired)
		{
			UpdatedBreakDecay = bZeroRemovalDuration? 1.f: FMath::Clamp<float>((BreakTimerRef - PostBreakDuration) / BreakRemovalDuration, 0.f, 1.f);
		}
	}
	return UpdatedBreakDecay;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FGeometryCollectionRemoveOnSleepDynamicFacade::FGeometryCollectionRemoveOnSleepDynamicFacade(FManagedArrayCollection& InCollection)
	: SleepTimerAttribute(InCollection, "SleepTimer", FGeometryCollection::TransformGroup)
	, MaxSleepTimeAttribute(InCollection, "MaxSleepTime", FGeometryCollection::TransformGroup)
	, SleepRemovalDurationAttribute(InCollection, "SleepRemovalDuration", FGeometryCollection::TransformGroup)
	, LastPositionAttribute(InCollection, "LastPosition", FGeometryCollection::TransformGroup)
{
}

bool FGeometryCollectionRemoveOnSleepDynamicFacade::IsValid() const
{
	return SleepTimerAttribute.IsValid()
		&& MaxSleepTimeAttribute.IsValid()
		&& SleepRemovalDurationAttribute.IsValid()
		&& LastPositionAttribute.IsValid()
	;
}

void FGeometryCollectionRemoveOnSleepDynamicFacade::AddAttributes(const FVector2D& MaximumSleepTime, const FVector2D& RemovalDuration)
{
	SleepTimerAttribute.AddAndFill(0.0f);

	LastPositionAttribute.AddAndFill(FVector::ZeroVector);
	
	if (!MaxSleepTimeAttribute.IsValid())
	{
		const float MinTime = FMath::Max(0.0f, MaximumSleepTime.X);
		const float MaxTime = FMath::Max(MinTime, MaximumSleepTime.Y);
		TManagedArray<float>& MaxSleepTime = MaxSleepTimeAttribute.Add();
		for (int32 Idx = 0; Idx < MaxSleepTime.Num(); ++Idx)
		{
			MaxSleepTime[Idx] = FMath::RandRange(MinTime, MaxTime);
		}
	}

	if (!SleepRemovalDurationAttribute.IsValid())
	{
		float MinTime = FMath::Max(0.0f, RemovalDuration.X);
		float MaxTime = FMath::Max(MinTime, RemovalDuration.Y);
		TManagedArray<float>& SleepRemovalDuration = SleepRemovalDurationAttribute.Add();
		for (int32 Idx = 0; Idx < SleepRemovalDuration.Num(); ++Idx)
		{
			SleepRemovalDuration[Idx] = FMath::RandRange(MinTime, MaxTime);
		}
	}
}

bool FGeometryCollectionRemoveOnSleepDynamicFacade::IsRemovalActive(int32 TransformIndex) const
{
	return (MaxSleepTimeAttribute.Get()[TransformIndex] >= 0);
}

bool FGeometryCollectionRemoveOnSleepDynamicFacade::ComputeSlowMovingState(int32 TransformIndex, const FVector& Position, float DeltaTime, FVector::FReal VelocityThreshold)
{
	bool IsSlowMoving = false;
	FVector& LastPositionRef = LastPositionAttribute.Modify()[TransformIndex];
	if (DeltaTime > 0)
	{
		const FVector::FReal InstantVelocity = (Position-LastPositionRef).Size() / (FVector::FReal)DeltaTime; 
		IsSlowMoving = (InstantVelocity < VelocityThreshold);
	}
	LastPositionRef = Position;
	return IsSlowMoving;
}

void FGeometryCollectionRemoveOnSleepDynamicFacade::UpdateSleepTimer(int32 TransformIndex, float DeltaTime)
{
	check(SleepTimerAttribute.IsValid());
	SleepTimerAttribute.Modify()[TransformIndex] += DeltaTime;
}

float FGeometryCollectionRemoveOnSleepDynamicFacade::ComputeDecay(int32 TransformIndex) const
{
	const float SleepRemovalDuration = SleepRemovalDurationAttribute.Get()[TransformIndex];
	const float MaxSleepTime = MaxSleepTimeAttribute.Get()[TransformIndex];
	const float SleepTimer = SleepTimerAttribute.Get()[TransformIndex];
	const bool bZeroRemovalDuration = (SleepRemovalDuration < UE_SMALL_NUMBER);
	return bZeroRemovalDuration? 1.f: FMath::Clamp<float>((SleepTimer -MaxSleepTime) / SleepRemovalDuration, 0.f, 1.f);
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FGeometryCollectionDecayDynamicFacade::FGeometryCollectionDecayDynamicFacade(FManagedArrayCollection& InCollection)
	: DecayAttribute(InCollection, "Decay", FGeometryCollection::TransformGroup)
	, UniformScaleAttribute(InCollection, "UniformScale", FGeometryCollection::TransformGroup)
{
}

bool FGeometryCollectionDecayDynamicFacade::IsValid() const
{
	return DecayAttribute.IsValid()
		&& UniformScaleAttribute.IsValid()
	;
}

void FGeometryCollectionDecayDynamicFacade::AddAttributes()
{
	DecayAttribute.AddAndFill(0.0f);
	UniformScaleAttribute.AddAndFill(FTransform::Identity);
}