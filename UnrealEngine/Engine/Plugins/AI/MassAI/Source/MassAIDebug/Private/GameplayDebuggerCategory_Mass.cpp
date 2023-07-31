// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayDebuggerCategory_Mass.h"

#if WITH_GAMEPLAY_DEBUGGER && WITH_MASSGAMEPLAY_DEBUG
#include "MassGameplayDebugTypes.h"
#include "MassEntityView.h"
#include "GameplayDebuggerConfig.h"
#include "GameplayDebuggerCategoryReplicator.h"
#include "MassDebuggerSubsystem.h"
#include "MassSignalSubsystem.h"
#include "MassActorSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "MassAgentComponent.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "MassNavigationFragments.h"
#include "Steering/MassSteeringFragments.h"
#include "MassLookAtFragments.h"
#include "MassStateTreeFragments.h"
#include "MassStateTreeExecutionContext.h"
#include "MassZoneGraphNavigationFragments.h"
#include "MassSmartObjectFragments.h"
#include "SmartObjectSubsystem.h"
#include "Util/ColorConstants.h"
#include "MassSimulationLOD.h"
#include "CanvasItem.h"
#include "Engine/World.h"
#include "MassDebugger.h"

namespace UE::Mass::Debug
{
	FMassEntityHandle GetEntityFromActor(const AActor& Actor, const UMassAgentComponent*& OutMassAgentComponent)
	{
		FMassEntityHandle EntityHandle;
		if (const UMassAgentComponent* AgentComp = Actor.FindComponentByClass<UMassAgentComponent>())
		{
			EntityHandle = AgentComp->GetEntityHandle();
			OutMassAgentComponent = AgentComp;
		}
		else if (UMassActorSubsystem* ActorSubsystem = UWorld::GetSubsystem<UMassActorSubsystem>(Actor.GetWorld()))
		{
			EntityHandle = ActorSubsystem->GetEntityHandleFromActor(&Actor);
		}
		return EntityHandle;
	};

	FMassEntityHandle GetBestEntity(const FVector ViewLocation, const FVector ViewDirection, const TConstArrayView<FMassEntityHandle> Entities, const TConstArrayView<FVector> Locations, const bool bLimitAngle)
	{
		// Reusing similar algorithm as UGameplayDebuggerLocalController for now 
		constexpr float MaxScanDistanceSq = 25000.0f * 25000.0f;
		constexpr float MinViewDirDot = 0.707f; // 45 degrees

		checkf(Entities.Num() == Locations.Num(), TEXT("Both Entities and Locations lists are expected to be of the same size: %d vs %d"), Entities.Num(), Locations.Num());
		
		float BestScore = bLimitAngle ? MinViewDirDot : (-1.f - KINDA_SMALL_NUMBER);	
		FMassEntityHandle BestEntity;

		for (int i = 0; i < Entities.Num(); ++i)
		{
			if (Entities[i].IsSet() == false)
			{
				continue;
			}
			
			const FVector DirToEntity = (Locations[i] - ViewLocation);
			const float DistToEntitySq = DirToEntity.SizeSquared();
			if (DistToEntitySq > MaxScanDistanceSq)
			{
				continue;
			}

			const FVector DirToEntityNormal = (FMath::IsNearlyZero(DistToEntitySq)) ? ViewDirection : (DirToEntity / FMath::Sqrt(DistToEntitySq));
			const float ViewDot = FVector::DotProduct(ViewDirection, DirToEntityNormal);
			if (ViewDot > BestScore)
			{
				BestScore = ViewDot;
				BestEntity = Entities[i];
			}
		}

		return BestEntity;
	}
} // namespace UE::Mass:Debug

//----------------------------------------------------------------------//
//  FGameplayDebuggerCategory_Mass
//----------------------------------------------------------------------//
FGameplayDebuggerCategory_Mass::FGameplayDebuggerCategory_Mass()
{
	CachedDebugActor = nullptr;
	bShowOnlyWithDebugActor = false;

	// @todo would be nice to have these saved in per-user settings 
	bShowArchetypes = false;
	bShowShapes = false;
	bShowAgentFragments = false;
	bPickEntity = false;
	bShowEntityDetails = false;
	bShowNearEntityOverview = true;
	bShowNearEntityAvoidance = false;
	bShowNearEntityPath = false;
	bMarkEntityBeingDebugged = true;

	BindKeyPress(EKeys::A.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Mass::OnToggleArchetypes, EGameplayDebuggerInputMode::Replicated);
	BindKeyPress(EKeys::S.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Mass::OnToggleShapes, EGameplayDebuggerInputMode::Replicated);
	BindKeyPress(EKeys::G.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Mass::OnToggleAgentFragments, EGameplayDebuggerInputMode::Replicated);
	BindKeyPress(EKeys::P.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Mass::OnPickEntity, EGameplayDebuggerInputMode::Replicated);
	BindKeyPress(EKeys::D.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Mass::OnToggleEntityDetails, EGameplayDebuggerInputMode::Replicated);
	BindKeyPress(EKeys::O.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Mass::OnToggleNearEntityOverview, EGameplayDebuggerInputMode::Replicated);
	BindKeyPress(EKeys::V.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Mass::OnToggleNearEntityAvoidance, EGameplayDebuggerInputMode::Replicated);
	BindKeyPress(EKeys::C.GetFName(), FGameplayDebuggerInputModifier::Shift, this, &FGameplayDebuggerCategory_Mass::OnToggleNearEntityPath, EGameplayDebuggerInputMode::Replicated);
}

