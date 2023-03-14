// Copyright Epic Games, Inc. All Rights Reserved.

#include "Toolkits/DMXPixelMappingToolkit.h"

#include "DMXEditorUtils.h"
#include "DMXPixelMappingLayoutSettings.h"
#include "DMXPixelMapping.h"
#include "DMXPixelMappingEditorCommands.h"
#include "DMXPixelMappingEditorModule.h"
#include "DMXPixelMappingEditorStyle.h"
#include "DMXPixelMappingEditorUtils.h"
#include "DMXPixelMappingToolbar.h"
#include "K2Node_PixelMappingBaseComponent.h"
#include "DMXPixelMappingComponentWidget.h"
#include "Components/DMXPixelMappingFixtureGroupComponent.h"
#include "Components/DMXPixelMappingRendererComponent.h"
#include "Components/DMXPixelMappingOutputComponent.h"
#include "Components/DMXPixelMappingRootComponent.h"
#include "Components/DMXPixelMappingMatrixComponent.h"
#include "Components/DMXPixelMappingMatrixCellComponent.h"
#include "Components/DMXPixelMappingScreenComponent.h"
#include "Templates/DMXPixelMappingComponentTemplate.h"
#include "ViewModels/DMXPixelMappingPaletteViewModel.h"
#include "Views/SDMXPixelMappingDesignerView.h"
#include "Views/SDMXPixelMappingDetailsView.h"
#include "Views/SDMXPixelMappingHierarchyView.h"
#include "Views/SDMXPixelMappingLayoutView.h"
#include "Views/SDMXPixelMappingPaletteView.h"
#include "Views/SDMXPixelMappingPreviewView.h"

#include "ScopedTransaction.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Commands/GenericCommands.h"
#include "Misc/ScopedSlowTask.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"


#define LOCTEXT_NAMESPACE "DMXPixelMappingToolkit"

const FName FDMXPixelMappingToolkit::PaletteViewTabID(TEXT("DMXPixelMappingEditor_PaletteViewTabID"));
const FName FDMXPixelMappingToolkit::HierarchyViewTabID(TEXT("DMXPixelMappingEditor_HierarchyViewTabID"));
const FName FDMXPixelMappingToolkit::DesignerViewTabID(TEXT("DMXPixelMappingEditor_DesignerViewTabID"));
const FName FDMXPixelMappingToolkit::PreviewViewTabID(TEXT("DMXPixelMappingEditor_PreviewViewTabID"));
const FName FDMXPixelMappingToolkit::DetailsViewTabID(TEXT("DMXPixelMappingEditor_DetailsViewTabID"));
const FName FDMXPixelMappingToolkit::LayoutViewTabID(TEXT("DMXPixelMappingEditor_LayoutViewTabID"));

const uint8 FDMXPixelMappingToolkit::RequestStopSendingMaxTicks = 5;

FDMXPixelMappingToolkit::FDMXPixelMappingToolkit()
	: DMXPixelMapping(nullptr)
	, bIsPlayingDMX(false)
	, bTogglePlayDMXAll(true)
	, bRequestStopSendingDMX(false)
{}

FDMXPixelMappingToolkit::~FDMXPixelMappingToolkit()
{
	UDMXPixelMappingBaseComponent::GetOnComponentAdded().RemoveAll(this);
	UDMXPixelMappingBaseComponent::GetOnComponentRemoved().RemoveAll(this);

	Toolbar.Reset();
}

void FDMXPixelMappingToolkit::InitPixelMappingEditor(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, UDMXPixelMapping* InDMXPixelMapping)
{
	check(InDMXPixelMapping);
	InDMXPixelMapping->DestroyInvalidComponents();

	DMXPixelMapping = InDMXPixelMapping;

	InitializeInternal(Mode, InitToolkitHost, FGuid::NewGuid());
}

void FDMXPixelMappingToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_TextureEditor", "DMX Pixel Mapping Editor"));
	TSharedRef<FWorkspaceItem> WorkspaceMenuCategoryRef = WorkspaceMenuCategory.ToSharedRef();

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(PaletteViewTabID, FOnSpawnTab::CreateSP(this, &FDMXPixelMappingToolkit::SpawnTab_PaletteView))
		.SetDisplayName(LOCTEXT("Tab_PaletteView", "Palette"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FDMXPixelMappingEditorStyle::GetStyleSetName(), "ClassIcon.DMXPixelMapping"));

	InTabManager->RegisterTabSpawner(HierarchyViewTabID, FOnSpawnTab::CreateSP(this, &FDMXPixelMappingToolkit::SpawnTab_HierarchyView))
		.SetDisplayName(LOCTEXT("Tab_HierarchyView", "Hierarchy"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Kismet.Tabs.Components"));

	InTabManager->RegisterTabSpawner(DesignerViewTabID, FOnSpawnTab::CreateSP(this, &FDMXPixelMappingToolkit::SpawnTab_DesignerView))
		.SetDisplayName(LOCTEXT("Tab_DesignerView", "Designer"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(PreviewViewTabID, FOnSpawnTab::CreateSP(this, &FDMXPixelMappingToolkit::SpawnTab_PreviewView))
		.SetDisplayName(LOCTEXT("Tab_PreviewView", "Preview"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FDMXPixelMappingEditorStyle::GetStyleSetName(), "Icons.Preview"));

	InTabManager->RegisterTabSpawner(DetailsViewTabID, FOnSpawnTab::CreateSP(this, &FDMXPixelMappingToolkit::SpawnTab_DetailsView))
		.SetDisplayName(LOCTEXT("Tab_DetailsView", "Details"))
		.SetGroup(WorkspaceMenuCategoryRef)
		.SetIcon(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "Icons.Details"));

	InTabManager->RegisterTabSpawner(LayoutViewTabID, FOnSpawnTab::CreateSP(this, &FDMXPixelMappingToolkit::SpawnTab_LayoutView))
		.SetDisplayName(LOCTEXT("Tab_LayoutView", "Layout"))
		.SetGroup(WorkspaceMenuCategoryRef);
}

void FDMXPixelMappingToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(PaletteViewTabID);
	InTabManager->UnregisterTabSpawner(HierarchyViewTabID);
	InTabManager->UnregisterTabSpawner(DesignerViewTabID);
	InTabManager->UnregisterTabSpawner(PreviewViewTabID);
	InTabManager->UnregisterTabSpawner(DetailsViewTabID);
	InTabManager->UnregisterTabSpawner(LayoutViewTabID);
}

FText FDMXPixelMappingToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "DMX Pixel Mapping");
}

FName FDMXPixelMappingToolkit::GetToolkitFName() const
{
	return FName("DMX Pixel Mapping");
}

FString FDMXPixelMappingToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "DMX Pixel Mapping ").ToString();
}

void FDMXPixelMappingToolkit::Tick(float DeltaTime)
{
	if (!ensure(DMXPixelMapping))
	{
		return;
	}

	UDMXPixelMappingRootComponent* RootComponent = DMXPixelMapping->RootComponent;
	if (!ensure(RootComponent))
	{
		return;
	}

	// render selected component
	if (!bIsPlayingDMX)
	{
		for (const FDMXPixelMappingComponentReference& SelectedComponentRef : SelectedComponents)
		{
			UDMXPixelMappingBaseComponent* SelectedComponent = SelectedComponentRef.Component.Get();
			if (IsValid(SelectedComponent))
			{
				// User select root component
				if (Cast<UDMXPixelMappingRootComponent>(SelectedComponent))
				{
					break;
				}

				// Try to get renderer component from selected component
				UDMXPixelMappingRendererComponent* RendererComponent = SelectedComponent->GetRendererComponent();
				if (!ensureMsgf(RendererComponent, TEXT("Component %s resides in pixelmapping but has no valid renderer."), *SelectedComponent->GetUserFriendlyName()))
				{
					break;
				}

				// Render
				RendererComponent->Render();

				// Render preview
				RendererComponent->RenderEditorPreviewTexture();

				// Render only once for all selected components
				break;
			}
		}
	}

	if (bIsPlayingDMX)
	{
		if (bTogglePlayDMXAll) // Send to all
		{
			// Render all components
			RootComponent->RenderAndSendDMX();

			for (FDMXPixelMappingComponentReference& SelectedComponentRef : SelectedComponents)
			{
				if (UDMXPixelMappingBaseComponent* SelectedComponent = SelectedComponentRef.Component.Get())
				{
					// Try to get renderer component from selected component
					UDMXPixelMappingRendererComponent* RendererComponent = SelectedComponent->GetRendererComponent();
					if (RendererComponent)
					{
						RendererComponent->RenderEditorPreviewTexture();
					}

					// Render only once for all selected components
					break;
				}
			}
		}
		else // Send to selected component
		{
			bool bRenderedOnce = false;

			for (FDMXPixelMappingComponentReference& SelectedComponentRef : SelectedComponents)
			{
				if (UDMXPixelMappingBaseComponent* SelectedComponent = SelectedComponentRef.Component.Get())
				{
					if (!bRenderedOnce)
					{
						// Try to get renderer component from selected component
						UDMXPixelMappingRendererComponent* RendererComponent = SelectedComponent->GetRendererComponent();
						if (RendererComponent)
						{
							RendererComponent->Render();

							RendererComponent->RenderEditorPreviewTexture();
						}

						bRenderedOnce = true;
					}

					SelectedComponent->SendDMX();
				}
			}
		}
	}
	else
	{
		if (bRequestStopSendingDMX)
		{
			if (RequestStopSendingTicks < RequestStopSendingMaxTicks)
			{
				if (bTogglePlayDMXAll) // Send to all
				{
					RootComponent->ResetDMX();
				}
				else // Send to selected component
				{
					for (FDMXPixelMappingComponentReference& SelectedComponentRef : SelectedComponents)
					{
						if (UDMXPixelMappingBaseComponent* SelectedComponent = SelectedComponentRef.Component.Get())
						{
							SelectedComponent->ResetDMX();
						}
					}
				}

				RequestStopSendingTicks++;
			}
			else
			{
				RequestStopSendingTicks = 0;
				bRequestStopSendingDMX = false;
			}
		}
	}
}

