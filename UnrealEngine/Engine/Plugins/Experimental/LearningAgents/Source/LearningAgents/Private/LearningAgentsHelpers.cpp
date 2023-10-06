// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsHelpers.h"

#include "LearningAgentsManager.h"
#include "LearningAgentsManagerComponent.h"
#include "LearningLog.h"

#include "Components/MeshComponent.h"
#include "Components/SplineComponent.h"
#include "Engine/World.h"

//------------------------------------------------------------------

void ULearningAgentsHelper::OnAgentsAdded(const TArray<int32>& AgentIds) {}
void ULearningAgentsHelper::OnAgentsRemoved(const TArray<int32>& AgentIds) {}
void ULearningAgentsHelper::OnAgentsReset(const TArray<int32>& AgentIds) {}

//------------------------------------------------------------------

USplineComponentHelper* USplineComponentHelper::AddSplineComponentHelper(ULearningAgentsManagerComponent* InManagerComponent, const FName Name)
{
	if (!InManagerComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("AddSplineComponentHelper: InManagerComponent is nullptr."));
		return nullptr;
	}

	const FName UniqueName = MakeUniqueObjectName(InManagerComponent, USplineComponentHelper::StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);

	USplineComponentHelper* Helper = NewObject<USplineComponentHelper>(InManagerComponent, UniqueName);
	Helper->ManagerComponent = InManagerComponent;
	InManagerComponent->AddHelper(Helper);
	return Helper;
}

FVector USplineComponentHelper::GetNearestPositionOnSpline(const int32 AgentId, const USplineComponent* SplineComponent, const FVector Position, const ESplineCoordinateSpace::Type CoordinateSpace) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return FVector::ZeroVector;
	}

	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: SplineComponent is nullptr."), *GetName());
		return FVector::ZeroVector;
	}

	const FVector SplinePosition = SplineComponent->FindLocationClosestToWorldLocation(Position, CoordinateSpace);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
			SplinePosition,
			10.0f,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
			Position,
			SplinePosition,
			VisualLogColor.ToFColor(true),
			TEXT("GetNearestPositionOnSpline\nAgent %i\nPosition: [% 6.1f % 6.1f % 6.1f]\nSpline Position: [% 6.1f % 6.1f % 6.1f]"),
			AgentId,
			Position.X, Position.Y, Position.Z,
			SplinePosition.X, SplinePosition.Y, SplinePosition.Z);

		UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
			Position,
			10.0f,
			VisualLogColor.ToFColor(true),
			TEXT(""));
	}
#endif

	return SplinePosition;
}

float USplineComponentHelper::GetDistanceAlongSplineAtPosition(const int32 AgentId, const USplineComponent* SplineComponent, const FVector Position, const ESplineCoordinateSpace::Type CoordinateSpace) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return 0.0f;
	}

	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: SplineComponent is nullptr."), *GetName());
		return 0.0f;
	}

	const float DistanceAlongSpline = SplineComponent->GetDistanceAlongSplineAtLocation(Position, CoordinateSpace);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		const FVector SplinePosition = SplineComponent->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);

		UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
			SplinePosition,
			10.0f,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
			Position,
			SplinePosition,
			VisualLogColor.ToFColor(true),
			TEXT("GetDistanceAlongSplineAtPosition\nAgent %i\nPosition: [% 6.1f % 6.1f % 6.1f]\nDistance Along Spline: [% 6.1f]"),
			AgentId,
			Position.X, Position.Y, Position.Z,
			DistanceAlongSpline);

		UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
			Position,
			10.0f,
			VisualLogColor.ToFColor(true),
			TEXT(""));
	}
#endif

	return DistanceAlongSpline;
}

