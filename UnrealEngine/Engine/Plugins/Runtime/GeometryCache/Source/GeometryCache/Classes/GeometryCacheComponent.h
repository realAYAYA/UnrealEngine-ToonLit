// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/MeshComponent.h"

#include "GeometryCacheComponent.generated.h"

class UGeometryCache;
struct FGeometryCacheMeshData;

/** Stores the RenderData for each individual track */
USTRUCT()
struct FTrackRenderData
{
	GENERATED_USTRUCT_BODY()

	FTrackRenderData() : Matrix(FMatrix::Identity), BoundingBox(EForceInit::ForceInitToZero), MatrixSampleIndex(INDEX_NONE), BoundsSampleIndex(INDEX_NONE) {}

	/** Transform matrix used to render this specific track. 
		This goes from track local space to component local space.
	*/
	FMatrix Matrix;

	/** Bounding box of this specific track */
	FBox BoundingBox;

	/** Sample Id's of the values we currently have registered the component with. */
	int32 MatrixSampleIndex;
	int32 BoundsSampleIndex;
};

/** GeometryCacheComponent, encapsulates a GeometryCache asset instance and implements functionality for rendering/and playback of GeometryCaches */
UCLASS(ClassGroup = (Rendering), hidecategories = (Object, LOD), meta = (BlueprintSpawnableComponent))
class GEOMETRYCACHE_API UGeometryCacheComponent : public UMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** Required for access to (protected) TrackSections */
	friend class FGeometryCacheSceneProxy;
		
	//~ Begin UObject Interface
	virtual void BeginDestroy() override;
	virtual void FinishDestroy() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface

	//~ Begin UActorComponent Interface.
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;
	//~ End UActorComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	/** Update LocalBounds member from the local box of each section */
	void UpdateLocalBounds();
	//~ Begin USceneComponent Interface.	

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 MaterialIndex) const override;
	//~ End UMeshComponent Interface.


	/**
	* OnObjectReimported, Callback function to refresh section data and update scene proxy.
	*
	* @param ImportedGeometryCache
	*/
	void OnObjectReimported(UGeometryCache* ImportedGeometryCache);
	
public:
	/** Start playback of GeometryCache */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void Play();

	/** Start playback of GeometryCache from the start */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void PlayFromStart();

	/** Start playback of GeometryCache in reverse*/
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void PlayReversed();
	
	/** Start playback of GeometryCache from the end and play in reverse */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void PlayReversedFromEnd();

	/** Pause playback of GeometryCache */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void Pause();

	/** Stop playback of GeometryCache */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void Stop();

	/** Get whether this GeometryCache is playing or not. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	bool IsPlaying() const;

	/** Get whether this GeometryCache is playing in reverse or not. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	bool IsPlayingReversed() const;

	/** Get whether this GeometryCache is looping or not. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	bool IsLooping() const;

	/** Set whether this GeometryCache is looping or not. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void SetLooping( const bool bNewLooping);

	/** Get whether this GeometryCache is extrapolating frames. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	bool IsExtrapolatingFrames() const;

	/** Set whether this GeometryCache is extrapolating frames. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void SetExtrapolateFrames(const bool bNewExtrapolating);

	/** Get current playback speed for GeometryCache. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	float GetPlaybackSpeed() const;

	/** Set new playback speed for GeometryCache. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void SetPlaybackSpeed(const float NewPlaybackSpeed);

	/** Get the motion vector scale. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	float GetMotionVectorScale() const;

	/** Set new motion vector scale. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void SetMotionVectorScale(const float NewMotionVectorScale);

	/** Change the Geometry Cache used by this instance. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	bool SetGeometryCache( UGeometryCache* NewGeomCache );
	
	/** Getter for Geometry cache instance referred by the component
		Note: This getter is not exposed to blueprints as you can use the readonly Uproperty for that
	*/
	UGeometryCache* GetGeometryCache() const;

	/** Get current start time offset for GeometryCache. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	float GetStartTimeOffset() const;

	/** Set current start time offset for GeometryCache. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void SetStartTimeOffset(const float NewStartTimeOffset);

	/** Set the current animation time for GeometryCache. Includes the influence of elapsed time and SetStartTimeOffset */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	float GetAnimationTime() const;

	/** Set the current animation time for GeometryCache. Includes the influence of elapsed time and SetStartTimeOffset */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	float GetPlaybackDirection() const;

	/** Geometry Cache instance referenced by the component */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = GeometryCache)
	TObjectPtr<UGeometryCache> GeometryCache;
		
	/** Get the duration of the playback */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	float GetDuration() const;

	/** Get the number of frames */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	int32 GetNumberOfFrames() const;

	/** Override wireframe color? */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void SetOverrideWireframeColor(bool bOverride);

	/** Check whether we are overriding the wireframe color or not. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	bool GetOverrideWireframeColor() const;

	/** Set the color, used when overriding the wireframe color is enabled. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void SetWireframeOverrideColor(const FLinearColor Color);

	/** Get the wireframe override color, used when overriding the wireframe color is enabled. */
	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	FLinearColor GetWireframeOverrideColor() const;


