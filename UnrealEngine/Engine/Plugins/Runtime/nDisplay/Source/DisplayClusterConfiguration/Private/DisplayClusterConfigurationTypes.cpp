// Copyright Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterConfigurationTypes.h"

#include "DisplayClusterConfigurationTypes_Base.h"
#include "DisplayClusterConfigurationTypes_Viewport.h"
#include "DisplayClusterConfigurationTypes_ICVFX.h"
#include "DisplayClusterConfigurationTypes_PostRender.h"

#include "DisplayClusterConfigurationLog.h"
#include "DisplayClusterConfigurationStrings.h"
#include "DisplayClusterProjectionStrings.h"

#include "Engine/StaticMesh.h"

#if WITH_EDITOR
#include "Kismet2/CompilerResultsLog.h"
#endif

#define SAVE_MAP_TO_ARRAY(Map, DestArray) \
	for (const auto& KeyVal : Map) \
	{ \
		auto Component = KeyVal.Value; \
		if(Component) \
		{ \
			DestArray.AddUnique(Component); \
		} \
	} \

#define SAVE_MAP(Map) \
	SAVE_MAP_TO_ARRAY(Map, OutObjects); \
	

FIntRect FDisplayClusterReplaceTextureCropRectangle::ToRect() const
{
	return FIntRect(FIntPoint(Origin.X, Origin.Y), FIntPoint(Origin.X + Size.W, Origin.Y + Size.H));
}

FIntRect FDisplayClusterConfigurationRectangle::ToRect() const
{
	return FIntRect(FIntPoint(X, Y), FIntPoint(X + W, Y + H));
}

UDisplayClusterConfigurationData::UDisplayClusterConfigurationData()
{
}

UDisplayClusterConfigurationViewport* UDisplayClusterConfigurationData::GetViewport(const FString& NodeId, const FString& ViewportId) const
{
	UDisplayClusterConfigurationClusterNode* Node = Cluster->GetNode(NodeId);
	if (Node)
	{
		TObjectPtr<UDisplayClusterConfigurationViewport>* Viewport = Node->Viewports.Find(ViewportId);
		if (Viewport)
		{
			return *Viewport;
		}
	}
	return nullptr;
}

bool UDisplayClusterConfigurationData::AssignPostprocess(const FString& NodeId, const FString& PostprocessId, const FString& Type, TMap<FString, FString> Parameters, int32 Order)
{
	if (Cluster && Cluster->Nodes.Contains(NodeId))
	{
		if (!PostprocessId.IsEmpty())
		{
			FDisplayClusterConfigurationPostprocess PostprocessData;
			PostprocessData.Type = Type;
			PostprocessData.Parameters.Append(Parameters);
			PostprocessData.Order = Order;

			Cluster->Nodes[NodeId]->Postprocess.Emplace(PostprocessId, PostprocessData);

			return true;
		}
	}

	return false;
}

bool UDisplayClusterConfigurationData::RemovePostprocess(const FString& NodeId, const FString& PostprocessId)
{
	if (Cluster && Cluster->Nodes.Contains(NodeId))
	{
		if (Cluster->Nodes[NodeId]->Postprocess.Contains(PostprocessId))
		{
			Cluster->Nodes[NodeId]->Postprocess.Remove(PostprocessId);
			return true;
		}
	}

	return false;
}

bool UDisplayClusterConfigurationData::GetPostprocess(const FString& NodeId, const FString& PostprocessId, FDisplayClusterConfigurationPostprocess& OutPostprocess) const
{
	if (!Cluster)
	{
		return false;
	}

	const UDisplayClusterConfigurationClusterNode* Node = Cluster->GetNode(NodeId);

	if (Node)
	{
		const FDisplayClusterConfigurationPostprocess* PostprocessOperation = Node->Postprocess.Find(PostprocessId);
		if (PostprocessOperation)
		{
			OutPostprocess = *PostprocessOperation;
			return true;
		}
	}

	return false;
}

