// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/CustomizableObjectInstanceEditor.h"

#include "AdvancedPreviewSceneModule.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/PoseAsset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "DetailsViewArgs.h"
#include "FileHelpers.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "IDetailsView.h"
#include "MuCO/CustomizableObjectInstance.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuCO/CustomizableSkeletalComponent.h"
#include "MuCOE/CustomizableObjectCompiler.h"
#include "MuCOE/CustomizableObjectEditorActions.h"
#include "MuCOE/CustomizableObjectEditorModule.h"
#include "MuCOE/CustomizableObjectEditorViewportClient.h"
#include "MuCOE/CustomizableObjectInstanceEditorActions.h"
#include "MuCOE/CustomizableObjectPreviewScene.h"
#include "MuCOE/SCustomizableObjectEditorTextureAnalyzer.h"
#include "MuCOE/SCustomizableObjectEditorViewport.h"
#include "MuCOE/UnrealEditorPortabilityHelpers.h"
#include "PropertyEditorModule.h"
#include "Selection.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

class FAdvancedPreviewScene;
class FProperty;

#define LOCTEXT_NAMESPACE "CustomizableObjectInstanceEditor"


DEFINE_LOG_CATEGORY_STATIC(LogCustomizableObjectInstanceEditor, Log, All);

const FName FCustomizableObjectInstanceEditor::ViewportTabId( TEXT( "CustomizableObjectInstanceEditor_Viewport" ) );
const FName FCustomizableObjectInstanceEditor::InstancePropertiesTabId( TEXT( "CustomizableObjectInstanceEditor_InstanceProperties" ) );
const FName FCustomizableObjectInstanceEditor::AdvancedPreviewSettingsTabId(TEXT("CustomizableObjectEditor_AdvancedPreviewSettings"));
const FName FCustomizableObjectInstanceEditor::TextureAnalyzerTabId(TEXT("CustomizableObjectInstanceEditor_TextureAnalyzer"));

void UUpdateClassWrapperClass::DelegatedCallback(UCustomizableObjectInstance* Instance)
{
	Delegate.ExecuteIfBound();
}

void FCustomizableObjectInstanceEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();

	InTabManager->RegisterTabSpawner( ViewportTabId, FOnSpawnTab::CreateSP(this, &FCustomizableObjectInstanceEditor::SpawnTab_Viewport) )
		.SetDisplayName( LOCTEXT( "ViewportTab", "Viewport" ) )
		.SetGroup( MenuStructure.GetToolsCategory() );
	
	InTabManager->RegisterTabSpawner( InstancePropertiesTabId, FOnSpawnTab::CreateSP(this, &FCustomizableObjectInstanceEditor::SpawnTab_InstanceProperties) )
		.SetDisplayName( LOCTEXT( "InstancePropertiesTab", "Instance Properties" ) )
		.SetGroup( MenuStructure.GetToolsCategory() );

	InTabManager->RegisterTabSpawner(AdvancedPreviewSettingsTabId, FOnSpawnTab::CreateSP(this, &FCustomizableObjectInstanceEditor::SpawnTab_AdvancedPreviewSettings))
		.SetDisplayName(LOCTEXT("AdvancedPreviewSettingsTab", "Advanced Preview Settings"))
		.SetGroup(MenuStructure.GetToolsCategory());

	InTabManager->RegisterTabSpawner(TextureAnalyzerTabId, FOnSpawnTab::CreateSP(this, &FCustomizableObjectInstanceEditor::SpawnTab_TextureAnalyzer))
		.SetDisplayName(LOCTEXT("InstanceTextureAnalyzer", "Texture Analyzer"));
}

void FCustomizableObjectInstanceEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner( ViewportTabId );
	InTabManager->UnregisterTabSpawner( InstancePropertiesTabId );
	InTabManager->UnregisterTabSpawner(AdvancedPreviewSettingsTabId);
	InTabManager->UnregisterTabSpawner(TextureAnalyzerTabId);
}


