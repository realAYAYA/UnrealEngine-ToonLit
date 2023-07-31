// Copyright Epic Games, Inc. All Rights Reserved.

#include "ParameterizationOps/UVLayoutOp.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Parameterization/MeshUVPacking.h"
#include "Properties/UVLayoutProperties.h"
#include "Selections/MeshConnectedComponents.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UVLayoutOp)

using namespace UE::Geometry;

void FUVLayoutOp::SetTransform(const FTransformSRT3d& Transform) 
{
	ResultTransform = Transform;
}


void FUVLayoutOp::CalculateResult(FProgressCancel* Progress)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UVLayoutOp_CalculateResult);
	
	if (Progress && Progress->Cancelled())
	{
		return;
	}
	ResultMesh->Copy(*OriginalMesh, true, true, true, true);

	if (!ensureMsgf(ResultMesh->HasAttributes(), TEXT("Attributes not found on mesh? Conversion should always create them, so this operator should not need to do so.")))
	{
		ResultMesh->EnableAttributes();
	}

	if (Progress && Progress->Cancelled())
	{
		return;
	}


	int UVLayerInput = UVLayerIndex;
	FDynamicMeshUVOverlay* UseUVLayer = ResultMesh->Attributes()->GetUVLayer(UVLayerInput);
	

	bool bWillRepackIslands = (UVLayoutMode != EUVLayoutOpLayoutModes::TransformOnly);

	// split bowties so that we can process islands independently
	if (bWillRepackIslands || bAlwaysSplitBowties)
	{
		UseUVLayer->SplitBowties();
	}

	if (Progress && Progress->Cancelled())
	{
		return;
	}

	FDynamicMeshUVPacker Packer(UseUVLayer);
	Packer.TextureResolution = this->TextureResolution;
	Packer.GutterSize = this->GutterSize;
	Packer.bAllowFlips = this->bAllowFlips;

	if (UVLayoutMode == EUVLayoutOpLayoutModes::RepackToUnitRect)
	{
		if (Packer.StandardPack() == false)
		{
			// failed... what to do?
			return;
		}
	}
	else if (UVLayoutMode == EUVLayoutOpLayoutModes::StackInUnitRect)
	{
		if (Packer.StackPack() == false)
		{
			// failed... what to do?
			return;
		}
	}

	if (Progress && Progress->Cancelled())
	{
		return;
	}

	if (UVScaleFactor != 1.0 || UVTranslation != FVector2f::Zero() )
	{
		for (int ElementID : UseUVLayer->ElementIndicesItr())
		{
			FVector2f UV = UseUVLayer->GetElement(ElementID);
			UV = (UV * UVScaleFactor) + UVTranslation;
			UseUVLayer->SetElement(ElementID, UV);
		}
	}

}

TUniquePtr<FDynamicMeshOperator> UUVLayoutOperatorFactory::MakeNewOperator()
{
	TUniquePtr<FUVLayoutOp> Op = MakeUnique<FUVLayoutOp>();

	Op->OriginalMesh = OriginalMesh;

	switch (Settings->LayoutType)
	{
	case EUVLayoutType::Transform:
		Op->UVLayoutMode = EUVLayoutOpLayoutModes::TransformOnly;
		break;
	case EUVLayoutType::Stack:
		Op->UVLayoutMode = EUVLayoutOpLayoutModes::StackInUnitRect;
		break;
	case EUVLayoutType::Repack:
		Op->UVLayoutMode = EUVLayoutOpLayoutModes::RepackToUnitRect;
		break;
	}

	Op->UVLayerIndex = GetSelectedUVChannel();
	Op->TextureResolution = Settings->TextureResolution;
	Op->bAllowFlips = Settings->bAllowFlips;
	Op->UVScaleFactor = Settings->Scale;
	Op->UVTranslation = FVector2f(Settings->Translation);
	Op->SetTransform(TargetTransform);

	return Op;
}
