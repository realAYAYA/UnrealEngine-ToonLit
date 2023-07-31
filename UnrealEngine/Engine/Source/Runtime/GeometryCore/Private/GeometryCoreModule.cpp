// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCoreModule.h"

#include "CompGeom/ExactPredicates.h"

#define LOCTEXT_NAMESPACE "FGeometryCoreModule"

DEFINE_LOG_CATEGORY(LogGeometry);

void FGeometryCoreModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	UE::Geometry::ExactPredicates::GlobalInit();
}

void FGeometryCoreModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGeometryCoreModule, GeometryCore)