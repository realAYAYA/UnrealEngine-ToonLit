// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/LogBenchmarkUtil.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/SkeletalMesh.h"
#include "TextureResource.h"
#include "GameFramework/Pawn.h"
#include "Components/SkeletalMeshComponent.h"

#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif

#include "MuCO/UnrealPortabilityHelpers.h"
#include "MuCO/CustomizableObjectSystemPrivate.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableObjectInstancePrivate.h"
#include "MuCO/CustomizableObjectInstanceUsage.h"


extern ENGINE_API UEngine* GEngine;


TAutoConsoleVariable<bool> CVarEnableBenchmark(
	TEXT("mutable.EnableBenchmark"),
	false,
	TEXT("Enable or disable the benchmarking."));


namespace LogBenchmarkUtil
{
	void Write(FArchive& Archive, const FString& Text )
	{
		const FString LogUID = TEXT("MUTABLE_BENCHMARK");
		UE_LOG(LogMutable, Display, TEXT("%s : %s"),*LogUID, *Text);
		
		const FString ComposedString =  FString::Printf(TEXT("%s\n"),*Text);
		const FStringView StringView = ComposedString;
		Archive.Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(StringView.GetData(), StringView.Len()).Get()), StringView.Len() * sizeof(ANSICHAR));
	}
}


TSharedPtr<FArchive> CreateFile()
{
	const FString Directory = FPaths::ProfilingDir() + TEXT("Mutable/Benchmark");
	IFileManager::Get().MakeDirectory(*Directory, true);

	const FDateTime FileDate = FDateTime::Now();
	const FString Filename = FString::Printf(TEXT("%s/%s.csv"), *Directory,  *FileDate.ToString());

	TSharedPtr<FArchive> Archive = MakeShareable(IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_AllowRead | FILEWRITE_NoFail));
	check(Archive);

	const FString HeaderRow = TEXT(
		"ID_CO;ID_COI;ID_UpdateType;ID_Descriptor;ID_UpdateResult;"											// Basic identifying data
		"Context_LevelBegunPlay;"																	// The context of the update
		"Time_Queue;Time_Update;Time_TaskGetMesh;Time_TaskLockCache;"										// Time and memory data ...
		"Time_TaskGetImages;Time_TaskConvertResources;Time_TaskCallbacks;Memory_Update;Memory_Update_Real;"
		"Time_TaskUpdateImage;Memory_TaskUpdateImage;Memory_TaskUpdateImage_Real");

	LogBenchmarkUtil::Write(*Archive, HeaderRow);

	return Archive;
}


FLogBenchmarkUtil::~FLogBenchmarkUtil()
{
	if (Archive)
	{
		Archive->Close();
	}
}


void FLogBenchmarkUtil::GetInstancesStats(int32& OutNumInstances, int32& OutNumBuiltInstances, int32& OutNumInstancesLOD0, int32& OutNumInstancesLOD1, int32& OutNumInstancesLOD2, int32& OutNumAllocatedSkeletalMeshes) const
{
	OutNumInstances = 0;
	OutNumBuiltInstances = 0;
	OutNumInstancesLOD0 = 0;
	OutNumInstancesLOD1 = 0;
	OutNumInstancesLOD2 = 0;
	OutNumAllocatedSkeletalMeshes = 0;
	
	for (TObjectIterator<UCustomizableObjectInstance> Instance; Instance; ++Instance)
	{
		if (!IsValid(*Instance) ||
			Instance->HasAnyFlags(RF_ClassDefaultObject))
		{
			continue;
		}
		
		++OutNumInstances;

		OutNumInstancesLOD0 += Instance->GetCurrentMinLOD() == 0;
		OutNumInstancesLOD1 += Instance->GetCurrentMinLOD() == 1;
		OutNumInstancesLOD2 += Instance->GetCurrentMinLOD() >= 2;

		if (Instance->GetPrivate()->SkeletalMeshStatus == ESkeletalMeshStatus::Success)
		{
			++OutNumBuiltInstances;
		}
		
		for (int32 ComponentIndex = 0; ComponentIndex < Instance->GetNumComponents(); ++ComponentIndex)
		{
			if (Instance->GetSkeletalMesh(ComponentIndex))
			{
				++OutNumAllocatedSkeletalMeshes;
			}
		}
	}
}


