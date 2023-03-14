// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/CacheManagerActor.h"

#include "Chaos/Adapters/CacheAdapter.h"
#include "Chaos/CacheCollection.h"
#include "Chaos/ChaosCache.h"
#include "Chaos/ChaosCachingPlugin.h"
#include "ChaosSolversModule.h"
#include "Components/BillboardComponent.h"
#include "PBDRigidsSolver.h"
#include "Features/IModularFeatures.h"
#include "Algo/Find.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Styling/CoreStyle.h"
#include "Styling/ISlateStyle.h"
#include "UObject/ConstructorHelpers.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#endif

#define LOCTEXT_NAMESPACE "ChaosCacheManager"

FName GetComponentCacheName(UPrimitiveComponent* InComponent)
{
	return FName(InComponent->GetPathName(InComponent->GetWorld()));
}

void FObservedComponent::ResetRuntimeData(const EStartMode ManagerStartMode)
{
	bTriggered       = ManagerStartMode == EStartMode::Timed;
	AbsoluteTime     = 0;
	TimeSinceTrigger = 0;
	Cache            = nullptr;

	TickRecord.Reset();
}

void FObservedComponent::PostSerialize(const FArchive& Ar)
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (Ar.IsLoading() && ComponentRef.OtherActor.IsValid())
	{
		SoftComponentRef.OtherActor = ComponentRef.OtherActor.Get();
		SoftComponentRef.ComponentProperty = ComponentRef.ComponentProperty;
		SoftComponentRef.OverrideComponent = ComponentRef.OverrideComponent;
		SoftComponentRef.PathToComponent = ComponentRef.PathToComponent; 
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}
UPrimitiveComponent* FObservedComponent::GetComponent()
{
	return Cast<UPrimitiveComponent>(SoftComponentRef.GetComponent(nullptr));
}

UPrimitiveComponent* FObservedComponent::GetComponent() const
{
	return Cast<UPrimitiveComponent>(SoftComponentRef.GetComponent(nullptr));
}

AChaosCacheManager::AChaosCacheManager(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
	, CacheMode(ECacheMode::Record)
	, StartMode(EStartMode::Timed)
	, bCanRecord(true)
	, bIsSimulating(false)
{
	// This actor will tick, just not normally. There needs to be a tick-like event both before physics simulation
	// and after physics simulation, we bind to some physics scene events in BeginPlay to handle this.
	PrimaryActorTick.bCanEverTick = true;

	// Add a scene component as our root
	RootComponent = ObjectInitializer.CreateDefaultSubobject<USceneComponent>(this, TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Static);

	// Add a sprite when in the editor
#if WITH_EDITOR
	if (!IsRunningCommandlet())
	{
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> SpriteTextureObject;
			FName                                                 ID_CacheManager;
			FText                                                 NAME_CacheManager;

			FConstructorStatics()
				: SpriteTextureObject(TEXT("/Engine/EditorResources/S_Actor"))
				, ID_CacheManager(TEXT("Cache Manager"))
				, NAME_CacheManager(NSLOCTEXT("SpriteCategory", "CacheManager", "Chaos Cache Manager"))
			{
			}
		};
		static FConstructorStatics ConstructorStatics;

		UBillboardComponent* SpriteComp = ObjectInitializer.CreateEditorOnlyDefaultSubobject<UBillboardComponent>(this, TEXT("Editor Icon"));

		if (SpriteComp)
		{
			SpriteComp->Sprite = ConstructorStatics.SpriteTextureObject.Get();
			SpriteComp->SpriteInfo.Category = ConstructorStatics.ID_CacheManager;
			SpriteComp->SpriteInfo.DisplayName = ConstructorStatics.NAME_CacheManager;
			SpriteComp->Mobility = EComponentMobility::Static;
			SpriteComp->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
#endif
}

void AChaosCacheManager::TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction)
{
	Super::TickActor(DeltaTime, TickType, ThisTickFunction);

	if(CacheCollection)
	{
		CacheCollection->FlushAllCacheWrites();
	}
}

#if WITH_EDITOR
void AChaosCacheManager::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(AChaosCacheManager, CacheMode))
	{
		if ((CacheMode == ECacheMode::Record) && !CanRecord())
		{
			// If we're not allowed to record but have somehow been set to record mode, revert to default
			CacheMode = ECacheMode::None;
		}

		SetObservedComponentProperties(CacheMode);
		
		if (CacheMode != ECacheMode::Record)
		{
			OnStartFrameChanged(StartTime);
		}
		else
		{
			OnStartFrameChanged(0.0);
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(AChaosCacheManager, StartTime)) 
	{
		if (CacheMode != ECacheMode::Record)
		{
			OnStartFrameChanged(StartTime);
		}
	}
}
#endif

