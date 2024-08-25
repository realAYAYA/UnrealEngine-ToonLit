// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfiguratorMPCDIImporter.h"

#include "DisplayClusterProjectionStrings.h"
#include "IDisplayClusterWarp.h"
#include "Blueprints/DisplayClusterBlueprint.h"
#include "Blueprints/DisplayClusterBlueprintGeneratedClass.h"
#include "ClusterConfiguration/DisplayClusterConfiguratorClusterUtils.h"
#include "ClusterConfiguration/ViewModels/DisplayClusterConfiguratorClusterNodeViewModel.h"
#include "ClusterConfiguration/ViewModels/DisplayClusterConfiguratorProjectionPolicyViewModel.h"
#include "ClusterConfiguration/ViewModels/DisplayClusterConfiguratorViewportViewModel.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterScreenComponent.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Misc/DisplayClusterHelpers.h"

bool FDisplayClusterConfiguratorMPCDIImporter::ImportMPCDIIntoBlueprint(const FString& InFilePath, UDisplayClusterBlueprint* InBlueprint, const FDisplayClusterConfiguratorMPCDIImporterParams& InParams)
{
	const FString MPCDIFileFullPath = DisplayClusterHelpers::filesystem::GetFullPathForConfigResource(InFilePath);

	TMap<FString, TMap<FString, FDisplayClusterWarpMPCDIAttributes>> MPCDIFile;
	IDisplayClusterWarp::Get().ReadMPCDFileStructure(InFilePath, MPCDIFile);


	FName ViewOriginComponentName = !InParams.ViewOriginComponentName.IsNone() ? InParams.ViewOriginComponentName : TEXT("DefaultViewPoint");

	USCS_Node* ViewOriginNode = nullptr;
	UDisplayClusterCameraComponent* ViewOriginComponent = Cast<UDisplayClusterCameraComponent>(FindBlueprintSceneComponent(InBlueprint, ViewOriginComponentName, &ViewOriginNode));

	FIPv4Address CurrentIPAddress = InParams.HostStartingIPAddress;
	for (const TPair<FString, TMap<FString, FDisplayClusterWarpMPCDIAttributes>>& Buffer : MPCDIFile)
	{
		for (const TPair<FString, FDisplayClusterWarpMPCDIAttributes>& Region : Buffer.Value)
		{
			if (InParams.bCreateStageGeometryComponents)
			{
				// For MPCDI 2D, we create Display Cluster screen components to match the MPCDI buffer and region configuration
				if (Region.Value.ProfileType == EDisplayClusterWarpProfileType::warp_2D)
				{
					bool bFoundExistingScreen = false;
					USCS_Node* ScreenNode = FindOrCreateScreenNodeForRegion(InBlueprint, Region.Key, bFoundExistingScreen);
					if (ScreenNode)
					{
						UDisplayClusterScreenComponent* ScreenComponent = CastChecked<UDisplayClusterScreenComponent>(ScreenNode->GetActualComponentTemplate(InBlueprint->GetGeneratedClass()));
						ConfigureScreenFromRegion(ScreenComponent, ViewOriginComponent, Region.Value, InParams);

						// If a new screen had to be created, it needs to be parented to a valid parent component
						if (!bFoundExistingScreen)
						{
							USCS_Node* ParentNode = nullptr;
							USceneComponent* ParentComponent = nullptr;
							if (InParams.ParentComponentName.IsNone())
							{
								ParentComponent = InBlueprint->SimpleConstructionScript->GetSceneRootComponentTemplate(false, &ParentNode);
							}
							else
							{
								ParentComponent = FindBlueprintSceneComponent(InBlueprint, InParams.ParentComponentName, &ParentNode);
							}

							if (ParentNode && ParentNode->GetSCS() == InBlueprint->SimpleConstructionScript)
							{
								ParentNode->AddChildNode(ScreenNode);
							}
							else
							{
								ScreenNode->SetParent(ParentComponent);
								InBlueprint->SimpleConstructionScript->AddNode(ScreenNode);
							}
						}
					}
				}
			}

			bool bFoundExistingViewport = false;
			UDisplayClusterConfigurationViewport* Viewport = FindOrCreateViewportForRegion(InBlueprint, Region.Key, bFoundExistingViewport);
			if (Viewport)
			{
				ConfigureViewportFromRegion(Viewport, Region.Value, InParams);

				if (!bFoundExistingViewport)
				{
					FDisplayClusterConfiguratorProjectionPolicyViewModel ProjectionPolicyViewModel(Viewport);

					ProjectionPolicyViewModel.SetPolicyType(DisplayClusterProjectionStrings::projection::MPCDI);

					ProjectionPolicyViewModel.SetParameterValue(DisplayClusterProjectionStrings::cfg::mpcdi::MPCDITypeKey, DisplayClusterProjectionStrings::cfg::mpcdi::TypeMPCDI);
					ProjectionPolicyViewModel.SetParameterValue(DisplayClusterProjectionStrings::cfg::mpcdi::File, MPCDIFileFullPath);
					ProjectionPolicyViewModel.SetParameterValue(DisplayClusterProjectionStrings::cfg::mpcdi::Buffer, Buffer.Key);
					ProjectionPolicyViewModel.SetParameterValue(DisplayClusterProjectionStrings::cfg::mpcdi::Region, Region.Key);
					ProjectionPolicyViewModel.SetParameterValue(DisplayClusterProjectionStrings::cfg::mpcdi::EnablePreview, TEXT("true"));

					if (Region.Value.ProfileType == EDisplayClusterWarpProfileType::warp_2D)
					{
						ProjectionPolicyViewModel.SetParameterValue(DisplayClusterProjectionStrings::cfg::mpcdi::MPCDIType, TEXT("2d"));

						if (InParams.bCreateStageGeometryComponents)
						{
							ProjectionPolicyViewModel.SetParameterValue(DisplayClusterProjectionStrings::cfg::mpcdi::Component, GetScreenNameForRegion(Region.Key));
						}
					}

					if (!InParams.ParentComponentName.IsNone())
					{
						ProjectionPolicyViewModel.SetParameterValue(DisplayClusterProjectionStrings::cfg::mpcdi::Origin, InParams.ParentComponentName.ToString());
					}

					if (UDisplayClusterConfigurationClusterNode* ClusterNode = Cast<UDisplayClusterConfigurationClusterNode>(Viewport->GetOuter()))
					{
						FDisplayClusterConfiguratorClusterNodeViewModel ClusterNodeViewModel(ClusterNode);
						ClusterNodeViewModel.SetHost(CurrentIPAddress.ToString());
					}
				}

				UDisplayClusterConfigurationCluster* Cluster = InBlueprint->GetOrLoadConfig()->Cluster;
				const bool bHasValidPrimaryNode = Cluster->Nodes.Contains(Cluster->PrimaryNode.Id);
				if (!bHasValidPrimaryNode)
				{
					Cluster->Modify();
					Cluster->PrimaryNode.Id = GetClusterNodeNameForRegion(Region.Key);
				}

				if (InParams.bIncrementHostIPAddress)
				{
					++CurrentIPAddress.Value;
				}
			}
		}
	}

	return true;
}

