// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/SkinWeightProfile.h"

#include "Animation/SkinWeightProfileManager.h"
#include "RenderingThread.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Components/SkinnedMeshComponent.h"
#include "ContentStreaming.h"
#include "UObject/AnimObjectVersion.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalMeshTypes.h"
#include "UObject/AnimObjectVersion.h"
#include "UObject/ObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "UObject/UObjectIterator.h"

#if WITH_EDITOR
#include "Rendering/SkeletalMeshLODImporterData.h"
#else
#include "Engine/GameEngine.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(SkinWeightProfile)

class ENGINE_API FSkinnedMeshComponentUpdateSkinWeightsContext
{
public:
	FSkinnedMeshComponentUpdateSkinWeightsContext(USkinnedAsset* InSkinnedAsset)
	{
		for (TObjectIterator<USkinnedMeshComponent> It; It; ++It)
		{
			if (It->GetSkinnedAsset() == InSkinnedAsset)
			{
				checkf(!It->IsUnreachable(), TEXT("%s"), *It->GetFullName());

				if (It->IsRenderStateCreated())
				{
					check(It->IsRegistered());
					MeshComponents.Add(*It);
				}
			}
		}
	}

	~FSkinnedMeshComponentUpdateSkinWeightsContext()
	{
		const int32 ComponentCount = MeshComponents.Num();
		for (int32 ComponentIndex = 0; ComponentIndex < ComponentCount; ++ComponentIndex)
		{
			USkinnedMeshComponent* Component = MeshComponents[ComponentIndex];

			if (Component->IsRegistered())
			{
				Component->UpdateSkinWeightOverrideBuffer();
			}
		}
	}


private:
	TArray< class USkinnedMeshComponent*> MeshComponents;
};


static void OnDefaultProfileCVarsChanged(IConsoleVariable* Variable)
{
	if (GSkinWeightProfilesLoadByDefaultMode >= 0)
	{
		const bool bClearBuffer = GSkinWeightProfilesLoadByDefaultMode == 2 || GSkinWeightProfilesLoadByDefaultMode == 0;
		const bool bSetBuffer = GSkinWeightProfilesLoadByDefaultMode == 3;

		if (bClearBuffer || bSetBuffer)
		{
			// Make sure no pending skeletal mesh LOD updates
			if (IStreamingManager::Get_Concurrent() && IStreamingManager::Get().IsRenderAssetStreamingEnabled(EStreamableRenderAssetType::SkeletalMesh))
			{
				IStreamingManager::Get().GetRenderAssetStreamingManager().BlockTillAllRequestsFinished();
			}

			for (TObjectIterator<USkeletalMesh> It; It; ++It)
			{
				if (*It)
				{
					if (FSkeletalMeshRenderData* RenderData = It->GetResourceForRendering())
					{
						FSkinnedMeshComponentRecreateRenderStateContext RecreateState(*It);
						for (int32 LODIndex = 0; LODIndex < RenderData->LODRenderData.Num(); ++LODIndex)
						{
							FSkeletalMeshLODRenderData& LOD = RenderData->LODRenderData[LODIndex];
							if (bClearBuffer)
							{
								LOD.SkinWeightProfilesData.ClearDynamicDefaultSkinWeightProfile(*It, LODIndex);
							}
							else if (bSetBuffer)
							{
								LOD.SkinWeightProfilesData.ClearDynamicDefaultSkinWeightProfile(*It, LODIndex);
								LOD.SkinWeightProfilesData.SetDynamicDefaultSkinWeightProfile(*It, LODIndex);
							}
						}
					}
				}
			}
		}
	}
}

int32 GSkinWeightProfilesLoadByDefaultMode = -1;
FAutoConsoleVariableRef CVarSkinWeightsLoadByDefaultMode(
	TEXT("a.SkinWeightProfile.LoadByDefaultMode"),
	GSkinWeightProfilesLoadByDefaultMode,
	TEXT("Enables/disables run-time optimization to override the original skin weights with a profile designated as the default to replace it. Can be used to optimize memory for specific platforms or devices")
	TEXT("-1 = disabled")
	TEXT("0 = static disabled")
	TEXT("1 = static enabled")
	TEXT("2 = dynamic disabled")
	TEXT("3 = dynamic enabled"),
	FConsoleVariableDelegate::CreateStatic(&OnDefaultProfileCVarsChanged),
	ECVF_Default
);

