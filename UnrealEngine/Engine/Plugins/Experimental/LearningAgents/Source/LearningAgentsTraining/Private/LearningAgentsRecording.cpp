// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsRecording.h"

#include "LearningLog.h"

#include "UObject/Package.h"
#include "Misc/FileHelper.h"

ULearningAgentsRecording::ULearningAgentsRecording() = default;
ULearningAgentsRecording::ULearningAgentsRecording(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsRecording::~ULearningAgentsRecording() = default;

namespace UE::Learning::Agents::Recording::Private
{
	static constexpr int32 MagicNumber = 0x06b5fb26;
	static constexpr int32 VersionNumber = 1;
}

void ULearningAgentsRecording::ResetRecording()
{
	Records.Empty();
	ForceMarkDirty();
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
			UE::Learning::DeserializeFromBytes(Offset, RecordingData, Records[RecordIdx].StepNum);
			UE::Learning::DeserializeFromBytes(Offset, RecordingData, Records[RecordIdx].ObservationDimNum);
			UE::Learning::DeserializeFromBytes(Offset, RecordingData, Records[RecordIdx].ActionDimNum);
			UE::Learning::DeserializeFromBytes(Offset, RecordingData, Records[RecordIdx].ObservationCompatibilityHash);
			UE::Learning::DeserializeFromBytes(Offset, RecordingData, Records[RecordIdx].ActionCompatibilityHash);
			UE::Learning::Array::DeserializeFromBytes<2, float>(Offset, RecordingData, Records[RecordIdx].ObservationData);
			UE::Learning::Array::DeserializeFromBytes<2, float>(Offset, RecordingData, Records[RecordIdx].ActionData);
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
			sizeof(int32) + // StepNum
			sizeof(int32) + // ObservationDimNum
			sizeof(int32) + // ActionDimNum
			sizeof(int32) + // ObservationCompatibilityHash
			sizeof(int32) + // ActionCompatibilityHash
			UE::Learning::Array::SerializationByteNum<2, float>({ Records[RecordIdx].StepNum, Records[RecordIdx].ObservationDimNum }) + // Observations
			UE::Learning::Array::SerializationByteNum<2, float>({ Records[RecordIdx].StepNum, Records[RecordIdx].ActionDimNum });	   // Actions
	}

	RecordingData.SetNumUninitialized(TotalByteNum);

	int32 Offset = 0;
	UE::Learning::SerializeToBytes(Offset, RecordingData, UE::Learning::Agents::Recording::Private::MagicNumber);
	UE::Learning::SerializeToBytes(Offset, RecordingData, UE::Learning::Agents::Recording::Private::VersionNumber);
	UE::Learning::SerializeToBytes(Offset, RecordingData, Records.Num());

	for (int32 RecordIdx = 0; RecordIdx < Records.Num(); RecordIdx++)
	{
		UE::Learning::SerializeToBytes(Offset, RecordingData, Records[RecordIdx].StepNum);
		UE::Learning::SerializeToBytes(Offset, RecordingData, Records[RecordIdx].ObservationDimNum);
		UE::Learning::SerializeToBytes(Offset, RecordingData, Records[RecordIdx].ActionDimNum);
		UE::Learning::SerializeToBytes(Offset, RecordingData, Records[RecordIdx].ObservationCompatibilityHash);
		UE::Learning::SerializeToBytes(Offset, RecordingData, Records[RecordIdx].ActionCompatibilityHash);
		UE::Learning::Array::SerializeToBytes<2, float>(Offset, RecordingData, { Records[RecordIdx].StepNum, Records[RecordIdx].ObservationDimNum }, Records[RecordIdx].ObservationData);
		UE::Learning::Array::SerializeToBytes<2, float>(Offset, RecordingData, { Records[RecordIdx].StepNum, Records[RecordIdx].ActionDimNum }, Records[RecordIdx].ActionData);
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

int32 ULearningAgentsRecording::GetRecordNum() const
{
	return Records.Num();
}

int32 ULearningAgentsRecording::GetRecordStepNum(const int32 Record) const
{
	if (Record < 0 || Record >= Records.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Record out of range. Asked for record %i but recording only has %i records."), *GetName(), Record, Records.Num());
		return 0;
	}

	return Records[Record].StepNum;
}


void ULearningAgentsRecording::GetObservationVector(TArray<float>& OutObservationVector, int32& OutObservationCompatibilityHash, const int32 Record, const int32 Step)
{
	if (Record < 0 || Record >= Records.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Record out of range. Asked for record %i but recording only has %i records."), *GetName(), Record, Records.Num());
		OutObservationVector.Empty();
		OutObservationCompatibilityHash = 0;
		return;
	}

	if (Step < 0 || Step >= Records[Record].StepNum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Step out of range. Asked for step %i but recording only has %i steps."), *GetName(), Step, Records[Record].StepNum);
		OutObservationVector.Empty();
		OutObservationCompatibilityHash = 0;
		return;
	}

	OutObservationVector = MakeArrayView(Records[Record].ObservationData).Slice(Records[Record].ObservationDimNum * Step, Records[Record].ObservationDimNum);
	OutObservationCompatibilityHash = Records[Record].ObservationCompatibilityHash;
}

void ULearningAgentsRecording::GetActionVector(TArray<float>& OutActionVector, int32& OutActionCompatibilityHash, const int32 Record, const int32 Step)
{
	if (Record < 0 || Record >= Records.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Record out of range. Asked for record %i but recording only has %i records."), *GetName(), Record, Records.Num());
		OutActionVector.Empty();
		OutActionCompatibilityHash = 0;
		return;
	}

	if (Step < 0 || Step >= Records[Record].StepNum)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Step out of range. Asked for step %i but recording only has %i steps."), *GetName(), Step, Records[Record].StepNum);
		OutActionVector.Empty();
		OutActionCompatibilityHash = 0;
		return;
	}

	OutActionVector = MakeArrayView(Records[Record].ActionData).Slice(Records[Record].ActionDimNum * Step, Records[Record].ActionDimNum);
	OutActionCompatibilityHash = Records[Record].ActionCompatibilityHash;
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