// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataChannel_Islands.h"

#include "Engine/AssetManager.h"
#include "UObject/UObjectGlobals.h"

#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "NiagaraWorldManager.h"
#include "NiagaraDataChannelCommon.h"

#include "DrawDebugHelpers.h"

//////////////////////////////////////////////////////////////////////////

namespace NDCIslands_Local
{
	int32 AllowAsyncLoad = 1;
	static FAutoConsoleVariableRef CVarAllowAsyncLoad(TEXT("fx.Niagara.DataChannels.AllowAsyncLoad"), AllowAsyncLoad, TEXT("True if we should attempt to load systems etc asynchronosly."), ECVF_Default);

	int32 BlockAsyncLoadOnUse = 1;
	static FAutoConsoleVariableRef CVarBlockAsyncLoadOnUse(TEXT("fx.Niagara.DataChannels.BlockAsyncLoadOnUse"), BlockAsyncLoadOnUse, TEXT("True if we should block on any pending async loads when those assets are used."), ECVF_Default);
}

void UNiagaraDataChannel_Islands::PostLoad()
{
	Super::PostLoad();
	
	//For now immediately load handler systems but in future we should look at deferring the load until a channel is used.
	//Sub classes may want to load differently so we'll keep this a soft ptr.
	AsyncLoadSystems();
}

TConstArrayView<TObjectPtr<UNiagaraSystem>> UNiagaraDataChannel_Islands::GetSystems()const
{
	check(IsInGameThread());
	if(NDCIslands_Local::BlockAsyncLoadOnUse && AsyncLoadHandle && AsyncLoadHandle->IsActive())
	{
		AsyncLoadHandle->WaitUntilComplete();
		PostLoadSystems();
	}

	return SystemsInternal;
}

void UNiagaraDataChannel_Islands::AsyncLoadSystems()const
{
	if(INiagaraModule::DataChannelsEnabled() && !IsRunningDedicatedServer())
	{
		TArray<FSoftObjectPath> Requests;
		for (const TSoftObjectPtr<UNiagaraSystem>& SoftSys : Systems)
		{
			if (SoftSys.IsPending())
			{
				Requests.Add(SoftSys.ToSoftObjectPath());
			}
		}

		if (Requests.Num() > 0)
		{
			if (NDCIslands_Local::AllowAsyncLoad && UAssetManager::IsInitialized())
			{
				AsyncLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Requests
					, FStreamableDelegate::CreateUObject(this, &UNiagaraDataChannel_Islands::PostLoadSystems)
					, FStreamableManager::AsyncLoadHighPriority
					, false
					, false
					, TEXT("NiagaraDataChannelIsland_HandlerSystems"));
			}
			else
			{
				for (FSoftObjectPath& Path : Requests)
				{
					Path.TryLoad();
				}
				PostLoadSystems();
			}
		}
		else
		{
			PostLoadSystems();
		}
	}
}

void UNiagaraDataChannel_Islands::PostLoadSystems()const
{
	check(IsInGameThread());
	SystemsInternal.Reset(Systems.Num());
	for(auto& SoftSys : Systems)
	{
		SystemsInternal.Add(SoftSys.Get());
	}

	AsyncLoadHandle.Reset();
}

//////////////////////////////////////////////////////////////////////////

UNiagaraDataChannelHandler* UNiagaraDataChannel_Islands::CreateHandler(UWorld* OwningWorld)const
{
	UNiagaraDataChannelHandler* NewHandler = NewObject<UNiagaraDataChannelHandler_Islands>(OwningWorld);
	NewHandler->Init(this);
	return NewHandler;
}

