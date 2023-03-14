// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Timecode.h"
#include "ImagePixelData.h"
#include "Containers/UnrealString.h"
#include "Engine/Scene.h"
#include "Engine/EngineTypes.h"
#include "DSP/BufferVectorOperations.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "Sections/MovieSceneSubSection.h"
#include "OpenColorIOColorSpace.h"
#include "Async/ParallelFor.h"
#include "MovieSceneSequenceID.h"
#include "MovieRenderDebugWidget.h"
#include "MovieRenderPipelineDataTypes.generated.h"

class UMovieSceneCinematicShotSection;

template<typename ElementType> class TRange;
template<class T, class TWeakObjectPtrBase> struct TWeakObjectPtr;

class UWorld;
class ULevelSequence;
class UMovieSceneCinematicShotSection;
class UMoviePipelineMasterConfig;
class UMoviePipelineShotConfig;
class UMovieSceneCameraCutSection;
class UMoviePipelineRenderPass;
struct FImagePixelData;
struct FImageOverlappedAccumulator;
class FMoviePipelineOutputMerger;
class FRenderTarget;
class UMoviePipeline;
struct FMoviePipelineFormatArgs;
class UMoviePipelineExecutorShot;

namespace Audio { class FMixerSubmix; }

/**
* What is the current overall state of the Pipeline? States are processed in order from first to
* last and all states will be hit (though there is no guarantee the state will not be transitioned
* away from on the same frame it entered it). Used to help track overall progress and validate
* code flow.
*/
UENUM(BlueprintType)
enum class EMovieRenderPipelineState : uint8
{
	/** The pipeline has not been initialized yet. Only valid operation is to call Initialize. */
	Uninitialized = 0,
	/** The pipeline has been initialized and is now controlling time and working on producing frames. */
	ProducingFrames = 1,
	/** All desired frames have been produced. Audio is already finalized. Outputs have a chance to finish long processing tasks. */
	Finalize = 2,
	/** All outputs have finished writing to disk or otherwise processing. Additional exports that may have needed information about the produced file can now be run. */
	Export = 3,
	/** The pipeline has been shut down. It is an error to shut it down again. */
	Finished = 4,
};

/**
* What is the current state of a shot? States are processed in order from first to last but not
* all states are required, ie: WarmUp and MotionBlur can be disabled and the shot will never
* pass through this state.
*/
UENUM(BlueprintType)
enum class EMovieRenderShotState : uint8
{
	/** The shot has not been initialized yet.*/
	Uninitialized = 0,

	/** The shot is warming up. Engine ticks are passing but no frames are being produced. */
	WarmingUp = 1,
	/*
	* The shot is doing additional pre-roll for motion blur. No frames are being produced,
	* but the rendering pipeline is being run to seed histories.
	*/
	MotionBlur = 2,
	/*
	* The shot is working on producing frames and may be currently doing a sub-frame or
	* a whole frame.
	*/
	Rendering = 3,
	/*
	* The shot has produced all frames it will produce. No more evaluation should be
	* done for this shot once it reaches this state.
	*/
	Finished = 4
};

USTRUCT(BlueprintType)
struct FMoviePipelinePassIdentifier
{
	GENERATED_BODY()

	FMoviePipelinePassIdentifier()
	{}

	// Name defaults to "camera" for backwards compatiblity with non-multicam metadata
	FMoviePipelinePassIdentifier(const FString& InPassName, const FString& InCameraName = TEXT("camera"))
		: Name(InPassName)
		, CameraName(InCameraName)
	{
	}

	bool operator == (const FMoviePipelinePassIdentifier& InRHS) const
	{
		return Name == InRHS.Name && CameraName == InRHS.CameraName;
	}

	bool operator != (const FMoviePipelinePassIdentifier& InRHS) const
	{
		return !(*this == InRHS);
	}

	friend uint32 GetTypeHash(FMoviePipelinePassIdentifier OutputState)
	{
		return HashCombine(GetTypeHash(OutputState.Name), GetTypeHash(OutputState.CameraName));
	}

public:
	// The name of the pass such as "FinalImage" or "ObjectId", etc.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Pipeline")
	FString Name;

	// The name of the camera that this pass is for. Stored here so we can differentiate between 
	// multiple cameras within a single pass.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movie Pipeline")
	FString CameraName;
};

namespace MoviePipeline
{
	/**
	* Frame info needed for creating the FSceneView for each tile.
	*/
	struct FMoviePipelineFrameInfo
	{
	public:
		FMoviePipelineFrameInfo() {}
		virtual ~FMoviePipelineFrameInfo() {};

		/**
		 * We have to apply camera motion vectors manually. So we keep the current and previous fram'es camera view and rotation.
		 * Then we render a sequence of the same movement, and update after running the game sim.
		 **/

		/** Current frame camera view location **/
		FVector CurrViewLocation;

		/** Current frame camera view rotation **/
		FRotator CurrViewRotation;

		/** Previous frame camera view location **/
		FVector PrevViewLocation;

		/** Previous frame camera view rotation **/
		FRotator PrevViewRotation;

		TArray<FVector> CurrSidecarViewLocations;
		TArray<FRotator> CurrSidecarViewRotations;
		TArray<FVector> PrevSidecarViewLocations;
		TArray<FRotator> PrevSidecarViewRotations;
	};

	/**
	* Utility function for the Pipeline to calculate the time metrics for the current
	* frame that can then be used by the custom time step to avoid custom time logic.
	*/
	struct FFrameTimeStepCache
	{
		FFrameTimeStepCache()
			: UndilatedDeltaTime(0.0)
		{}

		FFrameTimeStepCache(double InDeltaTime)
			: UndilatedDeltaTime(InDeltaTime)
		{}

		double UndilatedDeltaTime;
	};

	struct FOutputFrameData
	{
		FIntPoint Resolution;

		TArray<FColor> ColorBuffer;

		FString PassName;
	};

	struct MOVIERENDERPIPELINECORE_API FTileWeight1D
	{
	public:
		/**
		 * For tiles, this struct stores the weight function in one dimension.
		 *
		 *                X1           X2
		 * 1.0              |------------|
		 *                 /              \
		 *                /                \
		 *               /                  \
		 * 0.0 |--------|                    |----------------|
		 *     0       X0                    X3             Size
		 *
		 *  For a tile, the weight goes from:
		 *  [0 ,X0]    : 0.0
		 *  [X0,X1]    : 0.0 to 1.0
		 *  [X1,X2]    : 1.0
		 *  [X2,X3]    : 1.0 to 0.0
		 *  [X3,SizeX] : 0.0
		 **/

		/** Constructor. Sets to invalid values and must be initialized to reasonable values before using. **/
		FTileWeight1D()
		{
			X0 = 0;
			X1 = 0;
			X2 = 0;
			X3 = 0;
		}

