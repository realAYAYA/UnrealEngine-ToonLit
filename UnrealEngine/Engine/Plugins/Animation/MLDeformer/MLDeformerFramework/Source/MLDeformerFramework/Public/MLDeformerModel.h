// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"
#include "BoneContainer.h"
#include "RenderCommandFence.h"
#include "RenderResource.h"
#include "NeuralNetwork.h"
#include "Animation/AnimSequence.h"
#include "MLDeformerCurveReference.h"
#include "MLDeformerModel.generated.h"

class UMLDeformerAsset;
class UMLDeformerVizSettings;
class UMLDeformerModelInstance;
class UMLDeformerComponent;
class UMLDeformerInputInfo;

namespace UE::MLDeformer
{
	/**
	 * The vertex map, but in a GPU buffer. 
	 * This map basically has a DCC vertex number for every render vertex.
	 * So if a cube requires 32 render vertices, there will be 32 ints inside this buffer, and each item in this buffer
	 * will in this specific example case contain a value between 0 and 7, as a cube has only 8 vertices.
	 */
	class FVertexMapBuffer
		: public FVertexBufferWithSRV
	{
	public:
		/**
		 * Initialize the GPU buffer based on some array with integers.
		 * @param InVertexMap The array of ints we want to store on the GPU. The size of this array should be equal to the number of render vertices of the skeletal mesh.
		 */
		void Init(const TArray<int32>& InVertexMap)	{ VertexMap = InVertexMap; }

	private:
		/**
		 * This does the actual render resource init, which means this creates and fills the buffer on the GPU.
		 * After it successfully initializes, it will empty our VertexMap member array to not store the data in both GPU memory and main memory.
		 */
		void InitRHI() override;

		/** The array of integers we want to store on the GPU. This buffer will be emptied after successfully calling InitRHI. */
		TArray<int32> VertexMap;
	};
}	// namespace UE::MLDeformer

DECLARE_EVENT_OneParam(UMLDeformerModel, FMLDeformerModelOnPostEditProperty, FPropertyChangedEvent&)

/**
 * The ML Deformer runtime model base class.
 * All models should be inherited from this class.
 **/
UCLASS(Abstract)
class MLDEFORMERFRAMEWORK_API UMLDeformerModel 
	: public UObject	
	, public IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FNeuralNetworkModifyDelegate);

	virtual ~UMLDeformerModel() = default;

	/**
	 * Initialize the ML Deformer model.
	 * This will update the DeformerAsset and InputInfo properties. It internally calls CreateInputInfo if no InputInfo has been set yet.
	 * @param InDeformerAsset The deformer asset that this model will be part of.
	 */
	virtual void Init(UMLDeformerAsset* InDeformerAsset);

	/**
	 * Initialize the data that should be stored on the GPU. 
	 * This base class will store the VertexMap on the GPU by initializing the VertexMapBuffer member.
	 */
	virtual void InitGPUData();

	/**
	 * Create the input info for this model.
	 * @return A pointer to the newly created input info.
	 */
	virtual UMLDeformerInputInfo* CreateInputInfo();

	/**
	 * Create a new instance of this model, to be used in combination with a specific component.
	 * @param Component The ML Deformer component that will own this instance.
	 * @return A pointer to the model instance.
	 */
	virtual UMLDeformerModelInstance* CreateModelInstance(UMLDeformerComponent* Component);

	/**
	 * Get the display name of this model.
	 * This will also define with what name this model will appear inside the UI.
	 * On default this will return the class name.
	 * @return The name of the model.
	 */
	virtual FString GetDisplayName() const;

	/**
	 * Defines whether this model supports bone transforms as input or not.
	 * On default this is set to return true as most models have bone rotations as inputs to the neural network.
	 * @result Returns true when this model supports bones, or false otherwise.
	 */
	virtual bool DoesSupportBones() const					{ return true; }

	/**
	 * Defines whether this model supports curves as inputs or not. A curve is just a single float value.
	 * On default this returns true.
	 * @result Returns true when this model supports curves, or false otherwise.
	 */
	virtual bool DoesSupportCurves() const					{ return true; }

	/**
	 * Check whether the neural network of this model should run on the GPU or not.
	 * This is false on default, which makes it a CPU based neural network.
	 * Some code internally that creates and initializes the neural network will use the return value of this method to mark it to be on CPU or GPU.
	 * @return Returns true if the neural network of this model should run on the GPU. False is returned when it should run on the CPU.
	 */
	virtual bool IsNeuralNetworkOnGPU() const				{ return false; }	// CPU neural network.

	/**
	 * Get the default deformer graph asset path that this model uses, or an empty string if it doesn't require any deformer graph.
	 * An example is some string like: "/DeformerGraph/Deformers/DG_LinearBlendSkin_Morph_Cloth_RecomputeNormals.DG_LinearBlendSkin_Morph_Cloth_RecomputeNormals".
	 * @return The asset path of the deformer graph that should be used on default. This can return an empty string when no deformer graph is required.
	 */
	virtual FString GetDefaultDeformerGraphAssetPath() const	{ return FString(); }

	/**
	 * Get the skeletal mesh that is used during training.
	 * You typically want to apply the ML Deformer on this specific skeletal mesh in your game as well.
	 * @return A pointer to the skeletal mesh.
	 */
	const USkeletalMesh* GetSkeletalMesh() const			{ return SkeletalMesh.Get(); }
	USkeletalMesh* GetSkeletalMesh()						{ return SkeletalMesh.Get(); }

	/**
	 * Set the skeletal mesh that this deformer uses.
	 * @param SkelMesh The skeletal mesh.
	 */
	void SetSkeletalMesh(USkeletalMesh* SkelMesh)			{ SkeletalMesh = SkelMesh; }