void AChaosCacheManager::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	for(FObservedComponent& ObervedComponent : ObservedComponents)
	{
		ObervedComponent.PostSerialize(Ar);
	}
}

void AChaosCacheManager::SetStartTime(float InStartTime)
{
	StartTime = InStartTime;
	OnStartFrameChanged(InStartTime);
}

void AChaosCacheManager::ResetAllComponentTransforms()
{
	if(!CacheCollection)
	{
		return;
	}

	for(FObservedComponent& Observed : ObservedComponents)
	{
		if(UPrimitiveComponent* Comp = Observed.GetComponent())
		{
			if(UChaosCache* Cache = CacheCollection->FindCache(Observed.CacheName))
			{
				Comp->SetWorldTransform(Cache->Spawnable.InitialTransform);
			}
		}
	}
}

void AChaosCacheManager::ResetSingleTransform(int32 InIndex)
{
	if(!ObservedComponents.IsValidIndex(InIndex))
	{
		return;
	}

	FObservedComponent& Observed = ObservedComponents[InIndex];

	if(UPrimitiveComponent* Comp = Observed.GetComponent())
	{
		if(UChaosCache* Cache = CacheCollection->FindCache(Observed.CacheName))
		{
			Comp->SetWorldTransform(Cache->Spawnable.InitialTransform);
		}
	}
}

#if WITH_EDITOR
void AChaosCacheManager::SelectComponent(int32 InIndex)
{
	if(!ObservedComponents.IsValidIndex(InIndex))
	{
		return;
	}

	FObservedComponent& Observed = ObservedComponents[InIndex];

	if(UPrimitiveComponent* Comp = Observed.GetComponent())
	{
		GEditor->SelectNone(true, true);
		GEditor->SelectActor(Comp->GetOwner(), true, true);
		GEditor->SelectComponent(Comp, true, true);
	}
}
#endif


void AChaosCacheManager::BeginPlay()
{
	using namespace Chaos;

	Super::BeginPlay();

	BeginEvaluate();
}

