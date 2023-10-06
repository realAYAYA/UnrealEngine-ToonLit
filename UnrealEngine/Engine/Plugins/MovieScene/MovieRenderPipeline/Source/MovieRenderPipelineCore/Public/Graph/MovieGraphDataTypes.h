// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/MovieGraphTraversalContext.h"
#include "Graph/MovieGraphRenderDataIdentifier.h"
#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"
#include "Templates/SubclassOf.h"
#include "ImagePixelData.h"
#include "Containers/Queue.h"
#include "Misc/FrameRate.h"
#include "MovieGraphDataTypes.generated.h"

// Forward Declares
class UMovieGraphPipeline;
class UMovieGraphEvaluatedConfig;
class UMovieGraphTimeStepBase;
class UMovieGraphRendererBase;
class UMovieGraphTimeRangeBuilderBase;
class UMovieGraphDataCachingBase;
class UMoviePipelineExecutorJob;
class UMoviePipelineExecutorShot;
struct FImagePixelData;

namespace UE::MovieGraph
{
	struct FRenderTimeStatistics;
}

USTRUCT(BlueprintType)
struct MOVIERENDERPIPELINECORE_API FMovieGraphInitConfig
{
	GENERATED_BODY()
	
	FMovieGraphInitConfig();
	
	/** 
	* Which class should the UMovieGraphPipeline use to handle calculating per frame 
	* timesteps? Defaults to UMovieGraphLinearTimeStep.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	TSubclassOf<UMovieGraphTimeStepBase> TimeStepClass;
	
	/**
	* Which class should the UMovieGraphPipeline use to look for render layers and
	* request renders from. Defaults to UMovieGraphDefaultRenderer.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	TSubclassOf<UMovieGraphRendererBase> RendererClass;

	/**
	* Which class should the UMovieGraphPipeline use to build time ranges from, and
	* during evaluation, send callbacks about the time actually evaluated so you
	* can sync with an external source. Defaults to UMovieGraphSequenceDataSource.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	TSubclassOf<UMovieGraphDataSourceBase> DataSourceClass;

	/**
	* Should the UMovieGraphPipeline render the full player viewport? Defaults
	* to false (so no 3d content is rendered) so we can display the UMG widgets
	* and MRQ rendering always happens in an off-screen render target.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	bool bRenderViewport;
};

USTRUCT(BlueprintType)
struct FMovieGraphRenderPassOutputData
{
	GENERATED_BODY()
public:
	/** A list of file paths on disk (in order) that were generated for this particular render pass. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movie Pipeline")
	TArray<FString> FilePaths;
};

USTRUCT(BlueprintType)
struct MOVIERENDERPIPELINECORE_API FMovieGraphRenderOutputData
{
	GENERATED_BODY()
public:
	/** Which shot is this output data for. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movie Graph")
	TWeakObjectPtr<UMoviePipelineExecutorShot> Shot;

	/**
	* A mapping between render passes (such as "beauty") and an array containing the files written for that shot.
	* Will be multiple files if using image sequences
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movie Graph")
	TMap<FMovieGraphRenderDataIdentifier, FMovieGraphRenderPassOutputData> RenderPassData;
};


UCLASS(BlueprintType, Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphTimeStepBase : public UObject
{
	GENERATED_BODY()
public:
	/** Called each frame while the Movie Graph Pipeline is in a producing frames state. */
	virtual void TickProducingFrames() {}

	/** Called when the Movie Graph Pipeline is shutting down, use this to restore any changes. */
	virtual void Shutdown() {}

	/** 
	* TickProducingFrames will be called for a frame (before the frame starts) and then this will be
	* called at the end of the frame when we kick off the renders for the frame. Should return data
	* needed to calculate the correct rendering timestep.
	*/
	virtual FMovieGraphTimeStepData GetCalculatedTimeData() const { return FMovieGraphTimeStepData(); }

	UMovieGraphPipeline* GetOwningGraph() const;
};

