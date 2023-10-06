// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/CustomizableObjectSystem.h"

#include "Animation/Skeleton.h"
#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkinnedAsset.h"
#include "GameFramework/Pawn.h"
#include "Engine/SkinnedAssetCommon.h"
#include "GameFramework/PlayerController.h"
#include "Interfaces/ITargetPlatform.h"
#include "Kismet/GameplayStatics.h"
#include "MuCO/CustomizableInstanceLODManagement.h"
#include "MuCO/CustomizableInstancePrivateData.h"
#include "MuCO/CustomizableObjectPrivate.h"
#include "MuCO/DefaultImageProvider.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCO/ICustomizableObjectModule.h"
#include "MuCO/LogBenchmarkUtil.h"
#include "MuCO/LogInformationUtil.h"
#include "MuCO/UnrealBakeHelpers.h"
#include "MuCO/UnrealExtensionDataStreamer.h"
#include "MuCO/UnrealMutableImageProvider.h"
#include "MuCO/UnrealMutableModelDiskStreamer.h"
#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuR/Model.h"
#include "MuR/Settings.h"
#include "TextureResource.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "ContentStreaming.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Logging/MessageLog.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#else
#include "Engine/Engine.h"
#endif


#include UE_INLINE_GENERATED_CPP_BY_NAME(CustomizableObjectSystem)

class AActor;
class UAnimInstance;
class UMaterialInterface;

namespace impl
{
	void Task_Game_Callbacks_Work(UCustomizableObjectInstance* CustomizableObjectInstance);
}

DEFINE_STAT(STAT_MutableNumSkeletalMeshes);
DEFINE_STAT(STAT_MutableNumCachedSkeletalMeshes);
DEFINE_STAT(STAT_MutableNumAllocatedSkeletalMeshes);
DEFINE_STAT(STAT_MutableNumInstancesLOD0);
DEFINE_STAT(STAT_MutableNumInstancesLOD1);
DEFINE_STAT(STAT_MutableNumInstancesLOD2);
DEFINE_STAT(STAT_MutableSkeletalMeshResourceMemory);
DEFINE_STAT(STAT_MutableNumTextures);
DEFINE_STAT(STAT_MutableNumCachedTextures);
DEFINE_STAT(STAT_MutableNumAllocatedTextures);
DEFINE_STAT(STAT_MutableTextureResourceMemory);
DEFINE_STAT(STAT_MutableTextureGeneratedMemory);
DEFINE_STAT(STAT_MutableTextureCacheMemory);
DEFINE_STAT(STAT_MutablePendingInstanceUpdates);
DEFINE_STAT(STAT_MutableAbandonedInstanceUpdates);
DEFINE_STAT(STAT_MutableInstanceBuildTime);
DEFINE_STAT(STAT_MutableInstanceBuildTimeAvrg);
DEFINE_STAT(STAT_MutableStreamingOps);

DECLARE_CYCLE_STAT(TEXT("MutablePendingRelease Time"), STAT_MutablePendingRelease, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("MutableTask"), STAT_MutableTask, STATGROUP_Game);

UCustomizableObjectSystem* FCustomizableObjectSystemPrivate::SSystem = nullptr;

static TAutoConsoleVariable<int32> CVarWorkingMemory(
	TEXT("mutable.WorkingMemory"),
#if !PLATFORM_DESKTOP
	(10 * 1024),
#else
	(50 * 1024),
#endif
	TEXT("Limit the amount of memory (in KB) to use as working memory when building characters. More memory reduces the object construction time. 0 means no restriction. Defaults: Desktop = 50,000 KB, Others = 10,000 KB"),
	ECVF_Scalability);


TAutoConsoleVariable<bool> CVarClearWorkingMemoryOnUpdateEnd(
	TEXT("mutable.ClearWorkingMemoryOnUpdateEnd"),
	false,
	TEXT("Clear the working memory and cache after every Mutable operation."),
	ECVF_Scalability);


TAutoConsoleVariable<bool> CVarReuseImagesBetweenInstances(
	TEXT("mutable.ReuseImagesBetweenInstances"),
	true,
	TEXT("Enables or disables the reuse of images between instances."),
	ECVF_Scalability);

static TAutoConsoleVariable<int32> CVarGeneratedResourcesCacheSize(
	TEXT("mutable.GeneratedResourcesCacheSize"),
	512,
	TEXT("Limit the number of resources (images and meshes) that will be tracked for reusal. Each tracked resource uses a small amout of memory for its key."),
	ECVF_Scalability);


bool FMutablePendingInstanceWork::ArePendingUpdatesEmpty() const
{
	return PendingInstanceUpdates.Num() == 0;
}


int32 FMutablePendingInstanceWork::Num() const
{
	return PendingInstanceUpdates.Num() + PendingInstanceDiscards.Num() + PendingIDsToRelease.Num() + NumLODUpdatesLastTick;
}


void FMutablePendingInstanceWork::SetLODUpdatesLastTick(int32 NumLODUpdates)
{
	NumLODUpdatesLastTick = NumLODUpdates;
}


void FMutablePendingInstanceWork::AddUpdate(const FMutablePendingInstanceUpdate& UpdateToAdd)
{
	if (FMutablePendingInstanceUpdate* ExistingUpdate = PendingInstanceUpdates.Find(UpdateToAdd.CustomizableObjectInstance))
	{
		FMutablePendingInstanceUpdate TaskToEnqueue = UpdateToAdd;

		FInstanceUpdateDelegate* Callback = ExistingUpdate->Callback;
		FinishUpdateGlobal(ExistingUpdate->CustomizableObjectInstance.Get(), EUpdateResult::ErrorReplaced, Callback);

		TaskToEnqueue.PriorityType = FMath::Min(ExistingUpdate->PriorityType, UpdateToAdd.PriorityType);
		TaskToEnqueue.SecondsAtUpdate = FMath::Min(ExistingUpdate->SecondsAtUpdate, UpdateToAdd.SecondsAtUpdate);
		
		PendingInstanceUpdates.Remove(ExistingUpdate->CustomizableObjectInstance);
		PendingInstanceUpdates.Add(TaskToEnqueue);
	}
	else
	{
		PendingInstanceUpdates.Add(UpdateToAdd);
	}

	if (FMutablePendingInstanceDiscard* ExistingDiscard = PendingInstanceDiscards.Find(UpdateToAdd.CustomizableObjectInstance))
	{
		FinishUpdateGlobal(ExistingDiscard->CustomizableObjectInstance.Get(), EUpdateResult::ErrorReplaced, nullptr);

		PendingInstanceDiscards.Remove(ExistingDiscard->CustomizableObjectInstance);
	}
}


void FMutablePendingInstanceWork::RemoveUpdate(const TWeakObjectPtr<UCustomizableObjectInstance>& Instance)
{
	PendingInstanceUpdates.Remove(Instance);
}


const FMutablePendingInstanceUpdate* FMutablePendingInstanceWork::GetUpdate(const TWeakObjectPtr<UCustomizableObjectInstance>& Instance) const
{
	return PendingInstanceUpdates.Find(Instance);
}


void FMutablePendingInstanceWork::AddDiscard(const FMutablePendingInstanceDiscard& TaskToEnqueue)
{
	if (FMutablePendingInstanceUpdate* ExistingUpdate = PendingInstanceUpdates.Find(TaskToEnqueue.CustomizableObjectInstance.Get()))
	{
		FinishUpdateGlobal(ExistingUpdate->CustomizableObjectInstance.Get(), EUpdateResult::ErrorReplaced, ExistingUpdate->Callback);
		PendingInstanceUpdates.Remove(ExistingUpdate->CustomizableObjectInstance);
	}

	PendingInstanceDiscards.Add(TaskToEnqueue);

	TaskToEnqueue.CustomizableObjectInstance->GetPrivate()->SetCOInstanceFlags(Updating);
}


void FMutablePendingInstanceWork::AddIDRelease(mu::Instance::ID IDToRelease)
{
	PendingIDsToRelease.Add(IDToRelease);
}


void FMutablePendingInstanceWork::RemoveAllUpdatesAndDiscardsAndReleases()
{
	PendingInstanceUpdates.Empty();
	PendingInstanceDiscards.Empty();
	PendingIDsToRelease.Empty();
}


UCustomizableObjectSystem* UCustomizableObjectSystem::GetInstance()
{
	if (!FCustomizableObjectSystemPrivate::SSystem)
	{
		UE_LOG(LogMutable, Log, TEXT("Creating Mutable Customizable Object System."));

		check(IsInGameThread());

		FCustomizableObjectSystemPrivate::SSystem = NewObject<UCustomizableObjectSystem>(UCustomizableObjectSystem::StaticClass());
		check(FCustomizableObjectSystemPrivate::SSystem != nullptr);
		checkf(!GUObjectArray.IsDisregardForGC(FCustomizableObjectSystemPrivate::SSystem), TEXT("Mutable was initialized too early in the UE4 init process, for instance, in the constructor of a default UObject."));
		FCustomizableObjectSystemPrivate::SSystem->AddToRoot();
		checkf(!GUObjectArray.IsDisregardForGC(FCustomizableObjectSystemPrivate::SSystem), TEXT("Mutable was initialized too early in the UE4 init process, for instance, in the constructor of a default UObject."));
		FCustomizableObjectSystemPrivate::SSystem->InitSystem();

		//FCoreUObjectDelegates::PurgePendingReleaseSkeletalMesh.AddUObject(FCustomizableObjectSystemPrivate::SSystem, &UCustomizableObjectSystem::PurgePendingReleaseSkeletalMesh);
	}

	return FCustomizableObjectSystemPrivate::SSystem;
}


FString UCustomizableObjectSystem::GetPluginVersion() const
{
	// Bridge the call from the module. This implementation is available from blueprint.
	return ICustomizableObjectModule::Get().GetPluginVersion();
}


void UCustomizableObjectSystem::LogShowData(bool bFullInfo, bool ShowMaterialInfo) const
{
	LogInformationUtil::ResetCounters();

	AActor* ParentActor;
	TArray<UCustomizableObjectInstance*> ArrayData;

	for (TObjectIterator<UCustomizableSkeletalComponent> It; It; ++It)
	{
		UCustomizableSkeletalComponent* CustomizableSkeletalComponent = *It;

		if ((CustomizableSkeletalComponent != nullptr) && (CustomizableSkeletalComponent->CustomizableObjectInstance != nullptr))
		{
			ParentActor = CustomizableSkeletalComponent->GetAttachmentRootActor();

			if (ParentActor != nullptr)
			{
				ArrayData.AddUnique(CustomizableSkeletalComponent->CustomizableObjectInstance);
			}
		}
	}

	ArrayData.Sort([](UCustomizableObjectInstance& A, UCustomizableObjectInstance& B)
	{
		check(A.GetPrivate() != nullptr);
		check(B.GetPrivate() != nullptr);
		return (A.GetPrivate()->LastMinSquareDistFromComponentToPlayer < B.GetPrivate()->LastMinSquareDistFromComponentToPlayer);
	});

	int i;
	const int Max = ArrayData.Num();

	if (bFullInfo)
	{
		for (i = 0; i < Max; ++i)
		{
			LogInformationUtil::LogShowInstanceDataFull(ArrayData[i], ShowMaterialInfo);
		}
	}
	else
	{
		FString LogData = "\n\n";
		for (i = 0; i < Max; ++i)
		{
			LogInformationUtil::LogShowInstanceData(ArrayData[i], LogData);
		}
		UE_LOG(LogMutable, Log, TEXT("%s"), *LogData);

		UWorld* World = GWorld;

		if (World)
		{
			APlayerController* PlayerController = World->GetFirstPlayerController();
			if (PlayerController)
			{
				PlayerController->ClientMessage(LogData);
			}
		}
	}
}


FCustomizableObjectSystemPrivate* UCustomizableObjectSystem::GetPrivate()
{
	return Private.Get();
}


const FCustomizableObjectSystemPrivate* UCustomizableObjectSystem::GetPrivate() const
{
	return Private.Get();
}


FCustomizableObjectSystemPrivate* UCustomizableObjectSystem::GetPrivateChecked()
{
	check(Private)
	return Private.Get();
}


const FCustomizableObjectSystemPrivate* UCustomizableObjectSystem::GetPrivateChecked() const
{
	check(Private)
	return Private.Get();
}


bool UCustomizableObjectSystem::IsCreated()
{
	return FCustomizableObjectSystemPrivate::SSystem != 0;
}


void UCustomizableObjectSystem::InitSystem()
{
	// Everything initialized in Init() instead of constructor to prevent the default UCustomizableObjectSystem from registering a tick function
	Private = MakeShareable(new FCustomizableObjectSystemPrivate());
	check(Private != nullptr);
	Private->NewCompilerFunc = nullptr;

	Private->bReplaceDiscardedWithReferenceMesh = false;

	const IConsoleVariable* CVarSupport16BitBoneIndex = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUSkin.Support16BitBoneIndex"));
	Private->bSupport16BitBoneIndex = CVarSupport16BitBoneIndex ? CVarSupport16BitBoneIndex->GetBool() : false;

	Private->CurrentMutableOperation = nullptr;
	Private->CurrentInstanceBeingUpdated = nullptr;

#if !UE_SERVER
	Private->TickDelegate = FTickerDelegate::CreateUObject(this, &UCustomizableObjectSystem::Tick);
	Private->TickDelegateHandle = FTSTicker::GetCoreTicker().AddTicker(Private->TickDelegate, 0.f);
#endif // !UE_SERVER

	Private->TotalBuildMs = 0;
	Private->TotalBuiltInstances = 0;
	Private->NumInstances = 0;
	Private->TextureMemoryUsed = 0;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	SET_DWORD_STAT(STAT_MutableNumSkeletalMeshes, 0);
	SET_DWORD_STAT(STAT_MutableNumTextures, 0);
	SET_DWORD_STAT(STAT_MutableInstanceBuildTime, 0);
	SET_DWORD_STAT(STAT_MutableInstanceBuildTimeAvrg, 0);
#endif

	Private->LastWorkingMemoryBytes = CVarWorkingMemory.GetValueOnGameThread() * 1024;
	Private->LastGeneratedResourceCacheSize = CVarGeneratedResourcesCacheSize.GetValueOnGameThread();

	mu::Ptr<mu::Settings> pSettings = new mu::Settings;
	check(pSettings);
	pSettings->SetProfile(false);
	pSettings->SetWorkingMemoryBytes(Private->LastWorkingMemoryBytes);
	Private->ExtensionDataStreamer = MakeShared<FUnrealExtensionDataStreamer>(Private.ToSharedRef());
	Private->MutableSystem = new mu::System(pSettings, Private->ExtensionDataStreamer);
	check(Private->MutableSystem);

	Private->Streamer = MakeShared<FUnrealMutableModelBulkReader>();
	check(Private->Streamer != nullptr);
	Private->MutableSystem->SetStreamingInterface(Private->Streamer);

	// Set up the external image provider, for image parameters.
	TSharedPtr<FUnrealMutableImageProvider> Provider = MakeShared<FUnrealMutableImageProvider>();
	check(Provider != nullptr);
	Private->ImageProvider = Provider;
	Private->MutableSystem->SetImageParameterGenerator(Provider);

#if WITH_EDITOR
	if (!IsRunningGame())
	{
		FEditorDelegates::PreBeginPIE.AddUObject(this, &UCustomizableObjectSystem::OnPreBeginPIE);
	}
#endif

	DefaultInstanceLODManagement = NewObject<UCustomizableInstanceLODManagement>();
	check(DefaultInstanceLODManagement != nullptr);
	CurrentInstanceLODManagement = DefaultInstanceLODManagement;
}


void UCustomizableObjectSystem::BeginDestroy()
{
#if WITH_EDITOR
	if (RecompileCustomizableObjectsCompiler)
	{
		RecompileCustomizableObjectsCompiler->ForceFinishCompilation();
		delete RecompileCustomizableObjectsCompiler;
	}

	if (!IsRunningGame())
	{
		FEditorDelegates::PreBeginPIE.RemoveAll(this);
	}

#endif

	// It could be null, for the default object.
	if (Private.IsValid())
	{

#if !UE_SERVER
		FTSTicker::GetCoreTicker().RemoveTicker(Private->TickDelegateHandle);
#endif // !UE_SERVER

		// Discard pending game thread tasks
		Private->PendingTasks.Empty();

		// Complete pending taskgraph tasks
		Private->MutableTaskGraph.WaitForMutableTasks();

		// Clear the ongoing operation
		Private->CurrentMutableOperation = nullptr;

		// Deallocate streaming
		check(Private->Streamer != nullptr);
		Private->Streamer->EndStreaming();

		Private->CurrentInstanceBeingUpdated = nullptr;

		Private->MutablePendingInstanceWork.RemoveAllUpdatesAndDiscardsAndReleases();

		FCustomizableObjectSystemPrivate::SSystem = nullptr;

		Private = nullptr;
	}

	Super::BeginDestroy();
}


FString UCustomizableObjectSystem::GetDesc()
{
	return TEXT("Customizable Object System Singleton");
}


FCustomizableObjectCompilerBase* (*FCustomizableObjectSystemPrivate::NewCompilerFunc)() = nullptr;


