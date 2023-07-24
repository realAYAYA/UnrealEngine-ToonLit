// Copyright Epic Games, Inc. All Rights Reserved.

#include "VertexDeltaEditorModel.h"
#include "VertexDeltaModel.h"
#include "VertexDeltaModelVizSettings.h"
#include "VertexDeltaTrainingModel.h"
#include "MLDeformerAsset.h"
#include "NeuralNetwork.h"

#define LOCTEXT_NAMESPACE "VertexDeltaEditorModel"

namespace UE::VertexDeltaModel
{
	using namespace UE::MLDeformer;

	FMLDeformerEditorModel* FVertexDeltaEditorModel::MakeInstance()
	{
		return new FVertexDeltaEditorModel();
	}

	UVertexDeltaModel* FVertexDeltaEditorModel::GetVertexDeltaModel() const
	{
		return Cast<UVertexDeltaModel>(Model);
	}

	ETrainingResult FVertexDeltaEditorModel::Train()
	{
		return TrainModel<UVertexDeltaTrainingModel>(this);
	}

	UNeuralNetwork* FVertexDeltaEditorModel::LoadNeuralNetworkFromOnnx(const FString& Filename) const
	{
		const FString OnnxFile = FPaths::ConvertRelativePathToFull(Filename);
		if (FPaths::FileExists(OnnxFile))
		{
			UE_LOG(LogVertexDeltaModel, Display, TEXT("Loading Onnx file '%s'..."), *OnnxFile);
			UNeuralNetwork* Result = NewObject<UNeuralNetwork>(Model, UNeuralNetwork::StaticClass());		
			if (Result->Load(OnnxFile))
			{
				Result->SetDeviceType(ENeuralDeviceType::GPU, ENeuralDeviceType::CPU, ENeuralDeviceType::GPU);	
				if (Result->GetDeviceType() != ENeuralDeviceType::GPU || Result->GetOutputDeviceType() != ENeuralDeviceType::GPU || Result->GetInputDeviceType() != ENeuralDeviceType::CPU)
				{
					UE_LOG(LogVertexDeltaModel, Error, TEXT("Neural net in ML Deformer '%s' cannot run on the GPU, it will not be active."), *Model->GetDeformerAsset()->GetName());
				}
				UE_LOG(LogVertexDeltaModel, Display, TEXT("Successfully loaded Onnx file '%s'..."), *OnnxFile);
				return Result;
			}
			else
			{
				UE_LOG(LogVertexDeltaModel, Error, TEXT("Failed to load Onnx file '%s'"), *OnnxFile);
			}
		}
		else
		{
			UE_LOG(LogVertexDeltaModel, Error, TEXT("Onnx file '%s' does not exist!"), *OnnxFile);
		}

		return nullptr;
	}

	bool FVertexDeltaEditorModel::LoadTrainedNetwork() const
	{
		const FString OnnxFile = GetTrainedNetworkOnnxFile();
		UNeuralNetwork* Network = LoadNeuralNetworkFromOnnx(OnnxFile);
		if (Network)
		{
			GetVertexDeltaModel()->SetNNINetwork(Network);
			return true;
		}

		return false;
	}

	bool FVertexDeltaEditorModel::IsTrained() const
	{
		return (GetVertexDeltaModel()->GetNNINetwork() != nullptr);
	}

	FString FVertexDeltaEditorModel::GetHeatMapDeformerGraphPath() const
	{
		return FString(TEXT("/VertexDeltaModel/Deformers/DG_VertexDeltaModel_HeatMap.DG_VertexDeltaModel_HeatMap"));
	}
}	// namespace UE::VertexDeltaModel

#undef LOCTEXT_NAMESPACE
