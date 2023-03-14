// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "EngineDefines.h"
#include "Engine/StaticMesh.h"
#include "CinematicExporter.h"
#include "MovieSceneSequenceID.h"
#include "Evaluation/MovieSceneSequenceTransform.h"
#include "MovieSceneFwd.h"
#include "FbxImporter.h"
#include "UObject/GCObject.h"
#include "Animation/AnimTypes.h"

class ABrush;
class ACameraActor;
class ALandscapeProxy;
class ALight;
class ASkeletalMeshActor;
class IMovieScenePlayer;
class UAnimSequence;
class UCameraComponent;
class UInstancedStaticMeshComponent;
class ULightComponent;
class UMaterialInterface;
class UModel;
class UMovieScene;
class UMovieSceneSkeletalAnimationTrack;
class UMovieScene3DTransformTrack;
class UMovieScenePropertyTrack;
class UMovieSceneTrack;
class USkeletalMesh;
class USkeletalMeshComponent;
class USplineMeshComponent;
class UStaticMeshComponent;
class FColorVertexBuffer;
class UFbxExportOption;
struct FAnimControlTrackKey;
struct FExpressionInput;
struct FMovieSceneDoubleChannel;
struct FMovieSceneFloatChannel;
struct FMovieSceneIntegerChannel;
struct FMovieSceneSequenceTransform;

namespace UnFbx
{
	/** Adapter interface which allows ExportAnimTrack to act on sequencer without a tight coupling. */
	class UNREALED_API IAnimTrackAdapter
	{
	public:
		virtual ~IAnimTrackAdapter() {};
		virtual int32 GetLocalStartFrame() const = 0;
		virtual int32 GetStartFrame() const { return GetLocalStartFrame(); }
		virtual int32 GetLength() const = 0;
		/** Updates the runtime state of the animation track to the specified frame. */
		virtual void UpdateAnimation(int32 LocalFrame) = 0;
		virtual float GetFrameRate() const { return 1.f / DEFAULT_SAMPLERATE; }
		/** The anim sequence that drives this anim track */
		virtual UAnimSequence* GetAnimSequence(int32 LocalFrame) const { return nullptr; }
		/** The time into the anim sequence for the given LocalFrame */
		virtual float GetAnimTime(int32 LocalFrame) const { return 0.f; }
	};

	/** An anim track adapter for a level sequence. */
	class UNREALED_API FLevelSequenceAnimTrackAdapter : public IAnimTrackAdapter
	{
	public:
		FLevelSequenceAnimTrackAdapter(IMovieScenePlayer* InMovieScenePlayer, UMovieScene* InMovieScene, const FMovieSceneSequenceTransform& InRootToLocalTransform, UMovieSceneSkeletalAnimationTrack* InAnimTrack = nullptr);
		virtual int32 GetLocalStartFrame() const override;
		virtual int32 GetStartFrame() const override;
		virtual int32 GetLength() const override;
		virtual void UpdateAnimation(int32 LocalFrame) override;
		virtual float GetFrameRate() const override;
		virtual UAnimSequence* GetAnimSequence(int32 LocalFrame) const override;
		virtual float GetAnimTime(int32 LocalFrame) const override;

	private:
		IMovieScenePlayer* MovieScenePlayer;
		UMovieScene* MovieScene;
		FMovieSceneSequenceTransform RootToLocalTransform;
		UMovieSceneSkeletalAnimationTrack* AnimTrack;
	};
/**
 * Main FBX Exporter class.
 */
class UNREALED_API FFbxExporter : public FCinematicExporter, public FGCObject
{
public:
	/**
	 * Returns the exporter singleton. It will be created on the first request.
	 */
	static FFbxExporter* GetInstance();
	static void DeleteInstance();
	~FFbxExporter();
	
	//~ FGCObject
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FFbxExporter");
	}