FCustomizableObjectCompilerBase* UCustomizableObjectSystem::GetNewCompiler()
{
	check(Private != nullptr);
	if (Private->NewCompilerFunc != nullptr)
	{
		return Private->NewCompilerFunc();
	}
	else
	{
		return nullptr;
	}
}


void UCustomizableObjectSystem::SetNewCompilerFunc(FCustomizableObjectCompilerBase* (*InNewCompilerFunc)())
{
	check(Private != nullptr);
	Private->NewCompilerFunc = InNewCompilerFunc;
}


void FCustomizableObjectSystemPrivate::CreatedTexture(UTexture2D* Texture)
{
	// TODO: Do not keep this array unless we are gathering stats!
	TextureTrackerArray.Add(Texture);

	INC_DWORD_STAT(STAT_MutableNumTextures);
}


int32 FCustomizableObjectSystemPrivate::EnableMutableAnimInfoDebugging = 0;

static FAutoConsoleVariableRef CVarEnableMutableAnimInfoDebugging(
	TEXT("mutable.EnableMutableAnimInfoDebugging"), FCustomizableObjectSystemPrivate::EnableMutableAnimInfoDebugging,
	TEXT("If set to 1 or greater print on screen the animation info of the pawn's Customizable Object Instance. Anim BPs, slots and tags will be displayed."
	"If the root Customizable Object is recompiled after this command is run, the used skeletal meshes will also be displayed."),
	ECVF_Default);


void FCustomizableObjectSystemPrivate::UpdateStats()
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	bool bLogEnabled = true;
#else
	bool bLogEnabled = LogBenchmarkUtil::isLoggingActive();
#endif
	if (bLogEnabled)
	{
		CountAllocatedSkeletalMesh = 0;

		int CountLOD0 = 0;
		int CountLOD1 = 0;
		int CountLOD2 = 0;
		int CountTotal = 0;

		for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
		{
			if ( IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate())
			{
				++CountTotal;

				for (int32 ComponentIndex = 0; ComponentIndex < CustomizableObjectInstance->SkeletalMeshes.Num(); ++ComponentIndex)
				{
					if (CustomizableObjectInstance->SkeletalMeshes[ComponentIndex] && CustomizableObjectInstance->SkeletalMeshes[ComponentIndex]->GetResourceForRendering())
					{
						CountAllocatedSkeletalMesh++;

						if (CustomizableObjectInstance->GetCurrentMinLOD() < 1)
						{
							++CountLOD0;
						}
						else if (CustomizableObjectInstance->GetCurrentMaxLOD() < 2)
						{
							++CountLOD1;
						}
						else
						{
							++CountLOD2;
						}
					}
				}
			}
		}

		NumInstances = CountLOD0 + CountLOD1 + CountLOD2;
		TotalInstances = CountTotal;
		NumPendingInstances = MutablePendingInstanceWork.Num();
		//SET_DWORD_STAT(STAT_MutableSkeletalMeshResourceMemory, Size / 1024.f);
		SET_DWORD_STAT(STAT_MutablePendingInstanceUpdates, MutablePendingInstanceWork.Num());

		uint64 Size = 0;
		uint32 CountAllocated = 0;
		for (TWeakObjectPtr<UTexture2D>& Tracker : TextureTrackerArray)
		{
			if (Tracker.IsValid() && Tracker->GetResource())
			{
				CountAllocated++;

				if (Tracker->GetResource()->TextureRHI)
				{
					Size += Tracker->CalcTextureMemorySizeEnum(TMC_AllMips);
				}
			}
		}

		TextureMemoryUsed = int64_t(Size / 1024);

		uint64 SizeGenerated = 0;
		for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
		{
			if (IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate() && CustomizableObjectInstance->HasAnySkeletalMesh())
			{
				bool bHasResourceForRendering = false;
				for (int32 MeshIndex = 0; !bHasResourceForRendering && MeshIndex < CustomizableObjectInstance->SkeletalMeshes.Num(); ++MeshIndex)
				{
					bHasResourceForRendering = CustomizableObjectInstance->GetSkeletalMesh(MeshIndex) && CustomizableObjectInstance->GetSkeletalMesh(MeshIndex)->GetResourceForRendering();
				}

				if (bHasResourceForRendering)
				{
					for (const FGeneratedTexture& GeneratedTextures : CustomizableObjectInstance->GetPrivate()->GeneratedTextures)
					{
						if (GeneratedTextures.Texture)
						{
							check(GeneratedTextures.Texture != nullptr);
							SizeGenerated += GeneratedTextures.Texture->CalcTextureMemorySizeEnum(TMC_AllMips);
						}
					}
				}
			}
		}

		if (LogBenchmarkUtil::isLoggingActive())
		{
			LogBenchmarkUtil::updateStat("customizable_objects", (int32)CountAllocatedSkeletalMesh);
			LogBenchmarkUtil::updateStat("pending_instance_updates", MutablePendingInstanceWork.Num());
			LogBenchmarkUtil::updateStat("allocated_textures", (int32)CountAllocated);
			LogBenchmarkUtil::updateStat("texture_resource_memory", (long double)Size / 1048576.0L);
			LogBenchmarkUtil::updateStat("texture_generated_memory", (long double)SizeGenerated / 1048576.0L);
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		SET_DWORD_STAT(STAT_MutableNumInstancesLOD0, CountLOD0);
		SET_DWORD_STAT(STAT_MutableNumInstancesLOD1, CountLOD1);
		SET_DWORD_STAT(STAT_MutableNumInstancesLOD2, CountLOD2);
		SET_DWORD_STAT(STAT_MutableNumAllocatedSkeletalMeshes, CountAllocatedSkeletalMesh);
		SET_DWORD_STAT(STAT_MutablePendingInstanceUpdates, MutablePendingInstanceWork.Num());
		SET_DWORD_STAT(STAT_MutableNumAllocatedTextures, CountAllocated);
		SET_DWORD_STAT(STAT_MutableTextureResourceMemory, Size / 1024.f);
		SET_DWORD_STAT(STAT_MutableTextureGeneratedMemory, SizeGenerated / 1024.f);
#endif

#if WITH_EDITORONLY_DATA
		if (FCustomizableObjectSystemPrivate::IsMutableAnimInfoDebuggingEnabled())
		{
			if (GEngine)
			{
				bool bFoundPlayer = false;
				int32 MsgIndex = 15820; // Arbitrary big value to prevent collisions with other on-screen messages

				for (TObjectIterator<UCustomizableSkeletalComponent> CustomizableSkeletalComponent; CustomizableSkeletalComponent; ++CustomizableSkeletalComponent)
				{
					AActor* ParentActor = CustomizableSkeletalComponent->GetAttachmentRootActor();
					UCustomizableObjectInstance* Instance = CustomizableSkeletalComponent->CustomizableObjectInstance;

					APawn* PlayerPawn = nullptr;
					if (UWorld* World = CustomizableSkeletalComponent->GetWorld())
					{
						PlayerPawn = UGameplayStatics::GetPlayerPawn(World, 0);						
					}
					
					if (ParentActor && (ParentActor == PlayerPawn) && Instance)
					{
						bFoundPlayer = true;

						FString TagString;
						const FGameplayTagContainer& Tags = Instance->GetAnimationGameplayTags();

						for (const FGameplayTag& Tag : Tags)
						{
							TagString += !TagString.IsEmpty() ? FString(TEXT(", ")) : FString();
							TagString += Tag.ToString();
						}												
												
						GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Green, TEXT("Animation tags: ") + TagString);

						check(Instance->GetPrivate() != nullptr);
						FCustomizableInstanceComponentData* ComponentData = Instance->GetPrivate()->GetComponentData(CustomizableSkeletalComponent->ComponentIndex);

						if (ComponentData)
						{
							for (TPair<FName, TSoftClassPtr<UAnimInstance>>& Entry : ComponentData->AnimSlotToBP)
							{
								FString AnimBPSlot;

								AnimBPSlot += Entry.Key.ToString() + FString("-") + Entry.Value.GetAssetName();
								GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Green, AnimBPSlot);
							}
						}

						GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Green, TEXT("Slots-AnimBP: "));

						if (ComponentData)
						{
							if (ComponentData->MeshPartPaths.IsEmpty())
							{
								GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Magenta,
									TEXT("No meshes found. In order to see the meshes compile the pawn's root CustomizableObject after the 'mutable.EnableMutableAnimInfoDebugging 1' command has been run."));
							}

							for (const FString& MeshPath : ComponentData->MeshPartPaths)
							{
								GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Magenta, MeshPath);
							}
						}

						GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Magenta, TEXT("Meshes: "));

						GEngine->AddOnScreenDebugMessage(MsgIndex++, .0f, FColor::Cyan,
							TEXT("Player Pawn Mutable Mesh/Animation info for component ") + FString::Printf(TEXT("%d"), 
							CustomizableSkeletalComponent->ComponentIndex));
					}
				}

				if (!bFoundPlayer)
				{
					GEngine->AddOnScreenDebugMessage(MsgIndex, .0f, FColor::Yellow, TEXT("Mutable Animation info: N/A"));
				}
			}
		}
#endif
	}
}


void FCustomizableObjectSystemPrivate::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (CurrentInstanceBeingUpdated)
	{
		Collector.AddReferencedObject(CurrentInstanceBeingUpdated);
	}
}


int32 FCustomizableObjectSystemPrivate::EnableMutableProgressiveMipStreaming = 1;

// Warning! If this is enabled, do not get references to the textures generated by Mutable! They are owned by Mutable and could become invalid at any moment
static FAutoConsoleVariableRef CVarEnableMutableProgressiveMipStreaming(
	TEXT("mutable.EnableMutableProgressiveMipStreaming"), FCustomizableObjectSystemPrivate::EnableMutableProgressiveMipStreaming,
	TEXT("If set to 1 or greater use progressive Mutable Mip streaming for Mutable textures. If disabled, all mips will always be generated and spending memory. In that case, on Desktop platforms they will be stored in CPU memory, on other platforms textures will be non-streaming."),
	ECVF_Default);


int32 FCustomizableObjectSystemPrivate::EnableMutableLiveUpdate = 1;

static FAutoConsoleVariableRef CVarEnableMutableLiveUpdate(
	TEXT("mutable.EnableMutableLiveUpdate"), FCustomizableObjectSystemPrivate::EnableMutableLiveUpdate,
	TEXT("If set to 1 or greater Mutable can use the live update mode if set in the current Mutable state. If disabled, it will never use live update mode even if set in the current Mutable state."),
	ECVF_Default);


int32 FCustomizableObjectSystemPrivate::EnableReuseInstanceTextures = 1;

static FAutoConsoleVariableRef CVarEnableMutableReuseInstanceTextures(
	TEXT("mutable.EnableReuseInstanceTextures"), FCustomizableObjectSystemPrivate::EnableReuseInstanceTextures,
	TEXT("If set to 1 or greater and set in the corresponding setting in the current Mutable state, Mutable can reuse instance UTextures (only uncompressed and not streaming, so set the options in the state) and their resources between updates when they are modified. If geometry or state is changed they cannot be reused."),
	ECVF_Default);


bool FCustomizableObjectSystemPrivate::bEnableMutableReusePreviousUpdateData = false;
static FAutoConsoleVariableRef CVarEnableMutableReusePreviousUpdateData(
	TEXT("mutable.EnableMutableReusePreviousUpdateData"), FCustomizableObjectSystemPrivate::bEnableMutableReusePreviousUpdateData,
	TEXT("If true, Mutable will try to reuse render sections from previous SkeletalMeshes. If false, all SkeletalMeshes will be build from scratch."),
	ECVF_Default);


int32 FCustomizableObjectSystemPrivate::EnableOnlyGenerateRequestedLODs = 1;

static FAutoConsoleVariableRef CVarEnableOnlyGenerateRequestedLODs(
	TEXT("mutable.EnableOnlyGenerateRequestedLODs"), FCustomizableObjectSystemPrivate::EnableOnlyGenerateRequestedLODs,
	TEXT("If 1 or greater, Only the RequestedLODLevels will be generated. If 0, all LODs will be build."),
	ECVF_Default);

int32 FCustomizableObjectSystemPrivate::EnableSkipGenerateResidentMips = 1;

static FAutoConsoleVariableRef CVarSkipGenerateResidentMips(
	TEXT("mutable.EnableSkipGenerateResidentMips"), FCustomizableObjectSystemPrivate::EnableSkipGenerateResidentMips,
	TEXT("If 1 or greater, resident mip generation will be optional. If 0, resident mips will be always generated"),
	ECVF_Default);

int32 FCustomizableObjectSystemPrivate::MaxTextureSizeToGenerate = 0;

FAutoConsoleVariableRef CVarMaxTextureSizeToGenerate(
	TEXT("Mutable.MaxTextureSizeToGenerate"),
	FCustomizableObjectSystemPrivate::MaxTextureSizeToGenerate,
	TEXT("Max texture size on Mutable textures. Mip 0 will be the first mip with max size equal or less than MaxTextureSizeToGenerate."
		"If a texture doesn't have small enough mips, mip 0 will be the last mip available."));


void FinishUpdateGlobal(UCustomizableObjectInstance* Instance, EUpdateResult UpdateResult, FInstanceUpdateDelegate* UpdateCallback, const FDescriptorRuntimeHash InUpdatedHash)
{
	check(IsInGameThread())
	
	// Callbacks. Must be done at the end.
	if (Instance)
	{
		Instance->FinishUpdate(UpdateResult, InUpdatedHash);			
	}

	if (UpdateResult == EUpdateResult::Success)
	{
		// Call Customizable Skeletal Components updated callbacks.
		for (TObjectIterator<UCustomizableSkeletalComponent> It; It; ++It) // Since iterating objects is expensive, for now CustomizableSkeletalComponent does not have a FinishUpdate function.
		{
			if (const UCustomizableSkeletalComponent* CustomizableSkeletalComponent = *It;
				CustomizableSkeletalComponent &&
				CustomizableSkeletalComponent->CustomizableObjectInstance == Instance)
			{
				CustomizableSkeletalComponent->Callbacks();
			}
		}
	}

	if (UpdateCallback)
	{
		FUpdateContext Context;
		Context.UpdateResult = UpdateResult;
		
		UpdateCallback->ExecuteIfBound(Context);
	}

	UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableTaskGraph.AllowLaunchingMutableTaskLowPriority(true, false);
}


/** Update the given Instance Skeletal Meshes and call its callbacks. */
void UpdateSkeletalMesh(UCustomizableObjectInstance& CustomizableObjectInstance, const FDescriptorRuntimeHash& UpdatedDescriptorRuntimeHash, EUpdateResult UpdateResult, FInstanceUpdateDelegate* UpdateCallback)
{
	MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh);

	check(IsInGameThread());

	for (int32 ComponentIndex = 0; ComponentIndex < CustomizableObjectInstance.SkeletalMeshes.Num(); ++ComponentIndex)
	{
		if (TObjectPtr<USkeletalMesh> SkeletalMesh = CustomizableObjectInstance.SkeletalMeshes[ComponentIndex])
		{
#if WITH_EDITOR
			FUnrealBakeHelpers::BakeHelper_RegenerateImportedModel(SkeletalMesh);
#else
			SkeletalMesh->RebuildSocketMap();
#endif
		}
	}

	UCustomizableInstancePrivateData* CustomizableObjectInstancePrivateData = CustomizableObjectInstance.GetPrivate();
	check(CustomizableObjectInstancePrivateData != nullptr);
	for (TObjectIterator<UCustomizableSkeletalComponent> It; It; ++It)
	{
		UCustomizableSkeletalComponent* CustomizableSkeletalComponent = *It;

		if (CustomizableSkeletalComponent &&
			(CustomizableSkeletalComponent->CustomizableObjectInstance == &CustomizableObjectInstance) &&
			CustomizableObjectInstance.SkeletalMeshes.IsValidIndex(CustomizableSkeletalComponent->ComponentIndex)
		   )
		{
			MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_SetSkeletalMesh);

			const bool bIsCreatingSkeletalMesh = CustomizableObjectInstancePrivateData->HasCOInstanceFlags(CreatingSkeletalMesh); //TODO MTBL-391: Review
			CustomizableSkeletalComponent->SetSkeletalMesh(CustomizableObjectInstance.SkeletalMeshes[CustomizableSkeletalComponent->ComponentIndex], false, bIsCreatingSkeletalMesh);

			if (CustomizableObjectInstancePrivateData->HasCOInstanceFlags(ReplacePhysicsAssets))
			{
				CustomizableSkeletalComponent->SetPhysicsAsset(
					CustomizableObjectInstance.SkeletalMeshes[CustomizableSkeletalComponent->ComponentIndex] ? 
					CustomizableObjectInstance.SkeletalMeshes[CustomizableSkeletalComponent->ComponentIndex]->GetPhysicsAsset() : nullptr);
			}
		}
	}

	CustomizableObjectInstancePrivateData->SetCOInstanceFlags(Generated);
	CustomizableObjectInstancePrivateData->ClearCOInstanceFlags(CreatingSkeletalMesh);

	CustomizableObjectInstance.bEditorPropertyChanged = false;

	FinishUpdateGlobal(&CustomizableObjectInstance, UpdateResult, UpdateCallback, UpdatedDescriptorRuntimeHash);
}


