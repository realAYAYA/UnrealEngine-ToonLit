// Copyright Epic Games, Inc. All Rights Reserved.


#include "Selection/VolumeSelector.h"
#include "Selection/GeometrySelector.h"
#include "Selections/GeometrySelection.h"
#include "Selections/GeometrySelectionUtil.h"

#include "DynamicMesh/MeshIndexUtil.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTransforms.h"
#include "Changes/MeshVertexChange.h"

#include "GameFramework/Volume.h"
#include "Components/BrushComponent.h"
#include "ConversionUtils/VolumeToDynamicMesh.h"
#include "ConversionUtils/DynamicMeshToVolume.h"

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "FVolumeSelector"




bool FVolumeSelector::Initialize(
	FGeometryIdentifier SourceGeometryIdentifierIn)
{
	if (SourceGeometryIdentifierIn.TargetType != FGeometryIdentifier::ETargetType::PrimitiveComponent ||
		SourceGeometryIdentifierIn.ObjectType != FGeometryIdentifier::EObjectType::BrushComponent)
	{
		return false;
	}
	BrushComponent = Cast<UBrushComponent>(SourceGeometryIdentifierIn.TargetObject);
	if (BrushComponent == nullptr)
	{
		return false;
	}
	ParentVolume = Cast<AVolume>(BrushComponent->GetOwner());
	if (ParentVolume == nullptr)
	{
		return false;
	}
	
	LocalTargetMesh.Reset( NewObject<UDynamicMesh>() );

	LocalTargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
	{
		UE::Conversion::FVolumeToMeshOptions ConvertOptions;
		UE::Conversion::BrushComponentToDynamicMesh(BrushComponent, EditMesh, ConvertOptions);
	});

	FBaseDynamicMeshSelector::Initialize(SourceGeometryIdentifierIn, LocalTargetMesh.Get(), 
		[this]() { return IsValid(this->BrushComponent) ? (UE::Geometry::FTransformSRT3d)this->BrushComponent->GetComponentTransform() : FTransformSRT3d::Identity(); });

	return true;
}


void FVolumeSelector::Shutdown()
{
	LocalTargetMesh.Reset();
	FBaseDynamicMeshSelector::Shutdown();
}


IGeometrySelectionTransformer* FVolumeSelector::InitializeTransformation(const FGeometrySelection& Selection)
{
	check(!ActiveTransformer);

	ActiveTransformer = MakePimpl<FBasicDynamicMeshSelectionTransformer>();
	ActiveTransformer->Initialize(this);
	ActiveTransformer->OnEndTransformFunc = [this](IToolsContextTransactionsAPI*) 
	{ 
		CommitMeshTransform();
	};
	return ActiveTransformer.Get();
}

void FVolumeSelector::ShutdownTransformation(IGeometrySelectionTransformer* Transformer)
{
	ActiveTransformer.Reset();
}


void FVolumeSelector::CommitMeshTransform()
{
	FTransform SetTransform = ParentVolume->GetActorTransform();

	ParentVolume->Modify();
	BrushComponent->Modify();

	UE::Conversion::FMeshToVolumeOptions ConvertOptions;
	FDynamicMesh3 TmpMesh;
	TargetMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& SourceMesh)
	{
		TmpMesh = SourceMesh;
	});

	// This conversion will (somehow) mess w/ the transform on the Component/Volume? What seems like
	// is happening is that the mesh is interpreted as being in world-space, and the Actor transform recomputed,
	// so basically it moves to the Origin. So we need to re-set the previous Transform afterwards.
	UE::Conversion::DynamicMeshToVolume(TmpMesh, ParentVolume, ConvertOptions);
	ParentVolume->SetActorTransform(SetTransform);

	ParentVolume->PostEditChange();
}




bool FBrushComponentSelectorFactory::CanBuildForTarget(FGeometryIdentifier TargetIdentifier) const
{
	if (TargetIdentifier.TargetType == FGeometryIdentifier::ETargetType::PrimitiveComponent)
	{
		UBrushComponent* Component = TargetIdentifier.GetAsComponentType<UBrushComponent>();
		if (Component && Cast<AVolume>(Component->GetOwner()) != nullptr)
		{
			return true;
		}
	}
	return false;
}

TUniquePtr<IGeometrySelector> FBrushComponentSelectorFactory::BuildForTarget(FGeometryIdentifier TargetIdentifier) const
{
	UBrushComponent* Component = TargetIdentifier.GetAsComponentType<UBrushComponent>();
	check(Component != nullptr);

	TUniquePtr<FVolumeSelector> Selector = MakeUnique<FVolumeSelector>();
	bool bInitialized = Selector->Initialize(TargetIdentifier);
	return (bInitialized) ? MoveTemp(Selector) : TUniquePtr<IGeometrySelector>();
}


#undef LOCTEXT_NAMESPACE 