USCS_Node* FDisplayClusterConfiguratorMPCDIImporter::FindOrCreateScreenNodeForRegion(UDisplayClusterBlueprint* InBlueprint, const FString& RegionId, bool& bOutFoundExistingScreen)
{
	bOutFoundExistingScreen = false;

	const FString ScreenName = GetScreenNameForRegion(RegionId);
	if (USCS_Node* ExistingScreenNode = InBlueprint->SimpleConstructionScript->FindSCSNode(*ScreenName))
	{
		bOutFoundExistingScreen = true;
		return ExistingScreenNode;
	}

	return InBlueprint->SimpleConstructionScript->CreateNode(UDisplayClusterScreenComponent::StaticClass(), *ScreenName);
}

void FDisplayClusterConfiguratorMPCDIImporter::ConfigureScreenFromRegion(
	UDisplayClusterScreenComponent* InScreenComponent,
	UDisplayClusterCameraComponent* InViewOriginComponent,
	const FDisplayClusterWarpMPCDIAttributes& InAttributes,
	const FDisplayClusterConfiguratorMPCDIImporterParams& InParams)
{
	const FVector2D BufferActualSize(InAttributes.Buffer.Resolution.X * InParams.BufferToWorldScale, InAttributes.Buffer.Resolution.Y * InParams.BufferToWorldScale);
	const FVector2D RegionActualPosition((InAttributes.Region.Pos.X - 0.5) * BufferActualSize.X, (InAttributes.Region.Pos.Y - 0.5) * BufferActualSize.Y);
	const FVector2D RegionActualSize(InAttributes.Region.Size.X * BufferActualSize.X, InAttributes.Region.Size.Y * BufferActualSize.Y);

	FVector ScreenPosition = FVector(InParams.BufferToWorldDistance, RegionActualPosition.X + RegionActualSize.X * 0.5, RegionActualPosition.Y + RegionActualSize.Y * 0.5);

	if (InViewOriginComponent)
	{
		// We want to position the screen component to be in front of the view origin it is assigned to, so we must compute
		// the transforms from view origin to root and from root to screen component to correct position

		// Blueprint components don't have world transforms, so compute the view origin to root transform manually
		FTransform ViewOriginTransform = FTransform::Identity;
		for (const USceneComponent* Comp = InViewOriginComponent; Comp != nullptr; Comp = Comp->GetAttachParent())
		{
			ViewOriginTransform *= Comp->GetRelativeTransform();
		}

		// The screen may also be parented to a non-root component, so also compute its root transform
		FTransform ScreenTransform = FTransform::Identity;
		for (const USceneComponent* Comp = InScreenComponent->GetAttachParent(); Comp != nullptr; Comp = Comp->GetAttachParent())
		{
			ScreenTransform *= Comp->GetRelativeTransform();
		}

		FTransform ViewOriginToScreen = ViewOriginTransform * ScreenTransform.Inverse();
		ScreenPosition = ViewOriginToScreen.TransformPositionNoScale(ScreenPosition);
	}

	InScreenComponent->SetRelativeLocation(ScreenPosition);
	InScreenComponent->SetScreenSize(RegionActualSize);
}