TStatId FDMXPixelMappingToolkit::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FDMXPixelMappingToolkit, STATGROUP_Tickables);
}

FDMXPixelMappingComponentReference FDMXPixelMappingToolkit::GetReferenceFromComponent(UDMXPixelMappingBaseComponent* InComponent)
{
	return FDMXPixelMappingComponentReference(SharedThis(this), InComponent);
}

void FDMXPixelMappingToolkit::SetActiveRenderComponent(UDMXPixelMappingRendererComponent* InComponent)
{
	ActiveRendererComponent = InComponent;
}

template <typename ComponentType>
TArray<ComponentType> FDMXPixelMappingToolkit::MakeComponentArray(const TSet<FDMXPixelMappingComponentReference>& Components) const
{
	TArray<ComponentType> Result;
	for (const FDMXPixelMappingComponentReference& Component : Components)
	{
		if (ComponentType* CastedComponent = Cast<ComponentType>(Component))
		{
			Result.Add(CastedComponent);
		}
	}
}

void FDMXPixelMappingToolkit::SelectComponents(const TSet<FDMXPixelMappingComponentReference>& InSelectedComponents)
{
	// Update selection
	SelectedComponents.Empty();

	SetActiveRenderComponent(nullptr);
	ActiveOutputComponents.Empty();

	SelectedComponents.Append(InSelectedComponents);

	for (const FDMXPixelMappingComponentReference& ComponentReference : SelectedComponents)
	{
		if (UDMXPixelMappingRendererComponent* RendererComponent = Cast<UDMXPixelMappingRendererComponent>(ComponentReference.GetComponent()))
		{
			SetActiveRenderComponent(RendererComponent);
		}
		else
		{
			if (UDMXPixelMappingRendererComponent* RendererComponentParent = ComponentReference.GetComponent()->GetFirstParentByClass<UDMXPixelMappingRendererComponent>(ComponentReference.GetComponent()))
			{
				SetActiveRenderComponent(RendererComponentParent);
			}
		}

		if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(ComponentReference.GetComponent()))
		{
			ActiveOutputComponents.Add(OutputComponent);
		}
	}

	OnSelectedComponentsChangedDelegate.Broadcast();
}

bool FDMXPixelMappingToolkit::IsComponentSelected(UDMXPixelMappingBaseComponent* Component) const
{
	for (const FDMXPixelMappingComponentReference& ComponentReference : SelectedComponents)
	{
		if (Component && Component == ComponentReference.GetComponent())
		{
			return true;
		}
	}
	return false;
}

void FDMXPixelMappingToolkit::AddRenderer()
{
	if (DMXPixelMapping != nullptr)
	{
		{
			// Just use root component for now
			UDMXPixelMappingRendererComponent* RendererComponent = FDMXPixelMappingEditorUtils::AddRenderer(DMXPixelMapping);
			SetActiveRenderComponent(RendererComponent);
		}
	}
}

void FDMXPixelMappingToolkit::PlayDMX()
{
	bIsPlayingDMX = true;
}