int32 GSkinWeightProfilesDefaultLODOverride = -1;
FAutoConsoleVariableRef CVarSkinWeightProfilesDefaultLODOverride(
	TEXT("a.SkinWeightProfile.DefaultLODOverride"),
	GSkinWeightProfilesDefaultLODOverride,
	TEXT("Override LOD index from which on the default Skin Weight Profile should override the Skeletal Mesh's default Skin Weights"),
	FConsoleVariableDelegate::CreateStatic(&OnDefaultProfileCVarsChanged),
	ECVF_Scalability
);

int32 GSkinWeightProfilesAllowedFromLOD = -1;
FAutoConsoleVariableRef CVarSkinWeightProfilesAllowedFromLOD(
	TEXT("a.SkinWeightProfile.AllowedFromLOD"),
	GSkinWeightProfilesAllowedFromLOD,
	TEXT("Override LOD index from which on the Skin Weight Profile can be applied"),
	FConsoleVariableDelegate::CreateStatic(&OnDefaultProfileCVarsChanged),
	ECVF_Scalability
);

FArchive& operator<<(FArchive& Ar, FRuntimeSkinWeightProfileData& OverrideData)
{
#if WITH_EDITOR
	if (Ar.UEVer() < VER_UE4_SKINWEIGHT_PROFILE_DATA_LAYOUT_CHANGES)
	{
		Ar << OverrideData.OverridesInfo_DEPRECATED;
		Ar << OverrideData.Weights_DEPRECATED;
	}
	else
#endif
	{	
		Ar << OverrideData.BoneIDs;
		Ar << OverrideData.BoneWeights;
		Ar << OverrideData.NumWeightsPerVertex;
	}
	
	Ar << OverrideData.VertexIndexToInfluenceOffset;


	return Ar;
}

FArchive& operator<<(FArchive& Ar, FSkinWeightProfilesData& LODData)
{
	Ar << LODData.OverrideData;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FRuntimeSkinWeightProfileData::FSkinWeightOverrideInfo& OverrideInfo)
{
#if WITH_EDITOR
	Ar << OverrideInfo.InfluencesOffset;
	Ar << OverrideInfo.NumInfluences_DEPRECATED;
#endif

	return Ar;
}

#if WITH_EDITORONLY_DATA
FArchive& operator<<(FArchive& Ar, FImportedSkinWeightProfileData& ProfileData)
{
	Ar << ProfileData.SkinWeights;
	Ar << ProfileData.SourceModelInfluences;

	return Ar;
}

FArchive& operator<<(FArchive& Ar, FRawSkinWeight& OverrideEntry)
{
	Ar.UsingCustomVersion(FAnimObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	if (Ar.IsLoading())
	{
		FMemory::Memzero(OverrideEntry.InfluenceBones);
		FMemory::Memzero(OverrideEntry.InfluenceWeights);
	}

	if (Ar.CustomVer(FAnimObjectVersion::GUID) < FAnimObjectVersion::UnlimitedBoneInfluences)
	{
		for (int32 InfluenceIndex = 0; InfluenceIndex < EXTRA_BONE_INFLUENCES; ++InfluenceIndex)
		{
			if (Ar.CustomVer(FAnimObjectVersion::GUID) < FAnimObjectVersion::IncreaseBoneIndexLimitPerChunk)
			{
				uint8 BoneIndex = 0;
				Ar << BoneIndex;
				OverrideEntry.InfluenceBones[InfluenceIndex] = BoneIndex;
			}
			else
			{
				Ar << OverrideEntry.InfluenceBones[InfluenceIndex];
			}

			uint8 Weight = 0;
			Ar << Weight;
			OverrideEntry.InfluenceWeights[InfluenceIndex] = (static_cast<uint16>(Weight) << 8) | Weight;
		}
	}
	else if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::IncreasedSkinWeightPrecision)
	{
		for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex)
		{
			uint8 Weight = 0;
			Ar << OverrideEntry.InfluenceBones[InfluenceIndex];
			Ar << Weight;
			OverrideEntry.InfluenceWeights[InfluenceIndex] = (static_cast<uint16>(Weight) << 8) | Weight;
		}
	}
	else
	{
		for (int32 InfluenceIndex = 0; InfluenceIndex < MAX_TOTAL_INFLUENCES; ++InfluenceIndex)
		{
			Ar << OverrideEntry.InfluenceBones[InfluenceIndex];
			Ar << OverrideEntry.InfluenceWeights[InfluenceIndex];
		}
	}

	return Ar;
}
#endif // WITH_EDITORONLY_DATA

