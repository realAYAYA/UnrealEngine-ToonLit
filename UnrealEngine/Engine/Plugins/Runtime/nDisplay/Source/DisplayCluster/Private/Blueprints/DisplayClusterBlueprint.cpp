// Copyright Epic Games, Inc. All Rights Reserved.

#include "Blueprints/DisplayClusterBlueprint.h"
#include "Blueprints/DisplayClusterBlueprintGeneratedClass.h"
#include "DisplayClusterRootActor.h"
#include "Components/DisplayClusterCameraComponent.h"
#include "Components/DisplayClusterScreenComponent.h"

#include "IDisplayClusterConfiguration.h"

#include "EngineAnalytics.h"
#include "Engine/SCS_Node.h"
#include "Containers/Set.h"

#include "Misc/DisplayClusterLog.h"
#include "UObject/ObjectSaveContext.h"


UDisplayClusterBlueprint::UDisplayClusterBlueprint()
	: ConfigData(nullptr), AssetVersion(0)
{
	BlueprintType = BPTYPE_Normal;
#if WITH_EDITORONLY_DATA
	bRunConstructionScriptOnInteractiveChange = false;
#endif
}

#if WITH_EDITOR

UClass* UDisplayClusterBlueprint::GetBlueprintClass() const
{
	return UDisplayClusterBlueprintGeneratedClass::StaticClass();
}

void UDisplayClusterBlueprint::GetReparentingRules(TSet<const UClass*>& AllowedChildrenOfClasses,
	TSet<const UClass*>& DisallowedChildrenOfClasses) const
{
	AllowedChildrenOfClasses.Add(ADisplayClusterRootActor::StaticClass());
}
#endif

void UDisplayClusterBlueprint::UpdateConfigExportProperty()
{
	bool bConfigExported = false;

	if (UDisplayClusterConfigurationData* Config = GetOrLoadConfig())
	{
		PrepareConfigForExport();
		
		FString PrettyConfig;

		bConfigExported = IDisplayClusterConfiguration::Get().ConfigAsString(Config, PrettyConfig);

		if (bConfigExported)
		{
			// We cache a somewhat minified version of the config so that the context view of the asset registry data is less bloated.

			ConfigExport.Empty(PrettyConfig.Len());

			for (auto CharIt = PrettyConfig.CreateConstIterator(); CharIt; ++CharIt)
			{
				const TCHAR Char = *CharIt;

				// Remove tabs, carriage returns and newlines.
				if ((Char == TCHAR('\t')) || (Char == TCHAR('\r')) || (Char == TCHAR('\n')))
				{
					continue;
				}

				ConfigExport.AppendChar(Char);
			}
		}
	}

	if (!bConfigExported)
	{
		ConfigExport = TEXT("");
	}
}

namespace DisplayClusterBlueprint
{
	void SendAnalytics(const FString& EventName, const UDisplayClusterConfigurationData* const ConfigData)
	{
		if (!FEngineAnalytics::IsAvailable())
		{
			return;
		}

		// Gather attributes related to this config
		TArray<FAnalyticsEventAttribute> EventAttributes;

		if (ConfigData)
		{
			if (ConfigData->Cluster)
			{
				// Number of Nodes
				EventAttributes.Add(FAnalyticsEventAttribute(TEXT("NumNodes"), ConfigData->Cluster->Nodes.Num()));

				// Number of Viewports
				TSet<FString> UniquelyNamedViewports;

				for (auto NodesIt = ConfigData->Cluster->Nodes.CreateConstIterator(); NodesIt; ++NodesIt)
				{
					for (auto ViewportsIt = ConfigData->Cluster->Nodes.CreateConstIterator(); ViewportsIt; ++ViewportsIt)
					{
						UniquelyNamedViewports.Add(ViewportsIt->Key);
					}
				}

				// Number of uniquely named viewports
				EventAttributes.Add(FAnalyticsEventAttribute(TEXT("NumUniquelyNamedViewports"), UniquelyNamedViewports.Num()));
			}
		}

		FEngineAnalytics::GetProvider().RecordEvent(EventName, EventAttributes);
	}
}

