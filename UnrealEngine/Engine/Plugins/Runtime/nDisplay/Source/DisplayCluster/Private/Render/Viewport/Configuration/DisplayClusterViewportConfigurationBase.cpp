// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterViewportConfigurationBase.h"

#include "Render/Viewport/DisplayClusterViewport.h"
#include "Render/Viewport/DisplayClusterViewportManager.h"
#include "Render/Viewport/Postprocess/DisplayClusterViewportPostProcessManager.h"
#include "Render/Viewport/Postprocess/DisplayClusterViewportPostProcessOutputRemap.h"

#include "DisplayClusterViewportConfigurationHelpers.h"
#include "DisplayClusterConfigurationTypes.h"

#include "DisplayClusterRootActor.h"

#include "Engine/StaticMesh.h"
#include "ProceduralMeshComponent.h"

#include "Misc/DisplayClusterLog.h"

///////////////////////////////////////////////////////////////////
// Copied from "TextureShareDisplayCluster/Misc/TextureShareDisplayClusterStrings.h"
namespace TextureShareDisplayClusterStrings
{
	namespace Postprocess
	{
		static constexpr auto TextureShare = TEXT("TextureShare");
	}

	namespace Projection
	{
		static constexpr auto TextureShare = TEXT("textureshare");
	}
};

///////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationBase
///////////////////////////////////////////////////////////////////
TArray<FString> FDisplayClusterViewportConfigurationBase::DisabledPostprocessNames;

void FDisplayClusterViewportConfigurationBase::Update(const FString& ClusterNodeId)
{
	TMap<FString, UDisplayClusterConfigurationViewport*> DesiredViewports;

	// Get render viewports for cluster node
	const UDisplayClusterConfigurationClusterNode* ClusterNodeConfiguration = ConfigurationData.Cluster->GetNode(ClusterNodeId);
	if (ClusterNodeConfiguration)
	{
		for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportIt : ClusterNodeConfiguration->Viewports)
		{
			if (ViewportIt.Key.Len() && ViewportIt.Value)
			{
				DesiredViewports.Add(ViewportIt.Key, ViewportIt.Value);
			}
		}
	}

	// Collect unused viewports and delete
	{
		TArray<FString> ExistClusterNodesIDs;
		ConfigurationData.Cluster->GetNodeIds(ExistClusterNodesIDs);

		TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> UnusedViewports;
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager.ImplGetCurrentRenderFrameViewports())
		{
			if (ViewportIt.IsValid())
			{
				// ignore ICVFX internal resources
				if (!EnumHasAllFlags(ViewportIt->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource))
				{
					// Only viewports from cluster node in render
					if (ClusterNodeId.IsEmpty() || ViewportIt->GetClusterNodeId() == ClusterNodeId)
					{
						if (!DesiredViewports.Contains(ViewportIt->GetId()))
						{
							UnusedViewports.Add(ViewportIt);
						}
					}
					else
					{
						if (ExistClusterNodesIDs.Find(ViewportIt->GetClusterNodeId()) == INDEX_NONE)
						{
							// also remove viewports for deleted cluster nodes
							UnusedViewports.Add(ViewportIt);
						}
					}
				}
			}
		}

		// Delete unused viewports
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& UnusedViewportIt : UnusedViewports)
		{
			if (UnusedViewportIt.IsValid())
			{
				ViewportManager.ImplDeleteViewport(UnusedViewportIt);
			}
		}
	}

	// Update and Create new viewports
	for (TPair<FString, UDisplayClusterConfigurationViewport*>& CfgIt : DesiredViewports)
	{
		if (const UDisplayClusterConfigurationViewport* ConfigurationViewport = CfgIt.Value)
		{
			const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> ExistViewport = ViewportManager.ImplFindViewport(CfgIt.Key);
			if (ExistViewport.IsValid())
			{
				FDisplayClusterViewportConfigurationBase::UpdateViewportConfiguration(*ExistViewport, ViewportManager, RootActor, *ConfigurationViewport);
			}
			else
			{
				ViewportManager.CreateViewport(CfgIt.Key, *ConfigurationViewport);
			}
		}
	}
}

