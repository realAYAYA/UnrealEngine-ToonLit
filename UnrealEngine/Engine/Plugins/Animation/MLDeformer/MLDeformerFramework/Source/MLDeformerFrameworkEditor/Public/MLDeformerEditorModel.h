// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "MLDeformerEditorActor.h"
#include "Misc/FrameTime.h"
#include "MLDeformerVizSettings.h"
#include "MLDeformerModule.h"

class UMLDeformerModel;
class UMLDeformerInputInfo;
class AAnimationEditorPreviewActor;
class IPersonaPreviewScene;
class FEditorViewportClient;
class FSceneView;
class FViewport;
class FPrimitiveDrawInterface;
class UWorld;
class UMeshDeformer;
class USkeletalMesh;
class UAnimSequence;
class UMaterial;
class UGeometryCache;
class UNeuralNetwork;
class UMorphTarget;
class FMorphTargetVertexInfoBuffers;

/** Training process return codes. */
UENUM()
enum class ETrainingResult : uint8
{
	/** The training successfully finished. */
	Success = 0,

	/** The user has aborted the training process. */
	Aborted,

	/** The user has aborted the training process and we can't use the resulting network. */
	AbortedCantUse,

	/** The input or output data to the network has issues, which means we cannot train. */
	FailOnData,

	/** The python script has some error (see output log). Or it's missing some required model training class. */
	FailPythonError
};

namespace UE::MLDeformer
{
	class FMLDeformerEditorToolkit;
	class FMLDeformerSampler;
	class FMLDeformerEditorModel;

	/**
	 * The base class for the editor side of an UMLDeformerModel.
	 * The editor model class handles most of the editor interactions, such as property changes, triggering the training process,
	 * creation of the viewport actors, and many more things.
	 * In most cases you will want to inherit your own editor model from either FMLDeformerGeomCacheEditorModel or FMLDeformerMorphModelEditorModel though.
	 * These classes will already implement a lot of base functionality in case you work with geometry caches as ground truth data etc.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerEditorModel
		: public TSharedFromThis<FMLDeformerEditorModel>
		, public FGCObject
	{
	public:
		/** 
		 * The editor model initialization settings.
		 * This is used in the Init call.
		 */
		struct MLDEFORMERFRAMEWORKEDITOR_API InitSettings
		{
			FMLDeformerEditorToolkit* Editor = nullptr;
			UMLDeformerModel* Model = nullptr;
		};

		/**
		 * The destructor, which unregisters the instance of this editor model from the model registry.
		 * It also deletes the editor actors and unregisters from the OnPostEditChangeProperty delegate.
		 */
		virtual ~FMLDeformerEditorModel();

		// FGCObject overrides.
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FMLDeformerEditorModel"); }
		// ~END FGCObject overrides.

		// Required overrides.
		/**
		 * Get the number of training frames.
		 * This must not be clamped yet to the maximum training frame limit. It must return the full amount of frames available in your training data.
		 * For example this could return the number of frames inside your geometry cache.
		 * @return The number of frames in the training data.
		 */
		virtual int32 GetNumTrainingFrames() const;

		/**
		 * Launch the training. This gets executed when the Train button is pressed.
		 * Training can succeed, or be aborted, and there can be training errors. The result is returned by this method.
		 * You generally want to implement a class inherited from MLDeformerTrainingModel, and put a Train function inside of that, which you then execute through
		 * the TrainModel function.
		 * 
		 * A code example:
		 * 
		 * @code{.cpp}
		 * ETrainingResult FYourEditorModel::Train()
		 * {
		 *     return TrainModel<UYourTrainingModel>(this);
		 * }
		 * @endcode
		 * 
		 * That will internally call the Train function of your python class.
		 * Now inside your Python class you do something like:
		 * 
		 * @code{.py}
		 * @unreal.uclass()
		 * class YourModelPythonTrainingModel(unreal.YourTrainingModel):
		 *     @unreal.ufunction(override=True)
		 *     def train(self):
		 *         # ...do training here...
		 *         return 0   # A value of 0 is success, 1 means aborted, see ETrainingResult.
		 * @endcode
		 * 
		 * Calling the TrainModel function inside will then trigger the Python train method to be called.
		 * 
		 * @return The training result.
		 */
		virtual ETrainingResult Train();

		// Optional overrides.
		/**
		 * Initialize the model.
		 * This will mainly create the delta sampler, editor input info and some other things.
		 * @param Settings The initialization settings.
		 */
		virtual void Init(const InitSettings& Settings);