void FSkinWeightProfilesData::Init(FSkinWeightVertexBuffer* InBaseBuffer) 
{
	BaseBuffer = InBaseBuffer;
}

FSkinWeightProfilesData::~FSkinWeightProfilesData()
{
	ReleaseResources();

	BaseBuffer = nullptr;
	DefaultOverrideSkinWeightBuffer = nullptr;

	bDefaultOverridden = false;
	bStaticOverridden = false;
	DefaultProfileName = NAME_None;
}

FSkinWeightProfilesData::FOnPickOverrideSkinWeightProfile FSkinWeightProfilesData::OnPickOverrideSkinWeightProfile;
#if !WITH_EDITOR
void FSkinWeightProfilesData::OverrideBaseBufferSkinWeightData(USkeletalMesh* Mesh, int32 LODIndex)
{
	if (GSkinWeightProfilesLoadByDefaultMode == 1)
	{
		const TArray<FSkinWeightProfileInfo>& Profiles = Mesh->GetSkinWeightProfiles();
		// Try and find a default buffer and whether or not it is set for this LOD index 
		int32 DefaultProfileIndex = INDEX_NONE;

		// Setup to not apply any skin weight profiles at this LOD level
		if (LODIndex >= GSkinWeightProfilesAllowedFromLOD)
		{
			DefaultProfileIndex = OnPickOverrideSkinWeightProfile.IsBound() ? OnPickOverrideSkinWeightProfile.Execute(Mesh, MakeArrayView(Profiles), LODIndex) : Profiles.IndexOfByPredicate([LODIndex](FSkinWeightProfileInfo ProfileInfo)
			{
				// In case the default LOD index has been overridden check against that
				if (GSkinWeightProfilesDefaultLODOverride >= 0)
				{
					return (ProfileInfo.DefaultProfile.Default && LODIndex >= GSkinWeightProfilesDefaultLODOverride);
				}

				// Otherwise check if this profile is set as default and the current LOD index is applicable
				return (ProfileInfo.DefaultProfile.Default && LODIndex >= ProfileInfo.DefaultProfileFromLODIndex.Default);
			});
		}

		bool bProfileSet = false;
		// If we found a profile try and find the override skin weights and apply if found
		if (DefaultProfileIndex != INDEX_NONE)
		{
			const FRuntimeSkinWeightProfileData* ProfilePtr = OverrideData.Find(Profiles[DefaultProfileIndex].Name);
			if (ProfilePtr)
			{
				ProfilePtr->ApplyDefaultOverride(BaseBuffer);
			}

			bDefaultOverridden = true;
			bStaticOverridden = true;
			DefaultProfileName = Profiles[DefaultProfileIndex].Name;
		}
	}
}
#endif 

