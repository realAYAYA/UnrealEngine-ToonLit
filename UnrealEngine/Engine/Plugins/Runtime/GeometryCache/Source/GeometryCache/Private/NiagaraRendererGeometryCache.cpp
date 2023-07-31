// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraRendererGeometryCache.h"

#include "GeometryCacheStreamingManager.h"
#include "NiagaraConstants.h"
#include "NiagaraDataSet.h"
#include "NiagaraGeometryCacheRendererProperties.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraEmitter.h"
#include "Async/Async.h"

static float GNiagaraGeometryComponentRenderPoolInactiveTimeLimit = 5;
static FAutoConsoleVariableRef CVarNiagaraComponentRenderPoolInactiveTimeLimit(
	TEXT("fx.Niagara.GeometryComponentRenderPoolInactiveTimeLimit"),
	GNiagaraGeometryComponentRenderPoolInactiveTimeLimit,
	TEXT("The time in seconds an inactive component can linger in the pool before being destroyed."),
	ECVF_Default
);

//////////////////////////////////////////////////////////////////////////


FNiagaraRendererGeometryCache::FNiagaraRendererGeometryCache(ERHIFeatureLevel::Type FeatureLevel, const UNiagaraRendererProperties *InProps, const FNiagaraEmitterInstance* Emitter)
	: FNiagaraRenderer(FeatureLevel, InProps, Emitter)
{
	const UNiagaraGeometryCacheRendererProperties* Properties = CastChecked<const UNiagaraGeometryCacheRendererProperties>(InProps);
	ComponentPool.Reserve(Properties->ComponentCountLimit);
}

FNiagaraRendererGeometryCache::~FNiagaraRendererGeometryCache()
{
	// These should have been freed in DestroyRenderState_Concurrent
	check(ComponentPool.Num() == 0);
}

void FNiagaraRendererGeometryCache::DestroyRenderState_Concurrent()
{
	// Rendering resources are being destroyed, but the component pool and their owner actor must be destroyed on the game thread
	AsyncTask(
		ENamedThreads::GameThread,
		[Pool_GT = MoveTemp(ComponentPool), Owner_GT = MoveTemp(SpawnedOwner)]()
		{
			for (const FComponentPoolEntry& PoolEntry : Pool_GT)
			{
				if (PoolEntry.Component.IsValid())
				{
					IGeometryCacheStreamingManager::Get().RemoveStreamingComponent(PoolEntry.Component.Get());
					PoolEntry.Component->DestroyComponent();
				}
			}

			if (AActor* OwnerActor = Owner_GT.Get())
			{
				OwnerActor->Destroy();
			}
		}
	);
	SpawnedOwner.Reset();
}

UGeometryCache* ResolveGeometryCache(const FNiagaraGeometryCacheReference& Entry, const FNiagaraEmitterInstance* Emitter)
{
	UGeometryCache* FoundCache = nullptr;

	FNiagaraVariable Variable = Entry.GeometryCacheUserParamBinding.Parameter;
	if (Variable.IsValid() && Emitter)
	{
		UGeometryCache* GeometryCache = Cast<UGeometryCache>(Emitter->GetRendererBoundVariables().GetUObject(Variable));
		if (GeometryCache)
		{
			FoundCache = GeometryCache;
		}
	}

	if (!FoundCache)
	{
		FoundCache = Entry.GeometryCache;
	}

	return FoundCache;
}

int32 GetGeometryCacheIndex(const FNiagaraDataSetReaderInt32<int32>& ArrayIndexAccessor, bool bCreateRandomIfUnassigned, const FNiagaraEmitterInstance* Emitter, const UNiagaraGeometryCacheRendererProperties* Properties, int32 ParticleIndex, int32 ParticleID)
{
	if (bCreateRandomIfUnassigned == false && ArrayIndexAccessor.IsValid() == false)
	{
		return INDEX_NONE;
	}
	int32 CacheIndex = ArrayIndexAccessor.GetSafe(ParticleIndex, INDEX_NONE);
	if (CacheIndex == INDEX_NONE && Emitter->GetCachedEmitterData()->bDeterminism)
	{
		int32 Seed = Properties->bAssignComponentsOnParticleID ? ParticleID : ParticleIndex;
		FRandomStream RandomStream = FRandomStream(Seed * 907633515U); // multiply the seed, otherwise we get very poor randomness for small seeds
		CacheIndex = RandomStream.RandRange(0, Properties->GeometryCaches.Num());
	}
	else if (CacheIndex == INDEX_NONE)
	{
		CacheIndex = FMath::RandRange(0, Properties->GeometryCaches.Num() - 1);
	}
	return CacheIndex % Properties->GeometryCaches.Num(); // wrap around if index is too large
}