UDisplayClusterConfigurationViewport* FDisplayClusterConfiguratorMPCDIImporter::FindOrCreateViewportForRegion(UDisplayClusterBlueprint* InBlueprint, const FString& RegionId, bool& bOutFoundExistingViewport)
{
	UDisplayClusterConfigurationViewport* Viewport = nullptr;
	bOutFoundExistingViewport = false;

	const FString ViewportName = GetViewportNameForRegion(RegionId);
	UDisplayClusterConfigurationCluster* Cluster = InBlueprint->GetOrLoadConfig()->Cluster;
	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& NodePair : Cluster->Nodes)
	{
		if (NodePair.Value->Viewports.Contains(ViewportName))
		{
			Viewport = NodePair.Value->Viewports[ViewportName];
			bOutFoundExistingViewport = true;
			break;
		}
	}

	if (!Viewport)
	{
		const FString NodeName = GetClusterNodeNameForRegion(RegionId);
		UDisplayClusterConfigurationClusterNode* ClusterNodeTemplate = NewObject<UDisplayClusterConfigurationClusterNode>(InBlueprint, NAME_None, RF_Transactional | RF_ArchetypeObject | RF_Public);
		UDisplayClusterConfigurationClusterNode* NewClusterNode = UE::DisplayClusterConfiguratorClusterUtils::AddClusterNodeToCluster(ClusterNodeTemplate, Cluster, NodeName);

		UDisplayClusterConfigurationViewport* ViewportTemplate = NewObject<UDisplayClusterConfigurationViewport>(InBlueprint, NAME_None, RF_Transactional | RF_ArchetypeObject | RF_Public);
		Viewport = UE::DisplayClusterConfiguratorClusterUtils::AddViewportToClusterNode(ViewportTemplate, NewClusterNode, ViewportName);
	}

	return Viewport;
}

