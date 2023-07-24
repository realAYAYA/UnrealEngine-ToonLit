// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGGraphAssetTypeActions.h"
#include "PCGGraph.h"
#include "PCGEditor.h"

FText FPCGGraphAssetTypeActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "PCGGraphAssetTypeActions", "PCG Graph");
}

UClass* FPCGGraphAssetTypeActions::GetSupportedClass() const
{
	return UPCGGraph::StaticClass();
}

void FPCGGraphAssetTypeActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor)
{
	for (UObject* Object : InObjects)
	{
		if (UPCGGraph* PCGGraph = Cast<UPCGGraph>(Object))
		{
			const TSharedRef<FPCGEditor> PCGEditor = MakeShared<FPCGEditor>();
			PCGEditor->Initialize(EToolkitMode::Standalone, EditWithinLevelEditor, PCGGraph);
		}
	}
}

FText FPCGGraphInstanceAssetTypeActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "PCGGraphInstanceAssetTypeActions", "PCG Graph Instance");
}

UClass* FPCGGraphInstanceAssetTypeActions::GetSupportedClass() const
{
	return UPCGGraphInstance::StaticClass();
}

FText FPCGGraphInterfaceAssetTypeActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "PCGGraphInterfaceAssetTypeActions", "PCG Graph Interface");
}

UClass* FPCGGraphInterfaceAssetTypeActions::GetSupportedClass() const
{
	return UPCGGraphInterface::StaticClass();
}