void FCustomizableObjectSystemPrivate::GetMipStreamingConfig(const UCustomizableObjectInstance& Instance, bool& bOutNeverStream, int32& OutMipsToSkip) const
{
	const FString CurrentState = Instance.GetCurrentState();
	const FParameterUIData* State = Instance.GetCustomizableObject()->StateUIDataMap.Find(CurrentState);

	// \TODO: This should be controllable independently
	bOutNeverStream = State ? State->TextureCompressionStrategy != ETextureCompressionStrategy::None : false;
	bool bUseMipmapStreaming = !bOutNeverStream;
	OutMipsToSkip = 0; // 0 means all mips

#if PLATFORM_SUPPORTS_TEXTURE_STREAMING
	if (!IStreamingManager::Get().IsTextureStreamingEnabled())
	{
		bUseMipmapStreaming = false;
	}
#else
	bUseMipmapStreaming = false;
#endif

	if (bUseMipmapStreaming && EnableMutableProgressiveMipStreaming)
	{
		OutMipsToSkip = 255; // This means skip all possible mips until only UTexture::GetStaticMinTextureResidentMipCount() are left
	}
}


void FCustomizableObjectSystemPrivate::InitUpdateSkeletalMesh(UCustomizableObjectInstance& Instance, EQueuePriorityType Priority, bool bIsCloseDistTick, FInstanceUpdateDelegate* UpdateCallback)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectSystemPrivate::InitUpdateSkeletalMesh);

	check(IsInGameThread());

	bool bNeverStream = false;
	int32 MipsToSkip = 0;

	GetMipStreamingConfig(Instance, bNeverStream, MipsToSkip);
	
	const FDescriptorRuntimeHash UpdateDescriptorHash = Instance.GetUpdateDescriptorRuntimeHash();

	if (const FMutablePendingInstanceUpdate* QueueElem = MutablePendingInstanceWork.GetUpdate(&Instance))
	{
		if (UpdateDescriptorHash.IsSubset(FDescriptorRuntimeHash(QueueElem->InstanceDescriptor)))
		{
			FinishUpdateGlobal(&Instance, EUpdateResult::ErrorOptimized, UpdateCallback);			
			return; // The the requested update is equal to the last enqueued update.
		}
	}	

	if (CurrentMutableOperation &&
		&Instance == CurrentMutableOperation->CustomizableObjectInstance &&
		UpdateDescriptorHash.IsSubset(CurrentMutableOperation->InstanceDescriptorRuntimeHash))
	{
		FinishUpdateGlobal(&Instance, EUpdateResult::ErrorOptimized, UpdateCallback);			
		return; // The requested update is equal to the running update.
	}
	
	// These delegates must be called at the end of the begin update.
	Instance.BeginUpdateDelegate.Broadcast(&Instance);
	Instance.BeginUpdateNativeDelegate.Broadcast(&Instance);

	if (UpdateDescriptorHash.IsSubset(Instance.GetDescriptorRuntimeHash()) &&
		!(CurrentMutableOperation &&
			&Instance == CurrentMutableOperation->CustomizableObjectInstance)) // This condition is necessary because even if the descriptor is a subset, it will be replaced by the CurrentMutableOperation
	{
		Instance.SkeletalMeshStatus = ESkeletalMeshState::Correct; // TODO FutureGMT MTBL-1033 should not be here. Move to UCustomizableObjectInstance::Updated
		UpdateSkeletalMesh(Instance, Instance.GetDescriptorRuntimeHash(), EUpdateResult::Success, UpdateCallback);
	}
	else
	{
		// Cache Texture Parameters being used during the update:
		check(ImageProvider);

		// Cache new Texture Parameters
		for (const FCustomizableObjectTextureParameterValue& TextureParameters : Instance.GetDescriptor().GetTextureParameters())
		{
			ImageProvider->CacheImage(TextureParameters.ParameterValue, false);

			for (const FName& TextureParameter : TextureParameters.ParameterRangeValues)
			{
				ImageProvider->CacheImage(TextureParameter, false);
			}
		}

		UCustomizableInstancePrivateData* InstancePrivate = Instance.GetPrivate();
		check(InstancePrivate);

		// Uncache old Texture Parameters
		for (const FName& TextureParameter : InstancePrivate->UpdateTextureParameters)
		{
			ImageProvider->UnCacheImage(TextureParameter, false);
		}

		// Update which ones are currently are being used
		InstancePrivate->UpdateTextureParameters.Reset();
		for (const FCustomizableObjectTextureParameterValue& TextureParameters : Instance.GetDescriptor().GetTextureParameters())
		{
			InstancePrivate->UpdateTextureParameters.Add(TextureParameters.ParameterValue);

			for (const FName& TextureParameter : TextureParameters.ParameterRangeValues)
			{
				InstancePrivate->UpdateTextureParameters.Add(TextureParameter);
			}
		}

		Instance.SkeletalMeshStatus = ESkeletalMeshState::AsyncUpdatePending;

		if (!bIsCloseDistTick) // When called from bIsCloseDistTick, the update operation is directly processed without going to a queue first
		{
			const FMutablePendingInstanceUpdate InstanceUpdate(&Instance, Priority, UpdateCallback, bNeverStream, MipsToSkip);
			MutablePendingInstanceWork.AddUpdate(InstanceUpdate);
		}
	}
}


void FCustomizableObjectSystemPrivate::InitDiscardResourcesSkeletalMesh(UCustomizableObjectInstance* InCustomizableObjectInstance)
{
	check(IsInGameThread());

	if (InCustomizableObjectInstance && InCustomizableObjectInstance->IsValidLowLevel())
	{
		check(InCustomizableObjectInstance->GetPrivate() != nullptr);
		MutablePendingInstanceWork.AddDiscard(FMutablePendingInstanceDiscard(InCustomizableObjectInstance));
	}
}


void FCustomizableObjectSystemPrivate::InitInstanceIDRelease(mu::Instance::ID IDToRelease)
{
	check(IsInGameThread());

	MutablePendingInstanceWork.AddIDRelease(IDToRelease);
}


bool UCustomizableObjectSystem::IsReplaceDiscardedWithReferenceMeshEnabled() const
{
	if (Private.IsValid())
	{
		return Private->IsReplaceDiscardedWithReferenceMeshEnabled();
	}

	return false;
}


void UCustomizableObjectSystem::SetReplaceDiscardedWithReferenceMeshEnabled(bool bIsEnabled)
{
	if (Private.IsValid())
	{
		Private->SetReplaceDiscardedWithReferenceMeshEnabled(bIsEnabled);
	}
}


void UCustomizableObjectSystem::ClearResourceCacheProtected()
{
	check(IsInGameThread());

	ProtectedCachedTextures.Reset(0);
	check(GetPrivate() != nullptr);
	GetPrivate()->ProtectedObjectCachedImages.Reset(0);
}


#if WITH_EDITOR
bool UCustomizableObjectSystem::LockObject(const class UCustomizableObject* InObject)
{
	check(InObject != nullptr);
	check(!InObject->GetPrivate()->bLocked);
	check(IsInGameThread() && !IsInParallelGameThread());

	if (InObject && InObject->GetPrivate() && Private)
	{
		// If the current instance is for this object, make the lock fail by returning false
		if (Private->CurrentInstanceBeingUpdated &&
			Private->CurrentInstanceBeingUpdated->GetCustomizableObject() == InObject)
		{
			UE_LOG(LogMutable, Warning, TEXT("---- failed to lock object %s"), *InObject->GetName());

			return false;
		}

		FString Message = FString::Printf(TEXT("Customizable Object %s has pending texture streaming operations. Please wait a few seconds and try again."),
			*InObject->GetName());

		// Pre-check pending operations before locking. This check is redundant and incomplete because it's checked again after locking 
		// and some operations may start between here and the actual lock. But in the CO Editor preview it will prevent some 
		// textures getting stuck at low resolution when they try to update mips and are cancelled when the user presses 
		// the compile button but the compilation quits anyway because there are pending operations
		if (CheckIfDiskOrMipUpdateOperationsPending(*InObject))
		{
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Message);

			return false;
		}

		// Lock the object, no new file or mip streaming operations should start from this point
		InObject->GetPrivate()->bLocked = true;

		// But some could have started between the first CheckIfDiskOrMipUpdateOperationsPending and the lock a few lines back, so check again
		if (CheckIfDiskOrMipUpdateOperationsPending(*InObject))
		{
			UE_LOG(LogMutable, Warning, TEXT("%s"), *Message);

			// Unlock and return because the pending operations cannot be easily stopped now, the compilation hasn't started and the CO
			// hasn't changed state yet. It's simpler to quit the compilation, unlock and let the user try to compile again
			InObject->GetPrivate()->bLocked = false;

			return false;
		}

		// Ensure that we don't try to handle any further streaming operations for this object
		check(GetPrivate() != nullptr);
		if (GetPrivate()->Streamer)
		{
			GetPrivate()->Streamer->CancelStreamingForObject(InObject);
		}

		// Clear the cache for the instance, since we will remake it
		FMutableResourceCache& Cache = GetPrivate()->GetObjectCache(InObject);
		Cache.Clear();

		check(InObject->GetPrivate()->bLocked);

		return true;
	}
	else
	{
		FString ObjectName = InObject ? InObject->GetName() : FString("null");
		UE_LOG(LogMutable, Warning, TEXT("Failed to lock the object [%s] because it was null or the system was null or partially destroyed."), *ObjectName);

		return false;
	}
}


void UCustomizableObjectSystem::UnlockObject(const class UCustomizableObject* Obj)
{
	check(Obj != nullptr);
	check(Obj->GetPrivate()->bLocked);
	check(IsInGameThread() && !IsInParallelGameThread());
	
	if (Obj && Obj->GetPrivate())
	{
		Obj->GetPrivate()->bLocked = false;
	}
}


bool UCustomizableObjectSystem::CheckIfDiskOrMipUpdateOperationsPending(const UCustomizableObject& Object) const
{
	for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
	{
		if (CustomizableObjectInstance->GetCustomizableObject() == &Object)
		{
			for (const FGeneratedTexture& GeneratedTexture : CustomizableObjectInstance->GetPrivate()->GeneratedTextures)
			{
				if (GeneratedTexture.Texture->HasPendingInitOrStreaming())
				{
					return true;
				}
			}
		}
	}

	// Ensure that we don't try to handle any further streaming operations for this object
	check(GetPrivate());
	if (const FUnrealMutableModelBulkReader* Streamer = GetPrivate()->Streamer.Get())
	{
		if (Streamer->AreTherePendingStreamingOperationsForObject(&Object))
		{
			return true;
		}
	}

	return false;
}


void UCustomizableObjectSystem::EditorSettingsChanged(const FEditorCompileSettings& InEditorSettings)
{
	EditorSettings = InEditorSettings;
}


bool UCustomizableObjectSystem::IsCompilationDisabled() const
{
	return EditorSettings.bDisableCompilation;
}


bool UCustomizableObjectSystem::IsAutoCompileEnabled() const
{
	return EditorSettings.bEnableAutomaticCompilation;
}


bool UCustomizableObjectSystem::IsAutoCompilationSync() const
{
	return EditorSettings.bCompileObjectsSynchronously;
}

#endif


void UCustomizableObjectSystem::PurgePendingReleaseSkeletalMesh()
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObjectSystem::PurgePendingReleaseSkeletalMesh);

	double	CurTime = FPlatformTime::Seconds();
	static double TimeToDelete = 1.0;

	for (int32 InfoIndex = PendingReleaseSkeletalMesh.Num() - 1; InfoIndex >= 0; --InfoIndex)
	{
		FPendingReleaseSkeletalMeshInfo& Info = PendingReleaseSkeletalMesh[InfoIndex];
		
		if (Info.SkeletalMesh != nullptr)
		{
			if ((CurTime - Info.TimeStamp) >= TimeToDelete)
			{
				if (Info.SkeletalMesh->GetSkeleton())
				{
					Info.SkeletalMesh->GetSkeleton()->ClearCacheData();
				}
				Info.SkeletalMesh->GetRefSkeleton().Empty();
				Info.SkeletalMesh->GetMaterials().Empty();
				Info.SkeletalMesh->GetRefBasesInvMatrix().Empty();
				Info.SkeletalMesh->ReleaseResources();
				Info.SkeletalMesh->ReleaseResourcesFence.Wait();

				PendingReleaseSkeletalMesh.RemoveAt(InfoIndex);
			}
		}
	}
}


void UCustomizableObjectSystem::AddPendingReleaseSkeletalMesh(USkeletalMesh* SkeletalMesh)
{
	check(SkeletalMesh != nullptr);

	FPendingReleaseSkeletalMeshInfo	Info;
	Info.SkeletalMesh = SkeletalMesh;
	Info.TimeStamp = FPlatformTime::Seconds();

	PendingReleaseSkeletalMesh.Add(Info);
}


void UCustomizableObjectSystem::ClearCurrentMutableOperation()
{
	check(Private != nullptr);
	Private->CurrentInstanceBeingUpdated = nullptr;
	Private->CurrentMutableOperation = nullptr;
	ClearResourceCacheProtected();
}


void FCustomizableObjectSystemPrivate::UpdateMemoryLimit()
{
	// This must run on game thread, and when the mutable thread is not running
	check(IsInGameThread());

	const uint64 MemoryBytes = CVarWorkingMemory.GetValueOnGameThread() * 1024;
	if (MemoryBytes != LastWorkingMemoryBytes)
	{
		LastWorkingMemoryBytes = MemoryBytes;
		check(MutableSystem);
		MutableSystem->SetWorkingMemoryBytes(MemoryBytes);
	}

	const uint32 GeneratedResourceCacheSize = CVarGeneratedResourcesCacheSize.GetValueOnGameThread();
	if (GeneratedResourceCacheSize != LastGeneratedResourceCacheSize)
	{
		LastGeneratedResourceCacheSize = GeneratedResourceCacheSize;
		check(MutableSystem);
		MutableSystem->SetGeneratedCacheSize(GeneratedResourceCacheSize);
	}
}


// Asynchronous tasks performed during the creation or update of a mutable instance. 
// Check the documentation before modifying and keep it up to date.
// https://docs.google.com/drawings/d/109NlsdKVxP59K5TuthJkleVG3AROkLJr6N03U4bNp4s
// When it says "mutable thread" it means any task pool thread, but with the guarantee that no other thread is using the mutable runtime.
// Naming: Task_<thread>_<description>
namespace impl
{

