// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsRecording.h"

#include "LearningLog.h"

#include "UObject/Package.h"
#include "Misc/FileHelper.h"

bool FLearningAgentsRecord::Serialize(FArchive& Ar)
{
	Ar << SampleNum;
	Ar << ObservationDimNum;
	Ar << ActionDimNum;
	UE::Learning::Array::Serialize(Ar, Observations);
	UE::Learning::Array::Serialize(Ar, Actions);
	return true;
}

ULearningAgentsRecording::ULearningAgentsRecording() = default;
ULearningAgentsRecording::ULearningAgentsRecording(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsRecording::~ULearningAgentsRecording() = default;

namespace UE::Learning::Agents::Recording::Private
{
	static constexpr int32 MagicNumber = 0x06b5fb26;
	static constexpr int32 VersionNumber = 1;
}

void ULearningAgentsRecording::LoadRecordingFromFile(const FFilePath& File)
{
	TArray<uint8> RecordingData;

	if (FFileHelper::LoadFileToArray(RecordingData, *File.FilePath))
	{
		if (RecordingData.Num() < sizeof(int32) * 3)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network. Incorrect Format."), *GetName());
			return;
		}

		int32 Offset = 0;

		int32 MagicNumber;
		UE::Learning::DeserializeFromBytes(Offset, RecordingData, MagicNumber);

		if (MagicNumber != UE::Learning::Agents::Recording::Private::MagicNumber)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Failed to load recording. Incorrect Magic Number."), *GetName());
			return;
		}

		int32 VersionNumber;
		UE::Learning::DeserializeFromBytes(Offset, RecordingData, VersionNumber);

		if (VersionNumber != UE::Learning::Agents::Recording::Private::VersionNumber)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Failed to load recording. Unsupported Version Number %i."), *GetName(), VersionNumber);
			return;
		}

		int32 RecordNum;
		UE::Learning::DeserializeFromBytes(Offset, RecordingData, RecordNum);

		Records.SetNum(RecordNum);

		for (int32 RecordIdx = 0; RecordIdx < RecordNum; RecordIdx++)
		{
			UE::Learning::DeserializeFromBytes(Offset, RecordingData, Records[RecordIdx].SampleNum);
			UE::Learning::DeserializeFromBytes(Offset, RecordingData, Records[RecordIdx].ObservationDimNum);
			UE::Learning::DeserializeFromBytes(Offset, RecordingData, Records[RecordIdx].ActionDimNum);
			UE::Learning::Array::DeserializeFromBytes(Offset, RecordingData, Records[RecordIdx].Observations);
			UE::Learning::Array::DeserializeFromBytes(Offset, RecordingData, Records[RecordIdx].Actions);
		}

		UE_LEARNING_CHECK(Offset == RecordingData.Num());

		ForceMarkDirty();
	}
	else
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Failed to load recording. File not found: \"%s\""), *GetName(), *File.FilePath);
	}
}

void ULearningAgentsRecording::SaveRecordingToFile(const FFilePath& File) const
{
	TArray<uint8> RecordingData;

	int32 TotalByteNum = 
		sizeof(int32) + // Magic Num
		sizeof(int32) + // Version Num
		sizeof(int32);  // Record Num

	for (int32 RecordIdx = 0; RecordIdx < Records.Num(); RecordIdx++)
	{
		TotalByteNum +=
			sizeof(int32) + // SampleNum
			sizeof(int32) + // ObservationDimNum
			sizeof(int32) + // ActionDimNum
			UE::Learning::Array::SerializationByteNum<2, float>(Records[RecordIdx].Observations.Shape()) + // Observations
			UE::Learning::Array::SerializationByteNum<2, float>(Records[RecordIdx].Actions.Shape()); // Actions
	}

	RecordingData.SetNumUninitialized(TotalByteNum);

	int32 Offset = 0;
	UE::Learning::SerializeToBytes(Offset, RecordingData, UE::Learning::Agents::Recording::Private::MagicNumber);
	UE::Learning::SerializeToBytes(Offset, RecordingData, UE::Learning::Agents::Recording::Private::VersionNumber);
	UE::Learning::SerializeToBytes(Offset, RecordingData, Records.Num());

	for (int32 RecordIdx = 0; RecordIdx < Records.Num(); RecordIdx++)
	{
		UE::Learning::SerializeToBytes(Offset, RecordingData, Records[RecordIdx].SampleNum);
		UE::Learning::SerializeToBytes(Offset, RecordingData, Records[RecordIdx].ObservationDimNum);
		UE::Learning::SerializeToBytes(Offset, RecordingData, Records[RecordIdx].ActionDimNum);
		UE::Learning::Array::SerializeToBytes(Offset, RecordingData, Records[RecordIdx].Observations);
		UE::Learning::Array::SerializeToBytes(Offset, RecordingData, Records[RecordIdx].Actions);
	}

	UE_LEARNING_CHECK(Offset == RecordingData.Num());

	if (!FFileHelper::SaveArrayToFile(RecordingData, *File.FilePath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Failed to save recording to file: \"%s\""), *GetName(), *File.FilePath);
	}
}

void ULearningAgentsRecording::AppendRecordingFromFile(const FFilePath& File)
{
	ULearningAgentsRecording* TempRecording = NewObject<ULearningAgentsRecording>(this);
	TempRecording->LoadRecordingFromFile(File);

	Records.Append(TempRecording->Records);
	ForceMarkDirty();
}

void ULearningAgentsRecording::LoadRecordingFromAsset(ULearningAgentsRecording* RecordingAsset)
{
	if (!RecordingAsset)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is nullptr."), *GetName());
		return;
	}

	if (RecordingAsset == this)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is same as the current recording."), *GetName());
		return;
	}

	Records = RecordingAsset->Records;
	ForceMarkDirty();
}

void ULearningAgentsRecording::SaveRecordingToAsset(ULearningAgentsRecording* RecordingAsset)
{
	if (!RecordingAsset)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is nullptr."), *GetName());
		return;
	}

	if (RecordingAsset == this)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is same as the current recording."), *GetName());
		return;
	}

	RecordingAsset->Records = Records;
	RecordingAsset->ForceMarkDirty();
}

void ULearningAgentsRecording::AppendRecordingToAsset(ULearningAgentsRecording* RecordingAsset)
{
	if (!RecordingAsset)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is invalid."), *GetName());
		return;
	}

	if (RecordingAsset == this)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is same as the current recording."), *GetName());
		return;
	}

	RecordingAsset->Records.Append(Records);
	RecordingAsset->ForceMarkDirty();
}

void ULearningAgentsRecording::ForceMarkDirty()
{
	// Manually mark the package as dirty since just using `Modify` prevents 
	// marking packages as dirty during PIE which is most likely when this
	// is being used.
	if (UPackage* Package = GetPackage())
	{
		const bool bIsDirty = Package->IsDirty();

		if (!bIsDirty)
		{
			Package->SetDirtyFlag(true);
		}

		Package->PackageMarkedDirtyEvent.Broadcast(Package, bIsDirty);
	}
}