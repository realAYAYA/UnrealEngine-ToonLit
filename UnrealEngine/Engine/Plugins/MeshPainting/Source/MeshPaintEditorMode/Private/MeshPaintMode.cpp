// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshPaintMode.h"
#include "MeshPaintModeCommands.h"
#include "MeshPaintModeToolkit.h"
#include "Toolkits/ToolkitManager.h"
#include "Framework/Commands/UICommandList.h"
#include "MeshPaintModeSettings.h"
#include "Framework/Commands/Commands.h"
#include "MultiSelectionTool.h"
#include "InteractiveToolManager.h"
#include "EdModeInteractiveToolsContext.h"
#include "Components/MeshComponent.h"
#include "Dialogs/Dialogs.h"
#include "Components/StaticMeshComponent.h"
#include "MeshPaintHelpers.h"
#include "Components/SkeletalMeshComponent.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "ScopedTransaction.h"
#include "PackageTools.h"
#include "MeshPaintHelpers.h"
#include "Engine/Selection.h"
#include "MeshPaintHelpers.h"
#include "MeshPaintAdapterFactory.h"
#include "MeshPaintModeHelpers.h"
#include "MeshSelect.h"
#include "MeshTexturePaintingTool.h"
#include "Modules/ModuleManager.h"
#include "Settings/LevelEditorMiscSettings.h"
#include "LevelEditor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MeshPaintMode)


#define LOCTEXT_NAMESPACE "MeshPaintMode"

FName UMeshPaintMode::MeshPaintMode_Color = FName(TEXT("Color"));
FName UMeshPaintMode::MeshPaintMode_Weights = FName(TEXT("Weights"));
FName UMeshPaintMode::MeshPaintMode_Texture = FName(TEXT("Texture"));

FString UMeshPaintMode::VertexSelectToolName = TEXT("VertexAdapterClickTool");
FString UMeshPaintMode::TextureSelectToolName = TEXT("TextureAdapterClickTool");
FString UMeshPaintMode::ColorPaintToolName = TEXT("ColorBrushTool");
FString UMeshPaintMode::WeightPaintToolName = TEXT("WeightBrushTool");
FString UMeshPaintMode::TexturePaintToolName = TEXT("TextureBrushTool");