FVector USplineComponentHelper::GetPositionAtDistanceAlongSpline(const int32 AgentId, const USplineComponent* SplineComponent, const float DistanceAlongSpline, const ESplineCoordinateSpace::Type CoordinateSpace) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return FVector::ZeroVector;
	}

	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: SplineComponent is nullptr."), *GetName());
		return FVector::ZeroVector;
	}

	const FVector Position = SplineComponent->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, CoordinateSpace);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
			Position,
			10.0f,
			VisualLogColor.ToFColor(true),
			TEXT("GetPositionAtDistanceAlongSpline\nAgent %i\nDistance Along Spline: [% 6.1f]\nPosition: [% 6.1f % 6.1f % 6.1f]"),
			AgentId,
			DistanceAlongSpline,
			Position.X, Position.Y, Position.Z);
	}
#endif

	return Position;
}

FVector USplineComponentHelper::GetDirectionAtDistanceAlongSpline(const int32 AgentId, const USplineComponent* SplineComponent, const float DistanceAlongSpline, const ESplineCoordinateSpace::Type CoordinateSpace) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return FVector::ZeroVector;
	}

	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: SplineComponent is nullptr."), *GetName());
		return FVector::ZeroVector;
	}

	const FVector Direction = SplineComponent->GetDirectionAtDistanceAlongSpline(DistanceAlongSpline, CoordinateSpace);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		const FVector Position = SplineComponent->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);

		UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
			Position,
			Position + Direction * 100.0f,
			VisualLogColor.ToFColor(true),
			TEXT("GetDirectionAtDistanceAlongSpline\nAgent %i\nDistance Along Spline: [% 6.1f]\nDirection: [% 6.3f % 6.3f % 6.3f]"),
			AgentId,
			DistanceAlongSpline,
			Direction.X, Direction.Y, Direction.Z);
	}
#endif

	return Direction;
}

float USplineComponentHelper::GetProportionAlongSpline(const int32 AgentId, const USplineComponent* SplineComponent, const float DistanceAlongSpline) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return 0.0f;
	}

	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: SplineComponent is nullptr."), *GetName());
		return 0.0f;
	}

	const float Proportion = FMath::Clamp(DistanceAlongSpline / FMath::Max(SplineComponent->GetSplineLength(), UE_SMALL_NUMBER), 0.0f, 1.0f);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		const FVector Position = SplineComponent->GetLocationAtDistanceAlongSpline(DistanceAlongSpline, ESplineCoordinateSpace::World);

		UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
			Position,
			10.0f,
			VisualLogColor.ToFColor(true),
			TEXT("GetProportionAlongSpline\nAgent %i\nDistance Along Spline: [% 6.1f]\nProportion: [% 6.3f]"),
			AgentId,
			DistanceAlongSpline,
			Proportion);
	}
#endif

	return Proportion;
}

float USplineComponentHelper::GetProportionAlongSplineAsAngle(const int32 AgentId, const USplineComponent* SplineComponent, const float DistanceAlongSpline) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return 0.0f;
	}

	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: SplineComponent is nullptr."), *GetName());
		return 0.0f;
	}

	if (!SplineComponent->IsClosedLoop())
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Getting proportion along spline as angle, but spline is not closed loop. Consider using GetPropotionAlongSpline instead."), *GetName());
	}

	const float TotalDistance = SplineComponent->GetSplineLength();
	const float WrapDistance = FMath::Wrap(DistanceAlongSpline, 0.0f, TotalDistance);
	const float Proportion = WrapDistance / FMath::Max(TotalDistance, UE_SMALL_NUMBER);
	const float Angle = FMath::Wrap(UE_TWO_PI * Proportion, -UE_PI, UE_PI);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		const FVector Position = SplineComponent->GetLocationAtDistanceAlongSpline(WrapDistance, ESplineCoordinateSpace::World);

		UE_LEARNING_AGENTS_VLOG_ANGLE(this, LogLearning, Display,
			Angle,
			0.0f,
			Position,
			50.0f,
			VisualLogColor.ToFColor(true),
			TEXT("GetProportionAlongSplineAsAngle\nAgent %i\nDistance Along Spline: [% 6.1f]\nProportion: [% 6.3f]\nAngle: [% 6.1f]"),
			AgentId,
			WrapDistance,
			Proportion,
			FMath::RadiansToDegrees(Angle));
	}