	void Subtask_Mutable_UpdateParameterRelevancy(const TSharedPtr<FMutableOperationData>& OperationData, const mu::Parameters* MutableParameters)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_UpdateParameterRelevancy)

		check(OperationData);
		OperationData->RelevantParametersInProgress.Empty();

		check(MutableParameters);
		check(OperationData->InstanceID != 0);

		// This must run in the mutable thread.
		check(UCustomizableObjectSystem::GetInstance() != nullptr);
		check(UCustomizableObjectSystem::GetInstance()->GetPrivate() != nullptr);
		mu::SystemPtr MutableSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableSystem;

		int32 NumParameters = MutableParameters->GetCount();

		// Update the parameter relevancy.
		{
			MUTABLE_CPUPROFILER_SCOPE(ParameterRelevancy)

			TArray<bool> Relevant;
			Relevant.SetNumZeroed(NumParameters);
			MutableSystem->GetParameterRelevancy(OperationData->InstanceID, MutableParameters, Relevant.GetData());

			for (int32 ParamIndex = 0; ParamIndex < NumParameters; ++ParamIndex)
			{
				if (Relevant[ParamIndex])
				{
					OperationData->RelevantParametersInProgress.Add(ParamIndex);
				}
			}
		}
	}


	// This runs in the mutable thread.
	void Subtask_Mutable_BeginUpdate_GetMesh(const TSharedPtr<FMutableOperationData>& OperationData, TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model, const mu::Parameters* MutableParameters, int32 State)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_BeginUpdate_GetMesh)

		check(MutableParameters);
		check(OperationData);
		OperationData->InstanceUpdateData.Clear();

		check(UCustomizableObjectSystem::GetInstance() != nullptr);
		check(UCustomizableObjectSystem::GetInstance()->GetPrivate() != nullptr);
		mu::System* System = UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableSystem.get();
		check(System != nullptr);

		if (OperationData->bLiveUpdateMode)
		{
			if (OperationData->InstanceID == 0)
			{
				// It's the first update since the instance was put in LiveUpdate Mode, this ID will be reused from now on
				OperationData->InstanceID = System->NewInstance(Model);
				UE_LOG(LogMutable, Verbose, TEXT("Creating Mutable instance with id [%d] for reuse "), OperationData->InstanceID);
			}
			else
			{
				// The instance was already in LiveUpdate Mode, the ID is reused
				check(OperationData->InstanceID);
				UE_LOG(LogMutable, Verbose, TEXT("Reusing Mutable instance with id [%d] "), OperationData->InstanceID);
			}
		}
		else
		{
			// In non-LiveUpdate mode, we are forcing the recreation of mutable-side instances with every update.
			check(OperationData->InstanceID == 0);
			OperationData->InstanceID = System->NewInstance(Model);
			UE_LOG(LogMutable, Verbose, TEXT("Creating Mutable instance with id [%d] "), OperationData->InstanceID);
		}

		check(OperationData->InstanceID != 0);

		const mu::Instance* Instance = nullptr;

		// Main instance generation step
		{
			// LOD mask, set to all ones to build  all LODs
			uint32 LODMask = mu::System::AllLODs;

			Instance = System->BeginUpdate(OperationData->InstanceID, MutableParameters, State, LODMask);

			if (!Instance)
			{
				UE_LOG(LogMutable, Warning, TEXT("An Instace update has failed."));
				return;
			}
		}

		OperationData->NumLODsAvailable = Instance->GetLODCount();

		if (OperationData->CurrentMinLOD >= OperationData->NumLODsAvailable)
		{
			OperationData->CurrentMinLOD = OperationData->NumLODsAvailable - 1;
			OperationData->CurrentMaxLOD = OperationData->CurrentMinLOD;
		}
		else if (OperationData->CurrentMaxLOD >= OperationData->NumLODsAvailable)
		{
			OperationData->CurrentMaxLOD = OperationData->NumLODsAvailable - 1;
		}

		if(!OperationData->RequestedLODs.IsEmpty())
		{
			// Initialize RequestedLODs to zero if not set
			const int32 ComponentCount = Instance->GetComponentCount(OperationData->CurrentMinLOD);
			OperationData->RequestedLODs.SetNumZeroed(ComponentCount);
			
			for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
			{
				// Ensure we're generating at least one LOD
				if (OperationData->RequestedLODs[ComponentIndex] == 0)
				{
					OperationData->RequestedLODs[ComponentIndex] |= (1 << OperationData->CurrentMaxLOD);
				}

				// Make sure we are not requesting a LOD that doesn't exist in this state (Essentially for states with bBuildOnlyFirstLOD 
				// and NumExtraLODsToBuildPerPlatform when the ExtraLOD is not needed)
				if ((OperationData->RequestedLODs[ComponentIndex] & (1 << OperationData->CurrentMaxLOD)) == 0)
				{
					OperationData->CurrentMaxLOD = OperationData->CurrentMinLOD;

					// Ensure the fallback LOD actually exists
					OperationData->RequestedLODs[ComponentIndex] |= (1 << OperationData->CurrentMaxLOD);
				}
			}
		}

		// Map SharedSurfaceId to surface index
		TArray<int32> SurfacesSharedId;

		// Generate the mesh and gather all the required resource Ids
		OperationData->InstanceUpdateData.LODs.SetNum(Instance->GetLODCount());
		for (int32 MutableLODIndex = 0; MutableLODIndex < Instance->GetLODCount(); ++MutableLODIndex)
		{
			// Skip LODs outside the reange we want to generate
			if (MutableLODIndex < OperationData->CurrentMinLOD || MutableLODIndex > OperationData->CurrentMaxLOD)
			{
				continue;
			}

			FInstanceUpdateData::FLOD& LOD = OperationData->InstanceUpdateData.LODs[MutableLODIndex];
			LOD.FirstComponent = OperationData->InstanceUpdateData.Components.Num();
			LOD.ComponentCount = Instance->GetComponentCount(MutableLODIndex);

			const FInstanceGeneratedData::FLOD& PrevUpdateLODData = OperationData->LastUpdateData.LODs.IsValidIndex(MutableLODIndex) ?
				OperationData->LastUpdateData.LODs[MutableLODIndex] : FInstanceGeneratedData::FLOD();


			for (int32 ComponentIndex = 0; ComponentIndex < LOD.ComponentCount; ++ComponentIndex)
			{
				OperationData->InstanceUpdateData.Components.Push(FInstanceUpdateData::FComponent());
				FInstanceUpdateData::FComponent& Component = OperationData->InstanceUpdateData.Components.Last();
				Component.Id = Instance->GetComponentId(MutableLODIndex, ComponentIndex);
				Component.FirstSurface = OperationData->InstanceUpdateData.Surfaces.Num();
				Component.SurfaceCount = 0;

				const FInstanceGeneratedData::FComponent& PrevUpdateComponent = OperationData->LastUpdateData.Components.IsValidIndex(PrevUpdateLODData.FirstComponent + ComponentIndex) ?
					OperationData->LastUpdateData.Components[PrevUpdateLODData.FirstComponent +  ComponentIndex] : FInstanceGeneratedData::FComponent();

				const bool bGenerateLOD = OperationData->RequestedLODs.IsValidIndex(Component.Id) ? (OperationData->RequestedLODs[Component.Id] & (1 << MutableLODIndex)) != 0 : true;

				// Mesh
				if (Instance->GetMeshCount(MutableLODIndex, ComponentIndex) > 0)
				{
					MUTABLE_CPUPROFILER_SCOPE(GetMesh);

					Component.MeshID = Instance->GetMeshId(MutableLODIndex, ComponentIndex, 0);

					// Mesh cache is not enabled yet.
					//auto CachedMesh = Cache.Meshes.Find(meshId);
					//if (CachedMesh && CachedMesh->IsValid(false, true))
					//{
					//	UE_LOG(LogMutable, Verbose, TEXT("Mesh resource with id [%d] can be cached."), meshId);
					//	INC_DWORD_STAT(STAT_MutableNumCachedSkeletalMeshes);
					//}

					// Check if we can use data from the currently generated SkeletalMesh
					Component.bReuseMesh = OperationData->bCanReuseGeneratedData && PrevUpdateComponent.bGenerated ? PrevUpdateComponent.MeshID == Component.MeshID : false;

					if(bGenerateLOD && !Component.bReuseMesh)
					{
						Component.Mesh = System->GetMesh(OperationData->InstanceID, Component.MeshID);
					}
				}

				if (!Component.Mesh && !Component.bReuseMesh)
				{
					continue;
				}

				Component.bGenerated = true;

				const int32 SurfaceCount = Component.Mesh ? Component.Mesh->GetSurfaceCount() : PrevUpdateComponent.SurfaceCount;

				// Materials and images
				for (int32 MeshSurfaceIndex = 0; MeshSurfaceIndex < SurfaceCount; ++MeshSurfaceIndex)
				{
					uint32 SurfaceId = Component.Mesh ? Component.Mesh->GetSurfaceId(MeshSurfaceIndex) : OperationData->LastUpdateData.SurfaceIds[PrevUpdateComponent.FirstSurface + MeshSurfaceIndex];
					int32 InstanceSurfaceIndex = Instance->FindSurfaceById(MutableLODIndex, ComponentIndex, SurfaceId);
					check(Component.bReuseMesh || Component.Mesh->GetVertexCount() == 0 || InstanceSurfaceIndex >= 0);

					int32 BaseSurfaceIndex = InstanceSurfaceIndex;
					int32 BaseLODIndex = MutableLODIndex;

					if (InstanceSurfaceIndex >= 0)
					{
						OperationData->InstanceUpdateData.Surfaces.Push({});
						FInstanceUpdateData::FSurface& Surface = OperationData->InstanceUpdateData.Surfaces.Last();
						++Component.SurfaceCount;

						// Now Surface.MaterialIndex is decoded from a parameter at the end of this if()
						Surface.SurfaceId = SurfaceId;

						const int32 SharedSurfaceId = Instance->GetSharedSurfaceId(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex);
						const int32 SharedSurfaceIndex = SurfacesSharedId.Find(SharedSurfaceId);

						SurfacesSharedId.Add(SharedSurfaceId);

						if (SharedSurfaceId != INDEX_NONE)
						{
							if (SharedSurfaceIndex >= 0)
							{
								Surface = OperationData->InstanceUpdateData.Surfaces[SharedSurfaceIndex];
								continue;
							}

							// Find the first LOD where this surface can be found
							Instance->FindBaseSurfaceBySharedId(ComponentIndex, SharedSurfaceId, BaseSurfaceIndex, BaseLODIndex);

							Surface.SurfaceId = Instance->GetSurfaceId(BaseLODIndex, ComponentIndex, BaseSurfaceIndex);
						}

						// Images
						Surface.FirstImage = OperationData->InstanceUpdateData.Images.Num();
						Surface.ImageCount = Instance->GetImageCount(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 ImageIndex = 0; ImageIndex < Surface.ImageCount; ++ImageIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetImageId);

							OperationData->InstanceUpdateData.Images.Push({});
							FInstanceUpdateData::FImage& Image = OperationData->InstanceUpdateData.Images.Last();
							Image.Name = Instance->GetImageName(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, ImageIndex);
							Image.ImageID = Instance->GetImageId(BaseLODIndex, ComponentIndex, BaseSurfaceIndex, ImageIndex);
							Image.FullImageSizeX = 0;
							Image.FullImageSizeY = 0;
							Image.BaseLOD = BaseLODIndex;
						}

						// Vectors
						Surface.FirstVector = OperationData->InstanceUpdateData.Vectors.Num();
						Surface.VectorCount = Instance->GetVectorCount(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 VectorIndex = 0; VectorIndex < Surface.VectorCount; ++VectorIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetVector);
							OperationData->InstanceUpdateData.Vectors.Push({});
							FInstanceUpdateData::FVector& Vector = OperationData->InstanceUpdateData.Vectors.Last();
							Vector.Name = Instance->GetVectorName(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, VectorIndex);
							Vector.Vector = Instance->GetVector(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, VectorIndex);
						}

						// Scalars
						Surface.FirstScalar = OperationData->InstanceUpdateData.Scalars.Num();
						Surface.ScalarCount = Instance->GetScalarCount(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 ScalarIndex = 0; ScalarIndex < Surface.ScalarCount; ++ScalarIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetScalar)

							FString ScalarName = Instance->GetScalarName(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, ScalarIndex);
							float ScalarValue = Instance->GetScalar(BaseLODIndex, ComponentIndex, InstanceSurfaceIndex, ScalarIndex);
							
							FString EncodingMaterialIdString = "__MutableMaterialId";
							
							// Decoding Material Switch from Mutable parameter name
							if (ScalarName.Equals(EncodingMaterialIdString))
							{
								Surface.MaterialIndex = (uint32)ScalarValue;
							
								// This parameter is not needed in the final material instance
								Surface.ScalarCount -= 1;
							}
							else
							{
								OperationData->InstanceUpdateData.Scalars.Push({ ScalarName, ScalarValue });
							}
						}
					}
				}
			}
		}

		// Copy ExtensionData Object node input from the Instance to the InstanceUpdateData
		for (int32 ExtensionDataIndex = 0; ExtensionDataIndex < Instance->GetExtensionDataCount(); ExtensionDataIndex++)
		{
			mu::ExtensionDataPtrConst ExtensionData;
			const char* NameAnsi = nullptr;
			Instance->GetExtensionData(ExtensionDataIndex, ExtensionData, NameAnsi);

			check(ExtensionData);
			check(NameAnsi);

			FInstanceUpdateData::FNamedExtensionData& NewEntry = OperationData->InstanceUpdateData.ExtendedInputPins.AddDefaulted_GetRef();
			NewEntry.Data = ExtensionData;
			NewEntry.Name = NameAnsi;
			check(NewEntry.Name != NAME_None);
		}
	}


	// This runs in the mutable thread.
	void Subtask_Mutable_GetImages(const TSharedPtr<FMutableOperationData>& OperationData, TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model, const mu::Parameters* MutableParameters, int32 State)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_GetImages)

		check(OperationData);
		check(MutableParameters);

		check(UCustomizableObjectSystem::GetInstance() != nullptr);
		FCustomizableObjectSystemPrivate* CustomizableObjectSystemPrivateData = UCustomizableObjectSystem::GetInstance()->GetPrivate();
		check(CustomizableObjectSystemPrivateData != nullptr);
		mu::System* System = CustomizableObjectSystemPrivateData->MutableSystem.get();
		check(System != nullptr);

		// Generate all the required resources, that are not cached
		TArray<mu::FResourceID> ImagesInThisInstance;
		for (FInstanceUpdateData::FImage& Image : OperationData->InstanceUpdateData.Images)
		{
			MUTABLE_CPUPROFILER_SCOPE(GetImage);

			mu::FImageDesc ImageDesc;

			// This should only be done when using progressive images, since GetImageDesc does some actual processing.
			{
				System->GetImageDesc(OperationData->InstanceID, Image.ImageID, ImageDesc);

				uint16 MaxTextureSizeToGenerate = uint16(CustomizableObjectSystemPrivateData->MaxTextureSizeToGenerate);
				uint16 MaxSize = FMath::Max(ImageDesc.m_size[0], ImageDesc.m_size[1]);
				uint16 Reduction = 1;

				if (MaxTextureSizeToGenerate > 0 && MaxSize > MaxTextureSizeToGenerate)
				{
					Reduction = MaxSize / MaxTextureSizeToGenerate;
				}

				Image.FullImageSizeX = ImageDesc.m_size[0] / Reduction;
				Image.FullImageSizeY = ImageDesc.m_size[1] / Reduction;
			}

			const bool bCached = ImagesInThisInstance.Contains(Image.ImageID) || // See if it is cached from this same instance (can happen with LODs)
				(CVarReuseImagesBetweenInstances.GetValueOnAnyThread() && CustomizableObjectSystemPrivateData->ProtectedObjectCachedImages.Contains(Image.ImageID)); // See if it is cached from another instance

			if (bCached)
			{
				UE_LOG(LogMutable, VeryVerbose, TEXT("Texture resource with id [%d] is cached."), Image.ImageID);
				INC_DWORD_STAT(STAT_MutableNumCachedTextures);
			}
			else
			{
				int32 MaxSize = FMath::Max(Image.FullImageSizeX, Image.FullImageSizeY);
				int32 FullLODCount = FMath::CeilLogTwo(MaxSize) + 1;
				int32 MinMipsInImage = FMath::Min(FullLODCount, UTexture::GetStaticMinTextureResidentMipCount());
				int32 MaxMipsToSkip = FullLODCount - MinMipsInImage;
				int32 MipsToSkip = FMath::Min(MaxMipsToSkip, OperationData->MipsToSkip);

				if (!FMath::IsPowerOfTwo(Image.FullImageSizeX) || !FMath::IsPowerOfTwo(Image.FullImageSizeY))
				{
					// It doesn't make sense to skip mips as non-power-of-two size textures cannot be streamed anyway
					MipsToSkip = 0;
				}

				const int32 MipSizeX = FMath::Max(Image.FullImageSizeX >> MipsToSkip, 1);
				const int32 MipSizeY = FMath::Max(Image.FullImageSizeY >> MipsToSkip, 1);
				if (MipsToSkip > 0 && CustomizableObjectSystemPrivateData->EnableSkipGenerateResidentMips != 0 && OperationData->LowPriorityTextures.Find(Image.Name) != INDEX_NONE)
				{
					Image.Image = new mu::Image(MipSizeX, MipSizeY, FullLODCount - MipsToSkip, ImageDesc.m_format, mu::EInitializationType::Black);
				}
				else
				{
					Image.Image = System->GetImage(OperationData->InstanceID, Image.ImageID, MipsToSkip, Image.BaseLOD);
				}

				check(Image.Image);

				// If the image is a reference to an engine texture, we are done.
				if (Image.Image->IsReference())
				{
					continue;
				}


				// We should have genrated exactly this size.
				bool bSizeMissmatch = Image.Image->GetSizeX() != MipSizeX || Image.Image->GetSizeY() != MipSizeY;
				if (bSizeMissmatch)
				{
					// Generate a correctly-sized but empty image instead, to avoid crashes.
					UE_LOG(LogMutable, Warning, TEXT("Mutable generated a wrongly-sized image %d."), Image.ImageID);
					Image.Image = new mu::Image(MipSizeX, MipSizeY, FullLODCount - MipsToSkip, Image.Image->GetFormat(), mu::EInitializationType::Black);
				}

				// We need one mip or the complete chain. Otherwise there was a bug.
				int32 FullMipCount = Image.Image->GetMipmapCount(Image.Image->GetSizeX(), Image.Image->GetSizeY());
				int32 RealMipCount = Image.Image->GetLODCount();

				bool bForceMipchain = 
					// Did we fail to generate the entire mipchain (if we have mips at all)?
					(RealMipCount != 1) && (RealMipCount != FullMipCount);

				if (bForceMipchain)
				{
					MUTABLE_CPUPROFILER_SCOPE(GetImage_MipFix);

					UE_LOG(LogMutable, Warning, TEXT("Mutable generated an incomplete mip chain for image %d."), Image.ImageID);

					// Force the right number of mips. The missing data will be black.
					mu::Ptr<mu::Image> NewImage = new mu::Image(Image.Image->GetSizeX(), Image.Image->GetSizeY(), FullMipCount, Image.Image->GetFormat(), mu::EInitializationType::Black);
					check(NewImage);
					if (NewImage->GetDataSize() >= Image.Image->GetDataSize())
					{
						FMemory::Memcpy(NewImage->GetData(), Image.Image->GetData(), Image.Image->GetDataSize());
					}
					Image.Image = NewImage;
				}

				ImagesInThisInstance.Add(Image.ImageID);
			}
		}
	}
	

	// This runs in a worker thread
	void Subtask_Mutable_PrepareTextures(const TSharedPtr<FMutableOperationData>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_PrepareTextures)

		check(OperationData);
		for (const FInstanceUpdateData::FSurface& Surface : OperationData->InstanceUpdateData.Surfaces)
		{
			for (int32 ImageIndex = 0; ImageIndex<Surface.ImageCount; ++ImageIndex)
			{
				const FInstanceUpdateData::FImage& Image = OperationData->InstanceUpdateData.Images[Surface.FirstImage+ImageIndex];

				FString KeyName = Image.Name;
				mu::ImagePtrConst MutableImage = Image.Image;

				// If the image is null, it must be in the cache (or repeated in this instance), and we don't need to do anything here.
				if (MutableImage)
				{
					// Image refences are just references to texture assets and require no work at all
					if (!MutableImage->IsReference())
					{
						FTexturePlatformData* PlatformData = UCustomizableInstancePrivateData::MutableCreateImagePlatformData(MutableImage, -1, Image.FullImageSizeX, Image.FullImageSizeY);
						OperationData->ImageToPlatformDataMap.Add(Image.ImageID, PlatformData);
						OperationData->PendingTextureCoverageQueries.Add({ KeyName, Surface.MaterialIndex, PlatformData });
					}
				}
			}
		}
	}
	

	// This runs in a worker thread
	void Subtask_Mutable_PrepareSkeletonData(const TSharedPtr<FMutableOperationData>& OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_PrepareSkeletonData)

		check(OperationData);
		const int32 LODCount = OperationData->InstanceUpdateData.LODs.Num();
		const FInstanceUpdateData::FLOD& MinLOD = OperationData->InstanceUpdateData.LODs[OperationData->CurrentMinLOD];
		const int32 ComponentCount = MinLOD.ComponentCount;

		// Add SkeletonData for each component
		OperationData->InstanceUpdateData.Skeletons.AddDefaulted(ComponentCount);

		for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
		{
			FInstanceUpdateData::FComponent& MinLODComponent = OperationData->InstanceUpdateData.Components[MinLOD.FirstComponent + ComponentIndex];

			// Set the ComponentIndex
			OperationData->InstanceUpdateData.Skeletons[ComponentIndex].ComponentIndex = MinLODComponent.Id;

			// Fill the data used to generate the RefSkeletalMesh
			TArray<uint16>& SkeletonIds = OperationData->InstanceUpdateData.Skeletons[ComponentIndex].SkeletonIds;
			TArray<uint16>& BoneIds = OperationData->InstanceUpdateData.Skeletons[ComponentIndex].BoneIds;
			TArray<FMatrix44f>& BoneMatricesWithScale = OperationData->InstanceUpdateData.Skeletons[ComponentIndex].BoneMatricesWithScale;

			// Use first valid LOD bone count as a potential total number of bones, used for pre-allocating data arrays
			if (MinLODComponent.Mesh && MinLODComponent.Mesh->GetSkeleton())
			{
				const int32 TotalPossibleBones = MinLODComponent.Mesh->GetSkeleton()->GetBoneCount();

				// Out Data
				BoneIds.Reserve(TotalPossibleBones);
				BoneMatricesWithScale.Reserve(TotalPossibleBones);
			}

			for (int32 LODIndex = OperationData->CurrentMinLOD; LODIndex <= OperationData->CurrentMaxLOD && LODIndex < LODCount; ++LODIndex)
			{
				MUTABLE_CPUPROFILER_SCOPE(PrepareSkeletonData_LODs);

				const FInstanceUpdateData::FLOD& CurrentLOD = OperationData->InstanceUpdateData.LODs[LODIndex];
				FInstanceUpdateData::FComponent& CurrentLODComponent = OperationData->InstanceUpdateData.Components[CurrentLOD.FirstComponent + ComponentIndex];
				mu::MeshPtrConst Mesh = CurrentLODComponent.Mesh;

				if (!Mesh)
				{
					continue;
				}

				// Add SkeletonIds 
				const int32 SkeletonIDsCount = Mesh->GetSkeletonIDsCount();
				for (int32 SkeletonIndex = 0; SkeletonIndex < SkeletonIDsCount; ++SkeletonIndex)
				{
					SkeletonIds.AddUnique(Mesh->GetSkeletonID(SkeletonIndex));
				}

				// Append BoneMap to the array of BoneMaps
				const TArray<uint16>& BoneMap = Mesh->GetBoneMap();
				CurrentLODComponent.FirstBoneMap = OperationData->InstanceUpdateData.BoneMaps.Num();
				CurrentLODComponent.BoneMapCount = BoneMap.Num();
				OperationData->InstanceUpdateData.BoneMaps.Append(BoneMap);

				// Add active bone indices and poses
				const int32 MaxBoneIndex = Mesh->GetBonePoseCount();
				CurrentLODComponent.ActiveBones.Reserve(MaxBoneIndex);
				for (int32 BonePoseIndex = 0; BonePoseIndex < MaxBoneIndex; ++BonePoseIndex)
				{
					const uint16 BoneId = Mesh->GetBonePoseBoneId(BonePoseIndex);

					CurrentLODComponent.ActiveBones.Add(BoneId);

					if(BoneIds.Find(BoneId) == INDEX_NONE)
					{
						BoneIds.Add(BoneId);

						FTransform3f Transform;
						Mesh->GetBoneTransform(BonePoseIndex, Transform);
						BoneMatricesWithScale.Emplace(Transform.Inverse().ToMatrixWithScale());
					}
				}
			}
		}
	}


	// This runs in a worker thread.
	void Task_Mutable_Update_GetMesh(TSharedPtr<FMutableOperationData> OperationData, TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model, mu::ParametersPtrConst Parameters, bool bBuildParameterRelevancy, int32 State)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_Update_GetMesh)