		/** Make sure this map has valid values. **/
		void CheckValid() const
		{
			check(0 <= X0);
			check(X0 <= X1);
			check(X1 <= X2);
			check(X2 <= X3);
		}

		/** Is equal operator.**/
		bool operator==(const FTileWeight1D& Rhs) const;

		/**
		 * The full tile is of size PadLeft + SizeCenter + PadRight
		 *
		 *  |------PadLeft-------|------------SizeCenter-----------|------PadRight-----|
		 *  
		 * This function puts X0 in the middle of PadLeft, X3 in the middle of PadRight.
		 * And X1 is "reflected" around PadLeft, and X2 is "reflected" around PadRight.
		 *
		 *  |------PadLeft-------|------------SizeCenter-----------|------PadRight-----|
		 *           X0                  X1                  X2              X3
		 * Also note that PadLeft and PadRight are allowed to be zero, but SizeCenter must be > 0;
		 **/
		void InitHelper(int32 PadLeft, int32 SizeCenter, int32 PadRight);

		float CalculateWeight(int32 Pixel) const;

		void CalculateArrayWeight(TArray<float>& WeightData, int Size) const;

		int32 X0;
		int32 X1;
		int32 X2;
		int32 X3;
	};

	struct FFrameConstantMetrics
	{
		/** What is the tick resolution of the master sequence */
		FFrameRate TickResolution;
		/** What is the tick resolution of the current shot */
		FFrameRate ShotTickResolution;
		/** What is the effective frame rate of the output */
		FFrameRate FrameRate;
		/** How many ticks per output frame. */
		FFrameTime TicksPerOutputFrame;
		/** How many ticks per sub-frame sample. */
		FFrameTime TicksPerSample;
		/** How many ticks while the shutter is closed */
		FFrameTime TicksWhileShutterClosed;
		/** How many ticks while the shutter is opened */
		FFrameTime TicksWhileShutterOpen;

		/** Fraction of the output frame that the accumulation frames should cover. */
		double ShutterAnglePercentage;

		/** Fraction of the output frame that the shutter is closed (that accumulation frames should NOT cover). */
		double ShutterClosedFraction;

		/**
		* Our time samples are offset by this many ticks. For a given output frame N, we can bias the time N represents
		* to be before, during or after the data stored on time N.
		*/
		FFrameTime ShutterOffsetTicks;

		/** 
		* How many ticks do we have to go to center our motion blur? This makes it so the blurred area matches the object
		* position during the time it represents, and not before.
		*/
		FFrameTime MotionBlurCenteringOffsetTicks;

		FFrameTime GetFinalEvalTime(const FFrameNumber InTime) const
		{
			// We just use a consistent offset from the given time to take motion blur centering and
			// shutter timing offsets into account. Written here just to consolidate various usages.
			return FFrameTime(InTime) + MotionBlurCenteringOffsetTicks + ShutterOffsetTicks;
		}
	};

	struct FCameraCutSubSectionHierarchyNode : TSharedFromThis<FCameraCutSubSectionHierarchyNode>
	{
		FCameraCutSubSectionHierarchyNode()
			: bOriginalMovieSceneReadOnly(false)
			, bOriginalMovieScenePlaybackRangeLocked(false)
			, bOriginalMovieScenePackageDirty(false)
			, bOriginalShotSectionIsLocked(false)
			, bOriginalCameraCutIsActive(false)
			, bOriginalShotSectionIsActive(false)
			, EvaluationType(EMovieSceneEvaluationType::WithSubFrames)
			, NodeID(MovieSceneSequenceID::Invalid)
		{
		}

		/** The UMovieScene that this node has data for. */
		TWeakObjectPtr<class UMovieScene> MovieScene;
		/** The UMovieSceneSubSection within the movie scene (if any) */
		TWeakObjectPtr<class UMovieSceneSubSection> Section;
		/** The UMovieSceneCameraCutSection within the movie scene (if any) */
		TWeakObjectPtr<class UMovieSceneCameraCutSection> CameraCutSection;
		
		// These are used to restore this node back to its proper shape post render.
		TRange<FFrameNumber> OriginalMovieScenePlaybackRange;
		bool bOriginalMovieSceneReadOnly;
		bool bOriginalMovieScenePlaybackRangeLocked;
		bool bOriginalMovieScenePackageDirty;

		bool bOriginalShotSectionIsLocked;
		TRange<FFrameNumber> OriginalShotSectionRange;
		TRange<FFrameNumber> OriginalCameraCutSectionRange;

		bool bOriginalCameraCutIsActive;
		bool bOriginalShotSectionIsActive;

		/** An array of sections that we should expand, as well as their original range for restoration later. */
		TArray<TTuple<UMovieSceneSection*, TRange<FFrameNumber>>> AdditionalSectionsToExpand;

		EMovieSceneEvaluationType EvaluationType;
		FMovieSceneSequenceID NodeID;

		void AddChild(TSharedPtr<FCameraCutSubSectionHierarchyNode> InChild)
		{
			if (!InChild)
			{
				return;
			}

			InChild->Parent = AsShared();
			Children.AddUnique(InChild);
		}

		TArray<TSharedPtr<FCameraCutSubSectionHierarchyNode>> GetChildren() const
		{
			return Children;
		}

		void SetParent(TSharedPtr<FCameraCutSubSectionHierarchyNode> InParent)
		{
			if (!InParent)
			{
				return;
			}

			if (InParent == Parent)
			{
				return;
			}

			ensureAlwaysMsgf(!Parent, TEXT("Cannot switch parents of FCameraCutSubSectionHierarchy nodes."));

			// Automatically adds us as a child of the parent and assigns out parent.
			InParent->AddChild(AsShared());
		}

		bool IsEmpty() const
		{
			return *this == FCameraCutSubSectionHierarchyNode();
		}

		TSharedPtr<FCameraCutSubSectionHierarchyNode> GetParent() const { return Parent; }

	private:
		/** A pointer to the next node in the tree who actually includes us in the hierarchy. Different than outers as each moviescene is outered to its own package. */
		TSharedPtr<FCameraCutSubSectionHierarchyNode> Parent;
		/** An array of pointers to our children for downwards traversal. */
		TArray<TSharedPtr<FCameraCutSubSectionHierarchyNode>> Children;