#endif

	return FMath::RadiansToDegrees(Angle);
}

void USplineComponentHelper::GetPositionsAlongSpline(TArray<FVector>& OutPositions, const int32 AgentId, const USplineComponent* SplineComponent, const int32 PositionNum, const float StartDistanceAlongSpline, const float StopDistanceAlongSpline, const ESplineCoordinateSpace::Type CoordinateSpace) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutPositions.Empty();
		return;
	}

	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: SplineComponent is nullptr."), *GetName());
		OutPositions.Empty();
		return;
	}

	if (PositionNum < 1)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: Expected at least one position along the spline got %i."), *GetName(), PositionNum);
		OutPositions.Empty();
		return;
	}

	OutPositions.SetNumUninitialized(PositionNum);

	const float TotalDistance = SplineComponent->GetSplineLength();
	const bool bIsClosedLoop = SplineComponent->IsClosedLoop();

	for (int32 PositionIdx = 0; PositionIdx < PositionNum; PositionIdx++)
	{
		float PositionDistance = PositionNum == 1 ? 
			(StartDistanceAlongSpline + StopDistanceAlongSpline) / 2.0f : FMath::Lerp(
				StartDistanceAlongSpline,
				StopDistanceAlongSpline,
				((float)PositionIdx) / (PositionNum - 1));

		PositionDistance = bIsClosedLoop ? FMath::Wrap(PositionDistance, 0.0f, TotalDistance) : PositionDistance;

		OutPositions[PositionIdx] = SplineComponent->GetLocationAtDistanceAlongSpline(PositionDistance, CoordinateSpace);
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		for (int32 PositionIdx = 0; PositionIdx < PositionNum; PositionIdx++)
		{
			UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
				OutPositions[PositionIdx],
				2.5f,
				VisualLogColor.ToFColor(true),
				TEXT("GetPositionsAlongSpline\nAgent: %i\nPosition: [% 6.1f % 6.1f % 6.1f]"),
				AgentId,
				OutPositions[PositionIdx].X, OutPositions[PositionIdx].Y, OutPositions[PositionIdx].Z);
		}
	}
#endif
}

float USplineComponentHelper::GetVelocityAlongSpline(const int32 AgentId, const USplineComponent* SplineComponent, const FVector Position, const FVector Velocity, const float FiniteDifferenceDelta, const ESplineCoordinateSpace::Type CoordinateSpace) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return 0.0f;
	}

	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: SplineComponent is nullptr."), *GetName());
		return 0.0f;
	}

	float FiniteDiff = FiniteDifferenceDelta;

	if (FiniteDiff < UE_KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: FiniteDifferenceDelta is too small (%6.4f). Clamping to %6.4f."), *GetName(), FiniteDifferenceDelta, UE_KINDA_SMALL_NUMBER);
		FiniteDiff = UE_KINDA_SMALL_NUMBER;
	}

	const float RawDistance0 = SplineComponent->GetDistanceAlongSplineAtLocation(Position, CoordinateSpace);
	const float RawDistance1 = SplineComponent->GetDistanceAlongSplineAtLocation(Position + FiniteDiff * Velocity.GetSafeNormal(), CoordinateSpace);

	float Distance0 = RawDistance0, Distance1 = RawDistance1;

	if (SplineComponent->IsClosedLoop())
	{
		const float SplineDistance = SplineComponent->GetSplineLength();

		if (FMath::Abs(Distance0 - (Distance1 + SplineDistance)) < FMath::Abs(Distance0 - Distance1))
		{
			Distance1 = Distance1 + SplineDistance;
		}
		else if (FMath::Abs((Distance0 + SplineDistance) - Distance1) < FMath::Abs(Distance0 - Distance1))
		{
			Distance0 = Distance0 + SplineDistance;
		}
	}

	const float SplineVelocity = ((Distance1 - Distance0) / FiniteDiff) * Velocity.Length();

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		const FVector VelocityEnd = Position + Velocity;
		const FVector VelocityDiff = Position + FiniteDiff * Velocity.GetSafeNormal();

		UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
			Position,
			VelocityEnd,
			VisualLogColor.ToFColor(true),
			TEXT("GetVelocityAlongSpline\nAgent %i\nPosition [% 6.1f % 6.1f % 6.1f]\nVelocity: [% 6.1f % 6.1f % 6.1f]\nMagnitude: [% 6.1f]"),
			AgentId,
			Position.X, Position.Y, Position.Z,
			Velocity.X, Velocity.Y, Velocity.Z,
			Velocity.Length());

		UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
			Position,
			VelocityDiff,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		const FVector Position0 = SplineComponent->GetLocationAtDistanceAlongSpline(RawDistance0, CoordinateSpace);
		const FVector Position1 = SplineComponent->GetLocationAtDistanceAlongSpline(RawDistance1, CoordinateSpace);

		UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
			Position0,
			Position1,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
			Position,
			Position0,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
			VelocityDiff,
			Position1,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
			Position0,
			Position0 + (Position1 - Position0).GetSafeNormal() * FMath::Abs(SplineVelocity),
			VisualLogColor.ToFColor(true),
			TEXT("GetVelocityAlongSpline\nAgent %i\nSpline Velocity: [% 6.1f]"),
			AgentId,
			SplineVelocity);
	}
