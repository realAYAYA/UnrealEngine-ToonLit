// Copyright Epic Games, Inc. All Rights Reserved.


#include "Selection/StaticMeshSelector.h"
#include "Selection/GeometrySelector.h"
#include "Selections/GeometrySelection.h"
#include "Selections/GeometrySelectionUtil.h"

#include "DynamicMesh/MeshIndexUtil.h"
#include "DynamicMesh/MeshNormals.h"
#include "DynamicMesh/MeshTransforms.h"
#include "Changes/MeshVertexChange.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "MeshDescriptionToDynamicMesh.h"
#include "DynamicMeshToMeshDescription.h"

#include "RenderingThread.h"

using namespace UE::Geometry;

#define LOCTEXT_NAMESPACE "FStaticMeshSelector"


bool FStaticMeshSelector::Initialize(
	FGeometryIdentifier SourceGeometryIdentifierIn)
{
	if (SourceGeometryIdentifierIn.TargetType != FGeometryIdentifier::ETargetType::PrimitiveComponent ||
		SourceGeometryIdentifierIn.ObjectType != FGeometryIdentifier::EObjectType::StaticMeshComponent)
	{
		return false;
	}
	StaticMeshComponent = Cast<UStaticMeshComponent>(SourceGeometryIdentifierIn.TargetObject);
	if (StaticMeshComponent == nullptr)
	{
		return false;
	}
	StaticMesh = StaticMeshComponent->GetStaticMesh();
	if (StaticMesh == nullptr)
	{
		return false;
	}

	// To know if the static mesh changed, we will listen to this OnMeshChanged event.
	// It is currently a very large hammer, though...
	StaticMesh_OnMeshChangedHandle = StaticMesh->OnMeshChanged.AddLambda([this]()
	{
		CopyFromStaticMesh();
		InvalidateOnMeshChange(FDynamicMeshChangeInfo());
	});

	// create new transient UDyanmicMesh that will be owned by this Selector
	LocalTargetMesh.Reset( NewObject<UDynamicMesh>() );

	CopyFromStaticMesh();

	FBaseDynamicMeshSelector::Initialize(SourceGeometryIdentifierIn, LocalTargetMesh.Get(), 
		[this]() { return IsValid(this->StaticMeshComponent) ? (UE::Geometry::FTransformSRT3d)this->StaticMeshComponent->GetComponentTransform() : FTransformSRT3d::Identity(); });

	return true;
}

void FStaticMeshSelector::Shutdown()
{
	StaticMesh->OnMeshChanged.Remove(StaticMesh_OnMeshChangedHandle);
	StaticMesh_OnMeshChangedHandle.Reset();

	LocalTargetMesh.Reset();
	FBaseDynamicMeshSelector::Shutdown();
}



IGeometrySelectionTransformer* FStaticMeshSelector::InitializeTransformation(const FGeometrySelection& Selection)
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

void FStaticMeshSelector::ShutdownTransformation(IGeometrySelectionTransformer* Transformer)
{
	ActiveTransformer.Reset();
}


void FStaticMeshSelector::CopyFromStaticMesh()
{
	int32 UseLODIndex = 0;
	const FMeshDescription* SourceMesh = StaticMesh->GetMeshDescription(UseLODIndex);
	if (SourceMesh != nullptr)
	{
		FDynamicMesh3 NewMesh;
		FMeshDescriptionToDynamicMesh Converter;
		Converter.Convert(SourceMesh, NewMesh, false /*AssetOptions.bRequestTangents*/ );

		LocalTargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
		{
			EditMesh = MoveTemp(NewMesh);
		});
	}
	else
	{
		LocalTargetMesh->ResetToCube();
	}
}


void FStaticMeshSelector::CommitMeshTransform()
{
	int32 UseLODIndex = 0;
	FlushRenderingCommands();

	// emit transaction here??

	// make sure transactional flag is on for this asset
	StaticMesh->SetFlags(RF_Transactional);
	// mark as modified
	StaticMesh->Modify();

	FMeshDescription* MeshDescription = StaticMesh->GetMeshDescription(UseLODIndex);
	if (MeshDescription == nullptr)
	{
		MeshDescription = StaticMesh->CreateMeshDescription(UseLODIndex);
	}
	StaticMesh->ModifyMeshDescription(UseLODIndex);

	FConversionToMeshDescriptionOptions ConversionOptions;
	FDynamicMeshToMeshDescription Converter(ConversionOptions);
	TargetMesh->ProcessMesh([&](const FDynamicMesh3& ReadMesh)
	{
		Converter.Convert(&ReadMesh, *MeshDescription, false /*bCopyTangents*/ );
	});

	StaticMesh->CommitMeshDescription(UseLODIndex);
	StaticMesh->PostEditChange();
}




bool FStaticMeshComponentSelectorFactory::CanBuildForTarget(FGeometryIdentifier TargetIdentifier) const
{
	if (TargetIdentifier.TargetType == FGeometryIdentifier::ETargetType::PrimitiveComponent)
	{
		UStaticMeshComponent* Component = TargetIdentifier.GetAsComponentType<UStaticMeshComponent>();
		if (Component != nullptr)
		{
			return true;
		}
	}
	return false;
}

TUniquePtr<IGeometrySelector> FStaticMeshComponentSelectorFactory::BuildForTarget(FGeometryIdentifier TargetIdentifier) const
{
	UStaticMeshComponent* Component = TargetIdentifier.GetAsComponentType<UStaticMeshComponent>();
	check(Component != nullptr);

	TUniquePtr<FStaticMeshSelector> Selector = MakeUnique<FStaticMeshSelector>();
	bool bInitialized = Selector->Initialize(TargetIdentifier);
	return (bInitialized) ? MoveTemp(Selector) : TUniquePtr<IGeometrySelector>();
}


#undef LOCTEXT_NAMESPACE 