	/**
	* Load the export option from the last save state and show the dialog if bShowOptionDialog is true.
	* FullPath is the export file path we display in the dialog.
	* If the user cancels the dialog, the OutOperationCanceled will be true.
	* bOutExportAll will be true if the user wants to use the same option for all other assets they want to export.
	*
	* The function is saving the dialog state in a user ini file and reload it from there. It is not changing the CDO.
	*/
	void FillExportOptions(bool BatchMode, bool bShowOptionDialog, const FString& FullPath, bool& OutOperationCanceled, bool& bOutExportAll);

	/**
	* Custom set of export options instead of UI dialog. For automation.
	*/
	void SetExportOptionsOverride(UFbxExportOption* OverrideOptions);

	/**
	 * Creates and readies an empty document for export.
	 */
	virtual void CreateDocument();
	
	/**
	 * Closes the FBX document, releasing its memory.
	 */
	virtual void CloseDocument();
	
	/**
	 * Writes the FBX document to disk and releases it by calling the CloseDocument() function.
	 */
	virtual void WriteToFile(const TCHAR* Filename);
	
	/**
	 * Exports the light-specific information for a light actor.
	 */
	virtual void ExportLight( ALight* Actor, INodeNameAdapter& NodeNameAdapter );

	/**
	 * Exports the camera-specific information for a camera actor.
	 */
	virtual void ExportCamera( ACameraActor* Actor, bool bExportComponents, INodeNameAdapter& NodeNameAdapter );

	/**
	 * Exports the mesh and the actor information for a brush actor.
	 */
	virtual void ExportBrush(ABrush* Actor, UModel* InModel, bool bConvertToStaticMesh, INodeNameAdapter& NodeNameAdapter );

	/**
	 * Exports the basic scene information to the FBX document.
	 */
	virtual void ExportLevelMesh( ULevel* InLevel, bool bSelectedOnly, INodeNameAdapter& NodeNameAdapter , bool bSaveAnimSeq = true);

	/**
	* Exports the basic scene information to the FBX document, using the passed in Actors
	*/
	virtual void ExportLevelMesh(ULevel* InLevel, bool bExportLevelGeometry, TArray<AActor*>& ActorToExport, INodeNameAdapter& NodeNameAdapter, bool bSaveAnimSeq = true);


	/**
	 * Exports the given level sequence information into a FBX document.
	 *
	 * @return	true, if successful
	 */
	bool ExportLevelSequence(UMovieScene* MovieScene, const TArray<FGuid>& InBindings, IMovieScenePlayer* MovieScenePlayer, INodeNameAdapter& NodeNameAdapter, FMovieSceneSequenceIDRef SequenceID, const FMovieSceneSequenceTransform& RootToLocalTransform);

	/**
	 * Exports the given level sequence track information into a FBX document.
	 *
	 * @return	true, if successful
	 */
	bool ExportLevelSequenceTracks(UMovieScene* MovieScene, IMovieScenePlayer* MovieScenePlayer, FMovieSceneSequenceIDRef InSequenceID, FbxNode* FbxActor, UObject* BoundObject, const TArray<UMovieSceneTrack*>& Tracks, const FMovieSceneSequenceTransform& RootToLocalTransform);


	/**
	 * Exports the mesh and the actor information for a static mesh actor.
	 */
	virtual void ExportStaticMesh( AActor* Actor, UStaticMeshComponent* StaticMeshComponent, INodeNameAdapter& NodeNameAdapter );

	/**
	 * Exports a static mesh
	 * @param StaticMesh	The static mesh to export
	 * @param MaterialOrder	Optional ordering of materials to set up correct material ID's across multiple meshes being export such as BSP surfaces which share common materials. Should be used sparingly
	 */
	virtual void ExportStaticMesh( UStaticMesh* StaticMesh, const TArray<FStaticMaterial>* MaterialOrder = NULL );

	/**
	 * Exports BSP
	 * @param Model			 The model with BSP to export
	 * @param bSelectedOnly  true to export only selected surfaces (or brushes)
	 */
	virtual void ExportBSP( UModel* Model, bool bSelectedOnly );

	/**
	 * Exports a static mesh light map
	 */
	virtual void ExportStaticMeshLightMap( UStaticMesh* StaticMesh, int32 LODIndex, int32 UVChannel );