		bool operator == (const FCameraCutSubSectionHierarchyNode& InRHS) const
		{
			return MovieScene == InRHS.MovieScene
				&& Section == InRHS.Section
				&& CameraCutSection == InRHS.CameraCutSection
				&& OriginalMovieScenePlaybackRange == InRHS.OriginalMovieScenePlaybackRange
				&& bOriginalMovieSceneReadOnly == InRHS.bOriginalMovieSceneReadOnly
				&& bOriginalMovieScenePlaybackRangeLocked == InRHS.bOriginalMovieScenePlaybackRangeLocked
				&& bOriginalMovieScenePackageDirty == InRHS.bOriginalMovieScenePackageDirty
				&& bOriginalShotSectionIsLocked == InRHS.bOriginalShotSectionIsLocked
				&& OriginalShotSectionRange == InRHS.OriginalShotSectionRange
				&& OriginalCameraCutSectionRange == InRHS.OriginalCameraCutSectionRange
				&& bOriginalCameraCutIsActive == InRHS.bOriginalCameraCutIsActive
				&& bOriginalShotSectionIsActive == InRHS.bOriginalShotSectionIsActive
				&& EvaluationType == InRHS.EvaluationType
				&& NodeID == InRHS.NodeID
				&& Children == InRHS.Children
				&& Parent == InRHS.Parent;
		}

		bool operator != (const FCameraCutSubSectionHierarchyNode& InRHS) const
		{
			return !(*this == InRHS);
		}
	};
}

USTRUCT(BlueprintType)
struct FMoviePipelineSegmentWorkMetrics
{
	GENERATED_BODY()

public:
	FMoviePipelineSegmentWorkMetrics()
		: SegmentName()
		, OutputFrameIndex(-1)
		, TotalOutputFrameCount(-1)
		, OutputSubSampleIndex(-1)
		, TotalSubSampleCount(-1)
		, EngineWarmUpFrameIndex(-1)
		, TotalEngineWarmUpFrameCount(-1)
	{ }	
	
	// This information is used to be presented in the UI.
	
	/** The name of the segment (if any) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipeline Segment")
	FString SegmentName;
	
	/** Index of the output frame we are working on right now. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipeline Segment")
	int32 OutputFrameIndex;
	/** The number of output frames we expect to make for this segment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipeline Segment")
	int32 TotalOutputFrameCount;


	/** Which temporal/spatial sub sample are we working on right now. This counts temporal, spatial, and tile samples to accurately reflect how much work is needed for this output frame. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipeline Segment")
	int32 OutputSubSampleIndex;
	
	/** The total number of samples we will have to build to render this output frame. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipeline Segment")
	int32 TotalSubSampleCount;
	
	/** The index of the engine warm up frame that we are working on. No rendering samples are produced for these. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipeline Segment")
	int32 EngineWarmUpFrameIndex;
	
	/** The total number of engine warm up frames for this segment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Pipeline Segment")
	int32 TotalEngineWarmUpFrameCount;
};


UENUM(BlueprintType)
enum class EMoviePipelineShutterTiming : uint8
{
	FrameOpen,
	FrameCenter,
	FrameClose
};

// ToDo: Rename this to segment.
USTRUCT(BlueprintType)
struct FMoviePipelineCameraCutInfo
{
	GENERATED_BODY()
public:
	FMoviePipelineCameraCutInfo()
		: bEmulateFirstFrameMotionBlur(true)
		, NumTemporalSamples(0)
		, NumSpatialSamples(0)
		, NumTiles(0, 0)
		, State(EMovieRenderShotState::Uninitialized)
		, bHasEvaluatedMotionBlurFrame(false)
		, NumEngineWarmUpFramesRemaining(0)
		, VersionNumber(0)
	{
	}

	bool IsInitialized() const { return State != EMovieRenderShotState::Uninitialized; }
	void SetNextStateAfter(const EMovieRenderShotState InCurrentState);
	void CalculateWorkMetrics();

private:
	FFrameNumber GetOutputFrameCountEstimate() const;

public:
	
	TSharedPtr<MoviePipeline::FCameraCutSubSectionHierarchyNode> SubSectionHierarchy;


	/** 
	* Should we evaluate/render an extra frame at the start of this shot to show correct motion blur on the first frame? 
	* This emulates motion blur by evaluating forward one frame and then going backwards which is a approximation.
	*/
	bool bEmulateFirstFrameMotionBlur;

	/** How many temporal samples is each frame broken up into? */
	int32 NumTemporalSamples;

	/** For each temporal sample, how many spatial samples do we take? */
	int32 NumSpatialSamples;

	/** How many image tiles are going to be rendered per temporal frame. */
	FIntPoint NumTiles;

	/** Cached Frame Rate these are being rendered at. Simplifies some APIs. */
	FFrameRate CachedFrameRate;

	/** Cached Tick Resolution our numbers are in. Simplifies some APIs. */
	FFrameRate CachedTickResolution;
	
	/** Cached Tick Resolution the movie scene this range was generated for is in. Can be different than the master due to mixed tick resolutions. */
	FFrameRate CachedShotTickResolution;
public:
	/** The current state of processing this Shot is in. Not all states will be passed through. */
	EMovieRenderShotState State;

	/** The current tick of this shot that we're on in master space */
	FFrameNumber CurrentTickInMaster;

	/** Converts from the outermost space into the innermost space. Only works with linear transforms. */
	FMovieSceneTimeTransform OuterToInnerTransform;
	
	/** The total range of output frames in master space */
	TRange<FFrameNumber> TotalOutputRangeMaster;
	
	/** The total range of warmup frames in master space */
	TRange<FFrameNumber> WarmupRangeMaster;

	/** Metrics - How much work has been done for this particular shot and how much do we estimate we have to do? */
	FMoviePipelineSegmentWorkMetrics WorkMetrics;

public:
	/** Have we evaluated the motion blur frame? Only used if bEvaluateMotionBlurOnFirstFrame is set */
	bool bHasEvaluatedMotionBlurFrame;

	/** How many engine warm up frames are left to process for this shot. May be zero. */
	int32 NumEngineWarmUpFramesRemaining;

	/** What version number should this shot use when resolving format arguments. */
	int32 VersionNumber;
};

/**
* The Tick/Render loops are decoupled from the actual desired output. In some cases
* we may render but not desire an output (such as filling temporal histories) or
* we may be accumulating the results into a target which does not produce an output.
* Finally, we may be running the Tick/Render loop but not wanting to do anything!
*/
struct FMoviePipelineFrameOutputState
{
public:
	struct FTimeData
	{
		FTimeData()
			: MotionBlurFraction(0.f)
			, FrameDeltaTime(0.0)
			, WorldSeconds(0.0)
			, TimeDilation(1.f)
		{
		}

		/** A 0-1 fraction of how much motion blur should be applied. This is tied to the shutter angle but more complicated when using sub-sampling */
		float MotionBlurFraction;

		/** The delta time in seconds used for this frame. */
		double FrameDeltaTime;

		/** The total elapsed time since the pipeline started.*/
		double WorldSeconds;