FCustomizableObjectInstanceEditor::FCustomizableObjectInstanceEditor()
{
	CustomizableObjectInstance = nullptr;
	PreviewStaticMeshComponent = nullptr;
	HelperCallback = nullptr;
	PoseAsset = nullptr;
}


FCustomizableObjectInstanceEditor::~FCustomizableObjectInstanceEditor()
{
	if (HelperCallback) 
	{
		CustomizableObjectInstance->UpdatedDelegate.RemoveDynamic(HelperCallback, &UUpdateClassWrapperClass::DelegatedCallback);
		HelperCallback->Delegate.Unbind();
		HelperCallback = nullptr;
	}

	CustomizableObjectInstance = nullptr;

	for (UCustomizableSkeletalComponent* PreviewCustomizableSkeletalComponent : PreviewCustomizableSkeletalComponents)
	{
		if (PreviewCustomizableSkeletalComponent)
		{
			PreviewCustomizableSkeletalComponent->DestroyComponent();
		}
	}

	PreviewCustomizableSkeletalComponents.Reset();
	PreviewSkeletalMeshComponents.Reset();
	PreviewStaticMeshComponent = nullptr;

	CustomizableInstanceDetailsView.Reset();
	Viewport.Reset();

	FCoreUObjectDelegates::OnObjectModified.Remove(OnObjectModifiedHandle);
	if (Compiler)
	{
		Compiler->ForceFinishCompilation();
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnFilesLoaded().RemoveAll(this);

	FCoreUObjectDelegates::OnObjectPropertyChanged.RemoveAll(this);
}


void FCustomizableObjectInstanceEditor::InitCustomizableObjectInstanceEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UCustomizableObjectInstance* InCustomizableObjectInstance )
{
	// Register our commands. This will only register them if not previously registered
	FCustomizableObjectInstanceEditorCommands::Register();
	FCustomizableObjectEditorViewportCommands::Register();

	BindCommands();

	bool bUpdateFromSelection = false;
	bool bIsLockable = false;
	bool bAllowSearch = true;

	FPropertyEditorModule& PropPlugin = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ENameAreaSettings::HideNameArea;
	DetailsViewArgs.bAllowSearch = false;
	//DetailsViewArgs.bShowActorLabel = false;
	DetailsViewArgs.bShowObjectLabel = false;
	CustomizableInstanceDetailsView = PropPlugin.CreateDetailView( DetailsViewArgs );
	CustomizableInstanceDetailsView->SetObject(InCustomizableObjectInstance, true);
	//CustomizableInstanceDetailsView->SetIsPropertyVisibleDelegate( FIsPropertyVisible::CreateStatic(&IsPropertyVisible) );

	TextureAnalyzer = SNew(SCustomizableObjecEditorTextureAnalyzer).CustomizableObjectInstanceEditor(this).CustomizableObjectEditor(nullptr);

	Viewport = SNew(SCustomizableObjectEditorViewportTabBody)
		.CustomizableObjectEditor(SharedThis(this));

	// Set the instance
	check(InCustomizableObjectInstance);
	CustomizableObjectInstance = InCustomizableObjectInstance;
	bOnlyRelevantParameters = InCustomizableObjectInstance->bShowOnlyRelevantParameters;
	bOnlyRuntimeParameters = InCustomizableObjectInstance->bShowOnlyRuntimeParameters;

	// Check if the asset registry has finished loading assets before compiling the object or updating the instance
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	if (AssetRegistryModule.Get().IsLoadingAssets())
	{
		AssetRegistryModule.Get().OnFilesLoaded().AddRaw(this, &FCustomizableObjectInstanceEditor::OnAssetRegistryLoadComplete);
		AssetRegistryLoaded = false;
	}
	else
	{
		UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject();

		// Compile for the first time if necessary
		if (CustomizableObject && !CustomizableObject->IsCompiled())
		{
			
			CompileObject(CustomizableObject);
		}
		else if (CustomizableObject)
		{
			CreatePreviewInstance();
		}
		
		Viewport->SetAssetRegistryLoaded(true);
	}

	FAdvancedPreviewSceneModule& AdvancedPreviewSceneModule = FModuleManager::LoadModuleChecked<FAdvancedPreviewSceneModule>("AdvancedPreviewScene");

	TSharedPtr<FAdvancedPreviewScene> AdvancedPreviewScene = StaticCastSharedPtr<FAdvancedPreviewScene>(Viewport->GetPreviewScene());
	AdvancedPreviewSettingsWidget = AdvancedPreviewSceneModule.CreateAdvancedPreviewSceneSettingsWidget(AdvancedPreviewScene.ToSharedRef());

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_CustomizableObjectInstanceEditor_Layout_v2.1" )
	->AddArea
	(
		FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.8f)
			->AddTab(ViewportTabId, ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.2f)
			->AddTab(InstancePropertiesTabId, ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.2f)
			->AddTab(AdvancedPreviewSettingsTabId, ETabState::OpenedTab)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, CustomizableObjectInstanceEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, CustomizableObjectInstance );

	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// Clears selection highlight.
	OnInstancePropertySelectionChanged(NULL);

	OnObjectModifiedHandle = FCoreUObjectDelegates::OnObjectModified.AddRaw(this, &FCustomizableObjectInstanceEditor::OnObjectModified);
}


