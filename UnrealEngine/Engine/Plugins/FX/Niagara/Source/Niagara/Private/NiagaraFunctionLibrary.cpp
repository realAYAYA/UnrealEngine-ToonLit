// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraFunctionLibrary.h"
#include "EngineGlobals.h"
#include "GameFramework/WorldSettings.h"
#include "Engine/Engine.h"
#include "NiagaraComponent.h"
#include "NiagaraComponentSettings.h"
#include "NiagaraSystem.h"
#include "ContentStreaming.h"
#include "Internationalization/Internationalization.h"

#include "NiagaraAsyncGpuTraceHelper.h"
#include "NiagaraWorldManager.h"
#include "DataInterface/NiagaraDataInterfaceStaticMesh.h"
#include "NiagaraStats.h"
#include "NiagaraDataInterfaceTexture.h"
#include "NiagaraDataInterface2DArrayTexture.h"
#include "NiagaraDataInterfaceVolumeTexture.h"
#include "NiagaraDataInterfaceCubeTexture.h"
#include "Engine/VolumeTexture.h"
#include "Engine/Texture2DArray.h"
#include "Engine/TextureCube.h"
#include "Engine/TextureRenderTarget2DArray.h"
#include "Engine/TextureRenderTargetVolume.h"
#include "Engine/TextureRenderTargetCube.h"
#include "NiagaraGpuComputeDispatchInterface.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraFunctionLibrary)

#define LOCTEXT_NAMESPACE "NiagaraFunctionLibrary"

//DECLARE_CYCLE_STAT(TEXT("FastDot4"), STAT_NiagaraFastDot4, STATGROUP_Niagara);

#if WITH_EDITOR
int32 GForceNiagaraSpawnAttachedSolo = 0;
static FAutoConsoleVariableRef CVarForceNiagaraSpawnAttachedSolo(
	TEXT("fx.ForceNiagaraSpawnAttachedSolo"),
	GForceNiagaraSpawnAttachedSolo,
	TEXT("If > 0 Niagara systems which are spawned attached will be force to spawn in solo mode for debugging.\n"),
	ECVF_Default
);
#endif


int32 GAllowFastPathFunctionLibrary = 0;
static FAutoConsoleVariableRef CVarAllowFastPathFunctionLibrary(
	TEXT("fx.AllowFastPathFunctionLibrary"),
	GAllowFastPathFunctionLibrary,
	TEXT("If > 0 Allow the graph to insert custom fastpath operations into the graph.\n"),
	ECVF_Default
);

TArray<FNiagaraFunctionSignature> UNiagaraFunctionLibrary::VectorVMOps;
TArray<FString> UNiagaraFunctionLibrary::VectorVMOpsHLSL;

UNiagaraFunctionLibrary::UNiagaraFunctionLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}


UNiagaraComponent* CreateNiagaraSystem(UNiagaraSystem* SystemTemplate, UWorld* World, AActor* Actor, bool bAutoDestroy, ENCPoolMethod PoolingMethod)
{
	UNiagaraComponent* NiagaraComponent = nullptr;

	if (FApp::CanEverRender() && World && !World->IsNetMode(NM_DedicatedServer))
	{
		if (PoolingMethod == ENCPoolMethod::None)
		{
			if (UNiagaraComponentSettings::ShouldForceAutoPooling(SystemTemplate))
			{
				if (FNiagaraWorldManager* WorldManager = FNiagaraWorldManager::Get(World))
				{
					UNiagaraComponentPool* ComponentPool = WorldManager->GetComponentPool();
					NiagaraComponent = ComponentPool->CreateWorldParticleSystem(SystemTemplate, World, ENCPoolMethod::AutoRelease);
				}
			}
			else
			{
				NiagaraComponent = NewObject<UNiagaraComponent>((Actor ? Actor : (UObject*)World));
				NiagaraComponent->SetAsset(SystemTemplate);
				NiagaraComponent->bAutoActivate = false;
				NiagaraComponent->SetAutoDestroy(bAutoDestroy);
				NiagaraComponent->bAllowAnyoneToDestroyMe = true;
				NiagaraComponent->SetVisibleInRayTracing(false);
			}
		}
		else if (FNiagaraWorldManager* WorldManager = FNiagaraWorldManager::Get(World))
		{
			UNiagaraComponentPool* ComponentPool = WorldManager->GetComponentPool();
			NiagaraComponent = ComponentPool->CreateWorldParticleSystem(SystemTemplate, World, PoolingMethod);
		}
	}
	return NiagaraComponent;
}


UNiagaraComponent* UNiagaraFunctionLibrary::SpawnSystemAtLocation(const UObject* WorldContextObject, UNiagaraSystem* SystemTemplate, FVector SpawnLocation, FRotator SpawnRotation, FVector Scale, bool bAutoDestroy, bool bAutoActivate, ENCPoolMethod PoolingMethod, bool bPreCullCheck)
{
	FFXSystemSpawnParameters SpawnParams;
	SpawnParams.WorldContextObject = WorldContextObject;
	SpawnParams.SystemTemplate = SystemTemplate;
	SpawnParams.Location = SpawnLocation;
	SpawnParams.Rotation = SpawnRotation;
	SpawnParams.Scale = Scale;
	SpawnParams.bAutoDestroy = bAutoDestroy;
	SpawnParams.bAutoActivate = bAutoActivate;
	SpawnParams.PoolingMethod = ToPSCPoolMethod(PoolingMethod);
	SpawnParams.bPreCullCheck = bPreCullCheck;
	return SpawnSystemAtLocationWithParams(SpawnParams);
}

/**
* Spawns a Niagara System at the specified world location/rotation
* @return			The spawned UNiagaraComponent
*/
UNiagaraComponent* UNiagaraFunctionLibrary::SpawnSystemAtLocationWithParams(FFXSystemSpawnParameters& SpawnParams)
{
	UNiagaraComponent* PSC = NULL;
	UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(SpawnParams.SystemTemplate);
	if (NiagaraSystem)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(SpawnParams.WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
		if (World != nullptr)
		{
			bool bShouldCull = false;
			if (SpawnParams.bPreCullCheck)
			{
				bool bIsPlayerEffect = SpawnParams.bIsPlayerEffect;
				if (NiagaraSystem->AllowCullingForLocalPlayers() || bIsPlayerEffect == false)
				{
					FNiagaraWorldManager* WorldManager = FNiagaraWorldManager::Get(World);
					bShouldCull = WorldManager->ShouldPreCull(NiagaraSystem, SpawnParams.Location);
				}
			}

			if (!bShouldCull)
			{
				PSC = CreateNiagaraSystem(NiagaraSystem, World, World->GetWorldSettings(), SpawnParams.bAutoDestroy, ToNiagaraPooling(SpawnParams.PoolingMethod));
				if(PSC)
				{
					if (SpawnParams.bIsPlayerEffect)
					{
						PSC->SetForceLocalPlayerEffect(true);
					}

#if WITH_EDITORONLY_DATA
					PSC->bWaitForCompilationOnActivate = GIsAutomationTesting;
#endif

					if (!PSC->IsRegistered())
					{
						PSC->RegisterComponentWithWorld(World);
					}

					PSC->SetAbsolute(true, true, true);
					PSC->SetWorldLocationAndRotation(SpawnParams.Location, SpawnParams.Rotation);
					PSC->SetRelativeScale3D(SpawnParams.Scale);
					if (SpawnParams.bAutoActivate)
					{
						PSC->Activate(true);
					}
				}
			}
		}
	}
	return PSC;
}


/**
* Spawns a Niagara System attached to a component
* @return			The spawned UNiagaraComponent
*/
UNiagaraComponent* UNiagaraFunctionLibrary::SpawnSystemAttached(UNiagaraSystem* SystemTemplate, USceneComponent* AttachToComponent, FName AttachPointName, FVector Location, FRotator Rotation, EAttachLocation::Type LocationType, bool bAutoDestroy, bool bAutoActivate, ENCPoolMethod PoolingMethod, bool bPreCullCheck)
{
	return SpawnSystemAttached(SystemTemplate, AttachToComponent, AttachPointName, Location, Rotation, FVector(1, 1, 1), LocationType, bAutoDestroy, PoolingMethod, bAutoActivate, bPreCullCheck);
}

UNiagaraComponent* UNiagaraFunctionLibrary::SpawnSystemAttached(
	UNiagaraSystem* SystemTemplate,
	USceneComponent* AttachToComponent,
	FName AttachPointName,
	FVector Location,
	FRotator Rotation,
	FVector Scale,
	EAttachLocation::Type LocationType,
	bool bAutoDestroy,
	ENCPoolMethod PoolingMethod,
	bool bAutoActivate,
	bool bPreCullCheck
)
{
	FFXSystemSpawnParameters SpawnParams;
	SpawnParams.SystemTemplate = SystemTemplate;
	SpawnParams.AttachToComponent = AttachToComponent;
	SpawnParams.AttachPointName = AttachPointName;
	SpawnParams.Location = Location;
	SpawnParams.Rotation = Rotation;
	SpawnParams.Scale = Scale;
	SpawnParams.LocationType = LocationType;
	SpawnParams.bAutoDestroy = bAutoDestroy;
	SpawnParams.bAutoActivate = bAutoActivate;
	SpawnParams.PoolingMethod = ToPSCPoolMethod(PoolingMethod);
	SpawnParams.bPreCullCheck = bPreCullCheck;
	return SpawnSystemAttachedWithParams(SpawnParams);
}

/**
* Spawns a Niagara System attached to a component
* @return			The spawned UNiagaraComponent
*/

UNiagaraComponent* UNiagaraFunctionLibrary::SpawnSystemAttachedWithParams(FFXSystemSpawnParameters& SpawnParams)
{
	LLM_SCOPE(ELLMTag::Niagara);
	
	UNiagaraComponent* PSC = nullptr;
	UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(SpawnParams.SystemTemplate);
	if (NiagaraSystem)
	{
		if (!SpawnParams.AttachToComponent)
		{
			UE_LOG(LogScript, Warning, TEXT("UGameplayStatics::SpawnNiagaraEmitterAttached: NULL AttachComponent specified!"));
		}
		else
		{
			UWorld* const World = SpawnParams.AttachToComponent->GetWorld();
			if (World && !World->IsNetMode(NM_DedicatedServer))
			{
				bool bShouldCull = false;
				if (SpawnParams.bPreCullCheck)
				{
					//Don't precull if this is a local player linked effect and the system doesn't allow us to cull those.
					bool bIsPlayerEffect = SpawnParams.bIsPlayerEffect || FNiagaraWorldManager::IsComponentLocalPlayerLinked(SpawnParams.AttachToComponent);
					if (NiagaraSystem->AllowCullingForLocalPlayers() || bIsPlayerEffect == false)
					{
						FNiagaraWorldManager* WorldManager = FNiagaraWorldManager::Get(World);
						//TODO: For now using the attach parent location and ignoring the emitters relative location which is clearly going to be a bit wrong in some cases.
						bShouldCull = WorldManager->ShouldPreCull(NiagaraSystem, SpawnParams.AttachToComponent->GetComponentLocation());
					}
				}

				if (!bShouldCull)
				{
					PSC = CreateNiagaraSystem(NiagaraSystem, World, SpawnParams.AttachToComponent->GetOwner(), SpawnParams.bAutoDestroy, ToNiagaraPooling(SpawnParams.PoolingMethod));
					if (PSC)
					{
						if (SpawnParams.bIsPlayerEffect)
						{
							PSC->SetForceLocalPlayerEffect(true);
						}
						
#if WITH_EDITOR
						if (GForceNiagaraSpawnAttachedSolo > 0)
						{
							PSC->SetForceSolo(true);
						}
#endif
						auto SetupRelativeTransforms = [&]()
						{
							if (SpawnParams.LocationType == EAttachLocation::KeepWorldPosition)
							{
								const FTransform ParentToWorld = SpawnParams.AttachToComponent->GetSocketTransform(SpawnParams.AttachPointName);
								const FTransform ComponentToWorld(SpawnParams.Rotation, SpawnParams.Location, SpawnParams.Scale);
								const FTransform RelativeTM = ComponentToWorld.GetRelativeTransform(ParentToWorld);
								PSC->SetRelativeLocation_Direct(RelativeTM.GetLocation());
								PSC->SetRelativeRotation_Direct(RelativeTM.GetRotation().Rotator());
								PSC->SetRelativeScale3D_Direct(RelativeTM.GetScale3D());
							}
							else
							{
								PSC->SetRelativeLocation_Direct(SpawnParams.Location);
								PSC->SetRelativeRotation_Direct(SpawnParams.Rotation);

								if (SpawnParams.LocationType == EAttachLocation::SnapToTarget)
								{
									// SnapToTarget indicates we "keep world scale", this indicates we we want the inverse of the parent-to-world scale 
									// to calculate world scale at Scale 1, and then apply the passed in Scale
									const FTransform ParentToWorld = SpawnParams.AttachToComponent->GetSocketTransform(SpawnParams.AttachPointName);
									PSC->SetRelativeScale3D_Direct(SpawnParams.Scale * ParentToWorld.GetSafeScaleReciprocal(ParentToWorld.GetScale3D()));
								}
								else
								{
									PSC->SetRelativeScale3D_Direct(SpawnParams.Scale);
								}
							}
						};

						if (PSC->IsRegistered())
						{
							//It is now possible for us to be already regisetered so we must use AttachToComponent() instead.
							SetupRelativeTransforms();
							PSC->AttachToComponent(SpawnParams.AttachToComponent, FAttachmentTransformRules::KeepRelativeTransform, SpawnParams.AttachPointName);
						}
						else
						{
							PSC->SetupAttachment(SpawnParams.AttachToComponent, SpawnParams.AttachPointName);
							SetupRelativeTransforms();
							PSC->RegisterComponentWithWorld(World);
						}

						if (SpawnParams.bAutoActivate)
						{
							PSC->Activate(true);
						}

						// Notify the texture streamer so that PSC gets managed as a dynamic component.
						IStreamingManager::Get().NotifyPrimitiveUpdated(PSC);
					}
				}
			}
		}
	}
	return PSC;
}

