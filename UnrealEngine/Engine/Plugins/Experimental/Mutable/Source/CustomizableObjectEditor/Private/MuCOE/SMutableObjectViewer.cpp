// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCOE/SMutableObjectViewer.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/Views/TableViewMetadata.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "MuCOE/CustomizableObjectCompileRunnable.h"
#include "MuCOE/CustomizableObjectEditorStyle.h"
#include "MuCOE/SMutableCodeViewer.h"
#include "MuCOE/SMutableGraphViewer.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SNumericDropDown.h"
#include "Widgets/Views/STreeView.h"

class FExtender;
class FUICommandList;
class ITableRow;
class SWidget;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "SMutableDebugger"


void SMutableObjectViewer::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(CustomizableObject);
}


FString SMutableObjectViewer::GetReferencerName() const
{
	return TEXT("SMutableObjectViewer");
}


void SMutableObjectViewer::Construct(const FArguments& InArgs, UCustomizableObject* InObject)
{
	CustomizableObject = InObject;

	// Initialize the debugger compile options
	CompileOptions.TextureCompression = ECustomizableObjectTextureCompression::Fast;
	CompileOptions.OptimizationLevel = 2;
	{
		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		const TArray<ITargetPlatform*>& Platforms = TPM->GetActiveTargetPlatforms();
		CompileOptions.TargetPlatform = Platforms.IsEmpty() ? nullptr : Platforms[0];
	}

	FToolBarBuilder ToolbarBuilder(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None, TSharedPtr<FExtender>(), true);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Visible);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), "SlimToolBar");

	ToolbarBuilder.BeginSection("Compilation");

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &SMutableObjectViewer::GenerateMutableGraphPressed)),
		NAME_None,
		LOCTEXT("GenerateMutableGraph", "Unreal to Mutable Graph"),
		LOCTEXT("GenerateMutableGraphTooltip", "Generate a mutable graph from the customizable object source graph."),
		FSlateIcon(FCustomizableObjectEditorStyle::Get().GetStyleSetName(), "CustomizableObjectDebugger.GenerateMutableGraph", "CustomizableObjectDebugger.GenerateMutableGraph.Small"),
		EUserInterfaceActionType::Button
	);

	ToolbarBuilder.AddToolBarButton(
		FUIAction(FExecuteAction::CreateSP(this, &SMutableObjectViewer::CompileMutableCodePressed)),
		NAME_None,
		LOCTEXT("GenerateMutableCode", "Unreal to Mutable Code"),
		LOCTEXT("GenerateMutableCodeTooltipFromGraph", "Generate a mutable code from the customizable object source graph."),
		FSlateIcon(FCustomizableObjectEditorStyle::Get().GetStyleSetName(), "CustomizableObjectDebugger.CompileMutableCode", "CustomizableObjectDebugger.CompileMutableCode.Small"),
		EUserInterfaceActionType::Button
	);

	ToolbarBuilder.AddComboButton(
		FUIAction(),
		FOnGetContent::CreateSP(this, &SMutableObjectViewer::GenerateCompileOptionsMenuContent),
		LOCTEXT("Compile_Options_Label", "Compile Options"),
		LOCTEXT("Compile_Options_Tooltip", "Change Compile Options"),
		TAttribute<FSlateIcon>(),
		true);

	ToolbarBuilder.EndSection();
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		[
			ToolbarBuilder.MakeWidget()
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(DebuggerContentsBox, SBorder)
		]
	];
}


void SMutableObjectViewer::GenerateMutableGraphPressed()
{
	// Convert from Unreal graph to Mutable graph.
	TArray<TSoftObjectPtr<UTexture>> RuntimeTextures;
	TArray<TSoftObjectPtr<UTexture>> CompilerTextures;
	mu::Ptr<mu::Node> RootNode = Compiler.Export(CustomizableObject, CompileOptions, RuntimeTextures, CompilerTextures);
	if (!RootNode)
	{
		// TODO: Show errors
		return;
	}

	FString DataTag = FString::Printf(TEXT("Mutable Graph : %s"), *CompileOptions.TargetPlatform->PlatformName());

	TSharedRef<SMutableGraphViewer> GraphViewer = SNew(SMutableGraphViewer, RootNode)
		.DataTag(DataTag)
		.ReferencedRuntimeTextures(RuntimeTextures)
		.ReferencedCompileTextures(CompilerTextures);
	
	DebuggerContentsBox->ClearContent();
	DebuggerContentsBox->SetContent(GraphViewer);
}