UMeshVertexPaintingToolProperties* UMeshPaintMode::GetVertexToolProperties()
{
	if (GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")
		&& GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")->GetToolManager()->GetActiveTool(EToolSide::Mouse))
	{
		TArray<UObject*> PropertyArray = GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")->GetToolManager()->GetActiveTool(EToolSide::Mouse)->GetToolProperties();
		for(UObject* Property : PropertyArray)
		{
			if (UMeshVertexPaintingToolProperties* VertexProperties = Cast<UMeshVertexPaintingToolProperties>(Property))
			{
				return VertexProperties;
			}
		}
	}
	return nullptr;
}

UMeshColorPaintingToolProperties* UMeshPaintMode::GetColorToolProperties()
{
	if (GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")
		&& GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")->GetToolManager()->GetActiveTool(EToolSide::Mouse))
	{
		TArray<UObject*> PropertyArray = GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")->GetToolManager()->GetActiveTool(EToolSide::Mouse)->GetToolProperties();
		for (UObject* Property : PropertyArray)
		{
			if (UMeshColorPaintingToolProperties* ColorProperties = Cast<UMeshColorPaintingToolProperties>(Property))
			{
				return ColorProperties;
			}
		}
	}
	return nullptr;
}

UMeshWeightPaintingToolProperties* UMeshPaintMode::GetWeightToolProperties()
{
	if (GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")
		&& GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")->GetToolManager()->GetActiveTool(EToolSide::Mouse))
	{
		TArray<UObject*> PropertyArray = GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")->GetToolManager()->GetActiveTool(EToolSide::Mouse)->GetToolProperties();
		for (UObject* Property : PropertyArray)
		{
			if (UMeshWeightPaintingToolProperties* WeightProperties = Cast<UMeshWeightPaintingToolProperties>(Property))
			{
				return WeightProperties;
			}
		}
	}
	return nullptr;
}

UMeshTexturePaintingToolProperties* UMeshPaintMode::GetTextureToolProperties()
{
	if (GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")
		&& GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")->GetToolManager()->GetActiveTool(EToolSide::Mouse))
	{
		TArray<UObject*> PropertyArray = GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode")->GetToolManager()->GetActiveTool(EToolSide::Mouse)->GetToolProperties();
		for (UObject* Property : PropertyArray)
		{
			if (UMeshTexturePaintingToolProperties* TextureProperties = Cast<UMeshTexturePaintingToolProperties>(Property))
			{
				return TextureProperties;
			}
		}
	}
	return nullptr;
}

UMeshPaintMode* UMeshPaintMode::GetMeshPaintMode()
{
	return Cast<UMeshPaintMode>(GLevelEditorModeTools().GetActiveScriptableMode("MeshPaintMode"));
}

UMeshPaintMode::UMeshPaintMode()
	: Super()
{
	SettingsClass = UMeshPaintModeSettings::StaticClass();

	Info = FEditorModeInfo(
		FName(TEXT("MeshPaintMode")),
		LOCTEXT("ModeName", "Mesh Paint"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.MeshPaintMode", "LevelEditor.MeshPaintMode.Small"),
		true,
		600
	);
}

void UMeshPaintMode::Enter()
{
	Super::Enter();
	GEditor->OnEditorClose().AddUObject(this, &UMeshPaintMode::OnResetViewMode);
	FCoreUObjectDelegates::OnObjectsReplaced.AddUObject(this, &UMeshPaintMode::OnObjectsReplaced);
	ModeSettings = Cast<UMeshPaintModeSettings>(SettingsObject);
	
	FMeshPaintEditorModeCommands ToolManagerCommands = FMeshPaintEditorModeCommands::Get();

	UVertexAdapterClickToolBuilder* VertexClickToolBuilder = NewObject<UVertexAdapterClickToolBuilder>(this);
	RegisterTool(ToolManagerCommands.VertexSelect, VertexSelectToolName, VertexClickToolBuilder);

	UTextureAdapterClickToolBuilder* TextureClickToolBuilder = NewObject<UTextureAdapterClickToolBuilder>(this);
	RegisterTool(ToolManagerCommands.TextureSelect, TextureSelectToolName, TextureClickToolBuilder);


	UMeshColorPaintingToolBuilder* MeshColorPaintingToolBuilder = NewObject<UMeshColorPaintingToolBuilder>(this);
	RegisterTool(ToolManagerCommands.ColorPaint, ColorPaintToolName, MeshColorPaintingToolBuilder);

	UMeshWeightPaintingToolBuilder* WeightPaintingToolBuilder = NewObject<UMeshWeightPaintingToolBuilder>(this);
	RegisterTool(ToolManagerCommands.WeightPaint, WeightPaintToolName, WeightPaintingToolBuilder);

	UMeshTexturePaintingToolBuilder* TexturePaintingToolBuilder = NewObject<UMeshTexturePaintingToolBuilder>(this);
	RegisterTool(ToolManagerCommands.TexturePaint, TexturePaintToolName, TexturePaintingToolBuilder);
	UpdateSelectedMeshes();


	// Toolkit
	PaletteChangedHandle = Toolkit->OnPaletteChanged().AddUObject(this, &UMeshPaintMode::UpdateOnPaletteChange);

	// disable tool change tracking to activate default tool, and then switch to full undo/redo tracking mode
	GetToolManager()->ConfigureChangeTrackingMode(EToolChangeTrackingMode::NoChangeTracking);
	Toolkit->SetCurrentPalette(MeshPaintMode_Color);
	GetToolManager()->ConfigureChangeTrackingMode(EToolChangeTrackingMode::UndoToExit);

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(FName(TEXT("LevelEditor")));
	LevelEditor.OnRedrawLevelEditingViewports().AddUObject(this, &UMeshPaintMode::UpdateOnMaterialChange);
}

void UMeshPaintMode::Exit()
{
	Toolkit->OnPaletteChanged().Remove(PaletteChangedHandle);
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
	GEditor->OnEditorClose().RemoveAll(this);
	OnResetViewMode();
	const FMeshPaintEditorModeCommands& Commands = FMeshPaintEditorModeCommands::Get();
	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();
	for (const TSharedPtr<const FUICommandInfo> Action : Commands.Commands[UMeshPaintMode::MeshPaintMode_Color])
	{
		CommandList->UnmapAction(Action);
	}
	for (const TSharedPtr<const FUICommandInfo> Action : Commands.Commands[UMeshPaintMode::MeshPaintMode_Weights])
	{
		CommandList->UnmapAction(Action);
	}
	for (const TSharedPtr<const FUICommandInfo> Action : Commands.Commands[UMeshPaintMode::MeshPaintMode_Weights])
	{
		CommandList->UnmapAction(Action);
	}

	Super::Exit();

	GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->ResetState();

	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>(FName(TEXT("LevelEditor")));
	LevelEditor.OnRedrawLevelEditingViewports().RemoveAll(this);
}

void UMeshPaintMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FMeshPaintModeToolkit);
}

void UMeshPaintMode::Tick(FEditorViewportClient* ViewportClient, float DeltaTime)
{
	if (ViewportClient->IsPerspective())
	{
		// Make sure perspective viewports are still set to real-time
		GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->SetRealtimeViewport(true);

		// Set viewport show flags		
		GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->SetViewportColorMode(ModeSettings->ColorViewMode, ViewportClient);
	}


	if (bRecacheVertexDataSize)
	{
		UpdateCachedVertexDataSize();
	}

	// Make sure that correct tab is visible for the current tool
	// Note that currently Color and Weight mode share the same Select tool
	FString ActiveTool = GetToolManager()->GetActiveToolName(EToolSide::Mouse);
	FName ActiveTab = Toolkit->GetCurrentPalette();
	FName TargetTab = ActiveTab;
	if (ActiveTool == TexturePaintToolName || ActiveTool == TextureSelectToolName)
	{
		TargetTab = MeshPaintMode_Texture;
	}
	else if (ActiveTool == ColorPaintToolName)
	{
		TargetTab = MeshPaintMode_Color;
	}
	else if (ActiveTool == WeightPaintToolName)
	{
		TargetTab = MeshPaintMode_Weights;
	}
	if ( TargetTab != ActiveTab)
	{
		Toolkit->SetCurrentPalette(TargetTab);
	}
}

bool UMeshPaintMode::HandleClick(FEditorViewportClient* InViewportClient, HHitProxy* HitProxy, const FViewportClick& Click)
{
	return true;
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UMeshPaintMode::GetModeCommands() const
{
	return FMeshPaintEditorModeCommands::GetCommands();
}

void UMeshPaintMode::BindCommands()
{
	const FMeshPaintEditorModeCommands& Commands = FMeshPaintEditorModeCommands::Get();
	const TSharedRef<FUICommandList>& CommandList = Toolkit->GetToolkitCommands();

	CommandList->MapAction(Commands.SwitchForeAndBackgroundColor, FExecuteAction::CreateLambda([this]()
	{
		GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->SwapVertexColors();
	}));


	CommandList->MapAction(Commands.CycleToNextLOD, FExecuteAction::CreateUObject(this, &UMeshPaintMode::CycleMeshLODs, 1));
	CommandList->MapAction(Commands.CycleToPreviousLOD, FExecuteAction::CreateUObject(this, &UMeshPaintMode::CycleMeshLODs, -1));
	UMeshPaintingSubsystem* MeshPaintingSubsystem = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>();

	auto IsInValidPaintMode = [MeshPaintingSubsystem]() -> bool {
		return (GetVertexToolProperties() != nullptr) && MeshPaintingSubsystem && MeshPaintingSubsystem->SelectionContainsValidAdapters();
	};
	CommandList->MapAction(Commands.Fill, 
		FUIAction(FExecuteAction::CreateUObject(this, &UMeshPaintMode::FillWithVertexColor),
		FCanExecuteAction::CreateLambda(IsInValidPaintMode)));

	CommandList->MapAction(Commands.Propagate,
		FUIAction(
			FExecuteAction::CreateUObject(this, &UMeshPaintMode::PropagateVertexColorsToAsset),
			FCanExecuteAction::CreateUObject(this, &UMeshPaintMode::CanPropagateVertexColors)
		));

	auto IsAValidMeshComponentSelected = [this, MeshPaintingSubsystem]() -> bool {
		return (GetSelectedComponents<UMeshComponent>().Num() == 1) && MeshPaintingSubsystem && MeshPaintingSubsystem->SelectionContainsValidAdapters();
	};
	CommandList->MapAction(Commands.Import,
		FUIAction(
			FExecuteAction::CreateUObject(this, &UMeshPaintMode::ImportVertexColors),
			FCanExecuteAction::CreateLambda(IsAValidMeshComponentSelected)
		));

	CommandList->MapAction(Commands.Save,
		FUIAction(
			FExecuteAction::CreateUObject(this, &UMeshPaintMode::SavePaintedAssets),
			FCanExecuteAction::CreateUObject(this, &UMeshPaintMode::CanSaveMeshPackages)
		));

	CommandList->MapAction(Commands.Copy,
		FUIAction(
			FExecuteAction::CreateUObject(this, &UMeshPaintMode::CopyVertexColors),
			FCanExecuteAction::CreateUObject(this, &UMeshPaintMode::CanCopyInstanceVertexColors)
		));

	CommandList->MapAction(Commands.Paste,
		FUIAction(
			FExecuteAction::CreateUObject(this, &UMeshPaintMode::PasteVertexColors),
			FCanExecuteAction::CreateUObject(this, &UMeshPaintMode::CanPasteInstanceVertexColors)
		));

	CommandList->MapAction(Commands.Remove,
		FUIAction(
			FExecuteAction::CreateUObject(this, &UMeshPaintMode::RemoveVertexColors),
			FCanExecuteAction::CreateUObject(this, &UMeshPaintMode::CanRemoveInstanceColors)
		));

	CommandList->MapAction(Commands.Fix,
		FUIAction(
			FExecuteAction::CreateUObject(this, &UMeshPaintMode::FixVertexColors),
			FCanExecuteAction::CreateUObject(this, &UMeshPaintMode::DoesRequireVertexColorsFixup)
		));

	CommandList->MapAction(Commands.PropagateVertexColorsToLODs,
		FUIAction(
			FExecuteAction::CreateUObject(this, &UMeshPaintMode::PropagateVertexColorsToLODs),
			FCanExecuteAction::CreateUObject(this, &UMeshPaintMode::CanPropagateVertexColorsToLODs)
		));

	CommandList->MapAction(Commands.NextTexture, 
		FUIAction(
			FExecuteAction::CreateUObject(this, &UMeshPaintMode::CycleTextures, 1),
			FCanExecuteAction::CreateUObject(this, &UMeshPaintMode::CanCycleTextures)
		));
	CommandList->MapAction(Commands.PreviousTexture,
		FUIAction(
			FExecuteAction::CreateUObject(this, &UMeshPaintMode::CycleTextures, -1),
			FCanExecuteAction::CreateUObject(this, &UMeshPaintMode::CanCycleTextures)
		));
	CommandList->MapAction(Commands.SaveTexturePaint, FUIAction(FExecuteAction::CreateUObject(GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>(), &UMeshPaintModeSubsystem::SaveModifiedTextures), FCanExecuteAction::CreateUObject(GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>(), &UMeshPaintModeSubsystem::CanSaveModifiedTextures)));

	auto HasPaintChanges = [this]() -> bool { return GetNumberOfPendingPaintChanges() > 0; };
	CommandList->MapAction(Commands.PropagateTexturePaint, FExecuteAction::CreateUObject(this, &UMeshPaintMode::CommitAllPaintedTextures), FCanExecuteAction::CreateLambda(HasPaintChanges));

}

void UMeshPaintMode::OnToolStarted(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	FMeshPaintingToolActionCommands::UpdateToolCommandBinding(Tool, ToolCommandList, false);

	if (UMeshVertexPaintingTool* VertexPaintingTool = Cast<UMeshVertexPaintingTool>(GetToolManager()->GetActiveTool(EToolSide::Left)))
	{
		VertexPaintingTool->OnPaintingFinished().BindUObject(this, &UMeshPaintMode::OnVertexPaintFinished);
	}
}

void UMeshPaintMode::OnVertexPaintFinished()
{
	if (UMeshColorPaintingToolProperties* ColorPaintingToolProperties = UMeshPaintMode::GetColorToolProperties())
	{
		if (!ColorPaintingToolProperties->bPaintOnSpecificLOD)
		{
			PropagateVertexColorsToLODs();
		}
		else
		{
			if (UMeshPaintingSubsystem* MeshPaintingSubsystem = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>())
			{
				UpdateCachedVertexDataSize();
				MeshPaintingSubsystem->Refresh();
			}
		}
	}
}

void UMeshPaintMode::OnToolEnded(UInteractiveToolManager* Manager, UInteractiveTool* Tool)
{
	FMeshPaintingToolActionCommands::UpdateToolCommandBinding(Tool, ToolCommandList, true);
	// First update your bindings, then call the base behavior
	Super::OnToolEnded(Manager, Tool);
}

void UMeshPaintMode::ActorSelectionChangeNotify()
{
	UpdateSelectedMeshes();
}

void UMeshPaintMode::UpdateSelectedMeshes()
{
	if (UMeshPaintingSubsystem* MeshPaintingSubsystem = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>())
	{
		FName PaletteName = Toolkit->GetCurrentPalette();

		MeshPaintingSubsystem->ResetState();
		TArray<UMeshComponent*> CurrentMeshComponents = GetSelectedComponents<UMeshComponent>();
		MeshPaintingSubsystem->AddSelectedMeshComponents(CurrentMeshComponents);
		MeshPaintingSubsystem->bNeedsRecache = true;

		// Check the current selection for a material that supports texture paint
		if (PaletteName == UMeshPaintMode::MeshPaintMode_Texture)
		{
			CheckSelectionForTexturePaintCompat(CurrentMeshComponents);
		}
	}
	
	bRecacheVertexDataSize = true;
}

void UMeshPaintMode::CheckSelectionForTexturePaintCompat(const TArray<UMeshComponent*>& CurrentMeshComponents)
{
	bool bCurrentSelectionSupportsTexturePaint = false;
	if (CurrentMeshComponents.Num())
	{
		UActorComponent* ActorComponent = CurrentMeshComponents[0];
		TArray<FPaintableTexture> PaintableTextures;
		if (ActorComponent)
		{
			UMeshComponent* MeshComponent = Cast<UMeshComponent>(ActorComponent);
			if (MeshComponent)
			{
				TSharedPtr<IMeshPaintComponentAdapter> MeshAdapter = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->GetAdapterForComponent(MeshComponent);
				if (!MeshAdapter.IsValid())
				{
					MeshAdapter = FMeshPaintComponentAdapterFactory::CreateAdapterForMesh(MeshComponent, 0);
					if (MeshAdapter)
					{
						GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->AddToComponentToAdapterMap(MeshComponent, MeshAdapter);
					}
				}
				UTexturePaintToolset::RetrieveTexturesForComponent(MeshComponent, MeshAdapter.Get(), PaintableTextures);
			}
		}
		bCurrentSelectionSupportsTexturePaint = PaintableTextures.Num() > 0;
	}

	GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->SetSelectionHasMaterialValidForTexturePaint(bCurrentSelectionSupportsTexturePaint);
	if (!bCurrentSelectionSupportsTexturePaint 
		&& GetToolManager() 
		&& GetToolManager()->GetActiveTool(EToolSide::Mouse)
		&& GetToolManager()->GetActiveTool(EToolSide::Mouse)->IsA<UMeshTexturePaintingTool>())
	{
		GetInteractiveToolsContext()->EndTool(EToolShutdownType::Accept);
		GetInteractiveToolsContext()->StartTool(TextureSelectToolName);
	}
}

void UMeshPaintMode::UpdateOnMaterialChange(bool bInvalidateHitProxies)
{
	FName PaletteName = Toolkit->GetCurrentPalette();
	// Check the current selection for a material that supports texture paint
	if (PaletteName == UMeshPaintMode::MeshPaintMode_Texture)
	{
		const TArray<UMeshComponent*> CurrentMeshComponents = GetSelectedComponents<UMeshComponent>();
		CheckSelectionForTexturePaintCompat(CurrentMeshComponents);
	}
}

void UMeshPaintMode::OnObjectsReplaced(const TMap<UObject*, UObject*>& InOldToNewInstanceMap)
{
	if (UMeshPaintingSubsystem* MeshPaintingSubsystem = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>())
	{
		MeshPaintingSubsystem->ClearSelectedMeshComponents();
		MeshPaintingSubsystem->Refresh();
		UpdateSelectedMeshes();
	}
}

void UMeshPaintMode::FillWithVertexColor()
{
	FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionFillInstColors", "Filling Per-Instance Vertex Colors"));
	const TArray<UMeshComponent*> MeshComponents = GetSelectedComponents<UMeshComponent>();
	UMeshPaintingSubsystem* MeshPaintingSubsystem = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>();
	static const bool bConvertSRGB = false;
	FColor FillColor = FColor::White;
	FColor MaskColor = FColor::White;

	if (GetToolManager()->GetActiveTool(EToolSide::Mouse)->IsA<UMeshWeightPaintingTool>())
	{
		FillColor = MeshPaintingSubsystem->GenerateColorForTextureWeight((int32)GetWeightToolProperties()->TextureWeightType, (int32)GetWeightToolProperties()->PaintTextureWeightIndex).ToFColor(bConvertSRGB);
	}
	else if (UMeshColorPaintingToolProperties* ColorProperties = GetColorToolProperties())
	{
		FillColor = ColorProperties->PaintColor.ToFColor(bConvertSRGB);
		MaskColor.R = ColorProperties->bWriteRed ? 255 : 0;
		MaskColor.G = ColorProperties->bWriteGreen ? 255 : 0;
		MaskColor.B = ColorProperties->bWriteBlue ? 255 : 0;
		MaskColor.A = ColorProperties->bWriteAlpha ? 255 : 0;
	}

	TUniquePtr< FComponentReregisterContext > ComponentReregisterContext;
	/** Fill each mesh component with the given vertex color */
	for (UMeshComponent* Component : MeshComponents)
	{
		checkf(Component != nullptr, TEXT("Invalid Mesh Component"));
		Component->Modify();
		ComponentReregisterContext = MakeUnique<FComponentReregisterContext>(Component);

		TSharedPtr<IMeshPaintComponentAdapter> MeshAdapter = MeshPaintingSubsystem->GetAdapterForComponent(Component);
		if (MeshAdapter)
		{
			MeshAdapter->PreEdit();
		}

		UMeshColorPaintingToolProperties* ColorProperties = UMeshPaintMode::GetColorToolProperties();
		
		const bool bPaintOnSpecificLOD = ColorProperties ? ColorProperties->bPaintOnSpecificLOD : false;

		if (Component->IsA<UStaticMeshComponent>())
		{
			MeshPaintingSubsystem->FillStaticMeshVertexColors(Cast<UStaticMeshComponent>(Component), bPaintOnSpecificLOD ? ColorProperties->LODIndex : -1, FillColor, MaskColor);
		}
		else if (Component->IsA<USkeletalMeshComponent>())
		{
			GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->FillSkeletalMeshVertexColors(Cast<USkeletalMeshComponent>(Component), bPaintOnSpecificLOD ? ColorProperties->LODIndex : -1, FillColor, MaskColor);
		}
		else if (MeshAdapter) 			// We don't have a custom fill function for this type of component; try to go through the adapter.
		{
			TArray<uint32> MeshIndices = MeshAdapter->GetMeshIndices();
			UMeshPaintingSubsystem* PaintingSubsystem = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>();
			for (uint32 VID : MeshIndices)
			{
				FColor Color;
				MeshAdapter->GetVertexColor((int32)VID, Color);
				PaintingSubsystem->ApplyFillWithMask(Color, MaskColor, FillColor);
				MeshAdapter->SetVertexColor((int32)VID, Color);
			}
		}

		if (MeshAdapter)
		{
			MeshAdapter->PostEdit();
		}
	}
}


void UMeshPaintMode::PropagateVertexColorsToAsset()
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	FSuppressableWarningDialog::FSetupInfo SetupInfo(LOCTEXT("PushInstanceVertexColorsPrompt_Message", "This operation copies vertex colors from LOD 0 of the selected instance to all LODs of the source asset, overwriting any existing vertex colors.\n\nThis change will also propagate to all other instances of the same asset that do not have custom vertex colors."),
		LOCTEXT("PushInstanceVertexColorsPrompt_Title", "Warning: Overwriting Vertex Colors on Source Asset"), "Warning_PushInstanceVertexColorsPrompt");

	SetupInfo.ConfirmText = LOCTEXT("PushInstanceVertexColorsPrompt_ConfirmText", "Overwrite");
	SetupInfo.CancelText = LOCTEXT("PushInstanceVertexColorsPrompt_CancelText", "Cancel");
	SetupInfo.CheckBoxText = LOCTEXT("PushInstanceVertexColorsPrompt_CheckBoxText", "Always overwrite source asset without prompting");

	FSuppressableWarningDialog VertexColorCopyWarning(SetupInfo);

	// Prompt the user to see if they really want to push the vert colors to the source mesh and to explain
	// the ramifications of doing so. This uses a suppressible dialog so that the user has the choice to always ignore the warning.
	if (VertexColorCopyWarning.ShowModal() != FSuppressableWarningDialog::Cancel)
	{
		FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionPropogateColors", "Propagating Vertex Colors To Source Meshes"));
		GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->PropagateVertexColors(StaticMeshComponents);
	}
}

bool UMeshPaintMode::CanPropagateVertexColors() const
{
	// Check whether or not our selected Static Mesh Components contain instance based vertex colors (only these can be propagated to the base mesh)
	int32 NumInstanceVertexColorBytes = 0;

	TArray<UStaticMesh*> StaticMeshes;
	TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	return GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->CanPropagateVertexColors(StaticMeshComponents, StaticMeshes, NumInstanceVertexColorBytes);

}

void UMeshPaintMode::ImportVertexColors()
{
	const TArray<UMeshComponent*> MeshComponents = GetSelectedComponents<UMeshComponent>();
	if (MeshComponents.Num() == 1)
	{
		/** Import vertex color to single selected mesh component */
		FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionImportColors", "Importing Vertex Colors From Texture"));
		GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->ImportVertexColorsFromTexture(MeshComponents[0]);
	}
}

void UMeshPaintMode::SavePaintedAssets()
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	const TArray<USkeletalMeshComponent*> SkeletalMeshComponents = GetSelectedComponents<USkeletalMeshComponent>();

	/** Try and save outstanding dirty packages for currently selected mesh components */
	TArray<UObject*> ObjectsToSave;
	for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
		{
			ObjectsToSave.Add(StaticMeshComponent->GetStaticMesh());
		}
	}

	for (USkeletalMeshComponent* SkeletalMeshComponent : SkeletalMeshComponents)
	{
		if (SkeletalMeshComponent && SkeletalMeshComponent->GetSkeletalMeshAsset())
		{
			ObjectsToSave.Add(SkeletalMeshComponent->GetSkeletalMeshAsset());
		}
	}

	if (ObjectsToSave.Num() > 0)
	{
		UPackageTools::SavePackagesForObjects(ObjectsToSave);
	}
}

bool UMeshPaintMode::CanSaveMeshPackages() const
{
	// Check whether or not any of our selected mesh components contain mesh objects which require saving
	TArray<UMeshComponent*> Components = GetSelectedComponents<UMeshComponent>();

	bool bValid = false;

	for (UMeshComponent* Component : Components)
	{
		UObject* Object = nullptr;
		if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(Component))
		{
			Object = StaticMeshComponent->GetStaticMesh();
		}
		else if (USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(Component))
		{
			Object = SkeletalMeshComponent->GetSkeletalMeshAsset();
		}

		if (Object != nullptr && Object->GetOutermost()->IsDirty())
		{
			bValid = true;
			break;
		}
	}

	return bValid;
}

