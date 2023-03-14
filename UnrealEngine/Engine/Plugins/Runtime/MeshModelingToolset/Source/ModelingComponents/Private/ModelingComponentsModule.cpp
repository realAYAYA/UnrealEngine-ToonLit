// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModelingComponentsModule.h"
#include "Components/BaseDynamicMeshComponent.h"
#include "Materials/Material.h"

#define LOCTEXT_NAMESPACE "FModelingComponentsModule"

void FModelingComponentsModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FModelingComponentsModule::OnPostEngineInit);

	// Ensure that the GeometryFramework module is loaded so that UBaseDynamicMeshComponent materials are configured
	FModuleManager::Get().LoadModule(TEXT("GeometryFramework"));
}

void FModelingComponentsModule::OnPostEngineInit()
{
	// Replace the standard UBaseDynamicMeshComponent vertex color material with something higher quality.
	// This is done in ModelingComponents module (ie part of MeshModelingToolset plugin) to avoid having to
	// make this Material a special "engine material", which has various undesirable implication
	UMaterial* VertexColorMaterial = LoadObject<UMaterial>(nullptr, TEXT("/MeshModelingToolset/Materials/M_DynamicMeshComponentVtxColor"));
	if (ensure(VertexColorMaterial))
	{
		UBaseDynamicMeshComponent::SetDefaultVertexColorMaterial(VertexColorMaterial);
	}
}

void FModelingComponentsModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FModelingComponentsModule, ModelingComponents)