void FGameplayDebuggerCategory_Mass::SetCachedEntity(const FMassEntityHandle Entity, const FMassEntityManager& EntityManager)
{
	CachedEntity = Entity;
	FMassDebugger::SelectEntity(EntityManager, Entity);
}

void FGameplayDebuggerCategory_Mass::PickEntity(const APlayerController& OwnerPC, const UWorld& World, FMassEntityManager& EntityManager, const bool bLimitAngle)
{
	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	ensureMsgf(GetViewPoint(&OwnerPC, ViewLocation, ViewDirection), TEXT("GetViewPoint is expected to always succeed when passing a valid controller."));

	FMassEntityHandle BestEntity;
	// entities indicated by UE::Mass::Debug take precedence 
    if (UE::Mass::Debug::HasDebugEntities())
    {
		TArray<FMassEntityHandle> Entities;
	    TArray<FVector> Locations;
	    UE::Mass::Debug::GetDebugEntitiesAndLocations(EntityManager, Entities, Locations);
	    BestEntity = UE::Mass::Debug::GetBestEntity(ViewLocation, ViewDirection, Entities, Locations, bLimitAngle);
    }
	else
	{
		TArray<FMassEntityHandle> Entities;
		TArray<FVector> Locations;
		FMassExecutionContext ExecutionContext;
		FMassEntityQuery Query;
		Query.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		Query.ForEachEntityChunk(EntityManager, ExecutionContext, [&Entities, &Locations](FMassExecutionContext& Context)
		{
			Entities.Append(Context.GetEntities().GetData(), Context.GetEntities().Num());
			TConstArrayView<FTransformFragment> InLocations = Context.GetFragmentView<FTransformFragment>();
			Locations.Reserve(Locations.Num() + InLocations.Num());
			for (const FTransformFragment& TransformFragment : InLocations)
			{
				Locations.Add(TransformFragment.GetTransform().GetLocation());
			}
		});

		BestEntity = UE::Mass::Debug::GetBestEntity(ViewLocation, ViewDirection, Entities, Locations, bLimitAngle);
	}

	AActor* BestActor = nullptr;
	if (BestEntity.IsSet())
	{
		if (const UMassActorSubsystem* ActorSubsystem = World.GetSubsystem<UMassActorSubsystem>())
		{
			BestActor = ActorSubsystem->GetActorFromHandle(FMassEntityHandle(BestEntity));
		}
	}

	SetCachedEntity(BestEntity, EntityManager);
	CachedDebugActor = BestActor;
	GetReplicator()->SetDebugActor(BestActor);
}

TSharedRef<FGameplayDebuggerCategory> FGameplayDebuggerCategory_Mass::MakeInstance()
{
	return MakeShareable(new FGameplayDebuggerCategory_Mass());
}

