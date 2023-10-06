// Copyright Epic Games, Inc. All Rights Reserved.

#include "Visualizers/ChaosVDParticleDataVisualizer.h"

#include "ChaosVDEditorSettings.h"
#include "ChaosVDModule.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "Visualizers/ChaosVDDebugDrawUtils.h"

void FChaosVDParticleDataVisualizer::DrawVisualization(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	uint32 VisualizationFlagsToUse = 0;
	if (static_cast<EChaosVDParticleDataVisualizationFlags>(LocalVisualizationFlags) == EChaosVDParticleDataVisualizationFlags::None)
	{
		if (const UChaosVDEditorSettings* EditorSettings = GetDefault<UChaosVDEditorSettings>())
		{
			VisualizationFlagsToUse = EditorSettings->GlobalParticleDataVisualizationFlags;
		}
		else
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] Failed to retrive global visualization setting. Falling back to local settings"), ANSI_TO_TCHAR(__FUNCTION__));

			VisualizationFlagsToUse = LocalVisualizationFlags;
		}	
	}
	else
	{
		VisualizationFlagsToUse = LocalVisualizationFlags;
	}
	
	FChaosVDVisualizationContext VisualizationContext;
	DataProvider.GetVisualizationContext(VisualizationContext);

	const FChaosVDParticleDataWrapper* ParticleDataViewer = DataProvider.GetParticleData();
	if (!ensure(ParticleDataViewer))
	{
		return;
	}

	// TODO: Implement scale settings for the vectors, and take into account the simulation space transform

	const FVector& OwnerLocation = ParticleDataViewer->ParticlePositionRotation.MX;

	if (ParticleDataViewer->ParticleVelocities.HasValidData())
	{
		if (EnumHasAnyFlags(static_cast<EChaosVDParticleDataVisualizationFlags>(VisualizationFlagsToUse), EChaosVDParticleDataVisualizationFlags::Velocity))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerLocation, OwnerLocation + ParticleDataViewer->ParticleVelocities.MV, TEXT("Velocity"), FColor::Green);
		}

		if (EnumHasAnyFlags(static_cast<EChaosVDParticleDataVisualizationFlags>(VisualizationFlagsToUse), EChaosVDParticleDataVisualizationFlags::AngularVelocity))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerLocation, OwnerLocation + ParticleDataViewer->ParticleVelocities.MW, TEXT("Angular Velocity"), FColor::Blue);
		}
	}

	if (ParticleDataViewer->ParticleDynamics.HasValidData())
	{
		if (EnumHasAnyFlags(static_cast<EChaosVDParticleDataVisualizationFlags>(VisualizationFlagsToUse), EChaosVDParticleDataVisualizationFlags::Acceleration))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerLocation, OwnerLocation + ParticleDataViewer->ParticleDynamics.MAcceleration, TEXT("Acceleration"), FColor::Orange);
		}
		
		if (EnumHasAnyFlags(static_cast<EChaosVDParticleDataVisualizationFlags>(VisualizationFlagsToUse), EChaosVDParticleDataVisualizationFlags::AngularAcceleration))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerLocation, OwnerLocation + ParticleDataViewer->ParticleDynamics.MAngularAcceleration, TEXT("Angular Acceleration"), FColor::Purple);
		}

		if (EnumHasAnyFlags(static_cast<EChaosVDParticleDataVisualizationFlags>(VisualizationFlagsToUse), EChaosVDParticleDataVisualizationFlags::LinearImpulse))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerLocation, OwnerLocation + ParticleDataViewer->ParticleDynamics.MLinearImpulseVelocity, TEXT("Linear Implulse Velocity"), FColor::Purple);
		}

		if (EnumHasAnyFlags(static_cast<EChaosVDParticleDataVisualizationFlags>(VisualizationFlagsToUse), EChaosVDParticleDataVisualizationFlags::AngularImpulse))
		{
			FChaosVDDebugDrawUtils::DrawArrowVector(PDI, OwnerLocation, OwnerLocation + ParticleDataViewer->ParticleDynamics.MAngularImpulseVelocity, TEXT("Angular Implulse Velocity"), FColor::Emerald);
		}
	}
}