void FDMXPixelMappingToolkit::StopPlayingDMX()
{
	bIsPlayingDMX = false;

	RequestStopSendingTicks = 0;
	bRequestStopSendingDMX = true;

	FDMXEditorUtils::ClearFixturePatchCachedData();
	FDMXEditorUtils::ClearAllDMXPortBuffers();
}

void FDMXPixelMappingToolkit::ExecutebTogglePlayDMXAll()
{
	bTogglePlayDMXAll = !bTogglePlayDMXAll;
}

bool FDMXPixelMappingToolkit::CanSizeSelectedComponentToTexture() const
{
	if (SelectedComponents.Num() == 1)
	{
		if (UDMXPixelMappingBaseComponent* Component = SelectedComponents.Array()[0].GetComponent())
		{
			return
				Component->GetClass() == UDMXPixelMappingFixtureGroupComponent::StaticClass() ||
				Component->GetClass() == UDMXPixelMappingMatrixComponent::StaticClass() ||
				Component->GetClass() == UDMXPixelMappingScreenComponent::StaticClass() ||
				(Component->GetParent() && Component->GetParent()->GetClass() == UDMXPixelMappingFixtureGroupComponent::StaticClass()) ||
				(Component->GetParent() && Component->GetParent()->GetClass() == UDMXPixelMappingMatrixComponent::StaticClass()) ||
				(Component->GetParent() && Component->GetParent()->GetClass() == UDMXPixelMappingScreenComponent::StaticClass());
		}
	}
	return false;
}

void FDMXPixelMappingToolkit::SizeSelectedComponentToTexture()
{
	if (SelectedComponents.IsEmpty())
	{
		return;
	}	

	const UDMXPixelMappingRendererComponent* RendererComponent = GetActiveRendererComponent();
	if (!RendererComponent)
	{
		return;
	}

	const FVector2D TextureSize = RendererComponent->GetSize();
	if (TextureSize == FVector2D::ZeroVector)
	{
		return;
	}

	UDMXPixelMappingOutputComponent* Component = Cast<UDMXPixelMappingOutputComponent>(SelectedComponents.Array()[0].GetComponent());
	if (!Component)
	{
		return;
	}

	// Apply to parent where appropriate
	if (Component->GetClass() != UDMXPixelMappingFixtureGroupComponent::StaticClass() &&
		Component->GetClass() != UDMXPixelMappingMatrixComponent::StaticClass() &&
		Component->GetClass() != UDMXPixelMappingScreenComponent::StaticClass())
	{
		if ((Component->GetParent() && Component->GetParent()->GetClass() == UDMXPixelMappingFixtureGroupComponent::StaticClass()) ||
			(Component->GetParent() && Component->GetParent()->GetClass() == UDMXPixelMappingMatrixComponent::StaticClass()) ||
			(Component->GetParent() && Component->GetParent()->GetClass() == UDMXPixelMappingScreenComponent::StaticClass()))
		{
			Component = Cast<UDMXPixelMappingOutputComponent>(Component->GetParent());
		}
	}

	const FScopedTransaction SizeComponentToTextureTransaction(LOCTEXT("SizeComponentToTextureTransaction", "Size Component to Texture"));

	const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>();
	if (LayoutSettings && LayoutSettings->bScaleChildrenWithParent)
	{		
		// Scale children to parent
		const FVector2D RatioVector = TextureSize / Component->GetSize();
		for (UDMXPixelMappingBaseComponent* BaseChild : Component->GetChildren())
		{
			if (UDMXPixelMappingOutputComponent* Child = Cast<UDMXPixelMappingOutputComponent>(BaseChild))
			{
				Child->PreEditChange(UDMXPixelMappingOutputComponent::StaticClass()->FindPropertyByName(UDMXPixelMappingOutputComponent::GetPositionXPropertyName()));
				Child->PreEditChange(UDMXPixelMappingOutputComponent::StaticClass()->FindPropertyByName(UDMXPixelMappingOutputComponent::GetPositionYPropertyName()));
				Child->PreEditChange(UDMXPixelMappingOutputComponent::StaticClass()->FindPropertyByName(UDMXPixelMappingOutputComponent::GetSizeXPropertyName()));
				Child->PreEditChange(UDMXPixelMappingOutputComponent::StaticClass()->FindPropertyByName(UDMXPixelMappingOutputComponent::GetSizeYPropertyName()));

				// Scale size (SetSize already clamps)
				Child->SetSize(Child->GetSize() * RatioVector);

				// Scale position (new position is zero vector)
				const FVector2D ChildPosition = Child->GetPosition();
				const FVector2D NewPositionRelative = (ChildPosition - Component->GetPosition()) * RatioVector;
				Child->SetPosition(Component->GetPosition() + NewPositionRelative);

				Child->PostEditChange();
			}
		}
	}

	Component->PreEditChange(UDMXPixelMappingOutputComponent::StaticClass()->FindPropertyByName(UDMXPixelMappingOutputComponent::GetPositionXPropertyName()));
	Component->PreEditChange(UDMXPixelMappingOutputComponent::StaticClass()->FindPropertyByName(UDMXPixelMappingOutputComponent::GetPositionYPropertyName()));
	Component->PreEditChange(UDMXPixelMappingOutputComponent::StaticClass()->FindPropertyByName(UDMXPixelMappingOutputComponent::GetSizeXPropertyName()));
	Component->PreEditChange(UDMXPixelMappingOutputComponent::StaticClass()->FindPropertyByName(UDMXPixelMappingOutputComponent::GetSizeYPropertyName()));

	Component->SetPosition(FVector2D::ZeroVector);
	Component->SetSize(TextureSize);

	Component->PostEditChange();
}