bool UDisplayClusterConfigurationData::GetProjectionPolicy(const FString& NodeId, const FString& ViewportId, FDisplayClusterConfigurationProjection& OutProjection) const
{
	const UDisplayClusterConfigurationViewport* Viewport = GetViewport(NodeId, ViewportId);
	if (Viewport)
	{
		OutProjection = Viewport->ProjectionPolicy;
		return true;
	}

	return false;
}

#if WITH_EDITORONLY_DATA

const TSet<FString> UDisplayClusterConfigurationData::RenderSyncPolicies =
{
	TEXT("Ethernet"),
	TEXT("Nvidia"),
	TEXT("None")
};

const TSet<FString> UDisplayClusterConfigurationData::InputSyncPolicies =
{
	TEXT("ReplicatePrimary"),
	TEXT("None")
};

const TSet<FString> UDisplayClusterConfigurationData::ProjectionPolicies =
{
	TEXT("Simple"),
	TEXT("Camera"),
	TEXT("Mesh"),
	TEXT("MPCDI"),
	TEXT("EasyBlend"),
	TEXT("DomeProjection"),
	TEXT("VIOSO"),
	TEXT("Manual"),
};

#endif

FDisplayClusterConfigurationProjection::FDisplayClusterConfigurationProjection()
{
	Type = TEXT("simple");
}

FDisplayClusterConfigurationPostprocess::FDisplayClusterConfigurationPostprocess()
{

}

const float UDisplayClusterConfigurationViewport::ViewportMinimumSize = 1.0f;
const float UDisplayClusterConfigurationViewport::ViewportMaximumSize = 15360.0f;

UDisplayClusterConfigurationViewport::UDisplayClusterConfigurationViewport()
{
#if WITH_EDITORONLY_DATA
	bIsVisible = true;
	bIsUnlocked = true;
#endif
}

#if WITH_EDITOR
void UDisplayClusterConfigurationViewport::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	if (TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* ActiveMemberNode = PropertyAboutToChange.
		GetActiveMemberNode())
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(ActiveMemberNode->GetValue()))
		{
			if (StructProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationViewport, Region))
			{
				if (DisablePreviewTexture())
				{
					bIsManagingPreviewTexture = true;
				}
			}
		}
	}
	
	Super::PreEditChange(PropertyAboutToChange);
}

void UDisplayClusterConfigurationViewport::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		if (TDoubleLinkedList<FProperty*>::TDoubleLinkedListNode* ActiveMemberNode = PropertyChangedEvent.
			PropertyChain.GetActiveMemberNode())
		{
			if (const FStructProperty* StructProperty = CastField<FStructProperty>(ActiveMemberNode->GetValue()))
			{
				if (StructProperty->GetFName() == GET_MEMBER_NAME_CHECKED(UDisplayClusterConfigurationViewport, Region))
				{
					if (bIsManagingPreviewTexture)
					{
						EnablePreviewTexture();
					}
				}
			}
		}
	}
	
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	OnPostEditChangeChainProperty.Broadcast(PropertyChangedEvent);
}

void UDisplayClusterConfigurationViewport::EnablePreviewTexture()
{
	bAllowPreviewTexture = true;
	bIsManagingPreviewTexture = false;
}

bool UDisplayClusterConfigurationViewport::DisablePreviewTexture()
{
	if (bAllowPreviewTexture)
	{
		bAllowPreviewTexture = false;
		return true;
	}
	
	return false;
}

void UDisplayClusterConfigurationViewport::OnPreCompile(FCompilerResultsLog& MessageLog)
{
	Super::OnPreCompile(MessageLog);

	if (!ensure(bAllowPreviewTexture))
	{
		// Verify correct rendering value is applied. This branch shouldn't be hit as long as the Region
		// struct always has a final PostEditChangeProperty called without a change type of Interactive.
		EnablePreviewTexture();
	}
}

void UDisplayClusterConfigurationClusterNode::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	OnPostEditChangeChainProperty.Broadcast(PropertyChangedEvent);
}

void UDisplayClusterConfigurationHostDisplayData::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	OnPostEditChangeChainProperty.Broadcast(PropertyChangedEvent);
}
#endif

UDisplayClusterConfigurationClusterNode::UDisplayClusterConfigurationClusterNode()
	: bIsSoundEnabled(false)