bool UMeshPaintMode::CanRemoveInstanceColors() const
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	int32 PaintingMeshLODIndex = 0;
	if (UMeshColorPaintingToolProperties* ColorProperties = UMeshPaintMode::GetColorToolProperties())
	{
		PaintingMeshLODIndex = ColorProperties->bPaintOnSpecificLOD ? ColorProperties->LODIndex : 0;
	}
	int32 NumValidMeshes = 0;
	// Retrieve per instance vertex color information (only valid if the component contains actual instance vertex colors)
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		if (Component != nullptr && Component->GetStaticMesh() != nullptr && Component->GetStaticMesh()->GetNumLODs() > (int32)PaintingMeshLODIndex)
		{
			uint32 BufferSize = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->GetVertexColorBufferSize(Component, PaintingMeshLODIndex, true);

			if (BufferSize > 0)
			{
				++NumValidMeshes;
			}
		}
	}

	return (NumValidMeshes != 0);
}

bool UMeshPaintMode::CanPasteInstanceVertexColors() const
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	const TArray<FPerComponentVertexColorData> CopiedColorsByComponent = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->GetCopiedColorsByComponent();
	return GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->CanPasteInstanceVertexColors(StaticMeshComponents, CopiedColorsByComponent);
}

bool UMeshPaintMode::CanCopyInstanceVertexColors() const
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	int32 PaintingMeshLODIndex = 0;
	if (UMeshColorPaintingToolProperties* ColorProperties = UMeshPaintMode::GetColorToolProperties())
	{
		PaintingMeshLODIndex = ColorProperties->bPaintOnSpecificLOD ? ColorProperties->LODIndex : 0;
	}

	return GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->CanCopyInstanceVertexColors(StaticMeshComponents, PaintingMeshLODIndex);

}

