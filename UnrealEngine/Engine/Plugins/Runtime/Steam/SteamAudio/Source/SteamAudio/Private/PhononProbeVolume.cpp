//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononProbeVolume.h"

#include "PhononProbeComponent.h"
#include "PhononScene.h"
#include "PhononCommon.h"

#include "Components/PrimitiveComponent.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"
#include "Engine/World.h"

#if WITH_EDITOR

#include "Editor.h"
#include "LevelEditorViewport.h"

#endif

APhononProbeVolume::APhononProbeVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, PlacementStrategy(EPhononProbePlacementStrategy::UNIFORM_FLOOR)
	, HorizontalSpacing(400.0f)
	, HeightAboveFloor(150.0f)
	, NumProbes(0)
{
	auto RootPrimitiveComponent = Cast<UPrimitiveComponent>(this->GetRootComponent());
	RootPrimitiveComponent->BodyInstance.SetCollisionProfileName("NoCollision");
	RootPrimitiveComponent->SetGenerateOverlapEvents(false);

	FRotator DefaultRotation(0, 0, 0);
	PhononProbeComponent = CreateDefaultSubobject<UPhononProbeComponent>(TEXT("PhononProbeComponent0"));
	PhononProbeComponent->SetWorldLocation(this->GetActorLocation());
	PhononProbeComponent->SetWorldRotation(DefaultRotation);
	PhononProbeComponent->SetupAttachment(this->GetRootComponent());
}

#if WITH_EDITOR

//==================================================================================================================================================
// Probe placement
//==================================================================================================================================================

void APhononProbeVolume::PlaceProbes(IPLhandle PhononScene, IPLProbePlacementProgressCallback ProbePlacementCallback, 
	TArray<IPLSphere>& ProbeSpheres)
{
	// TODO: delete probe batch data from disk if it exists

	IPLhandle ProbeBox = nullptr;

	// Compute box transform
	float ProbeBoxTransformMatrix[16];
	auto VolumeTransform = this->GetTransform();
	VolumeTransform.MultiplyScale3D(FVector(200));
	SteamAudio::GetMatrixForTransform(VolumeTransform, ProbeBoxTransformMatrix);

	// Configure placement parameters
	IPLProbePlacementParams ProbePlacementParameters;
	ProbePlacementParameters.placement = PlacementStrategy == EPhononProbePlacementStrategy::CENTROID ? IPL_PLACEMENT_CENTROID : IPL_PLACEMENT_UNIFORMFLOOR;
	ProbePlacementParameters.heightAboveFloor = HeightAboveFloor * SteamAudio::SCALEFACTOR;
	ProbePlacementParameters.spacing = HorizontalSpacing * SteamAudio::SCALEFACTOR;
	ProbePlacementParameters.maxOctreeDepth = 0;
	ProbePlacementParameters.maxOctreeTriangles = 0;

	// Create probe box, generate probes
	iplCreateProbeBox(SteamAudio::GlobalContext, PhononScene, ProbeBoxTransformMatrix, ProbePlacementParameters, ProbePlacementCallback, &ProbeBox);

	ProbeDataSize = SaveProbeBoxToDisk(ProbeBox);

	// Get probe locations/radii
	NumProbes = iplGetProbeSpheres(ProbeBox, nullptr);
	ProbeSpheres.SetNumUninitialized(NumProbes);
	iplGetProbeSpheres(ProbeBox, ProbeSpheres.GetData());

	// Clean up
	iplDestroyProbeBox(&ProbeBox);

	MarkPackageDirty();
}

//==================================================================================================================================================
// Probe box and probe batch serialization
//==================================================================================================================================================