#if WITH_EDITOR
		uint32 StartCycles = FPlatformTime::Cycles();
#endif

		check(OperationData.IsValid());

		Subtask_Mutable_BeginUpdate_GetMesh(OperationData, Model, Parameters.get(), State);

		// TODO: Not strictly mutable: move to another worker thread task to free mutable access?
		Subtask_Mutable_PrepareSkeletonData(OperationData);

		if (bBuildParameterRelevancy)
		{
			Subtask_Mutable_UpdateParameterRelevancy(OperationData, Parameters.get());
		}
		else
		{
			OperationData->RelevantParametersInProgress.Reset();
		}

#if WITH_EDITOR
		uint32 EndCycles = FPlatformTime::Cycles();
		OperationData->MutableRuntimeCycles = EndCycles - StartCycles;
#endif

	}


	void Task_Mutable_Update_GetImages(TSharedPtr<FMutableOperationData> OperationData, TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model, mu::ParametersPtrConst Parameters, int32 State)
	{
		// This runs in a worker thread.
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_GetImages)

#if WITH_EDITOR
		uint32 StartCycles = FPlatformTime::Cycles();
#endif

		check(OperationData.IsValid());

		Subtask_Mutable_GetImages(OperationData, Model, Parameters.get(), State);

		// TODO: Not strictly mutable: move to another worker thread task to free mutable access?
		Subtask_Mutable_PrepareTextures(OperationData);

#if WITH_EDITOR
		uint32 EndCycles = FPlatformTime::Cycles();
		OperationData->MutableRuntimeCycles += EndCycles - StartCycles;
#endif
	}


	// This runs in a worker thread.
	void Task_Mutable_ReleaseInstance(TSharedPtr<FMutableOperationData> OperationData, mu::SystemPtr MutableSystem)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_ReleaseInstance)

		check(OperationData.IsValid());

		if (OperationData->InstanceID > 0)
		{
			MUTABLE_CPUPROFILER_SCOPE(EndUpdate);
			check(MutableSystem);
			MutableSystem->EndUpdate(OperationData->InstanceID);
			OperationData->InstanceUpdateData.Clear();

			if (!OperationData->bLiveUpdateMode)
			{
				MutableSystem->ReleaseInstance(OperationData->InstanceID);
				OperationData->InstanceID = 0;
			}
		}

		if (CVarClearWorkingMemoryOnUpdateEnd.GetValueOnAnyThread())
		{
			MutableSystem->ClearWorkingMemory();
		}

		UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableTaskGraph.AllowLaunchingMutableTaskLowPriority(true, true);
	}


	void Task_Mutable_ReleaseInstanceID(TSharedPtr<FMutableOperationData> OperationData, mu::SystemPtr MutableSystem)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_ReleaseInstanceID)

		// This runs in a worker thread.
		check(OperationData.IsValid());

		if (OperationData->InstanceID > 0)
		{
			MutableSystem->ReleaseInstance(OperationData->InstanceID);
			OperationData->InstanceID = 0;
		}

		if (CVarClearWorkingMemoryOnUpdateEnd.GetValueOnAnyThread())
		{
			MutableSystem->ClearWorkingMemory();
		}
	}


	void Task_Game_ReleasePlatformData(TSharedPtr<FMutableReleasePlatformOperationData> OperationData)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_ReleasePlatformData)

		check(OperationData.IsValid());
		TMap<uint32, FTexturePlatformData*>& ImageToPlatformDataMap = OperationData->ImageToPlatformDataMap;
		for (TPair<uint32, FTexturePlatformData*> Pair : ImageToPlatformDataMap)
		{
			delete Pair.Value; // If this is not null then it must mean it hasn't been used, otherwise they would have taken ownership and nulled it
		}
		ImageToPlatformDataMap.Reset();
	}

	
	void Task_Game_Callbacks(TSharedPtr<FMutableOperationData> OperationData, const TWeakObjectPtr<UCustomizableObjectInstance>& CustomizableObjectInstancePtr)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_Callbacks)

		check(IsInGameThread());

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
		if (!System || !System->IsValidLowLevel() || System->HasAnyFlags(RF_BeginDestroyed))
		{
			FinishUpdateGlobal(CustomizableObjectInstancePtr.Get(), EUpdateResult::Error, &OperationData->UpdateCallback);
			return;
		}

		UCustomizableObjectInstance* CustomizableObjectInstance = CustomizableObjectInstancePtr.Get();

		// TODO: Review checks.
		if (!CustomizableObjectInstance || !CustomizableObjectInstance->IsValidLowLevel() )
		{
			System->ClearCurrentMutableOperation();
			FinishUpdateGlobal(CustomizableObjectInstance, EUpdateResult::Error, &OperationData->UpdateCallback);
			return;
		}

		FCustomizableObjectSystemPrivate * CustomizableObjectSystemPrivateData = System->GetPrivate();
		check(CustomizableObjectSystemPrivateData != nullptr);

		// Actual work
		// TODO MTBL-391: Review This hotfix
		UpdateSkeletalMesh(*CustomizableObjectInstance, CustomizableObjectSystemPrivateData->CurrentMutableOperation->InstanceDescriptorRuntimeHash, OperationData->UpdateResult, &OperationData->UpdateCallback);

		// All work is done, release unused textures.
		if (CustomizableObjectSystemPrivateData->bReleaseTexturesImmediately)
		{
			FMutableResourceCache& Cache = CustomizableObjectSystemPrivateData->GetObjectCache(CustomizableObjectInstance->GetCustomizableObject());

			UCustomizableInstancePrivateData* CustomizableObjectInstancePrivateData = CustomizableObjectInstance->GetPrivate();
			for (FGeneratedTexture& GeneratedTexture : CustomizableObjectInstancePrivateData->TexturesToRelease)
			{
				UCustomizableInstancePrivateData::ReleaseMutableTexture(GeneratedTexture.Key, Cast<UTexture2D>(GeneratedTexture.Texture), Cache);
			}

			CustomizableObjectInstancePrivateData->TexturesToRelease.Empty();
		}

		// TODO: T2927
		if (LogBenchmarkUtil::isLoggingActive())
		{
			double deltaSeconds = FPlatformTime::Seconds() - CustomizableObjectSystemPrivateData->CurrentMutableOperation->StartUpdateTime;
			int32 deltaMs = int32(deltaSeconds * 1000);

			LogBenchmarkUtil::updateStat("customizable_instance_build_time", deltaMs);
			CustomizableObjectSystemPrivateData->TotalBuildMs += deltaMs;
			CustomizableObjectSystemPrivateData->TotalBuiltInstances++;
			SET_DWORD_STAT(STAT_MutableInstanceBuildTime, deltaMs);
			SET_DWORD_STAT(STAT_MutableInstanceBuildTimeAvrg, CustomizableObjectSystemPrivateData->TotalBuildMs / CustomizableObjectSystemPrivateData->TotalBuiltInstances);
		}

		// End Update
		System->ClearCurrentMutableOperation();
	}


	void Task_Game_ConvertResources(TSharedPtr<FMutableOperationData> OperationData, const TWeakObjectPtr<UCustomizableObjectInstance>& CustomizableObjectInstancePtr)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_ConvertResources)

		check(IsInGameThread());

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
		if (!System || !System->IsValidLowLevel() || System->HasAnyFlags(RF_BeginDestroyed))
		{
			FinishUpdateGlobal(CustomizableObjectInstancePtr.Get(), EUpdateResult::Error, &OperationData->UpdateCallback);
			return;
		}

		check(OperationData.IsValid());

		UCustomizableObjectInstance* CustomizableObjectInstance = CustomizableObjectInstancePtr.Get();

		// Actual work
		// TODO: Review checks.
		const bool bInstanceInvalid = !CustomizableObjectInstance || !CustomizableObjectInstance->IsValidLowLevel();
		if (!bInstanceInvalid)
		{
			UCustomizableInstancePrivateData* CustomizableInstancePrivateData = CustomizableObjectInstance->GetPrivate();

			// Process the pending texture coverage queries
			{
				MUTABLE_CPUPROFILER_SCOPE(GameTextureQueries);
				for (const FPendingTextureCoverageQuery& Query : OperationData->PendingTextureCoverageQueries)
				{
					UMaterialInterface* Material = nullptr;
					const uint32* InstanceIndex = CustomizableInstancePrivateData->ObjectToInstanceIndexMap.Find(Query.MaterialIndex);
					if (InstanceIndex && CustomizableInstancePrivateData->ReferencedMaterials.IsValidIndex(*InstanceIndex))
					{
						Material = CustomizableInstancePrivateData->ReferencedMaterials[*InstanceIndex];
					}

					UCustomizableInstancePrivateData::ProcessTextureCoverageQueries(OperationData, CustomizableObjectInstance->GetCustomizableObject(), Query.KeyName, Query.PlatformData, Material);
				}
				OperationData->PendingTextureCoverageQueries.Empty();
			}

			// Process texture coverage queries because it's safe to do now that the Mutable thread is stopped
			{
				if (OperationData->TextureCoverageQueries_MutableThreadResults.Num() > 0)
				{
					for (auto& Result : OperationData->TextureCoverageQueries_MutableThreadResults)
					{
						FTextureCoverageQueryData* FinalResultData = CustomizableInstancePrivateData->TextureCoverageQueries.Find(Result.Key);
						*FinalResultData = Result.Value;
					}

					OperationData->TextureCoverageQueries_MutableThreadResults.Empty();
				}

#if WITH_EDITOR
				CustomizableObjectInstance->LastUpdateMutableRuntimeCycles = OperationData->MutableRuntimeCycles;
#endif

				// Convert Step
				//-------------------------------------------------------------

				// \TODO: Bring that code here instead of keeping it in the UCustomizableObjectInstance
				if (CustomizableInstancePrivateData->UpdateSkeletalMesh_PostBeginUpdate0(CustomizableObjectInstance, OperationData))
				{
					// This used to be CustomizableObjectInstance::UpdateSkeletalMesh_PostBeginUpdate1
					{
						MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_PostBeginUpdate1);

						// \TODO: Bring here
						CustomizableInstancePrivateData->BuildMaterials(OperationData, CustomizableObjectInstance);
					}

					// This used to be CustomizableObjectInstance::UpdateSkeletalMesh_PostBeginUpdate2
					{
						MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_PostBeginUpdate2);

						for (int32 Component = 0; Component < CustomizableObjectInstance->SkeletalMeshes.Num(); ++Component)
						{
							if (CustomizableObjectInstance->SkeletalMeshes[Component] && CustomizableObjectInstance->SkeletalMeshes[Component]->GetLODInfoArray().Num())
							{
								MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_PostEditChangeProperty);

								CustomizableInstancePrivateData->PostEditChangePropertyWithoutEditor(CustomizableObjectInstance->SkeletalMeshes[Component]);
							}
						}
					}
				}
			} // END - Process texture coverage queries
		} // if (!bInstanceValid)

		FCustomizableObjectSystemPrivate* CustomizableObjectSystemPrivateData = System->GetPrivate();
		check(CustomizableObjectSystemPrivateData != nullptr);

		// Next Task: Release Mutable. We need this regardless if we cancel or not
		//-------------------------------------------------------------		
		{
			mu::SystemPtr MutableSystem = CustomizableObjectSystemPrivateData->MutableSystem;
			CustomizableObjectSystemPrivateData->MutableTaskGraph.AddMutableThreadTask(
				TEXT("Task_Mutable_ReleaseInstance"),
				[OperationData, MutableSystem]() {Task_Mutable_ReleaseInstance(OperationData, MutableSystem); });
		}


		// Next Task: Release Platform Data
		//-------------------------------------------------------------
		if (!bInstanceInvalid)
		{
			TSharedPtr<FMutableReleasePlatformOperationData> ReleaseOperationData = MakeShared<FMutableReleasePlatformOperationData>();
			check(ReleaseOperationData);
			ReleaseOperationData->ImageToPlatformDataMap = MoveTemp(OperationData->ImageToPlatformDataMap);
			CustomizableObjectSystemPrivateData->MutableTaskGraph.AddAnyThreadTask(
				TEXT("Mutable_ReleasePlatformData"),
				[ReleaseOperationData]()
				{
					Task_Game_ReleasePlatformData(ReleaseOperationData);
				}
			);


			// Unlock step
			//-------------------------------------------------------------
			if (CustomizableObjectInstance->GetCustomizableObject())
			{
				// Unlock the resource cache for the object used by this instance to avoid
				// the destruction of resources that we may want to reuse.
				System->ClearResourceCacheProtected();
			}

			// Next Task: Callbacks
			//-------------------------------------------------------------
			CustomizableObjectSystemPrivateData->AddGameThreadTask(
				{
				FMutableTaskDelegate::CreateLambda(
					[OperationData,CustomizableObjectInstancePtr]()
					{
						Task_Game_Callbacks(OperationData,CustomizableObjectInstancePtr);
					}),
					{}
				});
		}
		else
		{
			FinishUpdateGlobal(CustomizableObjectInstance, EUpdateResult::Error, &OperationData->UpdateCallback);
		}
	}


	/** "Lock Cached Resources" */
	void Task_Game_LockCache(TSharedPtr<FMutableOperationData> OperationData, const TWeakObjectPtr<UCustomizableObjectInstance>& CustomizableObjectInstancePtr, mu::Ptr<const mu::Parameters> Parameters, bool bBuildParameterRelevancy)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_LockCache)

		check(IsInGameThread());
		check(OperationData);

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
		if (!System)
		{
			return;
		}

		UCustomizableObjectInstance* ObjectInstance = CustomizableObjectInstancePtr.Get();

		if (!ObjectInstance)
		{
			System->ClearCurrentMutableOperation();
			FinishUpdateGlobal(ObjectInstance, EUpdateResult::Error, &OperationData->UpdateCallback);
			return;
		}

		UCustomizableInstancePrivateData* ObjectInstancePrivateData = ObjectInstance->GetPrivate();
		check(ObjectInstancePrivateData != nullptr);

		if (OperationData->bLiveUpdateMode)
		{
			check(OperationData->InstanceID != 0);

			if (ObjectInstancePrivateData->LiveUpdateModeInstanceID == 0)
			{
				// From now this instance will reuse this InstanceID until it gets out of LiveUpdateMode
				ObjectInstancePrivateData->LiveUpdateModeInstanceID = OperationData->InstanceID;
			}
		}

		const UCustomizableObject* CustomizableObject = ObjectInstance->GetCustomizableObject(); 
		if (!CustomizableObject)
		{
			System->ClearCurrentMutableOperation();
			FinishUpdateGlobal(CustomizableObjectInstancePtr.Get(), EUpdateResult::Error, &OperationData->UpdateCallback);
			return;
		}

		check(OperationData.IsValid());
		
		if (bBuildParameterRelevancy)
		{
			// Relevancy
			ObjectInstancePrivateData->RelevantParameters = OperationData->RelevantParametersInProgress;
		}

		
		// Selectively lock the resource cache for the object used by this instance to avoid the destruction of resources that we may want to reuse.
		// When protecting textures there mustn't be any left from a previous update
		check(System->ProtectedCachedTextures.Num() == 0);

		FCustomizableObjectSystemPrivate* SystemPrivateData = System->GetPrivate();
		check(SystemPrivateData != nullptr);

		// TODO: If this is the first code that runs after the CO program has finished AND if it's
		// guaranteed that the next CO program hasn't started yet, we need to call ClearActiveObject
		// and CancelPendingLoads on SystemPrivateData->ExtensionDataStreamer.
		//
		// ExtensionDataStreamer->AreAnyLoadsPending should return false if the program succeeded.
		//
		// If the program aborted, AreAnyLoadsPending may return true, as the program doesn't cancel
		// its own loads on exit (maybe it should?)

		FMutableResourceCache& Cache = SystemPrivateData->GetObjectCache(CustomizableObject);

		System->ProtectedCachedTextures.Reset(Cache.Images.Num());
		SystemPrivateData->ProtectedObjectCachedImages.Reset(Cache.Images.Num());

		for (const FInstanceUpdateData::FImage& Image : OperationData->InstanceUpdateData.Images)
		{
			FMutableImageCacheKey Key(Image.ImageID, OperationData->MipsToSkip);
			TWeakObjectPtr<UTexture2D>* TexturePtr = Cache.Images.Find(Key);

			if (TexturePtr && TexturePtr->Get() && SystemPrivateData->TextureHasReferences(Key))
			{
				System->ProtectedCachedTextures.Add(TexturePtr->Get());
				SystemPrivateData->ProtectedObjectCachedImages.Add(Image.ImageID);
			}
		}

		// Any external texture that may be needed for this update will be requested from Mutable Core's GetImage
		// which will safely access the GlobalExternalImages map, and then just get the cached image or issue a disk read

		// Copy data generated in the mutable thread over to the instance
		ObjectInstancePrivateData->PrepareForUpdate(OperationData);

		// Task: Mutable GetImages
		//-------------------------------------------------------------