/**
* Set a constant in an emitter of a Niagara System
void UNiagaraFunctionLibrary::SetUpdateScriptConstant(UNiagaraComponent* Component, FName EmitterName, FName ConstantName, FVector Value)
{
	TArray<TSharedPtr<FNiagaraEmitterInstance>> &Emitters = Component->GetSystemInstance()->GetEmitters();

	for (TSharedPtr<FNiagaraEmitterInstance> &Emitter : Emitters)
	{		
		if(UNiagaraEmitter* PinnedProps = Emitter->GetProperties().Get())
		{
			FName CurName = *PinnedProps->EmitterName;
			if (CurName == EmitterName)
			{
				Emitter->GetProperties()->UpdateScriptProps.ExternalConstants.SetOrAdd(FNiagaraTypeDefinition::GetVec4Def(), ConstantName, Value);
				break;
			}
		}
	}
}
*/

void UNiagaraFunctionLibrary::OverrideSystemUserVariableStaticMeshComponent(UNiagaraComponent* NiagaraSystem, const FString& OverrideName, UStaticMeshComponent* StaticMeshComponent)
{
	if (!NiagaraSystem)
	{
		if (FNiagaraUtilities::LogVerboseWarnings())
		{
			UE_LOG(LogNiagara, Warning, TEXT("NiagaraSystem in \"Set Niagara Static Mesh Component\" is NULL, OverrideName \"%s\" and StaticMeshComponent \"%s\", skipping."), *OverrideName, *GetFullNameSafe(StaticMeshComponent));
		}
		return;
	}

	if (!StaticMeshComponent)
	{
		UE_LOG(LogNiagara, Warning, TEXT("StaticMeshComponent in \"Set Niagara Static Mesh Component\" is NULL, OverrideName \"%s\" and NiagaraSystem \"%s\", skipping."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}

	const FNiagaraParameterStore& OverrideParameters = NiagaraSystem->GetOverrideParameters();

	FNiagaraVariable Variable(FNiagaraTypeDefinition(UNiagaraDataInterfaceStaticMesh::StaticClass()), *OverrideName);
	
	int32 Index = OverrideParameters.IndexOf(Variable);
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Could not find index of variable \"%s\" in the OverrideParameters map of NiagaraSystem \"%s\"."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}
	
	UNiagaraDataInterfaceStaticMesh* StaticMeshInterface = Cast<UNiagaraDataInterfaceStaticMesh>(OverrideParameters.GetDataInterface(Index));
	if (!StaticMeshInterface)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Did not find a matching Static Mesh Data Interface variable named \"%s\" in the User variables of NiagaraSystem \"%s\" ."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}

	StaticMeshInterface->SetSourceComponentFromBlueprints(StaticMeshComponent);
}

void UNiagaraFunctionLibrary::OverrideSystemUserVariableStaticMesh(UNiagaraComponent* NiagaraSystem, const FString& OverrideName, UStaticMesh* StaticMesh)
{
	if (!NiagaraSystem)
	{
		if (FNiagaraUtilities::LogVerboseWarnings())
		{
			UE_LOG(LogNiagara, Warning, TEXT("NiagaraSystem in \"Set Niagara Static Mesh Component\" is NULL, OverrideName \"%s\" and StaticMesh \"%s\", skipping."), *OverrideName, *GetFullNameSafe(StaticMesh));
		}
		return;
	}

	if (!StaticMesh)
	{
		UE_LOG(LogNiagara, Warning, TEXT("StaticMesh in \"Set Niagara Static Mesh Component\" is NULL, OverrideName \"%s\" and NiagaraSystem \"%s\", skipping."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}

	const FNiagaraParameterStore& OverrideParameters = NiagaraSystem->GetOverrideParameters();

	FNiagaraVariable Variable(FNiagaraTypeDefinition(UNiagaraDataInterfaceStaticMesh::StaticClass()), *OverrideName);

	int32 Index = OverrideParameters.IndexOf(Variable);
	if (Index == INDEX_NONE)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Could not find index of variable \"%s\" in the OverrideParameters map of NiagaraSystem \"%s\"."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}

	UNiagaraDataInterfaceStaticMesh* StaticMeshInterface = Cast<UNiagaraDataInterfaceStaticMesh>(OverrideParameters.GetDataInterface(Index));
	if (!StaticMeshInterface)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Did not find a matching Static Mesh Data Interface variable named \"%s\" in the User variables of NiagaraSystem \"%s\" ."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}

	StaticMeshInterface->SetDefaultMeshFromBlueprints(StaticMesh);
}

UNiagaraDataInterfaceSkeletalMesh* UNiagaraFunctionLibrary::GetSkeletalMeshDataInterface(UNiagaraComponent* NiagaraSystem, const FString& OverrideName)
{
	if (!NiagaraSystem)
	{
		return nullptr;
	}

	const FNiagaraParameterStore& OverrideParameters = NiagaraSystem->GetOverrideParameters();
	FNiagaraVariable Variable(FNiagaraTypeDefinition(UNiagaraDataInterfaceSkeletalMesh::StaticClass()), *OverrideName);

	const int32 Index = OverrideParameters.IndexOf(Variable);
	return Index != INDEX_NONE ? Cast<UNiagaraDataInterfaceSkeletalMesh>(OverrideParameters.GetDataInterface(Index)) : nullptr;
}