#if WITH_EDITORONLY_DATA
	/**
	 * Check whether this model currently has a training mesh setup or not.
	 * For example if there is say a GeometryCache as target mesh, it could check whether that property is a nullptr or not.
	 * @return Returns true when the training target mesh has been selected, otherwise false is returned.
	 */
	virtual bool HasTrainingGroundTruth() const				{ return false; }

	/**
	 * Sample the positions from the target (ground truth) mesh, at a specific time (in seconds).
	 * @param SampleTime The time to sample the positions at, in seconds.
	 * @param OutPositions The array that will receive the resulting vertex positions. This array will automatically be resized internally.
	 */
	virtual void SampleGroundTruthPositions(float SampleTime, TArray<FVector3f>& OutPositions) {}
#endif

#if WITH_EDITOR
	/**
	 * Update the cached number of vertices of both base and target meshes.
	 */ 
	virtual void UpdateCachedNumVertices();

	/**
	 * Update the cached number of vertices in the base mesh. 
	 * The number of vertices should be the number of DCC vertices, so not the number of render vertices.
	 * This just updates the NumBaseMeshVerts property.
	 */ 
	virtual void UpdateNumBaseMeshVertices();

	/**
	 * Update the cached number of target mesh vertices. Every model needs to implement this.
	 * The number of vertices should be the number of DCC vertices, so not the number of render vertices.
	 * This just updates the NumTargetMeshVerts property.
	 */ 
	virtual void UpdateNumTargetMeshVertices();

	/**
	 * Extract the number of imported (DCC) vertices from a skeletal mesh.
	 * @param SkeletalMesh The skeletal mesh to get the number of imported vertices for.
	 * @return The number of imported vertices, which is the vertex count as seen in the DCC.
	 */ 
	static int32 ExtractNumImportedSkinnedVertices(const USkeletalMesh* SkeletalMesh);