#endif

	return SplineVelocity;
}

//------------------------------------------------------------------

UProjectionHelper* UProjectionHelper::AddProjectionHelper(ULearningAgentsManagerComponent* InManagerComponent, const FName Name)
{
	if (!InManagerComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("AddProjectionHelper: InManagerComponent is nullptr."));
		return nullptr;
	}

	const FName UniqueName = MakeUniqueObjectName(InManagerComponent, UProjectionHelper::StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);

	UProjectionHelper* Helper = NewObject<UProjectionHelper>(InManagerComponent, UniqueName);
	Helper->ManagerComponent = InManagerComponent;
	InManagerComponent->AddHelper(Helper);
	return Helper;
}

FTransform UProjectionHelper::ProjectTransformOntoGroundPlane(const int32 AgentId, const FTransform Transform, const FVector LocalForwardVector) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return FTransform::Identity;
	}

	FVector Position = Transform.GetLocation();
	Position.Z = 0.0f;

	const FVector Direction = (FVector(1.0f, 1.0f, 0.0f) * Transform.TransformVectorNoScale(LocalForwardVector)).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	const FTransform Projected = FTransform(FQuat::FindBetweenNormals(FVector::ForwardVector, Direction), Position);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
			Transform.GetLocation(),
			Transform.GetRotation(),
			VisualLogColor.ToFColor(true),
			TEXT("ProjectTransformOntoGroundPlane\nAgent %i\nTransform\nLocal Forward: [% 6.3f % 6.3f % 6.3f]"),
			LocalForwardVector.X, LocalForwardVector.Y, LocalForwardVector.Z);

		UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
			Transform.GetLocation(),
			Transform.GetLocation() + 25.0f * Transform.GetRotation().RotateVector(LocalForwardVector),
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
			Transform.GetLocation(),
			Projected.GetLocation(),
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
			Projected.GetLocation(),
			Projected.GetRotation(),
			VisualLogColor.ToFColor(true),
			TEXT("ProjectTransformOntoGroundPlane\nAgent %i\nProjected"));
	}
#endif

	return Projected;
}

