// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rendering/SkinWeightVertexBuffer.h"
#include "PerPlatformProperties.h"
#include "UObject/NameTypes.h"
#include "Misc/CoreStats.h"
#include "RenderingThread.h"
#include "HAL/UnrealMemory.h"
#include "BoneIndices.h"
#include "RHIGPUReadback.h"
#include "Templates/UniquePtr.h"

#include "SkinWeightProfile.generated.h"

class USkeletalMesh;
class UDebugSkelMeshComponent;

namespace SkeletalMeshImportData
{
	struct FVertInfluence;
}

extern ENGINE_API int32 GSkinWeightProfilesLoadByDefaultMode;
extern ENGINE_API int32 GSkinWeightProfilesDefaultLODOverride;
extern ENGINE_API int32 GSkinWeightProfilesAllowedFromLOD;

extern ENGINE_API FAutoConsoleVariableRef CVarSkinWeightsLoadByDefaultMode;
extern ENGINE_API FAutoConsoleVariableRef CVarSkinWeightProfilesDefaultLODOverride;
extern ENGINE_API FAutoConsoleVariableRef CVarSkinWeightProfilesAllowedFromLOD;

/** Structure storing user facing properties, and is used to identify profiles at the SkeletalMesh level*/
USTRUCT()
struct FSkinWeightProfileInfo
{
	GENERATED_BODY()

	/** Name of the Skin Weight Profile*/
	UPROPERTY(EditAnywhere, Category = SkinWeights)
	FName Name;
	
	/** Whether or not this Profile should be considered the Default loaded for specific LODs rather than the original Skin Weights of the Skeletal Mesh */
	UPROPERTY(EditAnywhere, Category = SkinWeights)
	FPerPlatformBool DefaultProfile;

	/** When DefaultProfile is set any LOD below this LOD Index will override the Skin Weights of the Skeletal Mesh with the Skin Weights from this Profile */
	UPROPERTY(EditAnywhere, Category = SkinWeights, meta=(EditCondition="DefaultProfile", ClampMin=0, DisplayName = "Default Profile from LOD Index"))
	FPerPlatformInt DefaultProfileFromLODIndex;
	
#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Category = SkinWeights)
	TMap<int32, FString> PerLODSourceFiles;
#endif
};

#if WITH_EDITORONLY_DATA

/** Editor only skin weight representation */
struct FRawSkinWeight
{
	// MAX_TOTAL_INFLUENCES for now
	FBoneIndexType InfluenceBones[MAX_TOTAL_INFLUENCES];
	uint8 InfluenceWeights[MAX_TOTAL_INFLUENCES];

	friend FArchive& operator<<(FArchive& Ar, FRawSkinWeight& OverrideEntry);
};

/** Editor only representation of a Skin Weight profile, stored as part of FSkeletalMeshLODModel, used as a base for generating the runtime version (FSkeletalRenderDataSkinWeightProfilesData) */
struct FImportedSkinWeightProfileData
{
	TArray<FRawSkinWeight> SkinWeights;

	//This is the result of the imported data before the chunking
	//We use this data every time we need to re-chunk the skeletal mesh
	TArray<SkeletalMeshImportData::FVertInfluence> SourceModelInfluences;

	friend FArchive& operator<<(FArchive& Ar, FImportedSkinWeightProfileData& ProfileData);
};

#endif // WITH_EDITORONLY_DATA

/** Runtime structure containing the set of override weights and the associated vertex indices */
struct FRuntimeSkinWeightProfileData
{
	/** Structure containing per Skin Weight offset and length */
	struct FSkinWeightOverrideInfo
	{
		/** Offset into FRuntimeSkinWeightOverrideData.Weights */
		uint32 InfluencesOffset;
#if WITH_EDITORONLY_DATA
		/** Number of influences to be read from FRuntimeSkinWeightOverrideData.Weights */
		uint8 NumInfluences_DEPRECATED;
#endif
		friend FArchive& operator<<(FArchive& Ar, FSkinWeightOverrideInfo& OverrideInfo);
	};

	void ApplyOverrides(FSkinWeightVertexBuffer* OverrideBuffer, const void* DataBuffer, const int32 NumVerts) const;	
	void ApplyDefaultOverride(FSkinWeightVertexBuffer* Buffer) const;

#if WITH_EDITORONLY_DATA
	/** Per skin weight offset into Weights array and number of weights stored */
	TArray<FSkinWeightOverrideInfo> OverridesInfo_DEPRECATED;
	/** Bulk data containing all Weights, stored as bone id in upper and weight in lower (8) bits */
	TArray<uint16> Weights_DEPRECATED;	
#endif 

	// Either contains FBoneIndexType or uint8 bone indices
	TArray<uint8> BoneIDs;
	TArray<uint8> BoneWeights;
	/** Map between Vertex Indices and the influence offset into BoneIDs/BoneWeights (DEPRECATED and entries of OverridesInfo) */
	TMap<uint32, uint32> VertexIndexToInfluenceOffset;

	uint8 NumWeightsPerVertex;
	bool b16BitBoneIndices;
	
	friend FArchive& operator<<(FArchive& Ar, FRuntimeSkinWeightProfileData& OverrideData);
};

