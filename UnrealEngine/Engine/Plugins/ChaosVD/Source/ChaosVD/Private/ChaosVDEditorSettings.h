// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ChaosVDRecording.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"
#include "Visualizers/ChaosVDJointConstraintsDataComponentVisualizer.h"
#include "Visualizers/ChaosVDParticleDataComponentVisualizer.h"
#include "Visualizers/ChaosVDSceneQueryDataComponentVisualizer.h"
#include "Visualizers/ChaosVDSolverCollisionDataComponentVisualizer.h"

#include "ChaosVDEditorSettings.generated.h"

class UChaosVDEditorSettings;
class UMaterial;

DECLARE_MULTICAST_DELEGATE_OneParam(FChaosVDSettingChaged, UChaosVDEditorSettings* CVDEditorSettingsObject)

UENUM()
enum class EChaosVDActorTrackingTarget
{
	/** Disable Camera Auto-Tracking */
	Disabled,
	/** Follow the current selected object */
	SelectedObject,
};

UENUM(meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EChaosVDGeometryVisibilityFlags : uint8
{
	/** Draws all geometry that is for Query Only */
	Query = 1 << 1,
	/** Draws all geometry that is for [Physics Collision] or [Physics Collision and Query only] */
	Simulated = 1 << 2,
	/** Draws all simple geometry */
	Simple = 1 << 3,
	/** Draws all complex geometry */
	Complex = 1 << 4,
	/** Draws heightfields even if complex is not selected */
	ShowHeightfields = 1 << 5,
	/** Draws all particles that are in a disabled state */
	ShowDisabledParticles = 1 << 6,
	/** Draws all triangle lines that forms each geometry object. Similar to the wireframe view, but the polygons are till filled and shaded */
	ShowTriangleEdges = 1 << 7,
};
ENUM_CLASS_FLAGS(EChaosVDGeometryVisibilityFlags)

/** Structure holding the settings using to debug draw contact data on the Chaos Visual Debugger */
USTRUCT()
struct FChaosVDContactDebugDrawSettings
{
	GENERATED_BODY()

	/** The depth priority used for while drawing contact data. Can be World or Foreground (with this one the shapes will be drawn on top of the geometry and be always visible) */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	TEnumAsByte<ESceneDepthPriorityGroup> DepthPriority = ESceneDepthPriorityGroup::SDPG_World;

	/** The radius of the debug draw circle used to represent a contact point */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float ContactCircleRadius = 6.0f;

	/** The scale value to be applied to the normal vector of a contact used to change its size to make it easier to see */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float ContactNormalScale = 30.0f;
};

/** Structure holding the settings using to debug draw Particles shape based on their state on the Chaos Visual Debugger */
USTRUCT()
struct FChaosDebugDrawColorsByState
{
	GENERATED_BODY()

	/** Color used for dynamic particles */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor DynamicColor = FColor(255, 255, 0);
	
	/** Color used for sleeping particles */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor SleepingColor = FColor(128, 128, 128);

	/** Color used for kinematic particles */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor KinematicColor = FColor(0, 128, 255);

	/** Color used for static particles */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor StaticColor = FColor(255, 0, 0);

	FColor GetColorFromState(EChaosVDObjectStateType State) const;
};

USTRUCT()
struct FChaosParticleDataDebugDrawColors
{
	GENERATED_BODY()

	/** Color to apply to the Velocity vector when drawing it */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FLinearColor VelocityColor = FColor::Green;

	/** Color to apply to the Angular Velocity vector when drawing it */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FLinearColor AngularVelocityColor = FColor::Blue;

	/** Color to apply to the Acceleration vector when drawing it */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FLinearColor AccelerationColor = FColor::Orange;

	/** Color to apply to the Angular Acceleration vector when drawing it */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FLinearColor AngularAccelerationColor = FColor::Silver;

	/** Color to apply to the Linear Impulse when drawing it */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FLinearColor LinearImpulseColor = FColor::Turquoise;

	/** Color to apply to the Angular Impulse vector when drawing it */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FLinearColor AngularImpulseColor = FColor::Emerald;

	/** Color to apply the debug drawn sphere representing the center of mass location */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FLinearColor CenterOfMassColor = FColor::Red;

	/** Color to apply to when drawing the connectivity data */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FLinearColor ConnectivityDataColor = FColor::Yellow;
	
	FColor GetColorForDataID(EChaosVDParticleDataVisualizationFlags DataID, bool bIsSelected = false) const;
	const FLinearColor& GetLinearColorForDataID(EChaosVDParticleDataVisualizationFlags DataID) const;
};

USTRUCT()
struct FChaosParticleDataDebugDrawSettings
{
	GENERATED_BODY()

	/** The depth priority used for while drawing contact data. Can be World or Foreground (with this one the shapes will be drawn on top of the geometry and be always visible) */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	TEnumAsByte<ESceneDepthPriorityGroup> DepthPriority = ESceneDepthPriorityGroup::SDPG_World;

	/** Scale to apply to the Velocity vector before draw it. Unit is cm/s */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float VelocityScale = 0.5f;

	/** Scale to apply to the Angular Velocity vector before draw it. Unit is rad/s */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float AngularVelocityScale = 50.0f;

	/** Scale to apply to the Acceleration vector before draw it. Unit is cm/s2 */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float AccelerationScale = 0.005f;

	/** Scale to apply to the Angular Acceleration vector before draw it. Unit is rad/s2 */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float AngularAccelerationScale = 0.5f;

	/** Scale to apply to the Linear Impulse vector before draw it. Unit is g.m/s */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float LinearImpulseScale = 0.001;

	/** Scale to apply to the Angular Impulse vector before draw it. Unit is g.m2/s */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float AngularImpulseScale = 0.1f;

	/** Radius to use when creating the sphere that will represent the center of mass location */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float CenterOfMassRadius = 10.0f;

	float GetScaleFortDataID(EChaosVDParticleDataVisualizationFlags DataID) const;

	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FChaosParticleDataDebugDrawColors ColorSettings;
};

USTRUCT()
struct FChaosVDJointsDebugDrawSettings
{
	GENERATED_BODY()

	/** The depth priority used for while drawing data. Can be World or Foreground (with this one the shapes will be drawn on top of the geometry and be always visible) */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	TEnumAsByte<ESceneDepthPriorityGroup> DepthPriority = ESceneDepthPriorityGroup::SDPG_Foreground;

	/** Scale to apply to the Linear Impulse vector before draw it. */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float LinearImpulseScale = 0.001;

	/** Scale to apply to the Angular Impulse vector before draw it. */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float AngularImpulseScale = 0.1f;

	/** Scale to apply to anything that does not have a dedicated scale setting before draw it. */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float GeneralScale = 1.0f;

	/** Line thickness to use as a base to calculate the different line thickness values used to debug draw the data. */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float BaseLineThickness = 2.0f;

	/** Size of the debug drawn Center Of Mass. */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	float CenterOfMassSize = 1.0f;

	/** Size of the debug drawn if the Constraint Axis */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
    float ConstraintAxisLength = 10.0f;
};

/** Structure holding the settings using to debug draw Particles shape based on their shape type on the Chaos Visual Debugger */
USTRUCT()
struct FChaosDebugDrawColorsByShapeType
{
	GENERATED_BODY()

	/** Color used for Sphere, Plane, Cube, Capsule, Cylinder, tapered shapes */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor SimpleTypeColor = FColor(0, 255, 0); 

	/** Color used for convex shapes */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor ConvexColor = FColor(0, 255, 255);

	/** Color used for heightfield */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor HeightFieldColor = FColor(0, 0, 255);
	
	/** Color used for triangle meshes */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor TriangleMeshColor = FColor(255, 0, 0);

	/** Color used for triangle LevelSets */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor LevelSetColor = FColor(255, 0, 128);

	FColor GetColorFromShapeType(Chaos::EImplicitObjectType ShapeType) const;
};

/** Structure holding the settings using to debug draw Particles shape based on whether they are client or server objects (in PIE) Chaos Visual Debugger */
USTRUCT()
struct FChaosDebugDrawColorsByClientServer
{
	GENERATED_BODY()

	/** Color used for server shapes that are not awake or sleeping dynamic */
	UPROPERTY(EditAnywhere, Category=DebugDraw)
	FColor ServerColor = FColor(50, 0, 0); 

	/** Color used for server shapes that are awake dynamic */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	FColor ServerDynamicColor = FColor(150, 0, 0);

	/** Color used for server shapes that are sleeping dynamics */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	FColor ServerSleepingColor = FColor(10, 0, 0);

	/** Color used for client shapes that are not awake or sleeping dynamic */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	FColor ClientColor = FColor(0, 0, 50);

	/** Color used for server shapes that are awake dynamic */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	FColor ClientDynamicColor = FColor(0, 0, 150);

	/** Color used for client shapes that are sleeping dynamics */
	UPROPERTY(EditAnywhere, Category = DebugDraw)
	FColor ClientSleepingColor = FColor(0, 0, 100);

	FColor GetColorFromState(bool bIsServer, EChaosVDObjectStateType State) const;
};

UENUM()
enum class EChaosVDParticleDebugColorMode
{
	/** Draw particles with the default gray color */
	None,
	/** Draw particles with a specific color based on the recorded particle state */
	State,
	/** Draw particles with a specific color based on their shape type */
	ShapeType,
	/** Draw particles with a specific color based on if they are a Server Particle or Client particle */
	ClientServer,
};

UCLASS(config = Engine)
class UChaosVDEditorSettings : public UObject
{
	GENERATED_BODY()
public:

	/** If true, playback will respect the recorded frame times */
	UPROPERTY(EditAnywhere, Category = "Playback Settings")
	bool bPlaybackAtRecordedFrameRate = true;

	/** If play at recorded frame rate is disabled, CVD will attempt to play the recording at the specified frame rate */
	UPROPERTY(EditAnywhere, Category = "Playback Settings", meta=(EditCondition = "!bPlaybackAtRecordedFrameRate", EditConditionHides, ClampMin = 1))
	int32 TargetFrameRateOverride = 60;

	/** Sets the desired far clipping for CVD's viewport */
	UPROPERTY(EditAnywhere, Category = "Viewport", meta=(ClampMin = 1))
	float FarClippingOverride = 20000.0f;

	/** Set of flags to enable/disable visualization of specific particle data as debug draw */
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDParticleDataVisualizationFlags"))
	uint32 GlobalParticleDataVisualizationFlags = static_cast<uint32>(EChaosVDParticleDataVisualizationFlags::Velocity | EChaosVDParticleDataVisualizationFlags::AngularVelocity);

	/** Set of flags to enable/disable visualization of specific collision data as debug draw */
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDCollisionVisualizationFlags"))
	uint32 GlobalCollisionDataVisualizationFlags = static_cast<uint32>(EChaosVDCollisionVisualizationFlags::ContactInfo | EChaosVDCollisionVisualizationFlags::ContactPoints | EChaosVDCollisionVisualizationFlags::ContactNormal);

	/** Set of flags to enable/disable visualization of specific scene queries data as debug draw */
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDSceneQueryVisualizationFlags"))
	uint32 GlobalSceneQueriesVisualizationFlags = static_cast<uint32>(EChaosVDSceneQueryVisualizationFlags::DrawHits | EChaosVDSceneQueryVisualizationFlags::DrawLineTraceQueries);

	/** Set of flags to enable/disable visualization of specific scene queries data as debug draw */
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization Flags", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDJointsDataVisualizationFlags"))
	uint32 GlobalJointsDataVisualizationFlags = static_cast<uint32>(EChaosVDJointsDataVisualizationFlags::ActorConnector | EChaosVDJointsDataVisualizationFlags::DrawKinematic);

	/** If true, text information (if available) will be drawn alongside any other debug draw shape */
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization")
	bool bShowDebugText = false;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization",  meta=(EditCondition = "GlobalCollisionDataVisualizationFlags != 0", EditConditionHides))
	FChaosVDContactDebugDrawSettings ContactDebugDrawSettings;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization",  meta=(EditCondition = "GlobalParticleDataVisualizationFlags != 0", EditConditionHides))
	FChaosParticleDataDebugDrawSettings ParticleDataDebugDrawSettings;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization",  meta=(EditCondition = "GlobalParticleDataVisualizationFlags != 0", EditConditionHides))
	FChaosVDJointsDebugDrawSettings JointsDataDebugDrawSettings;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization")
	EChaosVDParticleDebugColorMode ParticleColorMode;
	
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization", meta=(EditCondition = "ParticleColorMode == EChaosVDParticleDebugColorMode::ShapeType", EditConditionHides))
	FChaosDebugDrawColorsByShapeType ColorsByShapeType;
	
	UPROPERTY(EditAnywhere, Category = "Viewport Visualization", meta=(EditCondition = "ParticleColorMode == EChaosVDParticleDebugColorMode::State", EditConditionHides))
	FChaosDebugDrawColorsByState ColorsByParticleState;

	UPROPERTY(EditAnywhere, Category = "Viewport Visualization", meta = (EditCondition = "ParticleColorMode == EChaosVDParticleDebugColorMode::ClientServer", EditConditionHides))
	FChaosDebugDrawColorsByClientServer ColorsByClientServer;

	/** Sets what should be auto tracked by CVD's Camera */
	UPROPERTY(EditAnywhere, Category = "Viewport Tracking")
	EChaosVDActorTrackingTarget TrackingTarget;

	/** By how much we should expand the bounding box used to track a target by bounding box. Used to see more of the screen while tracking in Bounding Box mode */
	UPROPERTY(EditAnywhere, Category = "Viewport Tracking", meta=(EditCondition = "TrackingTarget != EChaosVDActorTrackingTarget::Disabled"))
	float ExpandViewTrackingBy = 60.0f;

	/** Set of flags to enable/disable visibility of specific types of geometry/particles */
	UPROPERTY(EditAnywhere, Category = "Geometry Visibility", meta = (Bitmask, BitmaskEnum = "/Script/ChaosVD.EChaosVDGeometryVisibilityFlags"))
	uint8 GeometryVisibilityFlags = static_cast<uint8>(EChaosVDGeometryVisibilityFlags::Simulated | EChaosVDGeometryVisibilityFlags::Simple |  EChaosVDGeometryVisibilityFlags::ShowHeightfields);

	UPROPERTY(Config, Transient)
	TSoftObjectPtr<UMaterial> QueryOnlyMeshesMaterial;

	UPROPERTY(Config, Transient)
	TSoftObjectPtr<UMaterial> SimOnlyMeshesMaterial;

	UPROPERTY(Config, Transient)
	TSoftObjectPtr<UMaterial> InstancedMeshesMaterial;

	UPROPERTY(Config, Transient)
	TSoftObjectPtr<UMaterial> InstancedMeshesQueryOnlyMaterial;

	UPROPERTY(Config)
	FSoftClassPath SkySphereActorClass;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	FChaosVDSettingChaged& OnVisibilitySettingsChanged() { return VisibilitySettingsChangedDelegate; }

	FChaosVDSettingChaged& OnColorSettingsChanged() { return ColorsSettingsChangedDelegate; }

	FChaosVDSettingChaged& OnFarClippingOverrideChanged() { return FarClippingOverrideChangedDelegate; }

	FChaosVDSettingChaged& OnPlaybackSettingsChanged() { return PlaybackSettingsChangedDelegate ; }
	
	virtual void PostEditUndo() override;

protected:
	
	FChaosVDSettingChaged VisibilitySettingsChangedDelegate;
	FChaosVDSettingChaged ColorsSettingsChangedDelegate;
	FChaosVDSettingChaged FarClippingOverrideChangedDelegate;
	FChaosVDSettingChaged PlaybackSettingsChangedDelegate;
};
