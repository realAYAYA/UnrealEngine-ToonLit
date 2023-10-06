// Copyright Epic Games, Inc. All Rights Reserved.

#include "Avoidance/MassAvoidanceProcessors.h"
#include "Avoidance/MassAvoidanceFragments.h"
#include "DrawDebugHelpers.h"
#include "MassEntityView.h"
#include "MassExecutionContext.h"
#include "VisualLogger/VisualLogger.h"
#include "Math/Vector2D.h"
#include "Logging/LogMacros.h"
#include "MassSimulationLOD.h"
#include "MassCommonFragments.h"
#include "MassMovementFragments.h"
#include "MassNavigationSubsystem.h"
#include "MassNavigationFragments.h"
#include "MassNavigationUtils.h"
#include "Engine/World.h"

#define UNSAFE_FOR_MT 1

DEFINE_LOG_CATEGORY(LogAvoidance);
DEFINE_LOG_CATEGORY(LogAvoidanceVelocities);
DEFINE_LOG_CATEGORY(LogAvoidanceAgents);
DEFINE_LOG_CATEGORY(LogAvoidanceObstacles);

namespace UE::MassAvoidance
{
	namespace Tweakables
	{
		bool bEnableEnvironmentAvoidance = true;
		bool bEnableSettingsforExtendingColliders = true;
		bool bUseAdjacentCorridors = true;
		bool bUseDrawDebugHelpers = false;
	} // Tweakables

	FAutoConsoleVariableRef Vars[] = 
	{
		FAutoConsoleVariableRef(TEXT("ai.mass.avoidance.EnableEnvironmentAvoidance"), Tweakables::bEnableEnvironmentAvoidance, TEXT("Set to false to disable avoidance forces for environment (for debug purposes)."), ECVF_Cheat),
		FAutoConsoleVariableRef(TEXT("ai.mass.avoidance.EnableSettingsforExtendingColliders"), Tweakables::bEnableSettingsforExtendingColliders, TEXT("Set to false to disable using different settings for extending obstacles (for debug purposes)."), ECVF_Cheat),
		FAutoConsoleVariableRef(TEXT("ai.mass.avoidance.UseAdjacentCorridors"), Tweakables::bUseAdjacentCorridors, TEXT("Set to false to disable usage of adjacent lane width."), ECVF_Cheat),
		FAutoConsoleVariableRef(TEXT("ai.mass.avoidance.UseDrawDebugHelpers"), Tweakables::bUseDrawDebugHelpers, TEXT("Use debug draw helpers in addition to visual logs."), ECVF_Cheat)
	};

	constexpr int32 MaxExpectedAgentsPerCell = 6;
	constexpr int32 MinTouchingCellCount = 4;
	constexpr int32 MaxObstacleResults = MaxExpectedAgentsPerCell * MinTouchingCellCount;

	static void FindCloseObstacles(const FVector& Center, const FVector::FReal SearchRadius, const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid,
									TArray<FMassNavigationObstacleItem, TFixedAllocator<MaxObstacleResults>>& OutCloseEntities, const int32 MaxResults)
	{
		OutCloseEntities.Reset();
		const FVector Extent(SearchRadius, SearchRadius, 0.);
		const FBox QueryBox = FBox(Center - Extent, Center + Extent);

		struct FSortingCell
		{
			int32 X;
			int32 Y;
			int32 Level;
			FVector::FReal SqDist;
		};
		TArray<FSortingCell, TInlineAllocator<64>> Cells;
		const FVector QueryCenter = QueryBox.GetCenter();
		
		for (int32 Level = 0; Level < AvoidanceObstacleGrid.NumLevels; Level++)
		{
			const FVector::FReal CellSize = AvoidanceObstacleGrid.GetCellSize(Level);
			const FNavigationObstacleHashGrid2D::FCellRect Rect = AvoidanceObstacleGrid.CalcQueryBounds(QueryBox, Level);
			for (int32 Y = Rect.MinY; Y <= Rect.MaxY; Y++)
			{
				for (int32 X = Rect.MinX; X <= Rect.MaxX; X++)
				{
					const FVector::FReal CenterX = (X + 0.5) * CellSize;
					const FVector::FReal CenterY = (Y + 0.5) * CellSize;
					const FVector::FReal DX = CenterX - QueryCenter.X;
					const FVector::FReal DY = CenterY - QueryCenter.Y;
					const FVector::FReal SqDist = DX * DX + DY * DY;
					FSortingCell SortCell;
					SortCell.X = X;
					SortCell.Y = Y;
					SortCell.Level = Level;
					SortCell.SqDist = SqDist;
					Cells.Add(SortCell);
				}
			}
		}

		Cells.Sort([](const FSortingCell& A, const FSortingCell& B) { return A.SqDist < B.SqDist; });

		for (const FSortingCell& SortedCell : Cells)
		{
			if (const FNavigationObstacleHashGrid2D::FCell* Cell = AvoidanceObstacleGrid.FindCell(SortedCell.X, SortedCell.Y, SortedCell.Level))
			{
				const TSparseArray<FNavigationObstacleHashGrid2D::FItem>&  Items = AvoidanceObstacleGrid.GetItems();
				for (int32 Idx = Cell->First; Idx != INDEX_NONE; Idx = Items[Idx].Next)
				{
					OutCloseEntities.Add(Items[Idx].ID);
					if (OutCloseEntities.Num() >= MaxResults)
					{
						return;
					}
				}
			}
		}
	}

	// Adapted from ray-capsule intersection: https://iquilezles.org/www/articles/intersectors/intersectors.htm
	static FVector::FReal ComputeClosestPointOfApproach(const FVector2D Pos, const FVector2D Vel, const FVector::FReal Rad, const FVector2D SegStart, const FVector2D SegEnd, const FVector::FReal TimeHoriz)
	{
		const FVector2D SegDir = SegEnd - SegStart;
		const FVector2D RelPos = Pos - SegStart;
		const FVector::FReal VelSq = FVector2D::DotProduct(Vel, Vel);
		const FVector::FReal SegDirSq = FVector2D::DotProduct(SegDir, SegDir);
		const FVector::FReal DirVelSq = FVector2D::DotProduct(SegDir, Vel);
		const FVector::FReal DirRelPosSq = FVector2D::DotProduct(SegDir, RelPos);
		const FVector::FReal VelRelPosSq = FVector2D::DotProduct(Vel, RelPos);
		const FVector::FReal RelPosSq = FVector2D::DotProduct(RelPos, RelPos);
		const FVector::FReal A = SegDirSq * VelSq - DirVelSq * DirVelSq;
		const FVector::FReal B = SegDirSq * VelRelPosSq - DirRelPosSq * DirVelSq;
		const FVector::FReal C = SegDirSq * RelPosSq - DirRelPosSq * DirRelPosSq - FMath::Square(Rad) * SegDirSq;
		const FVector::FReal H = FMath::Max(0., B*B - A*C); // b^2 - ac, Using max for closest point of arrival result when no hit.
		const FVector::FReal T = FMath::Abs(A) > SMALL_NUMBER ? (-B - FMath::Sqrt(H)) / A : 0.;
		const FVector::FReal Y = DirRelPosSq + T * DirVelSq;
		
		if (Y > 0. && Y < SegDirSq) 
		{
			return FMath::Clamp(T, 0., TimeHoriz);
		}
		else 
		{
			// caps
			const FVector2D CapRelPos = (Y <= 0.) ? RelPos : Pos - SegEnd;
			const FVector::FReal Cb = FVector2D::DotProduct(Vel, CapRelPos);
			const FVector::FReal Cc = FVector2D::DotProduct(CapRelPos, CapRelPos) - FMath::Square(Rad);
			const FVector::FReal Ch = FMath::Max(0., Cb * Cb - VelSq * Cc);
			const FVector::FReal T1 = VelSq > SMALL_NUMBER ? (-Cb - FMath::Sqrt(Ch)) / VelSq : 0.;
			return FMath::Clamp(T1, 0., TimeHoriz);
		}
	}

