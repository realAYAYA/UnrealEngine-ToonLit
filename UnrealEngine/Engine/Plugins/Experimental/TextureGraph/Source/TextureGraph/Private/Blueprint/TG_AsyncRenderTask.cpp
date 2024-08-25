// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprint/TG_AsyncRenderTask.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "TG_Graph.h"
#include "TG_Node.h"
#include "Expressions/TG_Expression.h"
#include "2D/Tex.h"
#include "Expressions/Output/TG_Expression_Output.h"
#include "UObject/Package.h"
#include "Device/FX/DeviceBuffer_FX.h"
#include "Model/Mix/MixSettings.h"

UTG_AsyncRenderTask::UTG_AsyncRenderTask(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

UTG_AsyncRenderTask* UTG_AsyncRenderTask::TG_AsyncRenderTask( UTextureGraph* InTextureGraph)
{
	UTG_AsyncRenderTask* Task = NewObject<UTG_AsyncRenderTask>();

	if (InTextureGraph != nullptr)
	{
		Task->OriginalTextureGraphPtr = InTextureGraph;
		Task->TextureGraphPtr = (UTextureGraph*)StaticDuplicateObject(Task->OriginalTextureGraphPtr, GetTransientPackage(), NAME_None, ~RF_Standalone, UTextureGraph::StaticClass());
		FTG_HelperFunctions::InitTargets(Task->TextureGraphPtr);
		Task->RegisterWithTGAsyncTaskManger();
	}
	
	return Task;
}

void UTG_AsyncRenderTask::Activate()
{
	// Start the async task on a new thread
	Super::Activate();
	UE_LOG(LogTextureGraph, Log, TEXT("UTG_AsyncRenderTask:: Activate"));

	if (TextureGraphPtr == nullptr)
	{
		UE_LOG(LogTextureGraph, Warning, TEXT("UTG_AsyncRenderTask::Cannot render Texture Graph not selected"));
		return;
	}

	OutputBlobs.Empty();
	OutputRts.Empty();

	FTG_HelperFunctions::RenderAsync(TextureGraphPtr)
	.then([this](bool bRenderResult) mutable
	{
		TextureGraphPtr->Graph()->ForEachNodes([this](const UTG_Node* node, uint32 index)
		{
			if (node && node->GetExpression()->IsA(UTG_Expression_Output::StaticClass()))
			{
				auto outputs = FTG_HelperFunctions::GetTexturedOutputs(node);

				if (outputs.Num() > 0)
				{
					OutputBlobs.Add(outputs[0]);
				}
			}
		});

		return FinalizeAllOutputBlobs();
	})
	.then([this](bool bFinalized)
	{
		return GetRenderTextures();
	})
	.then([this](bool bRtResult)
	{
		OnDone.Broadcast(OutputRts);
		SetReadyToDestroy();
		return bRtResult;
	});
}

AsyncBool UTG_AsyncRenderTask::FinalizeAllOutputBlobs()
{
	std::vector<AsyncBlobResultPtr> promises;
	for (const auto& Blob : OutputBlobs)
	{
		auto TiledOutput = std::static_pointer_cast<TiledBlob>(Blob);
		promises.push_back(TiledOutput->OnFinalise());
	}

	return cti::when_all(promises.begin(), promises.end()).then([=](std::vector<const Blob*> results) mutable
	{
		return true;
	});
}

AsyncBool UTG_AsyncRenderTask::GetRenderTextures()
{
	std::vector<AsyncBufferResultPtr> promises;
	for (const auto& Blob : OutputBlobs)
	{
		auto TiledOutput = std::static_pointer_cast<TiledBlob>(Blob);
		promises.push_back(TiledOutput->CombineTiles(false, false));
	}

	return cti::when_all(promises.begin(), promises.end()).then([this](std::vector<BufferResultPtr> results) mutable
	{
		for (const auto& Blob : OutputBlobs)
		{
			auto TiledOutput = std::static_pointer_cast<TiledBlob>(Blob);
			auto FXBuffer = std::static_pointer_cast<DeviceBuffer_FX>(TiledOutput->GetBufferRef().GetPtr());
			auto Rt = FXBuffer->GetTexture()->GetRenderTarget();
			OutputRts.Add(Rt);
		}
		return true;
	});
}

void UTG_AsyncRenderTask::FinishDestroy()
{
	if (TextureGraphPtr != nullptr)
	{
		TextureGraphPtr->GetSettings()->FreeTargets();
		TextureGraphPtr = nullptr;
		OriginalTextureGraphPtr = nullptr;
	}
	Super::FinishDestroy();
}