FName FCustomizableObjectInstanceEditor::GetToolkitFName() const
{
	return FName("CustomizableObjectInstanceEditor");
}


FText FCustomizableObjectInstanceEditor::GetBaseToolkitName() const
{
	return LOCTEXT("ToolkitName", "Customizable Object Instance Editor");
}


void FCustomizableObjectInstanceEditor::CreatePreviewInstance()
{
	check(CustomizableObjectInstance);
	UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject();
	
	if (!CustomizableObject || (!CustomizableObject->IsCompiled() && !UCustomizableObjectSystem::GetInstance()->IsCompilationDisabled()))
	{
		return;
	}

	// Bind update delegate
	HelperCallback = NewObject<UUpdateClassWrapperClass>(GetTransientPackage());
	HelperCallback->Delegate.BindSP(this, &FCustomizableObjectInstanceEditor::OnUpdatePreviewInstance);
	CustomizableObjectInstance->UpdatedDelegate.AddDynamic(HelperCallback, &UUpdateClassWrapperClass::DelegatedCallback);

	PreviewStaticMeshComponent = nullptr;

	// Create a SkeletalMeshComponent for each component in the CO
	int32 NumMeshComponents = CustomizableObject->GetComponentCount();
	PreviewSkeletalMeshComponents.AddZeroed(NumMeshComponents);
	PreviewCustomizableSkeletalComponents.AddZeroed(NumMeshComponents);

	if (CustomizableObject->ReferenceSkeletalMeshes.Num())
	{
		for (int32 ComponentIndex = 0; ComponentIndex < NumMeshComponents; ++ComponentIndex)
		{
			PreviewCustomizableSkeletalComponents[ComponentIndex] = NewObject<UCustomizableSkeletalComponent>(UCustomizableSkeletalComponent::StaticClass());

			if (PreviewCustomizableSkeletalComponents[ComponentIndex])
			{
				PreviewCustomizableSkeletalComponents[ComponentIndex]->CustomizableObjectInstance = CustomizableObjectInstance;
				PreviewCustomizableSkeletalComponents[ComponentIndex]->ComponentIndex = ComponentIndex;

				PreviewSkeletalMeshComponents[ComponentIndex] = NewObject<UDebugSkelMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient);

				if (PreviewSkeletalMeshComponents[ComponentIndex])
				{
					PreviewCustomizableSkeletalComponents[ComponentIndex]->AttachToComponent(PreviewSkeletalMeshComponents[ComponentIndex], FAttachmentTransformRules::KeepRelativeTransform);
				}
				else
				{
					return;
				}
			}
			else
			{
				return;
			}
		}

		CustomizableObjectInstance->UpdateSkeletalMeshAsync(true, true);
		Viewport->SetPreviewComponents(PreviewSkeletalMeshComponents);
	}
}