void FSkinWeightProfilesData::SetDynamicDefaultSkinWeightProfile(USkeletalMesh* Mesh, int32 LODIndex, bool bSerialization /*= false*/)
{
	if (bStaticOverridden)
	{
		UE_LOG(LogSkeletalMesh, Error, TEXT("[%s] Skeletal Mesh has overridden the default Skin Weights buffer during serialization, cannot set any other skin weight profile."), *Mesh->GetName());
		return;
	}

	if (GSkinWeightProfilesLoadByDefaultMode == 3)
	{
		const TArray<FSkinWeightProfileInfo>& Profiles = Mesh->GetSkinWeightProfiles();
		// Try and find a default buffer and whether or not it is set for this LOD index 
		const int32 DefaultProfileIndex = Profiles.IndexOfByPredicate([LODIndex](FSkinWeightProfileInfo ProfileInfo)
		{
			// Setup to not apply any skin weight profiles at this LOD level
			if (LODIndex < GSkinWeightProfilesAllowedFromLOD)
			{
				return false;
			}

			// In case the default LOD index has been overridden check against that
			if (GSkinWeightProfilesDefaultLODOverride >= 0)
			{
				return (ProfileInfo.DefaultProfile.Default && LODIndex >= GSkinWeightProfilesDefaultLODOverride);
			}

			// Otherwise check if this profile is set as default and the current LOD index is applicable
			return (ProfileInfo.DefaultProfile.Default && LODIndex >= ProfileInfo.DefaultProfileFromLODIndex.Default);
		});

		bool bProfileSet = false;
		// If we found a profile try and find the override skin weights and apply if found
		if (DefaultProfileIndex != INDEX_NONE)
		{
			const bool bNoDefaultProfile = DefaultOverrideSkinWeightBuffer == nullptr;
			const bool bDifferentDefaultProfile = bNoDefaultProfile && (!bDefaultOverridden || DefaultProfileName != Profiles[DefaultProfileIndex].Name);
			if (bNoDefaultProfile || bDifferentDefaultProfile)
			{
				if (GetOverrideBuffer(Profiles[DefaultProfileIndex].Name) == nullptr)
				{
					if (bSerialization)
					{
						// During serialization the CPU copy of the weight should still be available
						const uint8* BaseBufferData = BaseBuffer->GetDataVertexBuffer()->GetWeightData();
						
						if (ensure(BaseBufferData))
						{
							FSkinWeightVertexBuffer* OverrideBuffer = nullptr;
							
							const FName& ProfileName = Profiles[DefaultProfileIndex].Name;
							const FRuntimeSkinWeightProfileData* ProfilePtr = OverrideData.Find(ProfileName);
							if (ProfilePtr)
							{
								FSkinnedMeshComponentUpdateSkinWeightsContext Context(Mesh);

								OverrideBuffer = new FSkinWeightVertexBuffer();
								OverrideBuffer->CopyMetaData(*BaseBuffer);
								ProfileNameToBuffer.Add(ProfileName, OverrideBuffer);
								
								ProfilePtr->ApplyOverrides(OverrideBuffer, BaseBufferData, BaseBuffer->GetNumVertices());

								DefaultOverrideSkinWeightBuffer = OverrideBuffer;
								bDefaultOverridden = true;
								DefaultProfileName = Profiles[DefaultProfileIndex].Name;
								
								const FName OwnerName(USkinnedAsset::GetLODPathName(Mesh, LODIndex));
								OverrideBuffer->SetOwnerName(OwnerName);
								OverrideBuffer->BeginInitResources();
							}
						}
					}
					else
					{
						FSkinWeightProfilesData* DataPtr = this;
						FRequestFinished Callback = [DataPtr](TWeakObjectPtr<USkeletalMesh> WeakMesh, FName ProfileName)
						{
							if (WeakMesh.IsValid())
							{
								FSkinnedMeshComponentRecreateRenderStateContext RecreateState(WeakMesh.Get());
								DataPtr->bDefaultOverridden = true;
								DataPtr->DefaultProfileName = ProfileName;
								DataPtr->SetupDynamicDefaultSkinweightProfile();								
							}
						};

						UWorld* World = nullptr;
#if WITH_EDITOR
						World = GWorld;
#else
						UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
						if (GameEngine)
						{
							World = GameEngine->GetGameWorld();
						}
#endif

						if (World)
						{
							if (FSkinWeightProfileManager* Manager = FSkinWeightProfileManager::Get(World))
							{
								Manager->RequestSkinWeightProfile(Profiles[DefaultProfileIndex].Name, Mesh, Mesh, Callback, LODIndex);
							}
						}
					}
				}
				else
				{
					bDefaultOverridden = true;
					DefaultProfileName = Profiles[DefaultProfileIndex].Name;

					SetupDynamicDefaultSkinweightProfile();
				}
			}
		}
	}
}

void FSkinWeightProfilesData::ClearDynamicDefaultSkinWeightProfile(USkeletalMesh* Mesh, int32 LODIndex)
{
	if (bStaticOverridden)
	{
		UE_LOG(LogSkeletalMesh, Error, TEXT("[%s] Skeletal Mesh has overridden the default Skin Weights buffer during serialization, cannot clear the skin weight profile."), *Mesh->GetName());		
		return;
	}

	if (bDefaultOverridden)
	{
		if (DefaultOverrideSkinWeightBuffer != nullptr)
		{
#if !WITH_EDITOR
			// Only release when not in Editor, as any other viewport / editor could be relying on this buffer
			ReleaseBuffer(DefaultProfileName, true);
#endif // !WITH_EDITOR
			DefaultOverrideSkinWeightBuffer = nullptr;
		}

		bDefaultOverridden = false;
		DefaultProfileName = NAME_None;		
	}
}

void FSkinWeightProfilesData::SetupDynamicDefaultSkinweightProfile()
{
	if (ProfileNameToBuffer.Contains(DefaultProfileName) && bDefaultOverridden && !bStaticOverridden)
	{
		DefaultOverrideSkinWeightBuffer = ProfileNameToBuffer.FindChecked(DefaultProfileName);
	}
}

bool FSkinWeightProfilesData::ContainsProfile(const FName& ProfileName) const
{
	return OverrideData.Contains(ProfileName);
}