#endif

	// UObject overrides.
	virtual void Serialize(FArchive& Archive) override;
	virtual void PostLoad() override;
	virtual void BeginDestroy() override;
	virtual bool IsReadyForFinishDestroy() override;
	// ~END UObject overrides.

	// IBoneReferenceSkeletonProvider overrides.
	virtual USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override;
	// ~END IBoneReferenceSkeletonProvider overrides.

	/**
	 * Get the ML deformer asset that this model is part of.
	 * @return A pointer to the ML deformer asset.
	 */
	UMLDeformerAsset* GetDeformerAsset() const;

	/**
	 * Get the input information, which is information about the inputs to the deformer.
	 * Inputs are things like bone transforms and curve values.
	 * @return A pointer to the input info object.
	 */
	UMLDeformerInputInfo* GetInputInfo() const					{ return InputInfo.Get(); }

	/**
	 * Get the number of vertices in the base mesh (linear skinned skeletal mesh).
	 * This is the number of vertices in the DCC, so not the render vertex count.
	 * @return The number of imported vertices inside linear skinned skeletal mesh.
	 */
	int32 GetNumBaseMeshVerts() const							{ return NumBaseMeshVerts; }

	/**
	 * Get the number of vertices of the target mesh.
	 * This is the number of vertices in the DCC, so not the render vertex count.
	 * @return The number of imported vertices inside the target mesh.
	 */
	int32 GetNumTargetMeshVerts() const							{ return NumTargetMeshVerts; }

	/**
	 * The mapping that maps from render vertices into dcc vertices.
	 * The length of this array is the same as the number of render vertices in the skeletal mesh.
	 * For a cube with 32 render vertices, the item values would be between 0..7 as in the dcc the cube has 8 vertices.
	 * @return A reference to the array that contains the DCC vertex number for each render vertex.
	 */
	const TArray<int32>& GetVertexMap() const					{ return VertexMap; }

	/**
	 * Manually set the vertex map. This normally gets initialized automatically.
	 * @param Map The original vertex number indices for each render vertex.
	 * @see GetVertexMap.
	 */
	void SetVertexMap(const TArray<int32>& Map)					{ VertexMap = Map; }

	/**
	 * Get the GPU buffer of the VertexMap.
	 * @return A reference to the GPU buffer resource object that holds the vertex map.
	 * @see GetVertexMap
	 */
	const UE::MLDeformer::FVertexMapBuffer& GetVertexMapBuffer() const { return VertexMapBuffer; }

	/**
	 * Get the neural network that we have trained. This can return a nullptr when no network has been trained yet.
	 * This network is used during inference.
	 * @return A pointer to the neural network, or nullptr when the network has not yet been trained.
	 */
	UNeuralNetwork* GetNeuralNetwork() const					{ return NeuralNetwork.Get(); }

	/**
	 * Set the neural network object that we use during inference.
	 * @param InNeuralNetwork The new neural network to use inside this deformer model.
	 */
	void SetNeuralNetwork(UNeuralNetwork* InNeuralNetwork);

	/**
	 * Get the neural network modified delegate.
	 * This triggers when the neural network pointers changes.
	 * @return A reference to the delegate.
	 */
	FNeuralNetworkModifyDelegate& GetNeuralNetworkModifyDelegate() { return NeuralNetworkModifyDelegate; }