#ifdef MUTABLE_USE_NEW_TASKGRAPH
		UE::Tasks::FTask Mutable_GetImagesTask;
#else
		FGraphEventRef Mutable_GetImagesTask;
#endif
		{
			// Task inputs
			TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = CustomizableObject->GetPrivate()->GetModel();
			int32 State = ObjectInstance->GetState();

			Mutable_GetImagesTask = SystemPrivateData->MutableTaskGraph.AddMutableThreadTask(
					TEXT("Task_Mutable_GetImages"),
					[OperationData, Parameters, Model, State]()
					{
						impl::Task_Mutable_Update_GetImages(OperationData, Model, Parameters, State);
					});
		}


		// Next Task: Load Unreal Assets
		//-------------------------------------------------------------
		FGraphEventRef Game_LoadUnrealAssets = ObjectInstancePrivateData->LoadAdditionalAssetsAsync(OperationData, ObjectInstance, UCustomizableObjectSystem::GetInstance()->GetStreamableManager());
		if (Game_LoadUnrealAssets)
		{
			Game_LoadUnrealAssets->SetDebugName(TEXT("LoadAdditionalAssetsAsync"));
		}

		// Next-next Task: Convert Resources
		//-------------------------------------------------------------
		SystemPrivateData->AddGameThreadTask(
			{
			FMutableTaskDelegate::CreateLambda(
				[OperationData,CustomizableObjectInstancePtr]()
				{
					Task_Game_ConvertResources(OperationData,CustomizableObjectInstancePtr);
				}),
#ifdef MUTABLE_USE_NEW_TASKGRAPH
				{},
#endif
				Game_LoadUnrealAssets,
				Mutable_GetImagesTask
			});
	}


	/** Enqueue the release ID operation in the Mutable queue */
	void Task_Game_ReleaseInstanceID(const mu::Instance::ID IDToRelease)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_ReleaseInstanceID)

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
		check(System != nullptr);

		FCustomizableObjectSystemPrivate* SystemPrivateData = System->GetPrivate();
		check(SystemPrivateData != nullptr);

		mu::SystemPtr MutableSystem = SystemPrivateData->MutableSystem;

		// Task: Release Instance ID
		//-------------------------------------------------------------
		TSharedPtr<FMutableOperationData> CurrentOperationData = MakeShared<FMutableOperationData>();
		check(CurrentOperationData);
		CurrentOperationData->InstanceID = IDToRelease;

#ifdef MUTABLE_USE_NEW_TASKGRAPH
		UE::Tasks::FTask Mutable_GetMeshTask;
#else
		FGraphEventRef Mutable_GetMeshTask;
#endif
		{
			// Task inputs
			TSharedPtr<FMutableOperation> CurrentMutableOperation = SystemPrivateData->CurrentMutableOperation;
			check(CurrentMutableOperation);

			Mutable_GetMeshTask = SystemPrivateData->MutableTaskGraph.AddMutableThreadTask(
				TEXT("Task_Mutable_ReleaseInstanceID"),
				[CurrentOperationData, MutableSystem]()
				{
					impl::Task_Mutable_ReleaseInstanceID(CurrentOperationData, MutableSystem);
				});
		}
	}


	/** "Start Update" */
	void Task_Game_StartUpdate(TSharedPtr<FMutableOperation> Operation)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Game_StartUpdate)

		UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableTaskGraph.AllowLaunchingMutableTaskLowPriority(false, false);
		
		check(Operation);

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
		check(System != nullptr);

		if (!Operation->CustomizableObjectInstance.IsValid() || !Operation->CustomizableObjectInstance->IsValidLowLevel()) // Only start if it hasn't been already destroyed (i.e. GC after finish PIE)
		{
			System->ClearCurrentMutableOperation();
			FinishUpdateGlobal(nullptr, EUpdateResult::Error, &Operation->UpdateCallback);
			return;
		}

		TObjectPtr<UCustomizableObjectInstance> CandidateInstance = Operation->CustomizableObjectInstance.Get();
		
		UCustomizableInstancePrivateData* CandidateInstancePrivateData = CandidateInstance->GetPrivate();
		if (!CandidateInstancePrivateData)
		{
			System->ClearCurrentMutableOperation();
			FinishUpdateGlobal(nullptr, EUpdateResult::Error, &Operation->UpdateCallback);
			return;
		}
		
		if (CandidateInstancePrivateData->HasCOInstanceFlags(PendingLODsUpdate))
		{
			CandidateInstancePrivateData->ClearCOInstanceFlags(PendingLODsUpdate);
			// TODO: Is anything needed for this now?
			//Operation->CustomizableObjectInstance->ReleaseMutableInstanceId(); // To make mutable regenerate the LODs even if the instance parameters have not changed
		}

		bool bCancel = false;

		// If the object is locked (for instance, compiling) we skip any instance update.
		TObjectPtr<UCustomizableObject> CustomizableObject = CandidateInstance->GetCustomizableObject();
		if (!CustomizableObject)
		{
			bCancel = true;
		}
		else
		{
			if (CustomizableObject->GetPrivate()->bLocked)
			{
				bCancel = true;
			}
		}

		// Only update resources if the instance is in range (it could have got far from the player since the task was queued)
		check(System->CurrentInstanceLODManagement != nullptr);
		if (System->CurrentInstanceLODManagement->IsOnlyUpdateCloseCustomizableObjectsEnabled()
			&& CandidateInstancePrivateData
			&& CandidateInstancePrivateData->LastMinSquareDistFromComponentToPlayer > FMath::Square(System->CurrentInstanceLODManagement->GetOnlyUpdateCloseCustomizableObjectsDist())
			&& CandidateInstancePrivateData->LastMinSquareDistFromComponentToPlayer != FLT_MAX // This means it is the first frame so it has to be updated
		   )
		{
			bCancel = true;
		}

		// Skip update, the requested update is equal to the running update.
		if (Operation->InstanceDescriptorRuntimeHash.IsSubset(CandidateInstance->GetDescriptorRuntimeHash()))
		{
			CandidateInstance->SkeletalMeshStatus = ESkeletalMeshState::Correct;
			UpdateSkeletalMesh(*CandidateInstance, CandidateInstance->GetDescriptorRuntimeHash(), EUpdateResult::Success, &Operation->UpdateCallback);
			bCancel = true;
		}

		mu::Ptr<const mu::Parameters> Parameters = Operation->GetParameters();
		if (!Parameters)
		{
			bCancel = true;
		}

		if (bCancel)
		{
			CandidateInstancePrivateData->ClearCOInstanceFlags(Updating);

			System->ClearCurrentMutableOperation();

			FinishUpdateGlobal(CandidateInstance, EUpdateResult::Error, &Operation->UpdateCallback);
			return;
		}

		if (LogBenchmarkUtil::isLoggingActive())
		{
			Operation->StartUpdateTime = FPlatformTime::Seconds();
		}

		FCustomizableObjectSystemPrivate* SystemPrivateData = System->GetPrivate();
		check(SystemPrivateData != nullptr);

		SystemPrivateData->CurrentInstanceBeingUpdated = CandidateInstance;

		// Prepare streaming for the current customizable object
		check(SystemPrivateData->Streamer != nullptr);
		SystemPrivateData->Streamer->PrepareStreamingForObject(CustomizableObject);

		check(SystemPrivateData->ExtensionDataStreamer != nullptr);
		SystemPrivateData->ExtensionDataStreamer->SetActiveObject(CustomizableObject);

		CandidateInstance->CommitMinMaxLOD();

		FString StateName = CandidateInstance->GetCustomizableObject()->GetStateName(CandidateInstance->GetState());
		const FParameterUIData* StateData = CandidateInstance->GetCustomizableObject()->StateUIDataMap.Find(StateName);

		bool bLiveUpdateMode = false;

		if (SystemPrivateData->EnableMutableLiveUpdate)
		{
			bLiveUpdateMode = StateData ? StateData->bLiveUpdateMode : false;
		}

		if (bLiveUpdateMode && (!Operation->bNeverStream || Operation->MipsToSkip > 0))
		{
			UE_LOG(LogMutable, Warning, TEXT("Instance LiveUpdateMode does not yet support progressive streaming of Mutable textures. Disabling LiveUpdateMode for this update."));
			bLiveUpdateMode = false;
		}

		bool bReuseInstanceTextures = false;

		if (SystemPrivateData->EnableReuseInstanceTextures)
		{
			bReuseInstanceTextures = StateData ? StateData->bReuseInstanceTextures : false;
			bReuseInstanceTextures |= CandidateInstancePrivateData->HasCOInstanceFlags(ReuseTextures);
			
			if (bReuseInstanceTextures && !Operation->bNeverStream)
			{
				UE_LOG(LogMutable, Warning, TEXT("Instance texture reuse requires that the current Mutable state is in non-streaming mode. Change it in the Mutable graph base node in the state definition."));
				bReuseInstanceTextures = false;
			}
		}

		if (!bLiveUpdateMode && CandidateInstancePrivateData->LiveUpdateModeInstanceID != 0)
		{
			// The instance was in live update mode last update, but now it's not. So the Id and resources have to be released.
			// Enqueue a new mutable task to release them
			Task_Game_ReleaseInstanceID(CandidateInstancePrivateData->LiveUpdateModeInstanceID);
			CandidateInstancePrivateData->LiveUpdateModeInstanceID = 0;
		}
		
		// Task: Mutable Update and GetMesh
		//-------------------------------------------------------------
		TSharedPtr<FMutableOperationData> CurrentOperationData = MakeShared<FMutableOperationData>();
		check(CurrentOperationData);
		CurrentOperationData->TextureCoverageQueries_MutableThreadParams = CandidateInstancePrivateData->TextureCoverageQueries;
		CurrentOperationData->TextureCoverageQueries_MutableThreadResults.Empty();
		CurrentOperationData->bCanReuseGeneratedData = SystemPrivateData->bEnableMutableReusePreviousUpdateData;
		CurrentOperationData->LastUpdateData = SystemPrivateData->bEnableMutableReusePreviousUpdateData ? CandidateInstancePrivateData->LastUpdateData : FInstanceGeneratedData();
		CurrentOperationData->CurrentMinLOD = Operation->InstanceDescriptorRuntimeHash.GetMinLOD();
		CurrentOperationData->CurrentMaxLOD = Operation->InstanceDescriptorRuntimeHash.GetMaxLOD();
		CurrentOperationData->bNeverStream = Operation->bNeverStream;
		CurrentOperationData->bLiveUpdateMode = bLiveUpdateMode;
		CurrentOperationData->bReuseInstanceTextures = bReuseInstanceTextures;
		CurrentOperationData->InstanceID = bLiveUpdateMode ? CandidateInstancePrivateData->LiveUpdateModeInstanceID : 0;
		CurrentOperationData->MipsToSkip = Operation->MipsToSkip;
		CurrentOperationData->MutableParameters = Parameters;
		CurrentOperationData->State = CandidateInstance->GetState();
		CurrentOperationData->UpdateResult = EUpdateResult::Success;
		CurrentOperationData->UpdateCallback = Operation->UpdateCallback;

		if (!CandidateInstancePrivateData->HasCOInstanceFlags(ForceGenerateMipTail))
		{
			CustomizableObject->GetLowPriorityTextureNames(CurrentOperationData->LowPriorityTextures);
		}

		if (System->IsOnlyGenerateRequestedLODsEnabled() && System->CurrentInstanceLODManagement->IsOnlyGenerateRequestedLODLevelsEnabled() && 
			!Operation->bForceGenerateAllLODs)
		{
			CurrentOperationData->RequestedLODs = Operation->InstanceDescriptorRuntimeHash.GetRequestedLODs();
		}

#ifdef MUTABLE_USE_NEW_TASKGRAPH
		UE::Tasks::FTask Mutable_GetMeshTask;
#else
		FGraphEventRef Mutable_GetMeshTask;