bool UMeshPaintMode::CanPropagateVertexColorsToLODs() const
{
	bool bPaintOnSpecificLOD = false;
	if (UMeshColorPaintingToolProperties* ColorProperties = UMeshPaintMode::GetColorToolProperties())
	{
		bPaintOnSpecificLOD = ColorProperties ? ColorProperties->bPaintOnSpecificLOD : false;
	}
	// Can propagate when the mesh contains per-lod vertex colors or when we are not painting to a specific lod
	const bool bSelectionContainsPerLODColors = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->SelectionContainsPerLODColors();
	return bSelectionContainsPerLODColors || !bPaintOnSpecificLOD;
}

void UMeshPaintMode::CopyVertexColors()
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	TArray<FPerComponentVertexColorData> CopiedColorsByComponent;
	GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->CopyVertexColors(StaticMeshComponents, CopiedColorsByComponent);
	GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->SetCopiedColorsByComponent(CopiedColorsByComponent);

}

void UMeshPaintMode::PasteVertexColors()
{
	FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionPasteInstColors", "Pasting Per-Instance Vertex Colors"));
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	TArray<FPerComponentVertexColorData> CopiedColorsByComponent = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->GetCopiedColorsByComponent();
	GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->PasteVertexColors(StaticMeshComponents, CopiedColorsByComponent);
	UpdateCachedVertexDataSize();
}