#if WITH_EDITORONLY_DATA
	, bIsVisible(true)
	, bIsUnlocked(true)
#endif
{
}

UDisplayClusterConfigurationHostDisplayData::UDisplayClusterConfigurationHostDisplayData()
	: bIsVisible(true)
	, bIsUnlocked(true)
{
	SetFlags(RF_Public);
}

void UDisplayClusterConfigurationClusterNode::GetObjectsToExport(TArray<UObject*>& OutObjects)
{
	Super::GetObjectsToExport(OutObjects);
	SAVE_MAP(Viewports);
}

void UDisplayClusterConfigurationCluster::GetObjectsToExport(TArray<UObject*>& OutObjects)
{
	Super::GetObjectsToExport(OutObjects);
	SAVE_MAP(Nodes);
}

void UDisplayClusterConfigurationData_Base::Serialize(FArchive& Ar)
{
#if WITH_EDITOR
	if (Ar.IsSaving())
	{
		/*
		 * We need to set everything to public so it can be saved & referenced properly with the object.
		 * The object ownership doesn't seem to be correct at this stage either and is sometimes
		 * owned by the main data object, but since subobjects embed subobjects the correct parent
		 * should be set prior to save.
		 */

		SetFlags(RF_Public);
		ClearFlags(RF_Transient);
		
		ExportedObjects.Reset();
		GetObjectsToExport(ExportedObjects);
		for (UObject* Object : ExportedObjects)
		{
			if (!ensure(Object != nullptr))
			{
				UE_LOG(LogDisplayClusterConfiguration, Warning, TEXT("Null object passed to GetObjectsToExport"));
				continue;
			}
			if (Object->GetOuter() != this)
			{
				Object->Rename(nullptr, this, REN_ForceNoResetLoaders | REN_DoNotDirty | REN_DontCreateRedirectors);
			}
			Object->SetFlags(RF_Public);
			Object->ClearFlags(RF_Transient);
		}
	}
#endif
	
	Super::Serialize(Ar);
}

void UDisplayClusterConfigurationScene::GetObjectsToExport(TArray<UObject*>& OutObjects)
{
	Super::GetObjectsToExport(OutObjects);
	
	SAVE_MAP(Xforms);
	SAVE_MAP(Screens);
	SAVE_MAP(Cameras);
}

FDisplayClusterConfigurationPrimaryNodePorts::FDisplayClusterConfigurationPrimaryNodePorts()
	: ClusterSync(41001)
	, ClusterEventsJson  (41003)
	, ClusterEventsBinary(41004)
{
}

FDisplayClusterConfigurationClusterSync::FDisplayClusterConfigurationClusterSync()
{
	using namespace DisplayClusterConfigurationStrings::config;
	RenderSyncPolicy.Type = cluster::render_sync::Ethernet;
	InputSyncPolicy.Type  = cluster::input_sync::InputSyncPolicyReplicatePrimary;
}

FDisplayClusterConfigurationNetworkSettings::FDisplayClusterConfigurationNetworkSettings()
	: ConnectRetriesAmount     (300)   // ...
	, ConnectRetryDelay        (1000)  // 5 minutes unless all nodes up
	, GameStartBarrierTimeout  (1000 * 3600 * 5) // 5 hours unless ready to start rendering
	, FrameStartBarrierTimeout (1000 * 60 * 30)  // 30 minutes for the barrier
	, FrameEndBarrierTimeout   (1000 * 60 * 30)  // 30 minutes for the barrier
	, RenderSyncBarrierTimeout (1000 * 60 * 30)  // 30 minutes for the barrier
{
}

void UDisplayClusterConfigurationViewport::GetReferencedMeshNames(TArray<FString>& OutMeshNames) const
{
	// Collect all mesh references from projection policies
	for (const TPair<FString, FString>& It : ProjectionPolicy.Parameters)
	{
		if (ProjectionPolicy.Type.Compare(DisplayClusterProjectionStrings::projection::Mesh, ESearchCase::IgnoreCase) == 0)
		{
			if (It.Key.Compare(DisplayClusterProjectionStrings::cfg::mesh::Component, ESearchCase::IgnoreCase) == 0)
			{
				OutMeshNames.Add(It.Value);
			}
		}
	}
}