		/**
		 * Create the editor actors. These are the actors that will appear in the viewport of the asset editor.
		 * Every actor has some specific ID. The base implementation of certain methods will assume there are specific actors, such as
		 * a linear skinned actor, ML Deformed actor, and a ground truth one. You can find the ID values of those inside the FMLDeformerEditorActor.h file.
		 * The ID values are things like: ActorID_Train_Base, ActorID_Train_GroundTruth, ActorID_Test_Base, ActorID_Test_MLDeformed and ActorID_Test_GroundTruth.
		 * Nothing prevents you from adding more or less actors though.
		 * @param InPersonaPreviewScene The persona scene.
		 */
		virtual void CreateActors(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene);

		/**
		 * Clears the world, which basically means it removes all the editor actors and engine actors from the editor world.
		 * It also resets the Persona preview scene.
		 */
		virtual void ClearWorld();

		/**
		 * Create an actor in the editor viewport.
		 * Each actor will have a specific ID and a label that is displayed above it, with a specific color.
		 * The ID values are things like: ActorID_Train_Base, ActorID_Train_GroundTruth, ActorID_Test_Base, ActorID_Test_MLDeformed and ActorID_Test_GroundTruth.
		 * You can find the ID values of those inside the FMLDeformerEditorActor.h file.
		 * @param Settings The creation settings for the editor actor you are creating.
		 * @return A pointer to the editor actor object that was created.
		 */
		virtual FMLDeformerEditorActor* CreateEditorActor(const FMLDeformerEditorActor::FConstructSettings& Settings) const;

		/**
		 * Create the vertex delta sampler object.
		 * You can create your own sampler in case you use anything else than say a Geometry Cache as ground truth target data.
		 * A geometry cache based editor model would create a new FMLDeformerGeomCacheSampler for example.
		 * @return A pointer to the newly created sampler.
		 */
		virtual FMLDeformerSampler* CreateSampler() const;

		/**
		 * Get the time in seconds, for a given frame in the training data.
		 * @param FrameNumber The frame number to get the time in seconds for.
		 * @return The time offset of this frame, in seconds.
		 */
		virtual double GetTrainingTimeAtFrame(int32 FrameNumber) const;

		/**
		 * Get the training data frame number for a specific time offset in seconds.
		 * @param TimeInSeconds The time offset, in seconds, to get the frame number for.
		 * @return The frame number for this time offset.
		 */
		virtual int32 GetTrainingFrameAtTime(double TimeInSeconds) const;

		/**
		 * Get the time in seconds, for a given frame in the test data.
		 * @param FrameNumber The frame number to get the time in seconds for.
		 * @return The time offset of this frame, in seconds.
		 */
		virtual double GetTestTimeAtFrame(int32 FrameNumber) const;

		/**
		 * Get the test data frame number for a specific time offset in seconds.
		 * @param TimeInSeconds The time offset, in seconds, to get the frame number for.
		 * @return The frame number for this time offset.
		 */
		virtual int32 GetTestFrameAtTime(double TimeInSeconds) const;

		/**
		 * Get the total number of frames in the test anim sequence.
		 * @return The number of frames.
		 */
		virtual int32 GetNumTestFrames() const;

		/**
		 * Tick the editor viewport.
		 * @param ViewportClient The editor viewport client object.
		 * @param DeltaTime The delta time of this tick, in seconds.
		 */
		virtual void Tick(FEditorViewportClient* ViewportClient, float DeltaTime);

		/**
		 * Create the linear skinned actor that is used as training base.
		 * This is the actor you see marked as "Training Base" in the training mode.
		 * An editor actor needs to be created by this method.
		 * The default implementation will directly set the persona actor and preview mesh and component to this actor.
		 * @param InPersonaPreviewScene The personal preview scene.
		 */
		virtual void CreateTrainingLinearSkinnedActor(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene);

		/**
		 * Create the linear skinned test actor.
		 * This is the "Linear Skinned" actor you see in testing mode.
		 * An editor actor needs to be created by this method.
		 * @param World The world to create the actor in.
		 */
		virtual void CreateTestLinearSkinnedActor(UWorld* World);

		/**
		 * Create the test ML Deformed actor.
		 * This is the actor you see in test mode marked as "ML Deformed".
		 * So this actor will have the ML Deformer component added to it as well.
		 * An editor actor needs to be created by this method.
		 * @param World The world to create the actor in.
		 */
		virtual void CreateTestMLDeformedActor(UWorld* World);

		/**
		 * Create the training ground truth actor.
		 * This is the training target, so the actor that has the complex deformations, for example using a geometry cache.
		 * An editor actor needs to be created by this method.
		 * @param World The world to create the actor in.
		 */
		virtual void CreateTrainingGroundTruthActor(UWorld* World) {}