#endif
		{
			// Task inputs
			TWeakObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstancePtr = CandidateInstance;
			check(CustomizableObjectInstancePtr.IsValid());
			TSharedPtr<FMutableOperation> CurrentMutableOperation = SystemPrivateData->CurrentMutableOperation;
			check(CurrentMutableOperation);
			bool bBuildParameterRelevancy = CurrentMutableOperation->IsBuildParameterRelevancy();
			TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = CustomizableObject->GetPrivate()->GetModel();
			int32 State = CustomizableObjectInstancePtr->GetState();

			Mutable_GetMeshTask = SystemPrivateData->MutableTaskGraph.AddMutableThreadTask(
				TEXT("Task_Mutable_Update_GetMesh"),
				[CurrentOperationData, bBuildParameterRelevancy, Parameters, Model, State]()
				{
					impl::Task_Mutable_Update_GetMesh(CurrentOperationData, Model, Parameters, bBuildParameterRelevancy, State);
				});
		}


		// Task: Lock cache
		//-------------------------------------------------------------
		{
			// Task inputs
			TWeakObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstancePtr = CandidateInstance;
			check(CustomizableObjectInstancePtr.IsValid());
			TSharedPtr<FMutableOperation> CurrentMutableOperation = SystemPrivateData->CurrentMutableOperation;
			check(CurrentMutableOperation);
			bool bBuildParameterRelevancy = CurrentMutableOperation->IsBuildParameterRelevancy();

			SystemPrivateData->AddGameThreadTask(
				{
				FMutableTaskDelegate::CreateLambda(
					[CurrentOperationData, CustomizableObjectInstancePtr, bBuildParameterRelevancy, Parameters]()
					{
						impl::Task_Game_LockCache(CurrentOperationData, CustomizableObjectInstancePtr, Parameters, bBuildParameterRelevancy);
					}),
				Mutable_GetMeshTask
				});
		}
	}
} // namespace impl


void UCustomizableObjectSystem::AdvanceCurrentOperation() 
{
	MUTABLE_CPUPROFILER_SCOPE(AdvanceCurrentOperation);

	check(Private != nullptr);

	// See if we have a game-thread task to process
	FMutableTask* PendingTask = Private->PendingTasks.Peek();
	if (PendingTask)
	{
		if (PendingTask->AreDependenciesComplete())
		{
			PendingTask->ClearDependencies();
			PendingTask->Function.Execute();
			Private->PendingTasks.Pop();
		}

		// Don't do anything else until the pending work is completed.
		return;
	}

	// It is safe to do this now.
	Private->UpdateMemoryLimit();

	// If we don't have an ongoing operation, don't do anything.
	if (!Private->CurrentMutableOperation.IsValid())
	{
		return;
	}

	// If we reach here it means:
	// - we have an ongoing operations
	// - we have no pending work for the ongoing operation
	// - so we are starting it.
	{
		MUTABLE_CPUPROFILER_SCOPE(OperationUpdate);

		// Start the first task of the update process. See namespace impl comments above.
		impl::Task_Game_StartUpdate(Private->CurrentMutableOperation);
	}
}


bool UCustomizableObjectSystem::Tick(float DeltaTime)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObjectSystem::Tick)
	
	// Building instances is not enabled in servers. If at some point relevant collision or animation data is necessary for server logic this will need to be changed.
#if UE_SERVER
	return true;
#endif

	if (!Private.IsValid())
	{
		return true;
	}

	UWorld* World = GWorld;
	if (World)
	{
		EWorldType::Type WorldType = World->WorldType;

		if (WorldType != EWorldType::PIE && WorldType != EWorldType::Game && WorldType != EWorldType::Editor && WorldType != EWorldType::GamePreview)
		{
			return true;
		}
	}

	// \TODO: Review: We should never compile an object from this tick, so this could be removed
#if WITH_EDITOR
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		return true; // Assets are still being loaded, so subobjects won't be found, compiled objects incomplete and thus updates wrong
	}

	// Do not tick if the CookCommandlet is running.
	if (IsRunningCookCommandlet())
	{
		return true;
	}
#endif
	
	// Get a new operation if we aren't working on one
	if (!Private->CurrentMutableOperation)
	{
		// Reset the instance relevancy
		// The RequestedUpdates only refer to LOD changes. User Customization and discards are handled separately
		FMutableInstanceUpdateMap RequestedLODUpdates;
		
		CurrentInstanceLODManagement->UpdateInstanceDistsAndLODs(RequestedLODUpdates);

		for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
		{
			if (IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate())
			{
				UCustomizableInstancePrivateData* ObjectInstancePrivateData = CustomizableObjectInstance->GetPrivate();

				if (ObjectInstancePrivateData->HasCOInstanceFlags(UsedByComponentInPlay))
				{
					ObjectInstancePrivateData->TickUpdateCloseCustomizableObjects(**CustomizableObjectInstance, RequestedLODUpdates);
				}
				else if (ObjectInstancePrivateData->HasCOInstanceFlags(UsedByComponent))
				{
					ensure(!RequestedLODUpdates.Contains(*CustomizableObjectInstance));
					ObjectInstancePrivateData->UpdateInstanceIfNotGenerated(**CustomizableObjectInstance, RequestedLODUpdates);
				}
				else
				{
					ensure(!RequestedLODUpdates.Contains(*CustomizableObjectInstance));
				}

				ObjectInstancePrivateData->ClearCOInstanceFlags((ECOInstanceFlags)(UsedByComponent | UsedByComponentInPlay | PendingLODsUpdate)); // TODO MTBL-391: Makes no sense to clear it here, what if an update is requested before we set it back to true
			}
			else
			{
				ensure(!RequestedLODUpdates.Contains(*CustomizableObjectInstance));
			}
		}

		TSharedPtr<FMutableOperation> FoundOperation;

		{
			// Look for the highest priority update between the pending updates and the LOD Requested Updates
			EQueuePriorityType MaxPriorityFound = EQueuePriorityType::Low;
			double MaxSquareDistanceFound = TNumericLimits<double>::Max();
			double MinTimeFound = TNumericLimits<double>::Max();
			const FMutablePendingInstanceUpdate* PendingInstanceUpdateFound = nullptr;
			FMutableUpdateCandidate* LODUpdateCandidateFound = nullptr;

			// Look for the highest priority Pending Update
			for (auto Iterator = Private->MutablePendingInstanceWork.GetUpdateIterator(); Iterator; ++Iterator)
			{
				FMutablePendingInstanceUpdate& PendingUpdate = *Iterator;

				if (PendingUpdate.CustomizableObjectInstance.IsValid())
				{
					EQueuePriorityType PriorityType = PendingUpdate.CustomizableObjectInstance->GetUpdatePriority(true, true, false, false);
					
					if (PendingUpdate.PriorityType <= MaxPriorityFound)
					{
						const double MinSquareDistFromComponentToPlayer = PendingUpdate.CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer;
						
						if (MinSquareDistFromComponentToPlayer < MaxSquareDistanceFound ||
							(MinSquareDistFromComponentToPlayer == MaxSquareDistanceFound && PendingUpdate.SecondsAtUpdate < MinTimeFound))
						{
							MaxPriorityFound = PriorityType;
							MaxSquareDistanceFound = MinSquareDistFromComponentToPlayer;
							MinTimeFound = PendingUpdate.SecondsAtUpdate;
							PendingInstanceUpdateFound = &PendingUpdate;
							LODUpdateCandidateFound = nullptr;
						}
					}
				}
				else
				{
					Iterator.RemoveCurrent();
				}
			}

			// Look for a higher priority LOD update
			for (TPair<const UCustomizableObjectInstance*, FMutableUpdateCandidate>& LODUpdateTuple : RequestedLODUpdates)
			{
				const UCustomizableObjectInstance* Instance = LODUpdateTuple.Key;

				if (Instance)
				{
					FMutableUpdateCandidate& LODUpdateCandidate = LODUpdateTuple.Value;
					ensure(LODUpdateCandidate.HasBeenIssued());

					if (LODUpdateCandidate.Priority <= MaxPriorityFound)
					{
						if (LODUpdateCandidate.CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer < MaxSquareDistanceFound)
						{
							MaxPriorityFound = LODUpdateCandidate.Priority;
							MaxSquareDistanceFound = LODUpdateCandidate.CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer;
							PendingInstanceUpdateFound = nullptr;
							LODUpdateCandidateFound = &LODUpdateCandidate;
						}
					}
				}
			}

			Private->MutablePendingInstanceWork.SetLODUpdatesLastTick(RequestedLODUpdates.Num());

			// If the chosen LODUpdate has the same instance as a PendingUpdate, choose the PendingUpdate to apply both the LOD update
			// and customization change
			if (LODUpdateCandidateFound)
			{
				if (const FMutablePendingInstanceUpdate *PendingUpdateWithSameInstance = Private->MutablePendingInstanceWork.GetUpdate(LODUpdateCandidateFound->CustomizableObjectInstance))
				{
					PendingInstanceUpdateFound = PendingUpdateWithSameInstance;
					LODUpdateCandidateFound = nullptr;

					// In the processing of the PendingUpdate just below, it will add the LODUpdate's LOD params
				}
			}

			if (PendingInstanceUpdateFound)
			{
				check(!LODUpdateCandidateFound);

				UCustomizableObjectInstance* PendingInstance = PendingInstanceUpdateFound->CustomizableObjectInstance.Get();
				ensure(PendingInstance);

				// Maybe there's a LODUpdate that has the same instance, merge both updates as an optimization
				FMutableUpdateCandidate* LODUpdateWithSameInstance = RequestedLODUpdates.Find(PendingInstance);
				
				if (LODUpdateWithSameInstance)
				{
					LODUpdateWithSameInstance->ApplyLODUpdateParamsToInstance();
				}

				// No need to do a DoUpdateSkeletalMesh, as it was already done when adding the pending update

				FoundOperation = MakeShared<FMutableOperation>(FMutableOperation::CreateInstanceUpdate(PendingInstance,
					PendingInstanceUpdateFound->bNeverStream, PendingInstanceUpdateFound->MipsToSkip, PendingInstanceUpdateFound->Callback));

				Private->MutablePendingInstanceWork.RemoveUpdate(PendingInstanceUpdateFound->CustomizableObjectInstance);
			}
			else if(LODUpdateCandidateFound)
			{
				// Commit the LOD changes
				LODUpdateCandidateFound->ApplyLODUpdateParamsToInstance();

				// No need to check if a PendingUpdate has the same instance, it would already have been caught in the 
				// previous if statements

				// LOD updates are detected automatically and have never had a DoUpdateSkeletalMesh, so call it without enqueing a new update
				const EUpdateRequired Required = EUpdateRequired::Update;
				LODUpdateCandidateFound->CustomizableObjectInstance->DoUpdateSkeletalMesh(true, false, false, false, &Required, nullptr);
				
				bool bNeverStream = false;
				int32 MipsToSkip = 0;

				ensure(LODUpdateCandidateFound->CustomizableObjectInstance);
				GetPrivate()->GetMipStreamingConfig(*LODUpdateCandidateFound->CustomizableObjectInstance, bNeverStream, MipsToSkip);

				FoundOperation = MakeShared<FMutableOperation>(FMutableOperation::CreateInstanceUpdate(LODUpdateCandidateFound->CustomizableObjectInstance,
					bNeverStream, MipsToSkip, nullptr));
			}
		}

		{
			for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
			{
				if (IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate())
				{
					CustomizableObjectInstance->GetPrivate()->LastMinSquareDistFromComponentToPlayer = CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer;
					CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer = FLT_MAX;
				}
			}
		}

		// Update the streaming limit if it has changed. It is safe to do this now.
		Private->UpdateMemoryLimit();

		// Free memory before starting the new update
		DiscardInstances();
		ReleaseInstanceIDs();

		// Decide the next mutable operation to perform.
		if (FoundOperation.IsValid())
		{
			Private->CurrentMutableOperation = FoundOperation;

			UCustomizableObjectInstance* FoundInstance = Private->CurrentMutableOperation->CustomizableObjectInstance.Get();
			
			if (FoundInstance->GetPrivate()->HasCOInstanceFlags(ForceGenerateAllLODs))
			{
				Private->CurrentMutableOperation->bForceGenerateAllLODs = true;
			}

			Private->CurrentMutableOperation->CustomizableObjectInstance->GetPrivate()->InstanceUpdateFlags(*FoundInstance);
		}
		else
		{
			Private->CurrentMutableOperation = nullptr;
		}
	}
	// Advance the current operation
	if (Private->CurrentMutableOperation)
	{
		AdvanceCurrentOperation();
	}
	
	// TODO: T2927
	if (LogBenchmarkUtil::isLoggingActive())
	{
		uint64 SizeCache = 0;
		for (const UTexture2D* CachedTextures : ProtectedCachedTextures)
		{
			if (CachedTextures)
			{
				SizeCache += CachedTextures->CalcTextureMemorySizeEnum(TMC_AllMips);
			}
		}
		SET_DWORD_STAT(STAT_MutableTextureCacheMemory, SizeCache / 1024.f);
	}

	Private->UpdateStats();

#if WITH_EDITOR
	TickRecompileCustomizableObjects();
#endif

	Private->MutableTaskGraph.Tick();

	return true;
}


TAutoConsoleVariable<int32> CVarMaxNumInstancesToDiscardPerTick(
	TEXT("mutable.MaxNumInstancesToDiscardPerTick"),
	30,
	TEXT("The maximum number of stale instances that will be discarded per tick by Mutable."),
	ECVF_Scalability);


void UCustomizableObjectSystem::DiscardInstances()
{
	// Handle instance discards
	int32 NumInstancesDiscarded = 0;
	const int32 DiscardLimitPerTick = CVarMaxNumInstancesToDiscardPerTick.GetValueOnGameThread();

	for (auto Iterator = Private->MutablePendingInstanceWork.GetDiscardIterator(); Iterator && NumInstancesDiscarded < DiscardLimitPerTick; ++Iterator)
	{
		MUTABLE_CPUPROFILER_SCOPE(OperationDiscard);

		UCustomizableObjectInstance* COI = Iterator->CustomizableObjectInstance.Get();

		if (COI)
		{
			UCustomizableInstancePrivateData* COIPrivateData = COI ? COI->GetPrivate() : nullptr;

			// Only discard resources if the instance is still out range (it could have got closer to the player since the task was queued)
			if (!CurrentInstanceLODManagement->IsOnlyUpdateCloseCustomizableObjectsEnabled() ||
				!COI ||
				((COIPrivateData != nullptr) &&
					(COIPrivateData->LastMinSquareDistFromComponentToPlayer > FMath::Square(CurrentInstanceLODManagement->GetOnlyUpdateCloseCustomizableObjectsDist()))
					)
				)
			{
				if (COI && COI->IsValidLowLevel())
				{
					check(COIPrivateData != nullptr);
					COIPrivateData->DiscardResourcesAndSetReferenceSkeletalMesh(COI);
					COIPrivateData->ClearCOInstanceFlags(Updating);
					COI->SkeletalMeshStatus = ESkeletalMeshState::Correct;
				}
			}
			else
			{
				check(COIPrivateData != nullptr);
				COIPrivateData->ClearCOInstanceFlags(Updating);
			}

			if (COI && !COI->HasAnySkeletalMesh())
			{
				// To solve the problem in the Mutable demo where PIE just after editor start made all instances appear as reference mesh until editor restart
				check(COIPrivateData != nullptr);
				COIPrivateData->ClearCOInstanceFlags(Generated);
			}
		}

		Iterator.RemoveCurrent();
		NumInstancesDiscarded++;
	}
}


TAutoConsoleVariable<int32> CVarMaxNumInstanceIDsToReleasePerTick(
	TEXT("mutable.MaxNumInstanceIDsToReleasePerTick"),
	30,
	TEXT("The maximum number of stale instances IDs that will be released per tick by Mutable."),
	ECVF_Scalability);


void UCustomizableObjectSystem::ReleaseInstanceIDs()
{
	// Handle ID discards
	int32 NumIDsReleased = 0;
	const int32 IDReleaseLimitPerTick = CVarMaxNumInstanceIDsToReleasePerTick.GetValueOnGameThread();

	for (auto Iterator = Private->MutablePendingInstanceWork.GetIDsToReleaseIterator();
		Iterator && NumIDsReleased < IDReleaseLimitPerTick; ++Iterator)
	{
		impl::Task_Game_ReleaseInstanceID(*Iterator);

		Iterator.RemoveCurrent();
		NumIDsReleased++;
	}
}


TArray<FCustomizableObjectExternalTexture> UCustomizableObjectSystem::GetTextureParameterValues()
{
	TArray<FCustomizableObjectExternalTexture> Result;

	for (const TWeakObjectPtr<UCustomizableSystemImageProvider> Provider : GetPrivateChecked()->GetImageProviderChecked()->ImageProviders)
	{
		if (Provider.IsValid())
		{
			Provider->GetTextureParameterValues(Result);
		}
	}

	return Result;
}


void UCustomizableObjectSystem::RegisterImageProvider(UCustomizableSystemImageProvider* Provider)
{
	GetPrivateChecked()->GetImageProviderChecked()->ImageProviders.Add(Provider);
}


void UCustomizableObjectSystem::UnregisterImageProvider(UCustomizableSystemImageProvider* Provider)
{
	GetPrivateChecked()->GetImageProviderChecked()->ImageProviders.Remove(Provider);
}



UDefaultImageProvider& UCustomizableObjectSystem::GetOrCreateDefaultImageProvider()
{
	if (!DefaultImageProvider)
	{
		DefaultImageProvider = NewObject<UDefaultImageProvider>();
		RegisterImageProvider(DefaultImageProvider);
	}

	return *DefaultImageProvider;
}