void UDisplayClusterBlueprint::PreSave(FObjectPreSaveContext SaveContext)
{
	Super::PreSave(SaveContext);

	UpdateConfigExportProperty();
	DisplayClusterBlueprint::SendAnalytics(TEXT("Usage.nDisplay.ConfigSaved"), ConfigData);
}

UDisplayClusterBlueprintGeneratedClass* UDisplayClusterBlueprint::GetGeneratedClass() const
{
	return Cast<UDisplayClusterBlueprintGeneratedClass>(*GeneratedClass);
}

UDisplayClusterConfigurationData* UDisplayClusterBlueprint::GetOrLoadConfig()
{
	if (GeneratedClass)
	{
		if (ADisplayClusterRootActor* CDO = Cast<ADisplayClusterRootActor>(GeneratedClass->ClassDefaultObject))
		{
			ConfigData = CDO->GetConfigData();
		}
	}
	
	return ConfigData;
}

void UDisplayClusterBlueprint::SetConfigData(UDisplayClusterConfigurationData* InConfigData, bool bForceRecreate)
{
#if WITH_EDITOR
	Modify();
#endif

	if (GeneratedClass)
	{
		if (ADisplayClusterRootActor* CDO = Cast<ADisplayClusterRootActor>(GeneratedClass->ClassDefaultObject))
		{
			CDO->UpdateConfigDataInstance(InConfigData, bForceRecreate);
			GetOrLoadConfig();
		}
	}
	
#if WITH_EDITORONLY_DATA
	if(InConfigData)
	{
		InConfigData->SaveConfig();
	}
#endif
}

const FString& UDisplayClusterBlueprint::GetConfigPath() const
{
	static FString EmptyString;
#if WITH_EDITORONLY_DATA
	return ConfigData ? ConfigData->PathToConfig : EmptyString;
#else
	return EmptyString;
#endif
}

void UDisplayClusterBlueprint::SetConfigPath(const FString& InPath)
{
#if WITH_EDITORONLY_DATA
	if(UDisplayClusterConfigurationData* LoadedConfigData = GetOrLoadConfig())
	{
		LoadedConfigData->PathToConfig = InPath;
		LoadedConfigData->SaveConfig();
	}
#endif
}