void UMeshPaintMode::FixVertexColors()
{
	FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionFixInstColors", "Fixing Per-Instance Vertex Colors"));
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		Component->FixupOverrideColorsIfNecessary();
	}
}

bool UMeshPaintMode::DoesRequireVertexColorsFixup() const
{
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	bool bAnyMeshNeedsFixing = false;
	/** Check if there are any static mesh components which require fixing */
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		bAnyMeshNeedsFixing |= Component->RequiresOverrideVertexColorsFixup();
	}

	return bAnyMeshNeedsFixing;
}

void UMeshPaintMode::RemoveVertexColors()
{
	FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_TransactionRemoveInstColors", "Removing Per-Instance Vertex Colors"));
	const TArray<UStaticMeshComponent*> StaticMeshComponents = GetSelectedComponents<UStaticMeshComponent>();
	for (UStaticMeshComponent* Component : StaticMeshComponents)
	{
		GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->RemoveComponentInstanceVertexColors(Component);
	}

	UpdateCachedVertexDataSize();
}


void UMeshPaintMode::PropagateVertexColorsToLODs()
{
	//Only show the lost data warning if there is actually some data to lose
	bool bAbortChange = false;
	UMeshPaintingSubsystem* MeshPaintingSubsystem = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>();
	TArray<UMeshComponent*> PaintableComponents = MeshPaintingSubsystem->GetPaintableMeshComponents();
	const bool bSelectionContainsPerLODColors = MeshPaintingSubsystem->SelectionContainsPerLODColors();
	if (bSelectionContainsPerLODColors)
	{
		//Warn the user they will lose custom painting data
		FSuppressableWarningDialog::FSetupInfo SetupInfo(LOCTEXT("LooseLowersLODsVertexColorsPrompt_Message", "This operation copies vertex colors from LOD 0 to all other LODs in this instance, overwriting any existing vertex colors.\n\nAt least one LOD has custom vertex colors that will be lost."),
			LOCTEXT("LooseLowersLODsVertexColorsPrompt_Title", "Warning: Overwriting Vertex Colors on LODs"), "Warning_LooseLowersLODsVertexColorsPrompt");

		SetupInfo.ConfirmText = LOCTEXT("LooseLowersLODsVertexColorsPrompt_ConfirmText", "Overwrite");
		SetupInfo.CancelText = LOCTEXT("LooseLowersLODsVertexColorsPrompt_CancelText", "Cancel");
		SetupInfo.CheckBoxText = LOCTEXT("LooseLowersLODsVertexColorsPrompt_CheckBoxText", "Always overwrite LODs without prompting");

		FSuppressableWarningDialog LooseLowersLODsVertexColorsWarning(SetupInfo);
	
		// Prompt the user to see if they really want to propagate the base lod vert colors to the lowers LODs.
		if (LooseLowersLODsVertexColorsWarning.ShowModal() == FSuppressableWarningDialog::Cancel)
		{
			bAbortChange = true;
		}
		else
		{
			// Reset the state flag as we'll be removing all per-lod colors 
			MeshPaintingSubsystem->ClearSelectionLODColors();
			GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->RemovePerLODColors(PaintableComponents);
		}
	}

	//The user cancel the change, avoid changing the value
	if (bAbortChange)
	{
		return;
	}

	for (UMeshComponent* SelectedComponent : PaintableComponents)
	{
		if (SelectedComponent)
		{
			TSharedPtr<IMeshPaintComponentAdapter> MeshAdapter = MeshPaintingSubsystem->GetAdapterForComponent(SelectedComponent);
			GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>()->ApplyVertexColorsToAllLODs(*MeshAdapter, SelectedComponent);
			FComponentReregisterContext ReregisterContext(SelectedComponent);
		}
	}
	UpdateCachedVertexDataSize();
	MeshPaintingSubsystem->Refresh();
}