	static FVector::FReal ComputeClosestPointOfApproach(const FVector RelPos, const FVector RelVel, const FVector::FReal TotalRadius, const FVector::FReal TimeHoriz)
	{
		// Calculate time of impact based on relative agent positions and velocities.
		const FVector::FReal A = FVector::DotProduct(RelVel, RelVel);
		const FVector::FReal Inv2A = A > SMALL_NUMBER ? 1. / (2. * A) : 0.;
		const FVector::FReal B = FMath::Min(0., 2. * FVector::DotProduct(RelVel, RelPos));
		const FVector::FReal C = FVector::DotProduct(RelPos, RelPos) - FMath::Square(TotalRadius);
		// Using max() here gives us CPA (closest point on arrival) when there is no hit.
		const FVector::FReal Discr = FMath::Sqrt(FMath::Max(0., B * B - 4. * A * C));
		const FVector::FReal T = (-B - Discr) * Inv2A;
		return FMath::Clamp(T, 0., TimeHoriz);
	}

	static bool UseDrawDebugHelper()
	{
		return Tweakables::bUseDrawDebugHelpers;
	}

#if WITH_MASSGAMEPLAY_DEBUG	

	// Colors
	static const FColor CurrentAgentColor = FColor::Emerald;

	static const FColor VelocityColor = FColor::Black;
	static const FColor PrefVelocityColor = FColor::Red;
	static const FColor DesiredVelocityColor = FColor::Yellow;
	static const FColor FinalSteeringForceColor = FColor::Cyan;
	static constexpr float BigArrowThickness = 6.f;
	static constexpr float BigArrowHeadSize = 12.f;

	// Agents colors
	static const FColor AgentsColor = FColor::Orange;
	static const FColor AgentSeparationForceColor = FColor(255, 145, 71);	// Orange red
	static const FColor AgentAvoidForceColor = AgentsColor;
	
	// Obstacles colors
	static const FColor ObstacleColor = FColor::Blue;
	static const FColor ObstacleContactNormalColor = FColor::Silver;
	static const FColor ObstacleAvoidForceColor = FColor::Magenta;
	static const FColor ObstacleSeparationForceColor = FColor(255, 66, 66);	// Bright red
	
	static const FVector DebugAgentHeightOffset = FVector(0., 0., 185.);
	static const FVector DebugLowCylinderOffset = FVector(0., 0., 20.);

	//----------------------------------------------------------------------//
	// Begin MassDebugUtils
	// @todo: Extract those generic debug functions to a separate location
	//----------------------------------------------------------------------//
	struct FDebugContext
	{
		FDebugContext(const UObject* InLogOwner, const FLogCategoryBase& InCategory, const UWorld* InWorld, const FMassEntityHandle InEntity)
			: LogOwner(InLogOwner)
			, Category(InCategory)
			, World(InWorld)
			, Entity(InEntity)
		{}

		const UObject* LogOwner;
		const FLogCategoryBase& Category;
		const UWorld* World;
		const FMassEntityHandle Entity;
	};

	static bool DebugIsSelected(const FMassEntityHandle Entity)
	{
		FColor Color;
		return UE::Mass::Debug::IsDebuggingEntity(Entity, &Color);
	}