void UNiagaraFunctionLibrary::OverrideSystemUserVariableSkeletalMeshComponent(UNiagaraComponent* NiagaraSystem, const FString& OverrideName, USkeletalMeshComponent* SkeletalMeshComponent)
{
	if (!NiagaraSystem)
	{
		if (FNiagaraUtilities::LogVerboseWarnings())
		{
			UE_LOG(LogNiagara, Warning, TEXT("NiagaraSystem in \"Set Niagara Skeletal Mesh Component\" is NULL, OverrideName \"%s\" and SkeletalMeshComponent \"%s\" SkeletalMesh \"%s\", skipping."), *OverrideName, *GetFullNameSafe(SkeletalMeshComponent), *GetFullNameSafe(SkeletalMeshComponent ? SkeletalMeshComponent->GetSkeletalMeshAsset() : nullptr));
		}
		return;
	}

	if (!SkeletalMeshComponent)
	{
		UE_LOG(LogNiagara, Warning, TEXT("SkeletalMeshComponent in \"Set Niagara Skeletal Mesh Component\" is NULL, OverrideName \"%s\" and NiagaraSystem \"%s\", skipping."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}

	UNiagaraDataInterfaceSkeletalMesh* SkeletalMeshInterface = GetSkeletalMeshDataInterface(NiagaraSystem, OverrideName);
	if (!SkeletalMeshInterface)
	{
		UE_LOG(LogNiagara, Warning, TEXT("UNiagaraFunctionLibrary::OverrideSystemUserVariableSkeletalMeshComponent: Did not find a matching Skeletal Mesh Data Interface variable named \"%s\" in the User variables of NiagaraSystem \"%s\" ."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}

	SkeletalMeshInterface->SetSourceComponentFromBlueprints(SkeletalMeshComponent);
}

void UNiagaraFunctionLibrary::SetSkeletalMeshDataInterfaceSamplingRegions(UNiagaraComponent* NiagaraSystem, const FString& OverrideName, const TArray<FName>& SamplingRegions)
{
	if (!NiagaraSystem)
	{
		if (FNiagaraUtilities::LogVerboseWarnings())
		{
			UE_LOG(LogNiagara, Warning, TEXT("NiagaraSystem in \"Set Skeletal Mesh Data Interface Sampling Regions\" is NULL, OverrideName \"%s\", skipping."), *OverrideName);
		}
		return;
	}

	UNiagaraDataInterfaceSkeletalMesh* SkeletalMeshInterface = GetSkeletalMeshDataInterface(NiagaraSystem, OverrideName);
	if (!SkeletalMeshInterface)
	{
		UE_LOG(LogNiagara, Warning, TEXT("UNiagaraFunctionLibrary::SetSkeletalMeshDataInterfaceSamplingRegions: Did not find a matching Skeletal Mesh Data Interface variable named \"%s\" in the User variables of NiagaraSystem \"%s\" ."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}

	SkeletalMeshInterface->SetSamplingRegionsFromBlueprints(SamplingRegions);
}


void UNiagaraFunctionLibrary::SetSkeletalMeshDataInterfaceFilteredBones(UNiagaraComponent* NiagaraSystem, const FString& OverrideName, const TArray<FName>& FilteredBones)
{
	if (!NiagaraSystem)
	{
		if (FNiagaraUtilities::LogVerboseWarnings())
		{
			UE_LOG(LogNiagara, Warning, TEXT("NiagaraSystem in \"Set Skeletal Mesh Data Interface Filtered Bones\" is NULL, OverrideName \"%s\", skipping."), *OverrideName);
		}
		return;
	}

	UNiagaraDataInterfaceSkeletalMesh* SkeletalMeshInterface = GetSkeletalMeshDataInterface(NiagaraSystem, OverrideName);
	if (!SkeletalMeshInterface)
	{
		UE_LOG(LogNiagara, Warning, TEXT("UNiagaraFunctionLibrary::SetSkeletalMeshDataInterfaceFilteredBones: Did not find a matching Skeletal Mesh Data Interface variable named \"%s\" in the User variables of NiagaraSystem \"%s\" ."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}

	SkeletalMeshInterface->SetFilteredBonesFromBlueprints(FilteredBones);
}

void UNiagaraFunctionLibrary::SetSkeletalMeshDataInterfaceFilteredSockets(UNiagaraComponent* NiagaraSystem, const FString& OverrideName, const TArray<FName>& FilteredSockets)
{
	if (!NiagaraSystem)
	{
		if (FNiagaraUtilities::LogVerboseWarnings())
		{
			UE_LOG(LogNiagara, Warning, TEXT("NiagaraSystem in \"Set Skeletal Mesh Data Interface Filtered Sockets\" is NULL, OverrideName \"%s\", skipping."), *OverrideName);
		}
		return;
	}

	UNiagaraDataInterfaceSkeletalMesh* SkeletalMeshInterface = GetSkeletalMeshDataInterface(NiagaraSystem, OverrideName);
	if (!SkeletalMeshInterface)
	{
		UE_LOG(LogNiagara, Warning, TEXT("UNiagaraFunctionLibrary::SetSkeletalMeshDataInterfaceFilteredSockets: Did not find a matching Skeletal Mesh Data Interface variable named \"%s\" in the User variables of NiagaraSystem \"%s\" ."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}

	SkeletalMeshInterface->SetFilteredSocketsFromBlueprints(FilteredSockets);
}

void UNiagaraFunctionLibrary::SetTextureObject(UNiagaraComponent* NiagaraSystem, const FString& OverrideName, UTexture* Texture)
{
	if (!NiagaraSystem)
	{
		if (FNiagaraUtilities::LogVerboseWarnings())
		{
			UE_LOG(LogNiagara, Warning, TEXT("NiagaraSystem in \"SetTextureObject\" is NULL, OverrideName \"%s\" and Texture \"%s\", skipping."), *OverrideName, *GetFullNameSafe(Texture));
		}
		return;
	}

	if (!Texture)
	{
		UE_LOG(LogNiagara, Warning, TEXT("Texture in \"SetTextureObject\" is NULL, OverrideName \"%s\" and NiagaraSystem \"%s\", skipping."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
		return;
	}

	const FNiagaraParameterStore& OverrideParameters = NiagaraSystem->GetOverrideParameters();

	if (Texture->IsA<UTexture2D>() || Texture->IsA<UTextureRenderTarget2D>())
	{
		const FNiagaraVariable Variable(FNiagaraTypeDefinition(UNiagaraDataInterfaceTexture::StaticClass()), *OverrideName);
		const int32 Index = OverrideParameters.IndexOf(Variable);
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Could not find index of variable \"%s\" in the OverrideParameters map of NiagaraSystem \"%s\"."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
			return;
		}

		UNiagaraDataInterfaceTexture* TextureDI = Cast<UNiagaraDataInterfaceTexture>(OverrideParameters.GetDataInterface(Index));
		if (!TextureDI)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Did not find a matching Texture Data Interface variable named \"%s\" in the User variables of NiagaraSystem \"%s\" ."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
			return;
		}

		TextureDI->SetTexture(Texture);
	}
	else if (Texture->IsA<UTexture2DArray>() || Texture->IsA<UTextureRenderTarget2DArray>())
	{
		const FNiagaraVariable Variable(FNiagaraTypeDefinition(UNiagaraDataInterface2DArrayTexture::StaticClass()), *OverrideName);
		const int32 Index = OverrideParameters.IndexOf(Variable);
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Could not find index of variable \"%s\" in the OverrideParameters map of NiagaraSystem \"%s\"."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
			return;
		}

		UNiagaraDataInterface2DArrayTexture* TextureDI = Cast<UNiagaraDataInterface2DArrayTexture>(OverrideParameters.GetDataInterface(Index));
		if (!TextureDI)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Did not find a matching Texture Data Interface variable named \"%s\" in the User variables of NiagaraSystem \"%s\" ."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
			return;
		}

		TextureDI->SetTexture(Texture);
	}
	else if (Texture->IsA<UVolumeTexture>() || Texture->IsA<UTextureRenderTargetVolume>())
	{
		const FNiagaraVariable Variable(FNiagaraTypeDefinition(UNiagaraDataInterfaceVolumeTexture::StaticClass()), *OverrideName);
		const int32 Index = OverrideParameters.IndexOf(Variable);
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Could not find index of variable \"%s\" in the OverrideParameters map of NiagaraSystem \"%s\"."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
			return;
		}

		UNiagaraDataInterfaceVolumeTexture* TextureDI = Cast<UNiagaraDataInterfaceVolumeTexture>(OverrideParameters.GetDataInterface(Index));
		if (!TextureDI)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Did not find a matching Texture Data Interface variable named \"%s\" in the User variables of NiagaraSystem \"%s\" ."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
			return;
		}

		TextureDI->SetTexture(Texture);
	}
	else if (Texture->IsA<UTextureCube>() || Texture->IsA<UTextureRenderTargetCube>())
	{
		const FNiagaraVariable Variable(FNiagaraTypeDefinition(UNiagaraDataInterfaceCubeTexture::StaticClass()), *OverrideName);
		const int32 Index = OverrideParameters.IndexOf(Variable);
		if (Index == INDEX_NONE)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Could not find index of variable \"%s\" in the OverrideParameters map of NiagaraSystem \"%s\"."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
			return;
		}

		UNiagaraDataInterfaceCubeTexture* TextureDI = Cast<UNiagaraDataInterfaceCubeTexture>(OverrideParameters.GetDataInterface(Index));
		if (!TextureDI)
		{
			UE_LOG(LogNiagara, Warning, TEXT("Did not find a matching Texture Data Interface variable named \"%s\" in the User variables of NiagaraSystem \"%s\" ."), *OverrideName, *GetFullNameSafe(NiagaraSystem));
			return;
		}

		TextureDI->SetTexture(Texture);
	}
	else
	{
		UE_LOG(LogNiagara, Warning, TEXT("Texture in \"SetTextureObject\" is of an unsupported type \"%s\", OverrideName \"%s\" and NiagaraSystem \"%s\", skipping."), *OverrideName, *GetNameSafe(Texture->GetClass()), *GetFullNameSafe(NiagaraSystem));
		return;
	}
}

void UNiagaraFunctionLibrary::SetTexture2DArrayObject(UNiagaraComponent* NiagaraSystem, const FString& OverrideName, UTexture2DArray* Texture)
	{
	SetTextureObject(NiagaraSystem, OverrideName, Texture);
	}

void UNiagaraFunctionLibrary::SetVolumeTextureObject(UNiagaraComponent* NiagaraSystem, const FString& OverrideName, UVolumeTexture* Texture)
{
	SetTextureObject(NiagaraSystem, OverrideName, Texture);
}

UNiagaraDataInterface* UNiagaraFunctionLibrary::GetDataInterface(UClass* DIClass, UNiagaraComponent* NiagaraSystem, FName OverrideName)
{
	if (NiagaraSystem == nullptr)
	{
		if (FNiagaraUtilities::LogVerboseWarnings())
		{
			UE_LOG(LogNiagara, Warning, TEXT("NiagaraSystem was nullptr for OverrideName \"%s\"."), *OverrideName.ToString());
		}
		return nullptr;
	}

	const FNiagaraParameterStore& OverrideParameters = NiagaraSystem->GetOverrideParameters();
	FNiagaraVariableBase Variable(FNiagaraTypeDefinition(DIClass), OverrideName);
	const int32* Index = OverrideParameters.FindParameterOffset(Variable, true);
	if (Index == nullptr)
	{
		UE_LOG(LogNiagara, Warning, TEXT("OverrideParameter\"%s\" DataInterface \"%s\" was not found"), *OverrideName.ToString(), *GetNameSafe(DIClass));
		return nullptr;
	}

	UNiagaraDataInterface* UntypedDI = OverrideParameters.GetDataInterface(*Index);
	if (UntypedDI == nullptr)
	{
		UE_LOG(LogNiagara, Warning, TEXT("OverrideParameter\"%s\" DataInterface is nullptr."), *OverrideName.ToString());
		return nullptr;
	}

	if (UntypedDI->IsA(DIClass))
	{
		return UntypedDI;
	}
	UE_LOG(LogNiagara, Warning, TEXT("OverrideParameter\"%s\" DataInterface is a \"%s\" and not expected type \"%s\""), *OverrideName.ToString(), *GetNameSafe(UntypedDI->GetClass()), *GetNameSafe(DIClass));
	return nullptr;
}

UNiagaraParameterCollectionInstance* UNiagaraFunctionLibrary::GetNiagaraParameterCollection(UObject* WorldContextObject, UNiagaraParameterCollection* Collection)
{
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World != nullptr)
	{
		return FNiagaraWorldManager::Get(World)->GetParameterCollection(Collection);
	}
	return nullptr;
}


const TArray<FNiagaraFunctionSignature>& UNiagaraFunctionLibrary::GetVectorVMFastPathOps(bool bIgnoreConsoleVariable)
{
	if (!bIgnoreConsoleVariable && (GAllowFastPathFunctionLibrary == 0))
	{
		static TArray<FNiagaraFunctionSignature> Empty;
		return Empty;
	}

	InitVectorVMFastPathOps();
	return VectorVMOps;
}

bool UNiagaraFunctionLibrary::DefineFunctionHLSL(const FNiagaraFunctionSignature& FunctionSignature, FString& HlslOutput)
{
	InitVectorVMFastPathOps();

	const int32 i = VectorVMOps.IndexOfByKey(FunctionSignature);
	if ( i == INDEX_NONE )
	{
		return false;
	}

	HlslOutput += VectorVMOpsHLSL[i];
	return true;
}

const FName FastPathLibraryName(TEXT("FastPathLibrary"));
const FName FastPathDot4Name(TEXT("FastPathDot4"));
const FName FastPathTransformPositionName(TEXT("FastPathTransformPosition"));
const FName FastMatrixToQuaternionName(TEXT("FastMatrixToQuaternion"));
const FName FastPathEmitterLifeCycleName(TEXT("FastPathEmitterLifeCycle"));
const FName FastPathSpawnRateName(TEXT("FastPathSpawnRate"));
const FName FastPathSpawnBurstInstantaneousName(TEXT("FastPathSpawnBurstInstantaneous"));
const FName FastPathSolveVelocitiesAndForces(TEXT("FastPathSolveVelocitiesAndForces"));

struct FVectorKernelFastDot4
{
	static FNiagaraFunctionSignature GetFunctionSignature()
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = FastPathDot4Name;
		Sig.OwnerName = FastPathLibraryName;
		Sig.bMemberFunction = false;
		Sig.bRequiresContext = false;
		Sig.SetDescription(LOCTEXT("FastPathDot4Desc", "Fast path for Vector4 dot product."));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("A")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), TEXT("B")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Value")));
		return Sig;
	}

	static FString GetFunctionHLSL()
	{
		return FString();
	}

	static void Exec(FVectorVMExternalFunctionContext& Context)
	{
		//SCOPE_CYCLE_COUNTER(STAT_NiagaraFastDot4);

		VectorVM::FExternalFuncInputHandler<float>InVecA[4] =
		{
			VectorVM::FExternalFuncInputHandler<float>(Context), 
			VectorVM::FExternalFuncInputHandler<float>(Context), 
			VectorVM::FExternalFuncInputHandler<float>(Context),
			VectorVM::FExternalFuncInputHandler<float>(Context)
		};

		VectorVM::FExternalFuncInputHandler<float>InVecB[4] =
		{
			VectorVM::FExternalFuncInputHandler<float>(Context),
			VectorVM::FExternalFuncInputHandler<float>(Context),
			VectorVM::FExternalFuncInputHandler<float>(Context),
			VectorVM::FExternalFuncInputHandler<float>(Context)
		};


		VectorVM::FExternalFuncRegisterHandler<float> OutValue(Context);

		VectorRegister4Float* RESTRICT AX = (VectorRegister4Float *)InVecA[0].GetDest();
		VectorRegister4Float* RESTRICT AY = (VectorRegister4Float *)InVecA[1].GetDest();
		VectorRegister4Float* RESTRICT AZ = (VectorRegister4Float *)InVecA[2].GetDest();
		VectorRegister4Float* RESTRICT AW = (VectorRegister4Float *)InVecA[3].GetDest();

		VectorRegister4Float* RESTRICT BX = (VectorRegister4Float *)InVecB[0].GetDest();
		VectorRegister4Float* RESTRICT BY = (VectorRegister4Float *)InVecB[1].GetDest();
		VectorRegister4Float* RESTRICT BZ = (VectorRegister4Float *)InVecB[2].GetDest();
		VectorRegister4Float* RESTRICT BW = (VectorRegister4Float *)InVecB[3].GetDest();
		VectorRegister4Float* RESTRICT Out = (VectorRegister4Float *)OutValue.GetDest();

		const int32 Loops = Context.GetNumLoops<4>();
		for (int32 i = 0; i < Loops; ++i)
		{

			VectorRegister4Float AVX0= VectorLoadAligned(&AX[i]);
			VectorRegister4Float AVY0= VectorLoadAligned(&AY[i]);
			VectorRegister4Float AVZ0= VectorLoadAligned(&AZ[i]);
			VectorRegister4Float AVW0= VectorLoadAligned(&AW[i]);
			VectorRegister4Float BVX0= VectorLoadAligned(&BX[i]);
			VectorRegister4Float BVY0= VectorLoadAligned(&BY[i]);
			VectorRegister4Float BVZ0= VectorLoadAligned(&BZ[i]);
			VectorRegister4Float BVW0= VectorLoadAligned(&BW[i]);

			/*
				 R[19] = :mul(R[21], R[25]);
				 R[21] = :mad(R[20], R[24], R[19]);
				 R[19] = :mad(R[22], R[26], R[21]);
				 R[20] = :mad(R[23], R[27], R[19]);
			*/
			VectorRegister4Float AMBX0	= VectorMultiply(AVX0, BVX0);
			VectorRegister4Float AMBXY0= VectorMultiplyAdd(AVY0, BVY0, AMBX0);
			VectorRegister4Float AMBXYZ0= VectorMultiplyAdd(AVZ0, BVZ0, AMBXY0);
			VectorRegister4Float AMBXYZW0= VectorMultiplyAdd(AVW0, BVW0, AMBXYZ0);
			VectorStoreAligned(AMBXYZW0, &Out[i]) ;

			/*
			(float Sum = 0.0f;
			for (int32 VecIdx = 0; VecIdx < 4; VecIdx++)
			{
				Sum += InVecA[VecIdx].GetAndAdvance() * InVecB[VecIdx].GetAndAdvance();
			}

			*OutValue.GetDestAndAdvance() = Sum;
			*/
		}
	}
};

struct FVectorKernelFastTransformPosition
{
	static FNiagaraFunctionSignature GetFunctionSignature()
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = FastPathTransformPositionName;
		Sig.OwnerName = FastPathLibraryName;
		Sig.bMemberFunction = false;
		Sig.bRequiresContext = false;
		Sig.SetDescription(LOCTEXT("FastPathTransformPositionDesc", "Fast path for Matrix4 transforming a Vector3 position"));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetMatrix4Def(), TEXT("Mat")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("PositionTransformed")));
		return Sig;
	}

	static FString GetFunctionHLSL()
	{
		return FString();
	}

	static void Exec(FVectorVMExternalFunctionContext& Context)
	{
#if 0
		TArray<VectorVM::FExternalFuncInputHandler<float>, TInlineAllocator<16>> InMatrix;
		for (int i = 0; i < 16; i++)
		{
			InMatrix.Emplace(Context);
		}

		TArray<VectorVM::FExternalFuncInputHandler<float>, TInlineAllocator<16>> InVec;
		for (int i = 0; i < 3; i++)
		{
			InVec.Emplace(Context);
		}

		TArray<VectorVM::FExternalFuncInputHandler<float>, TInlineAllocator<16>> OutVec;
		for (int i = 0; i < 3; i++)
		{
			OutVec.Emplace(Context);
		}

		/*
	29	| R[19] = :mul(R[20], R[26]);
	30	| R[27] = :mul(R[21], R[26]);
	31	| R[28] = :mul(R[22], R[26]);
	32	| R[29] = :mul(R[23], R[26]);
	33	| R[26] = :mul(R[20], R[25]);
	34	| R[30] = :mul(R[21], R[25]);
	35	| R[31] = :mul(R[22], R[25]);
	36	| R[32] = :mul(R[23], R[25]);
	37	| R[25] = :mul(R[20], R[24]);
	38	| R[33] = :mul(R[21], R[24]);
	39	| R[34] = :mul(R[22], R[24]);
	40	| R[35] = :mul(R[23], R[24]);
	41	| R[24] = :add(R[26], R[25]);
	42	| R[25] = :add(R[30], R[33]);
	43	| R[26] = :add(R[31], R[34]);
	44	| R[30] = :add(R[32], R[35]);
	45	| R[31] = :add(R[19], R[24]);
	46	| R[19] = :add(R[27], R[25]);
	47	| R[24] = :add(R[28], R[26]);
	48	| R[25] = :add(R[29], R[30]);
	49	| R[26] = :add(R[20], R[31]);
	50	| R[20] = :add(R[21], R[19]);
	51	| R[19] = :add(R[22], R[24]);
		*/
		/*float* RESTRICT M00 = (float *)InMatrix[0].GetDest();
		float* RESTRICT M01 = (float *)InMatrix[1].GetDest();
		float* RESTRICT M02 = (float *)InMatrix[2].GetDest();
		float* RESTRICT M03 = (float *)InMatrix[3].GetDest();
		float* RESTRICT M10 = (float *)InMatrix[4].GetDest();
		float* RESTRICT M11 = (float *)InMatrix[5].GetDest();
		float* RESTRICT M12 = (float *)InMatrix[6].GetDest();
		float* RESTRICT M13 = (float *)InMatrix[7].GetDest(); 
		float* RESTRICT M20 = (float *)InMatrix[8].GetDest();
		float* RESTRICT M21 = (float *)InMatrix[9].GetDest();
		float* RESTRICT M22 = (float *)InMatrix[10].GetDest();
		float* RESTRICT M23 = (float *)InMatrix[11].GetDest();
		float* RESTRICT M30 = (float *)InMatrix[12].GetDest();
		float* RESTRICT M31 = (float *)InMatrix[13].GetDest();
		float* RESTRICT M32 = (float *)InMatrix[14].GetDest();
		float* RESTRICT M33 = (float *)InMatrix[15].GetDest();*/
		TArray<float* RESTRICT, TInlineAllocator<16>> InMatrixDest;
		for (int i = 0; i < 16; i++)
		{
			InMatrixDest.Emplace((float *)InMatrix[i].GetDest());
		}

		float* RESTRICT BX = (float *)InVec[0].GetDest();
		float* RESTRICT BY = (float *)InVec[1].GetDest();
		float* RESTRICT BZ = (float *)InVec[2].GetDest();
		
		float* RESTRICT OutValueX = (float *)OutVec[0].GetDest();
		float* RESTRICT OutValueY = (float *)OutVec[1].GetDest();
		float* RESTRICT OutValueZ = (float *)OutVec[2].GetDest();
		
		int32 LoopInstances = Align(Context.NumInstances, 1) /1;
		for (int32 i = 0; i < Context.NumInstances; ++i)
		{
			/*FMatrix LocalMat;
			for (int32 Row = 0; Row < 4; Row++)
			{
				for (int32 Column = 0; Column < 4; Column++)
				{
					LocalMat.M[Row][Column] = *InMatrixDest[Row * 4 + Column]++;
				}
			}*/

			for (int32 Row = 0; Row < 4; Row++)
			{
				for (int32 Column = 0; Column < 4; Column++)
				{
					*InMatrixDest[Row * 4 + Column]++;
				}
			}
			*BX++; *BY++; *BZ++;

			//FVector LocalVec(*BX++, *BY++, *BZ++);

			//FVector LocalOutVec = LocalMat.TransformPosition(LocalVec);

			//*(OutValueX)++ = LocalOutVec.X;
			//*(OutValueY)++ = LocalOutVec.Y;
			//*(OutValueZ)++ = LocalOutVec.Z;
		}
#endif
	}
};

