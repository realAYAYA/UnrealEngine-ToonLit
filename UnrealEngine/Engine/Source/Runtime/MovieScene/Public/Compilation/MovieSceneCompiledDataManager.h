// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Compilation/MovieSceneCompiledDataID.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SparseArray.h"
#include "CoreTypes.h"
#include "Evaluation/MovieSceneEvaluationField.h"
#include "Evaluation/MovieSceneEvaluationTemplate.h"
#include "Evaluation/MovieSceneSequenceHierarchy.h"
#include "HAL/CriticalSection.h"
#include "Misc/AssertionMacros.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/FrameTime.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "MovieSceneFwd.h"
#include "UObject/Object.h"
#include "UObject/ObjectKey.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneCompiledDataManager.generated.h"

class UMovieSceneSequence;
class UMovieSceneSubSection;
class UMovieSceneSubTrack;
class UMovieSceneTrack;
struct FCompileOnTheFlyData;
struct FFrameNumber;
struct FGatherParameters;
struct FMovieSceneBinding;
struct FMovieSceneEvaluationOperand;
struct FMovieSceneGatheredCompilerData;
struct FMovieSceneSequenceID;
struct FTrackGatherParameters;
template<typename DataType> struct TMovieSceneEvaluationTreeDataIterator;

namespace UE
{
namespace MovieScene
{
	struct FSubSequencePath;
}
}

enum class EMovieSceneSequenceCompilerMask : uint8
{
	Hierarchy               = 1 << 0,
	EvaluationTemplate      = 1 << 1,
	EvaluationTemplateField = 1 << 2,
	EntityComponentField    = 1 << 3,

	None                    = 0,
};
ENUM_CLASS_FLAGS(EMovieSceneSequenceCompilerMask);

/** Flag struct necessary while flag enums are not supported on UPROPERTY */
USTRUCT()
struct FMovieSceneSequenceCompilerMaskStruct
{
	GENERATED_BODY()

	FMovieSceneSequenceCompilerMaskStruct()
		: bHierarchy(0)
		, bEvaluationTemplate(0)
		, bEvaluationTemplateField(0)
		, bEntityComponentField(0)
	{}

	FMovieSceneSequenceCompilerMaskStruct& operator=(EMovieSceneSequenceCompilerMask InMask)
	{
		bHierarchy               = EnumHasAnyFlags(InMask, EMovieSceneSequenceCompilerMask::Hierarchy);
		bEvaluationTemplate      = EnumHasAnyFlags(InMask, EMovieSceneSequenceCompilerMask::EvaluationTemplate);
		bEvaluationTemplateField = EnumHasAnyFlags(InMask, EMovieSceneSequenceCompilerMask::EvaluationTemplateField);
		bEntityComponentField    = EnumHasAnyFlags(InMask, EMovieSceneSequenceCompilerMask::EntityComponentField);
		return *this;
	}

	EMovieSceneSequenceCompilerMask AsEnum() const
	{
		EMovieSceneSequenceCompilerMask Enum = EMovieSceneSequenceCompilerMask::None;
		Enum |= bHierarchy               ? EMovieSceneSequenceCompilerMask::Hierarchy               : EMovieSceneSequenceCompilerMask::None;
		Enum |= bEvaluationTemplate      ? EMovieSceneSequenceCompilerMask::EvaluationTemplate      : EMovieSceneSequenceCompilerMask::None;
		Enum |= bEvaluationTemplateField ? EMovieSceneSequenceCompilerMask::EvaluationTemplateField : EMovieSceneSequenceCompilerMask::None;
		Enum |= bEntityComponentField    ? EMovieSceneSequenceCompilerMask::EntityComponentField    : EMovieSceneSequenceCompilerMask::None;
		return Enum;
	}

	UPROPERTY()
	uint8 bHierarchy : 1;
	UPROPERTY()
	uint8 bEvaluationTemplate : 1;
	UPROPERTY()
	uint8 bEvaluationTemplateField : 1;
	UPROPERTY()
	uint8 bEntityComponentField : 1;
};

/** Flags generated at compile time for a given sequence */
USTRUCT()
struct FMovieSceneCompiledSequenceFlagStruct
{
	GENERATED_BODY()