FSkinWeightVertexBuffer* FSkinWeightProfilesData::GetOverrideBuffer(const FName& ProfileName) const
{
	SCOPED_NAMED_EVENT(FSkinWeightProfilesData_GetOverrideBuffer, FColor::Red);

	// In case we have overridden the default skin weight buffer we do not need to create an override buffer, if it was statically overridden we cannot load any other profile
	if (bDefaultOverridden && (ProfileName == DefaultProfileName || bStaticOverridden))
	{	
		if (bStaticOverridden && ProfileName != DefaultProfileName)
		{
			UE_LOG(LogSkeletalMesh, Error, TEXT("Skeletal Mesh has overridden the default Skin Weights buffer during serialization, cannot set any other skin weight profile."));
		}	

		return nullptr;
	}

	if (BaseBuffer)
	{
		check(BaseBuffer->GetNumVertices() > 0);
		if (ProfileNameToBuffer.Contains(ProfileName))
		{
			FSkinWeightVertexBuffer* Buffer = ProfileNameToBuffer.FindChecked(ProfileName);
			return Buffer;
		}
		}

	return nullptr;
}


bool FSkinWeightProfilesData::ContainsOverrideBuffer(const FName& ProfileName) const
{
	return ProfileNameToBuffer.Contains(ProfileName);
}


const FRuntimeSkinWeightProfileData* FSkinWeightProfilesData::GetOverrideData(const FName& ProfileName) const
{
	return OverrideData.Find(ProfileName);
}

FRuntimeSkinWeightProfileData& FSkinWeightProfilesData::AddOverrideData(const FName& ProfileName)
{
	return OverrideData.FindOrAdd(ProfileName);
}


void FSkinWeightProfilesData::ReleaseBuffer(const FName& ProfileName, bool bForceRelease /*= false*/)
{
	if (ProfileNameToBuffer.Contains(ProfileName) && (!bDefaultOverridden || ProfileName != DefaultProfileName || bForceRelease))
	{
		FSkinWeightVertexBuffer* Buffer = nullptr;
		ProfileNameToBuffer.RemoveAndCopyValue(ProfileName, Buffer);

		if (Buffer)
		{
			DEC_DWORD_STAT_BY(STAT_SkeletalMeshVertexMemory, Buffer->GetVertexDataSize());
			ENQUEUE_RENDER_COMMAND(ReleaseSkinSkinWeightProfilesDataBufferCommand)(
				[Buffer](FRHICommandListImmediate& RHICmdList)
			{			
				Buffer->ReleaseResources();
				delete Buffer;		
			});
		}
	}
}

void FSkinWeightProfilesData::ReleaseResources()
{
	TArray<FSkinWeightVertexBuffer*> Buffers;
	ProfileNameToBuffer.GenerateValueArray(Buffers);
	ProfileNameToBuffer.Empty();

	// Never release a default _dynamic_ buffer
	if (bDefaultOverridden && !bStaticOverridden)
	{
		ensure(DefaultOverrideSkinWeightBuffer != nullptr);
		Buffers.Remove(DefaultOverrideSkinWeightBuffer);
		ProfileNameToBuffer.Add(DefaultProfileName, DefaultOverrideSkinWeightBuffer);
	}

	Buffers.Remove(nullptr);

	ResetGPUReadback();

	if (Buffers.Num())
	{
		ENQUEUE_RENDER_COMMAND(ReleaseSkinSkinWeightProfilesDataBufferCommand)(
			[Buffers](FRHICommandListImmediate& RHICmdList)
		{
			for (FSkinWeightVertexBuffer* Buffer : Buffers)
			{	
				Buffer->ReleaseResources();
				delete Buffer;
			}
		});
	}
}

SIZE_T FSkinWeightProfilesData::GetResourcesSize() const
{
	SIZE_T SummedSize = 0;
	for (const TPair<FName, FSkinWeightVertexBuffer*>& Pair : ProfileNameToBuffer)
	{
		SummedSize += Pair.Value->GetVertexDataSize();
	}

	return SummedSize;
}

SIZE_T FSkinWeightProfilesData::GetCPUAccessMemoryOverhead() const
{
	SIZE_T Result = 0;
	for (typename TMap<FName, FSkinWeightVertexBuffer*>::TConstIterator It(ProfileNameToBuffer); It; ++It)
	{
		Result += It->Value->GetNeedsCPUAccess() ? It->Value->GetVertexDataSize() : 0;
	}
	return Result;
}