void FCustomizableObjectInstanceEditor::OnAssetRegistryLoadComplete()
{
	AssetRegistryLoaded = true;
	Viewport->SetAssetRegistryLoaded(true);

	check(CustomizableObjectInstance);
	UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject();
	if (!CustomizableObject)
	{
		return;
	}

	// Compile for the first time if necessary
	if (!CustomizableObject->IsCompiled())
	{
		CompileObject(CustomizableObject);
	}
	else
	{
		CreatePreviewInstance();
	}
}


void FCustomizableObjectInstanceEditor::UpdatePreviewVisibility()
{
	const bool bEnableVisibility = CustomizableObjectInstance->SkeletalMeshStatus == ESkeletalMeshState::Correct;
	for (UDebugSkelMeshComponent* PreviewSkeletalMeshComponent : PreviewSkeletalMeshComponents)
	{
		PreviewSkeletalMeshComponent->SetVisibility(bEnableVisibility, true);
	}
}


void FCustomizableObjectInstanceEditor::SaveAsset_Execute()
{
	if (CustomizableObjectInstance)
	{
		return;
	}

	if (UPackage* Package = CustomizableObjectInstance->GetOutermost())
	{
		TArray<UPackage*> PackagesToSave;
		PackagesToSave.Add(Package);

		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, false, false);
	}
}

bool FCustomizableObjectInstanceEditor::CanOpenOrShowParent()
{
	if (!CustomizableObjectInstance)
	{
		return false;
	}

	return CustomizableObjectInstance->GetCustomizableObject() != nullptr;
}


void FCustomizableObjectInstanceEditor::ShowParentInContentBrowser()
{
	check(CustomizableObjectInstance);
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.Get().SyncBrowserToAssets(TArray<FAssetData>{FAssetData(CustomizableObjectInstance->GetCustomizableObject())});
}


void FCustomizableObjectInstanceEditor::OpenParentInEditor()
{
	check(CustomizableObjectInstance);
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(CustomizableObjectInstance->GetCustomizableObject());
}


void FCustomizableObjectInstanceEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	Collector.AddReferencedObject( CustomizableObjectInstance );
	
	for (auto& PreviewCustomizableSkeletalComponent : PreviewCustomizableSkeletalComponents)
	{
		Collector.AddReferencedObject(PreviewCustomizableSkeletalComponent);
	}

	Collector.AddReferencedObject( PreviewStaticMeshComponent );

	for (auto& PreviewSkeletalMeshComponent : PreviewSkeletalMeshComponents)
	{
		Collector.AddReferencedObject(PreviewSkeletalMeshComponent);
	}

	Collector.AddReferencedObject( HelperCallback );
}


TSharedRef<SDockTab> FCustomizableObjectInstanceEditor::SpawnTab_Viewport( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == ViewportTabId );

	return SNew(SDockTab)
		//.Icon(FCustomizableObjectEditorStyle::Get().GetBrush("CustomizableObjectEditor.Tabs.Preview"))
		.Label( FText::FromString( GetTabPrefix() + LOCTEXT("CustomizableObjectViewport_TabTitle", "Instance Viewport").ToString() ) )
		[
			Viewport.ToSharedRef()
		];
}