	FMovieSceneCompiledSequenceFlagStruct()
		: bParentSequenceRequiresLowerFence(0)
		, bParentSequenceRequiresUpperFence(0)
	{}

	/** True if this sequence should include a fence on the lower bound of any sub sequence's that include it */
	UPROPERTY()
	uint8 bParentSequenceRequiresLowerFence : 1;

	/** True if this sequence should include a fence on the upper bound of any sub sequence's that include it */
	UPROPERTY()
	uint8 bParentSequenceRequiresUpperFence : 1;
};


/** Used for serialization only */
UCLASS()
class UMovieSceneCompiledData : public UObject
{
public:
	GENERATED_BODY()

	UMovieSceneCompiledData();

	void Reset();

private:
	friend class UMovieSceneCompiledDataManager;

	/** 352 Bytes */
	UPROPERTY()
	FMovieSceneEvaluationTemplate EvaluationTemplate;

	/** 352 Bytes */
	UPROPERTY()
	FMovieSceneSequenceHierarchy Hierarchy;

	/** 272 Bytes */
	UPROPERTY()
	FMovieSceneEntityComponentField EntityComponentField;

	/** 64 Bytes */
	UPROPERTY()
	FMovieSceneEvaluationField TrackTemplateField;

	/** 16 Bytes */
	UPROPERTY()
	TArray<FFrameTime> DeterminismFences;

	/** 16 bytes */
	UPROPERTY()
	FGuid CompiledSignature;

	/** 16 Bytes */
	UPROPERTY()
	FGuid CompilerVersion;

	/** 1 Byte */
	UPROPERTY()
	FMovieSceneSequenceCompilerMaskStruct AccumulatedMask;

	/** 1 Byte */
	UPROPERTY()
	FMovieSceneSequenceCompilerMaskStruct AllocatedMask;

	/** 1 Byte */
	UPROPERTY()
	EMovieSceneSequenceFlags AccumulatedFlags;

	/** 1 Byte */
	FMovieSceneCompiledSequenceFlagStruct CompiledFlags;
};


struct FMovieSceneCompiledDataEntry
{
	FMovieSceneCompiledDataEntry();

	MOVIESCENE_API UMovieSceneSequence* GetSequence() const;

	/** 16 Bytes */
	FGuid CompiledSignature;

	/** 16 Bytes */
	TArray<FFrameTime> DeterminismFences;

	/** 8 Bytes */
	FObjectKey SequenceKey;

	/** 4 Bytes */
	FMovieSceneCompiledDataID DataID;

	/** 1 Byte */
	EMovieSceneSequenceFlags AccumulatedFlags;

	/** 1 Byte */
	EMovieSceneSequenceCompilerMask AllocatedMask;

	/** 1 Byte */
	EMovieSceneSequenceCompilerMask AccumulatedMask;

	/** 1 Byte */
	FMovieSceneCompiledSequenceFlagStruct CompiledFlags;
};