void FLogBenchmarkUtil::AddTexture(UTexture2D& Texture)
{
	check(IsInGameThread());

	if (!CVarEnableBenchmark.GetValueOnGameThread())
	{
		return;
	}

	TextureTrackerArray.Add(&Texture);
}


void FLogBenchmarkUtil::UpdateStats()
{
	check(IsInGameThread());
	
	if (!CVarEnableBenchmark.GetValueOnGameThread())
	{
		return;
	}

	const UCustomizableObjectSystem* System = UCustomizableObjectSystem::GetInstanceChecked();
	
	// Instances and Skeletal Mesh
	int32 LocalNumInstances = 0;
	int32 LocalNumBuiltInstances = 0;
	int32 LocalNumInstancesLOD0 = 0;
	int32 LocalNumInstancesLOD1 = 0;
	int32 LocalNumInstancesLOD2 = 0;
	int32 LocalNumAllocatedSkeletalMeshes = 0;

	GetInstancesStats(LocalNumInstances, LocalNumBuiltInstances, LocalNumInstancesLOD0, LocalNumInstancesLOD1, LocalNumInstancesLOD2, LocalNumAllocatedSkeletalMeshes);

	NumInstances = LocalNumInstances;
	NumBuiltInstances = LocalNumBuiltInstances;
	NumInstancesLOD0 = LocalNumInstancesLOD0;
	NumInstancesLOD1 = LocalNumInstancesLOD1;
	NumInstancesLOD2 = LocalNumInstancesLOD2;
	NumAllocatedSkeletalMeshes = LocalNumAllocatedSkeletalMeshes;

	NumPendingInstanceUpdates = System->GetPrivate()->MutablePendingInstanceWork.Num();

	// Textures
	uint32 LocalNumAllocatedTextures = 0;
	uint64 LocalTextureGPUSize = 0;

	for (auto Iterator = TextureTrackerArray.CreateIterator(); Iterator; ++Iterator)
	{
		if (Iterator->IsStale())
		{
			Iterator.RemoveCurrent();
		}
		else if (Iterator->IsValid())
		{
			++LocalNumAllocatedTextures;

			UTexture2D* ActualTexture = Iterator->Get();

			if (ActualTexture && ActualTexture->GetResource())
			{
				LocalTextureGPUSize += (*Iterator)->CalcTextureMemorySizeEnum(ETextureMipCount::TMC_ResidentMips);
			}
		}
	}

	NumAllocatedTextures = LocalNumAllocatedTextures;
	TextureGPUSize = LocalTextureGPUSize;

#if WITH_EDITORONLY_DATA
	if (UCustomizableObjectSystem::GetInstance()->IsMutableAnimInfoDebuggingEnabled())
	{
		return;
	}

	if (GEngine)
	{
		return;
	}

	bool bFoundPlayer = false;
	int32 MsgIndex = 15820; // Arbitrary big value to prevent collisions with other on-screen messages

	for (TObjectIterator<UCustomizableObjectInstanceUsage> CustomizableObjectInstanceUsage; CustomizableObjectInstanceUsage; ++CustomizableObjectInstanceUsage)
	{
		if (!IsValid(*CustomizableObjectInstanceUsage) || CustomizableObjectInstanceUsage->IsNetMode(NM_DedicatedServer))
		{
			continue;
		}

		USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(CustomizableObjectInstanceUsage->GetAttachParent());
		AActor* ParentActor = Parent ? 
			Parent->GetAttachmentRootActor()
			:  nullptr;
		UCustomizableObjectInstance* Instance = CustomizableObjectInstanceUsage->GetCustomizableObjectInstance();

		APawn* PlayerPawn = nullptr;
		UWorld* World = Parent ? Parent->GetWorld() : nullptr;

		if (World)
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
			FCustomizableInstanceComponentData* ComponentData = Instance->GetPrivate()->GetComponentData(CustomizableObjectInstanceUsage->GetComponentIndex());

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
					CustomizableObjectInstanceUsage->GetComponentIndex()));
		}
	}

	if (!bFoundPlayer)
	{
		GEngine->AddOnScreenDebugMessage(MsgIndex, .0f, FColor::Yellow, TEXT("Mutable Animation info: N/A"));
	}
#endif
}