	/**
	 * Exports a skeletal mesh
	 */
	virtual void ExportSkeletalMesh( USkeletalMesh* SkeletalMesh );

	/**
	 * Exports the mesh and the actor information for a skeletal mesh actor.
	 */
	virtual void ExportSkeletalMesh( AActor* Actor, USkeletalMeshComponent* SkeletalMeshComponent, INodeNameAdapter& NodeNameAdapter );
	
	/**
	 * Exports the mesh and the actor information for a landscape actor.
	 */
	void ExportLandscape(ALandscapeProxy* Landscape, bool bSelectedOnly, INodeNameAdapter& NodeNameAdapter);

	/**
	 * Exports a single UAnimSequence, and optionally a skeletal mesh
	 */
	FbxNode* ExportAnimSequence( const UAnimSequence* AnimSeq, const USkeletalMesh* SkelMesh, bool bExportSkelMesh, const TCHAR* MeshNames=NULL, FbxNode* ActorRootNode=NULL, const TArray<UMaterialInterface*>* OverrideMaterials = nullptr);

	/** A node name adapter for a level sequence. */
	class UNREALED_API FLevelSequenceNodeNameAdapter : public INodeNameAdapter
	{
	public:
		FLevelSequenceNodeNameAdapter( UMovieScene* InMovieScene, IMovieScenePlayer* InMovieScenePlayer, FMovieSceneSequenceIDRef InSequenceID);
		virtual FString GetActorNodeName(const AActor* InActor) override;
		virtual void AddFbxNode(UObject* InObject, FbxNode* InFbxNode) override;
		virtual FbxNode* GetFbxNode(UObject* InObject) override;
	private:
		UMovieScene* MovieScene;
		IMovieScenePlayer* MovieScenePlayer;
		FMovieSceneSequenceID SequenceID;
		TMap<FGuid, FbxNode*> GuidToFbxNodeMap;
	};

	/* Get a valid unique name from a name */
	FString GetFbxObjectName(const FString &FbxObjectNode, INodeNameAdapter& NodeNameAdapter);

	/**
	 * Exports the basic information about an actor and buffers it.
	 * This function creates one FBX node for the actor with its placement.
	 */
	FbxNode* ExportActor(AActor* Actor, bool bExportComponents, INodeNameAdapter& NodeNameAdapter, bool bSaveAnimSeq = true);

private:
	FFbxExporter();

	static TSharedPtr<FFbxExporter> StaticInstance;

	FbxManager* SdkManager;
	FbxScene* Scene;
	FbxAnimStack* AnimStack;
	FbxAnimLayer* AnimLayer;
	FbxCamera* DefaultCamera;
	
	FFbxDataConverter Converter;
	
	TMap<FString,int32> FbxNodeNameToIndexMap;
	TMap<const AActor*, FbxNode*> FbxActors;
	TMap<const USkeletalMeshComponent*, FbxNode*> FbxSkeletonRoots;
	TMap<const UMaterialInterface*, FbxSurfaceMaterial*> FbxMaterials;
	TMap<const UStaticMesh*, FbxMesh*> FbxMeshes;
	TMap<const UStaticMesh*, FbxMesh*> FbxCollisionMeshes;

	/** The frames-per-second (FPS) used when baking transforms */
	static const float BakeTransformsFPS;
	
	/** Whether or not to export vertices unwelded */
	static bool bStaticMeshExportUnWeldedVerts;

	UFbxExportOption *ExportOptionsUI;
	UFbxExportOption *ExportOptionsOverride;

	/**
	* Export Anim Track of the given SkeletalMeshComponent
	*/
	void ExportAnimTrack( IAnimTrackAdapter& AnimTrackAdapter, AActor* Actor, USkeletalMeshComponent* SkeletalMeshComponent, float SamplingRate );

	void ExportModel(UModel* Model, FbxNode* Node, const char* Name);

	FbxNode* ExportCollisionMesh(const UStaticMesh* StaticMesh, const TCHAR* MeshName, FbxNode* ParentActor);