void FDMXPixelMappingToolkit::UpdateBlueprintNodes(UDMXPixelMapping* InDMXPixelMapping)
{
	if (InDMXPixelMapping != nullptr)
	{
		for (TObjectIterator<UK2Node_PixelMappingBaseComponent> It(RF_Transient | RF_ClassDefaultObject, /** bIncludeDerivedClasses */ true, /** InternalExcludeFlags */ EInternalObjectFlags::Garbage); It; ++It)
		{
			It->OnPixelMappingChanged(InDMXPixelMapping);
		}
	}
}

void FDMXPixelMappingToolkit::OnSaveThumbnailImage()
{
	if (ActiveRendererComponent.IsValid())
	{
		DMXPixelMapping->ThumbnailImage = ActiveRendererComponent->GetPreviewRenderTarget();
	}
}

TArray<UDMXPixelMappingBaseComponent*> FDMXPixelMappingToolkit::CreateComponentsFromTemplates(UDMXPixelMappingRootComponent* RootComponent, UDMXPixelMappingBaseComponent* Target, const TArray<TSharedPtr<FDMXPixelMappingComponentTemplate>>& Templates)
{
	TArray<UDMXPixelMappingBaseComponent*> NewComponents;
	if (Templates.Num() > 0)
	{
		TGuardValue<bool>(bAddingComponents, true);

		if (ensureMsgf(RootComponent && Target, TEXT("Tried to create components from template but RootComponent or Target were invalid.")))
		{
			for (const TSharedPtr<FDMXPixelMappingComponentTemplate>& Template : Templates)
			{
				if (UDMXPixelMappingBaseComponent* NewComponent = Template->CreateComponent<UDMXPixelMappingBaseComponent>(RootComponent))
				{
					NewComponents.Add(NewComponent);

					Target->Modify();
					NewComponent->Modify();

					Target->AddChild(NewComponent);
				}
			}
		}

		UpdateBlueprintNodes(DMXPixelMapping);
	}

	return NewComponents;
}

void FDMXPixelMappingToolkit::DeleteSelectedComponents()
{
	if (SelectedComponents.Num() > 0)
	{
		TGuardValue<bool>(bRemovingComponents, true);

		TSet<FDMXPixelMappingComponentReference> ParentComponentReferences;
		for (const FDMXPixelMappingComponentReference& ComponentReference : SelectedComponents)
		{
			if (UDMXPixelMappingBaseComponent* Component = ComponentReference.GetComponent())
			{
				constexpr bool bModifyChildrenRecursively = true;
				Component->ForEachChild([](UDMXPixelMappingBaseComponent* ChildComponent)
					{
						ChildComponent->Modify();
					}, bModifyChildrenRecursively);

				if (UDMXPixelMappingBaseComponent* ParentComponent = Component->GetParent())
				{
					ParentComponentReferences.Add(GetReferenceFromComponent(ParentComponent));

					Component->SetFlags(RF_Transactional);
					Component->Modify();
					ParentComponent->Modify();

					ParentComponent->RemoveChild(Component);
				}
			
				if (UDMXPixelMappingOutputComponent* OutputComponent = Cast<UDMXPixelMappingOutputComponent>(Component))
				{
					ActiveOutputComponents.Remove(OutputComponent);
				}

				if (UDMXPixelMappingRendererComponent* RendererComponent = Cast<UDMXPixelMappingRendererComponent>(Component))
				{
					SetActiveRenderComponent(nullptr);
				}
			}
		}

		// Select the Parent Component 
		SelectComponents(ParentComponentReferences);

		UpdateBlueprintNodes(DMXPixelMapping);
	}
}

