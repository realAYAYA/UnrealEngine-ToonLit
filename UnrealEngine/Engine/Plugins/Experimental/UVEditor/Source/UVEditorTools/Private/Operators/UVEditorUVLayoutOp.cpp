// Copyright Epic Games, Inc. All Rights Reserved.

#include "Operators/UVEditorUVLayoutOp.h"

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Parameterization/MeshUVPacking.h"
#include "Selections/MeshConnectedComponents.h"
#include "Utilities/MeshUDIMClassifier.h"
#include "UVEditorUXSettings.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(UVEditorUVLayoutOp)

using namespace UE::Geometry;

void FUVEditorUVLayoutOp::SetTransform(const FTransformSRT3d& Transform)
{
	ResultTransform = Transform;
}


void FUVEditorUVLayoutOp::CalculateResult(FProgressCancel* Progress)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UVEditorUVLayoutOp_CalculateResult);
	
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
	

	bool bWillRepackIslands = (UVLayoutMode != EUVEditorUVLayoutOpLayoutModes::TransformOnly);

	// split bowties so that we can process islands independently
	if (bWillRepackIslands || bAlwaysSplitBowties)
	{
		UseUVLayer->SplitBowties();
	}

	if (Progress && Progress->Cancelled())
	{
		return;
	}

	if (bMaintainOriginatingUDIM)
	{		
		TOptional<TArray<int32>> SelectionArray;
		if (Selection.IsSet())
		{
			SelectionArray = Selection.GetValue().Array();
		}
		FDynamicMeshUDIMClassifier TileClassifier(UseUVLayer, SelectionArray);

		TArray<FVector2i> Tiles = TileClassifier.ActiveTiles();

		for (FVector2i TileIndex : Tiles)
		{
			TUniquePtr<TArray<int32>> TileTids;
			TileTids = MakeUnique<TArray<int32>>(TileClassifier.TidsForTile(TileIndex));

			// Do this first, so we don't need to keep the TileTids around after moving it into the packer.
			TSet<int32> ElementsToMove;
			ElementsToMove.Reserve(TileTids->Num() * 3);
			for (int Tid : *TileTids)
			{
				FIndex3i Elements = UseUVLayer->GetTriangle(Tid);
				ElementsToMove.Add(Elements[0]);
				ElementsToMove.Add(Elements[1]);
				ElementsToMove.Add(Elements[2]);
			}

			// TODO: There is a second connected components call inside the packer that might be unnessessary. Could be a future optimization.
			FDynamicMeshUVPacker Packer(UseUVLayer, MoveTemp(TileTids) );
			ExecutePacker(Packer);
			if (Progress && Progress->Cancelled())
			{
				return;
			}

			for (int32 Element : ElementsToMove)
			{
				FVector2f UV = FUVEditorUXSettings::InternalUVToExternalUV(UseUVLayer->GetElement(Element));
				UV = (UV) + (FVector2f)(TileIndex);
				UseUVLayer->SetElement(Element, FUVEditorUXSettings::ExternalUVToInternalUV(UV));
			}			
		}
	}
	else
	{
		TUniquePtr<TArray<int32>> TidsToLayout = nullptr;
		if (Selection.IsSet())
		{
			TidsToLayout = MakeUnique<TArray<int32>>();
			TSet<int32>& SelectionSet = Selection.GetValue();
			TidsToLayout->Reserve(SelectionSet.Num());
			for(int Tid : SelectionSet)
			{
				if (OriginalMesh->IsTriangle(Tid))
				{
					TidsToLayout->Add(Tid);
				}
			}
		}
		
		FDynamicMeshUVPacker Packer(UseUVLayer, MoveTemp(TidsToLayout) );
		ExecutePacker(Packer);
		if (Progress && Progress->Cancelled())
		{
			return;
		}
	}

	if (UVScaleFactor != 1.0 || UVTranslation != FVector2f::Zero() )
	{
		for (int ElementID : UseUVLayer->ElementIndicesItr())
		{
			FVector2f UV = FUVEditorUXSettings::InternalUVToExternalUV(UseUVLayer->GetElement(ElementID));
			UV = (UV * UVScaleFactor) + UVTranslation;
			UseUVLayer->SetElement(ElementID, FUVEditorUXSettings::ExternalUVToInternalUV(UV));
		}
	}

}

void FUVEditorUVLayoutOp::ExecutePacker(FDynamicMeshUVPacker& Packer)
{
	Packer.TextureResolution = this->TextureResolution;
	Packer.GutterSize = this->GutterSize;
	Packer.bAllowFlips = this->bAllowFlips;

	if (UVLayoutMode == EUVEditorUVLayoutOpLayoutModes::RepackToUnitRect)
	{
		if (Packer.StandardPack() == false)
		{
			// failed... what to do?
			return;
		}
	}
	else if (UVLayoutMode == EUVEditorUVLayoutOpLayoutModes::StackInUnitRect)
	{
		if (Packer.StackPack() == false)
		{
			// failed... what to do?
			return;
		}
	}
}

TUniquePtr<FDynamicMeshOperator> UUVEditorUVLayoutOperatorFactory::MakeNewOperator()
{
	TUniquePtr<FUVEditorUVLayoutOp> Op = MakeUnique<FUVEditorUVLayoutOp>();

	Op->OriginalMesh = OriginalMesh;

	switch (Settings->LayoutType)
	{
	case EUVEditorUVLayoutType::Transform:
		Op->UVLayoutMode = EUVEditorUVLayoutOpLayoutModes::TransformOnly;
		break;
	case EUVEditorUVLayoutType::Stack:
		Op->UVLayoutMode = EUVEditorUVLayoutOpLayoutModes::StackInUnitRect;
		break;
	case EUVEditorUVLayoutType::Repack:
		Op->UVLayoutMode = EUVEditorUVLayoutOpLayoutModes::RepackToUnitRect;
		break;
	}

	Op->UVLayerIndex = GetSelectedUVChannel();
	Op->TextureResolution = Settings->TextureResolution;
	Op->bAllowFlips = Settings->bAllowFlips;
	Op->UVScaleFactor = Settings->Scale;
	Op->UVTranslation = FVector2f(Settings->Translation);
	Op->SetTransform(TargetTransform);
	Op->bMaintainOriginatingUDIM = Settings->bEnableUDIMLayout;
	Op->Selection = Selection;

	return Op;
}
