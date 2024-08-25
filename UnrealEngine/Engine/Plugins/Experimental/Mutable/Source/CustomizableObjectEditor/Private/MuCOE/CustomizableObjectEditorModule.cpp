// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectEditorModule.h"

#include "AssetToolsModule.h"
#include "GameFramework/Pawn.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "MessageLogModule.h"
#include "MuCO/CustomizableObjectSystem.h"		// For defines related to memory function replacements.
#include "MuCO/CustomizableObjectInstanceUsage.h"
#include "MuCO/CustomizableSkeletalMeshActor.h"
#include "MuCO/ICustomizableObjectModule.h"		// For instance editor command utility function
#include "MuCOE/CustomizableInstanceDetails.h"
#include "MuCOE/CustomizableObjectCustomSettings.h"
#include "MuCOE/CustomizableObjectCustomSettingsDetails.h"
#include "MuCOE/CustomizableObjectDetails.h"
#include "MuCOE/CustomizableObjectEditor.h"
#include "MuCOE/CustomizableObjectEditorLogger.h"
#include "MuCOE/CustomizableObjectEditorSettings.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/CustomizableObjectIdentifierCustomization.h"
#include "MuCOE/CustomizableObjectInstanceEditor.h"
#include "MuCOE/CustomizableObjectInstanceFactory.h"
#include "MuCOE/CustomizableObjectNodeObjectGroupDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeCopyMaterialDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeEditMaterialBaseDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExtendMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExternalPin.h"
#include "MuCOE/Nodes/CustomizableObjectNodeExternalPinDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeLayoutBlocksDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipMorphDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipWithMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshClipWithMeshDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorph.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshMorphDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshapeCommon.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMeshReshapeSelectionDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMorphMaterial.h"
#include "MuCOE/Nodes/CustomizableObjectNodeMorphMaterialDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeObjectGroup.h"
#include "MuCOE/Nodes/CustomizableObjectNodePinViewerDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeProjectorConstant.h"
#include "MuCOE/Nodes/CustomizableObjectNodeProjectorParameterDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeRemoveMeshBlocks.h"
#include "MuCOE/Nodes/CustomizableObjectNodeRemoveMeshBlocksDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeSkeletalMeshDetails.h"
#include "MuCOE/Nodes/CustomizableObjectNodeStaticMesh.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTable.h"
#include "MuCOE/Nodes/CustomizableObjectNodeTableDetails.h"
#include "MuCOE/Widgets/CustomizableObjectLODReductionSettings.h"
#include "PropertyEditorModule.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCOE/Nodes/CustomizableObjectNodeGroupProjectorParameter.h"
#include "UObject/UObjectIterator.h"
#include "Subsystems/PlacementSubsystem.h"
#include "Components/SkeletalMeshComponent.h"

class AActor;
class FString;
class ICustomizableObjectDebugger;
class ICustomizableObjectEditor;
class ICustomizableObjectInstanceEditor;
class IToolkitHost;
class UObject;


const FName CustomizableObjectEditorAppIdentifier = FName(TEXT("CustomizableObjectEditorApp"));
const FName CustomizableObjectInstanceEditorAppIdentifier = FName(TEXT("CustomizableObjectInstanceEditorApp"));
const FName CustomizableObjectDebuggerAppIdentifier = FName(TEXT("CustomizableObjectDebuggerApp"));

#define LOCTEXT_NAMESPACE "MutableSettings"


constexpr float ShowOnScreenCompileWarningsTickerTime = 1.0f;


