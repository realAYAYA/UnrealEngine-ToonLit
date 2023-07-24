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
#include "MuCO/UnrealMutableImageProvider.h"
#include "MuCO/UnrealMutableModelDiskStreamer.h"
#include "MuR/Model.h"
#include "MuR/Settings.h"
#include "TextureResource.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/Notifications/SNotificationList.h"

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
DEFINE_STAT(STAT_MutableTextureParameterDecorationMemory);
DEFINE_STAT(STAT_MutablePendingInstanceUpdates);
DEFINE_STAT(STAT_MutableAbandonedInstanceUpdates);
DEFINE_STAT(STAT_MutableInstanceBuildTime);
DEFINE_STAT(STAT_MutableInstanceBuildTimeAvrg);
DEFINE_STAT(STAT_MutableStreamingOps);
DEFINE_STAT(STAT_MutableStreamingCache);

// These stats are provided by the mutable runtime
DEFINE_STAT(STAT_MutableProfile_LiveInstanceCount);
DEFINE_STAT(STAT_MutableProfile_StreamingCacheBytes);
DEFINE_STAT(STAT_MutableProfile_InstanceUpdateCount);

DECLARE_CYCLE_STAT(TEXT("MutablePendingRelease Time"), STAT_MutablePendingRelease, STATGROUP_Game);
DECLARE_CYCLE_STAT(TEXT("MutableTask"), STAT_MutableTask, STATGROUP_Game);

UCustomizableObjectSystem* FCustomizableObjectSystemPrivate::SSystem = nullptr;


static TAutoConsoleVariable<int32> CVarStreamingMemory(
	TEXT("b.MutableStreamingMemory"),
	-1,
	TEXT("If different than 0, limit the amount of memory (in KB) to use to cache streaming data when building characters. 0 means no limit, -1 means use default (40Mb in PC and 20Mb in consoles)."),
	ECVF_Scalability);


bool FMutableQueue::IsEmpty() const
{
	return Array.Num() == 0;
}


int FMutableQueue::Num() const
{
	return Array.Num();
}


void FMutableQueue::Enqueue(const FMutableQueueElem& TaskToEnqueue)
{
	for (FMutableQueueElem& QueueElem : Array)
	{
		check(QueueElem.Operation);
		check(TaskToEnqueue.Operation);
		if ((QueueElem.Operation->Type == FMutableOperation::EOperationType::Update || QueueElem.Operation->Type == FMutableOperation::EOperationType::Discard) &&
			QueueElem.Operation->CustomizableObjectInstance == TaskToEnqueue.Operation->CustomizableObjectInstance)
		{
			QueueElem.Operation = TaskToEnqueue.Operation;
			QueueElem.PriorityType = FMath::Min(QueueElem.PriorityType, TaskToEnqueue.PriorityType);
			QueueElem.Priority = FMath::Min(QueueElem.Priority, TaskToEnqueue.Priority);
			
			return;
		}
	}

	Array.HeapPush(TaskToEnqueue);
}


void FMutableQueue::Dequeue(FMutableQueueElem* DequeuedTask)
{
	Array.HeapPop(*DequeuedTask);
}


void FMutableQueue::ChangePriorities()
{
	for (FMutableQueueElem& Elem : Array)
	{
		check(Elem.Operation);
		if (Elem.Operation->CustomizableObjectInstance.IsValid())
		{
			if (UCustomizableInstancePrivateData* CustomizableObjectInstancePrivateData = Elem.Operation->CustomizableObjectInstance->GetPrivate())
			{
				Elem.Priority = CustomizableObjectInstancePrivateData->MinSquareDistFromComponentToPlayer;
			}
		}
	}
}


void FMutableQueue::UpdatePriority(const UCustomizableObjectInstance* Instance)
{
	for (FMutableQueueElem& Elem : Array)
	{
		check(Elem.Operation);
		if (Elem.Operation->CustomizableObjectInstance == Instance)
		{
			check(Instance->GetPrivate() != nullptr);
			Elem.Priority = Instance->GetPrivate()->MinSquareDistFromComponentToPlayer;
			break;
		}
	}
}


const FMutableQueueElem* FMutableQueue::Get(const UCustomizableObjectInstance* Instance) const
{
	for (const FMutableQueueElem& Elem : Array)
	{
		check(Elem.Operation);
		if (Elem.Operation->CustomizableObjectInstance == Instance)
		{
			return &Elem;
		}
	}

	return nullptr;
}


void FMutableQueue::Sort()
{
	Array.Heapify();
}