		/**
		 * Create the test ground truth actor.
		 * This is the actor typically marked as "Ground Truth" in the editor.
		 * It represents the test animation sequence ground truth deformation. So this could contain a geometry cache
		 * equivalent to the animation sequence.
		 * An editor actor needs to be created by this method.
		 * @param World The world to create the actor in.
		 */
		virtual void CreateTestGroundTruthActor(UWorld* World) {}

		/**
		 * This is triggered when the training frame is changed by the user.
		 * For example when they click the timeline in the editor, or enter a specific frame number.
		 * This happens only in training mode.
		 */
		virtual void OnTrainingDataFrameChanged();

		/**
		 * Update the transforms of all the editor actors.
		 * This should apply the mesh offsets to space the actors in the world/viewport.
		 */
		virtual void UpdateActorTransforms();

		/**
		 * Update the visibility status of the editor actors.
		 * This should hide test actors when the editor is in training mode for example, and the other way around.
		 */
		virtual void UpdateActorVisibility();

		/**
		 * Update the visibility of the labels, as well as their positions.
		 * For example labels of invisible actors should be made invisible as well.
		 */
		virtual void UpdateLabels();

		/**
		 * This is called when training data inputs change.
		 * This includes changing things such as the Skeletal Mesh, training anim sequence and target mesh.
		 * It will re-initialize the components. For example a skeletal mesh change will update the skeletal mesh component to the new mesh.
		 */
		virtual void OnInputAssetsChanged();

		/**
		 * This is executed after the input assets have changed.
		 * On default this will refresh the deformer graphs, update the timeline ranges, select the right frames, modify the training button ready state, etc.
		 */
		virtual void OnPostInputAssetChanged();

		/**
		 * This is called whenever a property value changes in the UI.
		 * The default implementation handles all the shared property changes already.
		 * You can overload this and call the base class method inside it, and then handle your own model specific property changes.
		 * @param PropertyChangedEvent The event information object that contains info about the property change.
		 */
		virtual void OnPropertyChanged(FPropertyChangedEvent& PropertyChangedEvent);

		/**
		 * Executed when the user presses the play button in the timeline.
		 * On default this will start playing the anim sequence related to whether we are in training or testing mode.
		 */
		virtual void OnPlayPressed();

		/**
		 * This is called just before we start the training process.
		 * You can use it to backup any specific values that might be modified by the training process.
		 * Sometimes you want to restore specific values when the user aborts the training, while the training already modified those values.
		 */
		virtual void OnPreTraining() {}

		/**
		 * This method is executed after training.
		 * @param TrainingResult The result of the training process, such as whether it was a success or the user aborted, etc.
		 * @param bUsePartiallyTrainedWhenAborted When the user aborts training, they can get offered to use the partially trained neural network. This boolean 
		 *                                        specifies whether they picked "Yes" (true) or "No" (false) to that question.
		 */
		virtual void OnPostTraining(ETrainingResult TrainingResult, bool bUsePartiallyTrainedWhenAborted);

		/**
		 * This is executed when the training process got aborted by the user.
		 * You can use this method to restore any values you had to backup during OnPreTraining for example.
		 * @param bUsePartiallyTrainedWhenAborted When the user aborts training, they can get offered to use the partially trained neural network. This boolean 
		 *                                        specifies whether they picked "Yes" (true) or "No" (false) to that question.
		 */
		virtual void OnTrainingAborted(bool bUsePartiallyTrainedData) {}

		/**
		 * Check whether we are playing an animation or not.
		 * This looks at whether we are in training or testing mode. It will look at the correct anim sequence based on this mode.
		 * So if you are in training mode and the testing animation is playing, while the training sequence isn't playing, then it returns false.
		 * It would in that case only return true when the training anim sequence is playing.
		 * The same goes in testing mode, if we are in testing mode it will only return true when the test anim sequence is playing.
		 * @return Returns true whether the mode specific animation sequence is playing.
		 */
		virtual bool IsPlayingAnim() const;

		/**
		 * Check whether our anim sequence is playing forward or backward.
		 * Just like the IsPlayingAnim method, it is specific to the mode we are in.
		 * @return Returns true when we are playing the animation forwards, and not backward.
		 */
		virtual bool IsPlayingForward() const;

		/**
		 * Get the current playback position of our training mode.
		 * This will use the target mesh if there is any, and uses the anim sequence play offset in case no target mesh has been set yet.
		 * So if you have a geometry cache or so specified as target mesh, it would extract the current time value from that.
		 * @return The time offset, in seconds, of our training data.
		 */
		virtual double CalcTrainingTimelinePosition() const;