void ShowOnScreenCompileWarnings()
{
	TSet<const UCustomizableObject*> Objects;
	
	for (TObjectIterator<UCustomizableObjectInstanceUsage> CustomizableObjectInstanceUsage; CustomizableObjectInstanceUsage; ++CustomizableObjectInstanceUsage)
	{
		if (!IsValid(*CustomizableObjectInstanceUsage) || CustomizableObjectInstanceUsage->IsTemplate())
		{
			continue;
		}
		
		const UCustomizableObjectInstance* Instance = CustomizableObjectInstanceUsage->GetCustomizableObjectInstance();
		if (!Instance)
		{
			continue;
		}

		const UCustomizableObject* Object = Cast<UCustomizableObject>(Instance->GetCustomizableObject());
		if (!Object)
		{
			continue;
		}
		
		const USkeletalMeshComponent* Parent = Cast<USkeletalMeshComponent>(CustomizableObjectInstanceUsage->GetAttachParent());
		if (!Parent)
		{
			continue;
		}

		const UWorld* World = Parent->GetWorld();
		if (!World)
		{
			continue;
		}

		if (World->WorldType != EWorldType::PIE)
		{
			continue;
		}

		Objects.Add(Object);
	}

	for (const UCustomizableObject* Object : Objects)
	{
		if (Object->GetPrivate()->Status.Get() != FCustomizableObjectStatus::EState::ModelLoaded)
		{
			continue;
		}
	
		// Show a warning if the compilation was not done with optimizations.
		const uint64 KeyCompiledWithOptimization = reinterpret_cast<uint64>(Object);
		if (Object->GetPrivate()->bIsCompiledWithoutOptimization)
		{
			FString Msg = FString::Printf(TEXT("Customizable Object [%s] was compiled without optimization."), *Object->GetName());
			GEngine->AddOnScreenDebugMessage(KeyCompiledWithOptimization, ShowOnScreenCompileWarningsTickerTime * 2.0f, FColor::Yellow, Msg);
		}
		else
		{
			GEngine->RemoveOnScreenDebugMessage(KeyCompiledWithOptimization);
		}
		
		const uint64 KeyCompiledOutOfDate = reinterpret_cast<uint64>(Object) + KEY_OFFSET_COMPILATION_OUT_OF_DATE; // Offset added to avoid collision with bIsCompiledWithOptimization warning
		TArray<FName> OutOfDatePackages;
		if (Object->GetPrivate()->IsCompilationOutOfDate(&OutOfDatePackages))
		{
			FString Msg = FString::Printf(TEXT("Customizable Object [%s] compilation out of date. See the Output Log for more information."), *Object->GetName());
			GEngine->AddOnScreenDebugMessage(KeyCompiledOutOfDate, ShowOnScreenCompileWarningsTickerTime * 2.0f, FColor::Yellow, Msg);
			
			if (!GEngine->OnScreenDebugMessageExists(KeyCompiledOutOfDate))
			{
				UE_LOG(LogMutable, Verbose, TEXT("Warning: Customizable Object [%s] compilation out of date. Modified packages since last compilation:"), *Object->GetName());
				for (const FName& OutOfDatePackage : OutOfDatePackages)
				{
					UE_LOG(LogMutable, Verbose, TEXT("%s"), *OutOfDatePackage.ToString());
				}
			}
		}
		else
		{
			GEngine->RemoveOnScreenDebugMessage(KeyCompiledOutOfDate);
		}
	}
}


IMPLEMENT_MODULE( FCustomizableObjectEditorModule, CustomizableObjectEditor );


static void* CustomMalloc(std::size_t Size_t, uint32_t Alignment)
{
	return FMemory::Malloc(Size_t, Alignment);
}


static void CustomFree(void* mem)
{
	return FMemory::Free(mem);
}


FCustomizableObjectCompilerBase* NewCompiler()
{
	return new FCustomizableObjectCompiler();
}


