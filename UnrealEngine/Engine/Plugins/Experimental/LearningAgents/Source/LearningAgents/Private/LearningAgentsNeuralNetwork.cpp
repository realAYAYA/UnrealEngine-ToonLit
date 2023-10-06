// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsNeuralNetwork.h"

#include "LearningNeuralNetwork.h"
#include "LearningLog.h"

#include "UObject/Package.h"
#include "Misc/FileHelper.h"

ULearningAgentsNeuralNetwork::ULearningAgentsNeuralNetwork() = default;
ULearningAgentsNeuralNetwork::ULearningAgentsNeuralNetwork(FVTableHelper& Helper) : Super(Helper) {}
ULearningAgentsNeuralNetwork::~ULearningAgentsNeuralNetwork() = default;

namespace UE::Learning::Agents
{
	ELearningAgentsActivationFunction GetLearningAgentsActivationFunction(const EActivationFunction ActivationFunction)
	{
		switch (ActivationFunction)
		{
		case EActivationFunction::ReLU: return ELearningAgentsActivationFunction::ReLU;
		case EActivationFunction::TanH: return ELearningAgentsActivationFunction::TanH;
		case EActivationFunction::ELU: return ELearningAgentsActivationFunction::ELU;
		default:UE_LOG(LogLearning, Error, TEXT("Unknown Activation Function.")); return ELearningAgentsActivationFunction::ELU;
		}
	}

	EActivationFunction GetActivationFunction(const ELearningAgentsActivationFunction ActivationFunction)
	{
		switch (ActivationFunction)
		{
		case ELearningAgentsActivationFunction::ReLU: return EActivationFunction::ReLU;
		case ELearningAgentsActivationFunction::TanH: return EActivationFunction::TanH;
		case ELearningAgentsActivationFunction::ELU: return EActivationFunction::ELU;
		default:UE_LOG(LogLearning, Error, TEXT("Unknown Activation Function.")); return EActivationFunction::ELU;
		}
	}
}

namespace UE::Learning::Agents::NeuralNetwork::Private
{
	static constexpr int32 MagicNumber = 0x0cd353cf;
	static constexpr int32 VersionNumber = 1;
}

void ULearningAgentsNeuralNetwork::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		bool bValid;
		Ar << bValid;
		if (bValid)
		{
			int32 VersionNumber;
			Ar << VersionNumber;

			if (VersionNumber != UE::Learning::Agents::NeuralNetwork::Private::VersionNumber)
			{
				UE_LOG(LogLearning, Warning, TEXT("%s: Unsupported Version Number %i."), *GetName(), VersionNumber);
			}

			NeuralNetwork = MakeShared<UE::Learning::FNeuralNetwork>();
			TArray<uint8> Bytes;
			Ar << Bytes;
			int32 Offset = 0;
			NeuralNetwork->DeserializeFromBytes(Offset, Bytes);
			UE_LEARNING_CHECK(Offset == Bytes.Num());
		}
		else
		{
			NeuralNetwork.Reset();
		}
	}
	else if (Ar.IsSaving())
	{
		bool bValid = NeuralNetwork.IsValid();
		Ar << bValid;
		if (bValid)
		{
			int32 VersionNumber = UE::Learning::Agents::NeuralNetwork::Private::VersionNumber;
			Ar << VersionNumber;
			TArray<uint8> Bytes;
			Bytes.SetNumUninitialized(UE::Learning::FNeuralNetwork::GetSerializationByteNum(
				NeuralNetwork->GetInputNum(),
				NeuralNetwork->GetOutputNum(), 
				NeuralNetwork->GetHiddenNum(),
				NeuralNetwork->GetLayerNum()));
			int32 Offset = 0;
			NeuralNetwork->SerializeToBytes(Offset, Bytes);
			UE_LEARNING_CHECK(Offset == Bytes.Num());
			Ar << Bytes;
		}
	}
}

void ULearningAgentsNeuralNetwork::ResetNetwork()
{
	NeuralNetwork.Reset();
	ForceMarkDirty();
}