void FLogBenchmarkUtil::FinishUpdateMesh(const TSharedRef<FUpdateContextPrivate>& Context)
{
	check(IsInGameThread());

	if (!CVarEnableBenchmark.GetValueOnGameThread())
	{
		return;
	}
	
	TotalUpdateTime += Context->UpdateTime;
	++NumUpdates;

	InstanceBuildTimeAvrg = TotalUpdateTime / NumUpdates;

	const UCustomizableObjectInstance* Instance = Context->Instance.Get();
	if (!Instance)
	{
		return;
	}
	
	const UCustomizableObject* Object = Instance->GetCustomizableObject();
	if (!Object)
	{
		return;
	}

	if (!Archive) // Create file if it does not exist.
	{
		Archive = CreateFile();
	}

	const FString ID_CO = Context->Instance->GetCustomizableObject()->GetPathName();
	const FString ID_COI = Instance->GetPathName();
	const FString ID_UpdateType = TEXT("Mesh");
	const FString ID_Descriptor = Context->GetCapturedDescriptor().ToString();
	const FString ID_UpdateResult = StaticEnum<EUpdateResult>()->GetValueAsString(Context->UpdateResult);
	const double Time_Queue = Context->QueueTime * 1000;
	const double Time_Update = Context->UpdateTime * 1000;
	const double Time_TaskGetMesh = Context->TaskGetMeshTime * 1000;
	const double Time_TaskLockCache = Context->TaskLockCacheTime * 1000;
	const double Time_TaskGetImages = Context->TaskGetImagesTime * 1000;
	const double Time_TaskConvertResources = Context->TaskConvertResourcesTime * 1000;
	const double Time_TaskCallbacks =  Context->TaskCallbacksTime * 1000;

	// Context data to know in what situation did the update happen
	const FString Context_LevelBegunPlay =  Context->bLevelBegunPlay ? TEXT("true") : TEXT("false");

	const double Memory_UpdateEndPeakMB = (Context->UpdateEndPeakBytes / 1024.0) / 1024.0;
	const double Memory_UpdateEndRealPeakMB = (Context->UpdateEndRealPeakBytes / 1024.0) / 1024.0;
	
	const FString UpdateString = FString::Printf(TEXT("%s;%s;%s;%s;%s;%s;%f;%f;%f;%f;%f;%f;%f;%f;%f"), *ID_CO, *ID_COI, *ID_UpdateType, *ID_Descriptor, *ID_UpdateResult, *Context_LevelBegunPlay, Time_Queue, Time_Update, Time_TaskGetMesh, Time_TaskLockCache, Time_TaskGetImages, Time_TaskConvertResources, Time_TaskCallbacks, Memory_UpdateEndPeakMB, Memory_UpdateEndRealPeakMB);
	LogBenchmarkUtil::Write(*Archive, UpdateString);
	Archive->Flush();
}


void FLogBenchmarkUtil::FinishUpdateImage(const FString& CustomizableObjectPathName, const FString& InstancePathName, const FString& InstanceDescriptor, const double TaskUpdateImageTime, const int64 TaskUpdateImageMemoryPeak, const int64 TaskUpdateImageRealMemoryPeak) const
{
	check(IsInGameThread());

	if (!CVarEnableBenchmark.GetValueOnGameThread())
	{
		return;
	}

	const FString& ID_CO = CustomizableObjectPathName;
	const FString& ID_COI = InstancePathName;
	const FString ID_Descriptor = InstanceDescriptor;
	const FString ID_UpdateType = TEXT("Image");
	const double Time_TaskUpdateImage = TaskUpdateImageTime * 1000;
	const double Memory_TaskUpdateImagePeakMB = (TaskUpdateImageMemoryPeak / 1024.0) / 1024.0;
	const double Memory_TaskUpdateImageRealPeakMB = (TaskUpdateImageRealMemoryPeak / 1024.0) / 1024.0;

	const FString UpdateString = FString::Printf(TEXT("%s;%s;%s;%s;;;;;;;;;;;;%f;%f;%f"), *ID_CO, *ID_COI, *ID_UpdateType, *ID_Descriptor, Time_TaskUpdateImage, Memory_TaskUpdateImagePeakMB,Memory_TaskUpdateImageRealPeakMB);
	LogBenchmarkUtil::Write(*Archive, UpdateString);
	Archive->Flush();	
}

