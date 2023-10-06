// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPointFromMeshElement.h"

#include "PCGComponent.h"
#include "PCGContext.h"
#include "PCGEdge.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"

#include "Engine/StaticMesh.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPointFromMeshElement)

#define LOCTEXT_NAMESPACE "PCGPointFromMeshElement"

#if WITH_EDITOR
FText UPCGPointFromMeshSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "PointFromMesh");
}

FText UPCGPointFromMeshSettings::GetNodeTooltipText() const
{
	return LOCTEXT("PointFromMeshNodeTooltip", "Creates a single point at the origin with an attribute named MeshPathAttributeName containing a SoftObjectPath to the StaticMesh.");
}
#endif

FPCGElementPtr UPCGPointFromMeshSettings::CreateElement() const
{
	return MakeShared<FPCGPointFromMeshElement>();
}

bool FPCGPointFromMeshElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointFromMeshElement::Execute);

	check(Context);

	const UPCGPointFromMeshSettings* Settings = Context->GetInputSettings<UPCGPointFromMeshSettings>();
	check(Settings);

	if (!Settings->StaticMesh.LoadSynchronous())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("LoadStaticMeshFailed", "Failed to load StaticMesh"));
		return true;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	UPCGPointData* OutPointData = NewObject<UPCGPointData>();
	Outputs.Emplace_GetRef().Data = OutPointData;
	
	TArray<FPCGPoint>& Points = OutPointData->GetMutablePoints();
	FPCGPoint& Point = Points.Emplace_GetRef();

	// Capture StaticMesh bounds
	const FBox StaticMeshBounds = Settings->StaticMesh->GetBoundingBox();
	Point.BoundsMin = StaticMeshBounds.Min;
	Point.BoundsMax = StaticMeshBounds.Max;

	// Write StaticMesh path to MeshPathAttribute
	check(OutPointData->Metadata);
	OutPointData->Metadata->CreateStringAttribute(Settings->MeshPathAttributeName, Settings->StaticMesh.ToString(), /*bAllowsInterpolation=*/false);

	return true;
}

#undef LOCTEXT_NAMESPACE