void FSkinWeightProfilesData::SerializeMetaData(FArchive& Ar)
{
	TArray<FName, TInlineAllocator<8>> ProfileNames;
	if (Ar.IsSaving())
	{
		OverrideData.GenerateKeyArray(ProfileNames);
		Ar << ProfileNames;
	}
	else
	{
		Ar << ProfileNames;
		OverrideData.Empty(ProfileNames.Num());
		for (int32 Idx = 0; Idx < ProfileNames.Num(); ++Idx)
		{
			OverrideData.Add(ProfileNames[Idx]);
		}
	}
}

void FSkinWeightProfilesData::ReleaseCPUResources()
{
	for (TMap<FName, FRuntimeSkinWeightProfileData>::TIterator It(OverrideData); It; ++It)
	{
		It->Value = FRuntimeSkinWeightProfileData();
	}

	ResetGPUReadback();
}

void FSkinWeightProfilesData::CreateRHIBuffers(FRHICommandListBase& RHICmdList, TArray<TPair<FName, FSkinWeightRHIInfo>>& OutBuffers)
{
	const int32 NumActiveProfiles = ProfileNameToBuffer.Num();
	check(BaseBuffer || !NumActiveProfiles);
	OutBuffers.Empty(NumActiveProfiles);
	for (TMap<FName, FSkinWeightVertexBuffer*>::TIterator It(ProfileNameToBuffer); It; ++It)
	{
		const FName& ProfileName = It->Key;
		FSkinWeightVertexBuffer* OverrideBuffer = It->Value;
		ApplyOverrideProfile(OverrideBuffer, ProfileName);
		OutBuffers.Emplace(ProfileName, OverrideBuffer->CreateRHIBuffer(RHICmdList));
	}
}

bool FSkinWeightProfilesData::IsPendingReadback() const
{
	return !ReadbackData.BufferReadback.IsValid();
}

void FSkinWeightProfilesData::EnqueueGPUReadback()
{
	ensure(!ReadbackData.BufferReadback.IsValid());

	if (BaseBuffer && BaseBuffer->GetDataVertexBuffer()->IsWeightDataValid() && BaseBuffer->GetDataVertexBuffer()->GetVertexDataSize())
	{
		static const FName ReadbackName = TEXT("ReadbackSkinWeightBuffer");
		FSkinweightReadbackData* DataPtr = &ReadbackData;
		ENQUEUE_RENDER_COMMAND(FSkinWeightProfilesData_EnqueueGPUReadback)(
			[DataPtr, this](FRHICommandListImmediate& RHICmdList)
		{
			DataPtr->BufferReadback.Reset(new FRHIGPUBufferReadback(ReadbackName));
			if (DataPtr->BufferReadback.IsValid() && BaseBuffer->GetDataVertexBuffer()->VertexBufferRHI->GetSize())
			{
				DataPtr->BufferReadback->EnqueueCopy(RHICmdList, BaseBuffer->GetDataVertexBuffer()->VertexBufferRHI);
			}
		});
	}
}

bool FSkinWeightProfilesData::IsGPUReadbackFinished() const
{
	return !IsPendingReadback() && ReadbackData.BufferReadback->IsReady();
}

void FSkinWeightProfilesData::EnqueueDataReadback()
{
	ensure(ReadbackData.ReadbackData.Num() == 0 && ReadbackData.BufferReadback->IsReady());
	
	if ( BaseBuffer )
	{
		ReadbackData.ReadbackData.SetNumZeroed(BaseBuffer->GetVertexDataSize());

		FSkinweightReadbackData* DataPtr = &ReadbackData;
		ENQUEUE_RENDER_COMMAND(FSkinWeightProfilesData_EnqueueDataReadback)(
				[DataPtr](FRHICommandListImmediate& RHICmdList)
		{
			if (DataPtr && DataPtr->BufferReadback.IsValid())
			{
				ensure(DataPtr->BufferReadback->IsReady());
				const void* BufferPtr = DataPtr->BufferReadback->Lock(DataPtr->ReadbackData.Num());
				FMemory::Memcpy(DataPtr->ReadbackData.GetData(), BufferPtr, DataPtr->ReadbackData.Num());
				DataPtr->BufferReadback->Unlock();

				DataPtr->ReadbackFinishedFrameIndex = GFrameNumberRenderThread;
			}
		});
	}
}

bool FSkinWeightProfilesData::IsDataReadbackPending() const
{
	return ReadbackData.ReadbackData.Num() > 0;
}