TSharedRef<SDockTab> FCustomizableObjectInstanceEditor::SpawnTab_InstanceProperties( const FSpawnTabArgs& Args )
{
	check( Args.GetTabId() == InstancePropertiesTabId );

	return SNew(SDockTab)
		//.Icon(FCustomizableObjectEditorStyle::Get().GetBrush("CustomizableObjectEditor.Tabs.CustomizableInstanceProperties"))
		.Label( FText::FromString( GetTabPrefix() + LOCTEXT( "CustomizableInstanceProperties_TabTitle", "Instance Properties" ).ToString() ) )
		[
			CustomizableInstanceDetailsView.ToSharedRef()
		];
}


TSharedRef<SDockTab> FCustomizableObjectInstanceEditor::SpawnTab_AdvancedPreviewSettings(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == AdvancedPreviewSettingsTabId);
	return SNew(SDockTab)
		//.Icon(FCustomizableObjectEditorStyle::Get().GetBrush("CustomizableObjectEditor.Tabs.PreviewSettings"))
		.Label(LOCTEXT("StaticMeshPreviewScene_TabTitle", "Preview Scene Settings"))
		[
			AdvancedPreviewSettingsWidget.ToSharedRef()
		];
}


UCustomizableObjectInstance* FCustomizableObjectInstanceEditor::GetPreviewInstance()
{
	return CustomizableObjectInstance;
}


void FCustomizableObjectInstanceEditor::BindCommands()
{
	const FCustomizableObjectInstanceEditorCommands& Commands = FCustomizableObjectInstanceEditorCommands::Get();

	const TSharedRef<FUICommandList>& UICommandList = GetToolkitCommands();
	
	UICommandList->MapAction(
		Commands.ShowParentCO,
		FExecuteAction::CreateSP(this, &FCustomizableObjectInstanceEditor::ShowParentInContentBrowser),
		FCanExecuteAction::CreateSP(this, &FCustomizableObjectInstanceEditor::CanOpenOrShowParent),
		FIsActionChecked()
	);

	UICommandList->MapAction(
		Commands.EditParentCO,
		FExecuteAction::CreateSP(this, &FCustomizableObjectInstanceEditor::OpenParentInEditor),
		FCanExecuteAction::CreateSP(this, &FCustomizableObjectInstanceEditor::CanOpenOrShowParent),
		FIsActionChecked()
	);

	// Texture Analyzer
	UICommandList->MapAction(
		Commands.TextureAnalyzer,
		FExecuteAction::CreateSP(this, &FCustomizableObjectInstanceEditor::OpenTextureAnalyzerTab),
		FCanExecuteAction(),
		FIsActionChecked());
}


void FCustomizableObjectInstanceEditor::ExtendToolbar()
{
	TSharedPtr<FUICommandList> CommandList = GetToolkitCommands();

	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, FCustomizableObjectInstanceEditor* Editor, TSharedPtr<FUICommandList> CommandList)
		{
			ToolbarBuilder.BeginSection("Utilities");
			ToolbarBuilder.AddToolBarButton(FCustomizableObjectInstanceEditorCommands::Get().ShowParentCO);
			ToolbarBuilder.AddToolBarButton(FCustomizableObjectInstanceEditorCommands::Get().EditParentCO);
			ToolbarBuilder.AddToolBarButton(FCustomizableObjectInstanceEditorCommands::Get().TextureAnalyzer);
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, this, CommandList));

	AddToolbarExtender(ToolbarExtender);

	ICustomizableObjectEditorModule* CustomizableObjectEditorModule = &FModuleManager::LoadModuleChecked<ICustomizableObjectEditorModule>("CustomizableObjectEditor");
	AddToolbarExtender(CustomizableObjectEditorModule->GetCustomizableObjectEditorToolBarExtensibilityManager()->GetAllExtenders());
}


FText FCustomizableObjectInstanceEditor::GetToolkitName() const
{
	FFormatNamedArguments Args;
	Args.Add(TEXT("ObjectName"), FText::FromString(GetEditingObject()->GetName()));
	Args.Add(TEXT("ToolkitName"), GetBaseToolkitName());
	return FText::Format(LOCTEXT("AppLabelWithAssetName", "{ObjectName} - {ToolkitName}"), Args);
}


FString FCustomizableObjectInstanceEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT( "WorldCentricTabPrefix", "CustomizableObjectInstance " ).ToString();
}


FLinearColor FCustomizableObjectInstanceEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.3f, 0.2f, 0.5f, 0.5f );
}


void FCustomizableObjectInstanceEditor::RefreshTool()
{
	RefreshViewport();
}


void FCustomizableObjectInstanceEditor::RefreshViewport()
{
	if (Viewport)
	{
		Viewport->GetViewportClient()->Invalidate();
	}
}


void FCustomizableObjectInstanceEditor::SetPoseAsset(class UPoseAsset* PoseAssetParameter)
{
	PoseAsset = PoseAssetParameter;

	if (PoseAsset != nullptr)
	{
		Viewport->SetAnimation(nullptr, EAnimationMode::AnimationBlueprint);

		for (UDebugSkelMeshComponent* PreviewSkeletalMeshComponent : PreviewSkeletalMeshComponents)
		{
			PreviewSkeletalMeshComponent->SetAnimationMode(EAnimationMode::AnimationBlueprint);
			PreviewSkeletalMeshComponent->InitAnim(false);
			PreviewSkeletalMeshComponent->SetAnimation(PoseAsset);

			UAnimSingleNodeInstance* SingleNodeInstance = Cast<UAnimSingleNodeInstance>(PreviewSkeletalMeshComponent->GetAnimInstance());
			if (SingleNodeInstance)
			{
				const TArray<FName>& ArrayPoseSmartNames = PoseAsset->GetPoseFNames();
				for (int32 i = 0; i < ArrayPoseSmartNames.Num(); ++i)
				{
					SingleNodeInstance->SetPreviewCurveOverride(ArrayPoseSmartNames[i], 1.0f, false);
				}
			}
		}
	}
}


void FCustomizableObjectInstanceEditor::OnObjectModified(UObject* Object)
{
	UCustomizableObjectInstance* Instance = Cast<UCustomizableObjectInstance>(Object);

	if ((Instance != nullptr) && (Instance == CustomizableObjectInstance))
	{
		if (Instance->ProjectorLayerChange)
		{
			// Variable used in a non multilayer projector to not lose the focus of the gizmo
			Instance->ProjectorLayerChange = false;
			return;
		}

		if (Viewport->GetGizmoHasAssignedData())
		{
			Instance->LastSelectedProjectorParameter = Viewport->GetGizmoProjectorParameterName();
			Instance->LastSelectedProjectorParameterWithIndex = Viewport->GetGizmoProjectorParameterNameWithIndex();
		}

		// Update projector parameter information (not projector parameter node information,
		// this is the case where a projector parameter in the object details is selected)
		const TArray<FCustomizableObjectProjectorParameterValue>& Parameters = CustomizableObjectInstance->GetProjectorParameters();
		const int32 MaxIndex = Parameters.Num();
		bool AnySelected = false;

		for (int32 i = 0; (!AnySelected && (i < MaxIndex)); ++i)
		{
			int32 RangeIndex = Parameters[i].RangeValues.Num() > 0 ? 0 : -1;

			for (; RangeIndex < Parameters[i].RangeValues.Num(); ++RangeIndex)
			{
				const FCustomizableObjectProjector& Value = (RangeIndex == -1) ? Parameters[i].Value : Parameters[i].RangeValues[RangeIndex];

				FString ParameterNameWithIndex = Parameters[i].ParameterName;

				if (RangeIndex != -1)
				{
					ParameterNameWithIndex += FString::Printf(TEXT("__%d"), RangeIndex);
				}

				if (CustomizableObjectInstance->GetProjectorState(Parameters[i].ParameterName, RangeIndex) == EProjectorState::Selected ||
					(CustomizableObjectInstance->GetProjectorState(Parameters[i].ParameterName, RangeIndex) == EProjectorState::TypeChanged &&
						ParameterNameWithIndex != Instance->LastSelectedProjectorParameterWithIndex))
				{
					Viewport->SetGizmoCallUpdateSkeletalMesh(false);
					Viewport->SetProjectorVisibility(true, Parameters[i].ParameterName, ParameterNameWithIndex, RangeIndex, Value, i);
					Viewport->SetGizmoCallUpdateSkeletalMesh(true);
				AnySelected = true;
			}
				else if (CustomizableObjectInstance->GetProjectorState(Parameters[i].ParameterName, RangeIndex) == EProjectorState::TypeChanged &&
					Parameters[i].ParameterName == Instance->LastSelectedProjectorParameter)
					{
					Viewport->SetProjectorType(true, Parameters[i].ParameterName, ParameterNameWithIndex, RangeIndex, Value, i);
						AnySelected = true;
					}
				}

		}

		if (!AnySelected && !Viewport->GetIsManipulatingGizmo())
		{
			Viewport->SetGizmoCallUpdateSkeletalMesh(false);
			Viewport->ResetProjectorVisibility(false);
			Viewport->SetGizmoCallUpdateSkeletalMesh(true);
			Instance->LastSelectedProjectorParameter = "";
			Instance->LastSelectedProjectorParameterWithIndex = "";
		}
	}
}