void SMutableObjectViewer::CompileMutableCodePressed()
{
	TArray<TSoftObjectPtr<UTexture>> RuntimeTextures;
	TArray<TSoftObjectPtr<UTexture>> CompilerTextures;
	if (CompileOptions.bForceLargeLODBias)
	{
		// Debug compile with many different biases
		constexpr int32 MaxBias = 15;
		for (int32 Bias = 0; Bias < MaxBias; ++Bias)
		{
			CompileOptions.DebugBias = Bias;

			RuntimeTextures.Empty();
			CompilerTextures.Empty();
			mu::NodePtr RootNode = Compiler.Export(CustomizableObject, CompileOptions, RuntimeTextures, CompilerTextures);
			if (!RootNode)
			{
				// TODO: Show errors
				ensure(false);
				return;
			}

			// Do the compilation to Mutable Code synchronously.
			TSharedPtr<FCustomizableObjectCompileRunnable> CompileTask = MakeShareable(new FCustomizableObjectCompileRunnable(RootNode));
			CompileTask->Options = CompileOptions;
			CompileTask->ReferencedTextures = CompilerTextures;
			CompileTask->Init();
			CompileTask->Run();
		}
	}

	// Convert from Unreal graph to Mutable graph.
	RuntimeTextures.Empty();
	CompilerTextures.Empty();
	mu::NodePtr RootNode = Compiler.Export(CustomizableObject, CompileOptions, RuntimeTextures, CompilerTextures);
	if (!RootNode)
	{
		// TODO: Show errors
		ensure(false);
		return;
	}

	// Do the compilation to Mutable Code synchronously.
	TSharedPtr<FCustomizableObjectCompileRunnable> CompileTask = MakeShareable(new FCustomizableObjectCompileRunnable(RootNode));
	CompileTask->Options = CompileOptions;
	CompileTask->ReferencedTextures = CompilerTextures;
	CompileTask->Init();
	CompileTask->Run();

	FString DataTag = FString::Printf( TEXT("Mutable Code : %s opt %d"), *CompileOptions.TargetPlatform->PlatformName(), CompileOptions.OptimizationLevel );

	TSharedRef<SMutableCodeViewer> CodeViewer = SNew(SMutableCodeViewer, CompileTask->Model, RuntimeTextures)
		.DataTag(DataTag);
	
	DebuggerContentsBox->ClearContent();
	DebuggerContentsBox->SetContent(CodeViewer);
}


