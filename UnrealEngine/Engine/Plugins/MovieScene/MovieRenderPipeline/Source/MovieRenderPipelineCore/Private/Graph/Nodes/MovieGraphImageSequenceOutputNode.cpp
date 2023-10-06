// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/Nodes/MovieGraphImageSequenceOutputNode.h"

#include "Graph/Nodes/MovieGraphOutputSettingNode.h"
#include "Graph/Nodes/MovieGraphRenderLayerNode.h"
#include "Graph/MovieGraphDataTypes.h"
#include "Graph/MovieGraphPipeline.h"
#include "Graph/MovieGraphConfig.h"
#include "Graph/MovieGraphFilenameResolveParams.h"
#include "Graph/MovieGraphBlueprintLibrary.h"
#include "Modules/ModuleManager.h"
#include "ImageWriteQueue.h"
#include "Misc/Paths.h"
#include "Async/TaskGraphInterfaces.h"

UMovieGraphImageSequenceOutputNode::UMovieGraphImageSequenceOutputNode()
{
	ImageWriteQueue = &FModuleManager::Get().LoadModuleChecked<IImageWriteQueueModule>("ImageWriteQueue").GetWriteQueue();
}

void UMovieGraphImageSequenceOutputNode::OnAllFramesSubmittedImpl()
{
	FinalizeFence = ImageWriteQueue->CreateFence();
}

bool UMovieGraphImageSequenceOutputNode::IsFinishedWritingToDiskImpl() const
{
	// Wait until the finalization fence is reached meaning we've written everything to disk.
	return Super::IsFinishedWritingToDiskImpl() && (!FinalizeFence.IsValid() || FinalizeFence.WaitFor(0));
}

