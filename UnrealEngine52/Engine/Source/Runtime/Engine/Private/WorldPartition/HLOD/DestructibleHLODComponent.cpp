// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/HLOD/DestructibleHLODComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/Texture2DDynamic.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "RenderingThread.h"
#include "TextureResource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DestructibleHLODComponent)

#define LOCTEXT_NAMESPACE "DestructibleHLODComponent"

DEFINE_LOG_CATEGORY_STATIC(LogHLODDestruction, Log, All);

UWorldPartitionDestructibleHLODComponent::UWorldPartitionDestructibleHLODComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsReplicatedByDefault(true);
}

const TArray<FName>& UWorldPartitionDestructibleHLODComponent::GetDestructibleActors() const
{
	return DestructibleActors;
}

void UWorldPartitionDestructibleHLODComponent::SetDestructibleActors(const TArray<FName>& InDestructibleActors)
{
	DestructibleActors = InDestructibleActors;
}

void FWorldPartitionDestructibleHLODState::Initialize(UWorldPartitionDestructibleHLODComponent* InDestructibleHLODComponent)
{
	OwnerComponent = InDestructibleHLODComponent;
	
	NumDestructibleActors = InDestructibleHLODComponent->GetDestructibleActors().Num();

	const ENetMode NetMode = OwnerComponent->GetNetMode();
	bIsClient = NetMode != ENetMode::NM_DedicatedServer;
	bIsServer = NetMode == ENetMode::NM_DedicatedServer || NetMode == ENetMode::NM_ListenServer;

	if (bIsServer)
	{
		DamagedActors.Reserve(NumDestructibleActors);
		ActorsToDamagedActorsMapping.SetNum(NumDestructibleActors);
		for (auto& Entry : ActorsToDamagedActorsMapping)
		{
			Entry = INDEX_NONE;
		}
	}
	
	if (bIsClient)
	{
		VisibilityBuffer.SetNumUninitialized(FMath::RoundUpToPowerOfTwo(NumDestructibleActors));
		FMemory::Memset(VisibilityBuffer.GetData(), FWorldPartitionDestructibleHLODDamagedActorState::MAX_HEALTH, VisibilityBuffer.Num());
	}
}

void FWorldPartitionDestructibleHLODState::SetActorHealth(int32 InActorIndex, uint8 InActorHealth)
{
	if (InActorIndex < 0 || InActorIndex >= NumDestructibleActors)
	{
		UE_LOG(LogHLODDestruction, Error, TEXT("Invalid actor index provided to SetActorHealth() (%d, max = %d)"), InActorIndex, NumDestructibleActors);
		return;
	}

	// If we are the server, replicate damage to clients
	if (IsServer())
	{
		int32 DamagedActorIdx = ActorsToDamagedActorsMapping[InActorIndex];

		// Add a new damaged actor entry
		if (DamagedActorIdx == INDEX_NONE)
		{
			DamagedActorIdx = DamagedActors.Num();
			ActorsToDamagedActorsMapping[InActorIndex] = DamagedActorIdx;
			DamagedActors.Emplace(InActorIndex);
		}

		FWorldPartitionDestructibleHLODDamagedActorState& DamagedActor = DamagedActors[DamagedActorIdx];
		check(DamagedActor.ActorIndex == InActorIndex);

		DamagedActor.ActorHealth = InActorHealth;
		MarkItemDirty(DamagedActor);
	}

	// If we are the client, directly update the visibility buffer
	if (IsClient())
	{
		VisibilityBuffer[InActorIndex] = InActorHealth;
	}
}

void FWorldPartitionDestructibleHLODState::PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize)
{
	PostReplicatedChange(AddedIndices, FinalSize);
}

void FWorldPartitionDestructibleHLODState::PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize)
{
	check(IsClient());

	for (int32 ChangedIndex : ChangedIndices)
	{
		if (DamagedActors.IsValidIndex(ChangedIndex))
		{
			const FWorldPartitionDestructibleHLODDamagedActorState& DamagedActorState = DamagedActors[ChangedIndex];
			VisibilityBuffer[DamagedActorState.ActorIndex] = DamagedActorState.ActorHealth;
		}
		else
		{
			UE_LOG(LogHLODDestruction, Error, TEXT("Invalid damaged actor index %d (num = %d)"), ChangedIndex, DamagedActors.Num());
		}
	}

	OwnerComponent->OnDestructionStateUpdated();
}

UWorldPartitionDestructibleHLODMeshComponent::UWorldPartitionDestructibleHLODMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWorldPartitionDestructibleHLODMeshComponent::GetLifetimeReplicatedProps(TArray< class FLifetimeProperty >& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Turn on push based replication for low-frequency variables. This saves the server time in determining what to replicate.
	FDoRepLifetimeParams Params;
	Params.bIsPushBased = true;

	DOREPLIFETIME_WITH_PARAMS_FAST(UWorldPartitionDestructibleHLODMeshComponent, DestructibleHLODState, Params);
}

void UWorldPartitionDestructibleHLODMeshComponent::BeginPlay()
{
	Super::BeginPlay();

	DestructibleHLODState.Initialize(this);
}