		/**
		* Check if there was a non-1.0 Time Dilation in effect when this frame was produced. This indicates that there
		* may be duplicate frame Source/Effective frame numbers as they find the closest ideal time to the current.
		*/
		float TimeDilation;

		void ResetPerFrameData()
		{
			MotionBlurFraction = 0.f;
			FrameDeltaTime = 0.0;
			TimeDilation = 1.f;
		}

		FORCEINLINE bool IsTimeDilated() const { return !FMath::IsNearlyEqual(TimeDilation, 1.f); }
	};

	FMoviePipelineFrameOutputState()
		: OutputFrameNumber(-1)
		, ShotCount(0)
		, TemporalSampleIndex(-1)
		, TemporalSampleCount(0)
	{
		// There is other code which relies on this starting on -1.
		check(OutputFrameNumber == -1);

		// Likewise, the code assumes you start on sample index -1.
		check(TemporalSampleIndex == -1);

		ResetPerFrameData();
		ResetPerShotData();
	}

	/** Is this the first temporal sample for the output frame? */
	FORCEINLINE bool IsFirstTemporalSample() const { return TemporalSampleIndex == 0; }
	FORCEINLINE bool IsLastTemporalSample() const { return TemporalSampleIndex == (TemporalSampleCount - 1); }

	/**
	* The expected output frame count that the render is working towards creating.
	* This number accurately tracks the number of frames we have produced even if
	* the file written to disk uses a different number (due to relative frame numbers
	* or offset frames being added.
	*/
	int32 OutputFrameNumber;

	/** Which shot is this output state for? */
	int32 ShotIndex;

	/** How many shots total will we be outputting? */
	int32 ShotCount;

	/** Which sub-frame are we on when using Accumulation Frame rendering. */
	int32 TemporalSampleIndex;

	/** How many temporal samples do we add together to produce one Output Frame? */
	int32 TemporalSampleCount;

	/** 
	* The expected output frame count for this current shot that we're working towards
	* creating. Like OutputFrameNumber but relative to this shot. This should get reset between shots. 
	*/
	int32 ShotOutputFrameNumber;

	/** The total number of samples (including warm ups) that have been sent to the GPU for this shot. */
	int32 ShotSamplesRendered;

	/** What time data should this frame use? Can vary between samples when TemporalSampleCount > 1. */
	FTimeData TimeData;

	/** The name of the currently active camera being rendered. May be empty. */
	FString CameraName;

	/** Name used by the {camera_name} format tag. May be empty */
	FString CameraNameOverride;

	/** THe name of the currently active shot. May be empty if there is no shot track. */
	FString ShotName;

	/** INFORMATION BELOW HERE SHOULD NOT GET PERSISTED BETWEEN FRAMES */

	void ResetPerFrameData()
	{
		TimeData.ResetPerFrameData();
		bSkipRendering = false;
		bCaptureRendering = false;
		bDiscardRenderResult = false;
		SourceFrameNumber = 0;
		SourceTimeCode = FTimecode();
		EffectiveFrameNumber = 0;
		EffectiveTimeCode = FTimecode();
		CurrentShotSourceFrameNumber = 0;
		CurrentShotSourceTimeCode = FTimecode();
		FileMetadata.Reset();
	}

	void ResetPerShotData()
	{
		ShotOutputFrameNumber = -1;
		ShotSamplesRendered = 0;
		ShotIndex = 0;
		TemporalSampleCount = 0;
		CameraName.Reset();
		ShotName.Reset();
	}

	/**
	* If true, then the rendering for this frame should be skipped (ie: nothing submitted to the gpu, and the output merger not told to expect this frame).
	* This is used for rendering every Nth frame for rendering drafts. We still run the game thread logic for the skipped frames (which is relatively cheap)
	* and simply omit rendering them. This increases consistency with non-skipped renders, and will be useful for consistency when rendering on a farm.
	*/
	bool bSkipRendering;

	/**
	* If true, and a IRenderCaptureProvider is available, trigger a capture of the rendering process of this frame.
	*/
	bool bCaptureRendering;

	/**
	* If this is true, then the frame will be rendered but the results discarded and not sent to the accumulator. This is used for render warmup frames
	* or gpu-based feedback loops. Ignored if bSkipRendering is true.
	*/
	bool bDiscardRenderResult;


	/** The closest frame number (in Display Rate) on the Sequence. May be duplicates in the case of different output framerate or Play Rate tracks. */
	int32 SourceFrameNumber;

	/** The closest time code version of the SourceFrameNumber on the Sequence. May be a duplicate in the case of different output framerate or Play Rate tracks. */
	FTimecode SourceTimeCode;

	/** 
	* The closest frame number (in Display Rate) on the Sequence adjusted for the effective output rate. These numbers will not line up with the frame
	* in the source Sequence if the output frame rate differs from the Sequence display rate. May be a duplicate in the event of Play Rate tracks.
	*/
	int32 EffectiveFrameNumber;

	/** The closest time code version of the EffectiveFrameNumber. May be a duplicate in the event of Play Rate tracks. */
	FTimecode EffectiveTimeCode;

	/** Metadata to attach to the output file (if supported by the output container) */
	TMap<FString, FString> FileMetadata;


	int32 CurrentShotSourceFrameNumber;
	
	FTimecode CurrentShotSourceTimeCode;

	int32 CameraIndex;


	bool operator == (const FMoviePipelineFrameOutputState& InRHS) const
	{
		// ToDo: I don't think this is used anymore. Was used to make it so that the output state
		// for the first temporal sample (which is what the expected output was built off of) could
		// be equality against the last temporal sample when used in a TSet but better to do that explicitly.
		return
			OutputFrameNumber == InRHS.OutputFrameNumber &&
			// TemporalSampleIndex == InRHS.TemporalSampleIndex &&
			// FMath::IsNearlyEqual(MotionBlurFraction,InRHS.MotionBlurFraction) &&
			// FMath::IsNearlyEqual(FrameDeltaTime, InRHS.FrameDeltaTime) &&
			// FMath::IsNearlyEqual(WorldSeconds, InRHS.WorldSeconds) &&
			SourceFrameNumber == InRHS.SourceFrameNumber &&
			SourceTimeCode == InRHS.SourceTimeCode &&
			EffectiveFrameNumber == InRHS.EffectiveFrameNumber &&
			EffectiveTimeCode == InRHS.EffectiveTimeCode;
	}

	bool operator != (const FMoviePipelineFrameOutputState& InRHS) const
	{
		return !(*this == InRHS);
	}

	friend uint32 GetTypeHash(FMoviePipelineFrameOutputState OutputState)
	{
		return GetTypeHash(OutputState.OutputFrameNumber);
	}
};

USTRUCT(BlueprintType)
struct FMoviePipelineFormatArgs
{
	GENERATED_BODY()