struct FVectorKernelFastMatrixToQuaternion
{
	static FNiagaraFunctionSignature GetFunctionSignature()
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = FastMatrixToQuaternionName;
		Sig.OwnerName = FastPathLibraryName;
		Sig.bMemberFunction = false;
		Sig.bRequiresContext = false;
		Sig.SetDescription(LOCTEXT("FastMatrixToQuaternionDesc", "Fast path for Matrix4 to Quaternion"));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetMatrix4Def(), TEXT("Mat")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetQuatDef(), TEXT("Quat")));
		return Sig;
	}

	static FString GetFunctionHLSL()
	{
		static FString FunctionHLSL = TEXT(R"(
	void FastMatrixToQuaternion_FastPathLibrary(float4x4 Mat, out float4 Quat)
	{
		float tr = Mat[0][0] + Mat[1][1] + Mat[2][2];
		if (tr > 0.0f)
		{
			float InvS = rsqrt(tr + 1.f);
			float s = 0.5f * InvS;

			Quat.x = (Mat[1][2] - Mat[2][1]) * s;
			Quat.y = (Mat[2][0] - Mat[0][2]) * s;
			Quat.z = (Mat[0][1] - Mat[1][0]) * s;
			Quat.w = 0.5f * rcp(InvS);
		}
		else if ( (Mat[0][0] > Mat[1][1]) && (Mat[0][0] > Mat[2][2]) )
		{
			float s = Mat[0][0] - Mat[1][1] - Mat[2][2] + 1.0f;
			float InvS = rsqrt(s);
			s = 0.5f * InvS;

			Quat.x = 0.5f * rcp(InvS);
			Quat.y = (Mat[0][1] + Mat[1][0]) * s;
			Quat.z = (Mat[0][2] + Mat[2][0]) * s;
			Quat.w = (Mat[1][2] - Mat[2][1]) * s;
		}
		else if ( Mat[1][1] > Mat[2][2] )
		{
			float s = Mat[1][1] - Mat[2][2] - Mat[0][0] + 1.0f;
			float InvS = rsqrt(s);
			s = 0.5f * InvS;

			Quat.x = (Mat[1][0] + Mat[0][1]) * s;
			Quat.y = 0.5f * rcp(InvS);
			Quat.z = (Mat[1][2] + Mat[2][1]) * s;
			Quat.w = (Mat[2][0] - Mat[0][2]) * s;

		}
		else
		{
			float s = Mat[2][2] - Mat[0][0] - Mat[1][1] + 1.0f;
			float InvS = rsqrt(s);
			s = 0.5f * InvS;

			Quat.x = (Mat[2][0] + Mat[0][2]) * s;
			Quat.y = (Mat[2][1] + Mat[1][2]) * s;
			Quat.z = 0.5f * rcp(InvS);
			Quat.w = (Mat[0][1] - Mat[1][0]) * s;
		}
	}
)");

		return FunctionHLSL;
	}

	static void Exec(FVectorVMExternalFunctionContext& Context)
	{
		TArray<VectorVM::FExternalFuncInputHandler<float>, TInlineAllocator<16>> InMatrix;
		for (int i=0; i < 16; i++)
		{
			InMatrix.Emplace(Context);
		}

		TArray<VectorVM::FExternalFuncRegisterHandler<float>, TInlineAllocator<4>> OutQuat;
		for (int i=0; i < 4; ++i)
		{
			OutQuat.Emplace(Context);
		}

		for ( int32 iInstance=0; iInstance < Context.GetNumInstances(); ++iInstance)
		{
			// Read Matrix
			FMatrix Mat;
			for ( int32 j=0; j < 16; ++j )
			{
				Mat.M[j >> 2][j & 3] = InMatrix[j].GetAndAdvance();
			}

			// Generate Quat (without checking for consistency with the GPU)
			FQuat Quat;
			{
				// Check diagonal (trace)
				const float tr = Mat.M[0][0] + Mat.M[1][1] + Mat.M[2][2];
				if (tr > 0.0f)
				{
					float InvS = FMath::InvSqrt(tr + 1.f);
					Quat.W = 0.5f * (1.f / InvS);
					const float s = 0.5f * InvS;

					Quat.X = (Mat.M[1][2] - Mat.M[2][1]) * s;
					Quat.Y = (Mat.M[2][0] - Mat.M[0][2]) * s;
					Quat.Z = (Mat.M[0][1] - Mat.M[1][0]) * s;
				}
				else
				{
					// diagonal is negative
					int32 i = 0;

					if (Mat.M[1][1] > Mat.M[0][0])
						i = 1;

					if (Mat.M[2][2] > Mat.M[i][i])
						i = 2;

					static const int32 nxt[3] = { 1, 2, 0 };
					const int32 j = nxt[i];
					const int32 k = nxt[j];

					float s = Mat.M[i][i] - Mat.M[j][j] - Mat.M[k][k] + 1.0f;

					float InvS = FMath::InvSqrt(s);

					float qt[4];
					qt[i] = 0.5f * (1.f / InvS);

					s = 0.5f * InvS;

					qt[3] = (Mat.M[j][k] - Mat.M[k][j]) * s;
					qt[j] = (Mat.M[i][j] + Mat.M[j][i]) * s;
					qt[k] = (Mat.M[i][k] + Mat.M[k][i]) * s;

					Quat.X = qt[0];
					Quat.Y = qt[1];
					Quat.Z = qt[2];
					Quat.W = qt[3];
				}
			}

			*OutQuat[0].GetDestAndAdvance() = Quat.X;
			*OutQuat[1].GetDestAndAdvance() = Quat.Y;
			*OutQuat[2].GetDestAndAdvance() = Quat.Z;
			*OutQuat[3].GetDestAndAdvance() = Quat.W;
		}
	}
};