void UProjectionHelper::ProjectPositionRotationOntoGroundPlane(FVector& OutPosition, FRotator& OutRotation, const int32 AgentId, const FVector InPosition, const FRotator InRotation, const FVector LocalForwardVector) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutPosition = FVector::ZeroVector;
		OutRotation = FRotator::ZeroRotator;
		return;
	}

	OutPosition = InPosition;
	OutPosition.Z = 0.0f;

	const FVector Direction = (FVector(1.0f, 1.0f, 0.0f) * InRotation.RotateVector(LocalForwardVector)).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	OutRotation = FQuat::FindBetweenNormals(FVector::ForwardVector, Direction).Rotator();

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
			InPosition,
			InRotation,
			VisualLogColor.ToFColor(true),
			TEXT("ProjectPositionRotationOntoGroundPlane\nAgent %i\nTransform\nLocal Forward: [% 6.3f % 6.3f % 6.3f]"),
			LocalForwardVector.X, LocalForwardVector.Y, LocalForwardVector.Z);

		UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
			InPosition,
			InPosition + 25.0f * InRotation.RotateVector(LocalForwardVector),
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_SEGMENT(this, LogLearning, Display,
			InPosition,
			OutPosition,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
			OutPosition,
			OutRotation,
			VisualLogColor.ToFColor(true),
			TEXT("ProjectPositionRotationOntoGroundPlane\nAgent %i\nProjected"));
	}
#endif
}

//------------------------------------------------------------------

UMeshComponentHelper* UMeshComponentHelper::AddMeshComponentHelper(ULearningAgentsManagerComponent* InManagerComponent, const FName Name)
{
	if (!InManagerComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("AddMeshComponentHelper: InManagerComponent is nullptr."));
		return nullptr;
	}

	const FName UniqueName = MakeUniqueObjectName(InManagerComponent, UMeshComponentHelper::StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);

	UMeshComponentHelper* Helper = NewObject<UMeshComponentHelper>(InManagerComponent, UniqueName);
	Helper->ManagerComponent = InManagerComponent;
	InManagerComponent->AddHelper(Helper);
	return Helper;
}

void UMeshComponentHelper::GetMeshBonePositions(TArray<FVector>& OutBonePositions, const int32 AgentId, const UMeshComponent* MeshComponent, const TArray<FName>& BoneNames) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutBonePositions.Empty();
		return;
	}

	if (!MeshComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Mesh Component is nullptr."), *GetName());
		OutBonePositions.Empty();
		return;
	}

	if (BoneNames.IsEmpty())
	{
		UE_LOG(LogLearning, Warning, TEXT("%s: No bone names provided!"), *GetName());
		OutBonePositions.Empty();
		return;
	}

	const int32 BoneNum = BoneNames.Num();

	OutBonePositions.SetNumUninitialized(BoneNum);

	for (int32 BoneIdx = 0; BoneIdx < BoneNum; BoneIdx++)
	{
		if (MeshComponent->DoesSocketExist(BoneNames[BoneIdx]))
		{
			OutBonePositions[BoneIdx] = MeshComponent->GetSocketLocation(BoneNames[BoneIdx]);
		}
		else
		{
			OutBonePositions[BoneIdx] = FVector::ZeroVector;
			UE_LOG(LogLearning, Warning, TEXT("%s: Bone \"%s\" does not exist!"), *GetName(), *BoneNames[BoneIdx].ToString());
		}
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		for (int32 BoneIdx = 0; BoneIdx < BoneNum; BoneIdx++)
		{
			UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
				OutBonePositions[BoneIdx],
				2.5f,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nBone: \"%s\"\nPosition: [% 6.1f % 6.1f % 6.1f]"),
				AgentId,
				*BoneNames[BoneIdx].ToString(),
				OutBonePositions[BoneIdx].X, OutBonePositions[BoneIdx].Y, OutBonePositions[BoneIdx].Z);
		}
	}
#endif
}

//------------------------------------------------------------------

URayCastHelper* URayCastHelper::AddRayCastHelper(ULearningAgentsManagerComponent* InManagerComponent, const FName Name)
{
	if (!InManagerComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("AddRayCastHelper: InManagerComponent is nullptr."));
		return nullptr;
	}

	const FName UniqueName = MakeUniqueObjectName(InManagerComponent, URayCastHelper::StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);

	URayCastHelper* Helper = NewObject<URayCastHelper>(InManagerComponent, UniqueName);
	Helper->ManagerComponent = InManagerComponent;
	InManagerComponent->AddHelper(Helper);
	return Helper;
}