	FMoviePipelineFormatArgs()
		: InJob(nullptr)
	{
	}

	/** A set of Key/Value pairs for output filename format strings (without {}) and their values. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	TMap<FString, FString> FilenameArguments;

	/** A set of Key/Value pairs for file metadata for file formats that support metadata. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	TMap<FString, FString> FileMetadata;

	/** Which job is this for? Some settings are specific to the level sequence being rendered. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	TObjectPtr<class UMoviePipelineExecutorJob> InJob;
};

USTRUCT(BlueprintType)
struct FMoviePipelineFilenameResolveParams
{
	GENERATED_BODY()

	FMoviePipelineFilenameResolveParams()
		: FrameNumber(0)
		, FrameNumberShot(0)
		, FrameNumberRel(0)
		, FrameNumberShotRel(0)
		, ZeroPadFrameNumberCount(0)
		, bForceRelativeFrameNumbers(false)
		, InitializationTime(0)
		, InitializationVersion(0)
		, Job(nullptr)
		, CameraIndex(-1)
		, ShotOverride(nullptr)
		, AdditionalFrameNumberOffset(0)
	{
	}
	
	/** Frame Number for the Master (matching what you see in the Sequencer timeline. ie: If the Sequence PlaybackRange starts on 50, this value would be 50 on the first frame.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	int32 FrameNumber;
	
	/** Frame Number for the Shot (matching what you would see in Sequencer at the sub-sequence level. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	int32 FrameNumberShot;
	
	/** Frame Number for the Master (relative to 0, not what you would see in the Sequencer timeline. ie: If sequence PlaybackRange starts on 50, this value would be 0 on the first frame. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	int32 FrameNumberRel;
	
	/** Frame Number for the Shot (relative to 0, not what you would see in the Sequencer timeline. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	int32 FrameNumberShotRel;
	
	/** Name used by the {camera_name} format tag. If specified, this will override the camera name (which is normally pulled from the ShotOverride object). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	FString CameraNameOverride;
	
	/** Name used by the {shot_name} format tag. If specified, this will override the shot name (which is normally pulled from the ShotOverride object) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	FString ShotNameOverride;

	/** When converitng frame numbers to strings, how many digits should we pad them up to? ie: 5 => 0005 with a count of 4. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	int32 ZeroPadFrameNumberCount;

	/** If true, force format strings (like {frame_number}) to resolve using the relative version. Used when slow-mo is detected as frame numbers would overlap. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	bool bForceRelativeFrameNumbers;

	/** Optional. If specified this is the filename that will be used instead of automatically building it from the Job's Output Setting. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	FString FileNameOverride;

	/** 
	* A map between "{format}" tokens and their values. These are applied after the auto-generated ones from the system,
	* which allows the caller to override things like {.ext} depending or {render_pass} which have dummy names by default.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	TMap<FString, FString> FileNameFormatOverrides;

	/** A key/value pair that maps metadata names to their values. Output is only supported in exr formats at the moment. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	TMap<FString, FString> FileMetadata;

	/** The initialization time for this job. Used to resolve time-based format arguments. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	FDateTime InitializationTime;

	/** The version for this job. Used to resolve version format arguments. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	int32 InitializationVersion;

	/** Required. This is the job all of the settings should be pulled from.*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	TObjectPtr<UMoviePipelineExecutorJob> Job;

	/** If this shot has multiple cameras in the sidecar array, which camera is this for? -1 will return the InnerName/Main camera for the shot. */
	int32 CameraIndex;

	/** Optional. If specified, settings will be pulled from this shot (if overriden by the shot). If null, always use the master configuration in the job. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	TObjectPtr<UMoviePipelineExecutorShot> ShotOverride;

	/** Additional offset added onto the offset provided by the Output Settings in the Job. Required for some internal things (FCPXML). */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Movie Render Pipeline")
	int32 AdditionalFrameNumberOffset;
};

/**
* These parameters define a single sample that a render pass should render with.
*/
struct FMoviePipelineRenderPassMetrics
{
public:
	/** What FrameIndex should the FSceneView use for its internal index. Used to ensure samples have unique TAA/Raytracing/etc. sequences */
	int32 FrameIndex;

	/** Is the Scene View frozen so no changes are made as a result of this sample? */
	bool bWorldIsPaused;

	/** Do we pass the camera cut flag to the renderer to clear histories? */
	bool bCameraCut;

	/** Which anti-aliasing method should this view use? */
	EAntiAliasingMethod AntiAliasingMethod;

	/** What type of output should the Post Processing output? (HDR /w ToneCurve, HDR w/o ToneCurve) */
	ESceneCaptureSource SceneCaptureSource;

	/** How many tiles on X and Y are there total. */
	FIntPoint OriginalTileCounts;

	/** How many tiles on X and Y are there total. */
	FIntPoint TileCounts;

	/** Of the TileCount, which X/Y Tile is this sample for. */
	FIntPoint TileIndexes;

	/** SpatialJitter offset X. */
	float SpatialShiftX;

	/** SpatialJitter offset Y. */
	float SpatialShiftY;


	/** Get a 0-(TileCount-1) version of TileIndex. */
	FORCEINLINE int32 GetTileIndex() const
	{
		return (TileIndexes.Y * TileCounts.Y) + TileIndexes.X;
	}

	FORCEINLINE int32 GetTileCount() const
	{
		return (TileCounts.X * TileCounts.Y);
	}

	/** How big is the back buffer for this sample? This is the final target size divided by the number of tiles with padding added. */
	FIntPoint BackbufferSize;

	/** Full Output image resolution for this shot. */
	FIntPoint EffectiveOutputResolution;

	/** How big is an individual tile for this sample? This is the final target size divided by the number of tiles (without padding added) */
	FIntPoint TileSize;

	/** If true, we will discard this sample after rendering. Used to get history set up correctly. */
	bool bDiscardResult;

	/** If true, we will write this sample to disk (and also send it to the accumulator.) */
	bool bWriteSampleToDisk;

	/** How many spatial jitters will there be total for this particular temporal accumulation frame? */
	int32 SpatialSampleCount;
	
	/** Of the SpatialSampleCount, which index is this? */
	int32 SpatialSampleIndex;

	/** How many temporal jitters will there be total for this particular output frame? */
	int32 TemporalSampleCount;

	/** Of the TemporalSampleCount, which index is this? */
	int32 TemporalSampleIndex;

	/** How many pixels are we overlapping with adjacent tiles (on each side) */
	FIntPoint OverlappedPad;

	/** How much is this sample offset, taking padding into account. */
	FIntPoint OverlappedOffset;

	/** Use overscan percentage to extend render region beyond the set resolution.  */
	float OverscanPercentage;