void UMovieGraphImageSequenceOutputNode::OnReceiveImageDataImpl(UMovieGraphPipeline* InPipeline, UE::MovieGraph::FMovieGraphOutputMergerFrame* InRawFrameData, const TSet<FMovieGraphRenderDataIdentifier>& InMask)
{
	check(InRawFrameData);

	// Gather the passes that need to be composited
	TArray<TPair<FMovieGraphRenderDataIdentifier, TUniquePtr<FImagePixelData>>> CompositingPasses;
	for (TPair<FMovieGraphRenderDataIdentifier, TUniquePtr<FImagePixelData>>& RenderData : InRawFrameData->ImageOutputData)
	{
		UE::MovieGraph::FMovieGraphSampleState* Payload = RenderData.Value->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();
		check(Payload);
		if (!Payload->bCompositeOnOtherRenders)
		{
			continue;
		}

		TPair<FMovieGraphRenderDataIdentifier, TUniquePtr<FImagePixelData>> CompositePass;
		CompositePass.Key = RenderData.Key;
		CompositePass.Value = RenderData.Value->CopyImageData();
		CompositingPasses.Add(MoveTemp(CompositePass));
	}

	// ToDo:
	// The ImageWriteQueue is set up in a fire-and-forget manner. This means that the data needs to be placed in the WriteQueue
	// as a TUniquePtr (so it can free the data when its done). Unfortunately we can have multiple output formats at once,
	// so we can't MoveTemp the data into it, we need to make a copy (though we could optimize for the common case where there is
	// only one output format).
	// Copying can be expensive (3ms @ 1080p, 12ms at 4k for a single layer image) so ideally we'd like to do it on the task graph
	// but this isn't really compatible with the ImageWriteQueue API as we need the future returned by the ImageWriteQueue to happen
	// in order, so that we push our futures to the main Movie Pipeline in order, otherwise when we encode files to videos they'll
	// end up with frames out of order. A workaround for this would be to chain all of the send-to-imagewritequeue tasks to each
	// other with dependencies, but I'm not sure that's going to scale to the potentialy high data volume going wide MRQ will eventually
	// need.

	// The base ImageSequenceOutputNode doesn't support any multilayer formats, so we write out each render pass separately.
	for (TPair<FMovieGraphRenderDataIdentifier, TUniquePtr<FImagePixelData>>& RenderData : InRawFrameData->ImageOutputData)
	{
		// If this pass is composited, skip it for now
		bool bSkip = false;
		for (TPair<FMovieGraphRenderDataIdentifier, TUniquePtr<FImagePixelData>>& CompositedPass : CompositingPasses)
		{
			if (CompositedPass.Key == RenderData.Key)
			{
				bSkip = true;
				break;
			}
		}

		if (bSkip)
		{
			continue;
		}
		
		// A layer within this output data may have chosen to not be written to disk by this CDO node
		if (!InMask.Contains(RenderData.Key))
		{
			continue;
		}

		UE::MovieGraph::FMovieGraphSampleState* Payload = RenderData.Value->GetPayload<UE::MovieGraph::FMovieGraphSampleState>();

		const bool bIncludeCDOs = true;
		UMovieGraphOutputSettingNode* OutputSettingNode = InRawFrameData->EvaluatedConfig->GetSettingForBranch<UMovieGraphOutputSettingNode>(RenderData.Key.RootBranchName, bIncludeCDOs);
		if (!ensure(OutputSettingNode))
		{
			continue;
		}

		FString RenderLayerName = RenderData.Key.RootBranchName.ToString();
		UMovieGraphRenderLayerNode* RenderLayerNode = InRawFrameData->EvaluatedConfig->GetSettingForBranch<UMovieGraphRenderLayerNode>(RenderData.Key.RootBranchName, bIncludeCDOs);
		if (RenderLayerNode)
		{
			RenderLayerName = RenderLayerNode->GetRenderLayerName();
		}
		// ToDo: Certain images may require transparency, at which point
		// we write out a .png instead of a .jpeg.
		EImageFormat PreferredOutputFormat = OutputFormat;

		const TCHAR* Extension = TEXT("");
		switch (PreferredOutputFormat)
		{
		case EImageFormat::PNG: Extension = TEXT("png"); break;
		case EImageFormat::JPEG: Extension = TEXT("jpeg"); break;
		case EImageFormat::BMP: Extension = TEXT("bmp"); break;
		case EImageFormat::EXR: Extension = TEXT("exr"); break;
		}

		// Generate one string that puts the directory combined with the filename format.
		FString FileNameFormatString = OutputSettingNode->OutputDirectory.Path / OutputSettingNode->FileNameFormat;

		// ToDo: Validate the string, ie: ensure it has {render_pass} in there somewhere there are multiple render passes
		// in the output data, include {camera_name} if there are multiple cameras for that render pass, etc. Validation
		// should insert {file_dup} tokens so it can put them at a logical place (ie: before frame numbers?)
		FileNameFormatString += TEXT(".{ext}");

		// Map the .ext to be specific to our output data.
		TMap<FString, FString> AdditionalFormatArgs;
		AdditionalFormatArgs.Add(TEXT("ext"), Extension);

		FMovieGraphFilenameResolveParams Params = FMovieGraphFilenameResolveParams();
		Params.RenderDataIdentifier = RenderData.Key;
		//Params.RootFrameNumber = Payload->TraversalContext.Time.RootFrameNumber;
		//Params.ShotFrameNumber = Payload->TraversalContext.Time.ShotFrameNumber;
		Params.RootFrameNumberRel = Payload->TraversalContext.Time.OutputFrameNumber;
		//Params.ShotFrameNumberRel = Payload->TraversalCOntext.Time.ShotFrameNumberRel
		//Params.FileMetadata = ToDo: Track File Metadata
		Params.Version = 1; // ToDo: Track versions
		Params.ZeroPadFrameNumberCount = OutputSettingNode->ZeroPadFrameNumbers;
		Params.FrameNumberOffset = OutputSettingNode->FrameNumberOffset;

		// If time dilation is in effect, RootFrameNumber and ShotFrameNumber will contain duplicates and the files will overwrite each other, 
		// so we force them into relative mode and then warn users we did that (as their numbers will jump from say 1001 -> 0000).
		bool bForceRelativeFrameNumbers = true; // TODO: Use relative frame numbers until we track Root vs. Shot frame numbers. (Previously false);
		//if (FileNameFormatString.Contains(TEXT("{frame")) Payload->TraversalContext.Time.IsTimeDilated() && !FileNameFormatString.Contains(TEXT("_rel}")))
		//{
		//	UE_LOG(LogMovieRenderPipeline, Warning, TEXT("Time Dilation was used but output format does not use relative time, forcing relative numbers. Change {frame_number} to {frame_number_rel} (or shot version) to remove this message."));
		//	bForceRelativeFrameNumbers = true;
		//}
		Params.bForceRelativeFrameNumbers = bForceRelativeFrameNumbers;
		Params.bEnsureAbsolutePath = true;
		Params.FileNameFormatOverrides = AdditionalFormatArgs;
		Params.InitializationTime = InPipeline->GetInitializationTime();
		Params.Job = InPipeline->GetCurrentJob();
		Params.EvaluatedConfig = InRawFrameData->EvaluatedConfig.Get();

		// Take our string path from the Output Setting and resolve it.
		FMovieGraphResolveArgs FinalResolvedKVPs;
		const FString FileName = UMovieGraphBlueprintLibrary::ResolveFilenameFormatArguments(FileNameFormatString, Params, FinalResolvedKVPs);

		TUniquePtr<FImageWriteTask> TileImageTask = MakeUnique<FImageWriteTask>();
		TileImageTask->Format = OutputFormat;
		TileImageTask->CompressionQuality = 100;
		TileImageTask->Filename = FileName;
		TileImageTask->PixelData = RenderData.Value->CopyImageData();

		EImagePixelType PixelType = TileImageTask->PixelData->GetType();

		// Perform compositing if any compositing passes were found earlier
		for (TPair<FMovieGraphRenderDataIdentifier, TUniquePtr<FImagePixelData>>& CompositedPass : CompositingPasses)
		{
			// This compositing pass will only composite on top of renders w/ the same branch and camera
			const FMovieGraphRenderDataIdentifier& Id = CompositedPass.Key;
			if ((Id.CameraName != RenderData.Key.CameraName) || (Id.RootBranchName != RenderData.Key.RootBranchName))
			{
				continue;
			}

			// There could be multiple renders within this branch using the composited pass, so we have to copy the image data
			switch (PixelType)
			{
			case EImagePixelType::Color:
				TileImageTask->PixelPreProcessors.Add(TAsyncCompositeImage<FColor>(CompositedPass.Value->CopyImageData()));
				break;
			case EImagePixelType::Float16:
				TileImageTask->PixelPreProcessors.Add(TAsyncCompositeImage<FFloat16Color>(CompositedPass.Value->CopyImageData()));
				break;
			case EImagePixelType::Float32:
				TileImageTask->PixelPreProcessors.Add(TAsyncCompositeImage<FLinearColor>(CompositedPass.Value->CopyImageData()));
				break;
			}
		}

		UE::MovieGraph::FMovieGraphOutputFutureData OutputData;
		OutputData.Shot = nullptr;
		OutputData.FilePath = FileName;
		OutputData.DataIdentifier = RenderData.Key;

		TFuture<bool> Future = ImageWriteQueue->Enqueue(MoveTemp(TileImageTask));

		InPipeline->AddOutputFuture(MoveTemp(Future), OutputData);
	}

}