	static void DebugDrawLine(const FDebugContext& Context, const FVector& Start, const FVector& End, const FColor& Color, const float Thickness = 0.f, const bool bPersistent = false)
	{
		if (!DebugIsSelected(Context.Entity))
		{
			return;
		}

		UE_VLOG_SEGMENT_THICK(Context.LogOwner, Context.Category, Log, Start, End, Color, (int16)Thickness, TEXT(""));

		if (UseDrawDebugHelper() && Context.World)
		{
			DrawDebugLine(Context.World, Start, End, Color, bPersistent, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
		}
	}

	static void DebugDrawArrow(const FDebugContext& Context, const FVector& Start, const FVector& End, const FColor& Color, const float HeadSize = 8.f, const float Thickness = 1.5f)
	{
		if (!DebugIsSelected(Context.Entity))
		{
			return;
		}

		const FVector::FReal Pointyness = 1.8;
		const FVector Line = End - Start;
		const FVector UnitV = Line.GetSafeNormal();
		const FVector Perp = FVector::CrossProduct(UnitV, FVector::UpVector);
		const FVector Left = Perp - (Pointyness*UnitV);
		const FVector Right = -Perp - (Pointyness*UnitV);
		UE_VLOG_SEGMENT_THICK(Context.LogOwner, Context.Category, Log, Start, End, Color, (int16)Thickness, TEXT(""));
		UE_VLOG_SEGMENT_THICK(Context.LogOwner, Context.Category, Log, End, End + HeadSize * Left, Color, (int16)Thickness, TEXT(""));
		UE_VLOG_SEGMENT_THICK(Context.LogOwner, Context.Category, Log, End, End + HeadSize * Right, Color, (int16)Thickness, TEXT(""));

		if (UseDrawDebugHelper() && Context.World)
		{
			DrawDebugLine(Context.World, Start, End, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
			DrawDebugLine(Context.World, End, End + HeadSize * Left, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
			DrawDebugLine(Context.World, End, End + HeadSize * Right, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
		}
	}

	static void DebugDrawSphere(const FDebugContext& Context, const FVector& Center, const float Radius, const FColor& Color)
	{
		if (!DebugIsSelected(Context.Entity))
		{
			return;
		}

		UE_VLOG_LOCATION(Context.LogOwner, Context.Category, Log, Center, Radius, Color, TEXT(""));

		if (UseDrawDebugHelper() && Context.World)
		{
			DrawDebugSphere(Context.World, Center, Radius, /*segments = */16, Color);
		}
	}

	static void DebugDrawBox(const FDebugContext& Context, const FBox& Box, const FColor& Color)
	{
		if (!DebugIsSelected(Context.Entity))
		{
			return;
		}

		UE_VLOG_BOX(Context.LogOwner, Context.Category, Log, Box, Color, TEXT(""));
		
		if (UseDrawDebugHelper() && Context.World)
		{
			DrawDebugBox(Context.World, Box.GetCenter(), Box.GetExtent(), Color);
		}
	}
	
	static void DebugDrawCylinder(const FDebugContext& Context, const FVector& Bottom, const FVector& Top, const float Radius, const FColor& Color, const FString& Text = FString())
	{
		if (!DebugIsSelected(Context.Entity))
		{
			return;
		}

		UE_VLOG_CYLINDER(Context.LogOwner, Context.Category, Log, Bottom, Top, Radius, Color, TEXT("%s"), *Text);

		if (UseDrawDebugHelper() && Context.World)
		{
			DrawDebugCylinder(Context.World, Bottom, Top, Radius, /*segments = */24, Color);
		}
	}
	//----------------------------------------------------------------------//
	// End MassDebugUtils
	//----------------------------------------------------------------------//


	// Local debug utils
	static void DebugDrawVelocity(const FDebugContext& Context, const FVector& Start, const FVector& End, const FColor& Color)
	{
		// Different arrow than DebugDrawArrow()
		if (!DebugIsSelected(Context.Entity))
		{
			return;
		}

		const float Thickness = 3.f;
		const FVector::FReal Pointyness = 1.8;
		const FVector Line = End - Start;
		const FVector UnitV = Line.GetSafeNormal();
		const FVector Perp = FVector::CrossProduct(UnitV, FVector::UpVector);
		const FVector Left = Perp - (Pointyness * UnitV);
		const FVector Right = -Perp - (Pointyness * UnitV);
		const FVector::FReal HeadSize = 0.08 * Line.Size();
		UE_VLOG_SEGMENT_THICK(Context.LogOwner, Context.Category, Log, Start, End, Color, (int16)Thickness, TEXT(""));
		UE_VLOG_SEGMENT_THICK(Context.LogOwner, Context.Category, Log, End, End + HeadSize * Left, Color, (int16)Thickness, TEXT(""));
		UE_VLOG_SEGMENT_THICK(Context.LogOwner, Context.Category, Log, End, End + HeadSize * Right, Color, (int16)Thickness, TEXT(""));
		UE_VLOG_SEGMENT_THICK(Context.LogOwner, Context.Category, Log, End + HeadSize * Left, End + HeadSize * Right, Color, (int16)Thickness, TEXT(""));

		if (UseDrawDebugHelper() && Context.World)
		{
			DrawDebugLine(Context.World, Start, End, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
			DrawDebugLine(Context.World, End, End + HeadSize * Left, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
			DrawDebugLine(Context.World, End, End + HeadSize * Right, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
			DrawDebugLine(Context.World, End + HeadSize * Left, End + HeadSize * Right, Color, /*bPersistent=*/ false, /*LifeTime =*/ -1.f, /*DepthPriority =*/ 0, Thickness);
		}
	}

	static void DebugDrawForce(const FDebugContext& Context, const FVector& Start, const FVector& End, const FColor& Color)
	{
		DebugDrawArrow(Context, Start, End, Color, /*HeadSize*/4.f, /*Thickness*/3.f);
	}

	static void DebugDrawSummedForce(const FDebugContext& Context, const FVector& Start, const FVector& End, const FColor& Color)
	{
		DebugDrawArrow(Context, Start + FVector(0.,0.,1.), End + FVector(0., 0., 1.), Color, /*HeadSize*/8.f, /*Thickness*/6.f);
	}

#endif // WITH_MASSGAMEPLAY_DEBUG

} // namespace UE::MassAvoidance


//----------------------------------------------------------------------//
//  UMassMovingAvoidanceProcessor
//----------------------------------------------------------------------//
UMassMovingAvoidanceProcessor::UMassMovingAvoidanceProcessor()
	: EntityQuery(*this) 
{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
	ExecutionOrder.ExecuteAfter.Add(UE::Mass::ProcessorGroupNames::LOD);
}

void UMassMovingAvoidanceProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNavigationEdgesFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassMediumLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassLowLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddConstSharedRequirement<FMassMovingAvoidanceParameters>(EMassFragmentPresence::All);
	EntityQuery.AddConstSharedRequirement<FMassMovementParameters>(EMassFragmentPresence::All);
}

void UMassMovingAvoidanceProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	World = Owner.GetWorld();
	NavigationSubsystem = UWorld::GetSubsystem<UMassNavigationSubsystem>(Owner.GetWorld());
}

void UMassMovingAvoidanceProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassMovingAvoidanceProcessor);

	if (!World || !NavigationSubsystem)
	{
		return;
	}

	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, &EntityManager](FMassExecutionContext& Context)
	{
		const float DeltaTime = Context.GetDeltaTimeSeconds();
		const double CurrentTime = World->GetTimeSeconds();
		const int32 NumEntities = Context.GetNumEntities();
		
		const TArrayView<FMassForceFragment> ForceList = Context.GetMutableFragmentView<FMassForceFragment>();
		const TConstArrayView<FMassNavigationEdgesFragment> NavEdgesList = Context.GetFragmentView<FMassNavigationEdgesFragment>();
		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FMassVelocityFragment> VelocityList = Context.GetFragmentView<FMassVelocityFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();
		const FMassMovingAvoidanceParameters& MovingAvoidanceParams = Context.GetConstSharedFragment<FMassMovingAvoidanceParameters>();
		const FMassMovementParameters& MovementParams = Context.GetConstSharedFragment<FMassMovementParameters>();

		const FVector::FReal InvPredictiveAvoidanceTime = 1. / MovingAvoidanceParams.PredictiveAvoidanceTime;

		// Arrays used to store close obstacles
		TArray<FMassNavigationObstacleItem, TFixedAllocator<UE::MassAvoidance::MaxObstacleResults>> CloseEntities;

		// Used for storing sorted list or nearest obstacles.
		struct FSortedObstacle
		{
			FVector LocationCached;
			FVector Forward;
			FMassNavigationObstacleItem ObstacleItem;
			FVector::FReal SqDist;
		};
		TArray<FSortedObstacle, TFixedAllocator<UE::MassAvoidance::MaxObstacleResults>> ClosestObstacles;

		// Potential contact between agent and environment. 
		struct FEnvironmentContact
		{
			FVector Position = FVector::ZeroVector;
			FVector Normal = FVector::ZeroVector;
			FVector::FReal Distance = 0.;
		};
		TArray<FEnvironmentContact, TInlineAllocator<16>> Contacts;

		// Describes collider to avoid, collected from neighbour obstacles.
		struct FCollider
		{
			FVector Location = FVector::ZeroVector;
			FVector Velocity = FVector::ZeroVector;
			float Radius = 0.f;
			bool bCanAvoid = true;
			bool bIsMoving = false;
		};
		TArray<FCollider, TInlineAllocator<16>> Colliders;

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			// @todo: this check should eventually be part of the query (i.e. only handle moving agents).
			const FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIndex];
			if (MoveTarget.GetCurrentAction() == EMassMovementAction::Animate || MoveTarget.GetCurrentAction() == EMassMovementAction::Stand)
			{
				continue;
			}

			FMassEntityHandle Entity = Context.GetEntity(EntityIndex);
			const FMassNavigationEdgesFragment& NavEdges = NavEdgesList[EntityIndex];
			const FTransformFragment& Location = LocationList[EntityIndex];
			const FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
			const FAgentRadiusFragment& Radius = RadiusList[EntityIndex];
			FMassForceFragment& Force = ForceList[EntityIndex];

			// Smaller steering max accel makes the steering more "calm" but less opportunistic, may not find solution, or gets stuck.
			// Max contact accel should be quite a big bigger than steering so that collision response is firm. 
			const FVector::FReal MaxSteerAccel = MovementParams.MaxAcceleration;
			const FVector::FReal MaximumSpeed = MovementParams.MaxSpeed;

			const FVector AgentLocation = Location.GetTransform().GetTranslation();
			const FVector AgentVelocity = FVector(Velocity.Value.X, Velocity.Value.Y, 0.);
			
			const FVector::FReal AgentRadius = Radius.Radius;
			const FVector::FReal SeparationAgentRadius = Radius.Radius * MovingAvoidanceParams.SeparationRadiusScale;
			const FVector::FReal PredictiveAvoidanceAgentRadius = Radius.Radius * MovingAvoidanceParams.PredictiveAvoidanceRadiusScale;
			
			FVector SteeringForce = Force.Value;

			// Near start and end fades are used to subdue the avoidance at the start and end of the path.
			FVector::FReal NearStartFade = 1.;
			FVector::FReal NearEndFade = 1.;

			if (MoveTarget.GetPreviousAction() != EMassMovementAction::Move)
			{
				// Fade in avoidance when transitioning from other than move action.
				// I.e. the standing behavior may move the agents so close to each,
				// and that causes the separation to push them out quickly when avoidance is activated. 
				NearStartFade = FMath::Min((CurrentTime - MoveTarget.GetCurrentActionStartTime()) / MovingAvoidanceParams.StartOfPathDuration, 1.);
			}

			if (MoveTarget.IntentAtGoal == EMassMovementAction::Stand)
			{
				// Estimate approach based on current desired speed.
				const FVector::FReal ApproachDistance = FMath::Max<FVector::FReal>(1., MovingAvoidanceParams.EndOfPathDuration * MoveTarget.DesiredSpeed.Get());
				NearEndFade = FMath::Clamp(MoveTarget.DistanceToGoal / ApproachDistance, 0., 1.);
			}
			
			const FVector::FReal NearStartScaling = FMath::Lerp<FVector::FReal>(MovingAvoidanceParams.StartOfPathAvoidanceScale, 1., NearStartFade);
			const FVector::FReal NearEndScaling = FMath::Lerp<FVector::FReal>(MovingAvoidanceParams.EndOfPathAvoidanceScale, 1., NearEndFade);
			
#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
			const UE::MassAvoidance::FDebugContext BaseDebugContext(this, LogAvoidance, World, Entity);
			const UE::MassAvoidance::FDebugContext VelocitiesDebugContext(this, LogAvoidanceVelocities, World, Entity);
			const UE::MassAvoidance::FDebugContext ObstacleDebugContext(this, LogAvoidanceObstacles, World, Entity);
			const UE::MassAvoidance::FDebugContext AgentDebugContext(this, LogAvoidanceAgents, World, Entity);
			
			if (UE::MassAvoidance::DebugIsSelected(Entity))
			{
				// Draw agent
				const FString Text = FString::Printf(TEXT("%i"), Entity.Index);
				DebugDrawCylinder(BaseDebugContext, AgentLocation, AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset, static_cast<float>(AgentRadius+1.), UE::MassAvoidance::CurrentAgentColor, Text);

				DebugDrawSphere(BaseDebugContext, AgentLocation, 10.f, UE::MassAvoidance::CurrentAgentColor);

				// Draw current velocity (black)
				DebugDrawVelocity(VelocitiesDebugContext, AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset, AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset + AgentVelocity, UE::MassAvoidance::VelocityColor);

				// Draw preferred velocity (red)
//				DebugDrawVelocity(VelocitiesDebugContext, AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset, AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset + PrefVelocity, UE::MassAvoidance::PrefVelocityColor);

				// Draw initial steering force
				DebugDrawArrow(BaseDebugContext, AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset, AgentLocation +UE::MassAvoidance:: DebugAgentHeightOffset + SteeringForce, UE::MassAvoidance::CurrentAgentColor, UE::MassAvoidance::BigArrowHeadSize, UE::MassAvoidance::BigArrowThickness);

				// Draw center
				DebugDrawSphere(BaseDebugContext, AgentLocation, /*Radius*/2.f, UE::MassAvoidance::CurrentAgentColor);
			}
#endif // WITH_MASSGAMEPLAY_DEBUG

			FVector OldSteeringForce = FVector::ZeroVector;

			//////////////////////////////////////////////////////////////////////////
			// Environment avoidance.
			//
			
			if (!MoveTarget.bOffBoundaries && UE::MassAvoidance::Tweakables::bEnableEnvironmentAvoidance)
			{
				const FVector DesiredAcceleration = UE::MassNavigation::ClampVector(SteeringForce, MaxSteerAccel);
				const FVector DesiredVelocity = UE::MassNavigation::ClampVector(AgentVelocity + DesiredAcceleration * DeltaTime, MaximumSpeed);

#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
				// Draw desired velocity (yellow)
				UE::MassAvoidance::DebugDrawVelocity(VelocitiesDebugContext, AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset, AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset + DesiredVelocity, UE::MassAvoidance::DesiredVelocityColor);
#endif // WITH_MASSGAMEPLAY_DEBUG

				OldSteeringForce = SteeringForce;
				Contacts.Reset();

				// Collect potential contacts between agent and environment edges.
				for (const FNavigationAvoidanceEdge& Edge : NavEdges.AvoidanceEdges)
				{
					const FVector EdgeDiff = Edge.End - Edge.Start;
					FVector EdgeDir = FVector::ZeroVector;
					FVector::FReal EdgeLength = 0.;
					EdgeDiff.ToDirectionAndLength(EdgeDir, EdgeLength);

					const FVector AgentToEdgeStart = AgentLocation - Edge.Start;
					const FVector::FReal DistAlongEdge = FVector::DotProduct(EdgeDir, AgentToEdgeStart);
					const FVector::FReal DistAwayFromEdge = FVector::DotProduct(Edge.LeftDir, AgentToEdgeStart);

					FVector::FReal ConDist = 0.;
					FVector ConNorm = FVector::ForwardVector;
					FVector ConPos = FVector::ZeroVector;
					bool bDirectlyBehindEdge = false;
					
					if (DistAwayFromEdge < 0.)
					{
						// Inside or behind the edge
						if (DistAlongEdge < 0.)
						{
							ConPos = Edge.Start;
							ConNorm = -EdgeDir;
							ConDist = -DistAlongEdge;
						}
						else if (DistAlongEdge > EdgeLength)
						{
							ConPos = Edge.End;
							ConNorm = EdgeDir;
							ConDist = DistAlongEdge;
						}
						else
						{
							ConPos = Edge.Start + EdgeDir * DistAlongEdge;
							ConNorm = Edge.LeftDir;
							ConDist = 0.;
							bDirectlyBehindEdge = true;
						}
					}
					else
					{
						if (DistAlongEdge < 0.)
						{
							// Start Corner
							ConPos = Edge.Start;
							EdgeDiff.ToDirectionAndLength(ConNorm, ConDist);
						}
						else if (DistAlongEdge > EdgeLength)
						{
							// End Corner
							ConPos = Edge.End;
							EdgeDiff.ToDirectionAndLength(ConNorm, ConDist);
						}
						else
						{
							// Front
							ConPos = Edge.Start + EdgeDir * DistAlongEdge;
							ConNorm = Edge.LeftDir;
							ConDist = DistAwayFromEdge;
						}
					}
					
					// Check to merge contacts
					bool bAdd = true;
					for (int ContactIndex = 0; ContactIndex < Contacts.Num(); ContactIndex++)
					{
						if (FVector::DotProduct(Contacts[ContactIndex].Normal, ConNorm) > 0. && FMath::Abs(FVector::DotProduct(ConNorm, Contacts[ContactIndex].Position - ConPos)) < (10./*cm*/))
						{
							// Contacts are on same place, merge
							if (ConDist < Contacts[ContactIndex].Distance)
							{
								// New is closer, override.
								Contacts[ContactIndex].Position = ConPos;
								Contacts[ContactIndex].Normal = ConNorm;
								Contacts[ContactIndex].Distance = ConDist;
							}
							bAdd = false;
							break;
						}
					}

					// Not found, add new contact
					if (bAdd)
					{
						FEnvironmentContact Contact;
						Contact.Position = ConPos;
						Contact.Normal = ConNorm;
						Contact.Distance = ConDist;
						Contacts.Add(Contact);
					}

					// Skip predictive avoidance when behind the edge.
					if (!bDirectlyBehindEdge)
					{
						// Avoid edges
						const FVector::FReal CPA = UE::MassAvoidance::ComputeClosestPointOfApproach(FVector2D(AgentLocation), FVector2D(DesiredVelocity), AgentRadius,
							FVector2D(Edge.Start), FVector2D(Edge.End), MovingAvoidanceParams.PredictiveAvoidanceTime);
						const FVector HitAgentPos = AgentLocation + DesiredVelocity * CPA;
						const FVector::FReal EdgeT = UE::MassNavigation::ProjectPtSeg(FVector2D(HitAgentPos), FVector2D(Edge.Start), FVector2D(Edge.End));
						const FVector HitObPos = FMath::Lerp(Edge.Start, Edge.End, EdgeT);

						// Calculate penetration at CPA
						FVector AvoidRelPos = HitAgentPos - HitObPos;
						AvoidRelPos.Z = 0.;	// @todo AT: ignore the z component for now until we clamp the height of obstacles
						const FVector::FReal AvoidDist = AvoidRelPos.Size();
						const FVector AvoidNormal = AvoidDist > 0. ? (AvoidRelPos / AvoidDist) : FVector::ForwardVector;

						const FVector::FReal AvoidPen = (PredictiveAvoidanceAgentRadius + MovingAvoidanceParams.PredictiveAvoidanceDistance) - AvoidDist;
						const FVector::FReal AvoidMag = FMath::Square(FMath::Clamp(AvoidPen / MovingAvoidanceParams.PredictiveAvoidanceDistance, 0., 1.));
						const FVector::FReal AvoidMagDist = 1. + FMath::Square(1. - CPA * InvPredictiveAvoidanceTime);
						const FVector AvoidForce = AvoidNormal * AvoidMag * AvoidMagDist * MovingAvoidanceParams.EnvironmentPredictiveAvoidanceStiffness * NearEndScaling; // Predictive avoidance against environment is tuned down towards the end of the path

						SteeringForce += AvoidForce;

#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
						// Draw contact normal
						UE::MassAvoidance::DebugDrawArrow(ObstacleDebugContext, ConPos, ConPos + 50. * ConNorm, UE::MassAvoidance::ObstacleContactNormalColor, /*HeadSize=*/ 5.f);
						UE::MassAvoidance::DebugDrawSphere(ObstacleDebugContext, ConPos, 2.5f, UE::MassAvoidance::ObstacleContactNormalColor);

						// Draw hit pos with edge
						UE::MassAvoidance::DebugDrawLine(ObstacleDebugContext, AgentLocation, HitAgentPos, UE::MassAvoidance::ObstacleAvoidForceColor);
						UE::MassAvoidance::DebugDrawCylinder(ObstacleDebugContext, HitAgentPos, HitAgentPos + UE::MassAvoidance::DebugAgentHeightOffset, static_cast<float>(AgentRadius), UE::MassAvoidance::ObstacleAvoidForceColor);

						// Draw avoid obstacle force
						UE::MassAvoidance::DebugDrawForce(ObstacleDebugContext, HitObPos, HitObPos + AvoidForce, UE::MassAvoidance::ObstacleAvoidForceColor);
#endif // WITH_MASSGAMEPLAY_DEBUG
					}
				} // edge loop

#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
				// Draw total steering force to avoid obstacles
				const FVector EnvironmentAvoidSteeringForce = SteeringForce - OldSteeringForce;
				UE::MassAvoidance::DebugDrawSummedForce(ObstacleDebugContext,
					AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset,
					AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset + EnvironmentAvoidSteeringForce,
					UE::MassAvoidance::ObstacleAvoidForceColor);
#endif // WITH_MASSGAMEPLAY_DEBUG

				// Process contacts to add edge separation force
				const FVector SteeringForceBeforeSeparation = SteeringForce;
				for (int ContactIndex = 0; ContactIndex < Contacts.Num(); ContactIndex++) 
				{
					const FVector ConNorm = Contacts[ContactIndex].Normal.GetSafeNormal();
					const FVector::FReal ContactDist = Contacts[ContactIndex].Distance;

					// Separation force (stay away from obstacles if possible)
					const FVector::FReal SeparationPenalty = (SeparationAgentRadius + MovingAvoidanceParams.EnvironmentSeparationDistance) - ContactDist;
					const FVector::FReal SeparationMag = UE::MassNavigation::Smooth(FMath::Clamp(SeparationPenalty / MovingAvoidanceParams.EnvironmentSeparationDistance, 0., 1.));
					const FVector SeparationForce = ConNorm * MovingAvoidanceParams.EnvironmentSeparationStiffness * SeparationMag;

					SteeringForce += SeparationForce;

#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
					// Draw individual contact forces
					DebugDrawForce(ObstacleDebugContext, Contacts[ContactIndex].Position + UE::MassAvoidance::DebugAgentHeightOffset,
					Contacts[ContactIndex].Position + SeparationForce + UE::MassAvoidance::DebugAgentHeightOffset, UE::MassAvoidance::ObstacleSeparationForceColor);
#endif // WITH_MASSGAMEPLAY_DEBUG
				}
				
#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
				// Draw total steering force to separate from close edges
				const FVector TotalSeparationForce = SteeringForce - SteeringForceBeforeSeparation;
				DebugDrawSummedForce(ObstacleDebugContext,
					AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset,
					AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset + TotalSeparationForce,
					UE::MassAvoidance::ObstacleSeparationForceColor);

				// Display close obstacle edges
				if (UE::MassAvoidance::DebugIsSelected(Entity))
				{
					for (const FNavigationAvoidanceEdge& Edge : NavEdges.AvoidanceEdges)
					{
						DebugDrawLine(ObstacleDebugContext, UE::MassAvoidance::DebugAgentHeightOffset + Edge.Start,
							UE::MassAvoidance::DebugAgentHeightOffset + Edge.End, UE::MassAvoidance::ObstacleColor, /*Thickness=*/2.f);
						const FVector Middle = UE::MassAvoidance::DebugAgentHeightOffset + 0.5f * (Edge.Start + Edge.End);
						DebugDrawArrow(ObstacleDebugContext, Middle, Middle + 10. * FVector::CrossProduct((Edge.End - Edge.Start), FVector::UpVector).GetSafeNormal(), UE::MassAvoidance::ObstacleColor, /*HeadSize=*/2.f);
					}
				}
#endif // WITH_MASSGAMEPLAY_DEBUG
			}

			//////////////////////////////////////////////////////////////////////////
			// Avoid close agents

			// Update desired velocity based on avoidance so far.
			const FVector DesAcc = UE::MassNavigation::ClampVector(SteeringForce, MaxSteerAccel);
			const FVector DesVel = UE::MassNavigation::ClampVector(AgentVelocity + DesAcc * DeltaTime, MaximumSpeed);

			// Find close obstacles
			const FNavigationObstacleHashGrid2D& AvoidanceObstacleGrid = NavigationSubsystem->GetObstacleGridMutable();
			UE::MassAvoidance::FindCloseObstacles(AgentLocation, MovingAvoidanceParams.ObstacleDetectionDistance, AvoidanceObstacleGrid, CloseEntities, UE::MassAvoidance::MaxObstacleResults);

			// Remove unwanted and find the closests in the CloseEntities
			const FVector::FReal DistanceCutOffSqr = FMath::Square(MovingAvoidanceParams.ObstacleDetectionDistance);
			ClosestObstacles.Reset();
			for (const FNavigationObstacleHashGrid2D::ItemIDType OtherEntity : CloseEntities)
			{
				// Skip self
				if (OtherEntity.Entity == Entity)
				{
					continue;
				}

				// Skip invalid entities.
				if (!EntityManager.IsEntityValid(OtherEntity.Entity))
				{
					UE_LOG(LogAvoidanceObstacles, VeryVerbose, TEXT("Close entity is invalid, skipped."));
					continue;
				}
				
				// Skip too far
				const FTransform& Transform = EntityManager.GetFragmentDataChecked<FTransformFragment>(OtherEntity.Entity).GetTransform();
				const FVector OtherLocation = Transform.GetLocation();
				
				const FVector::FReal SqDist = FVector::DistSquared(AgentLocation, OtherLocation);
				if (SqDist > DistanceCutOffSqr)
				{
					continue;
				}

				FSortedObstacle Obstacle;
				Obstacle.LocationCached = OtherLocation;
				Obstacle.Forward = Transform.GetRotation().GetForwardVector();
				Obstacle.ObstacleItem = OtherEntity;
				Obstacle.SqDist = SqDist;
				ClosestObstacles.Add(Obstacle);
			}
			ClosestObstacles.Sort([](const FSortedObstacle& A, const FSortedObstacle& B) { return A.SqDist < B.SqDist; });

			// Compute forces
			OldSteeringForce = SteeringForce;
			FVector TotalAgentSeparationForce = FVector::ZeroVector;

			// Fill collider list from close agents
			Colliders.Reset();
			constexpr int32 MaxColliders = 6;
			for (int32 Index = 0; Index < ClosestObstacles.Num(); Index++)
			{
				if (Colliders.Num() >= MaxColliders)
				{
					break;
				}

				FSortedObstacle& Obstacle = ClosestObstacles[Index];
				FMassEntityView OtherEntityView(EntityManager, Obstacle.ObstacleItem.Entity);

				const FMassVelocityFragment* OtherVelocityFragment = OtherEntityView.GetFragmentDataPtr<FMassVelocityFragment>();
				const FVector OtherVelocity = OtherVelocityFragment != nullptr ? OtherVelocityFragment->Value : FVector::ZeroVector; // Get velocity from FAvoidanceComponent

				// @todo: this is heavy fragment to access, see if we could handle this differently.
				const FMassMoveTargetFragment* OtherMoveTarget = OtherEntityView.GetFragmentDataPtr<FMassMoveTargetFragment>();
				const bool bCanAvoid = OtherMoveTarget != nullptr;
				const bool bOtherIsMoving = OtherMoveTarget ? OtherMoveTarget->GetCurrentAction() == EMassMovementAction::Move : true; // Assume moving if other does not have move target.
				
				// Check for colliders data
				if (EnumHasAnyFlags(Obstacle.ObstacleItem.ItemFlags, EMassNavigationObstacleFlags::HasColliderData))
				{
					if (const FMassAvoidanceColliderFragment* ColliderFragment = OtherEntityView.GetFragmentDataPtr<FMassAvoidanceColliderFragment>())
					{
						if (ColliderFragment->Type == EMassColliderType::Circle)
						{
							const FMassCircleCollider Circle = ColliderFragment->GetCircleCollider();
							
							FCollider& Collider = Colliders.Add_GetRef(FCollider{});
							Collider.Velocity = OtherVelocity;
							Collider.bCanAvoid = bCanAvoid;
							Collider.bIsMoving = bOtherIsMoving;
							Collider.Radius = Circle.Radius;
							Collider.Location = Obstacle.LocationCached;
						}
						else if (ColliderFragment->Type == EMassColliderType::Pill)
						{
							const FMassPillCollider Pill = ColliderFragment->GetPillCollider(); 

							FCollider& Collider = Colliders.Add_GetRef(FCollider{});
							Collider.Velocity = OtherVelocity;
							Collider.bCanAvoid = bCanAvoid;
							Collider.bIsMoving = bOtherIsMoving;
							Collider.Radius = Pill.Radius;
							Collider.Location = Obstacle.LocationCached + (Pill.HalfLength * Obstacle.Forward);

							if (Colliders.Num() < MaxColliders)
							{
								FCollider& Collider2 = Colliders.Add_GetRef(FCollider{});
								Collider2.Velocity = OtherVelocity;
								Collider2.bCanAvoid = bCanAvoid;
								Collider2.bIsMoving = bOtherIsMoving;
								Collider2.Radius = Pill.Radius;
								Collider2.Location = Obstacle.LocationCached + (-Pill.HalfLength * Obstacle.Forward);
							}
						}
					}
				}
				else
				{
					FCollider& Collider = Colliders.Add_GetRef(FCollider{});
					Collider.Location = Obstacle.LocationCached;
					Collider.Velocity = OtherVelocity;
					Collider.Radius = OtherEntityView.GetFragmentData<FAgentRadiusFragment>().Radius;
					Collider.bCanAvoid = bCanAvoid;
					Collider.bIsMoving = bOtherIsMoving;
				}
			}

			// Process colliders for avoidance
			for (const FCollider& Collider : Colliders)
			{
				bool bHasForcedNormal = false;
				FVector ForcedNormal = FVector::ZeroVector;

				if (Collider.bCanAvoid == false)
				{
					// If the other obstacle cannot avoid us, try to avoid the local minima they create between the wall and their collider.
					// If the space between edge and collider is less than MinClearance, make the agent to avoid the gap.
					const FVector::FReal MinClearance = 2. * AgentRadius * MovingAvoidanceParams.StaticObstacleClearanceScale;
					
					// Find the maximum distance from edges that are too close.
					FVector::FReal MaxDist = -1.;
					FVector ClosestPoint = FVector::ZeroVector;
					for (const FNavigationAvoidanceEdge& Edge : NavEdges.AvoidanceEdges)
					{
						const FVector Point = FMath::ClosestPointOnSegment(Collider.Location, Edge.Start, Edge.End);
						const FVector Offset = Collider.Location - Point;
						if (FVector::DotProduct(Offset, Edge.LeftDir) < 0.)
						{
							// Behind the edge, ignore.
							continue;
						}

						const FVector::FReal OffsetLength = Offset.Length();
						const bool bTooNarrow = (OffsetLength - Collider.Radius) < MinClearance; 
						if (bTooNarrow)
						{
							if (OffsetLength > MaxDist)
							{
								MaxDist = OffsetLength;
								ClosestPoint = Point;
							}
						}
					}

					if (MaxDist != -1.)
					{
						// Set up forced normal to avoid the gap between collider and edge.
						ForcedNormal = (Collider.Location - ClosestPoint).GetSafeNormal();
						bHasForcedNormal = true;
					}
				}

				FVector RelPos = AgentLocation - Collider.Location;
				RelPos.Z = 0.; // we assume we work on a flat plane for now
				const FVector RelVel = DesVel - Collider.Velocity;
				const FVector::FReal ConDist = RelPos.Size();
				const FVector ConNorm = ConDist > 0. ? RelPos / ConDist : FVector::ForwardVector;

				FVector SeparationNormal = ConNorm;
				if (bHasForcedNormal)
				{
					// The more head on the collisions is, the more we should avoid towards the forced direction.
					const FVector RelVelNorm = RelVel.GetSafeNormal();
					const FVector::FReal Blend = FMath::Max(0., -FVector::DotProduct(ConNorm, RelVelNorm));
					SeparationNormal = FMath::Lerp(ConNorm, ForcedNormal, Blend).GetSafeNormal();
				}
				
				const FVector::FReal StandingScaling = Collider.bIsMoving ? 1. : MovingAvoidanceParams.StandingObstacleAvoidanceScale; // Care less about standing agents so that we can push through standing crowd.
				
				// Separation force (stay away from agents if possible)
				const FVector::FReal PenSep = (SeparationAgentRadius + Collider.Radius + MovingAvoidanceParams.ObstacleSeparationDistance) - ConDist;
				const FVector::FReal SeparationMag = FMath::Square(FMath::Clamp(PenSep / MovingAvoidanceParams.ObstacleSeparationDistance, 0., 1.));
				const FVector SepForce = SeparationNormal * MovingAvoidanceParams.ObstacleSeparationStiffness;
				const FVector SeparationForce = SepForce * SeparationMag * StandingScaling;

				SteeringForce += SeparationForce;
				TotalAgentSeparationForce += SeparationForce;

				// Calculate closest point of approach based on relative agent positions and velocities.
				const FVector::FReal CPA = UE::MassAvoidance::ComputeClosestPointOfApproach(RelPos, RelVel, PredictiveAvoidanceAgentRadius + Collider.Radius, MovingAvoidanceParams.PredictiveAvoidanceTime);

				// Calculate penetration at CPA
				const FVector AvoidRelPos = RelPos + RelVel * CPA;
				const FVector::FReal AvoidDist = AvoidRelPos.Size();
				const FVector AvoidConNormal = AvoidDist > 0. ? (AvoidRelPos / AvoidDist) : FVector::ForwardVector;

				FVector AvoidNormal = AvoidConNormal;
				if (bHasForcedNormal)
				{
					// The more head on the predicted collisions is, the more we should avoid towards the forced direction.
					const FVector RelVelNorm = RelVel.GetSafeNormal();
					const FVector::FReal Blend = FMath::Max(0., -FVector::DotProduct(AvoidConNormal, RelVelNorm));
					AvoidNormal = FMath::Lerp(AvoidConNormal, ForcedNormal, Blend).GetSafeNormal();
				}
				
				const FVector::FReal AvoidPenetration = (PredictiveAvoidanceAgentRadius + Collider.Radius + MovingAvoidanceParams.PredictiveAvoidanceDistance) - AvoidDist; // Based on future agents distance
				const FVector::FReal AvoidMag = FMath::Square(FMath::Clamp(AvoidPenetration / MovingAvoidanceParams.PredictiveAvoidanceDistance, 0., 1.));
				const FVector::FReal AvoidMagDist = (1. - (CPA * InvPredictiveAvoidanceTime)); // No clamp, CPA is between 0 and PredictiveAvoidanceTime
				const FVector AvoidForce = AvoidNormal * AvoidMag * AvoidMagDist * MovingAvoidanceParams.ObstaclePredictiveAvoidanceStiffness * StandingScaling;

				SteeringForce += AvoidForce;

#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
				// Display close agent
				UE::MassAvoidance::DebugDrawCylinder(AgentDebugContext, Collider.Location, Collider.Location + UE::MassAvoidance::DebugLowCylinderOffset, Collider.Radius, UE::MassAvoidance::AgentsColor);

				if (bHasForcedNormal)
				{
					UE::MassAvoidance::DebugDrawCylinder(BaseDebugContext, Collider.Location, Collider.Location + UE::MassAvoidance::DebugAgentHeightOffset, Collider.Radius, FColor::Red);
				}

				// Draw agent contact separation force
				UE::MassAvoidance::DebugDrawSummedForce(AgentDebugContext,
					Collider.Location + UE::MassAvoidance::DebugAgentHeightOffset,
					Collider.Location + UE::MassAvoidance::DebugAgentHeightOffset + SeparationForce,
					UE::MassAvoidance::AgentSeparationForceColor); 
				
				if (AvoidForce.Size() > 0.)
				{
					// Draw agent vs agent hit positions
					const FVector HitPosition = AgentLocation + (DesVel * CPA);
					const FVector LeftOffset = PredictiveAvoidanceAgentRadius * UE::MassNavigation::GetLeftDirection(DesVel.GetSafeNormal(), FVector::UpVector);
					UE::MassAvoidance::DebugDrawLine(AgentDebugContext, AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset + LeftOffset, HitPosition + UE::MassAvoidance::DebugAgentHeightOffset + LeftOffset, UE::MassAvoidance::CurrentAgentColor, 1.5f);
					UE::MassAvoidance::DebugDrawLine(AgentDebugContext, AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset - LeftOffset, HitPosition + UE::MassAvoidance::DebugAgentHeightOffset - LeftOffset, UE::MassAvoidance::CurrentAgentColor, 1.5f);
					UE::MassAvoidance::DebugDrawCylinder(AgentDebugContext, HitPosition, HitPosition + UE::MassAvoidance::DebugAgentHeightOffset, static_cast<float>(PredictiveAvoidanceAgentRadius), UE::MassAvoidance::CurrentAgentColor);

					const FVector OtherHitPosition = Collider.Location + (Collider.Velocity * CPA);
					const FVector OtherLeftOffset = Collider.Radius * UE::MassNavigation::GetLeftDirection(Collider.Velocity.GetSafeNormal(), FVector::UpVector);
					const FVector Left = UE::MassAvoidance::DebugAgentHeightOffset + OtherLeftOffset;
					const FVector Right = UE::MassAvoidance::DebugAgentHeightOffset - OtherLeftOffset;
					UE::MassAvoidance::DebugDrawLine(AgentDebugContext, Collider.Location + Left, OtherHitPosition + Left, UE::MassAvoidance::AgentsColor, 1.5f);
					UE::MassAvoidance::DebugDrawLine(AgentDebugContext, Collider.Location + Right, OtherHitPosition + Right, UE::MassAvoidance::AgentsColor, 1.5f);
					UE::MassAvoidance::DebugDrawCylinder(AgentDebugContext, Collider.Location, Collider.Location + UE::MassAvoidance::DebugAgentHeightOffset, static_cast<float>(AgentRadius), UE::MassAvoidance::AgentsColor);
					UE::MassAvoidance::DebugDrawCylinder(AgentDebugContext, OtherHitPosition, OtherHitPosition + UE::MassAvoidance::DebugAgentHeightOffset, static_cast<float>(AgentRadius), UE::MassAvoidance::AgentsColor);

					// Draw agent avoid force
					UE::MassAvoidance::DebugDrawForce(AgentDebugContext,
						OtherHitPosition + UE::MassAvoidance::DebugAgentHeightOffset,
						OtherHitPosition + UE::MassAvoidance::DebugAgentHeightOffset + AvoidForce,
						UE::MassAvoidance::AgentAvoidForceColor);
				}
#endif // WITH_MASSGAMEPLAY_DEBUG
			} // close entities loop

			SteeringForce *= NearStartScaling * NearEndScaling;
			
			Force.Value = UE::MassNavigation::ClampVector(SteeringForce, MaxSteerAccel); // Assume unit mass

#if WITH_MASSGAMEPLAY_DEBUG && UNSAFE_FOR_MT
			const FVector AgentAvoidSteeringForce = SteeringForce - OldSteeringForce;

			// Draw total steering force to separate agents
			UE::MassAvoidance::DebugDrawSummedForce(AgentDebugContext,
				AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset,
				AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset + TotalAgentSeparationForce,
				UE::MassAvoidance::AgentSeparationForceColor);

			// Draw total steering force to avoid agents
			UE::MassAvoidance::DebugDrawSummedForce(AgentDebugContext,
				AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset,
				AgentLocation + UE::MassAvoidance::DebugAgentHeightOffset + AgentAvoidSteeringForce,
				UE::MassAvoidance::AgentAvoidForceColor);

			// Draw final steering force adding to the agent velocity
			UE::MassAvoidance::DebugDrawArrow(BaseDebugContext, 
				AgentLocation + AgentVelocity + UE::MassAvoidance::DebugAgentHeightOffset,
				AgentLocation + AgentVelocity + UE::MassAvoidance::DebugAgentHeightOffset + Force.Value,
				UE::MassAvoidance::FinalSteeringForceColor, UE::MassAvoidance::BigArrowHeadSize, UE::MassAvoidance::BigArrowThickness);
#endif // WITH_MASSGAMEPLAY_DEBUG
		}
	});
}