void AChaosCacheManager::BeginEvaluate()
{
	using namespace Chaos;
	bIsSimulating = !(CacheMode == ECacheMode::None);
	
	if(!CacheCollection)
	{
		UE_LOG(LogChaosCache, Warning, TEXT("%s has no valid cache asset. Components will revert to dynamic simulation."), *GetName());
		
		// without a collection the cache manager can't do anything, no reason to initialise the observed array
		SetActorTickEnabled(false);
		return;
	}
	else
	{
		SetActorTickEnabled(true);
	}

	// Build list of adapters for our observed components
	IModularFeatures&               ModularFeatures = IModularFeatures::Get();
	TArray<FComponentCacheAdapter*> Adapters        = ModularFeatures.GetModularFeatureImplementations<FComponentCacheAdapter>(FComponentCacheAdapter::FeatureName);

	for (FComponentCacheAdapter* Adapter : Adapters)
	{
		Adapter->Initialize();
	}
	ActiveAdapters.Reset();

	int32       NumFailedPlaybackEntries = 0;
	const int32 NumComponents            = ObservedComponents.Num();
	for(int32 Index = 0; Index < NumComponents; ++Index)
	{
		FObservedComponent& Observed = ObservedComponents[Index];

		auto ByPriority = [](const FComponentCacheAdapter* A, const FComponentCacheAdapter* B) {
			return A->GetPriority() < B->GetPriority();
		};
		UPrimitiveComponent* Comp = Observed.GetComponent();
		
		if(!Comp)
		{
			UE_LOG(LogChaosCache, Warning, TEXT("%s has invalid observed component reference."), *GetName());
			ActiveAdapters.Add(nullptr);
			continue;
		}
		
		UClass* ActualClass = Comp->GetClass();
    
		TArray<FComponentCacheAdapter*> DirectAdapters = Adapters.FilterByPredicate([ActualClass](const FComponentCacheAdapter* InTest) {
			return InTest && InTest->SupportsComponentClass(ActualClass) == Chaos::FComponentCacheAdapter::SupportType::Direct;
		});

		TArray<FComponentCacheAdapter*> DerivedAdapters = Adapters.FilterByPredicate([ActualClass](const FComponentCacheAdapter* InTest) {
			return InTest && InTest->SupportsComponentClass(ActualClass) == Chaos::FComponentCacheAdapter::SupportType::Derived;
		});

		Algo::Sort(DirectAdapters, ByPriority);
		Algo::Sort(DerivedAdapters, ByPriority);

		if(DirectAdapters.Num() == 0 && DerivedAdapters.Num() == 0)
		{	
			UE_LOG(LogChaosCache, Warning, TEXT("%s observing component with no appropriate adapter."), *GetName());

			// No actual adapter for this class type, log and push nullptr for this observed object
			ActiveAdapters.Add(nullptr);
		}
		else
		{
			if(DirectAdapters.Num() > 0)
			{
				ActiveAdapters.Add(DirectAdapters[0]);
			}
			else
			{
				ActiveAdapters.Add(DerivedAdapters[0]);
			}
		}

		// Reset timers and last cache
		Observed.ResetRuntimeData(StartMode);
		
		bool                    bRequiresRecord = false;
		FComponentCacheAdapter* CurrAdapter     = ActiveAdapters[Index];
		check(CurrAdapter);    // should definitely have added one above

		// We only need to register callbacks if in play/record mode
		if(CacheMode != ECacheMode::None)
		{
			if(Chaos::FPhysicsSolverEvents* Solver = CurrAdapter->BuildEventsSolver(Comp))
			{
				FPerSolverData* SolverData = PerSolverData.Find(Solver);

				if(!SolverData)
				{
					SolverData                  = &PerSolverData.Add(Solver);
					SolverData->PreSolveHandle  = Solver->AddPreAdvanceCallback(FSolverPreAdvance::FDelegate::CreateUObject(this, &AChaosCacheManager::HandlePreSolve, Solver));
					SolverData->PreBufferHandle = Solver->AddPreBufferCallback(FSolverPreAdvance::FDelegate::CreateUObject(this, &AChaosCacheManager::HandlePreBuffer, Solver));
					SolverData->PostSolveHandle = Solver->AddPostAdvanceCallback(FSolverPostAdvance::FDelegate::CreateUObject(this, &AChaosCacheManager::HandlePostSolve, Solver));
					SolverData->TeardownHandle  = Solver->AddTeardownCallback(FSolverTeardown::FDelegate::CreateUObject(this, &AChaosCacheManager::HandleTeardown, Solver));
				}

				if (CacheMode == ECacheMode::Play || !CanRecord())
				{ 
					if(UChaosCache*    PlayCache = CacheCollection->FindCache(Observed.CacheName))
					{
						FCacheUserToken Token = PlayCache->BeginPlayback();
						if(Token.IsOpen() && CurrAdapter->ValidForPlayback(Comp, PlayCache))
						{
							if (CacheMode == ECacheMode::Play)
							{
								SolverData->PlaybackIndices.Add(Index);
								SolverData->PlaybackTickRecords.AddDefaulted();
								SolverData->PlaybackTickRecords.Last().SetSpaceTransform(GetTransform());
								Observed.Cache = PlayCache;
								Observed.TickRecord.SetSpaceTransform(GetTransform());
								Observed.TickRecord.SetLastTime(StartTime);
								OpenPlaybackCaches.Add(TTuple<FCacheUserToken, UChaosCache*>(MoveTemp(Token), Observed.Cache));
								CurrAdapter->InitializeForPlayback(Comp, Observed.Cache, StartTime);
							}
						}
						else
						{
							if(Token.IsOpen())
							{
								UE_LOG(LogChaosCache,
										Warning,
										TEXT("Failed playback for component %s, Selected cache adapter unable to handle the cache (the cache is incompatible)"),
										*Comp->GetPathName());

								// The cache session was valid so make sure to end it
								PlayCache->EndPlayback(Token);
							}
							else    // Already open for record somewhere
								{
								UE_LOG(LogChaosCache,
										Warning,
										TEXT("Failed playback for component %s using cache %s, cache already open for record"),
										*Comp->GetName(),
										*PlayCache->GetPathName());
								}

							++NumFailedPlaybackEntries;
						}
					}
					else
					{
						UE_LOG(LogChaosCache, Log, TEXT("Skipping playback for component %s, no available cache."), *Comp->GetName());
					}
				}
				else // CacheMode == ECacheMode::Record
				{
					FName CacheName = (Observed.CacheName == NAME_None) ? MakeUniqueObjectName(CacheCollection, UChaosCache::StaticClass(), "Cache") : Observed.CacheName;

					UChaosCache*    RecordCache = CacheCollection->FindOrAddCache(CacheName);
					FCacheUserToken Token       = RecordCache->BeginRecord(Comp, CurrAdapter->GetGuid(), GetTransform());

					if(Token.IsOpen())
					{
						SolverData->RecordIndices.Add(Index);

						Observed.Cache = CacheCollection->FindOrAddCache(CacheName);
				
						// We'll record the observed component in Cache Manager's local space.
						Observed.TickRecord.SetSpaceTransform(GetTransform());
						OpenRecordCaches.Add(TTuple<FCacheUserToken, UChaosCache*>(MoveTemp(Token), Observed.Cache));
						CurrAdapter->InitializeForRecord(Comp, Observed.Cache);

						// Ensure we enable the actor tick to flush out the pending record writes
						bRequiresRecord =  true;
					}
				}
			}
		}

		// If we're recording then the physics thread(s) will be filling queues on each cache of pending writes
		// which we consume on the game thread in the manager tick.
		SetActorTickEnabled(bRequiresRecord);
	}

	if(NumFailedPlaybackEntries > 0)
	{
		UE_LOG(LogChaosCache, Warning, TEXT("Failed playback for %d components"), NumFailedPlaybackEntries);

#if WITH_EDITOR
		FNotificationInfo Info(FText::Format(LOCTEXT("FailedPlaybackToast", "Failed Chaos cache playback for {0} components."), FText::AsNumber(NumFailedPlaybackEntries)));
		Info.ExpireDuration = 5.0f;
		Info.bFireAndForget = true;
		Info.Image          = FCoreStyle::Get().GetBrush(TEXT("MessageLog.Warning"));
		FSlateNotificationManager::Get().AddNotification(Info);
#endif
	}
}

