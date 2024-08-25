// Copyright Epic Games, Inc. All Rights Reserved. 
#pragma once

#include "CoreMinimal.h"
#include "Dataflow/DataflowObjectInterface.h"


namespace UE::Geometry { class FDynamicMesh3; }
namespace GeometryCollection::Facades { class FRenderingFacade; }
class UDataflow;
class UObject;

namespace Dataflow
{
	namespace Conversion
	{
		// Convert a rendering facade to a dynamic mesh
		void DATAFLOWASSETTOOLS_API RenderingFacadeToDynamicMesh(const GeometryCollection::Facades::FRenderingFacade& Facade, UE::Geometry::FDynamicMesh3& DynamicMesh);

		// Convert a dataflow component to a dynamic mesh
		void DATAFLOWASSETTOOLS_API DataflowToDynamicMesh(TSharedPtr<::Dataflow::FEngineContext> DataflowContext, UObject* Asset, UDataflow* Dataflow, UE::Geometry::FDynamicMesh3& DynamicMesh);

		// Convert a dynamic mesh to a rendering facade
		void DATAFLOWASSETTOOLS_API DynamicMeshToRenderingFacade(const UE::Geometry::FDynamicMesh3& DynamicMesh, GeometryCollection::Facades::FRenderingFacade& Facade);

		// Convert a dynamic mesh to a dataflow component
		void DATAFLOWASSETTOOLS_API DynamicMeshToDataflow(const UE::Geometry::FDynamicMesh3& DynamicMesh, UDataflow* Dataflow);
	}

}	// namespace Dataflow