void FDisplayClusterConfiguratorMPCDIImporter::ConfigureViewportFromRegion(UDisplayClusterConfigurationViewport* InViewport, const FDisplayClusterWarpMPCDIAttributes& InAttributes, const FDisplayClusterConfiguratorMPCDIImporterParams& InParams)
{
	if (UDisplayClusterConfigurationClusterNode* ClusterNode = Cast<UDisplayClusterConfigurationClusterNode>(InViewport->GetOuter()))
	{
		FDisplayClusterConfiguratorClusterNodeViewModel ClusterNodeViewModel(ClusterNode);
		ClusterNodeViewModel.SetWindowRect(FDisplayClusterConfigurationRectangle(0, 0, InAttributes.Region.Resolution.X, InAttributes.Region.Resolution.Y));
	}

	FDisplayClusterConfiguratorViewportViewModel ViewportViewModel(InViewport);
	ViewportViewModel.SetRegion(FDisplayClusterConfigurationRectangle(0, 0, InAttributes.Region.Resolution.X, InAttributes.Region.Resolution.Y));

	if (InParams.ViewOriginComponentName != NAME_None)
	{
		ViewportViewModel.SetCamera(InParams.ViewOriginComponentName.ToString());
	}
}

USceneComponent* FDisplayClusterConfiguratorMPCDIImporter::FindBlueprintSceneComponent(UDisplayClusterBlueprint* InBlueprint, const FName& ComponentName, USCS_Node** OutComponentNode)
{
	*OutComponentNode = nullptr;

	UClass* GeneratedClass = InBlueprint->GetGeneratedClass();
	UClass* ParentClass = InBlueprint->ParentClass;

	AActor* CDO = Cast<AActor>(GeneratedClass->GetDefaultObject(false));
	if (CDO == nullptr && ParentClass != nullptr)
	{
		CDO = Cast<AActor>(ParentClass->GetDefaultObject(false));
	}

	// First, check to see if there exists a native component on the CDO that matches the specified name
	USceneComponent* FoundComponent = nullptr;
	if (CDO)
	{
		for (UActorComponent* Component : CDO->GetComponents())
		{
			if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
			{
				if (SceneComponent->GetFName() == ComponentName)
				{
					FoundComponent = SceneComponent;
					break;
				}
			}
		}
	}

	// If a native component was not found, check the SCS to see if one exists there
	if (!FoundComponent)
	{
		if (USCS_Node* FoundNode = InBlueprint->SimpleConstructionScript->FindSCSNode(ComponentName))
		{
			if (FoundNode->ComponentTemplate && FoundNode->ComponentTemplate->IsA<USceneComponent>())
			{
				*OutComponentNode = FoundNode;
				FoundComponent = Cast<USceneComponent>(FoundNode->ComponentTemplate);
			}
		}
	}

	return FoundComponent;
}

FString FDisplayClusterConfiguratorMPCDIImporter::GetScreenNameForRegion(const FString& RegionId)
{
	return FString::Printf(TEXT("%s_Screen"), *RegionId);
}

FString FDisplayClusterConfiguratorMPCDIImporter::GetClusterNodeNameForRegion(const FString& RegionId)
{
	return FString::Printf(TEXT("%s_Node"), *RegionId);
}

FString FDisplayClusterConfiguratorMPCDIImporter::GetViewportNameForRegion(const FString& RegionId)
{
	return FString::Printf(TEXT("%s_Viewport"), *RegionId);
}