void FGameplayDebuggerCategory_Mass::CollectData(APlayerController* OwnerPC, AActor* DebugActor)
{
	UWorld* World = GetDataWorld(OwnerPC, DebugActor);
	check(World);

	UMassEntitySubsystem* EntitySubsystem = UWorld::GetSubsystem<UMassEntitySubsystem>(World);
	if (EntitySubsystem == nullptr)
	{
		AddTextLine(FString::Printf(TEXT("{Red}EntitySubsystem instance is missing")));
		return;
	}
	FMassEntityManager& EntityManager = EntitySubsystem->GetMutableEntityManager();
	UMassDebuggerSubsystem* Debugger = World->GetSubsystem<UMassDebuggerSubsystem>();

	const UMassAgentComponent* AgentComp = nullptr;
	if (DebugActor)
	{
		const FMassEntityHandle EntityHandle = UE::Mass::Debug::GetEntityFromActor(*DebugActor, AgentComp);	
		SetCachedEntity(EntityHandle, EntityManager);
		CachedDebugActor = DebugActor;
	}
	else if (CachedDebugActor)
	{
		SetCachedEntity(FMassEntityHandle(), EntityManager);
		CachedDebugActor = nullptr;
	}

	if (OwnerPC)
	{
		// Ideally we would have a way to register in the main picking flow but that would require more changes to
		// also support client-server picking. For now, we handle explicit mass picking requests on the authority
		if (bPickEntity)
		{
			PickEntity(*OwnerPC, *World, EntityManager);
			bPickEntity = false;
		}
		// if we're debugging based on UE::Mass::Debug and the range changed
		else if (CachedDebugActor == nullptr && UE::Mass::Debug::HasDebugEntities() && UE::Mass::Debug::IsDebuggingEntity(CachedEntity) == false)
		{
			// using bLimitAngle = false to not limit the selection to only the things in from of the player
			PickEntity(*OwnerPC, *World, EntityManager, /*bLimitAngle=*/false);
		}
	}

	AddTextLine(FString::Printf(TEXT("{Green}Entities count active{grey}/all: {white}%d{grey}/%d"), EntityManager.DebugGetEntityCount(), EntityManager.DebugGetEntityCount()));
	AddTextLine(FString::Printf(TEXT("{Green}Registered Archetypes count: {white}%d {green}data ver: {white}%d"), EntityManager.DebugGetArchetypesCount(), EntityManager.GetArchetypeDataVersion()));

	if (UE::Mass::Debug::HasDebugEntities())
	{
		int32 RangeBegin, RangeEnd;
		UE::Mass::Debug::GetDebugEntitiesRange(RangeBegin, RangeEnd);
		AddTextLine(FString::Printf(TEXT("{Green}Debugged entity range: {orange}%d-%d"), RangeBegin, RangeEnd));
	}

	if (bShowArchetypes)
	{
		FStringOutputDevice Ar;
		Ar.SetAutoEmitLineTerminator(true);
		EntityManager.DebugPrintArchetypes(Ar, /*bIncludeEmpty*/false);

		AddTextLine(Ar);
	}

	if (CachedEntity.IsSet() && bMarkEntityBeingDebugged)
	{
		if (const FTransformFragment* TransformFragment = EntityManager.GetFragmentDataPtr<FTransformFragment>(CachedEntity))
		{
			const FVector Location = TransformFragment->GetTransform().GetLocation();
			AddShape(FGameplayDebuggerShape::MakeBox(Location, FVector(8,8,500), FColor::Purple,  FString::Printf(TEXT("[%s]"), *CachedEntity.DebugGetDescription())));
			AddShape(FGameplayDebuggerShape::MakePoint(Location, 10, FColor::Purple));
		}
	}

	if (CachedEntity.IsSet() && Debugger)
	{
		AddTextLine(Debugger->GetSelectedEntityInfo());
	}

	//@todo could shave off some perf cost if UMassDebuggerSubsystem used FGameplayDebuggerShape directly
	if (bShowShapes && Debugger)
	{
		const TArray<UMassDebuggerSubsystem::FShapeDesc>* Shapes = Debugger->GetShapes();
		check(Shapes);
		// EMassEntityDebugShape::Box
		for (const UMassDebuggerSubsystem::FShapeDesc& Desc : Shapes[uint8(EMassEntityDebugShape::Box)])
		{
			AddShape(FGameplayDebuggerShape::MakeBox(Desc.Location, FVector(Desc.Size), FColor::Blue));
		}
		// EMassEntityDebugShape::Cone
		// note that we're modifying the Size here because MakeCone is using the third param as Cone's "height", while all mass debugger shapes are created with agent radius
		// FGameplayDebuggerShape::Draw is using 0.25 rad for cone angle, so that's what we'll use here
		static const float Tan025Rad = FMath::Tan(0.25f);
		for (const UMassDebuggerSubsystem::FShapeDesc& Desc : Shapes[uint8(EMassEntityDebugShape::Cone)])
		{
			AddShape(FGameplayDebuggerShape::MakeCone(Desc.Location, FVector::UpVector, Desc.Size / Tan025Rad, FColor::Orange));
		}
		// EMassEntityDebugShape::Cylinder
		for (const UMassDebuggerSubsystem::FShapeDesc& Desc : Shapes[uint8(EMassEntityDebugShape::Cylinder)])
		{
			AddShape(FGameplayDebuggerShape::MakeCylinder(Desc.Location, Desc.Size, Desc.Size * 2, FColor::Yellow));
		}
		// EMassEntityDebugShape::Capsule
		for (const UMassDebuggerSubsystem::FShapeDesc& Desc : Shapes[uint8(EMassEntityDebugShape::Capsule)])
		{
			AddShape(FGameplayDebuggerShape::MakeCapsule(Desc.Location, Desc.Size, Desc.Size * 2, FColor::Green));
		}
	}

	if (bShowAgentFragments)
	{
		if (CachedEntity.IsSet())
		{
			// CachedEntity can become invalid if the entity "dies" or in editor mode when related actor gets moved 
			// (which causes the MassAgentComponent destruction and recreation).
			if (EntityManager.IsEntityActive(CachedEntity))
			{
				AddTextLine(FString::Printf(TEXT("{Green}Entity: {White}%s"), *CachedEntity.DebugGetDescription()));
				AddTextLine(FString::Printf(TEXT("{Green}Type: {White}%s"), (AgentComp == nullptr) ? TEXT("N/A") : AgentComp->IsPuppet() ? TEXT("PUPPET") : TEXT("AGENT")));

				if (bShowEntityDetails)
				{
					FStringOutputDevice FragmentsDesc;
					FragmentsDesc.SetAutoEmitLineTerminator(true);
					const TCHAR* PrefixToRemove = TEXT("DataFragment_");
					FMassDebugger::OutputEntityDescription(FragmentsDesc, EntityManager, CachedEntity, PrefixToRemove);
					AddTextLine(FString::Printf(TEXT("{Green}Fragments:\n{White}%s"), *FragmentsDesc));
				}
				else
				{
					const FMassArchetypeHandle Archetype = EntityManager.GetArchetypeForEntity(CachedEntity);
					const FMassArchetypeCompositionDescriptor& Composition = EntityManager.GetArchetypeComposition(Archetype);
					
					auto DescriptionBuilder = [](const TArray<FName>& ItemNames) -> FString {
						constexpr int ColumnsCount = 2;
						FString Description;
						int i = 0;
						for (const FName Name : ItemNames)
						{
							if ((i++ % ColumnsCount) == 0)
							{
								Description += TEXT("\n");
							}
							Description += FString::Printf(TEXT("%s,\t"), *Name.ToString());
						}
						return Description;
					};

					TArray<FName> ItemNames;
					Composition.Tags.DebugGetIndividualNames(ItemNames);
					AddTextLine(FString::Printf(TEXT("{Green}Tags:{White}%s"), *DescriptionBuilder(ItemNames)));
					
					ItemNames.Reset();
					Composition.Fragments.DebugGetIndividualNames(ItemNames);
					AddTextLine(FString::Printf(TEXT("{Green}Fragments:{White}%s"), *DescriptionBuilder(ItemNames)));
					
					ItemNames.Reset();
					Composition.ChunkFragments.DebugGetIndividualNames(ItemNames);
					AddTextLine(FString::Printf(TEXT("{Green}Chunk Fragments:{White}%s"), *DescriptionBuilder(ItemNames)));

					ItemNames.Reset();
					Composition.SharedFragments.DebugGetIndividualNames(ItemNames);
					AddTextLine(FString::Printf(TEXT("{Green}Shared Fragments:{White}%s"), *DescriptionBuilder(ItemNames)));
				}

				const FTransformFragment& TransformFragment = EntityManager.GetFragmentDataChecked<FTransformFragment>(CachedEntity);
				constexpr float CapsuleRadius = 50.f;
				AddShape(FGameplayDebuggerShape::MakeCapsule(TransformFragment.GetTransform().GetLocation() + 2.f * CapsuleRadius * FVector::UpVector, CapsuleRadius, CapsuleRadius * 2.f, FColor::Orange));
			}
			else
			{
				CachedEntity.Reset();
			}
		}
		else
		{
			AddTextLine(FString::Printf(TEXT("{Green}Entity: {Red}INACTIVE")));
		}
	}

	NearEntityDescriptions.Reset();
	if (bShowNearEntityOverview && OwnerPC)
	{
		FVector ViewLocation = FVector::ZeroVector;
		FVector ViewDirection = FVector::ForwardVector;
		ensureMsgf(GetViewPoint(OwnerPC, ViewLocation, ViewDirection), TEXT("GetViewPoint is expected to always succeed when passing a valid controller."));

		FMassEntityQuery EntityQuery;
		EntityQuery.AddRequirement<FMassStateTreeInstanceFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddConstSharedRequirement<FMassStateTreeSharedFragment>();
		EntityQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FAgentRadiusFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FMassSteeringFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FMassStandingSteeringFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FMassGhostLocationFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FMassVelocityFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FMassForceFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FMassMoveTargetFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FMassLookAtFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
		EntityQuery.AddRequirement<FMassSimulationLODFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);
		EntityQuery.AddRequirement<FMassZoneGraphShortPathFragment>(EMassFragmentAccess::ReadOnly);
		EntityQuery.AddRequirement<FMassSmartObjectUserFragment>(EMassFragmentAccess::ReadOnly, EMassFragmentPresence::Optional);

		const float CurrentTime = World->GetTimeSeconds();
		
		UMassStateTreeSubsystem* MassStateTreeSubsystem = World->GetSubsystem<UMassStateTreeSubsystem>();
		UMassSignalSubsystem* SignalSubsystem = World->GetSubsystem<UMassSignalSubsystem>();
		USmartObjectSubsystem* SmartObjectSubsystem = World->GetSubsystem<USmartObjectSubsystem>();
		
		if (MassStateTreeSubsystem && SignalSubsystem && SmartObjectSubsystem)
		{
			FMassExecutionContext Context(EntityManager.AsShared(), 0.0f);
		
			EntityQuery.ForEachEntityChunk(EntityManager, Context, [this, MassStateTreeSubsystem, SignalSubsystem, SmartObjectSubsystem, OwnerPC, ViewLocation, ViewDirection, CurrentTime](FMassExecutionContext& Context)
			{
				FMassEntityManager& EntityManager = Context.GetEntityManagerChecked();

				const int32 NumEntities = Context.GetNumEntities();
				const TConstArrayView<FMassStateTreeInstanceFragment> StateTreeInstanceList = Context.GetFragmentView<FMassStateTreeInstanceFragment>();
				const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
				const TConstArrayView<FAgentRadiusFragment> RadiusList = Context.GetFragmentView<FAgentRadiusFragment>();
				const TConstArrayView<FMassSteeringFragment> SteeringList = Context.GetFragmentView<FMassSteeringFragment>();
				const TConstArrayView<FMassStandingSteeringFragment> StandingSteeringList = Context.GetFragmentView<FMassStandingSteeringFragment>();
				const TConstArrayView<FMassGhostLocationFragment> GhostList = Context.GetFragmentView<FMassGhostLocationFragment>();
				const TConstArrayView<FMassVelocityFragment> VelocityList = Context.GetFragmentView<FMassVelocityFragment>();
				const TConstArrayView<FMassForceFragment> ForceList = Context.GetFragmentView<FMassForceFragment>();
				const TConstArrayView<FMassMoveTargetFragment> MoveTargetList = Context.GetFragmentView<FMassMoveTargetFragment>();
				const TConstArrayView<FMassLookAtFragment> LookAtList = Context.GetFragmentView<FMassLookAtFragment>();
				const TConstArrayView<FMassSimulationLODFragment> SimLODList = Context.GetFragmentView<FMassSimulationLODFragment>();
				const TConstArrayView<FMassZoneGraphShortPathFragment> ShortPathList = Context.GetFragmentView<FMassZoneGraphShortPathFragment>();
				const TConstArrayView<FMassSmartObjectUserFragment> SOUserList = Context.GetFragmentView<FMassSmartObjectUserFragment>();
				const FMassStateTreeSharedFragment& SharedStateTree = Context.GetConstSharedFragment<FMassStateTreeSharedFragment>();

				const bool bHasLOD = (SimLODList.Num() > 0);
				const bool bHasLookAt = (LookAtList.Num() > 0);
				const bool bHasSOUser = (SOUserList.Num() > 0);

				const UGameplayDebuggerUserSettings* Settings = GetDefault<UGameplayDebuggerUserSettings>();
				const float MaxViewDistance = Settings->MaxViewDistance;
				const float MinViewDirDot = FMath::Cos(FMath::DegreesToRadians(Settings->MaxViewAngle));

				const UStateTree* StateTree = SharedStateTree.StateTree;

				for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
				{
					const FTransformFragment& Transform = TransformList[EntityIndex];
					const FVector EntityLocation = Transform.GetTransform().GetLocation();
					
					// Cull entities
					const FVector DirToEntity = EntityLocation - ViewLocation;
					const float DistanceToEntitySq = DirToEntity.SquaredLength();
					if (DistanceToEntitySq > FMath::Square(MaxViewDistance))
					{
						continue;
					}
					const float ViewDot = FVector::DotProduct(DirToEntity.GetSafeNormal(), ViewDirection);
					if (ViewDot < MinViewDirDot)
					{
						continue;
					}

					const FAgentRadiusFragment& Radius = RadiusList[EntityIndex];
					const FMassSteeringFragment& Steering = SteeringList[EntityIndex];
					const FMassStandingSteeringFragment& StandingSteering = StandingSteeringList[EntityIndex];
					const FMassGhostLocationFragment& Ghost = GhostList[EntityIndex];
					const FMassVelocityFragment& Velocity = VelocityList[EntityIndex];
					const FMassForceFragment& Force = ForceList[EntityIndex];
					const FMassMoveTargetFragment& MoveTarget = MoveTargetList[EntityIndex];
					const FMassSimulationLODFragment& SimLOD = bHasLOD ? SimLODList[EntityIndex] : FMassSimulationLODFragment();
					const FMassZoneGraphShortPathFragment& ShortPath = ShortPathList[EntityIndex];
					const FMassStateTreeInstanceFragment& StateTreeInstance = StateTreeInstanceList[EntityIndex];

					const FVector EntityForward = Transform.GetTransform().GetRotation().GetForwardVector();

					constexpr float EyeHeight = 160.0f; // @todo: add eye height to agent.

					// Draw entity position and orientation.
					FVector BasePos = EntityLocation + FVector(0.0f ,0.0f ,25.0f );

					AddShape(FGameplayDebuggerShape::MakeCircle(BasePos, FVector::UpVector, Radius.Radius, FColor::White));
					AddShape(FGameplayDebuggerShape::MakeSegment(BasePos, BasePos + EntityForward * Radius.Radius * 1.25f, FColor::White));

					// Velocity and steering target
					BasePos += FVector(0.0f ,0.0f ,5.0f );
					AddShape(FGameplayDebuggerShape::MakeArrow(BasePos, BasePos + Velocity.Value, 10.0f, 2.0f, FColor::Yellow));
					BasePos += FVector(0.0f ,0.0f ,5.0f );
					AddShape(FGameplayDebuggerShape::MakeArrow(BasePos, BasePos + Steering.DesiredVelocity, 10.0f, 1.0f, FColorList::Pink));

					// Move target
					const FVector MoveBasePos = MoveTarget.Center + FVector(0,0,5);
					AddShape(FGameplayDebuggerShape::MakeArrow(MoveBasePos - MoveTarget.Forward * Radius.Radius, MoveBasePos + MoveTarget.Forward * Radius.Radius, 10.0f, 2.0f, FColorList::MediumVioletRed));

					// Look at
					constexpr float LookArrowLength = 100.0f;
					BasePos = EntityLocation + FVector(0,0,EyeHeight);

					if (bHasLookAt)
					{
						const FMassLookAtFragment& LookAt = LookAtList[EntityIndex];
						const FVector WorldLookDirection = Transform.GetTransform().TransformVector(LookAt.Direction);
						bool bLookArrowDrawn = false;
						if (LookAt.LookAtMode == EMassLookAtMode::LookAtEntity && EntityManager.IsEntityValid(LookAt.TrackedEntity))
						{
							if (const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(LookAt.TrackedEntity))
							{
								FVector TargetPosition = TargetTransform->GetTransform().GetLocation();
								TargetPosition.Z = BasePos.Z;
								AddShape(FGameplayDebuggerShape::MakeCircle(TargetPosition, FVector::UpVector, Radius.Radius, FColor::Red));

								const float TargetDistance = FMath::Max(LookArrowLength, FVector::DotProduct(WorldLookDirection, TargetPosition - BasePos));
								AddShape(FGameplayDebuggerShape::MakeSegment(BasePos, BasePos + WorldLookDirection * TargetDistance, FColorList::LightGrey));
								bLookArrowDrawn = true;
							}
						}

						if (LookAt.bRandomGazeEntities && EntityManager.IsEntityValid(LookAt.GazeTrackedEntity))
						{
							if (const FTransformFragment* TargetTransform = EntityManager.GetFragmentDataPtr<FTransformFragment>(LookAt.GazeTrackedEntity))
							{
								FVector TargetPosition = TargetTransform->GetTransform().GetLocation();
								TargetPosition.Z = BasePos.Z;
								AddShape(FGameplayDebuggerShape::MakeCircle(TargetPosition, FVector::UpVector, Radius.Radius, FColor::Turquoise));
							}
						}

						if (!bLookArrowDrawn)
						{
							AddShape(FGameplayDebuggerShape::MakeArrow(BasePos, BasePos + WorldLookDirection * LookArrowLength, 10.0f, 1.0f, FColor::Turquoise));
						}
					}

					// SmartObject
					if (bHasSOUser)
					{
						const FMassSmartObjectUserFragment& SOUser = SOUserList[EntityIndex];
						if (SOUser.InteractionHandle.IsValid())
						{
							const FVector ZOffset = FVector(0.0f , 0.0f , 25.0f );
							const FTransform SlotTransform = SmartObjectSubsystem->GetSlotTransform(SOUser.InteractionHandle).Get(FTransform::Identity);
							const FVector SlotLocation = SlotTransform.GetLocation();
							AddShape(FGameplayDebuggerShape::MakeSegment(EntityLocation + ZOffset, SlotLocation + ZOffset, 3.0f, FColorList::Orange));
						}
					}

					// Path
					if (bShowNearEntityPath)
					{
						const FVector ZOffset = FVector(0.0f , 0.0f , 25.0f );
						for (uint8 PointIndex = 0; PointIndex < ShortPath.NumPoints - 1; PointIndex++)
						{
							const FMassZoneGraphPathPoint& CurrPoint = ShortPath.Points[PointIndex];
							const FMassZoneGraphPathPoint& NextPoint = ShortPath.Points[PointIndex + 1];
							AddShape(FGameplayDebuggerShape::MakeSegment(CurrPoint.Position + ZOffset, NextPoint.Position + ZOffset, 3.0f, FColorList::Grey));
						}
					
						for (uint8 PointIndex = 0; PointIndex < ShortPath.NumPoints; PointIndex++)
						{
							const FMassZoneGraphPathPoint& CurrPoint = ShortPath.Points[PointIndex];
							const FVector CurrBase = CurrPoint.Position + ZOffset;
							// Lane tangents
							AddShape(FGameplayDebuggerShape::MakeSegment(CurrBase, CurrBase + CurrPoint.Tangent.GetVector() * 50.0f, 1.0f, FColorList::LightGrey));
						}
					}
					
					if (bShowNearEntityAvoidance)
					{
						// Standing avoidance.
						if (Ghost.IsValid(MoveTarget.GetCurrentActionID()))
						{
							FVector GhostBasePos = Ghost.Location + FVector(0.0f ,0.0f ,25.0f );
							AddShape(FGameplayDebuggerShape::MakeCircle(GhostBasePos, FVector::UpVector, Radius.Radius, FColorList::LightGrey));
							GhostBasePos += FVector(0,0,5);
							AddShape(FGameplayDebuggerShape::MakeArrow(GhostBasePos, GhostBasePos + Ghost.Velocity, 10.0f, 2.0f, FColorList::LightGrey));

							const FVector GhostTargetBasePos = StandingSteering.TargetLocation + FVector(0.0f ,0.0f ,25.0f );
							AddShape(FGameplayDebuggerShape::MakeCircle(GhostTargetBasePos, FVector::UpVector, Radius.Radius * 0.75f, FColorList::Orange));
						}
					}
					
					// Status
					if (DistanceToEntitySq < FMath::Square(MaxViewDistance * 0.5f))
					{
						FString Status;

						// Entity name
						FMassEntityHandle Entity = Context.GetEntity(EntityIndex);
						Status += TEXT("{orange}");
						Status += Entity.DebugGetDescription();
						Status += TEXT(" {white}LOD ");
						switch (SimLOD.LOD) {
						case EMassLOD::High:
							Status += TEXT("High");
							break;
						case EMassLOD::Medium:
							Status += TEXT("Med");
							break;
						case EMassLOD::Low:
							Status += TEXT("Low");
							break;
						case EMassLOD::Off:
							Status += TEXT("Off");
							break;
						default:
							Status += TEXT("?");
							break;
						}
						Status += TEXT("\n");
						
						// Current StateTree task
						if (StateTree != nullptr)
						{
							if (FStateTreeInstanceData* InstanceData = MassStateTreeSubsystem->GetInstanceData(StateTreeInstance.InstanceHandle))
							{
								FMassStateTreeExecutionContext StateTreeContext(*OwnerPC, *StateTree, *InstanceData, EntityManager, *SignalSubsystem, Context);
								StateTreeContext.SetEntity(Entity);

								Status += StateTreeContext.GetActiveStateName();
								Status += FString::Printf(TEXT("  {yellow}%d{white}\n"), StateTreeContext.GetStateChangeCount());
							}
							else
							{
								Status += TEXT("{red}<No StateTree instance>{white}\n");
							}
						}

						// Movement info
						Status += FString::Printf(TEXT("{yellow}%s/%03d {lightgrey}Speed:{white}%.1f {lightgrey}Force:{white}%.1f\n"),
							*UEnum::GetDisplayValueAsText(MoveTarget.GetCurrentAction()).ToString(), MoveTarget.GetCurrentActionID(), Velocity.Value.Length(), Force.Value.Length());
						Status += FString::Printf(TEXT("{pink}-> %s {white}Dist: %.1f\n"),
							*UEnum::GetDisplayValueAsText(MoveTarget.IntentAtGoal).ToString(), MoveTarget.DistanceToGoal);

						// Look
						if (bHasLookAt)
						{
							const FMassLookAtFragment& LookAt = LookAtList[EntityIndex];
							const float RemainingTime = LookAt.GazeDuration - (CurrentTime - LookAt.GazeStartTime);
							Status += FString::Printf(TEXT("{turquoise}%s/%s {lightgrey}%.1f\n"),
								*UEnum::GetDisplayValueAsText(LookAt.LookAtMode).ToString(), *UEnum::GetDisplayValueAsText(LookAt.RandomGazeMode).ToString(), RemainingTime);
						}
						
						if (!Status.IsEmpty())
						{
							BasePos += FVector(0,0,50);
							constexpr float ViewWeight = 0.6f; // Higher the number the more the view angle affects the score.
							const float ViewScale = 1.f - (ViewDot / MinViewDirDot); // Zero at center of screen
							NearEntityDescriptions.Emplace(DistanceToEntitySq * ((1.0f - ViewWeight) + ViewScale * ViewWeight), BasePos, Status);
						}
					}
				}
			});
		}

		if (bShowNearEntityAvoidance)
		{
			FMassEntityQuery EntityColliderQuery;
			EntityColliderQuery.AddRequirement<FMassAvoidanceColliderFragment>(EMassFragmentAccess::ReadOnly);
			EntityColliderQuery.AddRequirement<FTransformFragment>(EMassFragmentAccess::ReadOnly);
			FMassExecutionContext Context(EntityManager.AsShared(), 0.f);
			EntityColliderQuery.ForEachEntityChunk(EntityManager, Context, [this, ViewLocation, ViewDirection](const FMassExecutionContext& Context)
			{
				const int32 NumEntities = Context.GetNumEntities();
				const TConstArrayView<FTransformFragment> TransformList = Context.GetFragmentView<FTransformFragment>();
				const TConstArrayView<FMassAvoidanceColliderFragment> CollidersList = Context.GetFragmentView<FMassAvoidanceColliderFragment>();

				for (int32 EntityIndex = 0; EntityIndex < NumEntities; ++EntityIndex)
				{
					const FTransformFragment& Transform = TransformList[EntityIndex];
					const FVector EntityLocation = Transform.GetTransform().GetLocation();
					const FVector EntityForward = Transform.GetTransform().GetRotation().GetForwardVector();
					
					FVector BasePos = EntityLocation + FVector(0.0f ,0.0f ,25.0f );

					// Cull entities
					if (!IsLocationInViewCone(ViewLocation, ViewDirection, EntityLocation))
					{
						continue;
					}
					
					// Display colliders
					const FMassAvoidanceColliderFragment& Collider = CollidersList[EntityIndex];
					if (Collider.Type == EMassColliderType::Circle)
					{
						AddShape(FGameplayDebuggerShape::MakeCircle(BasePos, FVector::UpVector, Collider.GetCircleCollider().Radius, FColor::Blue));
					}
					else if (Collider.Type == EMassColliderType::Pill)
					{
						const FMassPillCollider& Pill = Collider.GetPillCollider();
						AddShape(FGameplayDebuggerShape::MakeCircle(BasePos + Pill.HalfLength * EntityForward, FVector::UpVector, Pill.Radius, FColor::Blue));
						AddShape(FGameplayDebuggerShape::MakeCircle(BasePos - Pill.HalfLength * EntityForward, FVector::UpVector, Pill.Radius, FColor::Blue));
					}
				}
			});
		}
		
		// Cap labels to closest ones.
		NearEntityDescriptions.Sort([](const FEntityDescription& LHS, const FEntityDescription& RHS){ return LHS.Score < RHS.Score; });
		constexpr int32 MaxLabels = 15;
		if (NearEntityDescriptions.Num() > MaxLabels)
		{
			NearEntityDescriptions.RemoveAt(MaxLabels, NearEntityDescriptions.Num() - MaxLabels);
		}
	}
	
}