#if WITH_EDITORONLY_DATA
	// UObject overrides.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	// ~END UObject overrides.

	/**
	 * Initialize the vertex map.
	 * This will grab the skeletal mesh imported model and copy over the imported mapping from there.
	 * This will make sure the VertexMap member is initialized.
	 * @see GetVertexMap
	 */
	void InitVertexMap();

	/**
	 * Check whether we should include bone transforms as input to the model during training or not.
	 * @return Returns true when bone transfomations should be a part of the network inputs, during the training process.
	 */
	bool ShouldIncludeBonesInTraining() const					{ return bIncludeBones; }

	/**
	 * Set whether we want to include bones during training or not.
	 * This will make bone transforms part of the neural network inputs.
	 * @param bInclude Set to true if you wish bone transforms to be included during training and at inference time.
	 */
	void SetShouldIncludeBonesInTraining(bool bInclude)			{ bIncludeBones = bInclude; }

	/**
	 * Check whether we should include curve values as input to the model during training or not.
	 * Curve values are single floats.
	 * @return Returns true when curve values should be a part of the network inputs, during the training process.
	 */
	bool ShouldIncludeCurvesInTraining() const					{ return bIncludeCurves; }

	/**
	 * Set whether we want to include curves during training.
	 * This will make curves part of the neural network inputs.
	 * @param bInclude Set to true to include curves during training and inference time.
	 */
	void SetShouldIncludeCurvesInTraining(bool bInclude)		{ bIncludeCurves = bInclude; }

	/**
	 * The delegate that gets fired when a property value changes.
	 * @return A reference to the delegate.
	 */
	FMLDeformerModelOnPostEditProperty& OnPostEditChangeProperty() { return PostEditPropertyDelegate; }

	/**
	 * Get the visualization settings for this model. These settings are only used in the editor.
	 * Visualization settings contain settings for the left side of the ML Deformer asset editor, containing 
	 * settings like what the mesh spacing is, whether to draw labels, what the test anim sequence is, etc.
	 * @return A pointer to the visualization settings. You can cast this to the type specific for your model, in case you inherited from 
	 *         the UMLDeformerVizSettings base class. This never return a nullptr.
	 */
	UMLDeformerVizSettings* GetVizSettings() const				{ return VizSettings; }

	/**
	 * Get the animation sequence that is used during training.
	 * Each frame of this anim sequence will contain a training pose.
	 * @return A pointer to the animation sequence used for training.
	 */
	const UAnimSequence* GetAnimSequence() const				{ return AnimSequence.LoadSynchronous();  }
	UAnimSequence* GetAnimSequence()							{ return AnimSequence.LoadSynchronous(); }

	/**
	 * Get the maximum number of training frames to use during training.
	 * Your training anim sequence might contain say 10000 frames, but for quickly iterating you might
	 * want to train on only 2000 frames instead. You can do this by setting the maximum training frames to 2000.
	 * @return The max number of frames to use during training.
	 */
	int32 GetTrainingFrameLimit() const							{ return MaxTrainingFrames; }

	/**
	 * Set the maximum number of frames to train on.
	 * For example if your training data has 10000 frames, but you wish to only train on 2000 frames, you can set this to 2000.
	 * @param MaxNumFrames The maximum number of frames to train on.
	 */
	void SetTrainingFrameLimit(int32 MaxNumFrames)				{ MaxTrainingFrames = MaxNumFrames; }

	/**
	 * Get the target mesh alignment tranformation.
	 * This is a transformation that is applied to the vertex positions of the target mesh, before we calculate the deltas
	 * between the linear skinned mesh and the target mesh.
	 * This is useful when you imported target mesh that isn't scaled the same, or perhaps it is rotated 90 degrees over the x axis.
	 * The alignment transform is then used to correct this and align both base and target mesh.
	 * @return The alignment transformation. When set to Identity it will not do anything, which is its default.
	 */
	const FTransform& GetAlignmentTransform() const				{ return AlignmentTransform; }

	/**
	 * Set the alignment transform, which is the transform applied to the target mesh vertices, before calculating the deltas.
	 * @param Transform The transformation to apply.
	 * @see GetAlignmentTransform.
	 */
	void SetAlignmentTransform(const FTransform& Transform)		{ AlignmentTransform = Transform; }

	/**
	 * Get the list of bones that we configured to be included during training.
	 * A bone reference is basically just a name of a bone.
	 * This can be different from the bone list inside the InputInfo property though.
	 * Please keep in mind that the BoneIncludeList is what is setup in the UI, but the InputInfo contains 
	 * the actual list of what we trained on, so what we use during inference.
	 * A user could change this bone list after they trained a model, in which case the input info and this bone include list
	 * will contain different items.
	 * @return The list of bones that should be included when performing the next training session.
	 */
	TArray<FBoneReference>& GetBoneIncludeList()				{ return BoneIncludeList; }
	const TArray<FBoneReference>& GetBoneIncludeList() const	{ return BoneIncludeList; }

	/**
	 * Set the list of bones that should be included during training.
	 * This list is ignored if ShouldIncludeBonesInTraining() returns false.
	 * If the list is empty, all bones will be included.
	 * @param List The list of bones to include.
	 */
	void SetBoneIncludeList(const TArray<FBoneReference>& List)	{ BoneIncludeList = List; }

	/**
	 * Get the list of curves that we configured to be included during training.
	 * A curve reference is basically just a name of a curve.
	 * This can be different from the curve list inside the InputInfo property though.
	 * Please keep in mind that the CurveIncludeList is what is setup in the UI, but the InputInfo contains 
	 * the actual list of what we trained on, so what we use during inference.
	 * A user could change this curve list after they trained a model, in which case the input info and this curve include list
	 * will contain different items.
	 * @return The list of curves that should be included when performing the next training session.
	 */
	TArray<FMLDeformerCurveReference>& GetCurveIncludeList()				{ return CurveIncludeList; }
	const TArray<FMLDeformerCurveReference>& GetCurveIncludeList() const	{ return CurveIncludeList; }

	/**
	 * Set the list of curves that should be included during training.
	 * This list is ignored if ShouldIncludeCurvesInTraining() returns false.
	 * If the list is empty, all curves will be included.
	 * @param List The list of curves to include.
	 */
	void SetCurveIncludeList(const TArray<FMLDeformerCurveReference>& List)	{ CurveIncludeList = List; }

	/**
	 * Get the delta cutoff length. Deltas that have a length larger than this length will be set to zero.
	 * This can be useful when there are some vertices that due to incorrect data have a very long length.
	 * Skipping those deltas will prevent issues.
	 * @return The length after which deltas will be ignored. So anything delta length larger than this value will be ignored.
	 */
	float GetDeltaCutoffLength() const										{ return DeltaCutoffLength; }

	/**
	 * Set the delta cutoff length. Deltas that are larger than this length will be set to zero.
	 * This can be useful when there are some vertices that due to incorrect data have a very long length.
	 * Skipping those deltas will prevent issues.
	 * @param Length The new delta cutoff length.
	 */
	void SetDeltaCutoffLength(float Length)									{ DeltaCutoffLength = Length; }

	/**
	 * Set the visualization settings object.
	 * You need to call this in the constructor of your model.
	 * @param VizSettings The visualization settings object for this model.
	 */
	void SetVizSettings(UMLDeformerVizSettings* VizSettingsObject)			{ VizSettings = VizSettingsObject; }

	// Get property names.
	static FName GetSkeletalMeshPropertyName()			{ return GET_MEMBER_NAME_CHECKED(UMLDeformerModel, SkeletalMesh); }
	static FName GetShouldIncludeBonesPropertyName()	{ return GET_MEMBER_NAME_CHECKED(UMLDeformerModel, bIncludeBones); }
	static FName GetShouldIncludeCurvesPropertyName()	{ return GET_MEMBER_NAME_CHECKED(UMLDeformerModel, bIncludeCurves); }
	static FName GetAnimSequencePropertyName()			{ return GET_MEMBER_NAME_CHECKED(UMLDeformerModel, AnimSequence); }
	static FName GetAlignmentTransformPropertyName()	{ return GET_MEMBER_NAME_CHECKED(UMLDeformerModel, AlignmentTransform); }
	static FName GetBoneIncludeListPropertyName()		{ return GET_MEMBER_NAME_CHECKED(UMLDeformerModel, BoneIncludeList); }
	static FName GetCurveIncludeListPropertyName()		{ return GET_MEMBER_NAME_CHECKED(UMLDeformerModel, CurveIncludeList); }
	static FName GetMaxTrainingFramesPropertyName()		{ return GET_MEMBER_NAME_CHECKED(UMLDeformerModel, MaxTrainingFrames); }
	static FName GetDeltaCutoffLengthPropertyName()		{ return GET_MEMBER_NAME_CHECKED(UMLDeformerModel, DeltaCutoffLength); }