template<typename ComponentClass>
TArray<ComponentClass*> UMeshPaintMode::GetSelectedComponents() const
{
	TArray<ComponentClass*> Components;
	FToolBuilderState SelectionState;
	GetToolManager()->GetContextQueriesAPI()->GetCurrentSelectionState(SelectionState);
//	if (PaintSettings->PaintMode == EPaintMode::Textures)
	{

		for (int32 SelectionIndex = 0; SelectionIndex < SelectionState.SelectedComponents.Num(); ++SelectionIndex)
		{
			ComponentClass* SelectedComponent = Cast<ComponentClass>(SelectionState.SelectedComponents[SelectionIndex]);
			if (SelectedComponent)
			{
				Components.AddUnique(SelectedComponent);
			}
		}
	}

	if (Components.Num() == 0)
	{
		for (int32 SelectionIndex = 0; SelectionIndex < SelectionState.SelectedActors.Num(); ++SelectionIndex)
		{
			AActor* SelectedActor = Cast<AActor>(SelectionState.SelectedActors[SelectionIndex]);
			if (SelectedActor)
			{
				TInlineComponentArray<ComponentClass*> ActorComponents;
				SelectedActor->GetComponents(ActorComponents);
				for (ComponentClass* Component : ActorComponents)
				{
					Components.AddUnique(Component);
				}
			}
		}
	}

	return Components;
}