void FGameplayDebuggerCategory_Mass::DrawData(APlayerController* OwnerPC, FGameplayDebuggerCanvasContext& CanvasContext)
{
	CanvasContext.Printf(TEXT("\n[{yellow}%s{white}] %s Archetypes"), *GetInputHandlerDescription(0), bShowArchetypes ? TEXT("Hide") : TEXT("Show"));
	CanvasContext.Printf(TEXT("[{yellow}%s{white}] %s Shapes"), *GetInputHandlerDescription(1), bShowShapes ? TEXT("Hide") : TEXT("Show"));
	CanvasContext.Printf(TEXT("[{yellow}%s{white}] %s Agent Fragments"), *GetInputHandlerDescription(2), bShowAgentFragments ? TEXT("Hide") : TEXT("Show"));
	if (bShowAgentFragments)
	{
		CanvasContext.Printf(TEXT("[{yellow}%s{white}] %s Entity details"), *GetInputHandlerDescription(4), bShowEntityDetails ? TEXT("Hide") : TEXT("Show"));
	}
	else
	{
		CanvasContext.Printf(TEXT("{grey}[%s] Entity details [enable Agent Fragments]{white}"), *GetInputHandlerDescription(4));
	}
	CanvasContext.Printf(TEXT("[{yellow}%s{white}] Pick Entity"), *GetInputHandlerDescription(3));
	CanvasContext.Printf(TEXT("[{yellow}%s{white}] %s Entity overview"), *GetInputHandlerDescription(5), bShowNearEntityOverview ? TEXT("Hide") : TEXT("Show"));
	CanvasContext.Printf(TEXT("[{yellow}%s{white}] %s Entity avoidance"), *GetInputHandlerDescription(6), bShowNearEntityAvoidance ? TEXT("Hide") : TEXT("Show"));
	CanvasContext.Printf(TEXT("[{yellow}%s{white}] %s Entity path"), *GetInputHandlerDescription(7), bShowNearEntityPath ? TEXT("Hide") : TEXT("Show"));

	struct FEntityLayoutRect
	{
		FVector2D Min = FVector2D::ZeroVector;
		FVector2D Max = FVector2D::ZeroVector;
		int32 Index = INDEX_NONE;
		float Alpha = 1.0f;
	};

	TArray<FEntityLayoutRect> Layout;

	// The loop below is O(N^2), make sure to keep the N small.
	constexpr int32 MaxDesc = 20;
	const int32 NumDescs = FMath::Min(NearEntityDescriptions.Num(), MaxDesc);
	
	// The labels are assumed to have been ordered in order of importance (i.e. front to back).
	for (int32 Index = 0; Index < NumDescs; Index++)
	{
		const FEntityDescription& Desc = NearEntityDescriptions[Index];
		if (Desc.Description.Len() && CanvasContext.IsLocationVisible(Desc.Location))
		{
			float SizeX = 0, SizeY = 0;
			const FVector2D ScreenLocation = CanvasContext.ProjectLocation(Desc.Location);
			CanvasContext.MeasureString(Desc.Description, SizeX, SizeY);
			
			FEntityLayoutRect Rect;
			Rect.Min = ScreenLocation + FVector2D(0, -SizeY * 0.5f);
			Rect.Max = Rect.Min + FVector2D(SizeX, SizeY);
			Rect.Index = Index;
			Rect.Alpha = 0.0f;

			// Calculate transparency based on how much more important rects are overlapping the new rect.
			const float Area = FMath::Max(0.0f, Rect.Max.X - Rect.Min.X) * FMath::Max(0.0f, Rect.Max.Y - Rect.Min.Y);
			const float InvArea = Area > KINDA_SMALL_NUMBER ? 1.0f / Area : 0.0f;
			float Coverage = 0.0;

			for (const FEntityLayoutRect& Other : Layout)
			{
				// Calculate rect intersection
				const float MinX = FMath::Max(Rect.Min.X, Other.Min.X);
				const float MinY = FMath::Max(Rect.Min.Y, Other.Min.Y);
				const float MaxX = FMath::Min(Rect.Max.X, Other.Max.X);
				const float MaxY = FMath::Min(Rect.Max.Y, Other.Max.Y);

				// return zero area if not overlapping
				const float IntersectingArea = FMath::Max(0.0f, MaxX - MinX) * FMath::Max(0.0f, MaxY - MinY);
				Coverage += (IntersectingArea * InvArea) * Other.Alpha;
			}

			Rect.Alpha = FMath::Square(1.0f - FMath::Min(Coverage, 1.0f));
			
			if (Rect.Alpha > KINDA_SMALL_NUMBER)
			{
				Layout.Add(Rect);
			}
		}
	}

	// Render back to front so that the most important item renders at top.
	const FVector2D Padding(5, 5);
	for (int32 Index = Layout.Num() - 1; Index >= 0; Index--)
	{
		const FEntityLayoutRect& Rect = Layout[Index];
		const FEntityDescription& Desc = NearEntityDescriptions[Rect.Index];

		const FVector2D BackgroundPosition(Rect.Min - Padding);
		FCanvasTileItem Background(Rect.Min - Padding, Rect.Max - Rect.Min + Padding * 2.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f * Rect.Alpha));
		Background.BlendMode = SE_BLEND_TranslucentAlphaOnly;
		CanvasContext.DrawItem(Background, BackgroundPosition.X, BackgroundPosition.Y);
		
		CanvasContext.PrintAt(Rect.Min.X, Rect.Min.Y, FColor::White, Rect.Alpha, Desc.Description);
	}

	FGameplayDebuggerCategory::DrawData(OwnerPC, CanvasContext);
}
#endif // WITH_GAMEPLAY_DEBUGGER && WITH_MASSGAMEPLAY_DEBUG

