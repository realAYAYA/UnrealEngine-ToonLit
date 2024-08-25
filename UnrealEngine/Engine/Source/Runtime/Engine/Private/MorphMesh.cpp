// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MorphMesh.cpp: Unreal morph target mesh and blending implementation.
=============================================================================*/

#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Serialization/MemoryArchive.h"

//////////////////////////////////////////////////////////////////////////

UMorphTarget::UMorphTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}


void UMorphTarget::Serialize( FArchive& Ar )
{
	LLM_SCOPE(ELLMTag::Animation);
	
	Super::Serialize( Ar );
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);

	FStripDataFlags StripFlags( Ar );
	if( !StripFlags.IsAudioVisualDataStripped() )
	{
		Ar << MorphLODModels;
	}
}

#if WITH_EDITORONLY_DATA
void UMorphTarget::DeclareCustomVersions(FArchive& Ar, const UClass* SpecificSubclass)
{
	Super::DeclareCustomVersions(Ar, SpecificSubclass);
	FMorphTargetLODModel MorphLODModel;
	Ar << MorphLODModel;
}
#endif

namespace
{
	void SerializeMorphLODModels(FMemoryArchive& Ar, TArray<FMorphTargetLODModel>& MorphLODModels)
	{
		int32 MorphLODModelNumber = 0;
		if (Ar.IsLoading())
		{
			Ar << MorphLODModelNumber;
			MorphLODModels.Empty(MorphLODModelNumber);
			MorphLODModels.AddDefaulted(MorphLODModelNumber);
		}
		else
		{
			MorphLODModelNumber = MorphLODModels.Num();
			Ar << MorphLODModelNumber;
		}

		for (int32 MorphIndex = 0; MorphIndex < MorphLODModelNumber; ++MorphIndex)
		{
			Ar << MorphLODModels[MorphIndex];
		}
	}
}

void UMorphTarget::SerializeMemoryArchive(FMemoryArchive & Ar)
{
	FName MorphTargetName = GetFName();
	Ar << MorphTargetName;
	SerializeMorphLODModels(Ar, MorphLODModels);
}

void UMorphTarget::PostLoad()
{
	Super::PostLoad();
	
#if WITH_EDITOR
	if (GetLinkerCustomVersion(FEditorObjectVersion::GUID) < FEditorObjectVersion::AddedMorphTargetSectionIndices &&
		BaseSkelMesh)
	{
		const int32 MaxLOD = FMath::Min(BaseSkelMesh->GetImportedModel()->LODModels.Num(), MorphLODModels.Num());
		for (int32 LODIndex = 0; LODIndex < MaxLOD; ++LODIndex)
		{
			FMorphTargetLODModel& MorphLODModel = MorphLODModels[LODIndex];
			MorphLODModel.SectionIndices.Empty();
			const FSkeletalMeshLODModel& LODModel = BaseSkelMesh->GetImportedModel()->LODModels[LODIndex];
			TArray<int32> BaseIndexes;
			TArray<int32> LastIndexes;
			for (int32 SectionIdx = 0; SectionIdx < LODModel.Sections.Num(); ++SectionIdx)
			{
				const int32 BaseVertexBufferIndex = LODModel.Sections[SectionIdx].GetVertexBufferIndex();
				BaseIndexes.Add(BaseVertexBufferIndex);
				LastIndexes.Add(BaseVertexBufferIndex + LODModel.Sections[SectionIdx].GetNumVertices());
			}
			// brute force
			for (int32 VertIndex = 0; VertIndex < MorphLODModel.Vertices.Num() && MorphLODModel.SectionIndices.Num() < BaseIndexes.Num(); ++VertIndex)
			{
				int32 SourceVertexIdx = MorphLODModel.Vertices[VertIndex].SourceIdx;
				for (int32 SectionIdx = 0; SectionIdx < BaseIndexes.Num(); ++SectionIdx)
				{
					if (!MorphLODModel.SectionIndices.Contains(SectionIdx))
					{
						if (BaseIndexes[SectionIdx] <= SourceVertexIdx && SourceVertexIdx < LastIndexes[SectionIdx])
						{
							MorphLODModel.SectionIndices.AddUnique(SectionIdx);
							break;
						}
					}
				}
			}
		}
	}
#endif //#if WITH_EDITOR
}

#if WITH_EDITOR

void FFinishBuildMorphTargetData::LoadFromMemoryArchive(FMemoryArchive & Ar)
{
	check(Ar.IsLoading());
	
	if (!ensureMsgf(!bApplyMorphTargetsData, TEXT("Error in FFinishBuildMorphTargetData::LoadFromMemoryArchive. The compilation context morph targets data was already set.")))
	{
		MorphLODModelsPerTargetName.Empty();
		bApplyMorphTargetsData = false;
	}
	bApplyMorphTargetsData = true;
	
	int32 MorphTargetNumber = 0;
	Ar << MorphTargetNumber;
	MorphLODModelsPerTargetName.Reserve(MorphTargetNumber);
	for (int32 MorphTargetIndex = 0; MorphTargetIndex < MorphTargetNumber; ++MorphTargetIndex)
	{
		FName MorphTargetName = NAME_None;
		Ar << MorphTargetName;
		TArray<FMorphTargetLODModel>&MorphLODModels = MorphLODModelsPerTargetName.FindOrAdd(MorphTargetName);
		SerializeMorphLODModels(Ar, MorphLODModels);
	}
}

#endif // WITH_EDITOR