UCLASS(BlueprintType, Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphRendererBase : public UObject
{
	GENERATED_BODY()
public:
	virtual void Render(const FMovieGraphTimeStepData& InTimeData) {}
	virtual void SetupRenderingPipelineForShot(UMoviePipelineExecutorShot* InShot) {}
	virtual void TeardownRenderingPipelineForShot(UMoviePipelineExecutorShot* InShot) {}
	virtual UE::MovieGraph::FRenderTimeStatistics* GetRenderTimeStatistics(const int32 InFrameNumber) { return nullptr; }
	
	UMovieGraphPipeline* GetOwningGraph() const;
};

/**
* Movie Graph Pipeline is mostly interested in knowing about ranges of time that
* it should render, and less concerned with the specifics of where that data comes
* from (ie: a Level Sequence). This lets you synchronize with a different data source
* to provide the ranges of time to render, and then the UMovieGraphTimeStepBase class
* figures out how to move around within that time step, before calling some functions
* to synchronize your external data source to actually match the evaluated time.
*/
UCLASS(BlueprintType, Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphDataSourceBase : public UObject
{
	GENERATED_BODY()
public:
	/** An internal, high resolution framerate that seeks, etc. will be returned in. (ie: 24,000/1) */
	virtual FFrameRate GetTickResolution() const { return FFrameRate(); }

	/** A lower res, human readable Frame Rate. We convert to the Tick Resolution internally. (ie: 24/1) */
	virtual FFrameRate GetDisplayRate() const { return FFrameRate(); }

	/** Called by the Time Step system when it wants the external data source to update. Time is in TickResolution scale. */
	virtual void SyncDataSourceTime(const FFrameTime& InTime) {}

	virtual void BuildTimeRanges() { }
	/** 
	* Called when the Movie Graph Pipeline starts before anything has happened, allowing you to 
	* cache your datasource before making any modifications to it as a result of rendering.
	*/
	virtual void CacheDataPreJob(const FMovieGraphInitConfig& InInitConfig) {}
	virtual void RestoreCachedDataPostJob() {}
	virtual void UpdateShotList() {}
	virtual void InitializeShot(UMoviePipelineExecutorShot* InShot) {}
	UMovieGraphPipeline* GetOwningGraph() const;
};

// ToDo: Both of these can probably go into the Default Renderer implementation.
struct FMovieGraphRenderPassLayerData
{
	FName BranchName;
	FGuid CameraIdentifier;
	TWeakObjectPtr<class UMovieGraphRenderPassNode> RenderPassNode;
};
// ToDo: Both of these can probably go into the Default Renderer implementation.

struct FMovieGraphRenderPassSetupData
{
	TWeakObjectPtr<class UMovieGraphDefaultRenderer> Renderer;
	TArray<FMovieGraphRenderPassLayerData> Layers;
};


namespace UE::MovieGraph
{
	/** Supplemental data to store with a file future so we can retrieve information about what the future was doing once it finishes writing to disk. */
	struct FMovieGraphOutputFutureData
	{
		FMovieGraphOutputFutureData()
			: Shot(nullptr)
		{}

		/** Which shot is this for? */
		class UMoviePipelineExecutorShot* Shot;

		/** What was the filepath on disk that was generated by the future? */
		FString FilePath;

		/** Which render pass is this for? */
		FMovieGraphRenderDataIdentifier DataIdentifier;
	};
	
	/**
	* Statistics tracked for a given frame during rendering, such as when the
	* render started and when it ended. This can be added as metadata during
	* file output to include useful information. 
	*
	* ToDo: This could potentially be tracked as just regular filemetadata
	* with a rendered sample but requires strong typing support and metadata
	*  merging support.
	*/
	struct FRenderTimeStatistics
	{
		/** Time in UTC that the frame was first started. */
		FDateTime StartTime;

		/** Time in UTC that the frame was fully rendered and merged together. */
		FDateTime EndTime;
	};

	struct FMovieGraphSampleState : IImagePixelDataPayload, public TSharedFromThis<FMovieGraphSampleState>
	{
		FMovieGraphSampleState()
			: bWriteSampleToDisk(false)
			, bRequiresAccumulator(false)
			, bFetchFromAccumulator(false)
			, bCompositeOnOtherRenders(false)
		{}

		/** The traversal context used to read graph values at the time of submission. */
		FMovieGraphTraversalContext TraversalContext;

		/** The backbuffer resolution of this render resource (ie: the resolution the sample was rendered at). */
		FIntPoint BackbufferResolution;

		/** 
		* The resolution the accumulation buffer is accumulating at. This may be different than our 
		* backbuffer resolution (due to tiling), and our final output resolution (due to overscan + crop). 
		*/
		FIntPoint AccumulatorResolution;

		/** Debug feature, should this sample be written to disk, not being accumulated? */
		bool bWriteSampleToDisk;

		/** Set this to true for every sample if an accumulator is required (ie: has temporal/spatial sub-samples, or high res tiling). */
		bool bRequiresAccumulator;

		/** Set this to true for the last sample for a given frame (if it requires accumulation) to fetch the accumulated data out of the accumulator. */
		bool bFetchFromAccumulator;

		/** Set this to true if this pass should be composited on top of other renders. */
		bool bCompositeOnOtherRenders;
	};

	/**
	* A collection of all of the data needed to produce the one output frame on disk. This holds
	* all of the pixel data associated with the output frame at once, so we can pass it to our 
	* image writing/video encoders in one go, to ensure we can do things like Multilayer EXRs which
	* embed all of the render passes inside of one file.
	*/
	struct FMovieGraphOutputMergerFrame
	{
		FMovieGraphOutputMergerFrame() = default;
		
		/**
		* The configuraton file that was being used by this frame during submission. We store it here so that it doesn't have to go through the pipeline
		* (which is difficult becuase it's a Garbage Collected object), and the Output Merger is the "join" step before we go back onto the Game Thread.
		*/
		TStrongObjectPtr<UMovieGraphEvaluatedConfig> EvaluatedConfig;

		/** 
		* Traversal context for the Movie Pipeline that isn't layer specific. Each image layer will have
		* its own traversal context within its payload, specific to itself. */
		FMovieGraphTraversalContext TraversalContext;

		/**
		* An array of expected data identifiers this frame. This is generated when the frame starts, 
		* and this frame won't be complete until ImageOutputData contains an image for each expected pass.
		*/
		TArray<FMovieGraphRenderDataIdentifier> ExpectedRenderPasses;

		/** Stores the actual pixel data for each render pass. */
		TMap<FMovieGraphRenderDataIdentifier, TUniquePtr<FImagePixelData>> ImageOutputData;
	};

	/**
	* Interface for the intermediate threadsafe class that allocates new frames, holds onto the individual
	* pieces as they come in, and eventually moves it back to the game thread once the data is complete.
	*/
	struct IMovieGraphOutputMerger : public TSharedFromThis<IMovieGraphOutputMerger>
	{
		virtual ~IMovieGraphOutputMerger() {};

		/** Call once per output frame to generate a struct to hold data. */
		virtual FMovieGraphOutputMergerFrame& AllocateNewOutputFrame_GameThread(int32 InFrameNumber) = 0;

		/** Getter for the Output Frame. Make sure you call AllocateNewOutputFrame_GameThread first, otherwise this trips a check. */
		virtual FMovieGraphOutputMergerFrame& GetOutputFrame_GameThread(int32 InFrameNumber) = 0;

		/** When a final accumulated render pass is available, call this function with the pixel data. */
		virtual void OnCompleteRenderPassDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData) = 0;

		/** When a individual sample is read back from the GPU, call this function. Debug Feature, immediately writes to disk. */
		virtual void OnSingleSampleDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData) = 0;

		/** Abandon any frames we're waiting on, ie; render stopped in the middle of a many temporal sample count frame. */
		virtual void AbandonOutstandingWork() = 0;

		/** Number of frames that have been started (via AllocateNewOutputFrame_GameThread) but haven't had all of their data come in yet via OnCompleteRenderPassDataAvailable_AnyThread. */
		virtual int32 GetNumOutstandingFrames() const = 0;

		/** Get a thread-safe FIFO queue of finished frame data. */
		virtual TQueue<FMovieGraphOutputMergerFrame>& GetFinishedFrames() = 0;
	};
}