int32 APhononProbeVolume::SaveProbeBatchToDisk(IPLhandle ProbeBatch)
{
	// Save probe batch data
	TArray<uint8> ProbeBatchData;
	int32 ProbeBatchDataSize = iplSaveProbeBatch(ProbeBatch, nullptr);
	ProbeBatchData.SetNumUninitialized(ProbeBatchDataSize);
	iplSaveProbeBatch(ProbeBatch, ProbeBatchData.GetData());

	// Write data to disk
	ProbeBatchFileName = ProbeBatchFileName.IsEmpty() ? GetProbeBatchFileName() : ProbeBatchFileName;
	FString ProbeBatchPath = SteamAudio::RuntimePath + ProbeBatchFileName;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle* ProbeBatchFileHandle = PlatformFile.OpenWrite(*ProbeBatchPath);
	if (ProbeBatchFileHandle)
	{
		ProbeBatchFileHandle->Write(ProbeBatchData.GetData(), ProbeBatchDataSize);
		delete ProbeBatchFileHandle;
		return ProbeBatchDataSize;
	}
	else
	{
		UE_LOG(LogSteamAudio, Warning, TEXT("Unable to save probe batch to disk."));
		return 0;
	}
}

int32 APhononProbeVolume::SaveProbeBoxToDisk(IPLhandle ProbeBox)
{
	// Save probe box data
	TArray<uint8> ProbeBoxData;
	int32 ProbeBoxDataSize = iplSaveProbeBox(ProbeBox, nullptr);
	ProbeBoxData.SetNumUninitialized(ProbeBoxDataSize);
	iplSaveProbeBox(ProbeBox, ProbeBoxData.GetData());

	// Write data to disk
	ProbeBoxFileName = ProbeBoxFileName.IsEmpty() ? GetProbeBoxFileName() : ProbeBoxFileName;
	FString ProbeBoxPath = SteamAudio::EditorOnlyPath + ProbeBoxFileName;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle* ProbeBoxFileHandle = PlatformFile.OpenWrite(*ProbeBoxPath);
	if (ProbeBoxFileHandle)
	{
		ProbeBoxFileHandle->Write(ProbeBoxData.GetData(), ProbeBoxDataSize);
		delete ProbeBoxFileHandle;
		return ProbeBoxDataSize;
	}
	else
	{
		UE_LOG(LogSteamAudio, Warning, TEXT("Unable to save probe box to disk."));
		return 0;
	}
}

//==================================================================================================================================================
// Updating box/batch data.
//==================================================================================================================================================

void APhononProbeVolume::UpdateProbeData(IPLhandle ProbeBox)
{
	// Update probe box serialized data
	SaveProbeBoxToDisk(ProbeBox);

	// Update probe batch serialized data
	IPLhandle ProbeBatch = nullptr;
	iplCreateProbeBatch(SteamAudio::GlobalContext, &ProbeBatch);

	NumProbes = iplGetProbeSpheres(ProbeBox, nullptr);
	for (int32 i = 0; i < NumProbes; ++i)
	{
		iplAddProbeToBatch(ProbeBatch, ProbeBox, i);
	}

	iplFinalizeProbeBatch(ProbeBatch);
	ProbeDataSize = SaveProbeBatchToDisk(ProbeBatch);
	iplDestroyProbeBatch(&ProbeBatch);

	MarkPackageDirty();
}

//==================================================================================================================================================
// UI editability rules
//==================================================================================================================================================

bool APhononProbeVolume::CanEditChange(const FProperty* InProperty) const
{
	const bool ParentVal = Super::CanEditChange(InProperty);

	if ((InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(APhononProbeVolume, HorizontalSpacing)) ||
		(InProperty->GetFName() == GET_MEMBER_NAME_CHECKED(APhononProbeVolume, HeightAboveFloor)))
	{
		return ParentVal && PlacementStrategy == EPhononProbePlacementStrategy::UNIFORM_FLOOR;
	}

	return ParentVal;
}

#endif

//==================================================================================================================================================
// Probe box and probe batch deserialization
//==================================================================================================================================================