TSharedRef<SWidget> SMutableObjectViewer::GenerateCompileOptionsMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = false;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	// settings
	MenuBuilder.BeginSection("Optimization", LOCTEXT("MutableCompileOptimizationHeading", "Optimization"));
	{
		// Unreal Graph to Mutable Graph options
		//-----------------------------------

		// Platform
		DebugPlatformStrings.Empty();

		TSharedPtr<FString> SelectedPlatform;
		{
			ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
			check(TPM);

			ITargetPlatform* CurrentPlatform = NULL;
			const TArray<ITargetPlatform*>& Platforms = TPM->GetTargetPlatforms();
			for (const ITargetPlatform* Platform : Platforms)
			{
				TSharedPtr<FString> ThisPlatform = MakeShareable(new FString(Platform->PlatformName()));
				DebugPlatformStrings.Add(ThisPlatform);
				if (Platform == CompileOptions.TargetPlatform)
				{
					SelectedPlatform = ThisPlatform;
				}
			}
		}

		if (!SelectedPlatform.IsValid() && DebugPlatformStrings.Num())
		{
			SelectedPlatform = DebugPlatformStrings[0];
		}

		DebugPlatformCombo =
			SNew(STextComboBox)
			.OptionsSource(&DebugPlatformStrings)
			.InitiallySelectedItem(SelectedPlatform)
			.OnSelectionChanged(this, &SMutableObjectViewer::OnChangeDebugPlatform)
			;

		MenuBuilder.AddWidget(DebugPlatformCombo.ToSharedRef(), LOCTEXT("MutableDebugPlatform", "Target Platform"));

		// Compilation options
		//-----------------------------------

		// Optimisation level
		CompileOptimizationStrings.Empty();
		CompileOptimizationStrings.Add(MakeShareable(new FString(LOCTEXT("OptimizationNone", "None").ToString())));
		CompileOptimizationStrings.Add(MakeShareable(new FString(LOCTEXT("OptimizationMin", "Minimal").ToString())));
		CompileOptimizationStrings.Add(MakeShareable(new FString(LOCTEXT("OptimizationMax", "Maximum").ToString())));

		CompileOptions.OptimizationLevel = FMath::Min(CompileOptions.OptimizationLevel, CompileOptimizationStrings.Num() - 1);

		CompileOptimizationCombo =
			SNew(STextComboBox)
			.OptionsSource(&CompileOptimizationStrings)
			.InitiallySelectedItem(CompileOptimizationStrings[CompileOptions.OptimizationLevel])
			.OnSelectionChanged(this, &SMutableObjectViewer::OnChangeCompileOptimizationLevel)
			;
		MenuBuilder.AddWidget(CompileOptimizationCombo.ToSharedRef(), LOCTEXT("MutableCompileOptimizationLevel", "Optimization Level"));

		{
			CompileTextureCompressionStrings.Empty();
			CompileTextureCompressionStrings.Add(MakeShareable(new FString(LOCTEXT("MutableTextureCompressionNone", "None").ToString())));
			CompileTextureCompressionStrings.Add(MakeShareable(new FString(LOCTEXT("MutableTextureCompressionFast", "Fast").ToString())));
			CompileTextureCompressionStrings.Add(MakeShareable(new FString(LOCTEXT("MutableTextureCompressionHighQuality", "High Quality").ToString())));

			int32 SelectedCompression = FMath::Clamp(int32(CompileOptions.TextureCompression), 0, CompileTextureCompressionStrings.Num() - 1);
			CompileTextureCompressionCombo =
				SNew(STextComboBox)
				.OptionsSource(&CompileTextureCompressionStrings)
				.InitiallySelectedItem(CompileTextureCompressionStrings[SelectedCompression])
				.OnSelectionChanged(this, &SMutableObjectViewer::OnChangeCompileTextureCompressionType)
				;

			MenuBuilder.AddWidget(CompileTextureCompressionCombo.ToSharedRef(), LOCTEXT("MutableCompileTextureCompressionType", "Texture Compression"));
		}

		// Image tiling
		// Unfortunately SNumericDropDown doesn't work with integers at the time of writing.
		TArray<SNumericDropDown<float>::FNamedValue> TilingOptions;
		TilingOptions.Add(SNumericDropDown<float>::FNamedValue(0, FText::FromString(TEXT("0")), FText::FromString(TEXT("Disabled"))));
		TilingOptions.Add(SNumericDropDown<float>::FNamedValue(64, FText::FromString(TEXT("64")), FText::FromString(TEXT("64"))));
		TilingOptions.Add(SNumericDropDown<float>::FNamedValue(128, FText::FromString(TEXT("128")), FText::FromString(TEXT("128"))));
		TilingOptions.Add(SNumericDropDown<float>::FNamedValue(256, FText::FromString(TEXT("256")), FText::FromString(TEXT("256"))));
		TilingOptions.Add(SNumericDropDown<float>::FNamedValue(512, FText::FromString(TEXT("512")), FText::FromString(TEXT("512"))));

		CompileTilingCombo = SNew(SNumericDropDown<float>)
			.DropDownValues(TilingOptions)
			.Value_Lambda( [&]() { return float(CompileOptions.ImageTiling); })
			.OnValueChanged_Lambda( [&](float Value) { CompileOptions.ImageTiling = int32(Value); } )
			;
		MenuBuilder.AddWidget(CompileTilingCombo.ToSharedRef(), LOCTEXT("MutableCompileImageTiling", "Image Tiling"));

		// Disk as cache
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Generate_MutableUseDisk", "Enable compiling using the disk as memory."),
			LOCTEXT("Generate_MutableUseDiskTooltip", "This is very slow but supports compiling huge objects. It requires a lot of free space in the OS disk."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]() { CompileOptions.bUseDiskCompilation = !CompileOptions.bUseDiskCompilation; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return CompileOptions.bUseDiskCompilation; })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);

		// Debug LODBias
		MenuBuilder.AddMenuEntry(
			LOCTEXT("ForceLargeLODBias", "Force a large texture LODBias."),
			LOCTEXT("ForceLargeLODBiasTooltip", "This is useful to test compilation of special cook modes."),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([this]() { CompileOptions.bForceLargeLODBias = !CompileOptions.bForceLargeLODBias; }),
				FCanExecuteAction(),
				FIsActionChecked::CreateLambda([this]() { return CompileOptions.bForceLargeLODBias; })),
			NAME_None,
			EUserInterfaceActionType::ToggleButton
		);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}


void SMutableObjectViewer::OnChangeCompileOptimizationLevel(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	CompileOptions.OptimizationLevel = CompileOptimizationStrings.Find(NewSelection);
}


void SMutableObjectViewer::OnChangeCompileTextureCompressionType(TSharedPtr<FString> NewSelection, ESelectInfo::Type)
{
	CompileOptions.TextureCompression = ECustomizableObjectTextureCompression(CompileTextureCompressionStrings.Find(NewSelection));
}


void SMutableObjectViewer::OnChangeDebugPlatform(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo)
{
	if (!CustomizableObject) return;

	CompileOptions.TargetPlatform = nullptr;

	ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	check(TPM);
	const TArray<ITargetPlatform*>& Platforms = TPM->GetTargetPlatforms();
	check(Platforms.Num());

	CompileOptions.TargetPlatform = Platforms[0];

	for (int32 Index = 1; Index < Platforms.Num(); Index++)
	{
		if (Platforms[Index]->PlatformName() == *NewSelection)
		{
			CompileOptions.TargetPlatform = Platforms[Index];
			break;
		}
	}
}

#undef LOCTEXT_NAMESPACE 