void FCustomizableObjectEditorModule::StartupModule()
{
	UCustomizableObjectSystem::GetInstance()->SetNewCompilerFunc(NewCompiler);

	// Register the thumbnail renderers
	//UThumbnailManager::Get().RegisterCustomRenderer(UCustomizableObject::StaticClass(), UCustomizableObjectThumbnailRenderer::StaticClass());
	//UThumbnailManager::Get().RegisterCustomRenderer(UCustomizableObjectInstance::StaticClass(), UCustomizableObjectInstanceThumbnailRenderer::StaticClass());
	
	// Property views
	FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	RegisterCustomDetails(PropertyModule, UCustomizableObject::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectInstance::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableInstanceDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeLayoutBlocks::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeLayoutBlocksDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeEditMaterial::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeEditMaterialDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeRemoveMeshBlocks::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeRemoveMeshBlocksDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeExtendMaterial::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeParentedMaterialDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeEditMaterialBase::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeEditMaterialBaseDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeMorphMaterial::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeMorphMaterialDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeObject::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeObjectDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeObjectGroup::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeObjectGroupDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeProjectorParameter::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeProjectorParameterDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeProjectorConstant::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeProjectorParameterDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeMeshMorph::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeMeshMorphDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeMeshClipMorph::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeMeshClipMorphDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeMeshClipWithMesh::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeMeshClipWithMeshDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeExternalPin::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeExternalPinDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomSettings::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectCustomSettingsDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeMaterial::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodePinViewerDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeCopyMaterial::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeCopyMaterialDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeSkeletalMesh::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeSkeletalMeshDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeStaticMesh::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodePinViewerDetails::MakeInstance));
	RegisterCustomDetails(PropertyModule, UCustomizableObjectNodeTable::StaticClass(), FOnGetDetailCustomizationInstance::CreateStatic(&FCustomizableObjectNodeTableDetails::MakeInstance));

	//Custom properties
	PropertyModule.RegisterCustomPropertyTypeLayout("CustomizableObjectIdentifier", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCustomizableObjectIdentifierCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FMeshReshapeBoneReference::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FMeshReshapeBonesReferenceCustomization::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout(FBoneToRemove::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FCustomizableObjectLODReductionSettings::MakeInstance));

	PropertyModule.NotifyCustomizationModuleChanged();

	// Register factory
	FCoreDelegates::OnPostEngineInit.AddRaw(this,&FCustomizableObjectEditorModule::RegisterFactory);

	// Additional UI style
	FCustomizableObjectEditorStyle::Initialize();

	RegisterSettings();

	// Create the message log category
	FMessageLogModule& MessageLogModule = FModuleManager::LoadModuleChecked<FMessageLogModule>("MessageLog");
	MessageLogModule.RegisterLogListing(FName("Mutable"), LOCTEXT("MutableLog", "Mutable"));

	CustomizableObjectEditor_ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
	CustomizableObjectEditor_MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);

	LaunchCOIECommand = IConsoleManager::Get().RegisterConsoleCommand(
		TEXT("mutable.OpenCOIE"),
		TEXT("Looks for a Customizable Object Instance within the player pawn and opens its Customizable Object Instance Editor. Specify slot ID to control which component is edited."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&FCustomizableObjectEditorModule::OpenCOIE));

	WarningsTickerHandle = FTSTicker::GetCoreTicker().AddTicker(TEXT("ShowOnScreenCompileWarnings"), ShowOnScreenCompileWarningsTickerTime, [](float)
	{
		ShowOnScreenCompileWarnings();
		return true;
	});
}


void FCustomizableObjectEditorModule::ShutdownModule()
{
	if( FModuleManager::Get().IsModuleLoaded( "PropertyEditor" ) )
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		for (const auto& ClassName : RegisteredCustomDetails)
		{
			PropertyModule.UnregisterCustomClassLayout(ClassName);
		}

		PropertyModule.NotifyCustomizationModuleChanged();
	}

	CustomizableObjectEditor_ToolBarExtensibilityManager.Reset();
	CustomizableObjectEditor_MenuExtensibilityManager.Reset();

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	FCustomizableObjectEditorStyle::Shutdown();

	FTSTicker::GetCoreTicker().RemoveTicker(WarningsTickerHandle);
}


FCustomizableObjectEditorLogger& FCustomizableObjectEditorModule::GetLogger()
{
	return Logger;
}


bool FCustomizableObjectEditorModule::HandleSettingsSaved()
{
	UCustomizableObjectEditorSettings* CustomizableObjectSettings = GetMutableDefault<UCustomizableObjectEditorSettings>();

	if (CustomizableObjectSettings != nullptr)
	{
		CustomizableObjectSettings->SaveConfig();
		
		FEditorCompileSettings CompileSettings;
		CompileSettings.bIsMutableEnabled = !CustomizableObjectSettings->bDisableMutableCompileInEditor;
		CompileSettings.bEnableAutomaticCompilation = CustomizableObjectSettings->bEnableAutomaticCompilation;
		CompileSettings.bCompileObjectsSynchronously = CustomizableObjectSettings->bCompileObjectsSynchronously;
		CompileSettings.bCompileRootObjectsOnStartPIE = CustomizableObjectSettings->bCompileRootObjectsOnStartPIE;
		
		UCustomizableObjectSystem::GetInstance()->EditorSettingsChanged(CompileSettings);
	}

    return true;
}