void AChaosCacheManager::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	EndEvaluate();
}

void AChaosCacheManager::EndEvaluate()
{
	bIsSimulating = false;
	
	FChaosSolversModule* Module = FChaosSolversModule::GetModule();
	check(Module);

	for(TPair<Chaos::FPhysicsSolverEvents*, FPerSolverData> PerSolver : PerSolverData)
	{
		Chaos::FPhysicsSolverEvents* CurrSolver = PerSolver.Key;
		FPerSolverData&        CurrData   = PerSolver.Value;
		if(ensure(CurrSolver))
		{
			ensure(CurrSolver->RemovePostAdvanceCallback(CurrData.PostSolveHandle));
			ensure(CurrSolver->RemovePreBufferCallback(CurrData.PreBufferHandle));
			ensure(CurrSolver->RemovePreAdvanceCallback(CurrData.PreSolveHandle));
			ensure(CurrSolver->RemoveTeardownCallback(CurrData.TeardownHandle));

			CurrData.PostSolveHandle.Reset();
			CurrData.PreSolveHandle.Reset();
			CurrData.PreBufferHandle.Reset();
			CurrData.TeardownHandle.Reset();
		}
	}
	PerSolverData.Reset();
	ActiveAdapters.Reset();

	// Close any open caches as the session is complete. this will flush pending writes and post-process the cache
	for(TTuple<FCacheUserToken, UChaosCache*>& OpenCache : OpenRecordCaches)
	{
		OpenCache.Get<1>()->EndRecord(OpenCache.Get<0>());
	}
	OpenRecordCaches.Reset();

	for(TTuple<FCacheUserToken, UChaosCache*>& OpenCache : OpenPlaybackCaches)
	{
		OpenCache.Get<1>()->EndPlayback(OpenCache.Get<0>());
	}
	OpenPlaybackCaches.Reset();
}

void AChaosCacheManager::HandlePreSolve(Chaos::FReal InDt, Chaos::FPhysicsSolverEvents* InSolver)
{
	if(!CacheCollection)
	{
		return;
	}

	FPerSolverData* Data = PerSolverData.Find(InSolver);

	if(!Data)
	{
		ensureMsgf(false, TEXT("AChaosCacheManager::HandlePreSolve couldn't find a solver entry - a solver binding has leaked."));
		return;
	}

	TickObservedComponents(Data->PlaybackIndices, InDt, [this, Data](UChaosCache* InCache, FObservedComponent& Observed, Chaos::FComponentCacheAdapter* InAdapter) {
		UPrimitiveComponent* Comp = Observed.GetComponent();
		if(ensure(Comp))
		{
			InAdapter->Playback_PreSolve(Comp, InCache, Observed.TimeSinceTrigger, Observed.TickRecord, Data->PendingKinematicUpdates);
		}
	});
}