	/** 
	* The gamma space to apply accumulation in. During accumulation, pow(x,AccumulationGamma) is applied
	* and pow(x,1/AccumulationGamma) is applied after accumulation is finished. 1.0 means no change."
	*/
	float AccumulationGamma;

	/**
	* The amount of screen-space shift used to replace TAA Projection Jitter, modified for each spatial sample
	* of the render.
	*/
	FVector2D ProjectionMatrixJitterAmount;

	/**
	* Any additional texture mip map bias that should be added when rendering. Can be used to force extra sharpness. A more negative number means more likely to use a higher quality mip map.
	*/
	float TextureSharpnessBias;

	/**
	* What screen percentage should each view be rendered at? 1.0 = standard size. Can gain more detail (at the cost of longer renders, more device timeouts), consider trying to use TextureSharpnessBias if possible instead.
	*/
	float GlobalScreenPercentageFraction;


	FMoviePipelineFrameOutputState OutputState;

	FVector2D OverlappedSubpixelShift;

	MoviePipeline::FMoviePipelineFrameInfo FrameInfo;

	FOpenColorIODisplayConfiguration* OCIOConfiguration;
};

namespace MoviePipeline
{
	struct FMoviePipelineRenderPassInitSettings
	{
	public:
		UE_DEPRECATED(5.0, "FMoviePipelineRenderPassInitSettings must be constructed with arguments")
		FMoviePipelineRenderPassInitSettings()
		{
			FeatureLevel = GMaxRHIFeatureLevel;
		}

		FMoviePipelineRenderPassInitSettings(ERHIFeatureLevel::Type InFeatureLevel, const FIntPoint& InBackbufferResolution, const FIntPoint& InTileCount)
			:	BackbufferResolution(InBackbufferResolution)
			,	TileCount(InTileCount)
			,	FeatureLevel(InFeatureLevel)
		{
		}

	public:
		/** This takes into account any padding needed for tiled rendering overlap. Different than the output resolution of the final image. */
		FIntPoint BackbufferResolution = FIntPoint(0, 0);

		/** How many tiles (in each direction) are we rendering with. */
		FIntPoint TileCount = FIntPoint(0, 0);

		ERHIFeatureLevel::Type FeatureLevel;
	};

	struct FCompositePassInfo
	{
		FCompositePassInfo() {}

		FMoviePipelinePassIdentifier PassIdentifier;
		TUniquePtr<FImagePixelData> PixelData;
	};

}

struct FImagePixelDataPayload : IImagePixelDataPayload, public TSharedFromThis<FImagePixelDataPayload, ESPMode::ThreadSafe>
{
	FMoviePipelineRenderPassMetrics SampleState;

	FMoviePipelinePassIdentifier PassIdentifier;
	
	/** Does this output data have to be transparent to be useful? Overrides output format to one that supports transparency. */
	bool bRequireTransparentOutput;

	int32 SortingOrder;
	bool bCompositeToFinalImage;

	/** If specified, use this as the output filename (not including output directory) when using debug write samples to disk */
	FString Debug_OverrideFilename;

	FImagePixelDataPayload()
		: bRequireTransparentOutput(false)
		, SortingOrder(TNumericLimits<int32>::Max())
		, bCompositeToFinalImage(false)
	{}

	virtual TSharedRef<FImagePixelDataPayload> Copy() const
	{
		return MakeShared<FImagePixelDataPayload>(*this);
	}

	virtual FIntPoint GetAccumulatorSize() const
	{
		return FIntPoint(SampleState.TileSize.X * SampleState.TileCounts.X, SampleState.TileSize.Y * SampleState.TileCounts.Y);
	}

	virtual FIntPoint GetOverlappedOffset() const
	{
		return SampleState.OverlappedOffset;
	}

	virtual FVector2D GetOverlappedSubpixelShift() const
	{
		return SampleState.OverlappedSubpixelShift;
	}

	virtual void GetWeightFunctionParams(MoviePipeline::FTileWeight1D& WeightFunctionX, MoviePipeline::FTileWeight1D& WeightFunctionY) const
	{
		WeightFunctionX.InitHelper(SampleState.OverlappedPad.X, SampleState.TileSize.X, SampleState.OverlappedPad.X);
		WeightFunctionY.InitHelper(SampleState.OverlappedPad.Y, SampleState.TileSize.Y, SampleState.OverlappedPad.Y);
	}

	virtual FIntPoint GetOverlapPaddedSize() const
	{
		return FIntPoint(
			(SampleState.TileSize.X + 2 * SampleState.OverlappedPad.X),
			(SampleState.TileSize.Y + 2 * SampleState.OverlappedPad.Y));
	}

	virtual bool GetOverlapPaddedSizeIsValid(const FIntPoint InRawSize) const
	{
		return (SampleState.TileSize.X + 2 * SampleState.OverlappedPad.X == InRawSize.X)
			&& (SampleState.TileSize.Y + 2 * SampleState.OverlappedPad.Y == InRawSize.Y);
	}

	/** Is this the first tile of an image and we should start accumulating? */
	FORCEINLINE bool IsFirstTile() const
	{
		return SampleState.TileIndexes.X == 0 && SampleState.TileIndexes.Y == 0 && SampleState.SpatialSampleIndex == 0;
	}

	/** Is this the last tile of an image and we should finish accumulating? */
	FORCEINLINE bool IsLastTile() const
	{
		return SampleState.TileIndexes.X == SampleState.TileCounts.X - 1 &&
			   SampleState.TileIndexes.Y == SampleState.TileCounts.Y - 1 &&
			   SampleState.SpatialSampleIndex == SampleState.SpatialSampleCount - 1;
	}

	FORCEINLINE bool IsFirstTemporalSample() const
	{
		return SampleState.TemporalSampleIndex == 0;
	}

	FORCEINLINE bool IsLastTemporalSample() const
	{
		return SampleState.TemporalSampleIndex == SampleState.TemporalSampleCount - 1;
	}
};

struct MOVIERENDERPIPELINECORE_API FMoviePipelineMergerOutputFrame
{
public:
	FMoviePipelineMergerOutputFrame() {}
	virtual ~FMoviePipelineMergerOutputFrame() {};

	FMoviePipelineMergerOutputFrame& operator=(FMoviePipelineMergerOutputFrame&& InOther)
	{
		FrameOutputState = InOther.FrameOutputState;
		ExpectedRenderPasses = InOther.ExpectedRenderPasses;
		ImageOutputData = MoveTemp(InOther.ImageOutputData);
	
		return *this;
	}
	FMoviePipelineMergerOutputFrame(FMoviePipelineMergerOutputFrame&& InOther)
		: FrameOutputState(InOther.FrameOutputState)
		, ExpectedRenderPasses(InOther.ExpectedRenderPasses)
		, ImageOutputData(MoveTemp(InOther.ImageOutputData))
	{
	}