UCLASS()
class MOVIESCENE_API UMovieSceneCompiledDataManager
	: public UObject
{
public:
	GENERATED_BODY()

	UMovieSceneCompiledDataManager();

#if WITH_EDITOR
	static UMovieSceneCompiledDataManager* GetPrecompiledData(EMovieSceneServerClientMask EmulatedMask = EMovieSceneServerClientMask::All);
#else
	static UMovieSceneCompiledDataManager* GetPrecompiledData();
#endif

	UMovieSceneCompiledData* MakeCompiledData(UMovieSceneSequence* Sequence) const;

	void SetEmulatedNetworkMask(EMovieSceneServerClientMask NewMask);

	void Reset(UMovieSceneSequence* Sequence);

	void DestroyAllData();

	FMovieSceneCompiledDataID GetDataID(UMovieSceneSequence* Sequence);

	FMovieSceneCompiledDataID FindDataID(UMovieSceneSequence* Sequence) const;

	void DestroyTemplate(FMovieSceneCompiledDataID DataID);

	bool IsDirty(UMovieSceneSequence* Sequence) const;

	bool IsDirty(FMovieSceneCompiledDataID CompiledDataID) const;

	bool IsDirty(const FMovieSceneCompiledDataEntry& Entry) const;

	/**
	 * Return a reference to a compiled data entry.
	 * WARNING: This reference will become invalid if any sequence in this manager is (re)compiled
	 */
	const FMovieSceneCompiledDataEntry& GetEntryRef(FMovieSceneCompiledDataID DataID) const
	{
		check(CompiledDataEntries.IsValidIndex(DataID.Value));
		return CompiledDataEntries[DataID.Value];
	}

	FMovieSceneCompiledDataID GetSubDataID(FMovieSceneCompiledDataID DataID, FMovieSceneSequenceID SubSequenceID);

	uint32 GetReallocationVersion() const
	{
		return ReallocationVersion;
	}

	const FMovieSceneSequenceHierarchy*    FindHierarchy(FMovieSceneCompiledDataID DataID) const { return Hierarchies.Find(DataID.Value);       }
	const FMovieSceneEvaluationTemplate*   FindTrackTemplate(FMovieSceneCompiledDataID DataID) const { return TrackTemplates.Find(DataID.Value);         }
	const FMovieSceneEvaluationField*      FindTrackTemplateField(FMovieSceneCompiledDataID DataID) const { return TrackTemplateFields.Find(DataID.Value);    }
	const FMovieSceneEntityComponentField* FindEntityComponentField(FMovieSceneCompiledDataID DataID) const { return EntityComponentFields.Find(DataID.Value);  }

	const FMovieSceneSequenceHierarchy&    GetHierarchyChecked(FMovieSceneCompiledDataID DataID) const { return Hierarchies.FindChecked(DataID.Value);           }
	const FMovieSceneEvaluationTemplate&   GetTrackTemplateChecked(FMovieSceneCompiledDataID DataID) const { return TrackTemplates.FindChecked(DataID.Value);        }
	const FMovieSceneEvaluationField&      GetTrackTemplateFieldChecked(FMovieSceneCompiledDataID DataID) const { return TrackTemplateFields.FindChecked(DataID.Value);   }
	const FMovieSceneEntityComponentField& GetEntityComponentFieldChecked(FMovieSceneCompiledDataID DataID) const { return EntityComponentFields.FindChecked(DataID.Value); }

	void Compile(FMovieSceneCompiledDataID DataID);

	FMovieSceneCompiledDataID Compile(UMovieSceneSequence* Sequence);

	void Compile(FMovieSceneCompiledDataID DataID, UMovieSceneSequence* Sequence);

	static bool CompileHierarchy(UMovieSceneSequence* Sequence, FMovieSceneSequenceHierarchy* InOutHierarchy, EMovieSceneServerClientMask NetworkMask);

	void CopyCompiledData(UMovieSceneSequence* Sequence);
	void LoadCompiledData(UMovieSceneSequence* Sequence);

private:

	void Gather(const FMovieSceneCompiledDataEntry& Entry, UMovieSceneSequence* Sequence, const FTrackGatherParameters& Params, FMovieSceneGatheredCompilerData* OutCompilerData) const;

	void CompileTrack(FMovieSceneCompiledDataEntry* OutEntry, const FMovieSceneBinding* ObjectBinding, UMovieSceneTrack* Track, const FTrackGatherParameters& Params, TSet<FGuid>* OutCompiledSignatures, FMovieSceneGatheredCompilerData* OutCompilerData);

	void GatherTrack(const FMovieSceneBinding* ObjectBinding, UMovieSceneTrack* Track, const FTrackGatherParameters& Params, const FMovieSceneEvaluationTemplate* TrackTemplate, FMovieSceneGatheredCompilerData* OutCompilerData) const;

	void CompileSubSequences(const FMovieSceneSequenceHierarchy& Hierarchy, const FTrackGatherParameters& Params, FMovieSceneGatheredCompilerData* OutCompilerData);

	static bool CompileHierarchy(UMovieSceneSequence* Sequence, const FGatherParameters& Params, FMovieSceneSequenceHierarchy* InOutHierarchy);

	static bool CompileHierarchyImpl(UMovieSceneSequence* Sequence, const FGatherParameters& Params, const FMovieSceneEvaluationOperand& Operand, UE::MovieScene::FSubSequencePath* RootPath, FMovieSceneSequenceHierarchy* InOutHierarchy);

	static bool GenerateSubSequenceData(UMovieSceneSequence* SubSequence, const FGatherParameters& Params, const FMovieSceneEvaluationOperand& Operand, UE::MovieScene::FSubSequencePath* RootPath, FMovieSceneSequenceHierarchy* InOutHierarchy);
	static bool GenerateSubSequenceData(UMovieSceneSubTrack* SubTrack, const FGatherParameters& Params, const FMovieSceneEvaluationOperand& Operand, UE::MovieScene::FSubSequencePath* RootPath, FMovieSceneSequenceHierarchy* InOutHierarchy);

	static void PopulateSubSequenceTree(UMovieSceneSequence* SubSequence, const FGatherParameters& Params, UE::MovieScene::FSubSequencePath* RootPath, FMovieSceneSequenceHierarchy* InOutHierarchy);
	static void PopulateSubSequenceTree(UMovieSceneSubTrack* SubTrack, const FGatherParameters& Params, UE::MovieScene::FSubSequencePath* RootPath, FMovieSceneSequenceHierarchy* InOutHierarchy);

	static TOptional<FFrameNumber> GetLoopingSubSectionEndTime(const UMovieSceneSequence* InRootSequence, const UMovieSceneSubSection* SubSection, const FGatherParameters& Params);

	void CompileTrackTemplateField(FMovieSceneCompiledDataEntry* OutEntry, const FMovieSceneSequenceHierarchy& Hierarchy, FMovieSceneGatheredCompilerData* InCompilerData);

	void PopulateEvaluationGroup(const TArray<FCompileOnTheFlyData>& SortedCompileData, FMovieSceneEvaluationGroup* OutGroup);

	void PopulateMetaData(const FMovieSceneSequenceHierarchy& RootHierarchy, const TArray<FCompileOnTheFlyData>& SortedCompileData, TMovieSceneEvaluationTreeDataIterator<FMovieSceneSubSequenceTreeEntry> SubSequences, FMovieSceneEvaluationMetaData* OutMetaData);

	void ProcessTrack(FMovieSceneCompiledDataEntry* OutEntry, const FMovieSceneBinding* ObjectBinding, UMovieSceneTrack* Track, const FTrackGatherParameters& Params, FMovieSceneGatheredCompilerData* OutCompilerData, TSet<FGuid>* OutCompiledSignatures);

	void ProcessSubTrack(FMovieSceneCompiledDataEntry* OutEntry, UMovieSceneSubTrack* SubTrack, const FGuid& ObjectBindingId, const FTrackGatherParameters& Params, FMovieSceneGatheredCompilerData* OutCompilerData);

	void DestroyData(FMovieSceneCompiledDataID DataID);

private:

	FMovieSceneCompiledDataEntry* GetEntryPtr(FMovieSceneCompiledDataID DataID)
	{
		check(CompiledDataEntries.IsValidIndex(DataID.Value));
		return &CompiledDataEntries[DataID.Value];
	}

	void ConsoleVariableSink();

private:

	FCriticalSection AsyncLoadCriticalSection;

	friend struct FMovieSceneCompileDataManagerGenerator;

	TMap<FObjectKey, FMovieSceneCompiledDataID> SequenceToDataIDs;

	TSparseArray<FMovieSceneCompiledDataEntry>  CompiledDataEntries;

	UPROPERTY()
	TMap<int32, FMovieSceneSequenceHierarchy>    Hierarchies;

	UPROPERTY()
	TMap<int32, FMovieSceneEvaluationTemplate>   TrackTemplates;

	UPROPERTY()
	TMap<int32, FMovieSceneEvaluationField>      TrackTemplateFields;

	UPROPERTY()
	TMap<int32, FMovieSceneEntityComponentField> EntityComponentFields;

	FGuid CompilerVersion;

	uint32 ReallocationVersion;

	EMovieSceneServerClientMask NetworkMask;
};