bool APhononProbeVolume::LoadProbeBoxFromDisk(IPLhandle* ProbeBox) const
{
	TArray<uint8> ProbeBoxData;

	if (ProbeBoxFileName.IsEmpty())
	{
		return false;
	}

	FString ProbeBoxPath = SteamAudio::EditorOnlyPath + ProbeBoxFileName;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle* ProbeBoxFileHandle = PlatformFile.OpenRead(*ProbeBoxPath);
	if (ProbeBoxFileHandle)
	{
		ProbeBoxData.SetNum(ProbeBoxFileHandle->Size());
		ProbeBoxFileHandle->Read(ProbeBoxData.GetData(), ProbeBoxData.Num());
		iplLoadProbeBox(SteamAudio::GlobalContext, ProbeBoxData.GetData(), ProbeBoxData.Num(), ProbeBox);
		delete ProbeBoxFileHandle;
	}
	else
	{
		UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load probe box from disk %s."), *ProbeBoxPath);
		return false;
	}

	return true;
}

bool APhononProbeVolume::LoadProbeBatchFromDisk(IPLhandle* ProbeBatch) const
{
	TArray<uint8> ProbeBatchData;

	if (ProbeBatchFileName.IsEmpty())
	{
		return false;
	}

	FString ProbeBatchPath = SteamAudio::RuntimePath + ProbeBatchFileName;
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	IFileHandle* ProbeBatchFileHandle = PlatformFile.OpenRead(*ProbeBatchPath);
	if (ProbeBatchFileHandle)
	{
		ProbeBatchData.SetNum(ProbeBatchFileHandle->Size());
		ProbeBatchFileHandle->Read(ProbeBatchData.GetData(), ProbeBatchData.Num());
		iplLoadProbeBatch(SteamAudio::GlobalContext, ProbeBatchData.GetData(), ProbeBatchData.Num(), ProbeBatch);
		delete ProbeBatchFileHandle;
	}
	else
	{
		UE_LOG(LogSteamAudio, Warning, TEXT("Unable to load probe batch from disk %s."), *ProbeBatchPath);
		return false;
	}

	return true;
}

//==================================================================================================================================================
// Data size queries
//==================================================================================================================================================

int32 APhononProbeVolume::GetProbeBoxDataSize() const
{
	int32 ProbeBoxDataSize = 0;
	
	if (!ProbeBoxFileName.IsEmpty())
	{
		FString ProbeBoxPath = SteamAudio::EditorOnlyPath + ProbeBoxFileName;
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* ProbeBoxFileHandle = PlatformFile.OpenRead(*ProbeBoxPath);
		if (ProbeBoxFileHandle)
		{
			ProbeBoxDataSize = ProbeBoxFileHandle->Size();
			delete ProbeBoxFileHandle;
		}
	}
	
	return ProbeBoxDataSize;
}

int32 APhononProbeVolume::GetProbeBatchDataSize() const
{
	int32 ProbeBatchDataSize = 0;
	
	if (!ProbeBatchFileName.IsEmpty())
	{
		FString ProbeBatchPath = SteamAudio::RuntimePath + ProbeBatchFileName;
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		IFileHandle* ProbeBatchFileHandle = PlatformFile.OpenRead(*ProbeBatchPath);
		if (ProbeBatchFileHandle)
		{
			ProbeBatchDataSize = ProbeBatchFileHandle->Size();
			delete ProbeBatchFileHandle;
		}
	}

	return ProbeBatchDataSize;
}

int32 APhononProbeVolume::GetDataSizeForSource(const FName& UniqueIdentifier) const
{
	int32 SourceDataSize = 0;
	for (const auto& BakedSourceInfo : BakedDataInfo)
	{
		if (BakedSourceInfo.Name == UniqueIdentifier)
		{
			SourceDataSize += BakedSourceInfo.Size;
		}
	}
	return SourceDataSize;
}

//==================================================================================================================================================
// Filename generation
//==================================================================================================================================================

FString APhononProbeVolume::GetProbeBoxFileName() const
{
	return SteamAudio::StrippedMapName(GetWorld()->GetMapName()) + "_" + GetName() + ".probebox";
}

FString APhononProbeVolume::GetProbeBatchFileName() const
{
	return SteamAudio::StrippedMapName(GetWorld()->GetMapName()) + "_" + GetName() + ".probebatch";
}