struct FVectorKernel_EmitterLifeCycle
{
	static FNiagaraFunctionSignature GetFunctionSignature()
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = FastPathEmitterLifeCycleName;
		Sig.OwnerName = FastPathLibraryName;
		Sig.bMemberFunction = false;
		Sig.bRequiresContext = false;
		Sig.SetDescription(LOCTEXT("FastPathEmitterLifeCycleDesc", "Fast path for life cycle"));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EngineDeltaTime")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("EngineNumParticles")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetExecutionStateEnum(), TEXT("ScalabilityEmitterExecutionState")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetExecutionStateEnum(), TEXT("SystemExecutionState")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetExecutionStateSouceEnum(), TEXT("SystemExecutionStateSource")));

		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("ModuleNextLoopDuration")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("ModuleNextLoopDelay")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("ModuleDurationRecalcEachLoop")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("ModuleDelayFirstLoopOnly")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ModuleMaxLoopCount")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("ModuleAutoComplete")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("ModuleCompleteOnInactive")));

		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetExecutionStateEnum(), TEXT("EmitterExecutionState")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetExecutionStateSouceEnum(), TEXT("EmitterExecutionStateSource")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterAge")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterLoopedAge")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterCurrentLoopDuration")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterCurrentLoopDelay")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("EmitterLoopCount")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterNormalizedLoopAge")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetExecutionStateEnum(), TEXT("EmitterExecutionState")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetExecutionStateSouceEnum(), TEXT("EmitterExecutionStateSource")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterAge")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterLoopedAge")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterCurrentLoopDuration")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterCurrentLoopDelay")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("EmitterLoopCount")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterNormalizedLoopAge")));

		return Sig;
	}

	static FString GetFunctionHLSL()
	{
		return FString();
	}

	static void Exec(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FExternalFuncInputHandler<float> InEngineDeltaTime(Context);
		VectorVM::FExternalFuncInputHandler<int>   InEngineNumParticles(Context);
		VectorVM::FExternalFuncInputHandler<ENiagaraExecutionState> InScalabilityEmitterExecutionState(Context);
		VectorVM::FExternalFuncInputHandler<ENiagaraExecutionState> InSystemExecutionState(Context);
		VectorVM::FExternalFuncInputHandler<ENiagaraExecutionStateSource> InSystemExecutionStateSource(Context);

		VectorVM::FExternalFuncInputHandler<float> InModuleNextLoopDuration(Context);
		VectorVM::FExternalFuncInputHandler<float> InModuleNextLoopDelay(Context);
		VectorVM::FExternalFuncInputHandler<bool>  InModuleDurationRecalcEachLoop(Context);
		VectorVM::FExternalFuncInputHandler<bool>  InModuleDelayFirstLoopOnly(Context);
		VectorVM::FExternalFuncInputHandler<int>   InModuleMaxLoopCount(Context);
		VectorVM::FExternalFuncInputHandler<bool>  InModuleAutoComplete(Context);
		VectorVM::FExternalFuncInputHandler<bool>  InModuleCompleteOnInactive(Context);

		VectorVM::FExternalFuncInputHandler<ENiagaraExecutionState> InEmitterExecutionState(Context);
		VectorVM::FExternalFuncInputHandler<ENiagaraExecutionStateSource> InEmitterExecutionStateSource(Context);
		VectorVM::FExternalFuncInputHandler<float> InEmitterAge(Context);
		VectorVM::FExternalFuncInputHandler<float> InEmitterLoopedAge(Context);
		VectorVM::FExternalFuncInputHandler<float> InEmitterCurrentLoopDuration(Context);
		VectorVM::FExternalFuncInputHandler<float> InEmitterCurrentLoopDelay(Context);
		VectorVM::FExternalFuncInputHandler<int>   InEmitterLoopCount(Context);
		VectorVM::FExternalFuncInputHandler<float> InEmitterNormalizedLoopAge(Context);

		VectorVM::FExternalFuncRegisterHandler<ENiagaraExecutionState> OutEmitterExecutionState(Context);
		VectorVM::FExternalFuncRegisterHandler<ENiagaraExecutionStateSource> OutEmitterExecutionStateSource(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutEmitterAge(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutEmitterLoopedAge(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutEmitterCurrentLoopDuration(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutEmitterCurrentLoopDelay(Context);
		VectorVM::FExternalFuncRegisterHandler<int>   OutEmitterLoopCount(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutEmitterNormalizedLoopAge(Context);

		for (int i = 0; i < Context.GetNumInstances(); ++i)
		{
			const float EngineDeltaTime = InEngineDeltaTime.GetAndAdvance();
			const int EngineNumParticles = InEngineNumParticles.GetAndAdvance();

			const ENiagaraExecutionState ScalabilityEmitterExecutionState = InScalabilityEmitterExecutionState.GetAndAdvance();

			const ENiagaraExecutionState SystemExecutionState = InSystemExecutionState.GetAndAdvance();
			const ENiagaraExecutionStateSource SystemExecutionStateSource = InSystemExecutionStateSource.GetAndAdvance();

			const float ModuleNextLoopDuration = InModuleNextLoopDuration.GetAndAdvance();
			const float ModuleNextLoopDelay = InModuleNextLoopDelay.GetAndAdvance();
			const bool ModuleDurationRecalcEachLoop = InModuleDurationRecalcEachLoop.GetAndAdvance();
			const bool ModuleDelayFirstLoopOnly = InModuleDelayFirstLoopOnly.GetAndAdvance();
			const int ModuleMaxLoopCount = InModuleMaxLoopCount.GetAndAdvance();
			const bool ModuleAutoComplete = InModuleAutoComplete.GetAndAdvance();
			const bool ModuleCompleteOnInactive = InModuleCompleteOnInactive.GetAndAdvance();

			ENiagaraExecutionState EmitterExecutionState = InEmitterExecutionState.GetAndAdvance();
			ENiagaraExecutionStateSource EmitterExecutionStateSource = InEmitterExecutionStateSource.GetAndAdvance();
			float EmitterAge = InEmitterAge.GetAndAdvance();
			float EmitterLoopedAge = InEmitterLoopedAge.GetAndAdvance();
			float EmitterCurrentLoopDuration = InEmitterCurrentLoopDuration.GetAndAdvance();
			float EmitterCurrentLoopDelay = InEmitterCurrentLoopDelay.GetAndAdvance();
			int EmitterLoopCount = InEmitterLoopCount.GetAndAdvance();
			float EmitterNormalizedLoopAge = InEmitterNormalizedLoopAge.GetAndAdvance();

			// Skip disabled emitters
			if ( EmitterExecutionState != ENiagaraExecutionState::Disabled )
			{
				// Initialize parameters
				if (EmitterAge == 0.0f)
				{
					EmitterLoopedAge = -ModuleNextLoopDelay;
					EmitterCurrentLoopDuration = ModuleNextLoopDuration;
					EmitterCurrentLoopDelay = ModuleNextLoopDelay;
				}

				// Handle emitter looping
				EmitterAge += EngineDeltaTime;
				EmitterLoopedAge += EngineDeltaTime;
				const int32 LoopsPerformed = FMath::FloorToInt(EmitterLoopedAge / EmitterCurrentLoopDuration);
				if (LoopsPerformed > 0)
				{
					EmitterLoopedAge -= float(LoopsPerformed) * EmitterCurrentLoopDuration;
					EmitterLoopCount += LoopsPerformed;

					if (ModuleDurationRecalcEachLoop)
					{
						EmitterCurrentLoopDuration = ModuleNextLoopDuration;
					}
					if (ModuleDelayFirstLoopOnly)
					{
						EmitterCurrentLoopDelay = 0.0f;
					}
					EmitterNormalizedLoopAge = EmitterLoopedAge / EmitterCurrentLoopDuration;
				}

				// Set emitter state from scalability (if allowed)
				if ( EmitterExecutionStateSource <= ENiagaraExecutionStateSource::Scalability)
				{
					EmitterExecutionState = ScalabilityEmitterExecutionState;
					EmitterExecutionStateSource = ENiagaraExecutionStateSource::Scalability;
				}

				// Exceeded maximum loops?
				if (ModuleMaxLoopCount > 0 && EmitterLoopCount >= ModuleMaxLoopCount)
				{
					if (EmitterExecutionStateSource <= ENiagaraExecutionStateSource::Internal)
					{
						EmitterExecutionState = ENiagaraExecutionState::Inactive;
						EmitterExecutionStateSource = ENiagaraExecutionStateSource::Internal;
					}
				}

				// Are we complete?
				if (EmitterExecutionState != ENiagaraExecutionState::Active && (ModuleCompleteOnInactive || (EngineNumParticles == 0 && ModuleAutoComplete)))
				{
					if ( EmitterExecutionStateSource <= ENiagaraExecutionStateSource::InternalCompletion )
					{
						EmitterExecutionState = ENiagaraExecutionState::Complete;
						EmitterExecutionStateSource = ENiagaraExecutionStateSource::InternalCompletion;
					}
				}
			}

			// Set values
			*OutEmitterExecutionState.GetDestAndAdvance() = EmitterExecutionState;
			*OutEmitterExecutionStateSource.GetDestAndAdvance() = EmitterExecutionStateSource;
			*OutEmitterAge.GetDestAndAdvance() = EmitterAge;
			*OutEmitterLoopedAge.GetDestAndAdvance() = EmitterLoopedAge;
			*OutEmitterCurrentLoopDuration.GetDestAndAdvance() = EmitterCurrentLoopDuration;
			*OutEmitterCurrentLoopDelay.GetDestAndAdvance() = EmitterCurrentLoopDelay;
			*OutEmitterLoopCount.GetDestAndAdvance() = EmitterLoopCount;
			*OutEmitterNormalizedLoopAge.GetDestAndAdvance() = EmitterNormalizedLoopAge;
		}
	}
};

struct FVectorKernel_SpawnRate
{
	static FNiagaraFunctionSignature GetFunctionSignature()
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = FastPathSpawnRateName;
		Sig.OwnerName = FastPathLibraryName;
		Sig.bMemberFunction = false;
		Sig.bRequiresContext = false;
		Sig.SetDescription(LOCTEXT("FastPathSpawnRateDesc", "Fast path for spawn rate"));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EngineDeltaTime")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("ModuleSpawnRate")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("ScalabilityEmitterSpawnCountScale")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EngineEmitterSpawnCountScale")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterModuleSpawnRemainder")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterLoopedAge")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("EmitterSpawnGroup")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("SpawningCanEverSpawn")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterModuleSpawnRemainder")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("EmitterModuleSpawnInfoCount")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterModuleSpawnInfoInterpStartDt")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterModuleSpawnInfoIntervalDt")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("EmitterModuleSpawnInfoSpawnGroup")));

		return Sig;
	}

	static FString GetFunctionHLSL()
	{
		return FString();
	}

	static void Exec(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FExternalFuncInputHandler<float> InEngineDeltaTime(Context);
		VectorVM::FExternalFuncInputHandler<float> InModuleSpawnRate(Context);
		VectorVM::FExternalFuncInputHandler<float> InScalabilityEmitterSpawnCountScale(Context);
		VectorVM::FExternalFuncInputHandler<float> InEngineEmitterSpawnCountScale(Context);
		VectorVM::FExternalFuncInputHandler<float> InEmitterModuleSpawnRemainder(Context);
		VectorVM::FExternalFuncInputHandler<float> InEmitterLoopedAge(Context);
		VectorVM::FExternalFuncInputHandler<int32> InEmitterSpawnGroup(Context);

		VectorVM::FExternalFuncRegisterHandler<bool> OutSpawningCanEverSpawn(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutEmitterModuleSpawnRemainder(Context);
		VectorVM::FExternalFuncRegisterHandler<int32> OutEmitterModuleSpawnInfoCount(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutEmitterModuleSpawnInfoInterpStartDt(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutEmitterModuleSpawnInfoIntervalDt(Context);
		VectorVM::FExternalFuncRegisterHandler<int32> OutEmitterModuleSpawnInfoSpawnGroup(Context);

		for (int i = 0; i < Context.GetNumInstances(); ++i)
		{
			// Gather values
			float EngineDeltaTime = InEngineDeltaTime.GetAndAdvance();
			float ModuleSpawnRate = InModuleSpawnRate.GetAndAdvance();
			float ScalabilityEmitterSpawnCountScale = InScalabilityEmitterSpawnCountScale.GetAndAdvance();
			float EngineEmitterSpawnCountScale = InEngineEmitterSpawnCountScale.GetAndAdvance();
			float EmitterModuleSpawnRemainder = InEmitterModuleSpawnRemainder.GetAndAdvance();
			float EmitterLoopedAge = InEmitterLoopedAge.GetAndAdvance();
			int32 EmitterSpawnGroup = InEmitterSpawnGroup.GetAndAdvance();

			//
			float LocalModuleSpawnRate = ModuleSpawnRate * ScalabilityEmitterSpawnCountScale * EngineEmitterSpawnCountScale;
			float LocalModuleIntervalDT = 1.0f / LocalModuleSpawnRate;
			float LocalModuleInterpStartDT = LocalModuleIntervalDT * (1.0f - EmitterModuleSpawnRemainder);

			FNiagaraSpawnInfo SpawnInfo;
			if (EmitterLoopedAge > 0.0f)
			{
				float fSpawnCount = (LocalModuleSpawnRate * EngineDeltaTime) + EmitterModuleSpawnRemainder;
				int32 LocalModuleSpawnCount = FMath::FloorToInt(fSpawnCount);
				EmitterModuleSpawnRemainder = fSpawnCount - float(LocalModuleSpawnCount);

				SpawnInfo.Count = LocalModuleSpawnCount;
				SpawnInfo.InterpStartDt = LocalModuleInterpStartDT;
				SpawnInfo.IntervalDt = LocalModuleIntervalDT;
				SpawnInfo.SpawnGroup = EmitterSpawnGroup;
			}

			// Write values
			*OutEmitterModuleSpawnInfoCount.GetDestAndAdvance() = SpawnInfo.Count;
			*OutEmitterModuleSpawnInfoInterpStartDt.GetDestAndAdvance() = SpawnInfo.InterpStartDt;
			*OutEmitterModuleSpawnInfoIntervalDt.GetDestAndAdvance() = SpawnInfo.IntervalDt;
			*OutEmitterModuleSpawnInfoSpawnGroup.GetDestAndAdvance() = SpawnInfo.SpawnGroup;
			*OutSpawningCanEverSpawn.GetDestAndAdvance() = true;
			*OutEmitterModuleSpawnRemainder.GetDestAndAdvance() = EmitterModuleSpawnRemainder;
		}
	}
};

struct FVectorKernel_SpawnBurstInstantaneous
{
	static FNiagaraFunctionSignature GetFunctionSignature()
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = FastPathSpawnBurstInstantaneousName;
		Sig.OwnerName = FastPathLibraryName;
		Sig.bMemberFunction = false;
		Sig.bRequiresContext = false;
		Sig.SetDescription(LOCTEXT("FastPathSpawnBurstInstantaneous", "Fast path for spawn burst instantaneous"));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EngineDeltaTime")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("ScalabilityEmitterSpawnCountScale")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterLoopedAge")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("ModuleSpawnTime")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ModuleSpawnCount")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("ModuleSpawnGroup")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("SpawningCanEverSpawn")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("EmitterModuleSpawnInfoCount")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterModuleSpawnInfoInterpStartDt")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EmitterModuleSpawnInfoIntervalDt")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("EmitterModuleSpawnInfoSpawnGroup")));

		return Sig;
	}

	static FString GetFunctionHLSL()
	{
		return FString();
	}

	static void Exec(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FExternalFuncInputHandler<float> InEngineDeltaTime(Context);
		VectorVM::FExternalFuncInputHandler<float> InScalabilityEmitterSpawnCountScale(Context);
		VectorVM::FExternalFuncInputHandler<float> InEmitterLoopedAge(Context);
		VectorVM::FExternalFuncInputHandler<float> InModuleSpawnTime(Context);
		VectorVM::FExternalFuncInputHandler<int32> InModuleSpawnCount(Context);
		VectorVM::FExternalFuncInputHandler<int32> InModuleSpawnGroup(Context);

		VectorVM::FExternalFuncRegisterHandler<bool> OutSpawningCanEverSpawn(Context);
		VectorVM::FExternalFuncRegisterHandler<int32> OutEmitterModuleSpawnInfoCount(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutEmitterModuleSpawnInfoInterpStartDt(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutEmitterModuleSpawnInfoIntervalDt(Context);
		VectorVM::FExternalFuncRegisterHandler<int32> OutEmitterModuleSpawnInfoSpawnGroup(Context);

		for (int i=0; i < Context.GetNumInstances(); ++i)
		{
			// Gather values
			float EngineDeltaTime = InEngineDeltaTime.GetAndAdvance();
			float ScalabilityEmitterSpawnCountScale = InScalabilityEmitterSpawnCountScale.GetAndAdvance();
			float EmitterLoopedAge = InEmitterLoopedAge.GetAndAdvance();
			float ModuleSpawnTime = InModuleSpawnTime.GetAndAdvance();
			int32 ModuleSpawnCount = InModuleSpawnCount.GetAndAdvance();
			int32 ModuleSpawnGroup = InModuleSpawnGroup.GetAndAdvance();

			const float PreviousTime = EmitterLoopedAge - EngineDeltaTime;

			*OutSpawningCanEverSpawn.GetDestAndAdvance() = EmitterLoopedAge <= ModuleSpawnTime;
			if ( (ModuleSpawnTime >= PreviousTime) && (ModuleSpawnTime < EmitterLoopedAge) )
			{
				*OutEmitterModuleSpawnInfoCount.GetDestAndAdvance() = ModuleSpawnCount;
				*OutEmitterModuleSpawnInfoInterpStartDt.GetDestAndAdvance() = ModuleSpawnTime - PreviousTime;
			}
			else
			{
				*OutEmitterModuleSpawnInfoCount.GetDestAndAdvance() = 0;
				*OutEmitterModuleSpawnInfoInterpStartDt.GetDestAndAdvance() = 0.0f;
			}
			*OutEmitterModuleSpawnInfoIntervalDt.GetDestAndAdvance() = 0.0f;
			*OutEmitterModuleSpawnInfoSpawnGroup.GetDestAndAdvance() = ModuleSpawnGroup;
		}
	}
};

struct FVectorKernel_SolveVelocitiesAndForces
{
	static FNiagaraFunctionSignature GetFunctionSignature()
	{
		FNiagaraFunctionSignature Sig;
		Sig.Name = FastPathSolveVelocitiesAndForces;
		Sig.OwnerName = FastPathLibraryName;
		Sig.bMemberFunction = false;
		Sig.bRequiresContext = false;
		Sig.SetDescription(LOCTEXT("FastPathSolveVelocitiesAndForces", "Fast path for SolveVelocitiesAndForces"));

		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("EngineDeltaTime")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("PhysicsForce")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("PhysicsDrag")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("ParticlesMass")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ParticlesPosition")));
		Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ParticlesVelocity")));

		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ParticlesPosition")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ParticlesVelocity")));
		Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ParticlesPreviousVelocity")));

		return Sig;
	}

	static FString GetFunctionHLSL()
	{
		return FString();
	}

	static void Exec(FVectorVMExternalFunctionContext& Context)
	{
		VectorVM::FExternalFuncInputHandler<float> InEngineDeltaTime(Context);
		VectorVM::FExternalFuncInputHandler<float> InPhysicsForceX(Context);
		VectorVM::FExternalFuncInputHandler<float> InPhysicsForceY(Context);
		VectorVM::FExternalFuncInputHandler<float> InPhysicsForceZ(Context);
		VectorVM::FExternalFuncInputHandler<float> InPhysicsDrag(Context);
		VectorVM::FExternalFuncInputHandler<float> InParticlesMass(Context);
		VectorVM::FExternalFuncInputHandler<float> InParticlesPositionX(Context);
		VectorVM::FExternalFuncInputHandler<float> InParticlesPositionY(Context);
		VectorVM::FExternalFuncInputHandler<float> InParticlesPositionZ(Context);
		VectorVM::FExternalFuncInputHandler<float> InParticlesVelocityX(Context);
		VectorVM::FExternalFuncInputHandler<float> InParticlesVelocityY(Context);
		VectorVM::FExternalFuncInputHandler<float> InParticlesVelocityZ(Context);

		VectorVM::FExternalFuncRegisterHandler<float> OutParticlesPositionX(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutParticlesPositionY(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutParticlesPositionZ(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutParticlesVelocityX(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutParticlesVelocityY(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutParticlesVelocityZ(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutParticlesPreviousVelocityX(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutParticlesPreviousVelocityY(Context);
		VectorVM::FExternalFuncRegisterHandler<float> OutParticlesPreviousVelocityZ(Context);
#if 1
		const float EngineDeltaTime = InEngineDeltaTime.Get();
		const float MassMin = 0.0001f;

		for (int i = 0; i < Context.GetNumInstances(); ++i)
		{
			// Gather values
			FVector PhysicsForce(InPhysicsForceX.GetAndAdvance(), InPhysicsForceY.GetAndAdvance(), InPhysicsForceZ.GetAndAdvance());
			float PhysicsDrag = InPhysicsDrag.GetAndAdvance();

			float ParticleMass = InParticlesMass.GetAndAdvance();
			FVector ParticlePosition(InParticlesPositionX.GetAndAdvance(), InParticlesPositionY.GetAndAdvance(), InParticlesPositionZ.GetAndAdvance());
			FVector ParticleVelocity(InParticlesVelocityX.GetAndAdvance(), InParticlesVelocityY.GetAndAdvance(), InParticlesVelocityZ.GetAndAdvance());

			*OutParticlesPreviousVelocityX.GetDestAndAdvance() = ParticleVelocity.X;
			*OutParticlesPreviousVelocityY.GetDestAndAdvance() = ParticleVelocity.Y;
			*OutParticlesPreviousVelocityZ.GetDestAndAdvance() = ParticleVelocity.Z;

			// Apply velocity
			const float OOParticleMassDT = (1.0f / FMath::Max(ParticleMass, MassMin)) * EngineDeltaTime;
			ParticleVelocity += PhysicsForce * OOParticleMassDT;

			// Apply Drag
			float ClampedDrag = FMath::Clamp(PhysicsDrag * EngineDeltaTime, 0.0f, 1.0f);
			ParticleVelocity -= ParticleVelocity * ClampedDrag;

			// Velocity Clamp
			//-TODO: Not used

			// Limit Acceleration
			//-TODO: Not used

			// Apply velocity
			ParticlePosition += ParticleVelocity * EngineDeltaTime;

			// Write parameters
			*OutParticlesPositionX.GetDestAndAdvance() = ParticlePosition.X;
			*OutParticlesPositionY.GetDestAndAdvance() = ParticlePosition.Y;
			*OutParticlesPositionZ.GetDestAndAdvance() = ParticlePosition.Z;

			*OutParticlesVelocityX.GetDestAndAdvance() = ParticleVelocity.X;
			*OutParticlesVelocityY.GetDestAndAdvance() = ParticleVelocity.Y;
			*OutParticlesVelocityZ.GetDestAndAdvance() = ParticleVelocity.Z;
		}
#else
		const VectorRegister4Float EngineDeltaTime = VectorSetFloat1(InEngineDeltaTime.Get());
		const VectorRegister4Float MassMin = VectorSetFloat1(0.0001f);

		for (int i=0; i < Context.GetNumLoops<4>(); ++i)
		{
			// Gather values
			VectorRegister4Float PhysicsForceX		= VectorLoad(InPhysicsForceX.GetDestAndAdvance());
			VectorRegister4Float PhysicsForceY		= VectorLoad(InPhysicsForceY.GetDestAndAdvance());
			VectorRegister4Float PhysicsForceZ		= VectorLoad(InPhysicsForceZ.GetDestAndAdvance());
			VectorRegister4Float PhysicsDrag			= VectorLoad(InPhysicsDrag.GetDestAndAdvance());

			VectorRegister4Float ParticlesMass		= VectorLoad(InParticlesMass.GetDestAndAdvance());
			VectorRegister4Float ParticlesPositionX	= VectorLoad(InParticlesPositionX.GetDestAndAdvance());
			VectorRegister4Float ParticlesPositionY	= VectorLoad(InParticlesPositionY.GetDestAndAdvance());
			VectorRegister4Float ParticlesPositionZ	= VectorLoad(InParticlesPositionZ.GetDestAndAdvance());
			VectorRegister4Float ParticlesVelocityX	= VectorLoad(InParticlesVelocityX.GetDestAndAdvance());
			VectorRegister4Float ParticlesVelocityY	= VectorLoad(InParticlesVelocityY.GetDestAndAdvance());
			VectorRegister4Float ParticlesVelocityZ	= VectorLoad(InParticlesVelocityZ.GetDestAndAdvance());

			VectorStore(ParticlesVelocityX, OutParticlesPreviousVelocityX.GetDestAndAdvance());
			VectorStore(ParticlesVelocityY, OutParticlesPreviousVelocityY.GetDestAndAdvance());
			VectorStore(ParticlesVelocityZ, OutParticlesPreviousVelocityZ.GetDestAndAdvance());

			// Apply velocity
			const VectorRegister4Float OOParticleMassDT = VectorMultiply(VectorReciprocal(VectorMax(ParticlesMass, MassMin)), EngineDeltaTime);
			ParticlesVelocityX = VectorMultiplyAdd(PhysicsForceX, OOParticleMassDT, ParticlesVelocityX);
			ParticlesVelocityY = VectorMultiplyAdd(PhysicsForceY, OOParticleMassDT, ParticlesVelocityY);
			ParticlesVelocityZ = VectorMultiplyAdd(PhysicsForceZ, OOParticleMassDT, ParticlesVelocityZ);

			// Apply Drag
			VectorRegister4Float ClampedDrag = VectorMultiply(PhysicsDrag, EngineDeltaTime);
			ClampedDrag = VectorMax(VectorMin(ClampedDrag, VectorOne()), VectorZero());
			ClampedDrag = VectorNegate(ClampedDrag);

			ParticlesVelocityX = VectorMultiplyAdd(ParticlesVelocityX, ClampedDrag, ParticlesVelocityX);
			ParticlesVelocityY = VectorMultiplyAdd(ParticlesVelocityY, ClampedDrag, ParticlesVelocityY);
			ParticlesVelocityZ = VectorMultiplyAdd(ParticlesVelocityZ, ClampedDrag, ParticlesVelocityZ);

			// Velocity Clamp
			//-TODO: Not used

			// Limit Acceleration
			//-TODO: Not used

			// Apply velocity
			ParticlesPositionX = VectorMultiplyAdd(ParticlesVelocityX, EngineDeltaTime, ParticlesPositionX);
			ParticlesPositionY = VectorMultiplyAdd(ParticlesVelocityY, EngineDeltaTime, ParticlesPositionY);
			ParticlesPositionZ = VectorMultiplyAdd(ParticlesVelocityZ, EngineDeltaTime, ParticlesPositionZ);

			// Write parameters
			VectorStore(ParticlesPositionX, OutParticlesPositionX.GetDestAndAdvance());
			VectorStore(ParticlesPositionY, OutParticlesPositionY.GetDestAndAdvance());
			VectorStore(ParticlesPositionZ, OutParticlesPositionZ.GetDestAndAdvance());

			VectorStore(ParticlesVelocityX, OutParticlesVelocityX.GetDestAndAdvance());
			VectorStore(ParticlesVelocityY, OutParticlesVelocityY.GetDestAndAdvance());
			VectorStore(ParticlesVelocityZ, OutParticlesVelocityZ.GetDestAndAdvance());
		}
#endif
	}

	template<bool bForceConstant, bool bDragConstant, bool bMassConstant>
	FORCEINLINE static void ExecOptimized(FVectorVMExternalFunctionContext& Context)
	{
#if 0
		const VectorRegister4Float MassMin = VectorSetFloat1(0.0001f);
		const VectorRegister4Float EngineDeltaTime = VectorSetFloat1(VectorVM::FExternalFuncInputHandler<float>(Context).Get());
		VectorRegister4Float PhysicsForceX = VectorLoadFloat1(VectorVM::FExternalFuncInputHandler<VectorRegister4Float>(Context).GetDest());
		VectorRegister4Float PhysicsForceY = VectorLoadFloat1(VectorVM::FExternalFuncInputHandler<VectorRegister4Float>(Context).GetDest());
		VectorRegister4Float PhysicsForceZ = VectorLoadFloat1(VectorVM::FExternalFuncInputHandler<VectorRegister4Float>(Context).GetDest());
		VectorRegister4Float PhysicsDrag = VectorLoadFloat1(VectorVM::FExternalFuncInputHandler<VectorRegister4Float>(Context).GetDest());
		VectorRegister4Float ParticlesMass = VectorLoadFloat1(VectorVM::FExternalFuncInputHandler<VectorRegister4Float>(Context).GetDest());
		const VectorRegister4Float* RESTRICT InParticlesPositionX = VectorVM::FExternalFuncInputHandler<VectorRegister4Float>(Context).GetDest();
		const VectorRegister4Float* RESTRICT InParticlesPositionY = VectorVM::FExternalFuncInputHandler<VectorRegister4Float>(Context).GetDest();
		const VectorRegister4Float* RESTRICT InParticlesPositionZ = VectorVM::FExternalFuncInputHandler<VectorRegister4Float>(Context).GetDest();
		const VectorRegister4Float* RESTRICT InParticlesVelocityX = VectorVM::FExternalFuncInputHandler<VectorRegister4Float>(Context).GetDest();
		const VectorRegister4Float* RESTRICT InParticlesVelocityY = VectorVM::FExternalFuncInputHandler<VectorRegister4Float>(Context).GetDest();
		const VectorRegister4Float* RESTRICT InParticlesVelocityZ = VectorVM::FExternalFuncInputHandler<VectorRegister4Float>(Context).GetDest();

		VectorRegister4Float* RESTRICT OutParticlesPositionX = VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float>(Context).GetDest();
		VectorRegister4Float* RESTRICT OutParticlesPositionY = VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float>(Context).GetDest();
		VectorRegister4Float* RESTRICT OutParticlesPositionZ = VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float>(Context).GetDest();
		VectorRegister4Float* RESTRICT OutParticlesVelocityX = VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float>(Context).GetDest();
		VectorRegister4Float* RESTRICT OutParticlesVelocityY = VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float>(Context).GetDest();
		VectorRegister4Float* RESTRICT OutParticlesVelocityZ = VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float>(Context).GetDest();
		VectorRegister4Float* RESTRICT OutParticlesPreviousVelocityX = VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float>(Context).GetDest();
		VectorRegister4Float* RESTRICT OutParticlesPreviousVelocityY = VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float>(Context).GetDest();
		VectorRegister4Float* RESTRICT OutParticlesPreviousVelocityZ = VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float>(Context).GetDest();

		const VectorRegister4Float OOParticleMassDT = VectorMultiply(VectorReciprocal(VectorMax(ParticlesMass, MassMin)), EngineDeltaTime);

		VectorRegister4Float ClampedDrag = VectorMultiply(PhysicsDrag, EngineDeltaTime);
		ClampedDrag = VectorMax(VectorMin(ClampedDrag, VectorOne()), VectorZero());
		ClampedDrag = VectorNegate(ClampedDrag);

		for (int i = 0; i < Context.GetNumLoops<4>(); ++i)
		{
			// Gather values
			VectorRegister4Float ParticlesPositionX = VectorLoad(InParticlesPositionX + i);
			VectorRegister4Float ParticlesPositionY = VectorLoad(InParticlesPositionY + i);
			VectorRegister4Float ParticlesPositionZ = VectorLoad(InParticlesPositionZ + i);
			VectorRegister4Float ParticlesVelocityX = VectorLoad(InParticlesVelocityX + i);
			VectorRegister4Float ParticlesVelocityY = VectorLoad(InParticlesVelocityY + i);
			VectorRegister4Float ParticlesVelocityZ = VectorLoad(InParticlesVelocityZ + i);

			VectorStore(ParticlesVelocityX, OutParticlesPreviousVelocityX + i);
			VectorStore(ParticlesVelocityY, OutParticlesPreviousVelocityY + i);
			VectorStore(ParticlesVelocityZ, OutParticlesPreviousVelocityZ + i);

			// Apply velocity
			ParticlesVelocityX = VectorMultiplyAdd(PhysicsForceX, OOParticleMassDT, ParticlesVelocityX);
			ParticlesVelocityY = VectorMultiplyAdd(PhysicsForceY, OOParticleMassDT, ParticlesVelocityY);
			ParticlesVelocityZ = VectorMultiplyAdd(PhysicsForceZ, OOParticleMassDT, ParticlesVelocityZ);

			// Apply Drag
			ParticlesVelocityX = VectorMultiplyAdd(ParticlesVelocityX, ClampedDrag, ParticlesVelocityX);
			ParticlesVelocityY = VectorMultiplyAdd(ParticlesVelocityY, ClampedDrag, ParticlesVelocityY);
			ParticlesVelocityZ = VectorMultiplyAdd(ParticlesVelocityZ, ClampedDrag, ParticlesVelocityZ);

			// Velocity Clamp
			//-TODO: Not used

			// Limit Acceleration
			//-TODO: Not used

			// Apply velocity
			ParticlesPositionX = VectorMultiplyAdd(ParticlesVelocityX, EngineDeltaTime, ParticlesPositionX);
			ParticlesPositionY = VectorMultiplyAdd(ParticlesVelocityY, EngineDeltaTime, ParticlesPositionY);
			ParticlesPositionZ = VectorMultiplyAdd(ParticlesVelocityZ, EngineDeltaTime, ParticlesPositionZ);

			// Write parameters
			VectorStore(ParticlesPositionX, OutParticlesPositionX + i);
			VectorStore(ParticlesPositionY, OutParticlesPositionY + i);
			VectorStore(ParticlesPositionZ, OutParticlesPositionZ + i);

			VectorStore(ParticlesVelocityX, OutParticlesVelocityX + i);
			VectorStore(ParticlesVelocityY, OutParticlesVelocityY + i);
			VectorStore(ParticlesVelocityZ, OutParticlesVelocityZ + i);
		}
#else
		VectorVM::FExternalFuncInputHandler<float> InEngineDeltaTime(Context);
		VectorVM::FExternalFuncInputHandler<VectorRegister4Float> InPhysicsForceXHandler(Context);
		VectorVM::FExternalFuncInputHandler<VectorRegister4Float> InPhysicsForceYHandler(Context);
		VectorVM::FExternalFuncInputHandler<VectorRegister4Float> InPhysicsForceZHandler(Context);
		VectorVM::FExternalFuncInputHandler<VectorRegister4Float> InPhysicsDragHandler(Context);
		VectorVM::FExternalFuncInputHandler<VectorRegister4Float> InParticlesMassHandler(Context);
		VectorVM::FExternalFuncInputHandler<VectorRegister4Float> InParticlesPositionXHandler(Context);
		VectorVM::FExternalFuncInputHandler<VectorRegister4Float> InParticlesPositionYHandler(Context);
		VectorVM::FExternalFuncInputHandler<VectorRegister4Float> InParticlesPositionZHandler(Context);
		VectorVM::FExternalFuncInputHandler<VectorRegister4Float> InParticlesVelocityXHandler(Context);
		VectorVM::FExternalFuncInputHandler<VectorRegister4Float> InParticlesVelocityYHandler(Context);
		VectorVM::FExternalFuncInputHandler<VectorRegister4Float> InParticlesVelocityZHandler(Context);

		VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float> OutParticlesPositionXHandler(Context);
		VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float> OutParticlesPositionYHandler(Context);
		VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float> OutParticlesPositionZHandler(Context);
		VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float> OutParticlesVelocityXHandler(Context);
		VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float> OutParticlesVelocityYHandler(Context);
		VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float> OutParticlesVelocityZHandler(Context);
		VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float> OutParticlesPreviousVelocityXHandler(Context);
		VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float> OutParticlesPreviousVelocityYHandler(Context);
		VectorVM::FExternalFuncRegisterHandler<VectorRegister4Float> OutParticlesPreviousVelocityZHandler(Context);

		const VectorRegister4Float EngineDeltaTime = VectorSetFloat1(InEngineDeltaTime.Get());
		const VectorRegister4Float* RESTRICT InPhysicsForceX = InPhysicsForceXHandler.GetDest();
		const VectorRegister4Float* RESTRICT InPhysicsForceY = InPhysicsForceYHandler.GetDest();
		const VectorRegister4Float* RESTRICT InPhysicsForceZ = InPhysicsForceZHandler.GetDest();
		const VectorRegister4Float* RESTRICT InPhysicsDrag = InPhysicsDragHandler.GetDest();
		const VectorRegister4Float* RESTRICT InParticlesMass = InParticlesMassHandler.GetDest();
		const VectorRegister4Float* RESTRICT InParticlesPositionX = InParticlesPositionXHandler.GetDest();
		const VectorRegister4Float* RESTRICT InParticlesPositionY = InParticlesPositionYHandler.GetDest();
		const VectorRegister4Float* RESTRICT InParticlesPositionZ = InParticlesPositionZHandler.GetDest();
		const VectorRegister4Float* RESTRICT InParticlesVelocityX = InParticlesVelocityXHandler.GetDest();
		const VectorRegister4Float* RESTRICT InParticlesVelocityY = InParticlesVelocityYHandler.GetDest();
		const VectorRegister4Float* RESTRICT InParticlesVelocityZ = InParticlesVelocityZHandler.GetDest();

		VectorRegister4Float* RESTRICT OutParticlesPositionX = OutParticlesPositionXHandler.GetDest();
		VectorRegister4Float* RESTRICT OutParticlesPositionY = OutParticlesPositionYHandler.GetDest();
		VectorRegister4Float* RESTRICT OutParticlesPositionZ = OutParticlesPositionZHandler.GetDest();
		VectorRegister4Float* RESTRICT OutParticlesVelocityX = OutParticlesVelocityXHandler.GetDest();
		VectorRegister4Float* RESTRICT OutParticlesVelocityY = OutParticlesVelocityYHandler.GetDest();
		VectorRegister4Float* RESTRICT OutParticlesVelocityZ = OutParticlesVelocityZHandler.GetDest();
		VectorRegister4Float* RESTRICT OutParticlesPreviousVelocityX = OutParticlesPreviousVelocityXHandler.GetDest();
		VectorRegister4Float* RESTRICT OutParticlesPreviousVelocityY = OutParticlesPreviousVelocityYHandler.GetDest();
		VectorRegister4Float* RESTRICT OutParticlesPreviousVelocityZ = OutParticlesPreviousVelocityZHandler.GetDest();

		const VectorRegister4Float MassMin = VectorSetFloat1(0.0001f);

		const VectorRegister4Float ConstantPhysicsForceX = bForceConstant ? VectorLoadFloat1(InPhysicsForceX) : VectorZero();
		const VectorRegister4Float ConstantPhysicsForceY = bForceConstant ? VectorLoadFloat1(InPhysicsForceY) : VectorZero();
		const VectorRegister4Float ConstantPhysicsForceZ = bForceConstant ? VectorLoadFloat1(InPhysicsForceZ) : VectorZero();
		const VectorRegister4Float ConstantPhysicsDrag = bDragConstant ? VectorLoadFloat1(InPhysicsDrag) : VectorZero();
		const VectorRegister4Float ConstantParticlesMass = bMassConstant ? VectorLoadFloat1(InParticlesMass) : VectorZero();

		for (int i = 0; i < Context.GetNumLoops<4>(); ++i)
		{
			// Gather values
			VectorRegister4Float PhysicsForceX = bForceConstant ? ConstantPhysicsForceX : VectorLoad(InPhysicsForceX + i);
			VectorRegister4Float PhysicsForceY = bForceConstant ? ConstantPhysicsForceY : VectorLoad(InPhysicsForceY + i);
			VectorRegister4Float PhysicsForceZ = bForceConstant ? ConstantPhysicsForceZ : VectorLoad(InPhysicsForceZ + i);
			VectorRegister4Float PhysicsDrag = bDragConstant ? ConstantPhysicsDrag : VectorLoad(InPhysicsDrag + i);

			VectorRegister4Float ParticlesMass = bMassConstant ? ConstantParticlesMass : VectorLoad(InParticlesMass + i);
			VectorRegister4Float ParticlesPositionX = VectorLoad(InParticlesPositionX + i);
			VectorRegister4Float ParticlesPositionY = VectorLoad(InParticlesPositionY + i);
			VectorRegister4Float ParticlesPositionZ = VectorLoad(InParticlesPositionZ + i);
			VectorRegister4Float ParticlesVelocityX = VectorLoad(InParticlesVelocityX + i);
			VectorRegister4Float ParticlesVelocityY = VectorLoad(InParticlesVelocityY + i);
			VectorRegister4Float ParticlesVelocityZ = VectorLoad(InParticlesVelocityZ + i);

			VectorStore(ParticlesVelocityX, OutParticlesPreviousVelocityX + i);
			VectorStore(ParticlesVelocityY, OutParticlesPreviousVelocityY + i);
			VectorStore(ParticlesVelocityZ, OutParticlesPreviousVelocityZ + i);

			// Apply velocity
			const VectorRegister4Float OOParticleMassDT = VectorMultiply(VectorReciprocal(VectorMax(ParticlesMass, MassMin)), EngineDeltaTime);
			ParticlesVelocityX = VectorMultiplyAdd(PhysicsForceX, OOParticleMassDT, ParticlesVelocityX);
			ParticlesVelocityY = VectorMultiplyAdd(PhysicsForceY, OOParticleMassDT, ParticlesVelocityY);
			ParticlesVelocityZ = VectorMultiplyAdd(PhysicsForceZ, OOParticleMassDT, ParticlesVelocityZ);

			// Apply Drag
			VectorRegister4Float ClampedDrag = VectorMultiply(PhysicsDrag, EngineDeltaTime);
			ClampedDrag = VectorMax(VectorMin(ClampedDrag, VectorOne()), VectorZero());
			ClampedDrag = VectorNegate(ClampedDrag);

			ParticlesVelocityX = VectorMultiplyAdd(ParticlesVelocityX, ClampedDrag, ParticlesVelocityX);
			ParticlesVelocityY = VectorMultiplyAdd(ParticlesVelocityY, ClampedDrag, ParticlesVelocityY);
			ParticlesVelocityZ = VectorMultiplyAdd(ParticlesVelocityZ, ClampedDrag, ParticlesVelocityZ);

			// Velocity Clamp
			//-TODO: Not used

			// Limit Acceleration
			//-TODO: Not used

			// Apply velocity
			ParticlesPositionX = VectorMultiplyAdd(ParticlesVelocityX, EngineDeltaTime, ParticlesPositionX);
			ParticlesPositionY = VectorMultiplyAdd(ParticlesVelocityY, EngineDeltaTime, ParticlesPositionY);
			ParticlesPositionZ = VectorMultiplyAdd(ParticlesVelocityZ, EngineDeltaTime, ParticlesPositionZ);

			// Write parameters
			VectorStore(ParticlesPositionX, OutParticlesPositionX + i);
			VectorStore(ParticlesPositionY, OutParticlesPositionY + i);
			VectorStore(ParticlesPositionZ, OutParticlesPositionZ + i);

			VectorStore(ParticlesVelocityX, OutParticlesVelocityX + i);
			VectorStore(ParticlesVelocityY, OutParticlesVelocityY + i);
			VectorStore(ParticlesVelocityZ, OutParticlesVelocityZ + i);
		}
#endif
	}
};

bool UNiagaraFunctionLibrary::GetVectorVMFastPathExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, FVMExternalFunction &OutFunc)
{
	if (BindingInfo.Name == FastPathDot4Name)
	{
		OutFunc = FVMExternalFunction::CreateStatic(FVectorKernelFastDot4::Exec);
		return true;
	}
	else if (BindingInfo.Name == FastPathTransformPositionName)
	{
		OutFunc = FVMExternalFunction::CreateStatic(FVectorKernelFastTransformPosition::Exec);
		return true;
	}
	else if (BindingInfo.Name == FastMatrixToQuaternionName)
	{
		OutFunc = FVMExternalFunction::CreateStatic(FVectorKernelFastMatrixToQuaternion::Exec);
		return true;
	}
	else if (BindingInfo.Name == FastPathEmitterLifeCycleName)
	{
		OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_EmitterLifeCycle::Exec);
		return true;
	}
	else if (BindingInfo.Name == FastPathSpawnRateName)
	{
		OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SpawnRate::Exec);
		return true;
	}
	else if (BindingInfo.Name == FastPathSpawnBurstInstantaneousName)
	{
		OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SpawnBurstInstantaneous::Exec);
		return true;
	}
	else if (BindingInfo.Name == FastPathSolveVelocitiesAndForces)
	{
#if 0
		OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SolveVelocitiesAndForces::Exec);
#else
		const bool bForceConstant = BindingInfo.InputParamLocations[1] && BindingInfo.InputParamLocations[2] && BindingInfo.InputParamLocations[3];
		const bool bDragConstant = BindingInfo.InputParamLocations[4];
		const bool bMassConstant = BindingInfo.InputParamLocations[5];

		if ( bForceConstant )
		{
			if ( bDragConstant )
			{
				if ( bMassConstant )
					OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SolveVelocitiesAndForces::ExecOptimized<true, true, true>);
				else
					OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SolveVelocitiesAndForces::ExecOptimized<true, true, false>);
			}
			else
			{
				if (bMassConstant)
					OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SolveVelocitiesAndForces::ExecOptimized<true, false, true>);
				else
					OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SolveVelocitiesAndForces::ExecOptimized<true, false, false>);
			}
		}
		else
		{
			if (bDragConstant)
			{
				if (bMassConstant)
					OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SolveVelocitiesAndForces::ExecOptimized<false, true, true>);
				else
					OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SolveVelocitiesAndForces::ExecOptimized<false, true, false>);
			}
			else
			{
				if (bMassConstant)
					OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SolveVelocitiesAndForces::ExecOptimized<false, false, true>);
				else
					OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SolveVelocitiesAndForces::Exec);
					//OutFunc = FVMExternalFunction::CreateStatic(FVectorKernel_SolveVelocitiesAndForces::ExecOptimized<false, false, false>);
			}
		}