bool FSkinWeightProfilesData::IsDataReadbackFinished() const
{
	return !IsPendingReadback() && IsGPUReadbackFinished() && ReadbackData.ReadbackFinishedFrameIndex != INDEX_NONE && GFrameNumberRenderThread > ReadbackData.ReadbackFinishedFrameIndex;
}

void FSkinWeightProfilesData::ResetGPUReadback()
{
	ReadbackData.BufferReadback.Reset();
	ReadbackData.ReadbackData.Empty();
	ReadbackData.ReadbackFinishedFrameIndex = INDEX_NONE;
}

void FSkinWeightProfilesData::InitialiseProfileBuffer(const FName& ProfileName)
{
	LLM_SCOPE(ELLMTag::SkeletalMesh);

	if (ProfileNameToBuffer.Contains(ProfileName))
	{
		// Maybe it has already been previously set-up and not unloaded yet
		return;
	}

	if (BaseBuffer)
	{
		const uint8* BaseBufferData;
		
		if (BaseBuffer->GetNeedsCPUAccess())
		{
			BaseBufferData = BaseBuffer->GetDataVertexBuffer()->GetWeightData();
		}
		else
		{
			ensure(IsDataReadbackFinished());
			BaseBufferData = ReadbackData.ReadbackData.GetData();
		}
		
		if (ensure(BaseBufferData))
		{
			FSkinWeightVertexBuffer* OverrideBuffer = new FSkinWeightVertexBuffer();
			OverrideBuffer->CopyMetaData(*BaseBuffer);
			ProfileNameToBuffer.Add(ProfileName, OverrideBuffer);

			OverrideBuffer->SetNeedsCPUAccess(BaseBuffer->GetNeedsCPUAccess());
			
			const FRuntimeSkinWeightProfileData* ProfilePtr = OverrideData.Find(ProfileName);
			if (ProfilePtr)
			{
				ProfilePtr->ApplyOverrides(OverrideBuffer, BaseBufferData, BaseBuffer->GetNumVertices());
			}

#if RHI_ENABLE_RESOURCE_INFO
			const FName OwnerName = FName(ProfileName.ToString() + TEXT("_FSkinWeightProfilesData"));
			OverrideBuffer->SetOwnerName(OwnerName);
#endif
			OverrideBuffer->BeginInitResources();
		}	
	}
}

void FSkinWeightProfilesData::ApplyOverrideProfile(FSkinWeightVertexBuffer* OverrideBuffer, const FName& ProfileName)
{
	OverrideBuffer->CopyMetaData(*BaseBuffer);

	const FRuntimeSkinWeightProfileData* ProfilePtr = OverrideData.Find(ProfileName);
	if (ProfilePtr)
	{
		ProfilePtr->ApplyOverrides(OverrideBuffer, BaseBuffer->GetDataVertexBuffer()->GetWeightData(), BaseBuffer->GetNumVertices());
	}	
}

void FSkinWeightProfilesData::CreateRHIBuffers_RenderThread(TArray<TPair<FName, FSkinWeightRHIInfo>>& OutBuffers)
{
	CreateRHIBuffers(FRHICommandListImmediate::Get(), OutBuffers);
}

void FSkinWeightProfilesData::CreateRHIBuffers_Async(TArray<TPair<FName, FSkinWeightRHIInfo>>& OutBuffers)
{
	FRHIAsyncCommandList CommandList;
	CreateRHIBuffers(*CommandList, OutBuffers);
}

void FSkinWeightProfilesData::InitRHIForStreaming(const TArray<TPair<FName, FSkinWeightRHIInfo>>& IntermediateBuffers, FRHIResourceUpdateBatcher& Batcher)
{
	for (int32 Idx = 0; Idx < IntermediateBuffers.Num(); ++Idx)
	{
		const FName& ProfileName = IntermediateBuffers[Idx].Key;
		const FSkinWeightRHIInfo& IntermediateBuffer = IntermediateBuffers[Idx].Value;
		ProfileNameToBuffer.FindChecked(ProfileName)->InitRHIForStreaming(IntermediateBuffer, Batcher);
	}
}

void FSkinWeightProfilesData::ReleaseRHIForStreaming(FRHIResourceUpdateBatcher& Batcher)
{
	for (TMap<FName, FSkinWeightVertexBuffer*>::TIterator It(ProfileNameToBuffer); It; ++It)
	{
		It->Value->ReleaseRHIForStreaming(Batcher);
	}
}