bool FCustomizableObjectInstanceEditor::GetAssetRegistryLoaded()
{
	return AssetRegistryLoaded;
}


void FCustomizableObjectInstanceEditor::OnInstancePropertySelectionChanged(FProperty* InProperty)
{
	RefreshViewport();
}


void FCustomizableObjectInstanceEditor::OnUpdatePreviewInstance()
{
	RefreshViewport();

	UpdatePreviewVisibility();

	bool bNeedsToResetPreviewComponents = false;
	for (UDebugSkelMeshComponent* PreviewSkeletalMeshComponent : PreviewSkeletalMeshComponents)
	{
		PreviewSkeletalMeshComponent->bComponentUseFixedSkelBounds = true; // First bounds computed would be using physics asset
		PreviewSkeletalMeshComponent->UpdateBounds();

		if (!PreviewSkeletalMeshComponent->IsRegistered())
		{
			bNeedsToResetPreviewComponents = true;
			PreviewSkeletalMeshComponent->RegisterComponentWithWorld(PreviewSkeletalMeshComponent->GetWorld());
		}
	}

	if(bNeedsToResetPreviewComponents)
	{
		Viewport->SetPreviewComponents(ObjectPtrDecay(PreviewSkeletalMeshComponents));
	}

	if (!CustomizableObjectInstance) return;
	
	// If the instance is updated due to a change in a parameter projector, set again the LastSelectedProjectorParameter variable with the
	// current projector the viewport gizmo has to continue having it as selected when rebuilding the parameter details widget.
	// Otherwise, reset the value.
	bool UpdateDetails = false;
	if (Viewport->GetGizmoHasAssignedData())
	{
		CustomizableObjectInstance->LastSelectedProjectorParameter = Viewport->GetGizmoProjectorParameterName();
		CustomizableObjectInstance->LastSelectedProjectorParameterWithIndex = Viewport->GetGizmoProjectorParameterNameWithIndex();
		UpdateDetails = true;
	}
	else
	{
		CustomizableObjectInstance->LastSelectedProjectorParameter = "";
		CustomizableObjectInstance->LastSelectedProjectorParameterWithIndex = "";
	}

	if (UpdateDetails && !CustomizableObjectInstance->ProjectorUpdatedInViewport)
	{
		CustomizableInstanceDetailsView->ForceRefresh();
	}

	if (TextureAnalyzer.IsValid())
	{
		TextureAnalyzer.Get()->RefreshTextureAnalyzerTable(CustomizableObjectInstance);
	}
}


