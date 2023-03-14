// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Actions/UVToolAction.h"
#include "GeometryBase.h"
#include "IndexTypes.h"

#include "Actions/UVToolAction.h"

#include "UVSeamSewAction.generated.h"

PREDECLARE_GEOMETRY(class FUVEditorDynamicMeshSelection);
PREDECLARE_GEOMETRY(class FDynamicMesh3);
class APreviewGeometryActor;
class ULineSetComponent;

UCLASS()
class UVEDITORTOOLS_API UUVSeamSewAction : public UUVToolAction
{	
	GENERATED_BODY()

	using FDynamicMesh3 = UE::Geometry::FDynamicMesh3;

public:
	virtual bool CanExecuteAction() const override;
	virtual bool ExecuteAction() override;

	static int32 FindSewEdgeOppositePairing(const FDynamicMesh3& UnwrapMesh, 
		const FDynamicMesh3& AppliedMesh, int32 UVLayerIndex, int32 UnwrapEid, 
		bool& bWouldPreferOppositeOrderOut);
};