void FCustomizableObjectEditorModule::RegisterSettings()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
    {
        ISettingsSectionPtr SettingsSectionPtr = SettingsModule->RegisterSettings("Project", "Plugins", "CustomizableObjectSettings",
            LOCTEXT("MutableSettings_Setting", "Mutable"),
            LOCTEXT("MutableSettings_Setting_Desc", "Mutable Settings"),
            GetMutableDefault<UCustomizableObjectEditorSettings>()
        );

        if (SettingsSectionPtr.IsValid())
        {
            SettingsSectionPtr->OnModified().BindRaw(this, &FCustomizableObjectEditorModule::HandleSettingsSaved);
        }

		if (UCustomizableObjectSystem::GetInstance() != nullptr)
		{
			UCustomizableObjectEditorSettings* CustomizableObjectSettings = GetMutableDefault<UCustomizableObjectEditorSettings>();
			if (CustomizableObjectSettings != nullptr)
			{
				FEditorCompileSettings CompileSettings;
				CompileSettings.bIsMutableEnabled = !CustomizableObjectSettings->bDisableMutableCompileInEditor;
				CompileSettings.bEnableAutomaticCompilation = CustomizableObjectSettings->bEnableAutomaticCompilation;
				CompileSettings.bCompileObjectsSynchronously = CustomizableObjectSettings->bCompileObjectsSynchronously;
				CompileSettings.bCompileRootObjectsOnStartPIE = CustomizableObjectSettings->bCompileRootObjectsOnStartPIE;
				
				UCustomizableObjectSystem::GetInstance()->EditorSettingsChanged(CompileSettings);
			}
		}
    }
}


void FCustomizableObjectEditorModule::RegisterCustomDetails(FPropertyEditorModule& PropertyModule, const UClass* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate)
{
	const FName ClassName = FName(Class->GetName());
	PropertyModule.RegisterCustomClassLayout(ClassName, DetailLayoutDelegate);

	RegisteredCustomDetails.Add(ClassName);
}


void FCustomizableObjectEditorModule::OpenCOIE(const TArray<FString>& Arguments)
{
	int32 SlotID = INDEX_NONE;
	if (Arguments.Num() >= 1)
	{
		SlotID = FCString::Atoi(*Arguments[0]);
	}

	const UWorld* CurrentWorld = []() -> const UWorld*
	{
		UWorld* WorldForCurrentCOI = nullptr;
		const TIndirectArray<FWorldContext>& WorldContexts = GEngine->GetWorldContexts();
		for (const FWorldContext& Context : WorldContexts)
		{
			if ((Context.WorldType == EWorldType::Game) && (Context.World() != NULL))
			{
				WorldForCurrentCOI = Context.World();
			}
		}
		// Fall back to GWorld if we don't actually have a world.
		if (WorldForCurrentCOI == nullptr)
		{
			WorldForCurrentCOI = GWorld;
		}
		return WorldForCurrentCOI;
	}();
	const int32 PlayerIndex = 0;

	// Open the Customizable Object Instance Editor
	if (UCustomizableObjectInstanceUsage* SelectedCustomizableObjectInstanceUsage = GetPlayerCustomizableObjectInstanceUsage(SlotID, CurrentWorld, PlayerIndex))
	{
		if (UCustomizableObjectInstance* COInstance = SelectedCustomizableObjectInstanceUsage->GetCustomizableObjectInstance())
		{
			FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
			TWeakPtr<IAssetTypeActions> WeakAssetTypeActions = AssetToolsModule.Get().GetAssetTypeActionsForClass(UCustomizableObjectInstance::StaticClass());

			if (TSharedPtr<IAssetTypeActions> AssetTypeActions = WeakAssetTypeActions.Pin())
			{
				TArray<UObject*> AssetsToEdit;
				AssetsToEdit.Add(COInstance);
				AssetTypeActions->OpenAssetEditor(AssetsToEdit);
			}
		}
	}
}


void FCustomizableObjectEditorModule::RegisterFactory()
{
	if (GEditor)
	{
		GEditor->ActorFactories.Add(NewObject<UCustomizableObjectInstanceFactory>());
		if (UPlacementSubsystem* PlacementSubsystem = GEditor->GetEditorSubsystem<UPlacementSubsystem>())
		{
			PlacementSubsystem->RegisterAssetFactory(NewObject<UCustomizableObjectInstanceFactory>());
		}
	}
}


/** Recursively get all Customizable Objects that reference the given Customizable Object. */
void GetReferencingCustomizableObjects(FName CustomizableObjectName, TArray<FName>& VisitedObjectNames, TArray<FName>& ObjectNames)
{
	if (VisitedObjectNames.Contains(CustomizableObjectName))
	{
		return;
	}

	VisitedObjectNames.Add(CustomizableObjectName);

	TArray<FName> ReferencedObjectNames;
	
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().GetReferencers(CustomizableObjectName, ReferencedObjectNames, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);

	TArray<FAssetData> AssetDataArray;

	FARFilter Filter;
	Filter.PackageNames = MoveTemp(ReferencedObjectNames);
	
	AssetRegistryModule.Get().GetAssets(Filter, AssetDataArray);

	for (FAssetData AssetData : AssetDataArray)
	{
		if (AssetData.GetClass() == UCustomizableObject::StaticClass())
		{
			FName ReferencedObjectName = AssetData.GetPackage()->GetFName();
	
			ObjectNames.Add(ReferencedObjectName);

			GetReferencingCustomizableObjects(ReferencedObjectName, VisitedObjectNames, ObjectNames);
		}			
	}
}