void FRuntimeSkinWeightProfileData::ApplyOverrides(FSkinWeightVertexBuffer* OverrideBuffer, const uint8* DataBuffer, const int32 NumVerts) const
{
	if (DataBuffer)
	{
		if (OverrideBuffer)
		{
			OverrideBuffer->CopySkinWeightRawDataFromBuffer(DataBuffer, NumVerts);

			uint8* TargetSkinWeightData = OverrideBuffer->GetDataVertexBuffer()->GetWeightData();
			
			const uint8 VertexStride = OverrideBuffer->GetConstantInfluencesVertexStride();
			const uint8 WeightDataOffset = OverrideBuffer->GetBoneIndexByteSize() * OverrideBuffer->GetMaxBoneInfluences();

			// Apply overrides
			for (auto VertexIndexOverridePair : VertexIndexToInfluenceOffset)
			{
				const uint32 VertexIndex = VertexIndexOverridePair.Key;
				const uint32 InfluenceOffset = VertexIndexOverridePair.Value;
				
				uint8* BoneData = TargetSkinWeightData + (VertexIndex * VertexStride);
				uint8* WeightData = BoneData + WeightDataOffset;

#if !UE_BUILD_SHIPPING
				uint32 VertexOffset = 0;
				uint32 VertexInfluenceCount = 0;
				OverrideBuffer->GetVertexInfluenceOffsetCount(VertexIndex, VertexOffset, VertexInfluenceCount);
				check(NumWeightsPerVertex <= VertexInfluenceCount);
				check((void*)(((uint8*)TargetSkinWeightData) + VertexOffset) == (void*)BoneData);
				check(b16BitBoneIndices == OverrideBuffer->Use16BitBoneIndex());
#endif
				// BoneIDs either contains FBoneIndexType entries spanning (2) uint8 values, or single uint8 bone indices (1)
				const uint32 BoneIndexByteSize = OverrideBuffer->GetBoneIndexByteSize();
				const uint32 BoneWeightByteSize = OverrideBuffer->GetBoneWeightByteSize();
				FMemory::Memcpy(BoneData, &BoneIDs[InfluenceOffset * NumWeightsPerVertex * BoneIndexByteSize], BoneIndexByteSize * NumWeightsPerVertex);
				FMemory::Memcpy(WeightData, &BoneWeights[InfluenceOffset * NumWeightsPerVertex * BoneWeightByteSize], BoneWeightByteSize * NumWeightsPerVertex);
			}
		}
	}
}

void FRuntimeSkinWeightProfileData::ApplyDefaultOverride(FSkinWeightVertexBuffer* Buffer) const
{
	if (Buffer)
	{
		const int32 ExpectedNumVerts = Buffer->GetNumVertices();
		if (ExpectedNumVerts)
		{
			uint8* TargetSkinWeightData = (uint8*)Buffer->GetDataVertexBuffer()->GetWeightData();

			const uint8 VertexStride = Buffer->GetConstantInfluencesVertexStride();
			const uint8 WeightDataOffset = Buffer->GetBoneIndexByteSize() * Buffer->GetMaxBoneInfluences();

			for (auto VertexIndexOverridePair : VertexIndexToInfluenceOffset)
			{
				const uint32 VertexIndex = VertexIndexOverridePair.Key;
				const uint32 InfluenceOffset = VertexIndexOverridePair.Value;

				const uint8* BoneData = TargetSkinWeightData + (VertexIndex * VertexStride);
				const uint8* WeightData = BoneData + WeightDataOffset;

#if !UE_BUILD_SHIPPING
				uint32 VertexOffset = 0;
				uint32 VertexInfluenceCount = 0;
				Buffer->GetVertexInfluenceOffsetCount(VertexIndex, VertexOffset, VertexInfluenceCount);
				check(NumWeightsPerVertex <= VertexInfluenceCount);
				check((void*)(((uint8*)TargetSkinWeightData) + VertexOffset) == (void*)BoneData);
				check(b16BitBoneIndices == Buffer->Use16BitBoneIndex());
#endif
				// BoneIDs either contains FBoneIndexType entries spanning (2) uint8 values, or single uint8 bone indices (1)
				FMemory::Memcpy((void*)BoneData, &BoneIDs[InfluenceOffset * NumWeightsPerVertex * Buffer->GetBoneIndexByteSize()], Buffer->GetBoneIndexByteSize() * NumWeightsPerVertex);
				FMemory::Memcpy((void*)WeightData, &BoneWeights[InfluenceOffset * NumWeightsPerVertex], sizeof(uint8) * NumWeightsPerVertex);
			}
		}
	}
}

