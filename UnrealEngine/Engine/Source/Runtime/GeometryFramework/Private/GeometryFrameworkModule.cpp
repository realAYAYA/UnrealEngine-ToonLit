// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryFrameworkModule.h"
#include "Components/BaseDynamicMeshComponent.h"

#define LOCTEXT_NAMESPACE "FGeometryFrameworkModule"

void FGeometryFrameworkModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FGeometryFrameworkModule::OnPostEngineInit);
}

void FGeometryFrameworkModule::OnPostEngineInit()
{
	// UBaseDynamicMeshComponent provides some global materials to all instances, rather than
	// directly accessing (eg) GEngine pointers. Initialize those here. 
	UBaseDynamicMeshComponent::InitializeDefaultMaterials();
}

void FGeometryFrameworkModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGeometryFrameworkModule, GeometryFramework)