template TArray<UStaticMeshComponent*> UMeshPaintMode::GetSelectedComponents<UStaticMeshComponent>() const;
template TArray<USkeletalMeshComponent*> UMeshPaintMode::GetSelectedComponents<USkeletalMeshComponent>() const;
template TArray<UMeshComponent*> UMeshPaintMode::GetSelectedComponents<UMeshComponent>() const;
template TArray<UGeometryCollectionComponent*> UMeshPaintMode::GetSelectedComponents<UGeometryCollectionComponent>() const;


void UMeshPaintMode::UpdateCachedVertexDataSize()
{
	CachedVertexDataSize = 0;

	const bool bInstance = true;
	if (UMeshPaintingSubsystem* MeshPaintingSubsystem = GEngine->GetEngineSubsystem<UMeshPaintingSubsystem>())
	{
		for (UMeshComponent* SelectedComponent : MeshPaintingSubsystem->GetPaintableMeshComponents())
		{
			int32 NumLODs = MeshPaintingSubsystem->GetNumberOfLODs(SelectedComponent);
			for (int32 LODIndex = 0; LODIndex < NumLODs; ++LODIndex)
			{
				CachedVertexDataSize += MeshPaintingSubsystem->GetVertexColorBufferSize(SelectedComponent, LODIndex, bInstance);
			}
		}
	}
	bRecacheVertexDataSize = false;
}