public:
	/** Helper to get the frame of the ABC asset at this time*/
	int32 GetFrameAtTime(const float Time) const;

	/** Helper to get the time at this frame */
	float GetTimeAtFrame(const int32 Frame) const;

public:
	/** Functions to override the default TickComponent */
	void SetManualTick(bool bInManualTick);
	bool GetManualTick() const;

	void ResetAnimationTime();

	UFUNCTION(BlueprintCallable, Category = "Components|GeometryCache")
	void TickAtThisTime(const float Time, bool bInIsRunning, bool bInBackwards, bool bInIsLooping);

protected:
	/**
	* Invalidate both the Matrix and Mesh sample indices
	*/
	void InvalidateTrackSampleIndices();

	/**
	* ReleaseResources, clears and removes data stored/copied from GeometryCache instance	
	*/
	void ReleaseResources();

	/**
	* Updates the game thread state of a track section
	*
	* @param TrackIndex - Index of the track we want to update
	*/
	bool UpdateTrackSection(int32 TrackIndex, float Time);

	/**
	* CreateTrackSection, Create/replace a track section.
	*
	* @param TrackIndex - Index of the track to create (corresponds to an index on the geometry cache)
	*/
	void CreateTrackSection(int32 TrackIndex);

	/**
	* SetupTrackData
	* Call CreateTrackSection for all tracks in the GeometryCache assigned to this object.
	*/
	void SetupTrackData();

	/**
	* ClearTrackData
	* Clean up data that was required for playback of geometry cache tracks
	*/
	void ClearTrackData();

	UPROPERTY(EditAnywhere, Interp, Category = GeometryCache)
	bool bRunning;

	UPROPERTY(EditAnywhere, Interp, Category = GeometryCache)
	bool bLooping;

	/** Enable frame extrapolation for sub-frame sampling of non-constant topologies with imported motion vectors */
	UPROPERTY(EditAnywhere, Category = GeometryCache, AdvancedDisplay)
	bool bExtrapolateFrames;

	UPROPERTY(EditAnywhere, Interp, Category = GeometryCache, meta = (UIMin = "-14400.0", UIMax = "14400.0", ClampMin = "-14400.0", ClampMax = "14400.0"))
	float StartTimeOffset;

	UPROPERTY(EditAnywhere, Interp, Category = GeometryCache, meta = (UIMin = "0.0", UIMax = "4.0", ClampMin = "0.0", ClampMax = "512.0"))
	float PlaybackSpeed;

	/** Scale factor to apply to the imported motion vectors */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = GeometryCache, meta = (ClampMin = "0.0", ClampMax = "100", UIMin = "0.0", UIMax = "100"))
	float MotionVectorScale;

	UPROPERTY(VisibleAnywhere, Category = GeometryCache)
	int32 NumTracks;

	UPROPERTY(VisibleAnywhere, transient, Category = GeometryCache)
	float ElapsedTime;

	/** Component local space bounds of geometry cache mesh */
	FBoxSphereBounds LocalBounds;

	/** Array containing the TrackData (used for rendering) for each individual track*/
	TArray<FTrackRenderData> TrackSections;

	/** Play (time) direction, either -1.0f or 1.0f */
	float PlayDirection;

	/** Duration of the animation (maximum time) */
	UPROPERTY(BlueprintReadOnly, Category = GeometryCache)
	float Duration;

	UPROPERTY(EditAnywhere, Category = GeometryCache)
	bool bManualTick;

	/** Do we override the wireframe rendering color? */
	UPROPERTY(EditAnywhere, Category = GeometryCache)
	bool bOverrideWireframeColor;

	/** The wireframe override color. */
	UPROPERTY(EditAnywhere, Category = GeometryCache, meta = (EditCondition = "bOVerrideWireframeColor"))
	FLinearColor WireframeOverrideColor;
};