void AChaosCacheManager::HandlePreBuffer(Chaos::FReal InDt, Chaos::FPhysicsSolverEvents* InSolver)
{
	if(!CacheCollection)
	{
		return;
	}

	FPerSolverData* Data = PerSolverData.Find(InSolver);

	if(!Data)
	{
		ensureMsgf(false, TEXT("AChaosCacheManager::HandlePreBuffer couldn't find a solver entry - a solver binding has leaked."));
		return;
	}
	if(Data->PendingKinematicUpdates.Num() > 0)
	{
		// If the pending rigid kinematic particles are not empty we are dealing with a rigid solver
		Chaos::FPhysicsSolver* PhysicsSolver = StaticCast<Chaos::FPhysicsSolver*>(InSolver);
		for(Chaos::TPBDRigidParticleHandle<Chaos::FReal, 3>* PendingKinematic : Data->PendingKinematicUpdates)
		{
			PhysicsSolver->GetParticles().MarkTransientDirtyParticle(PendingKinematic);
		}
		Data->PendingKinematicUpdates.Reset();
	}
}

void AChaosCacheManager::HandlePostSolve(Chaos::FReal InDt, Chaos::FPhysicsSolverEvents* InSolver)
{
	if(!CacheCollection)
	{
		return;
	}

	FPerSolverData* Data = PerSolverData.Find(InSolver);

	if(!Data)
	{
		ensureMsgf(false, TEXT("AChaosCacheManager::HandlePostSolve couldn't find a solver entry - a solver binding has leaked."));
		return;
	}

	TickObservedComponents(Data->RecordIndices, InDt, [](UChaosCache* InCache, FObservedComponent& Observed, Chaos::FComponentCacheAdapter* InAdapter) {
		UPrimitiveComponent* Comp = Observed.GetComponent();
		if (ensure(Comp && InCache))
		{
			
			// If we haven't advanced since the last record, don't push another frame
			if (Observed.TimeSinceTrigger > InCache->GetDuration())
			{
				FPendingFrameWrite NewFrame;
				NewFrame.Time = Observed.TimeSinceTrigger;
				InAdapter->Record_PostSolve(Comp, Observed.TickRecord.GetSpaceTransform(), NewFrame, Observed.TimeSinceTrigger);

				InCache->AddFrame_Concurrent(MoveTemp(NewFrame));
			}
		}
	});
}

void AChaosCacheManager::HandleTeardown(Chaos::FPhysicsSolverEvents* InSolver)
{
	// Solver has been deleted, we remove all reference to it to avoid dereferencing a dangling pointer.
	PerSolverData.Remove(InSolver);
}

void AChaosCacheManager::OnStartFrameChanged(Chaos::FReal InTime)
{
	if (bIsSimulating)
	{
		return;
	}
	
	if (!CacheCollection)
	{
		return;
	}
	for (int32 ObservedIdx = 0; ObservedIdx < ObservedComponents.Num(); ++ObservedIdx)
	{
		FObservedComponent& Observed = ObservedComponents[ObservedIdx];
		if (UPrimitiveComponent* Comp = Observed.GetComponent())
		{
			if (!Observed.BestFitAdapter)
			{
				// Cache the best fit adapater so that we don't need to look it up each time we evaluate a start time.
				Observed.BestFitAdapter = Chaos::FAdapterUtil::GetBestAdapterForClass(Comp->GetClass(), false);
			}

			if (!Observed.Cache)
			{
				Observed.Cache = CacheCollection->FindCache(Observed.CacheName);
			}

			if (!Observed.Cache || !Observed.BestFitAdapter || Observed.Cache->GetDuration()==0.0)
			{
				continue;
			}

			FCacheUserToken Token = Observed.Cache->BeginPlayback();
			if (Token.IsOpen())
			{
				if (Observed.BestFitAdapter->ValidForPlayback(Comp, Observed.Cache))
				{
					Observed.BestFitAdapter->SetRestState(Comp, Observed.Cache, GetTransform(), InTime);
				}
				Observed.Cache->EndPlayback(Token);
			}
		}
	}
}

void AChaosCacheManager::TriggerComponent(UPrimitiveComponent* InComponent)
{
	// #BGTODO Maybe not totally thread-safe, probably safer with an atomic or condition var rather than the bTriggered flag
	FObservedComponent* Found = Algo::FindByPredicate(ObservedComponents, [InComponent](const FObservedComponent& Test) {
		return Test.GetComponent() == InComponent;
	});

	if (Found && StartMode == EStartMode::Triggered)
	{
		Found->bTriggered = true;
	}
}