void UMeshPaintMode::CycleMeshLODs(int32 Direction)
{
	if (UMeshColorPaintingTool* ColorPaintingTool = Cast<UMeshColorPaintingTool>(GetToolManager()->GetActiveTool(EToolSide::Left)))
	{
		FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_CycleLOD", "Changed Current LOD"));
		ColorPaintingTool->CycleMeshLODs(Direction);
	}
}

void UMeshPaintMode::CycleTextures(int32 Direction)
{
	if (UMeshTexturePaintingTool* TexturePaintingTool = Cast<UMeshTexturePaintingTool>(GetToolManager()->GetActiveTool(EToolSide::Left)))
	{
		FScopedTransaction Transaction(LOCTEXT("LevelMeshPainter_CycleTexture", "Changed Current Texture"));
		TexturePaintingTool->CycleTextures(Direction);
	}
}

bool UMeshPaintMode::CanCycleTextures() const
{
	UMeshTexturePaintingTool* TexturePaintingTool = Cast<UMeshTexturePaintingTool>(GetToolManager()->GetActiveTool(EToolSide::Left));
	return TexturePaintingTool != nullptr;
}

void UMeshPaintMode::CommitAllPaintedTextures()
{
	if (UMeshTexturePaintingTool* TexturePaintingTool = Cast<UMeshTexturePaintingTool>(GetToolManager()->GetActiveTool(EToolSide::Left)))
	{
		TexturePaintingTool->CommitAllPaintedTextures();
	}
}

int32 UMeshPaintMode::GetNumberOfPendingPaintChanges()
{
	if (UMeshTexturePaintingTool* TexturePaintingTool = Cast<UMeshTexturePaintingTool>(GetToolManager()->GetActiveTool(EToolSide::Left)))
	{
		return TexturePaintingTool->GetNumberOfPendingPaintChanges();
	}
	return 0;
}


void UMeshPaintMode::ActivateDefaultTool()
{
	FName PaletteName = Toolkit->GetCurrentPalette();
	if (PaletteName == UMeshPaintMode::MeshPaintMode_Color)
	{
		GetInteractiveToolsContext()->StartTool(VertexSelectToolName);
	}
	if (PaletteName == UMeshPaintMode::MeshPaintMode_Weights)
	{
		GetInteractiveToolsContext()->StartTool(VertexSelectToolName);
	}
	if (PaletteName == UMeshPaintMode::MeshPaintMode_Texture)
	{
		GetInteractiveToolsContext()->StartTool(TextureSelectToolName);
	}
}

void UMeshPaintMode::UpdateOnPaletteChange(FName NewPaletteName)
{
	UpdateSelectedMeshes();

	FString ActiveTool = GetToolManager()->GetActiveToolName(EToolSide::Mouse);
	bool bInAnyPaintTool = (ActiveTool == WeightPaintToolName || ActiveTool == TexturePaintToolName || ActiveTool == ColorPaintToolName);

	// figure out which tool we would like to be in based on currently-active tool
	FString SwitchToTool;
	if (NewPaletteName == UMeshPaintMode::MeshPaintMode_Color)
	{
		SwitchToTool = (bInAnyPaintTool) ? ColorPaintToolName : VertexSelectToolName;
	}
	else if (NewPaletteName == UMeshPaintMode::MeshPaintMode_Weights)
	{
		SwitchToTool = (bInAnyPaintTool) ? WeightPaintToolName: VertexSelectToolName;
	}
	else if (NewPaletteName == UMeshPaintMode::MeshPaintMode_Texture)
	{
		SwitchToTool = (bInAnyPaintTool) ? TexturePaintToolName : TextureSelectToolName;
	}
	// change to new tool if it is different
	if (SwitchToTool.IsEmpty() == false && SwitchToTool != ActiveTool)
	{
		GetInteractiveToolsContext()->StartTool(SwitchToTool);
	}
}


void UMeshPaintMode::OnResetViewMode()
{
	// Reset viewport color mode for all active viewports
	for (FEditorViewportClient* ViewportClient : GEditor->GetAllViewportClients())
	{
		if (!ViewportClient || ViewportClient->GetModeTools() != GetModeManager())
		{
			continue;
		}

		GEditor->GetEditorSubsystem<UMeshPaintModeSubsystem>()->SetViewportColorMode(EMeshPaintDataColorViewMode::Normal, ViewportClient);
	}
}


#undef LOCTEXT_NAMESPACE