void FDMXPixelMappingToolkit::OnComponentAdded(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component)
{
	// For performance reasons don't update blueprint nodes if the methods in this class add (possibly many) components.
	// Instead call UpdateBlueprintNodes at the end such functions.
	if (!bAddingComponents)
	{
		UpdateBlueprintNodes(DMXPixelMapping);
	}
}

void FDMXPixelMappingToolkit::OnComponentRemoved(UDMXPixelMapping* PixelMapping, UDMXPixelMappingBaseComponent* Component)
{
	// For performance reasons don't update blueprint nodes if the methods in this class remove (possibly many) components .
	// Instead call UpdateBlueprintNodes at the end such functions.
	if (!bRemovingComponents)
	{
		UpdateBlueprintNodes(DMXPixelMapping);
	}
}

void FDMXPixelMappingToolkit::InitializeInternal(const EToolkitMode::Type Mode, const TSharedPtr<class IToolkitHost>& InitToolkitHost, const FGuid& MessageLogGuid)
{
	// Make sure we loaded all UObjects
	DMXPixelMapping->CreateOrLoadObjects();

	// Bind to component changes
	UDMXPixelMappingBaseComponent::GetOnComponentAdded().AddRaw(this, &FDMXPixelMappingToolkit::OnComponentAdded);
	UDMXPixelMappingBaseComponent::GetOnComponentRemoved().AddRaw(this, &FDMXPixelMappingToolkit::OnComponentRemoved);

	// Create commands
	DesignerCommandList = MakeShareable(new FUICommandList);
	DesignerCommandList->MapAction(FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &FDMXPixelMappingToolkit::DeleteSelectedComponents)
	);

	CreateInternalViewModels();
	CreateInternalViews();

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_PixelMapping_Layout_v6")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.25f)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(PaletteViewTabID, ETabState::OpenedTab)
						->SetSizeCoefficient(0.5f)
					)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(HierarchyViewTabID, ETabState::OpenedTab)
						->SetSizeCoefficient(0.5f)
					)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.5f)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(DesignerViewTabID, ETabState::OpenedTab)
						->SetSizeCoefficient(0.6f)
					)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(PreviewViewTabID, ETabState::OpenedTab)
						->SetSizeCoefficient(0.4f)
					)
				)
				->Split
				(
					FTabManager::NewSplitter()
					->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(0.5f)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(DetailsViewTabID, ETabState::OpenedTab)
						->SetSizeCoefficient(0.6f)
					)
					->Split
					(
						FTabManager::NewStack()
						->AddTab(LayoutViewTabID, ETabState::OpenedTab)
						->SetSizeCoefficient(0.4f)
					)
				)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FDMXPixelMappingEditorModule::DMXPixelMappingEditorAppIdentifier,
		StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, DMXPixelMapping);

	SetupCommands();
	ExtendToolbar();
	RegenerateMenusAndToolbars();
}