void URayCastHelper::RayCastGridHeights(
	TArray<float>& OutHeights, 
	const int32 AgentId,
	const FVector Position, 
	const FRotator Rotation, 
	const int32 RowNum, 
	const int32 ColNum, 
	const float RowWidth, 
	const float ColWidth, 
	const float MaxHeight, 
	const float MinHeight,
	const ECollisionChannel CollisionChannel) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutHeights.Empty();
		return;
	}

	if (RowNum < 1 || ColNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Need at least 1 row and 1 col in grid."), *GetName());
		OutHeights.Empty();
		return;
	}

	OutHeights.SetNumUninitialized(ColNum * RowNum);

	TArray<FHitResult, TInlineAllocator<25>> TraceHits;
	TraceHits.SetNum(ColNum * RowNum);

	const FRotator YawRotator(0.0f, Rotation.Yaw, 0.0f);

	for (int32 RowIdx = 0; RowIdx < RowNum; RowIdx++)
	{
		for (int32 ColIdx = 0; ColIdx < ColNum; ColIdx++)
		{
			const FVector Start = Position + YawRotator.RotateVector(FVector(
				RowWidth * (((float)RowIdx) / (RowNum - 1) - 0.5f),
				ColWidth * (((float)ColIdx) / (ColNum - 1) - 0.5f),
				MaxHeight));

			const FVector End = Position + YawRotator.RotateVector(FVector(
				RowWidth * (((float)RowIdx) / (RowNum - 1) - 0.5f),
				ColWidth * (((float)ColIdx) / (ColNum - 1) - 0.5f),
				MinHeight));

			FCollisionObjectQueryParams ObjectQueryParams;
			ObjectQueryParams.AddObjectTypesToQuery(CollisionChannel);

			const bool bHit = GetWorld()->LineTraceSingleByObjectType(TraceHits[RowIdx * ColNum + ColIdx], Start, End, ObjectQueryParams);

			if (bHit)
			{
				OutHeights[RowIdx * ColNum + ColIdx] = TraceHits[RowIdx * ColNum + ColIdx].ImpactPoint.Z;
			}
			else
			{
				OutHeights[RowIdx * ColNum + ColIdx] = MinHeight;
			}
		}
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
			Position,
			Rotation,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		for (int32 RowIdx = 0; RowIdx < RowNum; RowIdx++)
		{
			for (int32 ColIdx = 0; ColIdx < ColNum; ColIdx++)
			{
				const FVector Start = Position + YawRotator.RotateVector(FVector(
					RowWidth * (((float)RowIdx) / (RowNum - 1) - 0.5f),
					ColWidth * (((float)ColIdx) / (ColNum - 1) - 0.5f),
					MaxHeight));

				const FVector End = Position + YawRotator.RotateVector(FVector(
					RowWidth * (((float)RowIdx) / (RowNum - 1) - 0.5f),
					ColWidth * (((float)ColIdx) / (ColNum - 1) - 0.5f),
					MinHeight));

				const FVector ImpactPoint = 
					TraceHits[RowIdx * ColNum + ColIdx].bBlockingHit ?
					TraceHits[RowIdx * ColNum + ColIdx].ImpactPoint : End;

				UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
					Start,
					End,
					VisualLogColor.ToFColor(true),
					TEXT(""));

				UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
					ImpactPoint,
					2.5f,
					VisualLogColor.ToFColor(true),
					TEXT("Agent %i\nRow: %i\nCol: %i\nImpact Point: [% 6.1f % 6.1f % 6.1f]\nHeight: [% 6.1f]"),
					AgentId,
					RowIdx, ColIdx,
					ImpactPoint.X, ImpactPoint.Y, ImpactPoint.Z,
					OutHeights[RowIdx * ColNum + ColIdx]);
			}
		}
	}