//----------------------------------------------------------------------//
//  UMassStandingAvoidanceProcessor
//----------------------------------------------------------------------//
UMassStandingAvoidanceProcessor::UMassStandingAvoidanceProcessor()
	: EntityQuery(*this)

{
	bAutoRegisterWithProcessingPhases = true;
	ExecutionFlags = (int32)EProcessorExecutionFlags::All;
	ExecutionOrder.ExecuteInGroup = UE::Mass::ProcessorGroupNames::Avoidance;
	ExecutionOrder.ExecuteAfter.Add(TEXT("MassMovingAvoidanceProcessor"));
}

void UMassStandingAvoidanceProcessor::ConfigureQueries()
{
	EntityQuery.AddRequirement<FMassGhostLocationFragment>(EMassFragmentAccess::ReadWrite);
	EntityQuery.AddRequirement<FMassNavigationEdgesFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
	EntityQuery.AddTagRequirement<FMassMediumLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassLowLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddTagRequirement<FMassOffLODTag>(EMassFragmentPresence::None);
	EntityQuery.AddConstSharedRequirement<FMassStandingAvoidanceParameters>(EMassFragmentPresence::All);
}

void UMassStandingAvoidanceProcessor::Initialize(UObject& Owner)
{
	Super::Initialize(Owner);

	World = Owner.GetWorld();
	NavigationSubsystem = UWorld::GetSubsystem<UMassNavigationSubsystem>(Owner.GetWorld());
}