void UDisplayClusterBlueprint::PrepareConfigForExport()
{
	if (!ensure(GeneratedClass))
	{
		return;
	}
		
	ADisplayClusterRootActor* CDO = CastChecked<ADisplayClusterRootActor>(GeneratedClass->ClassDefaultObject);

	UDisplayClusterConfigurationData* Data = GetOrLoadConfig();
	check(Data);
	
	// Components to export
	TArray<UDisplayClusterCameraComponent*> CameraComponents;
	TArray<UDisplayClusterScreenComponent*> ScreenComponents;
	TArray<USceneComponent*>  XformComponents;
	// Auxiliary map for building hierarchy
	TMap<UActorComponent*, FString> ParentComponentsMap;

	// Make sure the 'Scene' object is there. Otherwise instantiate it.
	// Could be null on assets used during 4.27 development, before scene was added back in.
	const EObjectFlags CommonFlags = RF_Public | RF_Transactional;
	if (Data->Scene == nullptr)
	{
		Data->Scene = NewObject<UDisplayClusterConfigurationScene>(
			this,
			UDisplayClusterConfigurationScene::StaticClass(),
			NAME_None,
			IsTemplate() ? RF_ArchetypeObject | CommonFlags : CommonFlags);
	}
	
	// Extract CDO cameras (no screens in the CDO)
	// Get list of cameras
	CDO->GetComponents(CameraComponents);

	// Extract BP components

	const TArray<USCS_Node*>& Nodes = SimpleConstructionScript->GetAllNodes();
	for (const USCS_Node* const Node : Nodes)
	{
		// Fill ID info for all descendants
		GatherParentComponentsInfo(Node, ParentComponentsMap);

		// Cameras
		if (Node->ComponentClass->IsChildOf(UDisplayClusterCameraComponent::StaticClass()))
		{
			UDisplayClusterCameraComponent* ComponentTemplate = CastChecked<UDisplayClusterCameraComponent>(Node->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(GeneratedClass)));
			CameraComponents.Add(ComponentTemplate);
		}
		// Screens
		else if (Node->ComponentClass->IsChildOf(UDisplayClusterScreenComponent::StaticClass()))
		{
			UDisplayClusterScreenComponent* ComponentTemplate = CastChecked<UDisplayClusterScreenComponent>(Node->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(GeneratedClass)));
			ScreenComponents.Add(ComponentTemplate);
		}
		// All other components will be exported as Xforms
		else if (Node->ComponentClass->IsChildOf(USceneComponent::StaticClass()))
		{
			USceneComponent* ComponentTemplate = CastChecked<USceneComponent>(Node->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(GeneratedClass)));
			XformComponents.Add(ComponentTemplate);
		}
	}

	// Save asset path
	Data->Info.AssetPath = GetPathName();

	// Prepare the target containers
	Data->Scene->Cameras.Empty(CameraComponents.Num());
	Data->Scene->Screens.Empty(ScreenComponents.Num());
	Data->Scene->Xforms.Empty(XformComponents.Num());

	// Export cameras
	for (const UDisplayClusterCameraComponent* const CfgComp : CameraComponents)
	{
		UDisplayClusterConfigurationSceneComponentCamera* SceneComp = NewObject<UDisplayClusterConfigurationSceneComponentCamera>(Data->Scene, CfgComp->GetFName(), RF_Public);

		// Save the properties
		SceneComp->bSwapEyes = CfgComp->GetSwapEyes();
		SceneComp->InterpupillaryDistance = CfgComp->GetInterpupillaryDistance();
		// Safe to cast -- values match.
		SceneComp->StereoOffset = (EDisplayClusterConfigurationEyeStereoOffset)CfgComp->GetStereoOffset();

		FString* ParentId = ParentComponentsMap.Find(CfgComp);
		SceneComp->ParentId = (ParentId ? *ParentId : FString());
		SceneComp->Location = CfgComp->GetRelativeLocation();
		SceneComp->Rotation = CfgComp->GetRelativeRotation();

		// Store the object
		Data->Scene->Cameras.Emplace(GetObjectNameFromSCSNode(SceneComp), SceneComp);
	}

	// Export screens
	for (const UDisplayClusterScreenComponent* const CfgComp : ScreenComponents)
	{
		UDisplayClusterConfigurationSceneComponentScreen* SceneComp = NewObject<UDisplayClusterConfigurationSceneComponentScreen>(Data->Scene, CfgComp->GetFName());

		// Save the properties
		FString* ParentId = ParentComponentsMap.Find(CfgComp);
		SceneComp->ParentId = (ParentId ? *ParentId : FString());
		SceneComp->Location = CfgComp->GetRelativeLocation();
		SceneComp->Rotation = CfgComp->GetRelativeRotation();

		const FVector RelativeCompScale = CfgComp->GetRelativeScale3D();
		SceneComp->Size = FVector2D(RelativeCompScale.Y, RelativeCompScale.Z);

		// Store the object
		Data->Scene->Screens.Emplace(GetObjectNameFromSCSNode(SceneComp), SceneComp);
	}

	// Export xforms
	for (const USceneComponent* const CfgComp : XformComponents)
	{
		UDisplayClusterConfigurationSceneComponentXform* SceneComp = NewObject<UDisplayClusterConfigurationSceneComponentXform>(Data->Scene, CfgComp->GetFName());

		// Save the properties
		FString* ParentId = ParentComponentsMap.Find(CfgComp);
		SceneComp->ParentId = (ParentId ? *ParentId : FString());
		SceneComp->Location = CfgComp->GetRelativeLocation();
		SceneComp->Rotation = CfgComp->GetRelativeRotation();

		// Store the object
		Data->Scene->Xforms.Emplace(GetObjectNameFromSCSNode(SceneComp), SceneComp);
	}

	// Avoid empty string keys in the config data maps
	CleanupConfigMaps(Data);
}