/** Update render data buffer from attributes */
void FNiagaraRendererGeometryCache::PostSystemTick_GameThread(const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter)
{
	FNiagaraSystemInstance* SystemInstance = Emitter->GetParentSystemInstance();

	//Bail if we don't have the required attributes to render this emitter.
	const UNiagaraGeometryCacheRendererProperties* Properties = CastChecked<const UNiagaraGeometryCacheRendererProperties>(InProperties);
	if (!SystemInstance || !Properties || Properties->GeometryCaches.Num() == 0 || SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		return;
	}

	USceneComponent* AttachComponent = SystemInstance->GetAttachComponent();
	if (!AttachComponent)
	{
		// we can't attach the components anywhere, so just bail
		return;
	}

	FNiagaraDataSet& Data = Emitter->GetData();
	FNiagaraDataBuffer& ParticleData = Data.GetCurrentDataChecked();
	FNiagaraDataSetReaderInt32<FNiagaraBool> EnabledAccessor = FNiagaraDataSetAccessor<FNiagaraBool>::CreateReader(Data, Properties->EnabledBinding.GetDataSetBindableVariable().GetName());
	FNiagaraDataSetReaderInt32<int32> VisTagAccessor = FNiagaraDataSetAccessor<int32>::CreateReader(Data, Properties->RendererVisibilityTagBinding.GetDataSetBindableVariable().GetName());
	FNiagaraDataSetReaderInt32<int32> ArrayIndexAccessor = FNiagaraDataSetAccessor<int32>::CreateReader(Data, Properties->ArrayIndexBinding.GetDataSetBindableVariable().GetName());
	FNiagaraDataSetReaderInt32<int32> UniqueIDAccessor = FNiagaraDataSetAccessor<int32>::CreateReader(Data, FName("UniqueID"));
	float CurrentTime = AttachComponent->GetWorld()->GetRealTimeSeconds();
	
	auto IsParticleEnabled = [&EnabledAccessor, &VisTagAccessor, Properties](int32 ParticleIndex)
	{
		if (EnabledAccessor.GetSafe(ParticleIndex, true))
		{
			if (VisTagAccessor.IsValid())
			{
				return VisTagAccessor.GetSafe(ParticleIndex, 0) == Properties->RendererVisibility;
			}
			return true;
		}
		return false;
	};

	TMap<int32, int32> ParticlesWithComponents;
	TArray<int32> FreeList;
	if (Properties->bAssignComponentsOnParticleID && ComponentPool.Num() > 0)
	{
		FreeList.Reserve(ComponentPool.Num());

		// Determine the slots that were assigned to particles last frame
		TMap<int32, int32> UsedSlots;
		UsedSlots.Reserve(ComponentPool.Num());
		for (int32 EntryIndex = 0; EntryIndex < ComponentPool.Num(); ++EntryIndex)
		{
			FComponentPoolEntry& Entry = ComponentPool[EntryIndex];
			if (Entry.LastAssignedToParticleID >= 0)
			{
				UsedSlots.Emplace(Entry.LastAssignedToParticleID, EntryIndex);
			}
			else
			{
				FreeList.Add(EntryIndex);
			}
		}

		// Ensure the final list only contains particles that are alive and enabled
		ParticlesWithComponents.Reserve(UsedSlots.Num());
		for (uint32 ParticleIndex = 0; ParticleIndex < ParticleData.GetNumInstances(); ParticleIndex++)
		{
			int32 ParticleID = UniqueIDAccessor.GetSafe(ParticleIndex, -1);
			int32 PoolIndex;
			if (UsedSlots.RemoveAndCopyValue(ParticleID, PoolIndex))
			{
				if (IsParticleEnabled(ParticleIndex))
				{
					ParticlesWithComponents.Emplace(ParticleID, PoolIndex);
				}
				else
				{
					// Particle has disabled components since last tick, ensure the component for this entry gets deactivated before re-use
					USceneComponent* Component = ComponentPool[PoolIndex].Component.Get();
					if (Component && Component->IsActive())
					{
						Component->Deactivate();
						Component->SetVisibility(false, true);
					}
					FreeList.Add(PoolIndex);
					ComponentPool[PoolIndex].LastAssignedToParticleID = -1;
				}
			}
		}

		// Any remaining in the used slots are now free to be reclaimed, due to their particles either dying or having their component disabled
		for (TPair<int32, int32> UsedSlot : UsedSlots)
		{
			// Particle has died since last tick, ensure the component for this entry gets deactivated before re-use
			USceneComponent* Component = ComponentPool[UsedSlot.Value].Component.Get();
			if (Component && Component->IsActive())
			{
				Component->Deactivate();
				Component->SetVisibility(false, true);
			}
			FreeList.Add(UsedSlot.Value);
			ComponentPool[UsedSlot.Value].LastAssignedToParticleID = -1;
		}
	}

	const int32 MaxComponents = Properties->ComponentCountLimit;
	int32 ComponentCount = 0;
	for (uint32 ParticleIndex = 0; ParticleIndex < ParticleData.GetNumInstances(); ParticleIndex++)
	{
		if (!IsParticleEnabled(ParticleIndex))
		{
			// Skip particles that don't want a component
			continue;
		}

		int32 ParticleID = -1;
		int32 PoolIndex = -1;
		if (Properties->bAssignComponentsOnParticleID)
		{
			// Get the particle ID and see if we have any components already assigned to the particle
			ParticleID = UniqueIDAccessor.GetSafe(ParticleIndex, -1);
			ParticlesWithComponents.RemoveAndCopyValue(ParticleID, PoolIndex);
		}

		if (PoolIndex == -1 && ComponentCount + ParticlesWithComponents.Num() >= MaxComponents)
		{
			// The pool is full and there aren't any unused slots to claim
			continue;
		}

		// Acquire a component for this particle
		UGeometryCacheComponent* GeometryComponent = nullptr;
		if (PoolIndex == -1)
		{
			// Start by trying to pull from the pool
			if (!Properties->bAssignComponentsOnParticleID)
			{
				// We can just take the next slot
				PoolIndex = ComponentCount < ComponentPool.Num() ? ComponentCount : -1;
			}
			else if (FreeList.Num())
			{
				PoolIndex = FreeList.Pop(false);
			}
		}

		if (PoolIndex >= 0)
		{
			GeometryComponent = ComponentPool[PoolIndex].Component.Get();
		}

		bool bCreateNewComponent = !GeometryComponent || GeometryComponent->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed);
		int32 CacheIndex = GetGeometryCacheIndex(ArrayIndexAccessor, bCreateNewComponent, Emitter, Properties, ParticleIndex, ParticleID);
		if (Properties->GeometryCaches.IsValidIndex(CacheIndex))
		{
			const FNiagaraGeometryCacheReference& Entry = Properties->GeometryCaches[CacheIndex];
			UGeometryCache* Cache = ResolveGeometryCache(Entry, Emitter);
			if (!Cache)
			{
				// no valid geometry cache was provided for this particle
				continue;
			}
			
			if (bCreateNewComponent)
			{
				// Determine the owner actor or spawn one
				AActor* OwnerActor = SpawnedOwner.Get();
				if (OwnerActor == nullptr)
				{
					OwnerActor = AttachComponent->GetOwner();
					if (OwnerActor == nullptr)
					{
						// NOTE: This can happen with spawned systems
						OwnerActor = AttachComponent->GetWorld()->SpawnActor<AActor>();
						OwnerActor->SetFlags(RF_Transient);
						SpawnedOwner = OwnerActor;
					}
				}

				// if we don't have a pooled component we create a new one from the template
				GeometryComponent = NewObject<UGeometryCacheComponent>(OwnerActor);
				IGeometryCacheStreamingManager::Get().AddStreamingComponent(GeometryComponent);
				GeometryComponent->SetFlags(RF_Transient);
				GeometryComponent->SetupAttachment(AttachComponent);
				GeometryComponent->RegisterComponent();
				GeometryComponent->AddTickPrerequisiteComponent(AttachComponent);
				GeometryComponent->SetLooping(Properties->bIsLooping);
				GeometryComponent->SetManualTick(true); // we want to tick the component with the delta time of the niagara sim

				if (Emitter->GetCachedEmitterData()->bLocalSpace)
				{
					GeometryComponent->SetAbsolute(false, false, false);
				}
				else
				{
					GeometryComponent->SetAbsolute(true, true, true);
				}

				if (PoolIndex >= 0)
				{
					// This should only happen if the component was destroyed externally
					ComponentPool[PoolIndex].Component = GeometryComponent;
					ComponentPool[PoolIndex].LastElapsedTime = 0;
				}
				else
				{
					// Add a new pool entry
					PoolIndex = ComponentPool.Num();
					ComponentPool.AddDefaulted_GetRef().Component = GeometryComponent;
					ComponentPool[PoolIndex].LastElapsedTime = 0;
				}
			}

			// set the cache and override materials, since they might have changed from the last time we accessed the array index
			// if they did not change from last tick these calls should be no-ops.
			GeometryComponent->SetGeometryCache(Cache);
			for (int32 i = 0; i < Entry.OverrideMaterials.Num(); i++)
			{
				const TObjectPtr<UMaterialInterface>& OverrideMat = Entry.OverrideMaterials[i];
				GeometryComponent->SetMaterial(i, OverrideMat);
			}
		}
		else if (bCreateNewComponent)
		{
			// can't create a new component without valid geometry cache
			continue;
		}

		const auto PositionAccessor = FNiagaraDataSetAccessor<FNiagaraPosition>::CreateReader(Data, Properties->PositionBinding.GetDataSetBindableVariable().GetName());
		const auto RotationAccessor = FNiagaraDataSetAccessor<FVector3f>::CreateReader(Data, Properties->RotationBinding.GetDataSetBindableVariable().GetName());
		const auto ScaleAccessor = FNiagaraDataSetAccessor<FVector3f>::CreateReader(Data, Properties->ScaleBinding.GetDataSetBindableVariable().GetName());
		const auto ElapsedTimeAccessor = FNiagaraDataSetAccessor<float>::CreateReader(Data, Properties->ElapsedTimeBinding.GetDataSetBindableVariable().GetName());
		FNiagaraLWCConverter LwcConverter = SystemInstance->GetLWCConverter(Emitter->GetCachedEmitterData()->bLocalSpace);

		FVector Position = LwcConverter.ConvertSimulationPositionToWorld(PositionAccessor.GetSafe(ParticleIndex, FNiagaraPosition(ForceInit)));
		FVector Scale = (FVector)ScaleAccessor.GetSafe(ParticleIndex, FVector3f::OneVector);
		FVector RotationVector = (FVector)RotationAccessor.GetSafe(ParticleIndex, FVector3f::ZeroVector);
		FTransform Transform(FRotator(RotationVector.X, RotationVector.Y, RotationVector.Z), Position, Scale);
		GeometryComponent->SetRelativeTransform(Transform);

		// activate the component
		if (!GeometryComponent->IsActive())
		{
			GeometryComponent->SetVisibility(true, true);
			GeometryComponent->Activate(false);
		}

		float ElapsedTime = ElapsedTimeAccessor.GetSafe(ParticleIndex, 0);

		float Duration = GeometryComponent->GetDuration();
		if (Properties->bIsLooping && ElapsedTime < 0)
		{
			ElapsedTime -= FMath::CeilToFloat(ElapsedTime / Duration) * Duration;
		}
		else if (Properties->bIsLooping && ElapsedTime > Duration)
		{
			ElapsedTime -= FMath::Floor(ElapsedTime / Duration) * Duration;
		}

		FComponentPoolEntry& PoolEntry = ComponentPool[PoolIndex];
		GeometryComponent->TickAtThisTime(ElapsedTime, true, ElapsedTime < PoolEntry.LastElapsedTime, Properties->bIsLooping);

		PoolEntry.LastElapsedTime = ElapsedTime;
		PoolEntry.LastAssignedToParticleID = ParticleID;
		PoolEntry.LastActiveTime = CurrentTime;

		++ComponentCount;
		if (ComponentCount >= MaxComponents)
		{
			// We've hit our prescribed limit
			break;
		}
	}

	if (ComponentCount < ComponentPool.Num())
	{
		// go over the pooled components we didn't need this tick to see if we can destroy some and deactivate the rest
		for (int32 PoolIndex = 0; PoolIndex < ComponentPool.Num(); ++PoolIndex)
		{
			FComponentPoolEntry& PoolEntry = ComponentPool[PoolIndex];
			if (Properties->bAssignComponentsOnParticleID)
			{
				if (PoolEntry.LastAssignedToParticleID >= 0)
				{
					// This one's in use
					continue;
				}
			}
			else if (PoolIndex < ComponentCount)
			{
				continue;
			}

			USceneComponent* Component = PoolEntry.Component.Get();
			if (!Component || (CurrentTime - PoolEntry.LastActiveTime) >= GNiagaraGeometryComponentRenderPoolInactiveTimeLimit)
			{
				if (Component)
				{
					IGeometryCacheStreamingManager::Get().RemoveStreamingComponent(PoolEntry.Component.Get());
					Component->DestroyComponent();
				}

				// destroy the component pool slot
				ComponentPool.RemoveAtSwap(PoolIndex, 1, false);
				--PoolIndex;
				continue;
			}
			else if (Component->IsActive())
			{
				Component->Deactivate();
				Component->SetVisibility(false, true);
			}
		}
	}
}

void FNiagaraRendererGeometryCache::OnSystemComplete_GameThread(const UNiagaraRendererProperties* InProperties, const FNiagaraEmitterInstance* Emitter)
{
	ResetComponentPool(true);
}

void FNiagaraRendererGeometryCache::ResetComponentPool(bool bResetOwner)
{
	for (FComponentPoolEntry& PoolEntry : ComponentPool)
	{
		if (PoolEntry.Component.IsValid())
		{
			IGeometryCacheStreamingManager::Get().RemoveStreamingComponent(PoolEntry.Component.Get());
			PoolEntry.Component->DestroyComponent();
		}
	}
	ComponentPool.SetNum(0, false);

	if (bResetOwner)
	{
		if (AActor* OwnerActor = SpawnedOwner.Get())
		{
			SpawnedOwner.Reset();
			OwnerActor->Destroy();
		}
	}
}
