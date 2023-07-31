// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "SXcodeGPUDebuggerPluginEditorExtension.h"

#include "XcodeGPUDebuggerPluginCommands.h"
#include "XcodeGPUDebuggerPluginModule.h"
#include "XcodeGPUDebuggerPluginStyle.h"

#include "Editor/EditorEngine.h"
#include "Editor/UnrealEd/Public/SEditorViewportToolBarMenu.h"
#include "Editor/UnrealEd/Public/SViewportToolBarComboMenu.h"
#include "Editor/UnrealEd/Public/Kismet2/DebuggerCommands.h"
#include "Styling/AppStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxDefs.h"
#include "Interfaces/IMainFrameModule.h"
#include "LevelEditor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

extern UNREALED_API UEditorEngine* GEditor;

class SXcodeGPUDebuggerCaptureButton : public SViewportToolBar
{
public:
	SLATE_BEGIN_ARGS(SXcodeGPUDebuggerCaptureButton) { }
	SLATE_END_ARGS()

	/** Widget constructor */
	void Construct(const FArguments& Args)
	{
		FSlateIcon IconBrush = FSlateIcon(FXcodeGPUDebuggerPluginStyle::Get()->GetStyleSetName(), "XcodeGPUDebuggerPlugin.CaptureFrameIcon");

		ChildSlot
		[
			SNew(SButton)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Bottom)
			.ButtonStyle(FAppStyle::Get(), "ViewportMenu.Button")
			.ContentPadding(FMargin(1.0f))
			.ToolTipText(FXcodeGPUDebuggerPluginCommands::Get().CaptureFrameCommand->GetDescription())
			.OnClicked_Lambda([this]()
				{
					FPlayWorldCommands::GlobalPlayWorldActions->GetActionForCommand(FXcodeGPUDebuggerPluginCommands::Get().CaptureFrameCommand)->Execute();
					return(FReply::Handled());
				})
			[
				SNew(SImage)
				.Image(IconBrush.GetIcon())
			]
		];
	}
};

FXcodeGPUDebuggerPluginEditorExtension::FXcodeGPUDebuggerPluginEditorExtension(FXcodeGPUDebuggerPluginModule* ThePlugin)
	: LoadedDelegateHandle()
	, ToolbarExtension()
	, ExtensionManager()
	, ToolbarExtender()
	, IsEditorInitialized(false)
{
	// Defer Level Editor UI extensions until Level Editor has been loaded:
	if (FModuleManager::Get().IsModuleLoaded("LevelEditor"))
	{
		Initialize(ThePlugin);
	}
	else
	{
		FModuleManager::Get().OnModulesChanged().AddLambda([this, ThePlugin](FName name, EModuleChangeReason reason)
		{
			if ((name == "LevelEditor") && (reason == EModuleChangeReason::ModuleLoaded))
			{
				Initialize(ThePlugin);
			}
		});
	}
}

FXcodeGPUDebuggerPluginEditorExtension::~FXcodeGPUDebuggerPluginEditorExtension()
{
	if (ExtensionManager.IsValid())
	{
		FXcodeGPUDebuggerPluginStyle::Shutdown();
		FXcodeGPUDebuggerPluginCommands::Unregister();

		ToolbarExtender->RemoveExtension(ToolbarExtension.ToSharedRef());

		ExtensionManager->RemoveExtender(ToolbarExtender);
	}
	else
	{
		ExtensionManager.Reset();
	}
}

void FXcodeGPUDebuggerPluginEditorExtension::Initialize(FXcodeGPUDebuggerPluginModule* ThePlugin)
{
	if (GUsingNullRHI)
	{
		return;
	}

	// The LoadModule request below will crash if running as an editor commandlet!
	check(!IsRunningCommandlet());

	FXcodeGPUDebuggerPluginStyle::Initialize();
	FXcodeGPUDebuggerPluginCommands::Register();

	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedRef<FUICommandList> CommandBindings = LevelEditorModule.GetGlobalLevelEditorActions();
	ExtensionManager = LevelEditorModule.GetToolBarExtensibilityManager();
	ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtension = ToolbarExtender->AddToolBarExtension("CameraSpeed", EExtensionHook::After, CommandBindings,
		FToolBarExtensionDelegate::CreateLambda([this, ThePlugin](FToolBarBuilder& ToolbarBuilder)
	{
		AddToolbarExtension(ToolbarBuilder, ThePlugin); })
	);
	ExtensionManager->AddExtender(ToolbarExtender);

	IsEditorInitialized = false;
	FSlateRenderer* SlateRenderer = FSlateApplication::Get().GetRenderer();
	LoadedDelegateHandle = SlateRenderer->OnSlateWindowRendered().AddRaw(this, &FXcodeGPUDebuggerPluginEditorExtension::OnEditorLoaded);
}

void FXcodeGPUDebuggerPluginEditorExtension::OnEditorLoaded(SWindow& SlateWindow, void* ViewportRHIPtr)
{
	// Would be nice to use the preprocessor definition WITH_EDITOR instead, but the user may launch a standalone the game through the editor...
	if (GEditor == nullptr)
	{
		return;
	}

	if (IsInGameThread())
	{
		FSlateRenderer* SlateRenderer = FSlateApplication::Get().GetRenderer();
		SlateRenderer->OnSlateWindowRendered().Remove(LoadedDelegateHandle);
	}

	if (IsEditorInitialized)
	{
		return;
	}
	IsEditorInitialized = true;

	if(FPlayWorldCommands::GlobalPlayWorldActions.IsValid())
	{
		//Register the editor hotkeys
		FPlayWorldCommands::GlobalPlayWorldActions->MapAction(FXcodeGPUDebuggerPluginCommands::Get().CaptureFrameCommand,
			FExecuteAction::CreateLambda([]()
			{
				FXcodeGPUDebuggerPluginModule& PluginModule = FModuleManager::GetModuleChecked<FXcodeGPUDebuggerPluginModule>("XcodeGPUDebuggerPlugin");
				PluginModule.CaptureFrame(nullptr, IRenderCaptureProvider::ECaptureFlags_Launch, FString());
			}),
			FCanExecuteAction());
 	}
}

void FXcodeGPUDebuggerPluginEditorExtension::AddToolbarExtension(FToolBarBuilder& ToolbarBuilder, FXcodeGPUDebuggerPluginModule* ThePlugin)
{
#define LOCTEXT_NAMESPACE "LevelEditorToolBar"

	UE_LOG(XcodeGPUDebuggerPlugin, Log, TEXT("Attaching toolbar extension..."));
	ToolbarBuilder.AddSeparator();

	ToolbarBuilder.BeginSection("XcodeGPUDebuggerPlugin");
	ToolbarBuilder.AddWidget(SNew(SXcodeGPUDebuggerCaptureButton));
	ToolbarBuilder.EndSection();

#undef LOCTEXT_NAMESPACE
}

#endif //WITH_EDITOR
