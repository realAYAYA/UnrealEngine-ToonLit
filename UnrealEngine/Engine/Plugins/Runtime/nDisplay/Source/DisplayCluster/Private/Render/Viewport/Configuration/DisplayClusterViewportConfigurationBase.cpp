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


///////////////////////////////////////////////////////////////////
// FDisplayClusterViewportConfigurationBase
///////////////////////////////////////////////////////////////////

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

		TArray<FDisplayClusterViewport*> UnusedViewports;
		for (FDisplayClusterViewport* It : ViewportManager.ImplGetViewports())
		{
			// ignore ICVFX internal resources
			if ((It->GetRenderSettingsICVFX().RuntimeFlags & ViewportRuntime_InternalResource) == 0)
			{
				// Only viewports from cluster node in render
				if (ClusterNodeId.IsEmpty() || It->GetClusterNodeId() == ClusterNodeId)
				{
					if (!DesiredViewports.Contains(It->GetId()))
					{
						UnusedViewports.Add(It);
					}
				}
				else
				{
					if(ExistClusterNodesIDs.Find(It->GetClusterNodeId()) == INDEX_NONE)
					{
						// also remove viewports for deleted cluster nodes
						UnusedViewports.Add(It);
					}
				}
			}
		}

		// Delete unused viewports
		for (FDisplayClusterViewport* DeleteIt : UnusedViewports)
		{
			ViewportManager.ImplDeleteViewport(DeleteIt);
		}
	}

	// Update and Create new viewports
	for (TPair<FString, UDisplayClusterConfigurationViewport*>& CfgIt : DesiredViewports)
	{
		FDisplayClusterViewport* ExistViewport = ViewportManager.ImplFindViewport(CfgIt.Key);
		if (ExistViewport)
		{
			FDisplayClusterViewportConfigurationBase::UpdateViewportConfiguration(ViewportManager, RootActor, ExistViewport, CfgIt.Value);
		}
		else
		{
			ViewportManager.CreateViewport(CfgIt.Key, CfgIt.Value);
		}
	}
}

void FDisplayClusterViewportConfigurationBase::Update(const TArray<FString>& InViewportNames, FDisplayClusterRenderFrameSettings& InOutRenderFrameSettings)
{
	// Collect unused viewports and delete
	{
		TArray<FDisplayClusterViewport*> UnusedViewports;
		for (FDisplayClusterViewport* It : ViewportManager.ImplGetViewports())
		{
			// ignore ICVFX internal resources
			if ((It->GetRenderSettingsICVFX().RuntimeFlags & ViewportRuntime_InternalResource) == 0)
			{
				if (InViewportNames.Find(It->GetId()) == INDEX_NONE)
				{
					UnusedViewports.Add(It);
				}
			}
		}

		// Delete unused viewports
		for (FDisplayClusterViewport* DeleteIt : UnusedViewports)
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
				if (ViewportIt.Key.Len() && ViewportIt.Value && InViewportNames.Find(ViewportIt.Key) != INDEX_NONE)
				{
					FDisplayClusterViewport* ExistViewport = ViewportManager.ImplFindViewport(ViewportIt.Key);
					if (ExistViewport)
					{
						FDisplayClusterViewportConfigurationBase::UpdateViewportConfiguration(ViewportManager, RootActor, ExistViewport, ViewportIt.Value);
					}
					else
					{
						ViewportManager.CreateViewport(ViewportIt.Key, ViewportIt.Value);
					}
				}
			}
		}
	}

	// Do not use the cluster node name for this pass type
	InOutRenderFrameSettings.ClusterNodeId.Empty();
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
			static const FString TextureShareID(TEXT("TextureShare"));
			static IConsoleVariable* const CVarTextureShareEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("nDisplay.render.texturesharing"));

			// TextureShare with nDisplay is only supported for PIE and Runtime.
			// For PIE in nDisplay, the node must be selected in the preview options.
			const bool bEnableTextureSharePP = (CVarTextureShareEnabled && CVarTextureShareEnabled->GetInt() != 0) && ClusterNode->bEnableTextureShare && !InRenderFrameSettings.bIsPreviewRendering;

			{
				// Find unused PP:
				TArray<FString> UnusedPP;
				for (const FString& It : PPManager->GetPostprocess())
				{
					// Leave defined postprocess (dynamic reconf)
					bool IsDefinedPostProcess = ClusterNode->Postprocess.Contains(It);

					// Support TextureShare dynamic reconf
					if(bEnableTextureSharePP && It == TextureShareID)
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

			// Create and update PP
			for (const TPair<FString, FDisplayClusterConfigurationPostprocess>& It : ClusterNode->Postprocess)
			{
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
					PPManager->CreatePostprocess(It.Key, &It.Value);
				}
			}

			// Texture sharing is not supported in preview mode.
			if(bEnableTextureSharePP)
				{
					TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe> ExistPostProcess = PPManager->FindPostProcess(TextureShareID);
					if (!ExistPostProcess.IsValid())
					{
						// Helper create postprocess entry for TextureShare
						FDisplayClusterConfigurationPostprocess TextureShareConfiguration;
						TextureShareConfiguration.Type = TextureShareID;

						PPManager->CreatePostprocess(TextureShareID, &TextureShareConfiguration);
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
bool FDisplayClusterViewportConfigurationBase::UpdateViewportConfiguration(FDisplayClusterViewportManager& ViewportManager, ADisplayClusterRootActor& RootActor, FDisplayClusterViewport* DesiredViewport, const UDisplayClusterConfigurationViewport* ConfigurationViewport)
{
	check(IsInGameThread());
	check(DesiredViewport);
	check(ConfigurationViewport);

	FDisplayClusterViewportConfigurationHelpers::UpdateBaseViewportSetting(*DesiredViewport, RootActor, *ConfigurationViewport);
	FDisplayClusterViewportConfigurationHelpers::UpdateProjectionPolicy(*DesiredViewport, &(ConfigurationViewport->ProjectionPolicy));

	return true;
}