#endif	// #if WITH_EDITORONLY_DATA

protected:
	/**
	 * Set the training input information.
	 * @param Input A pointer to the input information object.
	 * @see CreateInputInfo
	 */
	void SetInputInfo(UMLDeformerInputInfo* Input)		{ InputInfo = Input; }

	/**
	 * Convert an array of floats to an array of Vector3's.
	 * A requirement is that the number of items in the float array is a multiple of 3 (xyz).
	 * The order of the float items must be like this: (x, y, z, x, y, z, x, y, z, ...).
	 * The OutVectorArray will be automatically resized internally.
	 * @param FloatArray The array of floats to build an array of Vector3's for.
	 * @param OutVectorArray The array that will contain the vectors instead of floats.
	 */
	void FloatArrayToVector3Array(const TArray<float>& FloatArray, TArray<FVector3f>& OutVectorArray);

	/**
	 * Set the number of vertices in the base mesh.
	 * This is the number of imported (dcc) vertices, so not render vertices.
	 * @param NumVerts The number of vertices.
	 */
	void SetNumBaseMeshVerts(int32 NumVerts)			{ NumBaseMeshVerts = NumVerts; }

	/**
	 * Set the number of vertices in the target mesh.
	 * This is the number of imported (dcc) vertices, so not render vertices.
	 * @param NumVerts The number of vertices.
	 */
	void SetNumTargetMeshVerts(int32 NumVerts)			{ NumTargetMeshVerts = NumVerts; }