void FDisplayClusterViewportConfigurationBase::Update(const TArray<FString>& InViewportNames, FDisplayClusterRenderFrameSettings& InOutRenderFrameSettings)
{
	// Collect unused viewports and delete
	{
		TArray<TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>> UnusedViewports;
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& ViewportIt : ViewportManager.ImplGetCurrentRenderFrameViewports())
		{
			if (ViewportIt.IsValid())
			{
				// ignore ICVFX internal resources
				if (!EnumHasAllFlags(ViewportIt->GetRenderSettingsICVFX().RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::InternalResource))
				{
					if (InViewportNames.Find(ViewportIt->GetId()) == INDEX_NONE)
					{
						UnusedViewports.Add(ViewportIt);
					}
				}
			}
		}

		// Delete unused viewports
		for (const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe>& DeleteIt : UnusedViewports)
		{
			ViewportManager.ImplDeleteViewport(DeleteIt);
		}
	}

	// Update and Create new viewports
	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& ClusterNodeConfigurationIt : ConfigurationData.Cluster->Nodes)
	{
		if (const UDisplayClusterConfigurationClusterNode* ClusterNodeConfiguration = ClusterNodeConfigurationIt.Value)
		{
			// Assign the cluster node name to viewport internals
			InOutRenderFrameSettings.ClusterNodeId = ClusterNodeConfigurationIt.Key;

			for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& ViewportIt : ClusterNodeConfiguration->Viewports)
			{
				if (const UDisplayClusterConfigurationViewport* ConfigurationViewport = ViewportIt.Value)
				{
					if (ViewportIt.Key.Len() && ViewportIt.Value && InViewportNames.Find(ViewportIt.Key) != INDEX_NONE)
					{
						const TSharedPtr<FDisplayClusterViewport, ESPMode::ThreadSafe> ExistViewport = ViewportManager.ImplFindViewport(ViewportIt.Key);
						if (ExistViewport.IsValid())
						{
							FDisplayClusterViewportConfigurationBase::UpdateViewportConfiguration(*ExistViewport, ViewportManager, RootActor, *ConfigurationViewport);
						}
						else
						{
							ViewportManager.CreateViewport(ViewportIt.Key, *ConfigurationViewport);
						}
					}
				}
			}
		}
	}

	// Do not use the cluster node name for this pass type
	InOutRenderFrameSettings.ClusterNodeId.Empty();
}

void FDisplayClusterViewportConfigurationBase::AddInternalPostprocess(const FString& InPostprocessName)
{
	InternalPostprocessNames.AddUnique(InPostprocessName);
}