void GetReferencingPackages(const UCustomizableObject& Object, TArray<FName>& ObjectNames)
{
	// Gather all child CustomizableObjects
	TArray<FName> VisitedObjectNames;
	GetReferencingCustomizableObjects(Object.GetPackage()->GetFName(), VisitedObjectNames, ObjectNames);

	// Gather all tables which will composite the final tables
	TArray<FName> CustomizableObjectNames = ObjectNames;
	for (const FName& CustomizableObjectName : CustomizableObjectNames)
	{
		const TSoftObjectPtr SoftObjectPtr(CustomizableObjectName.ToString());

		const UCustomizableObject* ChildCustomizableObject = Cast<UCustomizableObject>(SoftObjectPtr.LoadSynchronous());
		if (!ChildCustomizableObject)
		{
			continue;
		}

		TArray<UCustomizableObjectNodeTable*> TableNodes;
		ChildCustomizableObject->Source->GetNodesOfClass(TableNodes);

		FARFilter Filter;
		Filter.ClassPaths.Add(FTopLevelAssetPath(UDataTable::StaticClass()));

		for (const UCustomizableObjectNodeTable* TableNode : TableNodes)
		{
			TArray<FAssetData> DataTableAssets = TableNode->GetParentTables();

			for (const FAssetData& DataTableAsset : DataTableAssets)
			{
				if (DataTableAsset.IsValid())
				{
					ObjectNames.AddUnique(DataTableAsset.PackageName);
				}
			}
		}		
	}
}


bool FCustomizableObjectEditorModule::IsCompilationOutOfDate(const UCustomizableObject& Object, TArray<FName>* OutOfDatePackages) const
{
	if (!Object.GetPrivate()->DirtyParticipatingObjects.IsEmpty())
	{
		if (OutOfDatePackages)
		{
			OutOfDatePackages->Append(Object.GetPrivate()->DirtyParticipatingObjects);
		}
		else
		{
			return false;
		}
	}
	
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	for (const TTuple<FName, FGuid>& ParticipatingObject : Object.GetPrivate()->ParticipatingObjects)
	{
		TSoftObjectPtr SoftObjectPtr(ParticipatingObject.Key.ToString());
		if (SoftObjectPtr) // If loaded
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			const FGuid PackageGuid = SoftObjectPtr->GetPackage()->GetGuid();
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			
			if (PackageGuid != ParticipatingObject.Value)
			{
				if (OutOfDatePackages)
				{
					OutOfDatePackages->AddUnique(ParticipatingObject.Key);
				}
				else
				{
					return false;
				}
			}
		}
		else // Not loaded
		{
			FAssetPackageData AssetPackageData;
			const UE::AssetRegistry::EExists Result = AssetRegistryModule.Get().TryGetAssetPackageData(ParticipatingObject.Key, AssetPackageData);
				
			if (Result != UE::AssetRegistry::EExists::Exists)
			{
				if (OutOfDatePackages)
				{
					OutOfDatePackages->AddUnique(ParticipatingObject.Key);
				}
				else
				{
					return false;
				}
			}

			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			const FGuid PackageGuid = AssetPackageData.PackageGuid;
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
			
			if (PackageGuid != ParticipatingObject.Value)
			{
				if (OutOfDatePackages)
				{
					OutOfDatePackages->AddUnique(ParticipatingObject.Key);
				}
				else
				{
					return false;
				}
			}
		}
	}

	TArray<FName> ReferencingObjectNames;
	GetReferencingPackages(Object, ReferencingObjectNames);
	
	for (const FName& ObjectName : ReferencingObjectNames)
	{
		TSoftObjectPtr ReferencingObject(ObjectName.ToString()); 

		if ((ReferencingObject && ReferencingObject->GetPackage()->IsDirty()) ||
			!Object.GetPrivate()->ParticipatingObjects.Contains(ObjectName)) // Must be in the participating objects, if not it means it did not exist when compiling the object.
		{
			if (OutOfDatePackages)
			{
				OutOfDatePackages->AddUnique(ObjectName);
			}
			else
			{
				return false;
			}
		}
	}

	return !OutOfDatePackages->IsEmpty();	
}


#undef LOCTEXT_NAMESPACE
