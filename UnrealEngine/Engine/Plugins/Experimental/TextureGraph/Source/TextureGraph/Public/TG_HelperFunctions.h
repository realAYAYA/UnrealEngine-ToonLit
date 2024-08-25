// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Job/JobBatch.h"
#include "TextureGraph.h"
#include "Export/TextureExporter.h"
#include "Data/Blob.h"
#include "TG_Graph.h"
#include "TG_Node.h"
#include "2D/TextureHelper.h"

class TEXTUREGRAPH_API FTG_HelperFunctions
{

public:
	static void	InitTargets(UTextureGraph* InTextureGraph);
	static TArray<BlobPtr> GetTexturedOutputs(const UTG_Node* Node, FTG_EvaluationContext* TextureConversionContext = nullptr);

	static void EnsureOutputIsTexture(MixUpdateCyclePtr Cycle, UTG_Node* OutputNode);

	static AsyncBool ExportAsync(UTextureGraph* InTextureGraph, FString ExportPath, FString AssetName, FExportSettings& TargetExportSettings, bool OverrideExportPath, bool OverwriteTextures = true,bool ExportAllOutputs = false);
	static AsyncBool RenderAsync(UTextureGraph* InTextureGraph, JobBatchPtr ExistingBatch = nullptr);


	
	template <typename T_Type>
	static TArray<T_Type> GetOutputsOfType(const UTG_Node* Node)
	{
		TArray<T_Type> Outputs;

		if (Node)
		{
			auto OutPinIds = Node->GetOutputPinIds();
			for (auto Id : OutPinIds)
			{
				//This is a work around for checking the type of the output
				//Probably we need to have a better solution for checking output type
				auto Pin = Node->GetGraph()->GetPin(Id);
				T_Type Output;
				Pin->GetValue(Output);

				Outputs.Add(Output);
			}
		}

		return Outputs;
	}
};