void FDisplayClusterViewportConfigurationBase::UpdateClusterNodePostProcess(const FString& InClusterNodeId, const FDisplayClusterRenderFrameSettings& InRenderFrameSettings)
{
	check(!InClusterNodeId.IsEmpty());

	const UDisplayClusterConfigurationClusterNode* ClusterNode = ConfigurationData.Cluster->GetNode(InClusterNodeId);
	if (ClusterNode)
	{
		TSharedPtr<FDisplayClusterViewportPostProcessManager, ESPMode::ThreadSafe> PPManager = ViewportManager.GetPostProcessManager();
		if (PPManager.IsValid())
		{
			// Add TextureShare postprocess:
			if (ClusterNode->bEnableTextureShare && !InRenderFrameSettings.bIsPreviewRendering)
			{
				AddInternalPostprocess(TextureShareDisplayClusterStrings::Postprocess::TextureShare);
			}

			{
				// Find unused PP:
				TArray<FString> UnusedPP;
				for (const FString& It : PPManager->GetPostprocess())
				{
					// Leave defined postprocess (dynamic reconf)
					bool IsDefinedPostProcess = ClusterNode->Postprocess.Contains(It);

					// Support InternalPostprocess dynamic reconf
					if (InternalPostprocessNames.Find(It) != INDEX_NONE)
					{
						IsDefinedPostProcess = true;
					}

					if (!IsDefinedPostProcess)
					{
						UnusedPP.Add(It);
					}
				}

				// Delete unused PP:
				for (const FString& It : UnusedPP)
				{
					PPManager->RemovePostprocess(It);
				}
			}

			// Create InternalPostprocess
			for (const FString& InternalPostprocessId : InternalPostprocessNames)
			{
				if (DisabledPostprocessNames.Find(InternalPostprocessId) != INDEX_NONE)
				{
					// Always skip a postprocess that cannot be initialized
					continue;
				}

				TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe> ExistPostProcess = PPManager->FindPostProcess(InternalPostprocessId);
				if (!ExistPostProcess.IsValid())
				{
					// Create postprocess instance
					FDisplayClusterConfigurationPostprocess ConfigurationPostprocess;
					ConfigurationPostprocess.Type = InternalPostprocessId;

					if (PPManager->CanBeCreated(&ConfigurationPostprocess) && !PPManager->CreatePostprocess(InternalPostprocessId, &ConfigurationPostprocess))
					{
						// Can't create... Disable this postprocess
						DisabledPostprocessNames.AddUnique(InternalPostprocessId);

						UE_LOG(LogDisplayClusterViewport, Error, TEXT("Can't create postprocess '%s' required by cluster node '%s': Disabled"), *InternalPostprocessId, *InClusterNodeId);
					}
				}
			}

			// Create and update PP
			for (const TPair<FString, FDisplayClusterConfigurationPostprocess>& It : ClusterNode->Postprocess)
			{
				if (DisabledPostprocessNames.Find(It.Value.Type) != INDEX_NONE)
				{
					// Always skip a postprocess that cannot be initialized
					continue;
				}

				TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe> ExistPostProcess = PPManager->FindPostProcess(It.Key);
				if (ExistPostProcess.IsValid())
				{
					if (ExistPostProcess->IsConfigurationChanged(&It.Value))
					{
						PPManager->UpdatePostprocess(It.Key, &It.Value);
					}
				}
				else
				{
					if (PPManager->CanBeCreated(&It.Value) && !PPManager->CreatePostprocess(It.Key, &It.Value))
					{
						// Can't create... Disable this postprocess
						DisabledPostprocessNames.AddUnique(It.Value.Type);

						UE_LOG(LogDisplayClusterViewport, Error, TEXT("Can't create postprocess '%s' on cluster node '%s': Disabled"), *It.Value.Type, *InClusterNodeId);
					}
				}
			}

			// Update OutputRemap PP
			{
				const struct FDisplayClusterConfigurationFramePostProcess_OutputRemap& OutputRemapCfg = ClusterNode->OutputRemap;
				if (OutputRemapCfg.bEnable)
				{
					switch (OutputRemapCfg.DataSource)
					{
					case EDisplayClusterConfigurationFramePostProcess_OutputRemapSource::StaticMesh:
						PPManager->GetOutputRemap()->UpdateConfiguration_StaticMesh(OutputRemapCfg.StaticMesh);
						break;

					case EDisplayClusterConfigurationFramePostProcess_OutputRemapSource::MeshComponent:
						if (!OutputRemapCfg.MeshComponentName.IsEmpty())
						{
							// Get the StaticMeshComponent
							UStaticMeshComponent* StaticMeshComponent = RootActor.GetComponentByName<UStaticMeshComponent>(OutputRemapCfg.MeshComponentName);
							if (StaticMeshComponent != nullptr)
							{
								PPManager->GetOutputRemap()->UpdateConfiguration_StaticMeshComponent(StaticMeshComponent);
							}
							else
							{
								// Get the procedural mesh component
								UProceduralMeshComponent* ProceduralMeshComponent = RootActor.GetComponentByName<UProceduralMeshComponent>(OutputRemapCfg.MeshComponentName);
								PPManager->GetOutputRemap()->UpdateConfiguration_ProceduralMeshComponent(ProceduralMeshComponent);
							}
						}
						else
						{
							PPManager->GetOutputRemap()->UpdateConfiguration_Disabled();
						}
						break;

					case EDisplayClusterConfigurationFramePostProcess_OutputRemapSource::ExternalFile:
						PPManager->GetOutputRemap()->UpdateConfiguration_ExternalFile(OutputRemapCfg.ExternalFile);
						break;

					default:
						PPManager->GetOutputRemap()->UpdateConfiguration_Disabled();
					}
				}
				else
				{
					PPManager->GetOutputRemap()->UpdateConfiguration_Disabled();
				}
			}
		}
	}
}

// Assign new configuration to this viewport <Runtime>
bool FDisplayClusterViewportConfigurationBase::UpdateViewportConfiguration(FDisplayClusterViewport& DstViewport, FDisplayClusterViewportManager& ViewportManager, ADisplayClusterRootActor& RootActor, const UDisplayClusterConfigurationViewport& ConfigurationViewport)
{
	check(IsInGameThread());

	FDisplayClusterViewportConfigurationHelpers::UpdateBaseViewportSetting(DstViewport, RootActor, ConfigurationViewport);
	FDisplayClusterViewportConfigurationHelpers::UpdateProjectionPolicy(DstViewport, &(ConfigurationViewport.ProjectionPolicy));

	return true;
}