TSharedRef<SDockTab> FDMXPixelMappingToolkit::SpawnTab_PaletteView(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PaletteViewTabID);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("PaletteViewTabID", "Palette"))
		[
			PaletteView.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FDMXPixelMappingToolkit::SpawnTab_HierarchyView(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == HierarchyViewTabID);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("HierarchyViewTabID", "Hierarchy"))
		[
			HierarchyView.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FDMXPixelMappingToolkit::SpawnTab_DesignerView(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == DesignerViewTabID);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DesignerViewTabID", "Designer"))
		[
			DesignerView.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FDMXPixelMappingToolkit::SpawnTab_PreviewView(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewViewTabID);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("PreviewViewTabID", "Preview"))
		[
			PreviewView.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FDMXPixelMappingToolkit::SpawnTab_DetailsView(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == DetailsViewTabID);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("DetailsViewTabID", "Details"))
		[
			DetailsView.ToSharedRef()
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FDMXPixelMappingToolkit::SpawnTab_LayoutView(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == LayoutViewTabID);

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Label(LOCTEXT("LayoutViewTabID", "Layout"))
		[
			LayoutView.ToSharedRef()
		];

	return SpawnedTab;
}

void FDMXPixelMappingToolkit::CreateInternalViewModels()
{
	TSharedPtr<FDMXPixelMappingToolkit> ThisPtr(SharedThis(this));

	PaletteViewModel = MakeShared<FDMXPixelMappingPaletteViewModel>(ThisPtr);
}

void FDMXPixelMappingToolkit::CreateInternalViews()
{
	CreateOrGetView_PaletteView();
	CreateOrGetView_HierarchyView();
	CreateOrGetView_DesignerView();
	CreateOrGetView_PreviewView();
	CreateOrGetView_DetailsView();
	CreateOrGetView_LayoutView();
}

void FDMXPixelMappingToolkit::OnComponentRenamed(UDMXPixelMappingBaseComponent* InComponent)
{
	UpdateBlueprintNodes(GetDMXPixelMapping());
}

TSharedRef<SWidget> FDMXPixelMappingToolkit::CreateOrGetView_PaletteView()
{
	if (!PaletteView.IsValid())
	{
		PaletteView = SNew(SDMXPixelMappingPaletteView, SharedThis(this));
	}

	return PaletteView.ToSharedRef();
}

TSharedRef<SWidget> FDMXPixelMappingToolkit::CreateOrGetView_HierarchyView()
{
	if (!HierarchyView.IsValid())
	{
		HierarchyView = SNew(SDMXPixelMappingHierarchyView, SharedThis(this));
	}

	return HierarchyView.ToSharedRef();
}

TSharedRef<SWidget> FDMXPixelMappingToolkit::CreateOrGetView_DesignerView()
{
	if (!DesignerView.IsValid())
	{
		DesignerView = SNew(SDMXPixelMappingDesignerView, SharedThis(this));
	}

	return DesignerView.ToSharedRef();
}

TSharedRef<SWidget> FDMXPixelMappingToolkit::CreateOrGetView_PreviewView()
{
	if (!PreviewView.IsValid())
	{
		PreviewView = SNew(SDMXPixelMappingPreviewView, SharedThis(this));
	}

	return PreviewView.ToSharedRef();
}

TSharedRef<SWidget> FDMXPixelMappingToolkit::CreateOrGetView_DetailsView()
{
	if (!DetailsView.IsValid())
	{
		DetailsView = SNew(SDMXPixelMappingDetailsView, SharedThis(this));
	}

	return DetailsView.ToSharedRef();
}

TSharedRef<SWidget> FDMXPixelMappingToolkit::CreateOrGetView_LayoutView()
{
	if (!LayoutView.IsValid())
	{
		LayoutView = SNew(SDMXPixelMappingLayoutView, SharedThis(this));
	}

	return LayoutView.ToSharedRef();
}

void FDMXPixelMappingToolkit::SetupCommands()
{
	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().AddMapping,
		FExecuteAction::CreateSP(this, &FDMXPixelMappingToolkit::AddRenderer)
	);

	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().SaveThumbnailImage,
		FExecuteAction::CreateSP(this, &FDMXPixelMappingToolkit::OnSaveThumbnailImage)
	);

	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().PlayDMX,
		FExecuteAction::CreateSP(this, &FDMXPixelMappingToolkit::PlayDMX),
		FCanExecuteAction::CreateLambda([this] 
			{ 
				return !bIsPlayingDMX;
			}),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateLambda([this] 
			{ 
				return !bIsPlayingDMX; 
			})
	);

	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().StopPlayingDMX,
		FExecuteAction::CreateSP(this, &FDMXPixelMappingToolkit::StopPlayingDMX),
		FCanExecuteAction::CreateLambda([this] 
			{ 
				return bIsPlayingDMX; 
			}),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateLambda([this] 
			{ 
				return bIsPlayingDMX;
			})
	);

	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().TogglePlayDMXAll,
		FExecuteAction::CreateSP(this, &FDMXPixelMappingToolkit::ExecutebTogglePlayDMXAll),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([this]()
			{ 
				return bTogglePlayDMXAll; 
			})
	);

	// Layout related
	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().SizeComponentToTexture,
		FExecuteAction::CreateSP(this, &FDMXPixelMappingToolkit::SizeSelectedComponentToTexture),
		FCanExecuteAction::CreateSP(this, &FDMXPixelMappingToolkit::CanSizeSelectedComponentToTexture)
	);

	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().ToggleScaleChildrenWithParent,
		FExecuteAction::CreateLambda([]
			{
				if (UDMXPixelMappingLayoutSettings* LayoutSettings = GetMutableDefault<UDMXPixelMappingLayoutSettings>())
				{
					LayoutSettings->PreEditChange(nullptr);
					LayoutSettings->bScaleChildrenWithParent = !LayoutSettings->bScaleChildrenWithParent;
					LayoutSettings->PostEditChange();
				}
			}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]()
			{
				if (const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>())
				{
					return LayoutSettings->bScaleChildrenWithParent;
				}
				return false;
			})
	);

	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().ToggleAlwaysSelectGroup,
		FExecuteAction::CreateLambda([]
			{
				if (UDMXPixelMappingLayoutSettings* LayoutSettings = GetMutableDefault<UDMXPixelMappingLayoutSettings>())
				{
					LayoutSettings->PreEditChange(nullptr);
					LayoutSettings->bAlwaysSelectGroup = !LayoutSettings->bAlwaysSelectGroup;
					LayoutSettings->PostEditChange();
				}
			}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]()
			{
				if (const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>())
				{
					return LayoutSettings->bAlwaysSelectGroup;
				}
				return false;
			})
	);

	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().ToggleApplyLayoutScriptWhenLoaded,
		FExecuteAction::CreateLambda([]
			{
				if (UDMXPixelMappingLayoutSettings* LayoutSettings = GetMutableDefault<UDMXPixelMappingLayoutSettings>())
				{
					LayoutSettings->PreEditChange(nullptr);
					LayoutSettings->bApplyLayoutScriptWhenLoaded = !LayoutSettings->bApplyLayoutScriptWhenLoaded;
					LayoutSettings->PostEditChange();
				}
			}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]()
			{
				if (const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>())
				{
					return LayoutSettings->bApplyLayoutScriptWhenLoaded;
				}
				return false;
			})
	);

	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().ToggleShowComponentNames,
		FExecuteAction::CreateLambda([]
			{
				if (UDMXPixelMappingLayoutSettings* LayoutSettings = GetMutableDefault<UDMXPixelMappingLayoutSettings>())
				{
					LayoutSettings->PreEditChange(nullptr);
					LayoutSettings->bShowComponentNames = !LayoutSettings->bShowComponentNames;
					LayoutSettings->PostEditChange();
				}
			}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]()
			{
				if (const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>())
				{
					return LayoutSettings->bShowComponentNames;
				}
				return false;
			})
	);

	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().ToggleShowPatchInfo,
		FExecuteAction::CreateLambda([]
			{
				if (UDMXPixelMappingLayoutSettings* LayoutSettings = GetMutableDefault<UDMXPixelMappingLayoutSettings>())
				{
					LayoutSettings->PreEditChange(nullptr);
					LayoutSettings->bShowPatchInfo = !LayoutSettings->bShowPatchInfo;
					LayoutSettings->PostEditChange();
				}
			}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]()
			{
				if (const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>())
				{
					return LayoutSettings->bShowPatchInfo;
				}
				return false;
			})
	);

	GetToolkitCommands()->MapAction(
		FDMXPixelMappingEditorCommands::Get().ToggleShowCellIDs,
		FExecuteAction::CreateLambda([]
			{
				if (UDMXPixelMappingLayoutSettings* LayoutSettings = GetMutableDefault<UDMXPixelMappingLayoutSettings>())
				{
					LayoutSettings->PreEditChange(nullptr);
					LayoutSettings->bShowCellIDs = !LayoutSettings->bShowCellIDs;
					LayoutSettings->PostEditChange();
				}
			}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]()
			{
				if (const UDMXPixelMappingLayoutSettings* LayoutSettings = GetDefault<UDMXPixelMappingLayoutSettings>())
				{
					return LayoutSettings->bShowCellIDs;
				}
				return false;
			})
	);
}

void FDMXPixelMappingToolkit::ExtendToolbar()
{
	FDMXPixelMappingEditorModule& DMXPixelMappingEditorModule = FModuleManager::LoadModuleChecked<FDMXPixelMappingEditorModule>("DMXPixelMappingEditor");
	Toolbar = MakeShared<FDMXPixelMappingToolbar>(SharedThis(this));

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	Toolbar->BuildToolbar(ToolbarExtender);
	AddToolbarExtender(ToolbarExtender);

	// Let other part of the plugin extend DMX Pixel Maping Editor toolbar
	AddMenuExtender(DMXPixelMappingEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
	AddToolbarExtender(DMXPixelMappingEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

#undef LOCTEXT_NAMESPACE