void FCustomizableObjectInstanceEditor::CompileObject(UCustomizableObject* Object)
{

	if (!Object || !Object->Source)
	{
		return;
	}
	
	Viewport->UpdateGizmoDataToOrigin();

	if (!Compiler)
	{
		Compiler = MakeUnique<FCustomizableObjectCompiler>();
	}
	
	FCompilationOptions Options = Object->CompileOptions;
	Options.bSilentCompilation = false;
	Compiler->Compile(*Object, Options, true);
}


bool FCustomizableObjectInstanceEditor::IsTickable(void) const
{
	return true;
}


void FCustomizableObjectInstanceEditor::Tick(float InDeltaTime)
{
	check(CustomizableObjectInstance);
	if (Compiler && Compiler->Tick())
	{
		if (const UCustomizableObject* CustomizableObject = CustomizableObjectInstance->GetCustomizableObject();
			CustomizableObject && CustomizableObject->Source)
		{
			if (PreviewCustomizableSkeletalComponents.Num() > 0)
			{
				CustomizableObjectInstance->UpdateSkeletalMeshAsync(true, true);
			}
			else
			{
				CreatePreviewInstance();
			}

			CustomizableInstanceDetailsView->ForceRefresh();
		}

		Compiler.Reset();
	}

	// TEMP CODE
	if (CustomizableObjectInstance->TempUpdateGizmoInViewport)
	{
		CustomizableObjectInstance->TempUpdateGizmoInViewport = false;
		CustomizableObjectInstance->AvoidResetProjectorVisibilityForNonNode = true;
		Viewport->CopyTransformFromOriginData();
		CustomizableObjectInstance->UpdateSkeletalMeshAsync(true);

		EProjectorState::Type ProjectorState = CustomizableObjectInstance->GetProjectorState(CustomizableObjectInstance->TempProjectorParameterName, CustomizableObjectInstance->TempProjectorParameterRangeIndex);
		if (ProjectorState != EProjectorState::Selected)
		{
			// ResetProjectorVisibilityForNonNode = false;
			CustomizableObjectInstance->SetProjectorState(CustomizableObjectInstance->TempProjectorParameterName, CustomizableObjectInstance->TempProjectorParameterRangeIndex, EProjectorState::Selected);
			FCoreUObjectDelegates::BroadcastOnObjectModified(CustomizableObjectInstance);
		}
	}

	// If we want to show the Relevant/Runtime parameters, we need to refresh the details view to make sure that the scroll bar appears		
	if (bOnlyRelevantParameters != CustomizableObjectInstance->bShowOnlyRelevantParameters)
	{
		bOnlyRelevantParameters = CustomizableObjectInstance->bShowOnlyRelevantParameters;
		CustomizableInstanceDetailsView->ForceRefresh();
	}

	if (bOnlyRuntimeParameters != CustomizableObjectInstance->bShowOnlyRuntimeParameters)
	{
		bOnlyRuntimeParameters = CustomizableObjectInstance->bShowOnlyRuntimeParameters;
		CustomizableInstanceDetailsView->ForceRefresh();
	}
}


TStatId FCustomizableObjectInstanceEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FCustomizableObjectInstanceEditor, STATGROUP_Tickables);
}

void FCustomizableObjectInstanceEditor::OpenTextureAnalyzerTab()
{
	TabManager->TryInvokeTab(TextureAnalyzerTabId);
}


TSharedRef<SDockTab> FCustomizableObjectInstanceEditor::SpawnTab_TextureAnalyzer(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TextureAnalyzerTabId);
	return SNew(SDockTab)
		.Label(LOCTEXT("Texture Analyzer", "Texture Analyzer"))
		[
			TextureAnalyzer.ToSharedRef()
		];
}


#undef LOCTEXT_NAMESPACE