	/**
	 * Exports a static mesh
	 * @param StaticMesh	The static mesh to export
	 * @param MeshName		The name of the mesh for the FBX file
	 * @param FbxActor		The fbx node representing the mesh
	 * @param ExportLOD		The LOD of the mesh to export
	 * @param LightmapUVChannel Optional UV channel to export
	 * @param ColorBuffer	Vertex color overrides to export
	 * @param MaterialOrderOverride	Optional ordering of materials to set up correct material ID's across multiple meshes being export such as BSP surfaces which share common materials. Should be used sparingly
	 * @param OverrideMaterials	Optional array of materials to be used instead of the static mesh materials. Used for material overrides in static mesh components.
	 */
	FbxNode* ExportStaticMeshToFbx(const UStaticMesh* StaticMesh, int32 ExportLOD, const TCHAR* MeshName, FbxNode* FbxActor, int32 LightmapUVChannel = -1, const FColorVertexBuffer* ColorBuffer = NULL, const TArray<FStaticMaterial>* MaterialOrderOverride = NULL, const TArray<UMaterialInterface*>* OverrideMaterials = NULL);

	/**
	 * Exports a spline mesh
	 * @param SplineMeshComp	The spline mesh component to export
	 * @param MeshName		The name of the mesh for the FBX file
	 * @param FbxActor		The fbx node representing the mesh
	 */
	void ExportSplineMeshToFbx(const USplineMeshComponent* SplineMeshComp, const TCHAR* MeshName, FbxNode* FbxActor);

	/**
	 * Exports an instanced mesh
	 * @param InstancedMeshComp	The instanced mesh component to export
	 * @param MeshName		The name of the mesh for the FBX file
	 * @param FbxActor		The fbx node representing the mesh
	 */
	void ExportInstancedMeshToFbx(const UInstancedStaticMeshComponent* InstancedMeshComp, const TCHAR* MeshName, FbxNode* FbxActor);

	/**
	* Exports a landscape
	* @param Landscape		The landscape to export
	* @param MeshName		The name of the mesh for the FBX file
	* @param FbxActor		The fbx node representing the mesh
	*/
	void ExportLandscapeToFbx(ALandscapeProxy* Landscape, const TCHAR* MeshName, FbxNode* FbxActor, bool bSelectedOnly);

	/**
	* Fill an fbx light with from a unreal light component
	*@param ParentNode			The parent FbxNode the one over the light node
	* @param Camera				Fbx light object
	* @param CameraComponent	Unreal light component
	*/
	void FillFbxLightAttribute(FbxLight* Light, FbxNode* FbxParentNode, ULightComponent* BaseLight);

	/**
	* Fill an fbx camera with from a unreal camera component
	* @param ParentNode			The parent FbxNode the one over the camera node
	* @param Camera				Fbx camera object
	* @param CameraComponent	Unreal camera component
	*/
	void FillFbxCameraAttribute(FbxNode* ParentNode, FbxCamera* Camera, UCameraComponent *CameraComponent);

	/**
	 * Adds FBX skeleton nodes to the FbxScene based on the skeleton in the given USkeletalMesh, and fills
	 * the given array with the nodes created
	 */
	FbxNode* CreateSkeleton(const USkeletalMesh* SkelMesh, TArray<FbxNode*>& BoneNodes);

	/**
	 * Adds an Fbx Mesh to the FBX scene based on the data in the given FSkeletalMeshLODModel
	 * @param SkelMesh			The SkeletalMesh we are exporting
	 * @param MeshName			The SkeletalMesh name
	 * @param LODIndex			The mesh LOD index we are exporting
	 * @param AnimSeq			If an AnimSeq is provided and are exporting MorphTarget, the MorphTarget animation will be exported as well.
	 * @param OverrideMaterials Optional array of materials to be used instead of the skeletal mesh materials. Used for material overrides in skeletal mesh components.
	 */
	FbxNode* CreateMesh(const USkeletalMesh* SkelMesh, const TCHAR* MeshName, int32 LODIndex, const UAnimSequence* AnimSeq = nullptr, const TArray<UMaterialInterface*>* OverrideMaterials = nullptr);