		/**
		 * Calculate the test mode timeline position.
		 * This will use the ground truth test data if there is any, and uses the test anim sequence play offset in case no test ground truth has been set yet.
		 * So if you have a geometry cache or so specified as test ground truth, it would extract the current time value from that.
		 * @return The time offset, in seconds, of our test data.
		 */
		virtual double CalcTestTimelinePosition() const;

		/**
		 * This is executed when the timeline has been scrubbed.
		 * Sometimes this is also executed without user interaction, but to update the same data when there was scrubbed.
		 * @param NewScrubTime This is the new time value, in seconds.
		 * @param bIsScrubbing Specifies whether the user is scrubbing or just clicked.
		 */
		virtual void OnTimeSliderScrubPositionChanged(double NewScrubTime, bool bIsScrubbing);

		/**
		 * Update the playback speed of the test data.
		 * This will iterate over all the editor actors, check whether they are test mode actors, and if so
		 * it will update the play speed. For example a linear skinned mesh will update its skeletal mesh component's play speed, while
		 * a geometry cache ground truth actor would update the play speed set on the geometry cache component.
		 */
		virtual void UpdateTestAnimPlaySpeed();

		/**
		 * Clamp the current training frame index to be in a valid range.
		 * A valid range would be between 0 and the number of frames in the training data.
		 * So if the training data contains 100 frames, so index 0..99, and we set the TrainingFrameNumber member to 500, this method
		 * will turn that into 99.
		 */
		virtual void ClampCurrentTrainingFrameIndex();

		/**
		 * Clamp the current test frame index to be in a valid range.
		 * A valid range would be between 0 and the number of frames in the test data (anim sequence / ground truth).
		 * So if the test data contains 100 frames, so index 0..99, and we set the TestingFrameNumber member to 500, this method
		 * will turn that into 99.
		 */
		virtual void ClampCurrentTestFrameIndex();

		/**
		 * Get the total number of frames used for training, but limited by the maximum number of training frames we have setup.
		 * So if the training data contains 10000 frames, but we set to train on a maximum of 2000 frames, then this will return 2000.
		 * If the number of training frames is 10000 but we set the max number of training frames to 1 million, then it will still return only 10000.
		 * @return The number of frames we should be training on.
		 */
		virtual int32 GetNumFramesForTraining() const;

		/**
		 * Go to a given frame in the training data.
		 * Internally this will also call OnTimeSliderScrubPositionChanged, to go to the specific frame.
		 * @param FrameNumber The training frame to go to. This will automatically be clamped to a valid range in case it goes out of bounds.
		 */
		virtual void SetTrainingFrame(int32 FrameNumber);

		/**
		 * Go to a given frame in the test data.
		 * Internally this will also call OnTimeSliderScrubPositionChanged, to go to the specific frame.
		 * @param FrameNumber The test frame to go to. This will automatically be clamped to a valid range in case it goes out of bounds.
		 */
		virtual void SetTestFrame(int32 FrameNumber);

