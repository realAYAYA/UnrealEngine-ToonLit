// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDEditorSettings.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Chaos/ImplicitObjectType.h"
#include "Visualizers/ChaosVDParticleDataComponentVisualizer.h"

FColor FChaosDebugDrawColorsByState::GetColorFromState(EChaosVDObjectStateType State) const
{
	switch (State)
	{
		case EChaosVDObjectStateType::Sleeping:
			return SleepingColor;
		case EChaosVDObjectStateType::Kinematic:
			return KinematicColor;
		case EChaosVDObjectStateType::Static:
			return StaticColor;
		case EChaosVDObjectStateType::Dynamic:
			return DynamicColor;
		default:
			return FColor::Purple;
	}
}

FColor FChaosParticleDataDebugDrawColors::GetColorForDataID(EChaosVDParticleDataVisualizationFlags DataID, bool bIsSelected) const
{
	constexpr float DefaultIntensityFactor = 0.6f;
	constexpr float SelectedIntensityFactor = 1.0f;
	const float IntensityFactor = bIsSelected ? SelectedIntensityFactor : DefaultIntensityFactor;

	return (GetLinearColorForDataID(DataID) * IntensityFactor).ToFColorSRGB();
}

const FLinearColor& FChaosParticleDataDebugDrawColors::GetLinearColorForDataID(EChaosVDParticleDataVisualizationFlags DataID) const
{
	static FLinearColor InvalidColor = FColor::Purple;
	switch (DataID)
	{
	case EChaosVDParticleDataVisualizationFlags::Acceleration:
		return AccelerationColor;
	case EChaosVDParticleDataVisualizationFlags::Velocity:
		return VelocityColor;
	case EChaosVDParticleDataVisualizationFlags::AngularVelocity:
		return AngularVelocityColor;
	case EChaosVDParticleDataVisualizationFlags::AngularAcceleration:
		return AngularAccelerationColor;
	case EChaosVDParticleDataVisualizationFlags::LinearImpulse:
		return LinearImpulseColor;
	case EChaosVDParticleDataVisualizationFlags::AngularImpulse:
		return AngularImpulseColor;
	case EChaosVDParticleDataVisualizationFlags::ClusterConnectivityEdge:
		return ConnectivityDataColor;
	case EChaosVDParticleDataVisualizationFlags::CenterOfMass:
		return CenterOfMassColor;
	case EChaosVDParticleDataVisualizationFlags::None:
	case EChaosVDParticleDataVisualizationFlags::DrawDataOnlyForSelectedParticle:
	default:
		return InvalidColor;
	}
}


float FChaosParticleDataDebugDrawSettings::GetScaleFortDataID(EChaosVDParticleDataVisualizationFlags DataID) const
{
	switch (DataID)
    {
    	case EChaosVDParticleDataVisualizationFlags::Acceleration:
    		return AccelerationScale;
    	case EChaosVDParticleDataVisualizationFlags::Velocity:
    		return VelocityScale;
    	case EChaosVDParticleDataVisualizationFlags::AngularVelocity:
    		return AngularVelocityScale;
    	case EChaosVDParticleDataVisualizationFlags::AngularAcceleration:
    		return AngularAccelerationScale;
    	case EChaosVDParticleDataVisualizationFlags::LinearImpulse:
    		return LinearImpulseScale;
    	case EChaosVDParticleDataVisualizationFlags::AngularImpulse:
    		return AngularImpulseScale;
    	case EChaosVDParticleDataVisualizationFlags::ClusterConnectivityEdge:
    	case EChaosVDParticleDataVisualizationFlags::CenterOfMass:
    	case EChaosVDParticleDataVisualizationFlags::None:
    	case EChaosVDParticleDataVisualizationFlags::DrawDataOnlyForSelectedParticle:
    	default:
    		return 1.0f;
    }
}

FColor FChaosDebugDrawColorsByShapeType::GetColorFromShapeType(Chaos::EImplicitObjectType ShapeType) const
{
	switch(ShapeType)
	{
		case Chaos::ImplicitObjectType::Sphere:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::Box:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::Plane:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::Capsule:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::TaperedCylinder:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::Cylinder:
			return SimpleTypeColor;
		case Chaos::ImplicitObjectType::Convex:
			return ConvexColor;
		case Chaos::ImplicitObjectType::HeightField:
			return HeightFieldColor;
		case Chaos::ImplicitObjectType::TriangleMesh:
			return TriangleMeshColor;
		case Chaos::ImplicitObjectType::LevelSet:
			return LevelSetColor;			
		default:
			return FColor::Purple; 
	}
}

FColor FChaosDebugDrawColorsByClientServer::GetColorFromState(bool bIsServer, EChaosVDObjectStateType State) const
{
	switch (State)
	{
	case EChaosVDObjectStateType::Sleeping:
		return bIsServer ? ServerSleepingColor : ClientSleepingColor;
	case EChaosVDObjectStateType::Kinematic:
		return bIsServer ? ServerColor : ClientColor;
	case EChaosVDObjectStateType::Static:
		return bIsServer ? ServerColor : ClientColor;
	case EChaosVDObjectStateType::Dynamic:
		return bIsServer ? ServerDynamicColor : ClientDynamicColor;
	default:
		return FColor::Purple;
	}
}

void UChaosVDEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();
	if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, GeometryVisibilityFlags))
	{
		VisibilitySettingsChangedDelegate.Broadcast(this);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, ParticleColorMode)
			|| MemberPropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, ColorsByParticleState)
			|| MemberPropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, ColorsByShapeType))
	{
		ColorsSettingsChangedDelegate.Broadcast(this);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, FarClippingOverride))
	{
		FarClippingOverrideChangedDelegate.Broadcast(this);
	}
	else if (MemberPropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, bPlaybackAtRecordedFrameRate) ||
			 MemberPropertyName == GET_MEMBER_NAME_CHECKED(UChaosVDEditorSettings, TargetFrameRateOverride))
	{
		PlaybackSettingsChangedDelegate.Broadcast(this);
	}

	// TODO: If we keep this object as the main setting object,
	// we should have a single event for what changed and an enum flags that the listener could use to decide if cares about the change
}

void UChaosVDEditorSettings::PostEditUndo()
{
	UObject::PostEditUndo();

	// This is not ideal, but we don't get what property was changed in the post edit undo callback.
	// A proper fix to avoid calling all the settings change delegates will be done when we split this settings object into several objects
	// and expose the options as proper UE menus instead of a details panel. Jira for tracking UE-206957 
	VisibilitySettingsChangedDelegate.Broadcast(this);
	ColorsSettingsChangedDelegate.Broadcast(this);
	PlaybackSettingsChangedDelegate.Broadcast(this);
	FarClippingOverrideChangedDelegate.Broadcast(this);
}