private:
	/** The deformer asset that this model is part of. */
	TObjectPtr<UMLDeformerAsset> DeformerAsset = nullptr;

	/** The delegate that gets fired when a property is being modified. */
	FMLDeformerModelOnPostEditProperty PostEditPropertyDelegate;

	/** GPU buffers for Vertex Map. */
	UE::MLDeformer::FVertexMapBuffer VertexMapBuffer;

	/** Fence used in render thread cleanup on destruction. */
	FRenderCommandFence RenderResourceDestroyFence;

	/** Delegate that will be called immediately before the NeuralNetwork is changed. */
	FNeuralNetworkModifyDelegate NeuralNetworkModifyDelegate;

	/** Cached number of skeletal mesh vertices. */
	UPROPERTY()
	int32 NumBaseMeshVerts = 0;

	/** Cached number of target mesh vertices. */
	UPROPERTY()
	int32 NumTargetMeshVerts = 0;

	/** 
	 * The information about the neural network inputs. This contains things such as bone names and curve names.
	 */
	UPROPERTY()
	TObjectPtr<UMLDeformerInputInfo> InputInfo = nullptr;

	/** This is an index per vertex in the mesh, indicating the imported vertex number from the source asset. */
	UPROPERTY()
	TArray<int32> VertexMap;

	/** The neural network that is used during inference. */
	UPROPERTY()
	TObjectPtr<UNeuralNetwork> NeuralNetwork = nullptr;

	/** The skeletal mesh that represents the linear skinned mesh. */
	UPROPERTY(EditAnywhere, Category = "Base Mesh")
	TObjectPtr<USkeletalMesh> SkeletalMesh = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UMLDeformerVizSettings> VizSettings = nullptr;

	/** Specifies whether bone transformations should be included as inputs during the training process. */
	UPROPERTY(EditAnywhere, Category = "Inputs and Output")
	bool bIncludeBones = true;

	/** Specifies whether curve values (a float per curve) should be included as inputs during the training process. */
	UPROPERTY(EditAnywhere, Category = "Inputs and Output")
	bool bIncludeCurves = false;

	/**
	 * The animation sequence to apply to the base mesh. This has to match the animation of the target mesh's geometry cache. 
	 * Internally we force the Interpolation property for this motion to be "Step".
	 */
	UPROPERTY(EditAnywhere, Category = "Base Mesh")
	TSoftObjectPtr<UAnimSequence> AnimSequence = nullptr;

	/** The transform that aligns the Geometry Cache to the SkeletalMesh. This will mostly apply some scale and a rotation, but no translation. */
	UPROPERTY(EditAnywhere, Category = "Target Mesh")
	FTransform AlignmentTransform = FTransform::Identity;

	/** The bones to include during training. When none are provided, all bones of the Skeleton will be included. */
	UPROPERTY(EditAnywhere, Category = "Inputs and Output", meta = (EditCondition = "bIncludeBones"))
	TArray<FBoneReference> BoneIncludeList;

	/** The curves to include during training. When none are provided, all curves of the Skeleton will be included. */
	UPROPERTY(EditAnywhere, Category = "Inputs and Output", meta = (EditCondition = "bIncludeCurves"))
	TArray<FMLDeformerCurveReference> CurveIncludeList;

	/** The maximum numer of training frames (samples) to train on. Use this to train on a sub-section of your full training data. */
	UPROPERTY(EditAnywhere, Category = "Inputs and Output", meta = (ClampMin = "1"))
	int32 MaxTrainingFrames = 1000000;

	/**
	 * Sometimes there can be some vertices that cause some issues that cause deltas to be very long. We can ignore these deltas by setting a cutoff value. 
	 * Deltas that are longer than the cutoff value (in units), will be ignored and set to zero length. 
	 */
	UPROPERTY(EditAnywhere, Category = "Inputs and Output", meta = (ClampMin = "0.01", ForceUnits="cm"))
	float DeltaCutoffLength = 30.0f;
#endif
};