UCustomizableObjectSystem* UCustomizableObjectSystem::GetInstance()
{
	if (!FCustomizableObjectSystemPrivate::SSystem)
	{
		UE_LOG(LogMutable, Log, TEXT("Creating system."));

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
		UE_LOG(LogMutable, Warning, TEXT("%s"), *LogData);

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

	mu::SettingsPtr pSettings = new mu::Settings;
	check(pSettings);
	pSettings->SetProfile(false);
	pSettings->SetStreamingCache(MUTABLE_STREAMING_CACHE);
	Private->MutableSystem = new mu::System(pSettings);
	check(Private->MutableSystem);

	Private->LastStreamingMemorySize = MUTABLE_STREAMING_CACHE;
	Private->Streamer = new FUnrealMutableModelBulkStreamer();
	check(Private->Streamer != nullptr);
	Private->MutableSystem->SetStreamingInterface(Private->Streamer);

	// Set up the external image provider, for image parameters.
	FUnrealMutableImageProvider* Provider = new FUnrealMutableImageProvider();
	check(Provider != nullptr);
	Private->ImageProvider = Provider;
	Private->MutableSystem->SetImageParameterGenerator(Provider);

#if WITH_EDITOR
	if (!IsRunningGame())
	{
		FEditorDelegates::PreBeginPIE.AddUObject(this, &UCustomizableObjectSystem::OnPreBeginPIE);
	}
#endif

	if (FParse::Param(FCommandLine::Get(), TEXT("MutablePortableSerialization")))
	{
		Private->bCompactSerialization = false;
	}

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
		Private->WaitForMutableTasks();

		// Clear the ongoing operation
		Private->CurrentMutableOperation = nullptr;

		// Deallocate streaming
		check(Private->Streamer != nullptr);
		Private->Streamer->EndStreaming();

		Private->CurrentInstanceBeingUpdated = nullptr;

		while (!Private->MutableOperationQueue.IsEmpty())
		{
			FMutableQueueElem AuxOperation;
			Private->MutableOperationQueue.Dequeue(&AuxOperation);
		}

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
		int CountPending = 0;
		for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
		{
			if ( IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate())
			{
				++CountTotal;

				if (CustomizableObjectInstance->SkeletalMeshStatus == ESkeletalMeshState::AsyncUpdatePending)
				{
					++CountPending;
				}

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
		NumPendingInstances = CountPending;
		//SET_DWORD_STAT(STAT_MutableSkeletalMeshResourceMemory, Size / 1024.f);
		SET_DWORD_STAT(STAT_MutablePendingInstanceUpdates, MutableOperationQueue.Num());

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

		uint64 SizeDecoration = 0;
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
					for (const FParameterDecorations& ParameterDecoration : CustomizableObjectInstance->GetPrivate()->ParameterDecorations)
					{
						for (const UTexture2D* ParameterDecorationImage : ParameterDecoration.Images)
						{
							if (ParameterDecorationImage && ParameterDecorationImage->IsValidLowLevel())
							{
								SizeDecoration += ParameterDecorationImage->CalcTextureMemorySizeEnum(TMC_AllMips);
							}
						}
					}

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
			LogBenchmarkUtil::updateStat("pending_instance_updates", MutableOperationQueue.Num());
			LogBenchmarkUtil::updateStat("allocated_textures", (int32)CountAllocated);
			LogBenchmarkUtil::updateStat("texture_resource_memory", (long double)Size / 1048576.0L);
			LogBenchmarkUtil::updateStat("texture_generated_memory", (long double)SizeGenerated / 1048576.0L);
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		SET_DWORD_STAT(STAT_MutableNumInstancesLOD0, CountLOD0);
		SET_DWORD_STAT(STAT_MutableNumInstancesLOD1, CountLOD1);
		SET_DWORD_STAT(STAT_MutableNumInstancesLOD2, CountLOD2);
		SET_DWORD_STAT(STAT_MutableNumAllocatedSkeletalMeshes, CountAllocatedSkeletalMesh);
		SET_DWORD_STAT(STAT_MutablePendingInstanceUpdates, MutableOperationQueue.Num());
		SET_DWORD_STAT(STAT_MutableNumAllocatedTextures, CountAllocated);
		SET_DWORD_STAT(STAT_MutableTextureResourceMemory, Size / 1024.f);
		SET_DWORD_STAT(STAT_MutableTextureParameterDecorationMemory, SizeDecoration / 1024.f);
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
					APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(CustomizableSkeletalComponent->GetWorld(), 0);
					
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
							for (TPair<int32, TSoftClassPtr<UAnimInstance>>& Entry : ComponentData->AnimSlotToBP)
							{
								FString AnimBPSlot;

								AnimBPSlot += FString::Printf(TEXT("%d"), Entry.Key) + FString("-") + Entry.Value.GetAssetName();
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


int32 FCustomizableObjectSystemPrivate::EnableReuseInstanceTextures = 0;

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


/** Update the given Instance Skeletal Meshes and call its callbacks. */
void UpdateSkeletalMesh(UCustomizableObjectInstance* CustomizableObjectInstance, const FDescriptorRuntimeHash& UpdatedDescriptorRuntimeHash)
{
	// This used to be CustomizableObjectInstance::UpdateSkeletalMesh_PostBeginUpdate3
	{
		MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_PostBeginUpdate3);
		check(CustomizableObjectInstance != nullptr);

#if !WITH_EDITOR
		for (int32 ComponentIndex = 0; ComponentIndex < CustomizableObjectInstance->SkeletalMeshes.Num(); ++ComponentIndex)
		{
			if (CustomizableObjectInstance->SkeletalMeshes[ComponentIndex])
			{
				CustomizableObjectInstance->SkeletalMeshes[ComponentIndex]->RebuildSocketMap();
			}
		}
#endif

		UCustomizableInstancePrivateData* CustomizableObjectInstancePrivateData = CustomizableObjectInstance->GetPrivate();
		check(CustomizableObjectInstancePrivateData != nullptr);
		for (TObjectIterator<UCustomizableSkeletalComponent> It; It; ++It)
		{
			UCustomizableSkeletalComponent* CustomizableSkeletalComponent = *It;

			if (CustomizableSkeletalComponent &&
				(CustomizableSkeletalComponent->CustomizableObjectInstance == CustomizableObjectInstance) &&
				CustomizableObjectInstance->SkeletalMeshes.IsValidIndex(CustomizableSkeletalComponent->ComponentIndex)
			   )
			{
				MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_SetSkeletalMesh);

				const bool bIsCreatingSkeletalMesh = CustomizableObjectInstancePrivateData->HasCOInstanceFlags(CreatingSkeletalMesh); //TODO MTBL-391: Review
				CustomizableSkeletalComponent->SetSkeletalMesh(CustomizableObjectInstance->SkeletalMeshes[CustomizableSkeletalComponent->ComponentIndex], false, bIsCreatingSkeletalMesh);

				if (CustomizableObjectInstancePrivateData->HasCOInstanceFlags(ReplacePhysicsAssets))
				{
					CustomizableSkeletalComponent->SetPhysicsAsset(
						CustomizableObjectInstance->SkeletalMeshes[CustomizableSkeletalComponent->ComponentIndex] ? 
						CustomizableObjectInstance->SkeletalMeshes[CustomizableSkeletalComponent->ComponentIndex]->GetPhysicsAsset() : nullptr);
				}
			}
		}

		CustomizableObjectInstancePrivateData->SetCOInstanceFlags(Generated);
		CustomizableObjectInstancePrivateData->ClearCOInstanceFlags(CreatingSkeletalMesh);

		CustomizableObjectInstance->bEditorPropertyChanged = false;

		{
			MUTABLE_CPUPROFILER_SCOPE(UpdateSkeletalMesh_UpdatedDelegate);

			EUpdateResult State = CustomizableObjectInstance->SkeletalMeshStatus == ESkeletalMeshState::Correct ? EUpdateResult::Success : EUpdateResult::Error;
			CustomizableObjectInstance->Updated(State, UpdatedDescriptorRuntimeHash);


#if WITH_EDITOR
			CustomizableObjectInstance->InstanceUpdated = true;

			// \TODO: Review if we can avoid all this editor code here.
			for (int32 MeshIndex = 0; MeshIndex < CustomizableObjectInstance->SkeletalMeshes.Num(); ++MeshIndex)
			{
				if (USkeletalMesh* SkeletalMesh = CustomizableObjectInstance->GetSkeletalMesh(MeshIndex))
				{
					FUnrealBakeHelpers::BakeHelper_RegenerateImportedModel(SkeletalMesh);
				}
			}
#endif //WITH_EDITOR
		}

		check(UCustomizableObjectSystem::GetInstance() != nullptr);
		FCustomizableObjectSystemPrivate* CustomizableObjectSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate();
		check(CustomizableObjectSystem != nullptr);
		if (CustomizableObjectSystem->bReleaseTexturesImmediately)
		{
			FMutableResourceCache& Cache = CustomizableObjectSystem->GetObjectCache(CustomizableObjectInstance->GetCustomizableObject());

			for (TPair<uint32, FGeneratedTexture>& Item : CustomizableObjectInstancePrivateData->TexturesToRelease)
			{
				UCustomizableInstancePrivateData::ReleaseMutableTexture(Item.Value.Id, Item.Value.Texture, Cache);
			}

			CustomizableObjectInstancePrivateData->TexturesToRelease.Empty();
		}
	}
}


void FCustomizableObjectSystemPrivate::InitUpdateSkeletalMesh(UCustomizableObjectInstance& Instance, FMutableQueueElem::EQueuePriorityType Priority)
{
	MUTABLE_CPUPROFILER_SCOPE(FCustomizableObjectSystemPrivate::InitUpdateSkeletalMesh);

	check(IsInGameThread());
	
	const FString CurrentState = Instance.GetCurrentState();
	const FParameterUIData* State = Instance.GetCustomizableObject()->StateUIDataMap.Find(CurrentState);
	const bool bNeverStream = State ? State->bDontCompressRuntimeTextures : false;
	const bool bUseMipmapStreaming = !bNeverStream;
	int32 MipsToSkip = 0; // 0 means all mips

	if (bUseMipmapStreaming && EnableMutableProgressiveMipStreaming)
	{
		MipsToSkip = 255; // This means skip all possible mips until only UTexture::GetStaticMinTextureResidentMipCount() are left
	}
	
	const TSharedPtr<FMutableOperation> Operation = MakeShared<FMutableOperation>(FMutableOperation::CreateInstanceUpdate(&Instance, bNeverStream, MipsToSkip));

	const FDescriptorRuntimeHash UpdateDescriptorHash = Instance.GetUpdateDescriptorRuntimeHash();

	if (const FMutableQueueElem* QueueElem = MutableOperationQueue.Get(&Instance))
	{
		if (const TSharedPtr<FMutableOperation>& QueuedOperation = QueueElem->Operation;
			QueuedOperation &&
			QueuedOperation->Type == FMutableOperation::EOperationType::Update &&
			UpdateDescriptorHash.IsSubset(QueuedOperation->InstanceDescriptorRuntimeHash))
		{
			return; // The the requested update is equal to the last enqueued update.
		}
	}

	if (CurrentMutableOperation &&
		CurrentMutableOperation->Type == FMutableOperation::EOperationType::Update &&
		UpdateDescriptorHash.IsSubset(CurrentMutableOperation->InstanceDescriptorRuntimeHash))
	{
		return; // The requested update is equal to the running update.
	}
	
	// These delegates must be called at the end of the begin update.
	Instance.BeginUpdateDelegate.Broadcast(&Instance);
	Instance.BeginUpdateNativeDelegate.Broadcast(&Instance);

	if (UpdateDescriptorHash.IsSubset(Instance.GetDescriptorRuntimeHash()))
	{
		Instance.SkeletalMeshStatus = ESkeletalMeshState::Correct; // TODO GMT MTBL-1033 should not be here. Move to UCustomizableObjectInstance::Updated
		UpdateSkeletalMesh(&Instance, Instance.GetDescriptorRuntimeHash());
	}
	else
	{
		Instance.SkeletalMeshStatus = ESkeletalMeshState::AsyncUpdatePending;
		MutableOperationQueue.Enqueue(FMutableQueueElem::Create(Operation, Priority, Instance.GetPrivate()->MinSquareDistFromComponentToPlayer));
	}
}


void FCustomizableObjectSystemPrivate::InitDiscardResourcesSkeletalMesh(UCustomizableObjectInstance* InCustomizableObjectInstance)
{
	check(IsInGameThread());

	if (InCustomizableObjectInstance && InCustomizableObjectInstance->IsValidLowLevel())
	{
		const TSharedRef<FMutableOperation> Operation = MakeShared<FMutableOperation>(FMutableOperation::CreateInstanceDiscard(InCustomizableObjectInstance));
		check(InCustomizableObjectInstance->GetPrivate() != nullptr);
		MutableOperationQueue.Enqueue(FMutableQueueElem::Create(Operation, FMutableQueueElem::EQueuePriorityType::High, InCustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer));
	}
}


void FCustomizableObjectSystemPrivate::InitInstanceIDRelease(mu::Instance::ID IDToRelease)
{
	check(IsInGameThread());

	const TSharedRef<FMutableOperation> Operation = MakeShared<FMutableOperation>(FMutableOperation::CreateInstanceIDRelease(IDToRelease));
	MutableOperationQueue.Enqueue(FMutableQueueElem::Create(Operation, FMutableQueueElem::EQueuePriorityType::High, 0));
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
	check(InObject->GetPrivate() != nullptr);
	check(!InObject->GetPrivate()->bLocked);
	check(IsInGameThread() && !IsInParallelGameThread());

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


void UCustomizableObjectSystem::UnlockObject(const class UCustomizableObject* Obj)
{
	check(Obj != nullptr);
	check(Obj->GetPrivate() != nullptr);
	check(Obj->GetPrivate()->bLocked);
	check(IsInGameThread() && !IsInParallelGameThread());
	Obj->GetPrivate()->bLocked = false;
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
	if (const FUnrealMutableModelBulkStreamer* Streamer = GetPrivate()->Streamer)
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

				// TODO: This was done for version 4.18
				//info.SkeletalMesh->GetImportedResource()->LODModels.Empty();
				//info.SkeletalMesh->LODInfo.Empty();

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


void FCustomizableObjectSystemPrivate::UpdateStreamingLimit()
{
	// This must run on game thread, and when the mutable thread is not running
	check(IsInGameThread());

	int32 VarValue = CVarStreamingMemory.GetValueOnGameThread();
	uint64_t MemoryBytes = VarValue < 0 ? MUTABLE_STREAMING_CACHE : uint64_t(VarValue) * 1024;
	if (MemoryBytes != LastStreamingMemorySize)
	{
		LastStreamingMemorySize = MemoryBytes;
		check(MutableSystem);
		MutableSystem->SetStreamingCache(MemoryBytes);
	}
}


// Asynchronous tasks performed during the creation or update of a mutable instance. 
// Check the documentation before modifying and keep it up to date.
// https://docs.google.com/drawings/d/109NlsdKVxP59K5TuthJkleVG3AROkLJr6N03U4bNp4s
// When it says "mutable thread" it means any task pool thread, but with the guarantee that no other thread is using the mutable runtime.
// Naming: Task_<thread>_<description>
namespace impl
{

	void Subtask_Mutable_UpdateParameterDecorations(const TSharedPtr<FMutableOperationData>& OperationData, const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& MutableModel, const mu::Parameters* MutableParameters)
	{
		MUTABLE_CPUPROFILER_SCOPE(UCustomizableInstancePrivateData::UpdateParameterDecorations)

		check(OperationData);
		OperationData->ParametersUpdateData.Clear();
		OperationData->RelevantParametersInProgress.Empty();

		check(MutableParameters);

		// This must run in the mutable thread.
		check(UCustomizableObjectSystem::GetInstance() != nullptr);
		check(UCustomizableObjectSystem::GetInstance()->GetPrivate() != nullptr);
		mu::SystemPtr MutableSystem = UCustomizableObjectSystem::GetInstance()->GetPrivate()->MutableSystem;

		// Decorations
		int32 NumParameters = MutableParameters->GetCount();

		{
			MUTABLE_CPUPROFILER_SCOPE(ParameterDecorations)

				// Generate all the images that decorate the parameters UI, only in the editor.
				for (int32 ParamIndex = 0; ParamIndex < NumParameters; ++ParamIndex)
				{
					FParameterDecorationsUpdateData::FParameterDesc ParamData;

					for (int32 ImageIndex = 0; ImageIndex < MutableParameters->GetAdditionalImageCount(ParamIndex); ++ImageIndex)
					{
						mu::ImagePtrConst Image = MutableSystem->BuildParameterAdditionalImage(
							MutableModel,
							MutableParameters,
							ParamIndex, ImageIndex);

						ParamData.Images.Add(Image);
					};

					OperationData->ParametersUpdateData.Parameters.Add(ParamData);
				}
		}

		// Update the parameter relevancy.
		{
			MUTABLE_CPUPROFILER_SCOPE(ParameterRelevancy)

			TArray<bool> Relevant;
			Relevant.SetNumZeroed(NumParameters);
			MutableSystem->GetParameterRelevancy(MutableModel, MutableParameters, Relevant.GetData());

			for (int32 ParamIndex = 0; ParamIndex < NumParameters; ++ParamIndex)
			{
				if (Relevant[ParamIndex])
				{
					OperationData->RelevantParametersInProgress.Add(ParamIndex);
				}
			}
		}
	}


	void Subtask_Mutable_BeginUpdate_GetMesh(const TSharedPtr<FMutableOperationData>& OperationData, TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model, const mu::Parameters* MutableParameters, int32 State)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_BeginUpdate);

		// This runs in the mutable thread.
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
			uint32 LODMask = 0xFFFFFFFF;

			Instance = System->BeginUpdate(OperationData->InstanceID, MutableParameters, State, LODMask);

			if (!Instance)
			{
				const mu::Error MutableError = mu::GetError();
				if (MutableError == mu::Error::Unsupported)
				{
					UE_LOG(LogMutable, Log, TEXT("The necessary functionality is not supported in this version of Mutable"));
				}

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
			}
		}

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

					if (InstanceSurfaceIndex >= 0)
					{
						OperationData->InstanceUpdateData.Surfaces.Push({});
						FInstanceUpdateData::FSurface& Surface = OperationData->InstanceUpdateData.Surfaces.Last();
						++Component.SurfaceCount;

						Surface.MaterialIndex = Instance->GetSurfaceCustomId(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex);
						Surface.SurfaceId = SurfaceId;

						// Images
						Surface.FirstImage = OperationData->InstanceUpdateData.Images.Num();
						Surface.ImageCount = Instance->GetImageCount(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 ImageIndex = 0; ImageIndex < Surface.ImageCount; ++ImageIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetImageId);

							OperationData->InstanceUpdateData.Images.Push({});
							FInstanceUpdateData::FImage& Image = OperationData->InstanceUpdateData.Images.Last();
							Image.Name = Instance->GetImageName(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex, ImageIndex);
							Image.ImageID = Instance->GetImageId(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex, ImageIndex);
							Image.FullImageSizeX = 0;
							Image.FullImageSizeY = 0;
						}

						// Vectors
						Surface.FirstVector = OperationData->InstanceUpdateData.Vectors.Num();
						Surface.VectorCount = Instance->GetVectorCount(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 VectorIndex = 0; VectorIndex < Surface.VectorCount; ++VectorIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetVector);
							OperationData->InstanceUpdateData.Vectors.Push({});
							FInstanceUpdateData::FVector& Vector = OperationData->InstanceUpdateData.Vectors.Last();
							Vector.Name = Instance->GetVectorName(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex, VectorIndex);
							Instance->GetVector(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex, VectorIndex, &Vector.Vector.R, &Vector.Vector.G, &Vector.Vector.B, &Vector.Vector.A);
						}

						// Scalars
						Surface.FirstScalar = OperationData->InstanceUpdateData.Scalars.Num();
						Surface.ScalarCount = Instance->GetScalarCount(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex);
						for (int32 ScalarIndex = 0; ScalarIndex < Surface.ScalarCount; ++ScalarIndex)
						{
							MUTABLE_CPUPROFILER_SCOPE(GetScalar);
							OperationData->InstanceUpdateData.Scalars.Push({});
							FInstanceUpdateData::FScalar& Scalar = OperationData->InstanceUpdateData.Scalars.Last();
							Scalar.Name = Instance->GetScalarName(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex, ScalarIndex);
							Scalar.Scalar = Instance->GetScalar(MutableLODIndex, ComponentIndex, InstanceSurfaceIndex, ScalarIndex);
						}
					}
				}
			}
		}

	}

	
	void Subtask_Mutable_GetImages(const TSharedPtr<FMutableOperationData>& OperationData, TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model, const mu::Parameters* MutableParameters, int32 State)
	{
		MUTABLE_CPUPROFILER_SCOPE(Subtask_Mutable_GetImages);

		// This runs in the mutable thread.

		check(OperationData);
		check(MutableParameters);

		check(UCustomizableObjectSystem::GetInstance() != nullptr);
		FCustomizableObjectSystemPrivate* CustomizableObjectSystemPrivateData = UCustomizableObjectSystem::GetInstance()->GetPrivate();
		check(CustomizableObjectSystemPrivateData != nullptr);
		mu::System* System = CustomizableObjectSystemPrivateData->MutableSystem.get();
		check(System != nullptr);

		// Generate all the required resources, that are not cached
		TArray<mu::RESOURCE_ID> ImagesInThisInstance;
		for (FInstanceUpdateData::FImage& Image : OperationData->InstanceUpdateData.Images)
		{
			MUTABLE_CPUPROFILER_SCOPE(GetImage);

			// This should only be done when using progressive images, since GetImageDesc does some actual processing.
			{
				mu::FImageDesc ImageDesc;
				System->GetImageDesc(OperationData->InstanceID, Image.ImageID, ImageDesc);
				Image.FullImageSizeX = ImageDesc.m_size[0];
				Image.FullImageSizeY = ImageDesc.m_size[1];
			}

			bool bCached = false;

			// See if it is cached from this same instance (can happen with LODs)
			bCached = ImagesInThisInstance.Contains(Image.ImageID);

			// See if it is cached from another instance
			if (!bCached)
			{
				bCached = CustomizableObjectSystemPrivateData->ProtectedObjectCachedImages.Contains(Image.ImageID);
			}

			if (bCached)
			{
				UE_LOG(LogMutable, Verbose, TEXT("Texture resource with id [%d] is cached."), Image.ImageID);
				INC_DWORD_STAT(STAT_MutableNumCachedTextures);
			}
			else
			{
				int32 MaxSize = FMath::Max(Image.FullImageSizeX, Image.FullImageSizeY);
				int32 FullLODCount = FMath::CeilLogTwo(MaxSize) + 1;
				int32 MinMipsInImage = FMath::Min(FullLODCount, UTexture::GetStaticMinTextureResidentMipCount());
				int32 MaxMipsToSkip = FullLODCount - MinMipsInImage;
				int32 MipsToSkip = FMath::Min(MaxMipsToSkip, OperationData->MipsToSkip);
				Image.Image = System->GetImage(OperationData->InstanceID, Image.ImageID, MipsToSkip);

				// We need one mip or the complete chain. Otherwise there was a bug.
				check(Image.Image);
				int32 FullMipCount = Image.Image->GetMipmapCount(Image.Image->GetSizeX(), Image.Image->GetSizeY());
				int32 RealMipCount = Image.Image->GetLODCount();
				if ((RealMipCount != 1) && (RealMipCount != FullMipCount))
				{
					MUTABLE_CPUPROFILER_SCOPE(GetImage_MipFix);

					UE_LOG(LogMutable, Warning, TEXT("Mutable generated an incomplete mip chain for image %d."), Image.ImageID);

					// Force the right number of mips. The missing data will be black.
					mu::ImagePtr NewImage = new mu::Image(Image.Image->GetSizeX(), Image.Image->GetSizeY(), FullMipCount, Image.Image->GetFormat());
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


	void Subtask_Mutable_PrepareTextures(const TSharedPtr<FMutableOperationData>& OperationData)
	{
		// This runs in a worker thread

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
					FTexturePlatformData* PlatformData = UCustomizableInstancePrivateData::MutableCreateImagePlatformData(MutableImage, -1, Image.FullImageSizeX, Image.FullImageSizeY);
					OperationData->ImageToPlatformDataMap.Add(Image.ImageID, PlatformData);
					OperationData->PendingTextureCoverageQueries.Add({ KeyName, Surface.MaterialIndex, PlatformData });
				}
			}
		}
	}


	void Subtask_Mutable_PrepareSkeletonData(const TSharedPtr<FMutableOperationData>& OperationData)
	{
		// This runs in a worker thread

		MUTABLE_CPUPROFILER_SCOPE(PrepareSkeletonData);

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
			TArray<uint32>& SkeletonIds = OperationData->InstanceUpdateData.Skeletons[ComponentIndex].SkeletonIds;
			TArray<FName>& BoneNames = OperationData->InstanceUpdateData.Skeletons[ComponentIndex].BoneNames;
			TMap<FName, FMatrix44f>& BoneMatricesWithScale = OperationData->InstanceUpdateData.Skeletons[ComponentIndex].BoneMatricesWithScale;

			// Use first valid LOD bone count as a potential total number of bones, used for pre-allocating data arrays
			if (MinLODComponent.Mesh && MinLODComponent.Mesh->GetSkeleton())
			{
				const int32 TotalPossibleBones = MinLODComponent.Mesh->GetSkeleton()->GetBoneCount();

				// Out Data
				BoneNames.Reserve(TotalPossibleBones);
				BoneMatricesWithScale.Reserve(TotalPossibleBones);
			}

			for (int32 LODIndex = OperationData->CurrentMinLOD; LODIndex <= OperationData->CurrentMaxLOD && LODIndex < LODCount; ++LODIndex)
			{
				MUTABLE_CPUPROFILER_SCOPE(PrepareSkeletonData_LODs);

				const FInstanceUpdateData::FLOD& CurrentLOD = OperationData->InstanceUpdateData.LODs[LODIndex];
				FInstanceUpdateData::FComponent& CurrentLODComponent = OperationData->InstanceUpdateData.Components[CurrentLOD.FirstComponent + ComponentIndex];
				mu::MeshPtrConst Mesh = CurrentLODComponent.Mesh;

				if (!Mesh || !Mesh->GetSkeleton())
				{
					continue;
				}

				// Add SkeletonIds 
				const int32 SkeletonIDsCount = Mesh->GetSkeletonIDsCount();
				for (int32 SkeletonIndex = 0; SkeletonIndex < SkeletonIDsCount; ++SkeletonIndex)
				{
					SkeletonIds.AddUnique(Mesh->GetSkeletonID(SkeletonIndex));
				}


				const int32 MaxBoneIndex = Mesh->GetBonePoseCount();

				// Add active bones and new names to the BoneNames array 
				CurrentLODComponent.BoneMap.Reserve(MaxBoneIndex);
				CurrentLODComponent.ActiveBones.Reserve(MaxBoneIndex);

				for (int32 BonePoseIndex = 0; BonePoseIndex < MaxBoneIndex; ++BonePoseIndex)
				{
					const FName BoneName = Mesh->GetBonePoseName(BonePoseIndex);
					check(BoneName != NAME_None);

					int32 BoneIndex = BoneNames.Find(BoneName);
					if (BoneIndex == INDEX_NONE)
					{
						BoneIndex = BoneNames.Add(BoneName);

						FTransform3f Transform;
						Mesh->GetBoneTransform(BonePoseIndex, Transform);
						BoneMatricesWithScale.Emplace(BoneName, Transform.Inverse().ToMatrixWithScale());
					}

					if (EnumHasAnyFlags(Mesh->GetBoneUsageFlags(BonePoseIndex), mu::EBoneUsageFlags::Skinning))
					{
						CurrentLODComponent.BoneMap.Add(BoneIndex);
					}

					CurrentLODComponent.ActiveBones.Add(BoneIndex);
				}
			}
		}
	}


	void Task_Mutable_Update_GetMesh(TSharedPtr<FMutableOperationData> OperationData, TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model, mu::ParametersPtrConst Parameters, bool bBuildParameterDecorations, int32 State)
	{
		// This runs in a worker thread.
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_GetMesh)