	/**
	 * Adds Fbx Clusters necessary to skin a skeletal mesh to the bones in the BoneNodes list
	 */
	void BindMeshToSkeleton(const USkeletalMesh* SkelMesh, FbxNode* MeshRootNode, TArray<FbxNode*>& BoneNodes, int32 LODIndex);

	/**
	 * Add a bind pose to the scene based on the FbxMesh and skinning settings of the given node
	 */
	void CreateBindPose(FbxNode* MeshRootNode);

	/**
	 * Add the given skeletal mesh to the Fbx scene in preparation for exporting.  Makes all new nodes a child of the given node
	 */
	FbxNode* ExportSkeletalMeshToFbx(const USkeletalMesh* SkelMesh, const UAnimSequence* AnimSeq, const TCHAR* MeshName, FbxNode* ActorRootNode, const TArray<UMaterialInterface*>* OverrideMaterials = nullptr);

	/** Export SkeletalMeshComponent */
	void ExportSkeletalMeshComponent(USkeletalMeshComponent* SkelMeshComp, const TCHAR* MeshName, FbxNode* ActorRootNode, INodeNameAdapter& NodeNameAdapter, bool bSaveAnimSeq = true);

	/** Initializing the AnimStack playrate from the AnimSequence */
	bool SetupAnimStack(const UAnimSequence* AnimSeq);

	/**
	 * Add the given animation sequence as rotation and translation tracks to the given list of bone nodes
	 */
	void ExportAnimSequenceToFbx(const UAnimSequence* AnimSeq, const USkeletalMesh* SkelMesh, TArray<FbxNode*>& BoneNodes, FbxAnimLayer* AnimLayer,
		float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime);

	/**
	 * Add the custom Curve data to the FbxAnimCurves passed in parameter by matching their name to the skeletal mesh custom curves.
	 */
	void ExportCustomAnimCurvesToFbx(const TMap<FName, FbxAnimCurve*>& CustomCurves, const UAnimSequence* AnimSeq,
		float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime, float ValueScale = 1.f);

	/**
	 * Used internally to reuse the AnimSequence iteration code when exporting various kind of curves.
	 */
	void IterateInsideAnimSequence(const UAnimSequence* AnimSeq, float AnimStartOffset, float AnimEndOffset, float AnimPlayRate, float StartTime, TFunctionRef<void(float, FbxTime, bool)> IterationLambda);

	/** 
	 * The curve code doesn't differentiate between angles and other data, so an interpolation from 179 to -179
	 * will cause the bone to rotate all the way around through 0 degrees.  So here we make a second pass over the 
	 * rotation tracks to convert the angles into a more interpolation-friendly format.  
	 */
	void CorrectAnimTrackInterpolation( TArray<FbxNode*>& BoneNodes, FbxAnimLayer* AnimLayer );

	/**
	 * Exports a level sequence 3D transform track into the FBX animation stack.
	 */
	void ExportLevelSequence3DTransformTrack(FbxNode* FbxActor, IMovieScenePlayer* MovieScenePlayer, FMovieSceneSequenceIDRef InSequenceID, UMovieScene3DTransformTrack& TransformTrack, UObject* BoundObject, const TRange<FFrameNumber>& InPlaybackRange, const FMovieSceneSequenceTransform& RootToLocalTransform);

	/**
	 * Exports a level sequence 3D transform track that's getting baked (sample all sections) onto the FBX animation stack.
	 */
	void ExportLevelSequenceBaked3DTransformTrack(IAnimTrackAdapter& AnimTrackAdapter, FbxNode* FbxActor, IMovieScenePlayer* MovieScenePlayer, FMovieSceneSequenceIDRef InSequenceID, TArray<TWeakObjectPtr<UMovieScene3DTransformTrack> > TransformTracks, UObject* BoundObject, const TRange<FFrameNumber>& InPlaybackRange, const FMovieSceneSequenceTransform& RootToLocalTransform);