FString UDisplayClusterBlueprint::GetObjectNameFromSCSNode(const UObject* const Object) const
{
	FString OutCompName;

	if (Object)
	{
		OutCompName = Object->GetName();
		OutCompName.RemoveFromEnd(TEXT("_GEN_VARIABLE"));
	}

	return OutCompName;
}

void UDisplayClusterBlueprint::GatherParentComponentsInfo(const USCS_Node* const InNode,
	TMap<UActorComponent*, FString>& OutParentsMap) const
{
	if (InNode && InNode->ComponentClass->IsChildOf(UActorComponent::StaticClass()))
	{
		// Save current node to the map
		UActorComponent* ParentNode = InNode->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(GeneratedClass));
		if (!OutParentsMap.Contains(ParentNode))
		{
			OutParentsMap.Emplace(ParentNode);
		}

		// Now iterate through the children nodes
		for (USCS_Node* ChildNode : InNode->ChildNodes)
		{
			UActorComponent* ChildComponentTemplate = CastChecked<UActorComponent>(ChildNode->GetActualComponentTemplate(CastChecked<UBlueprintGeneratedClass>(GeneratedClass)));
			if (ChildComponentTemplate)
			{
				OutParentsMap.Emplace(ChildComponentTemplate, GetObjectNameFromSCSNode(InNode->ComponentTemplate));
			}

			GatherParentComponentsInfo(ChildNode, OutParentsMap);
		}
	}
}

void UDisplayClusterBlueprint::CleanupConfigMaps(UDisplayClusterConfigurationData* Data) const
{
	check(Data && Data->Cluster);

	static const FString InvalidKey = FString();

	// Set of the maps we're going to process
	TSet<TMap<FString, FString>*> MapsToProcess;
	// Pre-allocate some memory. Not a precise amount of elements, but should be enough in most cases
	MapsToProcess.Reserve(3 + 4 * Data->Cluster->Nodes.Num());

	// Add single instance maps
	MapsToProcess.Add(&Data->CustomParameters);
	MapsToProcess.Add(&Data->Cluster->Sync.InputSyncPolicy.Parameters);
	MapsToProcess.Add(&Data->Cluster->Sync.RenderSyncPolicy.Parameters);

	// Add per-node and per-viewport maps
	Data->Cluster->Nodes.Remove(InvalidKey);
	for (TPair<FString, UDisplayClusterConfigurationClusterNode*> Node : Data->Cluster->Nodes)
	{
		check(Node.Value);

		// Per-node maps
		Node.Value->Postprocess.Remove(InvalidKey);
		for (TPair<FString, FDisplayClusterConfigurationPostprocess> PostOpIt : Node.Value->Postprocess)
		{
			MapsToProcess.Add(&PostOpIt.Value.Parameters);
		}

		// Per-viewport maps
		Node.Value->Viewports.Remove(InvalidKey);
		for (TPair<FString, UDisplayClusterConfigurationViewport*> ViewportIt : Node.Value->Viewports)
		{
			check(ViewportIt.Value);
			MapsToProcess.Add(&ViewportIt.Value->ProjectionPolicy.Parameters);
		}
	}

	// Finally, remove all the pairs with empty keys
	for (TMap<FString, FString>* Map : MapsToProcess)
	{
		Map->Remove(InvalidKey);
	}
}