#if WITH_EDITOR
		uint32 StartCycles = FPlatformTime::Cycles();
#endif

		check(OperationData.IsValid());

		if (bBuildParameterDecorations)
		{
			// This cannot happen inside the beginupdate-endupdate cycle.
			Subtask_Mutable_UpdateParameterDecorations(OperationData, Model, Parameters.get());
		}
		else
		{
			OperationData->ParametersUpdateData.Clear();
			OperationData->RelevantParametersInProgress.Reset();
		}

		Subtask_Mutable_BeginUpdate_GetMesh(OperationData, Model, Parameters.get(), State);

		// TODO: Not strictly mutable: move to another worker thread task to free mutable access?
		Subtask_Mutable_PrepareSkeletonData(OperationData);

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


	void Task_Mutable_ReleaseInstance(TSharedPtr<FMutableOperationData> OperationData, mu::SystemPtr MutableSystem)
	{
		MUTABLE_CPUPROFILER_SCOPE(Task_Mutable_ReleaseInstance)

		// This runs in a worker thread.
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
			return;
		}

		UCustomizableObjectInstance* CustomizableObjectInstance = CustomizableObjectInstancePtr.Get();

		// TODO: Review checks.
		if (!CustomizableObjectInstance || !CustomizableObjectInstance->IsValidLowLevel() )
		{
			System->ClearCurrentMutableOperation();
			return;
		}

		FCustomizableObjectSystemPrivate * CustomizableObjectSystemPrivateData = System->GetPrivate();
		check(CustomizableObjectSystemPrivateData != nullptr);

		// Actual work
		// TODO MTBL-391: Review This hotfix
		UpdateSkeletalMesh(CustomizableObjectInstance, CustomizableObjectSystemPrivateData->CurrentMutableOperation->InstanceDescriptorRuntimeHash);

		// TODO: T2927
		if (LogBenchmarkUtil::isLoggingActive())
		{
			double deltaSeconds = FPlatformTime::Seconds() - CustomizableObjectSystemPrivateData->CurrentMutableOperation->StartUpdateTime;
			int32 deltaMs = int32(deltaSeconds * 1000);
			int64 streamingCache = CustomizableObjectSystemPrivateData->LastStreamingMemorySize / 1024;

			LogBenchmarkUtil::updateStat("customizable_instance_build_time", deltaMs);
			LogBenchmarkUtil::updateStat("mutable_streaming_cache_memory", (long double)streamingCache / 1024.0);
			CustomizableObjectSystemPrivateData->TotalBuildMs += deltaMs;
			CustomizableObjectSystemPrivateData->TotalBuiltInstances++;
			SET_DWORD_STAT(STAT_MutableInstanceBuildTime, deltaMs);
			SET_DWORD_STAT(STAT_MutableInstanceBuildTimeAvrg, CustomizableObjectSystemPrivateData->TotalBuildMs / CustomizableObjectSystemPrivateData->TotalBuiltInstances);
			SET_DWORD_STAT(STAT_MutableStreamingCache, streamingCache);

			mu::System* MutableSystem = CustomizableObjectSystemPrivateData->MutableSystem.get();
			if (MutableSystem)
			{
				SET_DWORD_STAT(STAT_MutableProfile_LiveInstanceCount, MutableSystem->GetProfileMetric(mu::System::ProfileMetric::LiveInstanceCount));
				SET_DWORD_STAT(STAT_MutableProfile_StreamingCacheBytes, MutableSystem->GetProfileMetric(mu::System::ProfileMetric::StreamingCacheBytes));
				SET_DWORD_STAT(STAT_MutableProfile_InstanceUpdateCount, MutableSystem->GetProfileMetric(mu::System::ProfileMetric::InstanceUpdateCount));
			}
			else
			{
				SET_DWORD_STAT(STAT_MutableProfile_LiveInstanceCount, 0);
				SET_DWORD_STAT(STAT_MutableProfile_StreamingCacheBytes, 0);
				SET_DWORD_STAT(STAT_MutableProfile_InstanceUpdateCount, 0);
			}
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
			return;
		}

		check(OperationData.IsValid());

		UCustomizableObjectInstance* CustomizableObjectInstance = CustomizableObjectInstancePtr.Get();

		// TODO: Review checks.
		bool bCancel = false;
		if (!CustomizableObjectInstance || !CustomizableObjectInstance->IsValidLowLevel())
		{
			bCancel = true;
		}

		// Actual work


		if (!bCancel)
		{
			UCustomizableInstancePrivateData* CustomizableInstancePrivateData = CustomizableObjectInstance->GetPrivate();
			check(CustomizableInstancePrivateData != nullptr);

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
		} // if (!bCancel)

		FCustomizableObjectSystemPrivate* CustomizableObjectSystemPrivateData = System->GetPrivate();
		check(CustomizableObjectSystemPrivateData != nullptr);

		// Next Task: Release Mutable. We need this regardless if we cancel or not
		//-------------------------------------------------------------		
		{
			mu::SystemPtr MutableSystem = CustomizableObjectSystemPrivateData->MutableSystem;
			CustomizableObjectSystemPrivateData->AddMutableThreadTask(
				TEXT("Task_Mutable_ReleaseInstance"),
				[OperationData, MutableSystem]() {Task_Mutable_ReleaseInstance(OperationData, MutableSystem); },
				UE::Tasks::ETaskPriority::BackgroundNormal);
		}


		// Next Task: Release Platform Data
		//-------------------------------------------------------------
		if (!bCancel)
		{
			TSharedPtr<FMutableReleasePlatformOperationData> ReleaseOperationData = MakeShared<FMutableReleasePlatformOperationData>();
			check(ReleaseOperationData);
			ReleaseOperationData->ImageToPlatformDataMap = MoveTemp(OperationData->ImageToPlatformDataMap);
			CustomizableObjectSystemPrivateData->AddAnyThreadTask(
				TEXT("Mutable_ReleasePlatformData"),
				[ReleaseOperationData]() { Task_Game_ReleasePlatformData(ReleaseOperationData); },
				UE::Tasks::ETaskPriority::BackgroundNormal
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
	}


	/** "Lock Cached Resources" */
	void Task_Game_LockCache(TSharedPtr<FMutableOperationData> OperationData, const TWeakObjectPtr<UCustomizableObjectInstance>& CustomizableObjectInstancePtr, mu::Ptr<const mu::Parameters> Parameters, bool bBuildParameterDecorations)
	{
		check(IsInGameThread());

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
		if (!System)
		{
			return;
		}

		UCustomizableObjectInstance* ObjectInstance = CustomizableObjectInstancePtr.Get();

		if (!ObjectInstance)
		{
			System->ClearCurrentMutableOperation();
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
			return;
		}

		check(OperationData.IsValid());
		
		// Process the parameter decorations if requested
		if (bBuildParameterDecorations)
		{
			ObjectInstancePrivateData->UpdateParameterDecorationsEngineResources(OperationData);
		}

		if (const TObjectPtr<UDefaultImageProvider> DefaultImageProvider = System->GetDefaultImageProvider())
		{
			DefaultImageProvider->CacheTextures(*ObjectInstance);
		}
		
		// Selectively lock the resource cache for the object used by this instance to avoid the destruction of resources that we may want to reuse.
		// When protecting textures there mustn't be any left from a previous update
		check(System->ProtectedCachedTextures.Num() == 0);

		FCustomizableObjectSystemPrivate* SystemPrivateData = System->GetPrivate();
		check(SystemPrivateData != nullptr);

		FMutableResourceCache& Cache = SystemPrivateData->GetObjectCache(CustomizableObject);

		System->ProtectedCachedTextures.Reset(Cache.Images.Num());
		SystemPrivateData->ProtectedObjectCachedImages.Reset(Cache.Images.Num());

		for (const FInstanceUpdateData::FImage& Image : OperationData->InstanceUpdateData.Images)
		{
			FMutableImageCacheKey Key(Image.ImageID, OperationData->MipsToSkip);
			TWeakObjectPtr<UTexture2D>* TexturePtr = Cache.Images.Find(Key);

			if (TexturePtr && TexturePtr->Get() && SystemPrivateData->TextureHasReferences(Image.ImageID))
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
		FGraphEventRef Mutable_GetImagesTask;
		{
			// Task inputs
			check(CustomizableObject->GetPrivate() != nullptr);
			TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = CustomizableObject->GetPrivate()->GetModel();
			int32 State = ObjectInstance->GetState();

			Mutable_GetImagesTask = SystemPrivateData->AddMutableThreadTask(
					TEXT("Task_Mutable_GetImages"),
					[OperationData, Parameters, Model, State]()
					{
						impl::Task_Mutable_Update_GetImages(OperationData, Model, Parameters, State);
					},
					UE::Tasks::ETaskPriority::BackgroundHigh);
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

		FGraphEventRef Mutable_GetMeshTask;
		{
			// Task inputs
			TSharedPtr<FMutableOperation> CurrentMutableOperation = SystemPrivateData->CurrentMutableOperation;
			check(CurrentMutableOperation);

			Mutable_GetMeshTask = SystemPrivateData->AddMutableThreadTask(
				TEXT("Task_Mutable_ReleaseInstanceID"),
				[CurrentOperationData, MutableSystem]()
				{
					impl::Task_Mutable_ReleaseInstanceID(CurrentOperationData, MutableSystem);
				},
				UE::Tasks::ETaskPriority::BackgroundHigh);
		}
	}


	/** Enqueue the release ID operation in the Mutable queue */
	void Task_Game_ReleaseInstanceID(TSharedPtr<FMutableOperation> Operation)
	{
		check(Operation);
		check(Operation->Type == FMutableOperation::EOperationType::IDRelease);

		Task_Game_ReleaseInstanceID(Operation->IDToRelease);
	}


	/** "Start Update" */
	void Task_Game_StartUpdate(TSharedPtr<FMutableOperation> Operation)
	{
		check(Operation);
		check(Operation->Type == FMutableOperation::EOperationType::Update);

		UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstance();
		check(System != nullptr);

		if (!Operation->CustomizableObjectInstance.IsValid() || !Operation->CustomizableObjectInstance->IsValidLowLevel()) // Only start if it hasn't been already destroyed (i.e. GC after finish PIE)
		{
			System->ClearCurrentMutableOperation();
			return;
		}

		TObjectPtr<UCustomizableObjectInstance> CandidateInstance = Operation->CustomizableObjectInstance.Get();
		
		UCustomizableInstancePrivateData* CandidateInstancePrivateData = CandidateInstance->GetPrivate();
		check(CandidateInstancePrivateData != nullptr);

		if (CandidateInstancePrivateData && CandidateInstancePrivateData->HasCOInstanceFlags(PendingLODsUpdate))
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
			check(CustomizableObject->GetPrivate());
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
			UpdateSkeletalMesh(CandidateInstance, CandidateInstance->GetDescriptorRuntimeHash());
			bCancel = true;
		}

		mu::Ptr<const mu::Parameters> Parameters = Operation->GetParameters();
		if (!Parameters)
		{
			bCancel = true;
		}

		if (bCancel)
		{
			if (CandidateInstancePrivateData) 
			{
				CandidateInstancePrivateData->ClearCOInstanceFlags(Updating);
			}

			System->ClearCurrentMutableOperation();
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
			if (Operation->bNeverStream)
			{
				bReuseInstanceTextures = StateData ? StateData->bReuseInstanceTextures : false;
				bReuseInstanceTextures |= CandidateInstancePrivateData->HasCOInstanceFlags(ReuseTextures);
			}
			else
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

		if (System->IsOnlyGenerateRequestedLODsEnabled() && System->CurrentInstanceLODManagement->IsOnlyGenerateRequestedLODLevelsEnabled() && !CandidateInstancePrivateData->HasCOInstanceFlags(ForceGenerateAllLODs))
		{
			CurrentOperationData->RequestedLODs = Operation->InstanceDescriptorRuntimeHash.GetRequestedLODs();
		}

		FGraphEventRef Mutable_GetMeshTask;
		{
			// Task inputs
			TWeakObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstancePtr = CandidateInstance;
			check(CustomizableObjectInstancePtr.IsValid());
			TSharedPtr<FMutableOperation> CurrentMutableOperation = SystemPrivateData->CurrentMutableOperation;
			check(CurrentMutableOperation);
			bool bBuildParameterDecorations = CurrentMutableOperation->IsBuildParameterDecorations();
			check(CustomizableObject->GetPrivate() != nullptr);
			TSharedPtr<mu::Model, ESPMode::ThreadSafe> Model = CustomizableObject->GetPrivate()->GetModel();
			int32 State = CustomizableObjectInstancePtr->GetState();

			Mutable_GetMeshTask = SystemPrivateData->AddMutableThreadTask(
				TEXT("Task_Mutable_Update_GetMesh"),
				[CurrentOperationData, bBuildParameterDecorations, Parameters, Model, State]()
				{
					impl::Task_Mutable_Update_GetMesh(CurrentOperationData, Model, Parameters, bBuildParameterDecorations, State);
				},
				UE::Tasks::ETaskPriority::BackgroundHigh);
		}


		// Task: Lock cache
		//-------------------------------------------------------------
		{
			// Task inputs
			TWeakObjectPtr<UCustomizableObjectInstance> CustomizableObjectInstancePtr = CandidateInstance;
			check(CustomizableObjectInstancePtr.IsValid());
			TSharedPtr<FMutableOperation> CurrentMutableOperation = SystemPrivateData->CurrentMutableOperation;
			check(CurrentMutableOperation);
			bool bBuildParameterDecorations = CurrentMutableOperation->IsBuildParameterDecorations();

			SystemPrivateData->AddGameThreadTask(
				{
				FMutableTaskDelegate::CreateLambda(
					[CurrentOperationData, CustomizableObjectInstancePtr, bBuildParameterDecorations, Parameters]()
					{
						impl::Task_Game_LockCache(CurrentOperationData, CustomizableObjectInstancePtr, Parameters, bBuildParameterDecorations);
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

	// See if we can clear the last reference to the mutable-thread task
	Private->ClearMutableTaskIfDone();

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
	Private->UpdateStreamingLimit();

	// If we don't have an ongoing operation, don't do anything.
	if (!Private->CurrentMutableOperation.IsValid())
	{
		return;
	}

	// If we reach here it means:
	// - we have an ongoing operations
	// - we have no pending work for the ongoing operation
	// - so we are starting it.
	switch (Private->CurrentMutableOperation->Type)
	{
		case FMutableOperation::EOperationType::Discard:
		{
			MUTABLE_CPUPROFILER_SCOPE(OperationDiscard);

			// \TODO: Discards could be done in any case, concurrently with update operations. Should they be
			// in their own "queue"?

			UCustomizableObjectInstance* COI = Private->CurrentMutableOperation->CustomizableObjectInstance.Get();
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

			ClearCurrentMutableOperation();

			break;
		}

		case FMutableOperation::EOperationType::Update:
		{
			MUTABLE_CPUPROFILER_SCOPE(OperationUpdate);

			// Start the first task of the update process. See namespace impl comments above.
			impl::Task_Game_StartUpdate(Private->CurrentMutableOperation);
			break;
		}

		case FMutableOperation::EOperationType::IDRelease:
		{
			impl::Task_Game_ReleaseInstanceID(Private->CurrentMutableOperation);

			ClearCurrentMutableOperation();

			break;
		}

		default:
			check(false);
			break;
	}

}


bool UCustomizableObjectSystem::Tick(float DeltaTime)
{
	MUTABLE_CPUPROFILER_SCOPE(UCustomizableObjectSystem::Tick)
	
	// Building instances is not enable in servers. If at some point relevant collision or animation data is necessary for server logic this will need to be changed.
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
#endif


	MUTABLE_CPUPROFILER_SCOPE(TickCustomizableObjectSystem)

	// Get a new operation if we aren't working on one
	if (!Private->CurrentMutableOperation)
	{
		// Reset the instance relevancy
		// \TODO: Review
		// TODO: This should be done only when requiring a new job. And for the current operation instance, in case it needs to be cancelled.
		CurrentInstanceLODManagement->UpdateInstanceDistsAndLODs();

		for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
		{
			if (IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate())
			{
				UCustomizableInstancePrivateData* ObjectInstancePrivateData = CustomizableObjectInstance->GetPrivate();
				check(ObjectInstancePrivateData != nullptr);

				if (ObjectInstancePrivateData->HasCOInstanceFlags(UsedByComponentInPlay))
				{
					ObjectInstancePrivateData->TickUpdateCloseCustomizableObjects(**CustomizableObjectInstance);
				}
				else if (ObjectInstancePrivateData->HasCOInstanceFlags(UsedByComponent))
				{
					ObjectInstancePrivateData->UpdateInstanceIfNotGenerated(**CustomizableObjectInstance);
				}

				ObjectInstancePrivateData->ClearCOInstanceFlags((ECOInstanceFlags)(UsedByComponent | UsedByComponentInPlay | PendingLODsUpdate)); // TODO MTBL-391: Makes no sense to clear it here, what if an update is requested before we set it back to true
			}
		}

		// Update the queue. TODO: This should be done only when requiring a new job. And for the current operation instance, in case it needs to be cancelled.
		{
			Private->MutableOperationQueue.ChangePriorities();

			for (TObjectIterator<UCustomizableObjectInstance> CustomizableObjectInstance; CustomizableObjectInstance; ++CustomizableObjectInstance)
			{
				if (IsValidChecked(*CustomizableObjectInstance) && CustomizableObjectInstance->GetPrivate())
				{
					CustomizableObjectInstance->GetPrivate()->LastMinSquareDistFromComponentToPlayer = CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer;
					CustomizableObjectInstance->GetPrivate()->MinSquareDistFromComponentToPlayer = FLT_MAX;
				}
			}

			Private->MutableOperationQueue.Sort();
		}

		// Update the streaming limit if it has changed. It is safe to do this now.
		Private->UpdateStreamingLimit();

		// Decide the next mutable operation to perform.
		if (!Private->MutableOperationQueue.IsEmpty())
		{
			FMutableQueueElem AuxElem;
			Private->MutableOperationQueue.Dequeue(&AuxElem);
			Private->CurrentMutableOperation = AuxElem.Operation;
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

	return true;
}


TArray<FCustomizableObjectExternalTexture> UCustomizableObjectSystem::GetTextureParameterValues()
{
	TArray<FCustomizableObjectExternalTexture> Result;

	check(Private != nullptr);
	check(Private->ImageProvider != nullptr);
	for (const TWeakObjectPtr<UCustomizableSystemImageProvider> Provider : Private->ImageProvider->ImageProviders)
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
	check(Private != nullptr);
	check(Private->ImageProvider != nullptr);

	Private->ImageProvider->ImageProviders.Add(Provider);
}


void UCustomizableObjectSystem::UnregisterImageProvider(UCustomizableSystemImageProvider* Provider)
{
	check(Private != nullptr);
	check(Private->ImageProvider != nullptr);

	Private->ImageProvider->ImageProviders.Remove(Provider);
}


TObjectPtr<UDefaultImageProvider> UCustomizableObjectSystem::GetDefaultImageProvider() const
{
	return DefaultImageProvider;
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


FMutableOperation FMutableOperation::CreateInstanceUpdate(UCustomizableObjectInstance* InCustomizableObjectInstance, bool bInNeverStream, int32 InMipsToSkip)
{
	check(InCustomizableObjectInstance != nullptr);
	check(InCustomizableObjectInstance->GetPrivate() != nullptr);
	check(InCustomizableObjectInstance->GetCustomizableObject() != nullptr);

	FMutableOperation Op;
	Op.Type = EOperationType::Update;
	Op.bNeverStream = bInNeverStream;
	Op.MipsToSkip = InMipsToSkip;
	Op.CustomizableObjectInstance = InCustomizableObjectInstance;
	Op.InstanceDescriptorRuntimeHash = InCustomizableObjectInstance->GetUpdateDescriptorRuntimeHash();
	Op.bStarted = false;
	Op.bBuildParameterDecorations = InCustomizableObjectInstance->GetBuildParameterDecorations();
	Op.Parameters = InCustomizableObjectInstance->GetPrivate()->GetParameters(InCustomizableObjectInstance);

	InCustomizableObjectInstance->GetCustomizableObject()->ApplyStateForcedValuesToParameters(InCustomizableObjectInstance->GetState(), Op.Parameters.get());

	if (!Op.Parameters)
	{
		// Cancel the update because the parameters aren't valid, probably because the object is not compiled
		Op.CustomizableObjectInstance = nullptr;
	}

	return Op;
}


FMutableOperation FMutableOperation::CreateInstanceDiscard(UCustomizableObjectInstance* InCustomizableObjectInstance)
{
	check(InCustomizableObjectInstance != nullptr);
	check(InCustomizableObjectInstance->GetPrivate() != nullptr);

	FMutableOperation Op;
	Op.Type = EOperationType::Discard;
	Op.CustomizableObjectInstance = InCustomizableObjectInstance;

	Op.CustomizableObjectInstance->GetPrivate()->SetCOInstanceFlags(Updating);

	return Op;
}


FMutableOperation FMutableOperation::CreateInstanceIDRelease(mu::Instance::ID IDToRelease)
{
	FMutableOperation Op;
	Op.Type = EOperationType::IDRelease;
	Op.IDToRelease = IDToRelease;

	return Op;
}


void FMutableOperation::MutableIsDisabledCase()
{
	check(CustomizableObjectInstance != nullptr);
	check(CustomizableObjectInstance->GetPrivate() != nullptr);
	check(CustomizableObjectInstance->GetCustomizableObject() != nullptr);

	for (TObjectIterator<UCustomizableSkeletalComponent> It; It; ++It)
	{
		UCustomizableSkeletalComponent* CustomizableSkeletalComponent = *It;

		if (CustomizableSkeletalComponent && CustomizableSkeletalComponent->CustomizableObjectInstance == CustomizableObjectInstance.Get())
		{
			CustomizableSkeletalComponent->SetSkeletalMesh(CustomizableObjectInstance->GetCustomizableObject()->GetRefSkeletalMesh(CustomizableSkeletalComponent->ComponentIndex), false);
		}
	}

	CustomizableObjectInstance->GetPrivate()->SetCOInstanceFlags(Generated);
	CustomizableObjectInstance->GetPrivate()->ClearCOInstanceFlags(CreatingSkeletalMesh);

	if (CustomizableObjectInstance->bEditorPropertyChanged)
	{
		CustomizableObjectInstance->bEditorPropertyChanged = false;
	}

	// We must invalidate the current DescriptorRuntimeHash, since we're discarding everything.
	CustomizableObjectInstance->Updated(EUpdateResult::Success, FDescriptorRuntimeHash());

#if WITH_EDITOR
	CustomizableObjectInstance->InstanceUpdated = true;
#endif
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


bool UCustomizableObjectSystem::IsCompactSerializationEnabled() const
{
	check(Private != nullptr);
	return Private->bCompactSerialization;
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


bool UCustomizableObjectSystem::IsOnlyGenerateRequestedLODsEnabled() const
{
	check(Private != nullptr);
	return Private->EnableOnlyGenerateRequestedLODs != 0;
}


void UCustomizableObjectSystem::AddUncompiledCOWarning(UCustomizableObject* InObject)
{
	if (!InObject)
	{
		return;
	}

	FString Msg;
	Msg += FString::Printf(TEXT("Warning: Customizable Object [%s] not compiled. Please compile and save the object."), *InObject->GetName());
	GEngine->AddOnScreenDebugMessage((uint64)((PTRINT)InObject), 10.0f, FColor::Red, Msg);

#if WITH_EDITOR
	if (UncompiledCustomizableObjectIds.Find(InObject->GetVersionId()) == INDEX_NONE)
	{
		UncompiledCustomizableObjectIds.Add(InObject->GetVersionId());

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
	}
#endif
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
	AssetRegistryFilter.ClassPaths.Add(FTopLevelAssetPath(TEXT("/Script/CustomizableObject"), TEXT("CustomizableObject")));
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
	if (ObjectsToRecompile.Num() % 10 == 0) // TODO DRB: What?  If we can add more than one between calls, this might never be hit.  Why % 10?
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}

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


void UCustomizableObjectSystem::CacheImage(uint64 ImageId)
{
	check(GetPrivate() != nullptr);
	check(GetPrivate()->ImageProvider != nullptr);
	GetPrivate()->ImageProvider->CacheImage(ImageId);
}


void UCustomizableObjectSystem::UnCacheImage(uint64 ImageId)
{
	check(GetPrivate() != nullptr); 
	check(GetPrivate()->ImageProvider != nullptr);
	GetPrivate()->ImageProvider->UnCacheImage(ImageId);
}


void UCustomizableObjectSystem::CacheAllImagesInAllProviders(bool bClearPreviousCacheImages)
{
	check(GetPrivate() != nullptr);
	check(GetPrivate()->ImageProvider != nullptr);
	GetPrivate()->ImageProvider->CacheAllImagesInAllProviders(bClearPreviousCacheImages);
}


void UCustomizableObjectSystem::ClearImageCache()
{
	check(GetPrivate() != nullptr);
	check(GetPrivate()->ImageProvider != nullptr);
	GetPrivate()->ImageProvider->ClearCache();
}


bool FCustomizableObjectSystemPrivate::IsMutableAnimInfoDebuggingEnabled() const
{ 
#if WITH_EDITORONLY_DATA
	return EnableMutableAnimInfoDebugging > 0;
#else
	return false;
#endif
}


bool UCustomizableObjectSystem::IsMutableAnimInfoDebuggingEnabled() const
{
#if WITH_EDITOR
	return GetPrivate()->IsMutableAnimInfoDebuggingEnabled();
#else
	return false;
#endif
}