UNiagaraDataChannelHandler_Islands::UNiagaraDataChannelHandler_Islands(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UNiagaraDataChannelHandler_Islands::BeginDestroy()
{
	Super::BeginDestroy();
	ActiveIslands.Empty();
	FreeIslands.Empty();
	IslandPool.Empty();
}

void UNiagaraDataChannelHandler_Islands::Init(const UNiagaraDataChannel* InChannel)
{
	Super::Init(InChannel);
	if (const UNiagaraDataChannel_Islands* IslandChannel = CastChecked<UNiagaraDataChannel_Islands>(InChannel))
	{
		IslandChannel->AsyncLoadSystems();

		int32 InitialPoolCount = IslandChannel->GetIslandPoolSize();
		ActiveIslands.Reserve(InitialPoolCount);
		FreeIslands.Reserve(InitialPoolCount);
		IslandPool.SetNum(InitialPoolCount);
		for (int32 i = 0; i < InitialPoolCount; ++i)
		{
			IslandPool[i].Init(this);
			FreeIslands.Emplace(i);
		}
	}
}

void UNiagaraDataChannelHandler_Islands::BeginFrame(float DeltaTime, FNiagaraWorldManager* OwningWorld)
{
	Super::BeginFrame(DeltaTime, OwningWorld);
	for (auto It = ActiveIslands.CreateIterator(); It; ++It)
	{
		FNDCIsland& Island = IslandPool[*It];
		Island.BeginFrame();
	}
}

void UNiagaraDataChannelHandler_Islands::EndFrame(float DeltaTime, FNiagaraWorldManager* OwningWorld)
{
	Super::EndFrame(DeltaTime, OwningWorld);
	for (auto It = ActiveIslands.CreateIterator(); It; ++It)
	{
		FNDCIsland& Island = IslandPool[*It];
		Island.EndFrame();
	}
}

void UNiagaraDataChannelHandler_Islands::Tick(float DeltaTime, ETickingGroup TickGroup, FNiagaraWorldManager* OwningWorld)
{
	Super::Tick(DeltaTime, TickGroup, OwningWorld);

	//static UEnum* TGEnum = StaticEnum<ETickingGroup>();
	//UE_LOG(LogNiagara,Warning, TEXT("UNiagaraDataChannelHandler_Islands::Tick - %s"), *TGEnum->GetDisplayNameTextByValue((int32)TickGroup).ToString());
	const UNiagaraDataChannel_Islands* IslandChannel = GetChannelTyped<const UNiagaraDataChannel_Islands>();
	for (auto It = ActiveIslands.CreateIterator(); It; ++It)
	{
		FNDCIsland& Island = IslandPool[*It];
		Island.Tick(TickGroup);

		if(IslandChannel->GetDebugDrawSettings().ShowBounds())
		{
			Island.DebugDrawBounds();
		}

		//Free up islands that are not being used.
		//We do this immediately as we leave it up to the handler systems to delay their cleanup as long as they want to.
		if (Island.IsBeingUsed() == false)
		{
			FreeIslands.Emplace(*It);
			Island.OnReleased();
			It.RemoveCurrentSwap();
		}
	}
}

FNiagaraDataChannelDataPtr UNiagaraDataChannelHandler_Islands::FindData(FNiagaraDataChannelSearchParameters SearchParams, ENiagaraResourceAccess AccessType)
{
	if (FNDCIsland* Island = FindOrCreateIsland(SearchParams, AccessType))
	{
		return Island->GetData();
	}
	return nullptr;
}

FNDCIsland* UNiagaraDataChannelHandler_Islands::FindOrCreateIsland(const FNiagaraDataChannelSearchParameters& SearchParams, ENiagaraResourceAccess AccessType)
{
	const UNiagaraDataChannel_Islands* IslandChannel = CastChecked<const UNiagaraDataChannel_Islands>(DataChannel);

	FVector Location = SearchParams.GetLocation();
	if (AccessType == ENiagaraResourceAccess::ReadOnly)
	{
		//If we're reading only then we just try to find the right island to read from.
		//When writing we'll grow/spawn islands to accommodate the data.

		//First we see if this is an islands handler system.
		if(SearchParams.GetOwner())
		{
			for (int32 i : ActiveIslands)
			{
				FNDCIsland& Island = IslandPool[i];
				if (Island.IsHandlerSystem(SearchParams.GetOwner()))
				{
					return &Island;
				}
			}
			//Failing that, we'll just return the first island that intersects the owners bounds.
			for (int32 i : ActiveIslands)
			{
				FNDCIsland& Island = IslandPool[i];
				if (Island.Intersects(SearchParams.GetOwner()->Bounds))
				{
					return &Island;
				}
			}
		}
		else
		{
			//We don't have an owning component so just return the first island to contain the location.
			for (int32 i : ActiveIslands)
			{
				FNDCIsland& Island = IslandPool[i];
				if (Island.Contains(Location))
				{
					return &Island;
				}
			}
		}
		return nullptr;
	}
	else
	{
		UWorld* World = GetWorld();
		check(World);
		double WorldTime = World->GetTimeSeconds();

		//Find the first island that could contain this point.
		//For now we do a linear search of active islands.
		//Assuming that overall active island count will be low.
		//If this is not the case then we'll want to add an acceleration structure to speed up this search.
		FNDCIsland* IslandToUse = nullptr;
		FVector MaxExtents = IslandChannel->GetMaxExtents();
		FVector PerElementExtents = IslandChannel->GetPerElementExtents();
		for (int32 i : ActiveIslands)
		{
			FNDCIsland& Island = IslandPool[i];
			FBoxSphereBounds GrowthBounds;
			if (Island.TryGrow(Location, PerElementExtents, MaxExtents))
			{
				IslandToUse = &Island;
			}
		}

		if (IslandToUse)
		{
			return IslandToUse;
		}

		//Failing that, get a init a new island from the pool.
		int32 NewIslandIndex = ActivateNewIsland(Location);
		if (ensure(NewIslandIndex != INDEX_NONE))
		{
			IslandToUse = &IslandPool[NewIslandIndex];
		}

		if (IslandToUse)
		{
			IslandToUse->OnAcquired(Location);
		}

		return IslandToUse;
	}
}

int32 UNiagaraDataChannelHandler_Islands::ActivateNewIsland(FVector Location)
{
	int32 NewIndex = INDEX_NONE;
	FNDCIsland* NewIsland = nullptr;
	if (FreeIslands.Num() > 0)
	{
		NewIndex = FreeIslands.Pop(EAllowShrinking::No);
		NewIsland = &IslandPool[NewIndex];
	}

	if (NewIsland == nullptr)
	{
		NewIndex = IslandPool.Num();
		NewIsland = &IslandPool.AddDefaulted_GetRef();
	}

	if (NewIndex != INDEX_NONE)
	{
		check(NewIsland);
		ActiveIslands.Add(NewIndex);
		NewIsland->Init(this);
	}
	else
	{
		UE_LOG(LogNiagara, Error, TEXT("Failed to allocate new island for Island Data Channel %s"), *GetDataChannel()->GetName());
	}

	return NewIndex;
}

//////////////////////////////////////////////////////////////////////////

FNDCIsland::FNDCIsland()
{
}

FNDCIsland::~FNDCIsland()
{
	for (UNiagaraComponent* Comp : NiagaraSystems)
	{
		if (IsValid(Comp))
		{
			Comp->ReleaseToPool();
		}
	}
	NiagaraSystems.Empty();
}

void FNDCIsland::Init(UNiagaraDataChannelHandler_Islands* InOwner)
{
	Owner = InOwner;

	Data = Owner->CreateData();

	const UNiagaraDataChannel_Islands* Channel = CastChecked<UNiagaraDataChannel_Islands>(Owner->GetDataChannel());

	Bounds.Origin = FVector::ZeroVector;
	Bounds.BoxExtent = Channel->GetInitialExtents();
}

void FNDCIsland::OnAcquired(FVector Location)
{
	const UNiagaraDataChannel_Islands* Channel = CastChecked<UNiagaraDataChannel_Islands>(Owner->GetDataChannel());

	if(Channel->GetMode() == ENiagraDataChannel_IslandMode::AlignedStatic)
	{
		//Align our location down to the nearest lower multiple of the max island size to avoid overlapping islands.
		//TODO: make our padding extents come from the owning component or the handler systems?
		FVector ActualMaxSize = Channel->GetMaxExtents() + Channel->GetPerElementExtents();
		FVector BoxSize = Channel->GetMaxExtents() * 2;
		FVector BoxN = Location / BoxSize;
		BoxN.X = FMath::RoundToNegativeInfinity(BoxN.X);
		BoxN.Y = FMath::RoundToNegativeInfinity(BoxN.Y);
		BoxN.Z = FMath::RoundToNegativeInfinity(BoxN.Z);
		Bounds.Origin = BoxN * BoxSize + Channel->GetMaxExtents();
		Bounds.BoxExtent = ActualMaxSize;
	}
	else
	{
		Bounds.Origin = Location;
		Bounds.BoxExtent = Channel->GetInitialExtents();
	}

	Data->SetLwcTile(FLargeWorldRenderScalar::GetTileFor(Bounds.Origin));
	//Spawn our handler systems.
	FBox HandlerSystemBounds(-Bounds.BoxExtent, Bounds.BoxExtent);
	for (const TObjectPtr<UNiagaraSystem>& Sys : Channel->GetSystems())
	{
		if(Sys)
		{
			FFXSystemSpawnParameters SpawnParams;
			SpawnParams.bAutoActivate = false;//We must activate AFTER adding this component to our handle system array.
			SpawnParams.bAutoDestroy = false;
			SpawnParams.Location = Bounds.Origin;
			SpawnParams.PoolingMethod = EPSCPoolMethod::ManualRelease;
			SpawnParams.bPreCullCheck = false;
			SpawnParams.SystemTemplate = Sys;
			SpawnParams.WorldContextObject = Owner->GetWorld();
			if(UNiagaraComponent* NewComp = UNiagaraFunctionLibrary::SpawnSystemAtLocationWithParams(SpawnParams))
			{
				NiagaraSystems.Add(NewComp);
				NewComp->SetSystemFixedBounds(HandlerSystemBounds);
				NewComp->Activate();
			}
		}
	}

	if (Channel->GetMode() == ENiagraDataChannel_IslandMode::Dynamic)
	{
		ensure(TryGrow(Location, Channel->GetPerElementExtents(), Channel->GetMaxExtents()));
	}
}

void FNDCIsland::OnReleased()
{
	Data->Reset();
	for (UNiagaraComponent* Comp : NiagaraSystems)
	{
		Comp->ReleaseToPool();
	}
	NiagaraSystems.Reset();
}

void FNDCIsland::BeginFrame()
{
	Data->BeginFrame(Owner);
}

void FNDCIsland::EndFrame()
{
	Data->EndFrame(Owner);
}

void FNDCIsland::Tick(const ETickingGroup& TickGroup)
{
	int32 AddedData = Data->ConsumePublishRequests(Owner, TickGroup);
	if (IsBeingUsed() && AddedData > 0)
	{
		for (UNiagaraComponent* Comp : NiagaraSystems)
		{
			if (Comp->IsComplete())
			{
				Comp->Activate();
			}
		}
	}
}

bool FNDCIsland::Contains(FVector Point)
{
	if (DistanceToPoint(Point) > 0)
	{
		return false;
	}

	return true;
}

double FNDCIsland::DistanceToPoint(FVector Point)
{
	return Bounds.ComputeSquaredDistanceFromBoxToPoint(Point);
}

bool FNDCIsland::Intersects(const FBoxSphereBounds& CheckBounds)
{
	return FBoxSphereBounds::BoxesIntersect(Bounds, CheckBounds);
}

bool FNDCIsland::IsHandlerSystem(USceneComponent* Component)
{
	return NiagaraSystems.Contains(Component);
}

bool FNDCIsland::IsBeingUsed()const
{
	return Data.IsUnique() == false;//If someone is making use of our data then we're still in use.
}

bool FNDCIsland::TryGrow(FVector Point, FVector PerElementExtents, FVector MaxIslandExtents)
{
	FVector CurrMax = Bounds.Origin + Bounds.BoxExtent;
	FVector CurrMin = Bounds.Origin - Bounds.BoxExtent;

	FVector ElementMax = Point + PerElementExtents;
	FVector ElementMin = Point - PerElementExtents;

	//The max size of the island, we pad these out with the per element extents to ensure bounds cover anything right at the boundary.
	FVector Max = Bounds.Origin + MaxIslandExtents + PerElementExtents;
	FVector Min = Bounds.Origin - MaxIslandExtents - PerElementExtents;

	//Can this island contain this element at all?

	//Calculate the require bounds to encompass the point + the per element bounds.
	FVector NewMax(FMath::Max(ElementMax.X, CurrMax.X), FMath::Max(ElementMax.Y, CurrMax.Y), FMath::Max(ElementMax.Z, CurrMax.Z));
	FVector NewMin(FMath::Min(ElementMin.X, CurrMin.X), FMath::Min(ElementMin.Y, CurrMin.Y), FMath::Min(ElementMin.Z, CurrMin.Z));

	//Return false if the new bounds are outside the max allowed for this island.
	if (NewMax.X > Max.X || NewMax.Y > Max.Y || NewMax.Z > Max.Z || NewMin.X < Min.X || NewMin.Y < Min.Y || NewMin.Z < Min.Z)
	{
		return false;
	}

	FVector OldOrigin = Bounds.Origin;
	FBox LocalBounds(NewMin - Bounds.Origin, NewMax - Bounds.Origin);
	Bounds = LocalBounds;
	Bounds.Origin += OldOrigin;

	//If new bounds are valid, grow our bounds and update the 
	FBox HandlerSystemBounds(-Bounds.BoxExtent, Bounds.BoxExtent);
	for (auto It = NiagaraSystems.CreateIterator(); It; ++It)
	{
		UNiagaraComponent* Comp = *It;
		if (IsValid(Comp))
		{
			Comp->SetSystemFixedBounds(LocalBounds);
			Comp->SetWorldLocation(Bounds.Origin);
		}
		else
		{
			It.RemoveCurrentSwap();
		}
	}

	return true;
}

void FNDCIsland::DebugDrawBounds()
{
	DrawDebugBox(Owner->GetWorld(), Bounds.Origin, Bounds.BoxExtent, FQuat::Identity, FColor::Red);
}