#endif
		return true;
	}
	return false;
}


void UNiagaraFunctionLibrary::InitVectorVMFastPathOps()
{
	if (VectorVMOps.Num() > 0)
		return;

	VectorVMOps.Emplace(FVectorKernelFastDot4::GetFunctionSignature());
	VectorVMOps.Emplace(FVectorKernelFastTransformPosition::GetFunctionSignature());
	VectorVMOps.Emplace(FVectorKernelFastMatrixToQuaternion::GetFunctionSignature());
	VectorVMOps.Emplace(FVectorKernel_EmitterLifeCycle::GetFunctionSignature());
	VectorVMOps.Emplace(FVectorKernel_SpawnRate::GetFunctionSignature());
	VectorVMOps.Emplace(FVectorKernel_SpawnBurstInstantaneous::GetFunctionSignature());
	VectorVMOps.Emplace(FVectorKernel_SolveVelocitiesAndForces::GetFunctionSignature());

	VectorVMOpsHLSL.Emplace(FVectorKernelFastDot4::GetFunctionHLSL());
	VectorVMOpsHLSL.Emplace(FVectorKernelFastTransformPosition::GetFunctionHLSL());
	VectorVMOpsHLSL.Emplace(FVectorKernelFastMatrixToQuaternion::GetFunctionHLSL());
	VectorVMOpsHLSL.Emplace(FVectorKernel_EmitterLifeCycle::GetFunctionHLSL());
	VectorVMOpsHLSL.Emplace(FVectorKernel_SpawnRate::GetFunctionHLSL());
	VectorVMOpsHLSL.Emplace(FVectorKernel_SpawnBurstInstantaneous::GetFunctionHLSL());
	VectorVMOpsHLSL.Emplace(FVectorKernel_SolveVelocitiesAndForces::GetFunctionHLSL());

	check(VectorVMOps.Num() == VectorVMOpsHLSL.Num());
}