	/** 
	 * Exports a level sequence property track into the FBX animation stack. 
	 */
	void ExportLevelSequenceTrackChannels( FbxNode* FbxActor, UMovieSceneTrack& Track, const TRange<FFrameNumber>& InPlaybackRange, const FMovieSceneSequenceTransform& RootToLocalTransform);

	/** Defines value export modes for the EportRichCurveToFbxCurve method. */
	enum class ERichCurveValueMode
	{
		/** Export values directly */
		Default,
		/** Export fov values which get processed to focal length. */
		Fov
	};

	/** Generic implementation of exporting a movie scene bezier curve channel to an fbx animation curve */
	template<typename ChannelType>
	void ExportBezierChannelToFbxCurve(FbxAnimCurve& InFbxCurve, const ChannelType& InChannel, FFrameRate TickResolution, ERichCurveValueMode ValueMode, bool bNegative, const FMovieSceneSequenceTransform& RootToLocalTransform);

	/** Exports a movie scene float channel to an fbx animation curve. */
	void ExportChannelToFbxCurve(FbxAnimCurve& InFbxCurve, const FMovieSceneFloatChannel& InChannel, FFrameRate TickResolution, ERichCurveValueMode ValueMode = ERichCurveValueMode::Default, bool bNegative = false, const FMovieSceneSequenceTransform& RootToLocalTransform = FMovieSceneSequenceTransform());

	/** Exports a movie scene double channel to an fbx animation curve. */
	void ExportChannelToFbxCurve(FbxAnimCurve& InFbxCurve, const FMovieSceneDoubleChannel& InChannel, FFrameRate TickResolution, ERichCurveValueMode ValueMode = ERichCurveValueMode::Default, bool bNegative = false, const FMovieSceneSequenceTransform& RootToLocalTransform = FMovieSceneSequenceTransform());

	/** Exports a movie scene integer channel to an fbx animation curve. */
	void ExportChannelToFbxCurve(FbxAnimCurve& InFbxCurve, const FMovieSceneIntegerChannel& InChannel, FFrameRate TickResolution, const FMovieSceneSequenceTransform& RootToLocalTransform = FMovieSceneSequenceTransform());

	/**
	 * Finds the given actor in the already-exported list of structures
	 * @return FbxNode* the FBX node created from the UE actor
	 */
	FbxNode* FindActor(AActor* Actor, INodeNameAdapter* NodeNameAdapter = nullptr);

	/** Create fbx node with the given name */
	FbxNode* CreateNode(const FString& NodeName);

	/**
	 * Find bone array of FbxNOdes of the given skeletalmeshcomponent  
	 */
	bool FindSkeleton(USkeletalMeshComponent* SkelComp, TArray<FbxNode*>& BoneNodes, INodeNameAdapter* NodeNameAdapter = nullptr);

	/** recursively get skeleton */
	void GetSkeleton(FbxNode* RootNode, TArray<FbxNode*>& BoneNodes);

	bool FillFbxTextureProperty(const char *PropertyName, const FExpressionInput& MaterialInput, FbxSurfaceMaterial* FbxMaterial);
	/**
	 * Exports the profile_COMMON information for a material.
	 */
	FbxSurfaceMaterial* ExportMaterial(UMaterialInterface* Material);
	
	FbxSurfaceMaterial* CreateDefaultMaterial();
	
	/**
	 * Create user property in Fbx Node.
	 * Some Unreal animatable property can't be animated in FBX property. So create user property to record the animation of property.
	 *
	 * @param Node  FBX Node the property append to.
	 * @param Value Property value.
	 * @param Name  Property name.
	 * @param Label Property label.
	 */
	template<typename T>
	void CreateAnimatableUserProperty(FbxNode* Node, T Value, const char* Name, const char* Label, FbxDataType DataType = FbxFloatDT);

	/** Exports all the object's FBX metadata to the FBX node */
	void ExportObjectMetadata(const UObject* ObjectToExport, FbxNode* Node);

public:
	/** Returns currently active FBX export options. Automation or UI dialog based options. */
	UFbxExportOption* GetExportOptions();

	bool bSceneGlobalTimeLineSet = false;
};



} // namespace UnFbx