#endif
}

void URayCastHelper::RayCastRadial(
	TArray<float>& OutDistances,
	const int32 AgentId,
	const FVector Position,
	const FRotator Rotation,
	const int32 RayNum,
	const float MinAngle,
	const float MaxAngle,
	const float MaxRayDist,
	const FVector LocalForward,
	const ECollisionChannel CollisionChannel) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		OutDistances.Empty();
		return;
	}

	if (RayNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Need at least 1 ray."), *GetName());
		OutDistances.Empty();
		return;
	}

	OutDistances.SetNumUninitialized(RayNum);

	TArray<FHitResult, TInlineAllocator<25>> TraceHits;
	TraceHits.SetNum(RayNum);


	for (int32 RayIdx = 0; RayIdx < RayNum; RayIdx++)
	{
		const float Alpha = RayNum == 1 ? 0.0f : ((float)RayIdx) / (RayNum - 1);
		const FRotator YawRotator(0.0f, Rotation.Yaw + Alpha * (MaxAngle - MinAngle) + MinAngle, 0.0f);

		const FVector Start = Position;
		const FVector End = Position + YawRotator.RotateVector(MaxRayDist * LocalForward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));

		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(CollisionChannel);

		const bool bHit = GetWorld()->LineTraceSingleByObjectType(TraceHits[RayIdx], Start, End, ObjectQueryParams);

		if (bHit)
		{
			OutDistances[RayIdx] = TraceHits[RayIdx].Distance;
		}
		else
		{
			OutDistances[RayIdx] = MaxRayDist;
		}
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	{
		UE_LEARNING_AGENTS_VLOG_TRANSFORM(this, LogLearning, Display,
			Position,
			Rotation,
			VisualLogColor.ToFColor(true),
			TEXT(""));

		for (int32 RayIdx = 0; RayIdx < RayNum; RayIdx++)
		{
			const float Alpha = RayNum == 1 ? 0.0f : ((float)RayIdx) / (RayNum - 1);
			const FRotator YawRotator(0.0f, Rotation.Yaw + Alpha * (MaxAngle - MinAngle) + MinAngle, 0.0f);

			const FVector Start = Position;
			const FVector End = Position + YawRotator.RotateVector(MaxRayDist * LocalForward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));

			const FVector ImpactPoint =
				TraceHits[RayIdx].bBlockingHit ?
				TraceHits[RayIdx].ImpactPoint : End;

			UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
				Start,
				End,
				VisualLogColor.ToFColor(true),
				TEXT(""));

			UE_LEARNING_AGENTS_VLOG_LOCATION(this, LogLearning, Display,
				ImpactPoint,
				2.5f,
				VisualLogColor.ToFColor(true),
				TEXT("Agent %i\nRay: %i\nImpact Point: [% 6.1f % 6.1f % 6.1f]\nDistance: [% 6.1f]"),
				AgentId,
				RayIdx,
				ImpactPoint.X, ImpactPoint.Y, ImpactPoint.Z,
				OutDistances[RayIdx]);
		}
	}
#endif
}



UCollisionMonitorHelper* UCollisionMonitorHelper::AddCollisionMonitorHelper(
	ULearningAgentsManagerComponent* InManagerComponent,
	const FName Name)
{
	if (!InManagerComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("AddCollisionMonitorHelper: InManagerComponent is nullptr."));
		return nullptr;
	}

	const FName UniqueName = MakeUniqueObjectName(InManagerComponent, UCollisionMonitorHelper::StaticClass(), Name, EUniqueObjectNameOptions::GloballyUnique);

	UCollisionMonitorHelper* Helper = NewObject<UCollisionMonitorHelper>(InManagerComponent, UniqueName);
	Helper->Init(InManagerComponent->GetAgentManager()->GetMaxAgentNum());
	Helper->ManagerComponent = InManagerComponent;

	InManagerComponent->AddHelper(Helper);
	return Helper;
}