//////////////////////////////////////////////////////////////////////////
//GPU Ray Traced Collision Functions

void UNiagaraFunctionLibrary::SetComponentNiagaraGPURayTracedCollisionGroup(UObject* WorldContextObject, UPrimitiveComponent* Primitive, int32 CollisionGroup)
{
#if NIAGARA_ASYNC_GPU_TRACE_COLLISION_GROUPS
	if ( UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) )
	{
		if ( FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = FNiagaraGpuComputeDispatchInterface::Get(World) )
		{
			ComputeDispatchInterface->GetAsyncGpuTraceHelper().SetPrimitiveRayTracingCollisionGroup_GT(Primitive, CollisionGroup);
		}
	}
#endif
}

void UNiagaraFunctionLibrary::SetActorNiagaraGPURayTracedCollisionGroup(UObject* WorldContextObject, AActor* Actor, int32 CollisionGroup)
{
#if NIAGARA_ASYNC_GPU_TRACE_COLLISION_GROUPS
	if (Actor == nullptr)
	{
		return;
	}

	if ( UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) )
	{
		if (FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = FNiagaraGpuComputeDispatchInterface::Get(World))
		{
			FNiagaraAsyncGpuTraceHelper& Helper = ComputeDispatchInterface->GetAsyncGpuTraceHelper();

			Actor->ForEachComponent<UPrimitiveComponent>(
				/*bIncludeFromChildActors*/true,
				[&Helper, &CollisionGroup](UPrimitiveComponent* PrimitiveComponent)
				{
					Helper.SetPrimitiveRayTracingCollisionGroup_GT(PrimitiveComponent, CollisionGroup);
				});
			}
	}
#endif
}

int32 UNiagaraFunctionLibrary::AcquireNiagaraGPURayTracedCollisionGroup(UObject* WorldContextObject)
{
#if NIAGARA_ASYNC_GPU_TRACE_COLLISION_GROUPS
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = FNiagaraGpuComputeDispatchInterface::Get(World))
		{
			return ComputeDispatchInterface->GetAsyncGpuTraceHelper().AcquireGPURayTracedCollisionGroup_GT();
		}
	}
#endif
	return INDEX_NONE;
}

void UNiagaraFunctionLibrary::ReleaseNiagaraGPURayTracedCollisionGroup(UObject* WorldContextObject, int32 CollisionGroup)
{
#if NIAGARA_ASYNC_GPU_TRACE_COLLISION_GROUPS
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (FNiagaraGpuComputeDispatchInterface* ComputeDispatchInterface = FNiagaraGpuComputeDispatchInterface::Get(World))
		{
			ComputeDispatchInterface->GetAsyncGpuTraceHelper().ReleaseGPURayTracedCollisionGroup_GT(CollisionGroup);
		}
	}
#endif
}
#undef LOCTEXT_NAMESPACE