void UDisplayClusterConfigurationClusterNode::GetViewportIds(TArray<FString>& OutViewportIds) const
{
	Viewports.GenerateKeyArray(OutViewportIds);
}

UDisplayClusterConfigurationViewport* UDisplayClusterConfigurationClusterNode::GetViewport(const FString& ViewportId) const
{
	TObjectPtr<UDisplayClusterConfigurationViewport> const* Viewport = Viewports.Find(ViewportId);
	return Viewport ? *Viewport : nullptr;
}

void UDisplayClusterConfigurationClusterNode::GetReferencedMeshNames(TArray<FString>& OutMeshNames) const
{
	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationViewport>>& It : Viewports)
	{
		if (It.Value == nullptr)
		{
			// Could be null temporarily during a reimport when this is called.
			continue;
		}
		It.Value->GetReferencedMeshNames(OutMeshNames);
	}
}

void UDisplayClusterConfigurationCluster::GetNodeIds(TArray<FString>& OutNodeIds) const
{
	Nodes.GenerateKeyArray(OutNodeIds);
}

UDisplayClusterConfigurationClusterNode* UDisplayClusterConfigurationCluster::GetNode(const FString& NodeId) const
{
	TObjectPtr<UDisplayClusterConfigurationClusterNode> const* Node = Nodes.Find(NodeId);
	return Node ? *Node : nullptr;
}

void UDisplayClusterConfigurationCluster::GetReferencedMeshNames(TArray<FString>& OutMeshNames) const
{
	for (const TPair<FString, TObjectPtr<UDisplayClusterConfigurationClusterNode>>& It : Nodes)
	{
		if (It.Value == nullptr)
		{
			// Could be null temporarily during a reimport when this is called.
			continue;
		}
		It.Value->GetReferencedMeshNames(OutMeshNames);
	}
}

void UDisplayClusterConfigurationData::GetReferencedMeshNames(TArray<FString>& OutMeshNames) const
{
	if (Cluster != nullptr)
	{
		Cluster->GetReferencedMeshNames(OutMeshNames);
	}
}

UDisplayClusterConfigurationData* UDisplayClusterConfigurationData::CreateNewConfigData(UObject* Owner, EObjectFlags ObjectFlags)
{
	UDisplayClusterConfigurationData* NewConfigData = NewObject<UDisplayClusterConfigurationData>(Owner ? Owner : GetTransientPackage(), NAME_None,
		ObjectFlags | RF_ArchetypeObject | RF_Public | RF_Transactional);

	NewConfigData->Scene = NewObject<UDisplayClusterConfigurationScene>(NewConfigData, NAME_None,
		RF_ArchetypeObject | RF_Public | RF_Transactional);

	NewConfigData->Cluster = NewObject<UDisplayClusterConfigurationCluster>(NewConfigData, NAME_None,
		RF_ArchetypeObject | RF_Public | RF_Transactional);

	return NewConfigData;
}

FDisplayClusterConfigurationICVFX_CameraRenderSettings::FDisplayClusterConfigurationICVFX_CameraRenderSettings()
{
	// Setup incamera defaults:
	GenerateMips.bAutoGenerateMips = true;
}

FDisplayClusterConfigurationOCIOConfiguration::FDisplayClusterConfigurationOCIOConfiguration()
{
	OCIOConfiguration.bIsEnabled = true;
}

FDisplayClusterConfigurationOCIOProfile::FDisplayClusterConfigurationOCIOProfile()
{
	OCIOConfiguration.bIsEnabled = true;
}

FDisplayClusterConfigurationICVFX_CameraCustomFrustum::FDisplayClusterConfigurationICVFX_CameraCustomFrustum():
	EstimatedOverscanResolution(ForceInitToZero), InnerFrustumResolution(ForceInitToZero), OverscanPixelsIncrease(0.f)
{
}

FDisplayClusterConfigurationICVFX_CameraSettings::FDisplayClusterConfigurationICVFX_CameraSettings()
{
	AllNodesColorGrading.bEnableEntireClusterColorGrading = true;
}
