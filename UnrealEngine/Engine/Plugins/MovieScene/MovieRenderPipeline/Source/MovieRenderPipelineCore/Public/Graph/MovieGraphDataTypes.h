// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Graph/MovieGraphTraversalContext.h"
#include "Graph/MovieGraphRenderDataIdentifier.h"
#include "UObject/Object.h"
#include "UObject/StrongObjectPtr.h"
#include "Templates/SubclassOf.h"
#include "ImagePixelData.h"
#include "Containers/Queue.h"
#include "Containers/Set.h"
#include "Misc/FrameRate.h"
#include "MovieRenderPipelineDataTypes.h"
#include "Engine/EngineCustomTimeStep.h"

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
struct FMovieGraphImagePreviewData
{
	GENERATED_BODY()

	FMovieGraphImagePreviewData()
	: Texture(nullptr)
	{}

	/** The texture this preview image was rendered to. */
	UPROPERTY(BlueprintReadOnly, Category = "Movie Graph")
	class UTexture* Texture;
	
	/** The identifier for the image, containing the branch name, renderer, etc. */
	UPROPERTY(BlueprintReadOnly, Category = "Movie Graph")
	FMovieGraphRenderDataIdentifier Identifier;
};

USTRUCT(BlueprintType)
struct MOVIERENDERPIPELINECORE_API FMovieGraphInitConfig
{
	GENERATED_BODY()
	
	FMovieGraphInitConfig();
	
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
	 * Which class should the UMovieGraphPipeline use to generate audio. Defaults to
	 * UMovieGraphDefaultAudioOutput.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	TSubclassOf<UMovieGraphAudioRendererBase> AudioRendererClass;

	/**
	* Should the UMovieGraphPipeline render the full player viewport? Defaults
	* to false (so no 3d content is rendered) so we can display the UMG widgets
	* and MRQ rendering always happens in an off-screen render target.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Graph")
	bool bRenderViewport;
};

UCLASS(BlueprintType, Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphTimeStepBase : public UObject
{
	GENERATED_BODY()
public:
	/** Called each frame while the Movie Graph Pipeline is in a producing frames state. */
	virtual void TickProducingFrames() {}

	/** Called when this time step instance becomes active (ie: at the start of a shot). */
	virtual void Initialize() {}
	/** Called when this time step instance is no longer active (ie: at the end of a shot). */
	virtual void Shutdown() {}

	/** 
	* TickProducingFrames will be called for a frame (before the frame starts) and then this will be
	* called at the end of the frame when we kick off the renders for the frame. Should return data
	* needed to calculate the correct rendering timestep.
	*/
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	virtual FMovieGraphTimeStepData GetCalculatedTimeData() const { return FMovieGraphTimeStepData(); }
	
	/**
	* When expanding shots, do we need to expand the first frame by one extra to account for how
	* temporal sub-sampling can sample outside the first frame (due to centered frame evals).
	*/
	virtual bool IsExpansionForTSRequired(const TObjectPtr<UMovieGraphEvaluatedConfig>& InConfig) const { return false; }

	UMovieGraphPipeline* GetOwningGraph() const;
};


UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphEngineTimeStep : public UEngineCustomTimeStep
{
	GENERATED_BODY()
public:
	UMovieGraphEngineTimeStep();

	struct FTimeStepCache
	{
		FTimeStepCache()
			: UndilatedDeltaTime(0.0)
		{}

		FTimeStepCache(double InUndilatedDeltaTime)
			: UndilatedDeltaTime(InUndilatedDeltaTime)
		{}

		double UndilatedDeltaTime;
	};

	struct FSharedData
	{
		FSharedData()
			: OutputFrameNumber(0)
			, RenderedFrameNumber(0)
		{}
		
		
		/** Which output frame are we working on, relative to zero.*/
		int32 OutputFrameNumber;

		/** 
		* Index of which frames we've submitted for rendering. Doesn't line up with OutputFrameNumber when using warm-up frames.
		* Used internally by the rendering engine to keep track of which frames need to be read back.
		*/
		int32 RenderedFrameNumber;
	};

public:
	void SetCachedFrameTiming(const FTimeStepCache& InTimeCache);

	// UEngineCustomTimeStep Interface
	virtual bool Initialize(UEngine* InEngine) override;
	virtual void Shutdown(UEngine* InEngine) override;
	virtual bool UpdateTimeStep(UEngine* InEngine) override;
	virtual ECustomTimeStepSynchronizationState GetSynchronizationState() const override { return ECustomTimeStepSynchronizationState::Synchronized; }
	// ~UEngineCustomTimeStep Interface

	/** We don't do any thinking on our own, instead we just spit out the numbers stored in our time cache. */
	FTimeStepCache TimeCache;

	/** Data that should be shared between all shot time step instances. */
	FSharedData SharedTimeStepData;

	// Not cached in TimeCache as TimeCache is reset every frame.
	float PrevMinUndilatedFrameTime;
	float PrevMaxUndilatedFrameTime;
};