static bool bRevertCacheTextureParameters = false;
static FAutoConsoleVariableRef CVarRevertCacheTextureParameters(
	TEXT("mutable.RevertCacheTextureParameters"), bRevertCacheTextureParameters,
	TEXT("If true, FMutableOperation will not cache/uncache texture parameters. If false, FMutableOperation will add an additional reference to the TextureParameters being used in the update."));

void CacheTexturesParameters(const TArray<FName>& TextureParameters)
{
	if (bRevertCacheTextureParameters)
	{
		return;
	}

	if (!TextureParameters.IsEmpty() && UCustomizableObjectSystem::IsCreated())
	{
		FUnrealMutableImageProvider* ImageProvider = UCustomizableObjectSystem::GetInstance()->GetPrivateChecked()->GetImageProviderChecked();
		check(ImageProvider);

		for (const FName& TextureParameter : TextureParameters)
		{
			ImageProvider->CacheImage(TextureParameter, false);
		}
	}
}


void UnCacheTexturesParameters(const TArray<FName>& TextureParameters)
{
	if (bRevertCacheTextureParameters)
	{
		return;
	}

	if (!TextureParameters.IsEmpty() && UCustomizableObjectSystem::IsCreated())
	{
		FUnrealMutableImageProvider* ImageProvider = UCustomizableObjectSystem::GetInstance()->GetPrivateChecked()->GetImageProviderChecked();
		check(ImageProvider);

		for (const FName& TextureParameter : TextureParameters)
		{
			ImageProvider->UnCacheImage(TextureParameter, false);
		}
	}
}


FMutableOperation::FMutableOperation(const FMutableOperation& Other)
{
	bNeverStream = Other.bNeverStream;
	MipsToSkip = Other.MipsToSkip;
	CustomizableObjectInstance = Other.CustomizableObjectInstance;
	InstanceDescriptorRuntimeHash = Other.InstanceDescriptorRuntimeHash;
	bStarted = Other.bStarted;
	bBuildParameterRelevancy = Other.bBuildParameterRelevancy;
	Parameters = Other.Parameters;
	TextureParameters = Other.TextureParameters;
	UpdateCallback = Other.UpdateCallback;

	CacheTexturesParameters(TextureParameters);
}


FMutableOperation& FMutableOperation::operator=(const FMutableOperation& Other)
{
	bNeverStream = Other.bNeverStream;
	MipsToSkip = Other.MipsToSkip;
	CustomizableObjectInstance = Other.CustomizableObjectInstance;
	InstanceDescriptorRuntimeHash = Other.InstanceDescriptorRuntimeHash;
	bStarted = Other.bStarted;
	bBuildParameterRelevancy = Other.bBuildParameterRelevancy;
	Parameters = Other.Parameters;
	TextureParameters = Other.TextureParameters;
	UpdateCallback = Other.UpdateCallback;

	CacheTexturesParameters(TextureParameters);

	return *this;
}


FMutableOperation::~FMutableOperation()
{
	// Uncache Texture Parameters
	UnCacheTexturesParameters(TextureParameters);
}


FMutableOperation FMutableOperation::CreateInstanceUpdate(UCustomizableObjectInstance* InCustomizableObjectInstance, bool bInNeverStream, int32 InMipsToSkip, const FInstanceUpdateDelegate* UpdateCallback)
{
	check(InCustomizableObjectInstance != nullptr);
	check(InCustomizableObjectInstance->GetPrivate() != nullptr);
	check(InCustomizableObjectInstance->GetCustomizableObject() != nullptr);

	FMutableOperation Op;
	Op.bNeverStream = bInNeverStream;
	Op.MipsToSkip = InMipsToSkip;
	Op.CustomizableObjectInstance = InCustomizableObjectInstance;
	Op.InstanceDescriptorRuntimeHash = InCustomizableObjectInstance->GetUpdateDescriptorRuntimeHash();
	Op.bStarted = false;
	Op.bBuildParameterRelevancy = InCustomizableObjectInstance->GetBuildParameterRelevancy();
	Op.Parameters = InCustomizableObjectInstance->GetDescriptor().GetParameters();
	Op.TextureParameters = InCustomizableObjectInstance->GetPrivate()->UpdateTextureParameters;

	if (UpdateCallback)
	{
		Op.UpdateCallback = *UpdateCallback;		
	}
	
	InCustomizableObjectInstance->GetCustomizableObject()->ApplyStateForcedValuesToParameters(InCustomizableObjectInstance->GetState(), Op.Parameters.get());

	if (!Op.Parameters)
	{
		// Cancel the update because the parameters aren't valid, probably because the object is not compiled
		Op.CustomizableObjectInstance = nullptr;
	}

	CacheTexturesParameters(Op.TextureParameters);

	return Op;
}


int32 UCustomizableObjectSystem::GetNumInstances() const
{
	check(Private != nullptr);
	return Private->NumInstances;
}

int32 UCustomizableObjectSystem::GetNumPendingInstances() const
{
	check(Private != nullptr);
	return Private->NumPendingInstances;
}

int32 UCustomizableObjectSystem::GetTotalInstances() const
{
	check(Private != nullptr);
	return Private->TotalInstances;
}

int32 UCustomizableObjectSystem::GetTextureMemoryUsed() const
{
	check(Private != nullptr);
	return int32(Private->TextureMemoryUsed);
}

int32 UCustomizableObjectSystem::GetAverageBuildTime() const
{
	check(Private != nullptr);
	return Private->TotalBuiltInstances == 0 ? 0 : Private->TotalBuildMs / Private->TotalBuiltInstances;
}


bool UCustomizableObjectSystem::IsSupport16BitBoneIndexEnabled() const
{
	check(Private != nullptr);
	return Private->bSupport16BitBoneIndex;
}


bool UCustomizableObjectSystem::IsProgressiveMipStreamingEnabled() const
{
	check(Private != nullptr);
	return Private->EnableMutableProgressiveMipStreaming != 0;
}


void UCustomizableObjectSystem::SetProgressiveMipStreamingEnabled(bool bIsEnabled)
{
	check(Private != nullptr);
	Private->EnableMutableProgressiveMipStreaming = bIsEnabled ? 1 : 0;
}


bool UCustomizableObjectSystem::IsOnlyGenerateRequestedLODsEnabled() const
{
	check(Private != nullptr);
	return Private->EnableOnlyGenerateRequestedLODs != 0;
}


void UCustomizableObjectSystem::SetOnlyGenerateRequestedLODsEnabled(bool bIsEnabled)
{
	check(Private != nullptr);
	Private->EnableOnlyGenerateRequestedLODs = bIsEnabled ? 1 : 0;
}


void UCustomizableObjectSystem::AddUncompiledCOWarning(const UCustomizableObject& InObject, FString const* OptionalLogInfo)
{
	FString Msg;
	Msg += FString::Printf(TEXT("Warning: Customizable Object [%s] not compiled."), *InObject.GetName());
	GEngine->AddOnScreenDebugMessage((uint64)((PTRINT)&InObject), 10.0f, FColor::Red, Msg);

#if WITH_EDITOR
	// Mutable will spam these warnings constantly due to the tick and LOD manager checking for instances to update with every tick. Send only one message per CO in the editor.
	if (UncompiledCustomizableObjectIds.Find(InObject.GetVersionId()) != INDEX_NONE)
	{
		return;
	}
	
	// Add notification
	UncompiledCustomizableObjectIds.Add(InObject.GetVersionId());

	FMessageLog MessageLog("Mutable");
	MessageLog.Warning(FText::FromString(Msg));

	if (!UncompiledCustomizableObjectsNotificationPtr.IsValid())
	{
		FNotificationInfo Info(FText::FromString("Uncompiled Customizable Object/s found. Please, check the Message Log - Mutable for more information."));
		Info.bFireAndForget = true;
		Info.bUseThrobber = true;
		Info.FadeOutDuration = 1.0f;
		Info.ExpireDuration = 5.0f;

		UncompiledCustomizableObjectsNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	}

	const FString ErrorString = FString::Printf(
		TEXT("Customizable Object [%s] not compiled.  Compile via the editor or via code before instancing.  %s"),
		*InObject.GetName(), OptionalLogInfo ? **OptionalLogInfo : TEXT(""));

#else // !WITH_EDITOR
	const FString ErrorString = FString::Printf(
		TEXT("Customizable Object [%s] not compiled.  This is not an Editor build, so this is an unrecoverable bad state; could be due to code or a cook failure.  %s"),
		*InObject.GetName(), OptionalLogInfo ? **OptionalLogInfo : TEXT(""));
#endif

	// Also log an error so if this happens as part of a bug report we'll have this info.
	UE_LOG(LogMutable, Error, TEXT("%s"), *ErrorString);
}

void UCustomizableObjectSystem::EnableBenchmark()
{
	LogBenchmarkUtil::startLogging();
}

void UCustomizableObjectSystem::EndBenchmark()
{
	LogBenchmarkUtil::shutdownAndSaveResults();
}


void UCustomizableObjectSystem::SetReleaseMutableTexturesImmediately(bool bReleaseTextures)
{
	check(Private != nullptr);
	Private->bReleaseTexturesImmediately = bReleaseTextures;
}


#if WITH_EDITOR

void UCustomizableObjectSystem::OnPreBeginPIE(const bool bIsSimulatingInEditor)
{
	if (!EditorSettings.bCompileRootObjectsOnStartPIE || IsRunningGame() || IsCompilationDisabled())
	{
		return;
	}
	
	// Find root customizable objects
	FARFilter AssetRegistryFilter;
	UE_MUTABLE_GET_CLASSPATHS(AssetRegistryFilter).Add(UE_MUTABLE_TOPLEVELASSETPATH(TEXT("/Script/CustomizableObject"), TEXT("CustomizableObject")));
	AssetRegistryFilter.TagsAndValues.Add(FName("IsRoot"), FString::FromInt(1));

	TArray<FAssetData> OutAssets;
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().GetAssets(AssetRegistryFilter, OutAssets);

	TArray<FAssetData> TempObjectsToRecompile;
	for (const FAssetData& Asset : OutAssets)
	{
		// If it is referenced by PIE it should be loaded
		if (!Asset.IsAssetLoaded())
		{
			continue;
		}
		
		const UCustomizableObject* Object = Cast<UCustomizableObject>(Asset.GetAsset());
		if (!Object || Object->IsCompiled() || Object->IsLocked() || Object->bIsChildObject)
		{
			continue;
		}
		
		// Add uncompiled objects to the objects to cook list
		TempObjectsToRecompile.Add(Asset);
	}

	if (!TempObjectsToRecompile.IsEmpty())
	{
		FText Msg = FText::FromString(TEXT("Warning: one or more Customizable Objects used in PIE are uncompiled.\n\nDo you want to compile them?"));
		if (FMessageDialog::Open(EAppMsgType::OkCancel, Msg) == EAppReturnType::Ok)
		{
			ObjectsToRecompile.Empty(TempObjectsToRecompile.Num());
			RecompileCustomizableObjects(TempObjectsToRecompile);
		}
	}
}

void UCustomizableObjectSystem::StartNextRecompile()
{
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	FAssetData Itr = ObjectsToRecompile.Pop();
	UCustomizableObject* CustomizableObject = Cast<UCustomizableObject>(Itr.GetAsset());

	if (CustomizableObject)
	{
		FText UpdateMsg = FText::FromString(FString::Printf(TEXT("Compiling Customizable Objects:\n%s"), *CustomizableObject->GetName()));
		FSlateNotificationManager::Get().UpdateProgressNotification(RecompileNotificationHandle, NumObjectsCompiled, TotalNumObjectsToRecompile, UpdateMsg);

		// Use default options
		FCompilationOptions Options = CustomizableObject->CompileOptions;
		Options.bSilentCompilation = true;
		check(RecompileCustomizableObjectsCompiler != nullptr);
		RecompileCustomizableObjectsCompiler->Compile(*CustomizableObject, Options, true);
	}
}

void UCustomizableObjectSystem::RecompileCustomizableObjectAsync(const FAssetData& InAssetData,
	const UCustomizableObject* InObject)
{
	if (IsRunningGame() || IsCompilationDisabled())
	{
		return;
	}
	
	if ((InObject && InObject->IsLocked()) || ObjectsToRecompile.Find((InAssetData)) != INDEX_NONE)
	{
		return;
	}
	
	if (!ObjectsToRecompile.IsEmpty())
	{
		ObjectsToRecompile.Add(InAssetData);
	}
	else
	{
		RecompileCustomizableObjects({InAssetData});
	}
}

void UCustomizableObjectSystem::RecompileCustomizableObjects(const TArray<FAssetData>& InObjects)
{
	if (IsRunningGame() || IsCompilationDisabled())
	{
		return;
	}

	if (InObjects.Num())
	{
		if (!RecompileCustomizableObjectsCompiler)
		{
			RecompileCustomizableObjectsCompiler = GetNewCompiler();

			if (!RecompileCustomizableObjectsCompiler)
			{
				return;
			}
		}

		ObjectsToRecompile.Append(InObjects);

		TotalNumObjectsToRecompile = ObjectsToRecompile.Num();
		NumObjectsCompiled = 0;

		if (RecompileNotificationHandle.IsValid())
		{
			++TotalNumObjectsToRecompile;
			FSlateNotificationManager::Get().UpdateProgressNotification(RecompileNotificationHandle, NumObjectsCompiled, TotalNumObjectsToRecompile);
		}
		else
		{
			RecompileNotificationHandle = FSlateNotificationManager::Get().StartProgressNotification(FText::FromString(TEXT("Compiling Customizable Objects")), TotalNumObjectsToRecompile);
			StartNextRecompile();
		}
	}
}


void UCustomizableObjectSystem::TickRecompileCustomizableObjects()
{
	bool bUpdated = false;
	
	if (RecompileCustomizableObjectsCompiler)
	{
		bUpdated = RecompileCustomizableObjectsCompiler->Tick() || RecompileCustomizableObjectsCompiler->GetCompilationState() == ECustomizableObjectCompilationState::Failed;
	}

	if (bUpdated)
	{
		NumObjectsCompiled++;

		if (!ObjectsToRecompile.IsEmpty())
		{
			StartNextRecompile();
		}
		else // All objects compiled, clean up
		{
			// Delete compiler
			delete RecompileCustomizableObjectsCompiler;
			RecompileCustomizableObjectsCompiler = nullptr;

			// Remove progress bar
			FSlateNotificationManager::Get().UpdateProgressNotification(RecompileNotificationHandle, NumObjectsCompiled, TotalNumObjectsToRecompile);
			FSlateNotificationManager::Get().CancelProgressNotification(RecompileNotificationHandle);
			RecompileNotificationHandle.Reset();

			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
	}
}


uint64 UCustomizableObjectSystem::GetMaxChunkSizeForPlatform(const ITargetPlatform* TargetPlatform)
{
	const FString& PlatformName = TargetPlatform ? TargetPlatform->IniPlatformName() : FPlatformProperties::IniPlatformName();

	if (const int64* CachedMaxChunkSize = PlatformMaxChunkSize.Find(PlatformName))
	{
		return *CachedMaxChunkSize;
	}

	int64 MaxChunkSize = -1;

	if (!FParse::Value(FCommandLine::Get(), TEXT("ExtraFlavorChunkSize="), MaxChunkSize) || MaxChunkSize < 0)
	{
		FConfigFile PlatformIniFile;
		FConfigCacheIni::LoadLocalIniFile(PlatformIniFile, TEXT("Game"), true, *PlatformName);
		FString ConfigString;
		if (PlatformIniFile.GetString(TEXT("/Script/UnrealEd.ProjectPackagingSettings"), TEXT("MaxChunkSize"), ConfigString))
		{
			MaxChunkSize = FCString::Atoi64(*ConfigString);
		}
	}

	// If no limit is specified default it to MUTABLE_STREAMED_DATA_MAXCHUNKSIZE 
	if (MaxChunkSize <= 0)
	{
		MaxChunkSize = MUTABLE_STREAMED_DATA_MAXCHUNKSIZE;
	}

	PlatformMaxChunkSize.Add(PlatformName, MaxChunkSize);

	return MaxChunkSize;
}

#endif // WITH_EDITOR


void UCustomizableObjectSystem::CacheImage(FName ImageId)
{
	GetPrivateChecked()->GetImageProviderChecked()->CacheImage(ImageId, true);
}


void UCustomizableObjectSystem::UnCacheImage(FName ImageId)
{
	GetPrivateChecked()->GetImageProviderChecked()->UnCacheImage(ImageId, true);
}


void UCustomizableObjectSystem::ClearImageCache()
{
	GetPrivateChecked()->GetImageProviderChecked()->ClearCache(true);
}


bool FCustomizableObjectSystemPrivate::IsMutableAnimInfoDebuggingEnabled() const
{ 
#if WITH_EDITORONLY_DATA
	return EnableMutableAnimInfoDebugging > 0;
#else
	return false;
#endif
}


FUnrealMutableImageProvider* FCustomizableObjectSystemPrivate::GetImageProviderChecked() const
{
	check(ImageProvider)
	return ImageProvider.Get();
}


bool UCustomizableObjectSystem::IsMutableAnimInfoDebuggingEnabled() const
{
#if WITH_EDITOR
	return GetPrivateChecked()->IsMutableAnimInfoDebuggingEnabled();
#else
	return false;
#endif
}
