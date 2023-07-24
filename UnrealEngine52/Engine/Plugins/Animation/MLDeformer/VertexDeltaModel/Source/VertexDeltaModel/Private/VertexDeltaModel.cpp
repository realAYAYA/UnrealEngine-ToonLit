// Copyright Epic Games, Inc. All Rights Reserved.

#include "VertexDeltaModel.h"
#include "VertexDeltaModelVizSettings.h"
#include "VertexDeltaModelInstance.h"
#include "MLDeformerComponent.h"
#include "MLDeformerAsset.h"
#include "NeuralNetwork.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "ShaderCore.h"

#define LOCTEXT_NAMESPACE "VertexDeltaModel"

VERTEXDELTAMODEL_API DEFINE_LOG_CATEGORY(LogVertexDeltaModel)

// Implement the module.
namespace UE::VertexDeltaModel
{
	class VERTEXDELTAMODEL_API FVertexDeltaModelModule
		: public IModuleInterface
	{
		void StartupModule() override
		{
			// Register an additional shader path for our shaders used inside the deformer graph system.
			const FString PluginShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("VertexDeltaModel"))->GetBaseDir(), TEXT("Shaders"));
			AddShaderSourceDirectoryMapping(TEXT("/Plugin/VertexDeltaModel"), PluginShaderDir);
		}
	};
}
IMPLEMENT_MODULE(UE::VertexDeltaModel::FVertexDeltaModelModule, VertexDeltaModel)

UVertexDeltaModel::UVertexDeltaModel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	SetVizSettings(ObjectInitializer.CreateEditorOnlyDefaultSubobject<UVertexDeltaModelVizSettings>(this, TEXT("VizSettings")));
#endif
}

UMLDeformerModelInstance* UVertexDeltaModel::CreateModelInstance(UMLDeformerComponent* Component)
{
	return NewObject<UVertexDeltaModelInstance>(Component);
}

FString UVertexDeltaModel::GetDefaultDeformerGraphAssetPath() const 
{ 
	return FString(TEXT("/VertexDeltaModel/Deformers/DG_VertexDeltaModel.DG_VertexDeltaModel"));
}

void UVertexDeltaModel::PostLoad()
{
	Super::PostLoad();

	if (NNINetwork)
	{
		NNINetwork->SetDeviceType(ENeuralDeviceType::GPU, ENeuralDeviceType::CPU, ENeuralDeviceType::GPU);
		if (NNINetwork->GetDeviceType() != ENeuralDeviceType::GPU || NNINetwork->GetOutputDeviceType() != ENeuralDeviceType::GPU || NNINetwork->GetInputDeviceType() != ENeuralDeviceType::CPU)
		{
			UE_LOG(LogVertexDeltaModel, Error, TEXT("Neural net in MLD Vertex Delta Model '%s' cannot run on the GPU, it will not be active."), *GetDeformerAsset()->GetName());
		}
	}
}

void UVertexDeltaModel::SetNNINetwork(UNeuralNetwork* InNeuralNetwork)
{
	GetNeuralNetworkModifyDelegate().Broadcast();
	NNINetwork = InNeuralNetwork;
}

UNeuralNetwork* UVertexDeltaModel::GetNNINetwork() const
{
	return NNINetwork.Get();
}

#if WITH_EDITOR
	void UVertexDeltaModel::UpdateMemoryUsage()
	{
		Super::UpdateMemoryUsage();

		// Check if the neural network is on the GPU, if so, count the memory to the GPU and remove it from Main memory, as we added it to 
		// that already when we did the Model->GetResourceSizeBytes.
		if (NNINetwork)
		{
			if (NNINetwork->GetDeviceType() == ENeuralDeviceType::GPU)
			{
				const uint64 NeuralNetSize = static_cast<uint64>(NNINetwork->GetResourceSizeBytes(EResourceSizeMode::Type::EstimatedTotal));
				GPUMemUsageInBytes += NeuralNetSize;
				MemUsageInBytes -= NeuralNetSize;
				CookedMemUsageInBytes -= NeuralNetSize;
			}
		}
	}
#endif

#undef LOCTEXT_NAMESPACE
