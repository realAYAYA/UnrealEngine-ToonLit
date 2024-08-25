// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphOutputMerger.h"
#include "Graph/MovieGraphPipeline.h"
#include "Graph/MovieGraphRenderDataIdentifier.h"
#include "Async/Async.h"

namespace UE::MovieGraph
{
	FMovieGraphOutputMerger::FMovieGraphOutputMerger(UMovieGraphPipeline* InOwningMoviePipeline)
		: WeakMoviePipeline(MakeWeakObjectPtr(InOwningMoviePipeline))
	{
	}

	void FMovieGraphOutputMerger::AbandonOutstandingWork()
	{
		FScopeLock ScopeLock(&PendingDataMutex);
		FinishedFrames.Empty();
		PendingData.Empty();
	}

	FMovieGraphOutputMergerFrame& FMovieGraphOutputMerger::AllocateNewOutputFrame_GameThread(const int32 InRenderedFrameNumber)
	{
		FScopeLock ScopeLock(&PendingDataMutex);

		// Ensure this frame hasn't already been entered somehow.
		check(!PendingData.Find(InRenderedFrameNumber));

		FMovieGraphOutputMergerFrame& NewFrame = PendingData.Add(InRenderedFrameNumber);
		return NewFrame;
	}

	FMovieGraphOutputMergerFrame& FMovieGraphOutputMerger::GetOutputFrame_GameThread(const int32 InRenderedFrameNumber)
	{
		check(PendingData.Find(InRenderedFrameNumber));
		return *PendingData.Find(InRenderedFrameNumber);
	}

	
	void FMovieGraphOutputMerger::OnSingleSampleDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData)
	{
		// This is to support outputting individual samples (skipping accumulation) for debug reasons,
		// or because you want to post-process them yourself. We just forward this directly on for output to disk.

		TWeakObjectPtr<UMovieGraphPipeline> LocalWeakPipeline = WeakMoviePipeline;

		AsyncTask(ENamedThreads::GameThread, [LocalData = MoveTemp(InData), LocalWeakPipeline]() mutable
		{
			if (ensureAlwaysMsgf(LocalWeakPipeline.IsValid(), TEXT("A memory lifespan issue has left an output builder alive without an owning Movie Pipeline.")))
			{
				// LocalWeakPipeline->Debug_OnSampleRendered(MoveTemp(LocalData));
				// ToDo: Read the finished frames from MRQ
				// also implement the above function.
			}
		}
		);
	}
	
	void FMovieGraphOutputMerger::OnCompleteRenderPassDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData)
	{	
		// Lock the ActiveData when we're updating what data has been gathered.
		FScopeLock ScopeLock(&PendingDataMutex);
		
		// Fetch our payload from the data. If this check fails then you didn't attach the payload to the image.
		FMovieGraphSampleState* Payload = InData->GetPayload<FMovieGraphSampleState>();
		check(Payload);
		
		const int32 RenderedFrameNumber = Payload->TraversalContext.Time.RenderedFrameNumber;

		// See if we can find the frame this data is for. This should always be valid, if it's not
		// valid it means they either forgot to declare they were going to produce it, or this is
		// coming in after the system already thinks it's finished that frame.
		FMovieGraphOutputMergerFrame* OutputFrame = PendingData.Find(RenderedFrameNumber);
		
		// Make sure we expected this frame number
		if (!ensureAlwaysMsgf(OutputFrame, TEXT("Received data for unknown frame. Frame was either already processed or not queued yet!")))
		{
			return;
		}
		
		// Make sure this render pass identifier was expected as well.
		FMovieGraphRenderDataIdentifier NewLayerId = Payload->TraversalContext.RenderDataIdentifier;
		if (!ensureAlwaysMsgf(OutputFrame->ExpectedRenderPasses.Contains(NewLayerId), TEXT("Received data for unexpected render pass: %s"), *LexToString(NewLayerId)))
		{
			return;
		}
		
		// Put the new data inside this output frame.
		OutputFrame->ImageOutputData.FindOrAdd(NewLayerId) = MoveTemp(InData);
		
		// Check to see if this was the last piece of data needed for this frame.
		int32 TotalPasses = OutputFrame->ExpectedRenderPasses.Num();
		int32 FinishedPasses = OutputFrame->ImageOutputData.Num();
		
		if (FinishedPasses == TotalPasses)
		{
			// Sort the output frames. This is only really important for multi-channel formats like EXR, but it lets passes
			// specify which one should be the thumbnail/default rgba channels instead of a first-come-first-serve.
			OutputFrame->ImageOutputData.ValueStableSort([](const TUniquePtr<FImagePixelData>& First, const TUniquePtr<FImagePixelData>& Second) -> bool
				{
					FMovieGraphSampleState* FirstPayload = First->GetPayload<FMovieGraphSampleState>();
					FMovieGraphSampleState* SecondPayload = Second->GetPayload<FMovieGraphSampleState>();

					return FirstPayload->CompositingSortOrder < SecondPayload->CompositingSortOrder;
				}
			);
			// Move this frame into our FinishedFrames array so the Game Thread can read it at its leisure
			FMovieGraphOutputMergerFrame FinalFrame;
			ensureMsgf(PendingData.RemoveAndCopyValue(RenderedFrameNumber, FinalFrame), TEXT("Could not find frame in pending data, output will be skipped!"));
			
			// TQueue is thread safe so it's okay to just push the data into it.
			FinishedFrames.Enqueue(MoveTemp(FinalFrame));
		}
	}
}		