	bool HasDataFromMultipleCameras() const
	{
		TMap<FString, int32> CameraNameUseCounts;
		for(const FMoviePipelinePassIdentifier& PassIdentifier : ExpectedRenderPasses)
		{
			CameraNameUseCounts.FindOrAdd(PassIdentifier.CameraName) += 1;
		}

		// ToDo: This logic might get more complicated because of burn ins, etc. that don't have camera names, 
		// need to check.
		return CameraNameUseCounts.Num() > 1;
	}

	bool HasDataFromMultipleRenderPasses(const TArray<MoviePipeline::FCompositePassInfo>& InCompositedPasses) const
	{
		TMap<FString, int32> RenderPassUseCounts;
		for (const FMoviePipelinePassIdentifier& PassIdentifier : ExpectedRenderPasses)
		{
			RenderPassUseCounts.FindOrAdd(PassIdentifier.Name) += 1;
		}

		// Remove any render passes that will be composited on later
		for (const MoviePipeline::FCompositePassInfo& CompositePass : InCompositedPasses)
		{
			RenderPassUseCounts.Remove(CompositePass.PassIdentifier.Name);
		}

		return RenderPassUseCounts.Num() > 1;
	}

private:
	// Explicitly delete copy operators since we own unique data.
	// void operator=(const FMoviePipelineMergerOutputFrame&);
	// FMoviePipelineMergerOutputFrame(const FMoviePipelineMergerOutputFrame&);

public:
	FMoviePipelineFrameOutputState FrameOutputState;

	TArray<FMoviePipelinePassIdentifier> ExpectedRenderPasses;

	TMap<FMoviePipelinePassIdentifier, TUniquePtr<FImagePixelData>> ImageOutputData;
};


namespace MoviePipeline
{
	struct MOVIERENDERPIPELINECORE_API IMoviePipelineOverlappedAccumulator : public TSharedFromThis<IMoviePipelineOverlappedAccumulator>
	{
	};

	struct IMoviePipelineOutputMerger : public TSharedFromThis<IMoviePipelineOutputMerger>
	{
		virtual FMoviePipelineMergerOutputFrame& QueueOutputFrame_GameThread(const FMoviePipelineFrameOutputState& CachedOutputState) = 0;
		virtual void OnCompleteRenderPassDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData) = 0;
		virtual void OnSingleSampleDataAvailable_AnyThread(TUniquePtr<FImagePixelData>&& InData) = 0;
		virtual void AbandonOutstandingWork() = 0;
		virtual int32 GetNumOutstandingFrames() const = 0;

		virtual ~IMoviePipelineOutputMerger()
		{
		}
	};

	struct FAudioState
	{
		struct FAudioSegment
		{
			FAudioSegment()
			{
				Id = FGuid::NewGuid();
			}

			FGuid Id;
			FMoviePipelineFrameOutputState OutputState;

			float NumChannels;
			float SampleRate;

			Audio::AlignedFloatBuffer SegmentData;
		};

		FAudioState()
			: bIsRecordingAudio(false)
			, PrevUnfocusedAudioMultiplier(1.f)
			, PrevRenderEveryTickValue(1)
		{}

		bool bIsRecordingAudio;

		float PrevUnfocusedAudioMultiplier;
		int32 PrevRenderEveryTickValue;

		/** Float Buffers for Movie Segments we've finished rendering. Stored until shutdown. Fully available during Finalize stage. */
		TArray<FAudioSegment> FinishedSegments;

		/** An array of active submixes we are recording for this shot. Gets cleared when recording stops on a shot. */
		TArray<TWeakPtr<Audio::FMixerSubmix, ESPMode::ThreadSafe>> ActiveSubmixes;
	};
}

USTRUCT(BlueprintType)
struct FMoviePipelineRenderPassOutputData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movie Pipeline")
	TArray<FString> FilePaths;
};

USTRUCT(BlueprintType)
struct FMoviePipelineShotOutputData
{
	GENERATED_BODY()

	/** Which shot was this output data for? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movie Pipeline")
	TWeakObjectPtr<UMoviePipelineExecutorShot> Shot;

	/** 
	* A mapping between render passes (such as 'FinalImage') and an array containing the files written for that shot.
	* Will be multiple files if using image sequences.
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movie Pipeline")
	TMap<FMoviePipelinePassIdentifier, FMoviePipelineRenderPassOutputData> RenderPassData;
};

namespace MoviePipeline
{
	struct FMoviePipelineOutputFutureData
	{
		FMoviePipelineOutputFutureData()
			: Shot(nullptr)
		{}

		UMoviePipelineExecutorShot* Shot;
		FString FilePath;
		FMoviePipelinePassIdentifier PassIdentifier;
	};
}

namespace UE
{
	namespace MoviePipeline
	{
		struct FViewportArgs
		{
			TSubclassOf<UMovieRenderDebugWidget> DebugWidgetClass;
			bool bRenderViewport = false;
		};
	}
}
/**
* Contains information about the to-disk output generated by a movie pipeline. This structure is used both for per-shot work finished
* callbacks and for the final render finished callback. When used as a per-shot callback ShotData will only have one entry (for the
* shot that was just finished), and for the final render callback it will have data for all shots that managed to render. Can be empty
* if the job failed to produce any files.
*/
USTRUCT(BlueprintType)
struct FMoviePipelineOutputData
{
	GENERATED_BODY()

	FMoviePipelineOutputData()
	: Pipeline(nullptr)
	, Job(nullptr)
	, bSuccess(false)
	{}
	
	/** 
	* The UMoviePipeline instance that generated this data. This is only provided as an id (in the event you were the one who created
	* the UMoviePipeline instance. DO NOT CALL FUNCTIONS ON THIS (unless you know what you're doing)
	*
	* Provided here for backwards compatibility.
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movie Pipeline")
	TObjectPtr<UMoviePipeline> Pipeline;
	
	/** Job the data is for. Job may still be in progress (if a shot callback) so be careful about modifying properties on it */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movie Pipeline")
	TObjectPtr<UMoviePipelineExecutorJob> Job;
	
	/** Did the job succeed, or was it canceled early due to an error (such as failure to write file to disk)? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movie Pipeline")
	bool bSuccess;

	/** 
	* The file data for each shot that was rendered. If no files were written this will be empty. If this is from the per-shot work
	* finished callback it will only have one entry (for the just finished shot). Will not include shots that did not get rendered
	* due to the pipeline encountering an error.
	*/
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Movie Pipeline")
	TArray<FMoviePipelineShotOutputData> ShotData;
};

/**
 * A pixel preprocessor for use with FImageWriteTask::PixelPreProcessor that does a simple alpha blend of the provided image onto the
 * target pixel data. This isn't very general purpose.
 */
template<typename PixelType> struct TAsyncCompositeImage;

template<>
struct TAsyncCompositeImage<FColor>
{
	TAsyncCompositeImage(TUniquePtr<FImagePixelData>&& InPixelData)
		: ImageToComposite(MoveTemp(InPixelData))
	{}