void UCollisionMonitorHelper::Init(const int32 MaxAgentNum)
{
	Components.SetNumUninitialized({ MaxAgentNum });
	OtherComponentTags.SetNumUninitialized({ MaxAgentNum });
	CollisionsOccured.SetNumUninitialized({ MaxAgentNum });

	UE::Learning::Array::Set<1, UPrimitiveComponent*>(Components, nullptr);
	UE::Learning::Array::Set<1, FName>(OtherComponentTags, NAME_None);
	UE::Learning::Array::Set<1, bool>(CollisionsOccured, false);
}

void UCollisionMonitorHelper::OnAgentsRemoved(const TArray<int32>& AgentIds)
{
	ULearningAgentsHelper::OnAgentsRemoved(AgentIds);

	for (const int32 AgentId : AgentIds)
	{
		if (Components[AgentId])
		{
			Components[AgentId]->OnComponentHit.RemoveDynamic(this, &UCollisionMonitorHelper::HandleOnHit);
		}
	}

	UE::Learning::Array::Set<1, UPrimitiveComponent*>(Components, nullptr);
	UE::Learning::Array::Set<1, FName>(OtherComponentTags, NAME_None, AgentIds);
	UE::Learning::Array::Set<1, bool>(CollisionsOccured, false, AgentIds);
}

void UCollisionMonitorHelper::OnAgentsReset(const TArray<int32>& AgentIds)
{
	ULearningAgentsHelper::OnAgentsReset(AgentIds);

	UE::Learning::Array::Set(CollisionsOccured, false, AgentIds);
}

void UCollisionMonitorHelper::SetComponent(const int32 AgentId, UPrimitiveComponent* Component, const FName OtherComponentTag)
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	if (Components[AgentId] != Component)
	{
		if (Components[AgentId])
		{
			Components[AgentId]->OnComponentHit.RemoveDynamic(this, &UCollisionMonitorHelper::HandleOnHit);
		}

		Components[AgentId] = Component;
		OtherComponentTags[AgentId] = OtherComponentTag;
		Component->OnComponentHit.AddUniqueDynamic(this, &UCollisionMonitorHelper::HandleOnHit);
	}
}

bool UCollisionMonitorHelper::GetCollisionOccurred(const int32 AgentId) const
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return false;
	}

	return CollisionsOccured[AgentId];
}

bool UCollisionMonitorHelper::GetAndResetCollisionOccurred(const int32 AgentId)
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return false;
	}

	bool bOccurred = CollisionsOccured[AgentId];
	CollisionsOccured[AgentId] = false;
	return bOccurred;
}

void UCollisionMonitorHelper::ResetCollisionOccurred(const int32 AgentId)
{
	if (!ManagerComponent->HasAgent(AgentId))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: AgentId %d not found in the agents set."), *GetName(), AgentId);
		return;
	}

	CollisionsOccured[AgentId] = false;
}

void UCollisionMonitorHelper::HandleOnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	for (int32 AgentId = 0; AgentId < Components.Num(); AgentId++)
	{
		if (Components[AgentId] == HitComponent)
		{
			CollisionsOccured[AgentId] = true;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
			{
				UE_LEARNING_AGENTS_VLOG_ARROW(this, LogLearning, Display,
					Hit.ImpactPoint,
					Hit.ImpactPoint + Hit.ImpactNormal * 100.0f,
					VisualLogColor.ToFColor(true),
					TEXT("Agent %i\nCollision \"%s\" and \"%s\"\nTag: \"%s\""),
					AgentId,
					*HitComponent->GetName(),
					*OtherComp->GetName(),
					*OtherComponentTags[AgentId].ToString());
			}
#endif
			return;
		}
	}

	UE_LOG(LogLearning, Error, TEXT("%s: HitComponent not found in the set of agent's Components."), *GetName());
}