void AChaosCacheManager::TriggerComponentByCache(FName InCacheName)
{
	FObservedComponent* Found = Algo::FindByPredicate(ObservedComponents, [InCacheName](const FObservedComponent& Test) {
		return Test.CacheName == InCacheName && Test.GetComponent();
	});

	if (Found && StartMode == EStartMode::Triggered)
	{
		Found->bTriggered = true;
	}
}

void AChaosCacheManager::TriggerAll()
{
	for(FObservedComponent& Observed : ObservedComponents)
	{
		if (StartMode == EStartMode::Triggered && Observed.GetComponent())
		{
			Observed.bTriggered = true;
		}
	}
}

FObservedComponent* AChaosCacheManager::FindObservedComponent(UPrimitiveComponent* InComponent)
{
	return Algo::FindByPredicate(ObservedComponents, [ToTest = InComponent](const FObservedComponent& Item) {
		return Item.GetComponent() == ToTest;
	});
}

FObservedComponent& AChaosCacheManager::AddNewObservedComponent(UPrimitiveComponent* InComponent)
{
	check(InComponent->CreationMethod != EComponentCreationMethod::UserConstructionScript);
	ObservedComponents.AddDefaulted();
	FObservedComponent& NewEntry = ObservedComponents.Last();

	NewEntry.SoftComponentRef.PathToComponent = InComponent->GetPathName(InComponent->GetOwner());
	NewEntry.SoftComponentRef.OtherActor      = InComponent->GetOwner();

	FName CacheName = NAME_None;
#if WITH_EDITOR
	CacheName = FName(InComponent->GetOwner()->GetActorLabel());
#endif

	NewEntry.CacheName = MakeUniqueObjectName(CacheCollection, UChaosCache::StaticClass(), CacheName);

	return NewEntry;
}

FObservedComponent& AChaosCacheManager::FindOrAddObservedComponent(UPrimitiveComponent* InComponent)
{
	FObservedComponent* Found = FindObservedComponent(InComponent);
	return Found ? *Found : AddNewObservedComponent(InComponent);
}

void AChaosCacheManager::ClearObservedComponents()
{
	for (FObservedComponent& ObservedComponent : ObservedComponents)
	{
		UPrimitiveComponent* Comp = ObservedComponent.GetComponent();
		Comp->DestroyComponent();
	}
	ObservedComponents.Empty();
}

void AChaosCacheManager::TickObservedComponents(const TArray<int32>& InIndices, Chaos::FReal InDt, FTickObservedFunction InCallable)
{
	for(int32 Index : InIndices)
	{
		check(ObservedComponents.IsValidIndex(Index) && ObservedComponents.Num() == ActiveAdapters.Num());

		FObservedComponent&            Observed = ObservedComponents[Index];
		Chaos::FComponentCacheAdapter* Adapter  = ActiveAdapters[Index];

		if(!Observed.Cache)
		{
			// Skip if no available cache - this can happen if a component was deleted while being observed - the other components
			// can play fine, we just omit any that we cannot find.
			continue;
		}

		Observed.AbsoluteTime += InDt;

		// Adapters can be null if there isn't support available for a selected component
		// (happens if a plugin implemented it but is no longer loaded)
		if(Observed.bTriggered && Adapter)
		{
			if (CacheMode == ECacheMode::Play)
			{
				Observed.TickRecord.SetDt(InDt);
			}

			Observed.TimeSinceTrigger += InDt;
			InCallable(Observed.Cache, Observed, Adapter);
		}
	}
}

#if WITH_EDITOR
void AChaosCacheManager::SetObservedComponentProperties(const ECacheMode& NewCacheMode)
{
	for (FObservedComponent& ObservedComponent : ObservedComponents)
	{
		if (UPrimitiveComponent* PrimComp = ObservedComponent.GetComponent())
		{
			if (NewCacheMode == ECacheMode::Record)
			{
				PrimComp->BodyInstance.bSimulatePhysics = ObservedComponent.bIsSimulating;	
			}
			else
			{
				PrimComp->BodyInstance.bSimulatePhysics = false;
			}
		}
	}
}
#endif

AChaosCachePlayer::AChaosCachePlayer(const FObjectInitializer& ObjectInitializer /*= FObjectInitializer::Get()*/)
	: Super(ObjectInitializer)
{
	bCanRecord = false;
}

#undef LOCTEXT_NAMESPACE
