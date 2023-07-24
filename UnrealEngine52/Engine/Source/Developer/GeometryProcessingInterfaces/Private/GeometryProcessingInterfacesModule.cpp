// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryProcessingInterfacesModule.h"
#include "CoreGlobals.h"
#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

#include "GeometryProcessingInterfaces/ApproximateActors.h"


//DEFINE_LOG_CATEGORY_STATIC(LogMeshReduction, Verbose, All);

IMPLEMENT_MODULE(FGeometryProcessingInterfacesModule, GeometryProcessingInterfaces);


void FGeometryProcessingInterfacesModule::StartupModule()
{

}


void FGeometryProcessingInterfacesModule::ShutdownModule()
{
	ApproximateActors = nullptr;
}


IGeometryProcessing_ApproximateActors* FGeometryProcessingInterfacesModule::GetApproximateActorsImplementation()
{
	if (ApproximateActors == nullptr)
	{
		TArray<IGeometryProcessing_ApproximateActors*> ApproximateActorsOptions =
			IModularFeatures::Get().GetModularFeatureImplementations<IGeometryProcessing_ApproximateActors>(IGeometryProcessing_ApproximateActors::GetModularFeatureName());

		ApproximateActors = (ApproximateActorsOptions.Num() > 0) ? ApproximateActorsOptions[0] : nullptr;
	}

	return ApproximateActors;
}