	void operator()(FImagePixelData* PixelData)
	{
		check(PixelData->GetType() == EImagePixelType::Color);
		check(ImageToComposite && ImageToComposite->GetType() == EImagePixelType::Color);
		if (!ensureMsgf(ImageToComposite->GetSize() == PixelData->GetSize(), TEXT("Cannot composite images of different sizes! Source: (%d,%d) Target: (%d,%d)"),
			ImageToComposite->GetSize().X, ImageToComposite->GetSize().Y, PixelData->GetSize().X, PixelData->GetSize().Y))
		{
			return;
		}


		TImagePixelData<FColor>* DestColorData = static_cast<TImagePixelData<FColor>*>(PixelData);
		TImagePixelData<FColor>* SrcColorData = static_cast<TImagePixelData<FColor>*>(ImageToComposite.Get());

		ParallelFor(DestColorData->GetSize().Y,
			[&](int32 ScanlineIndex = 0)
			{
				for (int64 ColumnIndex = 0; ColumnIndex < DestColorData->GetSize().X; ColumnIndex++)
				{
					int64 DstIndex = int64(ScanlineIndex) * int64(DestColorData->GetSize().X) + int64(ColumnIndex);

					FColor& Dst = DestColorData->Pixels[DstIndex];
					FColor& Src = SrcColorData->Pixels[DstIndex];

					float SourceAlpha = Src.A / 255.f;
					FColor Out;
					Out.A = FMath::Clamp(Src.A + FMath::RoundToInt(Dst.A * (1.f - SourceAlpha)), 0, 255);
					Out.R = FMath::Clamp(Src.R + FMath::RoundToInt(Dst.R * (1.f - SourceAlpha)), 0, 255);
					Out.G = FMath::Clamp(Src.G + FMath::RoundToInt(Dst.G * (1.f - SourceAlpha)), 0, 255);
					Out.B = FMath::Clamp(Src.B + FMath::RoundToInt(Dst.B * (1.f - SourceAlpha)), 0, 255);
					Dst = Out;
				}
			});
	}

	TUniquePtr<FImagePixelData> ImageToComposite;
};

template<>
struct TAsyncCompositeImage<FFloat16Color>
{
	TAsyncCompositeImage(TUniquePtr<FImagePixelData>&& InPixelData)
		: ImageToComposite(MoveTemp(InPixelData))
	{}

	void operator()(FImagePixelData* PixelData)
	{
		check(PixelData->GetType() == EImagePixelType::Float16);
		check(ImageToComposite && ImageToComposite->GetType() == EImagePixelType::Color);
		if (!ensureMsgf(ImageToComposite->GetSize() == PixelData->GetSize(), TEXT("Cannot composite images of different sizes! Source: (%d,%d) Target: (%d,%d)"),
			ImageToComposite->GetSize().X, ImageToComposite->GetSize().Y, PixelData->GetSize().X, PixelData->GetSize().Y))
		{
			return;
		}

		TImagePixelData<FFloat16Color>* DestColorData = static_cast<TImagePixelData<FFloat16Color>*>(PixelData);
		TImagePixelData<FColor>* SrcColorData = static_cast<TImagePixelData<FColor>*>(ImageToComposite.Get());
		ParallelFor(DestColorData->GetSize().Y,
			[&](int32 ScanlineIndex = 0)
			{
				for (int64 ColumnIndex = 0; ColumnIndex < DestColorData->GetSize().X; ColumnIndex++)
				{
					int64 DstIndex = int64(ScanlineIndex) * int64(DestColorData->GetSize().X) + int64(ColumnIndex);
					FFloat16Color& Dst = DestColorData->Pixels[DstIndex];
					FColor& Src = SrcColorData->Pixels[DstIndex];

					float SourceAlpha = Src.A / 255.f;
					FFloat16Color Out;
					Out.A = (Src.A / 255.f) + (Dst.A * (1.f - SourceAlpha));
					Out.R = (Src.R / 255.f) + (Dst.R * (1.f - SourceAlpha));
					Out.G = (Src.G / 255.f) + (Dst.G * (1.f - SourceAlpha));
					Out.B = (Src.B / 255.f) + (Dst.B * (1.f - SourceAlpha));
					Dst = Out;
				}
			});
	}

	TUniquePtr<FImagePixelData> ImageToComposite;
};

template<>
struct TAsyncCompositeImage<FLinearColor>
{
	TAsyncCompositeImage(TUniquePtr<FImagePixelData>&& InPixelData)
		: ImageToComposite(MoveTemp(InPixelData))
	{}

	void operator()(FImagePixelData* PixelData)
	{
		check(PixelData->GetType() == EImagePixelType::Float32);
		check(ImageToComposite && ImageToComposite->GetType() == EImagePixelType::Color);
		if (!ensureMsgf(ImageToComposite->GetSize() == PixelData->GetSize(), TEXT("Cannot composite images of different sizes! Source: (%d,%d) Target: (%d,%d)"),
			ImageToComposite->GetSize().X, ImageToComposite->GetSize().Y, PixelData->GetSize().X, PixelData->GetSize().Y))
		{
			return;
		}

		TImagePixelData<FLinearColor>* DestColorData = static_cast<TImagePixelData<FLinearColor>*>(PixelData);
		TImagePixelData<FColor>* SrcColorData = static_cast<TImagePixelData<FColor>*>(ImageToComposite.Get());

		ParallelFor(DestColorData->GetSize().Y,
			[&](int32 ScanlineIndex = 0)
			{
				for (int64 ColumnIndex = 0; ColumnIndex < DestColorData->GetSize().X; ColumnIndex++)
				{
					int64 DstIndex = int64(ScanlineIndex) * int64(DestColorData->GetSize().X) + int64(ColumnIndex);
					FLinearColor& Dst = DestColorData->Pixels[DstIndex];
					FColor& Src = SrcColorData->Pixels[DstIndex];

					float SourceAlpha = Src.A / 255.f;
					FLinearColor Out;
					Out.A = (Src.A / 255.f) + (Dst.A * (1.f - SourceAlpha));
					Out.R = (Src.R / 255.f) + (Dst.R * (1.f - SourceAlpha));
					Out.G = (Src.G / 255.f) + (Dst.G * (1.f - SourceAlpha));
					Out.B = (Src.B / 255.f) + (Dst.B * (1.f - SourceAlpha));
					Dst = Out;
				}
			});
	}

	TUniquePtr<FImagePixelData> ImageToComposite;
};