void ULearningAgentsNeuralNetwork::LoadNetworkFromSnapshot(const FFilePath& File)
{
	TArray<uint8> RecordingData;

	if (FFileHelper::LoadFileToArray(RecordingData, *File.FilePath))
	{
		if (RecordingData.Num() < sizeof(int32) * 2)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network. Incorrect Format."), *GetName());
			return;
		}

		int32 Offset = 0;

		int32 MagicNumber;
		UE::Learning::DeserializeFromBytes(Offset, RecordingData, MagicNumber);

		if (MagicNumber != UE::Learning::Agents::NeuralNetwork::Private::MagicNumber)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network. Incorrect Magic Number."), *GetName());
			return;
		}

		int32 VersionNumber;
		UE::Learning::DeserializeFromBytes(Offset, RecordingData, VersionNumber);

		if (VersionNumber != UE::Learning::Agents::NeuralNetwork::Private::VersionNumber)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network. Unsupported Version Number %i."), *GetName(), VersionNumber);
			return;
		}

		TSharedPtr<UE::Learning::FNeuralNetwork> TempNeuralNetwork = MakeShared<UE::Learning::FNeuralNetwork>();
		TempNeuralNetwork->DeserializeFromBytes(Offset, RecordingData);

		if (Offset != RecordingData.Num())
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network. Unexpected end of file."));
			return;
		}

		if (NeuralNetwork)
		{
			// If we already have a neural network check settings match

			if (TempNeuralNetwork->GetInputNum() != NeuralNetwork->GetInputNum() ||
				TempNeuralNetwork->GetOutputNum() != NeuralNetwork->GetOutputNum() ||
				TempNeuralNetwork->GetLayerNum() != NeuralNetwork->GetLayerNum() ||
				TempNeuralNetwork->ActivationFunction != NeuralNetwork->ActivationFunction)
			{
				UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network from snapshot as settings don't match."), *GetName());
				return;
			}
			else
			{
				*NeuralNetwork = *TempNeuralNetwork;
			}
		}
		else
		{
			// Otherwise use loaded neural network as-is

			NeuralNetwork = TempNeuralNetwork;
		}

		ForceMarkDirty();
	}
	else
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network. File not found: \"%s\""), *GetName(), *File.FilePath);
	}
}

void ULearningAgentsNeuralNetwork::SaveNetworkToSnapshot(const FFilePath& File)
{
	TArray<uint8> RecordingData;

	int32 TotalByteNum =
		sizeof(int32) + // Magic Num
		sizeof(int32) + // Version Num
		NeuralNetwork->GetSerializationByteNum(
			NeuralNetwork->GetInputNum(), 
			NeuralNetwork->GetOutputNum(), 
			NeuralNetwork->GetHiddenNum(), 
			NeuralNetwork->GetLayerNum());

	RecordingData.SetNumUninitialized(TotalByteNum);

	int32 Offset = 0;
	UE::Learning::SerializeToBytes(Offset, RecordingData, UE::Learning::Agents::NeuralNetwork::Private::MagicNumber);
	UE::Learning::SerializeToBytes(Offset, RecordingData, UE::Learning::Agents::NeuralNetwork::Private::VersionNumber);
	NeuralNetwork->SerializeToBytes(Offset, RecordingData);
	UE_LEARNING_CHECK(Offset == RecordingData.Num());

	if (!FFileHelper::SaveArrayToFile(RecordingData, *File.FilePath))
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Failed to save network to file: \"%s\""), *GetName(), *File.FilePath);
	}
}

void ULearningAgentsNeuralNetwork::LoadNetworkFromAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!NeuralNetworkAsset || !NeuralNetworkAsset->NeuralNetwork)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is invalid."), *GetName());
		return;
	}

	if (NeuralNetworkAsset == this)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is same as the current network."), *GetName());
		return;
	}

	if (NeuralNetwork)
	{
		if (NeuralNetworkAsset->NeuralNetwork->GetInputNum() != NeuralNetwork->GetInputNum() ||
			NeuralNetworkAsset->NeuralNetwork->GetOutputNum() != NeuralNetwork->GetOutputNum() ||
			NeuralNetworkAsset->NeuralNetwork->GetLayerNum() != NeuralNetwork->GetLayerNum() ||
			NeuralNetworkAsset->NeuralNetwork->ActivationFunction != NeuralNetwork->ActivationFunction)
		{
			UE_LOG(LogLearning, Error, TEXT("%s: Failed to load network from asset as settings don't match."), *GetName());
			return;
		}
	}
	else
	{
		NeuralNetwork = MakeShared<UE::Learning::FNeuralNetwork>();
	}

	*NeuralNetwork = *NeuralNetworkAsset->NeuralNetwork;
	ForceMarkDirty();
}

void ULearningAgentsNeuralNetwork::SaveNetworkToAsset(ULearningAgentsNeuralNetwork* NeuralNetworkAsset)
{
	if (!NeuralNetworkAsset)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is invalid."), *GetName());
		return;
	}

	if (NeuralNetworkAsset == this)
	{
		UE_LOG(LogLearning, Error, TEXT("%s: Asset is same as the current network."), *GetName());
		return;
	}

	if (!NeuralNetworkAsset->NeuralNetwork)
	{
		NeuralNetworkAsset->NeuralNetwork = MakeShared<UE::Learning::FNeuralNetwork>();
	}

	*NeuralNetworkAsset->NeuralNetwork = *NeuralNetwork;
	NeuralNetworkAsset->ForceMarkDirty();
}

void ULearningAgentsNeuralNetwork::ForceMarkDirty()
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