void UWorldPartitionDestructibleHLODMeshComponent::OnDestructionStateUpdated()
{
	UpdateVisibilityTexture();
}

void UWorldPartitionDestructibleHLODMeshComponent::DestroyActor(int32 ActorIndex)
{
	DamageActor(ActorIndex, 0.0f);
}

void UWorldPartitionDestructibleHLODMeshComponent::DamageActor(int32 ActorIndex, float RemainingHealthPercent)
{
	// Set percentage health as uint8 in the visibility texture
	const uint8 HealthInt = FMath::Clamp(RemainingHealthPercent, 0, 1) * FWorldPartitionDestructibleHLODDamagedActorState::MAX_HEALTH;

	DestructibleHLODState.SetActorHealth(ActorIndex, HealthInt);

	UpdateVisibilityTexture();
}

void UWorldPartitionDestructibleHLODMeshComponent::SetupVisibilityTexture()
{
	if (!VisibilityTexture && DestructibleHLODMaterial)
	{
		// Retrieve number of instance stored inside of this LOD actor
		float NumberOfInstances = 0.0f;
		if (DestructibleHLODMaterial->GetScalarParameterValue(FMaterialParameterInfo("NumInstances"), NumberOfInstances, true))
		{
			// Create dynamic texture size of (NumInstances, 1) if required
			uint32 TextureSize = FMath::TruncToInt(NumberOfInstances);

			FTexture2DDynamicCreateInfo CreateInfo;
			CreateInfo.Format = PF_G8;
			CreateInfo.Filter = TF_Nearest;
			CreateInfo.SamplerAddressMode = AM_Clamp;
			CreateInfo.bSRGB = false;

			UTexture2DDynamic* DynamicInstanceTexture = UTexture2DDynamic::Create(TextureSize, 1, CreateInfo);
			if (DynamicInstanceTexture)
			{
				VisibilityTexture = DynamicInstanceTexture;
			}

			if (VisibilityTexture)
			{
				// Create dynamic material instance if required and set it to us the dynamic texture
				UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(DestructibleHLODMaterial, this);
				MaterialInstance->SetTextureParameterValue("InstanceVisibilityTexture", VisibilityTexture);
				VisibilityMaterial = MaterialInstance;
			}

			if (VisibilityMaterial)
			{
				// For components that are referencing the destructible material, override it with our MID
				GetOwner()->ForEachComponent<UStaticMeshComponent>(false, [this](UStaticMeshComponent* SMComponent)
				{
					if (UMaterialInterface* Material = SMComponent->GetMaterial(0))
					{
						if (Material == DestructibleHLODMaterial)
						{
							SMComponent->SetMaterial(0, VisibilityMaterial);
						}
					}
				});
			}
		}
	}
}

void UWorldPartitionDestructibleHLODMeshComponent::UpdateVisibilityTexture()
{
	if (!DestructibleHLODState.IsClient() || !FApp::CanEverRender())
	{
		return;
	}

	SetupVisibilityTexture();

	if (VisibilityTexture)
	{
		FTexture2DDynamicResource* TextureResource = static_cast<FTexture2DDynamicResource*>(VisibilityTexture->GetResource());

		if (FApp::CanEverRender() && ensure(TextureResource))
		{
			ENQUEUE_RENDER_COMMAND(FUpdateHLODVisibilityTexture)(
				[TextureResource, VisibilityBuffer = DestructibleHLODState.GetVisibilityBuffer()](FRHICommandListImmediate& RHICmdList)
			{
					UpdateVisibilityTexture_RenderThread(TextureResource, VisibilityBuffer);
			});
		}
	}
}

void UWorldPartitionDestructibleHLODMeshComponent::SetDestructibleHLODMaterial(UMaterialInterface* InDestructibleMaterial)
{
	DestructibleHLODMaterial = InDestructibleMaterial;
}

void UWorldPartitionDestructibleHLODMeshComponent::UpdateVisibilityTexture_RenderThread(FTexture2DDynamicResource* TextureResource, const TArray<uint8>& VisibilityBuffer)
{
	check(IsInRenderingThread());

	FRHITexture2D* TextureRHI = TextureResource->GetTexture2DRHI();

	uint32 DestStride = 0;
	uint8* DestData = reinterpret_cast<uint8*>(RHILockTexture2D(TextureRHI, 0, RLM_WriteOnly, DestStride, false, false));

#if !(UE_BUILD_SHIPPING)
	if ((uint32)VisibilityBuffer.Num() > DestStride)
	{
		UE_LOG(LogHLODDestruction, Fatal, TEXT("UpdateVisibilityTexture_RenderThread: copy dest (%d) is smaller than source (%d). Memory will be stomped."), DestStride, VisibilityBuffer.Num());
	}
#endif

	FMemory::Memcpy(DestData, VisibilityBuffer.GetData(), FMath::Min((uint32)VisibilityBuffer.Num(), DestStride));

	RHIUnlockTexture2D(TextureRHI, 0, false, false);
}

#undef LOCTEXT_NAMESPACE