void UMassStandingAvoidanceProcessor::Execute(FMassEntityManager& EntityManager, FMassExecutionContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(UMassStandingAvoidanceProcessor);

	if (!World || !NavigationSubsystem)
	{
		return;
	}

	// Avoidance while standing
	EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, &EntityManager](FMassExecutionContext& Context)
	{
		const int32 NumEntities = Context.GetNumEntities();
		const float DeltaTime = Context.GetDeltaTimeSeconds();

		const TArrayView<FMassGhostLocationFragment> GhostList = Context.GetMutableFragmentView<FMassGhostLocationFragment>();
		const TConstArrayView<FTransformFragment> LocationList = Context.GetFragmentView<FTransformFragment>();
		const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
		const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();
		const FMassStandingAvoidanceParameters& StandingParams = Context.GetConstSharedFragment<FMassStandingAvoidanceParameters>();

		const FVector::FReal GhostSeparationDistance = StandingParams.GhostSeparationDistance;
		const FVector::FReal GhostSeparationStiffness = StandingParams.GhostSeparationStiffness;

		const FVector::FReal MovingSeparationDistance = StandingParams.GhostSeparationDistance * StandingParams.MovingObstacleAvoidanceScale;
		const FVector::FReal MovingSeparationStiffness = StandingParams.GhostSeparationStiffness * StandingParams.MovingObstacleAvoidanceScale;

		// Arrays used to store close agents
		TArray<FMassNavigationObstacleItem, TFixedAllocator<UE::MassAvoidance::MaxObstacleResults>> CloseEntities;

		struct FSortedObstacle
		{
			FSortedObstacle() = default;
			FSortedObstacle(const FMassEntityHandle InEntity, const FVector InLocation, const FVector InForward, const FVector::FReal InDistSq) : Entity(InEntity), Location(InLocation), Forward(InForward), DistSq(InDistSq) {}
			
			FMassEntityHandle Entity;
			FVector Location = FVector::ZeroVector;
			FVector Forward = FVector::ForwardVector;
			FVector::FReal DistSq = 0.;
		};
		TArray<FSortedObstacle, TFixedAllocator<UE::MassAvoidance::MaxObstacleResults>> ClosestObstacles;

		for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
		{
			// @todo: this check should eventually be part of the query.
			const FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIndex];
			if (MoveTarget.GetCurrentAction() != EMassMovementAction::Stand)
			{
				continue;
			}
			
			FMassGhostLocationFragment& Ghost = GhostList[EntityIndex];
			// Skip if the ghost is not valid for this movement action yet.
			if (Ghost.IsValid(MoveTarget.GetCurrentActionID()) == false)
			{
				continue;
			}

			const FTransformFragment& Location = LocationList[EntityIndex];
			const FAgentRadiusFragment& Radius = RadiusList[EntityIndex];

			FMassEntityHandle Entity = Context.GetEntity(EntityIndex);
			const FVector AgentLocation = Location.GetTransform().GetTranslation();
			const FVector::FReal AgentRadius = Radius.Radius;

			// Steer ghost to move target.
			const FVector::FReal SteerK = 1. / StandingParams.GhostSteeringReactionTime;
			constexpr FVector::FReal SteeringMinDistance = 1.; // Do not bother to steer if the distance is less than this.

			FVector SteerDirection = FVector::ZeroVector;
			FVector Delta = MoveTarget.Center - Ghost.Location;
			Delta.Z = 0.;
			const FVector::FReal Distance = Delta.Size();
			FVector::FReal SpeedFade = 0.;
			if (Distance > SteeringMinDistance)
			{
				SteerDirection = Delta / Distance;
				SpeedFade = FMath::Clamp(Distance / FMath::Max(KINDA_SMALL_NUMBER, StandingParams.GhostStandSlowdownRadius), 0., 1.);
			}

			const FVector GhostDesiredVelocity = SteerDirection * StandingParams.GhostMaxSpeed * SpeedFade;
			FVector GhostSteeringForce = SteerK * (GhostDesiredVelocity - Ghost.Velocity); // Goal force
			
			// Find close obstacles
			// @todo: optimize FindCloseObstacles() and cache results. We're intentionally using agent location here, to allow to share the results with moving avoidance.
			const FNavigationObstacleHashGrid2D& ObstacleGrid = NavigationSubsystem->GetObstacleGridMutable();
			UE::MassAvoidance::FindCloseObstacles(AgentLocation, StandingParams.GhostObstacleDetectionDistance, ObstacleGrid, CloseEntities, UE::MassAvoidance::MaxObstacleResults);

			// Remove unwanted and find the closest in the CloseEntities
			const FVector::FReal DistanceCutOffSqr = FMath::Square(StandingParams.GhostObstacleDetectionDistance);
			ClosestObstacles.Reset();
			for (const FNavigationObstacleHashGrid2D::ItemIDType OtherEntity : CloseEntities)
			{
				// Skip self
				if (OtherEntity.Entity == Entity)
				{
					continue;
				}

				// Skip invalid entities.
				if (!EntityManager.IsEntityValid(OtherEntity.Entity))
				{
					UE_LOG(LogAvoidanceObstacles, VeryVerbose, TEXT("Close entity is invalid, skipped."));
					continue;
				}

				// Skip too far
				const FTransformFragment& OtherTransform = EntityManager.GetFragmentDataChecked<FTransformFragment>(OtherEntity.Entity);
				const FVector OtherLocation = OtherTransform.GetTransform().GetLocation();
				const FVector::FReal DistSq = FVector::DistSquared(AgentLocation, OtherLocation);
				if (DistSq > DistanceCutOffSqr)
				{
					continue;
				}

				ClosestObstacles.Emplace(OtherEntity.Entity, OtherLocation, OtherTransform.GetTransform().GetRotation().GetForwardVector(), DistSq);
			}
			ClosestObstacles.Sort([](const FSortedObstacle& A, const FSortedObstacle& B) { return A.DistSq < B.DistSq; });

			const FVector::FReal GhostRadius = AgentRadius * StandingParams.GhostSeparationRadiusScale;
			
			// Compute forces
			constexpr int32 MaxCloseObstacleTreated = 6;
			const int32 NumCloseObstacles = FMath::Min(ClosestObstacles.Num(), MaxCloseObstacleTreated);
			for (int32 Index = 0; Index < NumCloseObstacles; Index++)
			{
				FSortedObstacle& OtherAgent = ClosestObstacles[Index];
				FMassEntityView OtherEntityView(EntityManager, OtherAgent.Entity);

				const FVector::FReal OtherRadius = OtherEntityView.GetFragmentData<FAgentRadiusFragment>().Radius;
				const FVector::FReal TotalRadius = GhostRadius + OtherRadius;

				// @todo: this is heavy fragment to access, see if we could handle this differently.
				const FMassMoveTargetFragment* OtherMoveTarget = OtherEntityView.GetFragmentDataPtr<FMassMoveTargetFragment>();
				const FMassGhostLocationFragment* OtherGhost = OtherEntityView.GetFragmentDataPtr<FMassGhostLocationFragment>();

				const bool bOtherHasGhost = OtherMoveTarget != nullptr && OtherGhost != nullptr
											&& OtherMoveTarget->GetCurrentAction() == EMassMovementAction::Stand
											&& OtherGhost->IsValid(OtherMoveTarget->GetCurrentActionID());

				// If other has ghost active, avoid that, else avoid the actual agent.
				if (bOtherHasGhost)
				{
					// Avoid the other agent more, when it is further away from it's goal location.
					const FVector::FReal OtherDistanceToGoal = FVector::Distance(OtherGhost->Location, OtherMoveTarget->Center);
					const FVector::FReal OtherSteerFade = FMath::Clamp(OtherDistanceToGoal / StandingParams.GhostToTargetMaxDeviation, 0., 1.);
					const FVector::FReal SeparationStiffness = FMath::Lerp(GhostSeparationStiffness, MovingSeparationStiffness, OtherSteerFade);

					// Ghost separation
					FVector RelPos = Ghost.Location - OtherGhost->Location;
					RelPos.Z = 0.; // we assume we work on a flat plane for now
					const FVector::FReal ConDist = RelPos.Size();
					const FVector ConNorm = ConDist > 0. ? RelPos / ConDist : FVector::ForwardVector;

					// Separation force (stay away from obstacles if possible)
					const FVector::FReal PenSep = (TotalRadius + GhostSeparationDistance) - ConDist;
					const FVector::FReal SeparationMag = UE::MassNavigation::Smooth(FMath::Clamp(PenSep / GhostSeparationDistance, 0., 1.));
					const FVector SeparationForce = ConNorm * SeparationStiffness * SeparationMag;

					GhostSteeringForce += SeparationForce;
				}
				else
				{
					// Avoid more when the avoidance other is in front,
					const FVector DirToOther = (OtherAgent.Location - Ghost.Location).GetSafeNormal();
					const FVector::FReal DirectionalFade = FMath::Square(FMath::Max(0., FVector::DotProduct(MoveTarget.Forward, DirToOther)));
					const FVector::FReal DirectionScale = FMath::Lerp(StandingParams.MovingObstacleDirectionalScale, 1., DirectionalFade);

					// Treat the other agent as a 2D capsule protruding towards forward.
 					const FVector OtherBasePosition = OtherAgent.Location;
					const FVector OtherPersonalSpacePosition = OtherAgent.Location + OtherAgent.Forward * OtherRadius * StandingParams.MovingObstaclePersonalSpaceScale * DirectionScale;
					const FVector OtherLocation = FMath::ClosestPointOnSegment(Ghost.Location, OtherBasePosition, OtherPersonalSpacePosition);

					FVector RelPos = Ghost.Location - OtherLocation;
					RelPos.Z = 0.;
					const FVector::FReal ConDist = RelPos.Size();
					const FVector ConNorm = ConDist > 0. ? RelPos / ConDist : FVector::ForwardVector;

					// Separation force (stay away from obstacles if possible)
					const FVector::FReal PenSep = (TotalRadius + MovingSeparationDistance) - ConDist;
					const FVector::FReal SeparationMag = UE::MassNavigation::Smooth(FMath::Clamp(PenSep / MovingSeparationDistance, 0., 1.));
					const FVector SeparationForce = ConNorm * MovingSeparationStiffness * SeparationMag;

					GhostSteeringForce += SeparationForce;
				}
			}

			GhostSteeringForce.Z = 0.;
			GhostSteeringForce = UE::MassNavigation::ClampVector(GhostSteeringForce, StandingParams.GhostMaxAcceleration); // Assume unit mass
			Ghost.Velocity += GhostSteeringForce * DeltaTime;
			Ghost.Velocity.Z = 0.;
			
			// Damping
			FMath::ExponentialSmoothingApprox(Ghost.Velocity, FVector::ZeroVector, DeltaTime, StandingParams.GhostVelocityDampingTime);
			
			Ghost.Location += Ghost.Velocity * DeltaTime;

			// Dont let the ghost location too far from move target center.
			const FVector DirToCenter = Ghost.Location - MoveTarget.Center;
			const FVector::FReal DistToCenter = DirToCenter.Length();
			if (DistToCenter > StandingParams.GhostToTargetMaxDeviation)
			{
				Ghost.Location = MoveTarget.Center + DirToCenter * (StandingParams.GhostToTargetMaxDeviation / DistToCenter);
			}
		}
	});
	
}

#undef UNSAFE_FOR_MT