UCLASS(BlueprintType, Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphRendererBase : public UObject
{
	GENERATED_BODY()
public:
	/** Get an array of image previews that are valid this frame. */
	UFUNCTION(BlueprintCallable, Category = "Movie Graph")
	virtual TArray<FMovieGraphImagePreviewData> GetPreviewData() const { return TArray<FMovieGraphImagePreviewData>(); }
		
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
	
	/** Called by the Time Step system when the external data source should start playback (time values will have been set by SyncDataSourceTime */
	virtual void PlayDataSource() {}

	/** Called by the Time Step system when the external data source should pause playback. */
	virtual void PauseDataSource() {}

	/** Called by the Time Step system when the external data source should jump to the given time. Time is in TickResolution scale. */
	virtual void JumpDataSource(const FFrameTime& InTimeToJumpTo) {}

	/** 
	* Called when the Movie Graph Pipeline starts before anything has happened, allowing you to 
	* cache your datasource before making any modifications to it as a result of rendering.
	*/
	virtual void CacheDataPreJob(const FMovieGraphInitConfig& InInitConfig) {}
	virtual void RestoreCachedDataPostJob() {}
	virtual void UpdateShotList() {}
	virtual void InitializeShot(const TObjectPtr<UMoviePipelineExecutorShot>& InShot) {}
	virtual void CacheHierarchyForShot(const TObjectPtr<UMoviePipelineExecutorShot>& InShot) {}
	virtual void RestoreHierarchyForShot(const TObjectPtr<UMoviePipelineExecutorShot> &InShot) {}
	virtual void MuteShot(const TObjectPtr<UMoviePipelineExecutorShot>& InShot) {}
	virtual void UnmuteShot(const TObjectPtr<UMoviePipelineExecutorShot>& InShot) {}
	virtual void ExpandShot(const TObjectPtr<UMoviePipelineExecutorShot>& InShot, const int32 InLeftDeltaFrames, const int32 InLeftDeltaFramesUserPoV,
		const int32 InRightDeltaFrames, const bool bInPrepass) {}

	UMovieGraphPipeline* GetOwningGraph() const;
};

/** Base class for generating audio while the pipeline is running. */
UCLASS(BlueprintType, Abstract)
class MOVIERENDERPIPELINECORE_API UMovieGraphAudioRendererBase : public UObject
{
	GENERATED_BODY()

public:
	/** Tell our submixes to start capturing the data they are generating. Should only be called once output frames are being produced. */
	virtual void StartAudioRecording() {}

	/** Tell our submixes to stop capturing the data, and then store a copy of it. */
	virtual void StopAudioRecording() {}

	/** Attempt to process the audio thread work. This is complicated by non-linear time steps. */
	virtual void ProcessAudioTick() {}

	/** Prepares for audio rendering (ensuring volume is set correctly, the needed cvars are set, etc). */
	virtual void SetupAudioRendering() {}

	/** Undoes the work done in SetupAudioRendering(). */
	virtual void TeardownAudioRendering() const {}

	/** Gets the pipeline that owns this audio output instance. */
	UMovieGraphPipeline* GetOwningGraph() const;

	/** Gets the current state of the audio renderer. This is the main data source that audio-related nodes can reference. */
	const MoviePipeline::FAudioState& GetAudioState() const;

protected:
	MoviePipeline::FAudioState AudioState;
};

// ToDo: Both of these can probably go into the Default Renderer implementation.
struct FMovieGraphRenderPassLayerData
{
	FName BranchName;
	FString LayerName;
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
			, OverscanFraction(0.f)
			, CompositingSortOrder(0)
			, bAllowOCIO(true)
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

		/** When using high-res tiling, how many pixels does each tile overlap the adjacent tiles (on each side)? This should be zero if not using tiling. */
		FIntPoint OverlappedPad;

		/** When using high-res tiling, how many pixels offset into the output image does this accumulation get added to. */
		FIntPoint OverlappedOffset;

		/** When using spatial jitters, how much do we need to shift the output data during accumulation to counter-act the jitter. */
		FVector2D OverlappedSubpixelShift;

		/** When using camera overscan, what fraction (0-1) did this render use? Needed so that exrs can take a center-out crop of the data. */
		float OverscanFraction;

		/**
		* If multiple passes are composited on top of a render, the sort order determines the order in which they're composited.
		* Passes with a low sort order will composite on top of passes with a higher sort order.
		*/
		int32 CompositingSortOrder;

		/** Allow OpenColorIO transform to be used on this render. */
		bool bAllowOCIO;

		/** Render scene capture source used for tracking the output color space (without OpenColorIO). */
		ESceneCaptureSource SceneCaptureSource;
	};

	/**
	* A collection of validation information extracted from the FMovieGraphOutputMergerFrame 
	*/
	struct FMovieGraphRenderDataValidationInfo
	{
		int32 LayerCount = 0;
		int32 BranchCount = 0;
		int32 ActiveBranchRendererCount = 0;
		int32 ActiveRendererSubresourceCount = 0;
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

		/** Additional metadata to be added to the output (if supported by the output container). */
		TMap<FString, FString> FileMetadata;

		/** Get filename token validation info from the (expected) render passes. */
		MOVIERENDERPIPELINECORE_API FMovieGraphRenderDataValidationInfo GetValidationInfo(const FMovieGraphRenderDataIdentifier& InRenderID, bool bInDiscardCompositedRenders = true) const;
	};

	/**
	* Interface for the intermediate threadsafe class that allocates new frames, holds onto the individual
	* pieces as they come in, and eventually moves it back to the game thread once the data is complete.
	*/
	struct IMovieGraphOutputMerger : public TSharedFromThis<IMovieGraphOutputMerger>
	{
		virtual ~IMovieGraphOutputMerger() {};

		/** Call once per output frame to generate a struct to hold data. */
		virtual FMovieGraphOutputMergerFrame& AllocateNewOutputFrame_GameThread(const int32 InRenderedFrameNumber) = 0;

		/** Getter for the Output Frame. Make sure you call AllocateNewOutputFrame_GameThread first, otherwise this trips a check. */
		virtual FMovieGraphOutputMergerFrame& GetOutputFrame_GameThread(const int32 InRenderedFrameNumber) = 0;

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
