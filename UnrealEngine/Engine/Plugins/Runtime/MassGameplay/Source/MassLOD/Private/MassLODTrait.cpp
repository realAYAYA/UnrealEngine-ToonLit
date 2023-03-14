// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassLODTrait.h"
#include "MassEntityTemplateRegistry.h"
#include "Engine/World.h"
#include "MassCommonFragments.h"
#include "StructUtilsTypes.h"
#include "MassLODFragments.h"
#include "MassEntityUtils.h"


void UMassLODCollectorTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.AddFragment<FMassViewerInfoFragment>();
	BuildContext.AddTag<FMassCollectLODViewerInfoTag>();
	BuildContext.RequireFragment<FTransformFragment>();
}

void UMassSimulationLODTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	BuildContext.RequireFragment<FMassViewerInfoFragment>();
	BuildContext.RequireFragment<FTransformFragment>();

	FMassSimulationLODFragment& LODFragment = BuildContext.AddFragment_GetRef<FMassSimulationLODFragment>();

	// Start all simulation LOD in the Off 
	if(Params.bSetLODTags || bEnableVariableTicking)
	{
		LODFragment.LOD = EMassLOD::Off;
		BuildContext.AddTag<FMassOffLODTag>();
	}

	FMassEntityManager& EntityManager = UE::Mass::Utils::GetEntityManagerChecked(World);

	FConstSharedStruct ParamsFragment = EntityManager.GetOrCreateConstSharedFragment(Params);
	BuildContext.AddConstSharedFragment(ParamsFragment);

	uint32 ParamsHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(Params));
	FSharedStruct SharedFragment = EntityManager.GetOrCreateSharedFragmentByHash<FMassSimulationLODSharedFragment>(ParamsHash, Params);
	BuildContext.AddSharedFragment(SharedFragment);

	// Variable ticking from simulation LOD
	if(bEnableVariableTicking)
	{
		BuildContext.AddFragment<FMassSimulationVariableTickFragment>();
		BuildContext.AddChunkFragment<FMassSimulationVariableTickChunkFragment>();

		FConstSharedStruct VariableTickParamsFragment = EntityManager.GetOrCreateConstSharedFragment(VariableTickParams);
		BuildContext.AddConstSharedFragment(VariableTickParamsFragment);

		uint32 VariableTickParamsHash = UE::StructUtils::GetStructCrc32(FConstStructView::Make(VariableTickParams));
		FSharedStruct VariableTickSharedFragment = EntityManager.GetOrCreateSharedFragmentByHash<FMassSimulationVariableTickSharedFragment>(VariableTickParamsHash, VariableTickParams);
		BuildContext.AddSharedFragment(VariableTickSharedFragment);
	}
}