struct FSkinweightReadbackData
{
	TUniquePtr<FRHIGPUBufferReadback> BufferReadback;
	TArray<uint8> ReadbackData;
	uint32 ReadbackFinishedFrameIndex;
};

/** Runtime structure for keeping track of skin weight profile(s) and the associated buffer */
struct ENGINE_API FSkinWeightProfilesData
{
	FSkinWeightProfilesData() : BaseBuffer(nullptr), DefaultOverrideSkinWeightBuffer(nullptr), bDefaultOverriden(false), bStaticOverriden(false), DefaultProfileName(NAME_None) {}
	void Init(FSkinWeightVertexBuffer* InBaseBuffer);

	~FSkinWeightProfilesData();
	
	DECLARE_DELEGATE_RetVal_ThreeParams(int32 /** Index into Profiles ArrayView */, FOnPickOverrideSkinWeightProfile, const USkeletalMesh* /** Skeletal Mesh to pick the profile for */, const TArrayView<const FSkinWeightProfileInfo> /** Available skin weight profiles to pick from */, int32 /** LOD Index */);
	static FOnPickOverrideSkinWeightProfile OnPickOverrideSkinWeightProfile;

#if !WITH_EDITOR
	// Mark this as non-editor only to prevent mishaps from users
	void OverrideBaseBufferSkinWeightData(USkeletalMesh* Mesh, int32 LODIndex);
#endif 
	void SetDynamicDefaultSkinWeightProfile(USkeletalMesh* Mesh, int32 LODIndex, bool bSerialization = false);	
	void ClearDynamicDefaultSkinWeightProfile(USkeletalMesh* Mesh, int32 LODIndex);
	void SetupDynamicDefaultSkinweightProfile();
	FSkinWeightVertexBuffer* GetDefaultOverrideBuffer() const { return DefaultOverrideSkinWeightBuffer; }

	bool ContainsProfile(const FName& ProfileName) const;
	FSkinWeightVertexBuffer* GetOverrideBuffer(const FName& ProfileName) const;
	bool ContainsOverrideBuffer(const FName& ProfileName) const;
	
#if WITH_EDITOR
	const FRuntimeSkinWeightProfileData* GetOverrideData(const FName& ProfileName) const;
	FRuntimeSkinWeightProfileData& AddOverrideData(const FName& ProfileName);
#endif // WITH_EDITOR
	
	void ReleaseBuffer(const FName& ProfileName, bool bForceRelease = false);
	void ReleaseResources();

	SIZE_T GetResourcesSize() const;
	SIZE_T GetCPUAccessMemoryOverhead() const;

	friend FArchive& operator<<(FArchive& Ar, FSkinWeightProfilesData& OverrideData);

	void SerializeMetaData(FArchive& Ar);

	void ReleaseCPUResources();

	void CreateRHIBuffers_RenderThread(TArray<TPair<FName, FSkinWeightRHIInfo>>& OutBuffers);
	void CreateRHIBuffers_Async(TArray<TPair<FName, FSkinWeightRHIInfo>>& OutBuffers);

	template <uint32 MaxNumUpdates>
	void InitRHIForStreaming(const TArray<TPair<FName, FSkinWeightRHIInfo>>& IntermediateBuffers, TRHIResourceUpdateBatcher<MaxNumUpdates>& Batcher)
	{
		for (int32 Idx = 0; Idx < IntermediateBuffers.Num(); ++Idx)
		{
			const FName& ProfileName = IntermediateBuffers[Idx].Key;
			const FSkinWeightRHIInfo& IntermediateBuffer = IntermediateBuffers[Idx].Value;
			ProfileNameToBuffer.FindChecked(ProfileName)->InitRHIForStreaming(IntermediateBuffer, Batcher);
		}
	}
	
	template <uint32 MaxNumUpdates>
	void ReleaseRHIForStreaming(TRHIResourceUpdateBatcher<MaxNumUpdates>& Batcher)
	{
		for (TMap<FName, FSkinWeightVertexBuffer*>::TIterator It(ProfileNameToBuffer); It; ++It)
		{
			It->Value->ReleaseRHIForStreaming(Batcher);
		}
	}

	bool IsPendingReadback() const;
	void EnqueueGPUReadback();
	bool IsGPUReadbackFinished() const;
	void EnqueueDataReadback();
	bool IsDataReadbackPending() const;
	bool IsDataReadbackFinished() const;
	void ResetGPUReadback();
	void InitialiseProfileBuffer(const FName& ProfileName);

protected:
	void ApplyOverrideProfile(FSkinWeightVertexBuffer* OverrideBuffer, const FName& ProfileName);

	template <bool bRenderThread>
	void CreateRHIBuffers_Internal(TArray<TPair<FName, FSkinWeightRHIInfo>>& OutBuffers);

	FSkinWeightVertexBuffer* BaseBuffer;
	FSkinWeightVertexBuffer* DefaultOverrideSkinWeightBuffer;

	TMap<FName, FSkinWeightVertexBuffer*> ProfileNameToBuffer;
	TMap<FName, FRuntimeSkinWeightProfileData> OverrideData;

	bool bDefaultOverriden;
	bool bStaticOverriden;
	FName DefaultProfileName;

protected:
	FSkinweightReadbackData ReadbackData;
};