		/**
		 * Render additional (debug) things in the viewport.
		 * This will draw the vertex deltas when we are in training mode and aren't playing the training data.
		 * @param View The scene view object.
		 * @param Viewport The viewport object.
		 * @param PDI The draw primitive interface used for drawing things.
		 */
		virtual void Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);

		/**
		 * Update the state that is used to decide whether the Train button in the UI is enabled or not.
		 * One criteria could be to check whether we have all required inputs, such as a skeletal mesh, target mesh and anim sequence.
		 * This should set the bIsReadyForTraining bool to true when it is ready, or false if not.
		 */
		virtual void UpdateIsReadyForTrainingState() { bIsReadyForTraining = false; }

		/**
		 * Get the text that we should display as overlay on top of the asset editor viewport.
		 * @return The text, or an empty text object if there is nothing to show.
		 */
		virtual FText GetOverlayText() const;

		/**
		 * Initialize the input information, which basically describes what the inputs to the neural network are going to be.
		 * This contains things such as bone and curve names. The input info is used during both training and inference time.
		 * It is used to see what bone transforms and curve values to feed into the network inputs.
		 * Next to bones and curve reference it also stores the vertex counts of the base and target meshes, so we can later do some
		 * compatibility checks when applying this ML Deformer onto some character.
		 * @param InputInfo The input info object to initialize.
		 */
		virtual void InitInputInfo(UMLDeformerInputInfo* InputInfo);

		/**
		 * Call the SetupComponent on every ML Deformer component on our editor actors and update the compatibility status of the deformer components.
		 * This gets triggered when some input assets change.
		 */
		virtual void RefreshMLDeformerComponents();

		/**
		 * Create the heat map material for this model.
		 * This updates the HeatMapMaterial member.
		 */
		virtual void CreateHeatMapMaterial();

		/**
		 * Create the heat map deformer graph for this model.
		 * This updates the HeatMapDeformerGraph member.
		 */
		virtual void CreateHeatMapDeformerGraph();

		/**
		 * Create the heat map assets for this model.
		 * This calls CreateHeatMapMaterial and CreateHeatMapDeformerGraph on default.
		 */
		virtual void CreateHeatMapAssets();

		/**
		 * Specify whether the heat map material should be enabled or not.
		 * This will activate the heat map visualization, or deactivate it.
		 * @param bEnabled Set to true to enable heat map mode.
		 */
		virtual void SetHeatMapMaterialEnabled(bool bEnabled);

		/**
		 * Load the default Optimus deformer graph to use on the skeletal mesh when using this model.
		 * @return The mesh deformer graph.
		 */
		virtual UMeshDeformer* LoadDefaultDeformerGraph() const;

		/**
		 * This makes sure that the deformer graph selected in the visualization settings is not a nullptr.
		 * If the current graph hasn't been set, it will assign the default deformer graph to it.
		 */
		virtual void SetDefaultDeformerGraphIfNeeded();

		/**
		 * This applies the right mesh deformer to the editor actors that have an ML Deformer component on them.
		 * Basically this just ensures that the heat map deformer graph is active when this is enabled. Otherwise the default
		 * mesh deformer is used.
		 */
		virtual void UpdateDeformerGraph();

		/**
		 * Sample the vertex deltas between the training base model and target model.
		 * This will initialize the sampler if needed and set the sampler space to post-skinning deltas, and then samples them using the sampler.
		 * So this will update the internal state of the Sampler member. It will use the current training frame number to sample from.
		 * If this number is somehow out of range it will automatically clamp it to be inside a valid range.
		 */
		virtual void SampleDeltas();

		/**
		 * Load the trained network from an onnx file.
		 * The filename of the onnx file is determined by the GetTrainedNetworkOnnxFile method.
		 */
		virtual bool LoadTrainedNetwork() const;

		/**
		 * Get the onnx file name of the trained network.
		 * This is the filename that your python script will save the onnx file to.
		 * On default this will return the following filename: "<intermediatedir>/YourModel/YourModel.onnx".
		 * Where "intermediatedir" is your project's Intermediate folder. And "YourModel" is the name of your runtime model class.
		 * So if you have a model class named UFancyModel, on default this method will return "<intermediatedir>/FancyModel/FancyModel.onnx"
		 */
		virtual FString GetTrainedNetworkOnnxFile() const;

		/**
		 * Check whether our model is trained or not.
		 * This is determined by looking whether the neural network pointer is nullptr or not.
		 * @return Returns true when the model is trained already, or false if it hasn't been trained yet.
		 */
		virtual bool IsTrained() const;

		/**
		 * Get the editor actor that defines the timeline play position.
		 * In training mode this is the training target actor, while in test mode it is the test ground truth actor.
		 * @return A pointer to the ground truth actor depending on whether we're in test or training mode.
		 */
		virtual FMLDeformerEditorActor* GetTimelineEditorActor() const;

		/**
		 * Get the path to the heat map material asset.
		 * @return The path to the heat map asset.
		 */
		virtual FString GetHeatMapMaterialPath() const;

		/**
		 * Get the path to the heat map deformer graph asset.
		 * @return The path to the heat map deformer graph asset.
		 */
		virtual FString GetHeatMapDeformerGraphPath() const;

		/** Get the current view range. */
		TRange<double> GetViewRange() const;

		/** Set the current view range. */
		void SetViewRange(TRange<double> InRange);

		/** Get the working range of the model's data. */
		TRange<double> GetWorkingRange() const;

		/** Get the playback range of the model's data. */
		TRange<FFrameNumber> GetPlaybackRange() const;

		/** Get the current scrub position. */
		FFrameNumber GetTickResScrubPosition() const;

		/** Get how many ticks there are per frame. */
		int32 GetTicksPerFrame() const;

		/** Get the current scrub time. */
		float GetScrubTime() const;

		/** Set the current scrub position. */
		void SetScrubPosition(FFrameTime NewScrubPostion);

		/** Set the current scrub position. */
		void SetScrubPosition(FFrameNumber NewScrubPostion);

		/** Set if frames are displayed. */
		void SetDisplayFrames(bool bDisplayFrames);

		/** Is the timeline displaying frames rather than seconds? */
		bool IsDisplayingFrames() const;

		/** Handle the timeline changes when our model changes. */
		void HandleModelChanged();

		/** Handle the timeline changes required when changing the visualization mode, so when we switch between training and testing mode. */
		void HandleVizModeChanged(EMLDeformerVizMode Mode);

		/** Handle the view range being changed. */
		void HandleViewRangeChanged(TRange<double> InRange);

		/** Handle the working range being changed. */
		void HandleWorkingRangeChanged(TRange<double> InRange);

		/** Get the framerate specified by the anim sequence. */
		double GetFrameRate() const;

		/** Get the tick resolution we are displaying at. */
		int32 GetTickResolution() const;

		/** Get a pointer to the ML Deformer editor tookit. Cannot be a nullptr. */
		FMLDeformerEditorToolkit* GetEditor() const { return Editor; }

		/** Get a pointer to the runtime ML Deformer model. Cannot really be a nullptr. */
		UMLDeformerModel* GetModel() const { return Model.Get(); }

		/** Get a pointer to the world that our editor is displaying. Cannot be nullptr. */
		UWorld* GetWorld() const;

		/** Get all the created editor actors. */
		const TArray<FMLDeformerEditorActor*>& GetEditorActors() const { return EditorActors; }

		/** Find an editor actor with a specific ID, or return nullptr when not found. */
		FMLDeformerEditorActor* FindEditorActor(int32 ActorTypeID) const;

		/** Check whether we are ready to train. The Train button in the UI will be enabled or disabled based on this return value. */
		bool IsReadyForTraining() const { return bIsReadyForTraining; }

		/** Get the sampler we use to calculate vertex deltas when we are in training mdoe and enable deltas. */
		FMLDeformerSampler* GetSampler() const { return Sampler; }

		/**
		 * Set whether we need to resample inputs or not. This can be used by the training code to determine if we need to resample inputs and outputs, or if
		 * some cache can be used. Resampling is generally needed when input assets change, or other specific settings.
		 * @param bNeeded Set to true to mark that inputs or other cached data need resampling.
		 */
		void SetResamplingInputOutputsNeeded(bool bNeeded) { bNeedToResampleInputOutputs = bNeeded; }

		/**
		 * Check whether we need to resample inputs and outputs.
		 * @return Returns true if resampling of the inputs and outputs is needed, for example because input assets have changed.
		 * @see SetResamplingInputOutputsNeeded
		 */
		bool GetResamplingInputOutputsNeeded() const { return bNeedToResampleInputOutputs; }

		/**
		 * Check whether the vertex counts of the skeletal mesh have changed compared to the one we have trained on.
		 * @return An error string if there is an error, otherwise an empty text object is returned.
		 */
		FText GetBaseAssetChangedErrorText() const;

		/**
		 * Check whether the vertex map has changed or not.
		 * This basically checks whether the vertex order has changed.
		 * @return An error string if there is an error, otherwise an empty text object is returned.
		 */
		FText GetVertexMapChangedErrorText() const;

		/**
		 * Check for errors in the inputs. This basically checks if there are bones or curves to train on.
		 * If there aren't any it will give some error.
		 * @return An error string if there is an error, otherwise an empty text object is returned.
		 */
		FText GetInputsErrorText() const;

		/**
		 * Check for incompatible skeletons between the skeletal mesh and anim sequence skeleton.
		 * @return An error string if there is an error, otherwise an empty text object is returned.
		 */
		FText GetIncompatibleSkeletonErrorText(USkeletalMesh* InSkelMesh, UAnimSequence* InAnimSeq) const;

		/**
		 * Check to see if the skeletal mesh has to be reimported because it misses some newly added data.
		 * We have added imported mesh information to the Fbx importer. Older assets won't have this info, while this is required
		 * to make the ML Deformer work properly.
		 * @return An error string if there is an error, otherwise an empty text object is returned.
		 */
		FText GetSkeletalMeshNeedsReimportErrorText() const;

		/**
		 * Check whether the vertex count in the target mesh has changed or not.
		 * If this is the case you would need to re-train the model.
		 * @return An error string if there is an error, otherwise an empty text object is returned.
		 */
		FText GetTargetAssetChangedErrorText() const;

		/**
		 * Get the editor's input info object.
		 * This is the information the user currently has setup in the UI, so not the input info of the runtime model.
		 * The runtime model's input info contains the data inputs the model has been trained on, which can be different than what the user currently has setup in the UI.
		 * @return A pointer to the editor's input info object.
		 */
		UMLDeformerInputInfo* GetEditorInputInfo() const { return EditorInputInfo.Get(); }	

		/**
		 * Update the editor's input info based on the current UI settings.
		 */
		void UpdateEditorInputInfo();

		/**
		 * This will call OnInputAssetsChanged first, followed by OnPostInputAssetChanged.
		 * It then will update the editor only flags for the assets, and then forces a refresh of the model details panel and optionally also the visualization settings panel.
		 * @param RefreshVizSettings Set to true if you want the visualization settings details panel to also refresh.
		 */
		void TriggerInputAssetChanged(bool bRefreshVizSettings=false);

		/**
		 * Initialize the bone include list of the runtime model to the set of bones that are animated.
		 * A bone is seen as animated if its animation data has keyframes that have a different value than the first keyframe.
		 * So if the bone's animation data has 1000 keyframes, and they are all the same value, it won't see it as animated.
		 */
		void InitBoneIncludeListToAnimatedBonesOnly();

		/**
		 * Initialize the curve include list of the runtime model to the set of curves that are animated.
		 * A curve is seen as animated if its animation data has keyframes that have a different value than the first keyframe.
		 * So if the curve's animation data has 1000 keyframes, and they are all the same value, it won't see it as animated.
		 */
		void InitCurveIncludeListToAnimatedCurvesOnly();

		/**
		 * Get the number of curves on a specific skeletal mesh.
		 * @param SkelMesh The skeletal mesh to get the curves count from.
		 * @return The number of curves found on the skeletal mesh.
		 */
		int32 GetNumCurvesOnSkeletalMesh(USkeletalMesh* SkelMesh) const;

		/**
		 * Load a neural network from a given Onnx file.
		 * @param Filename The onnx file name.
		 * @return A pointer to the newly created neural network, or nullptr in case it failed to load.
		 */
		UNeuralNetwork* LoadNeuralNetworkFromOnnx(const FString& Filename) const;

		/**
		 * Get the currently desired training frame number.
		 * You can call CheckTrainingFrameChanged in order to make the desired frame the actual frame.
		 * @return The currenty desired training frame number, which might not the the same as the real training frame number.
		 */
		int32 GetCurrentTrainingFrame() const { return CurrentTrainingFrame; }

		/**
		 * Check whether our training data frame number changed.
		 * We have a TrainingFrameNumber and a CurrentTrainingFrame. If they don't match it will call OnTrainingDataFrameChanged.
		 */
		void CheckTrainingDataFrameChanged();

		/**
		 * Debug draw specific morph targets using lines and points.
		 * This can show the user what deltas are included in which morph target.
		 * @param PDI A pointer to the draw interface.
		 * @param MorphDeltas A buffer of deltas for ALL morph targets. The size of the buffer must be a multiple of Model->GetBaseNumVerts().
		 *        So the layout of this buffer is [Morph0_Deltas][Morph1_Deltas][Morph2_Deltas] etc.
		 * @param DeltaThreshold Deltas with a length  larger or equal to the given threshold value will be colored differently than the ones smaller than this threshold.
		 * @param MorphTargetIndex The morph target number to visualize.
		 * @param DrawOffset An offset to perform the debug draw at.
		 */
		void DrawMorphTarget(FPrimitiveDrawInterface* PDI, const TArray<FVector3f>& MorphDeltas, float DeltaThreshold, int32 MorphTargetIndex, const FVector& DrawOffset);

	protected:
		/**
		 * Executed when a property changes by a change in the UI.
		 * This internally calls OnPropertyChanged.
		 */
		void OnPostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

		/**
		 * Delete all editor actors.
		 * This does not actually delete the actors in the world.
		 * You can call ClearWorld to include that.
		 */
		void DeleteEditorActors();

		/**
		 * Perform some basic checks to see if the editor can be ready to train the model.
		 * This checks whether there is an anim sequence, target mesh, and skeletal mesh.
		 * It can be called inside the UpdateIsReadyForTrainingState method, which can then do additional checks afterwards.
		 * @return Returns true when at least the basic required inputs have been selected, false if not.
		 */
		bool IsEditorReadyForTrainingBasicChecks();

		/** Zero all deltas with a length equal to, or smaller than the threshold value. */
		void ZeroDeltasByThreshold(TArray<FVector3f>& Deltas, float Threshold);

		/**
		 * Generate engine morph targets from a set of deltas.
		 * @param OutMorphTargets The output array with generated morph targets. This array will be reset, and then filled with generated morph targets.
		 * @param Deltas The per vertex deltas for all morph targets, as one big buffer. Each morph target has 'GetNumBaseMeshVerts()' number of deltas.
		 * @param NamePrefix The morph target name prefix. If set to "MorphTarget_" the names will be "MorphTarget_000", "MorphTarget_001", "MorphTarget_002", etc.
		 * @param LOD The LOD index to generate the morphs for.
		 * @param DeltaThreshold Only include deltas with a length larger than this threshold in the morph targets.
		 */
		void CreateEngineMorphTargets(TArray<UMorphTarget*>& OutMorphTargets, const TArray<FVector3f>& Deltas, const FString& NamePrefix = TEXT("MorphTarget_"), int32 LOD = 0, float DeltaThreshold = 0.01f);

		/** 
		 * Compress morph targets into GPU based morph buffers.
		 * @param OutMorphBuffers The output compressed GPU based morph buffers. If this buffer is already initialized it will be released first.
		 * @param MorphTargets The morph targets to compress into GPU friendly buffers.
		 * @param LOD The LOD index to generate the morphs for.
		 * @param MorphErrorTolerance The error tolerance for the delta compression, in cm. Higher values compress better but can result in artifacts.
		 */
		void CompressEngineMorphTargets(FMorphTargetVertexInfoBuffers& OutMorphBuffers, const TArray<UMorphTarget*>& MorphTargets, int32 LOD = 0, float MorphErrorTolerance = 0.01f);

		/**
		 * Get the base actor depending on the currently active mode (train or test mode).
		 * The base actor is the linear skinned one.
		 * @return A pointer to the linear skinned actor in either test or training mode.
		 */
		FMLDeformerEditorActor* GetVisualizationModeBaseActor() const;

		/**
		 * Get the animation sequence, either the test or training one, depending on in which mode we are in.
		 * This can return a nullptr if no anim sequence has been setup yet.
		 * @return A pointer to the test sequence when we are in test mode, or the training anim sequence if we are in training mode.
		 */
		const UAnimSequence* GetAnimSequence() const;

		/**
		 * Calculate the current time line position offset, of the currently selected frame.
		 * This depends on whether we are in test or training mode.
		 * @return The time line offset, in seconds.
		 */
		double CalcTimelinePosition() const;

		/**
		 * Update the timeline related ranges, based on the length of the training or testing data.
		 */
		void UpdateRanges();

	protected:
		/** The runtime model associated with this editor model. */
		TObjectPtr<UMLDeformerModel> Model = nullptr;

		/** The set of actors that can appear inside the editor viewport. */
		TArray<FMLDeformerEditorActor*> EditorActors;

		/** A pointer to the editor toolkit. */
		FMLDeformerEditorToolkit* Editor = nullptr;

		/** A pointer to the sampler, which can sample target meshes to calculate deltas. */
		FMLDeformerSampler* Sampler = nullptr;

		/**
		 * The input info as currently setup in the editor.
		 * This is different from the runtime model's input info, as that is the one that was used to train with.
		 */
		TObjectPtr<UMLDeformerInputInfo> EditorInputInfo = nullptr;

		/** The heatmap material. */
		TObjectPtr<UMaterial> HeatMapMaterial = nullptr;

		/** The heatmap deformer graph. */
		TObjectPtr<UMeshDeformer> HeatMapDeformerGraph = nullptr;

		/** The delegate handle to the post edit property event. */
		FDelegateHandle PostEditPropertyDelegateHandle;

		/** The current training frame. */
		int32 CurrentTrainingFrame = -1;

		/** The range we are currently viewing */
		TRange<double> ViewRange;

		/** The working range of this model, encompassing the view range */
		TRange<double> WorkingRange;

		/** The playback range of this model for each timeframe */
		TRange<double> PlaybackRange;

		/** The current scrub position. */
		FFrameTime ScrubPosition;

		/**
		 * Are we ready for training? 
		 * The training button in the editor will be enabled or disabled based on this on default.
		 */
		bool bIsReadyForTraining = false;	

		/** Do we need to resample all input/output data? */
		bool bNeedToResampleInputOutputs = true;

		/** Display frame numbers? */
		bool bDisplayFrames = true;
	};

	/**
	 * Train the model.
	 * This will execute the train function inside your python code.
	 * Provide the UMLDeformerTrainingModel inherited class type as template argument.
	 */
	template<class TrainingModelClass>
	ETrainingResult TrainModel(FMLDeformerEditorModel* EditorModel)
	{
		// Find class, which will include our Python class, generated from the python script.
		TArray<UClass*> TrainingModels;
		GetDerivedClasses(TrainingModelClass::StaticClass(), TrainingModels);
		if (TrainingModels.IsEmpty())
		{
			// We didn't define a derived class in Python.
			return ETrainingResult::FailPythonError;
		}

		// Perform the training.
		// This will trigger the Python class train function to be called.
		TrainingModelClass* TrainingModel = Cast<TrainingModelClass>(TrainingModels.Last()->GetDefaultObject());
		TrainingModel->Init(EditorModel);
		const int32 ReturnCode = TrainingModel->Train();
		return static_cast<ETrainingResult>(ReturnCode);
	}
}	// namespace UE::MLDeformer
