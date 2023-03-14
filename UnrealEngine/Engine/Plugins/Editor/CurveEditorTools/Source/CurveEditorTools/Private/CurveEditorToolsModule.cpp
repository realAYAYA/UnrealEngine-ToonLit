// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "ICurveEditorModule.h"
#include "CurveEditorFocusExtension.h"
#include "CurveEditorToolCommands.h"
#include "Tools/CurveEditorTransformTool.h"
#include "Tools/CurveEditorRetimeTool.h"
#include "Tools/CurveEditorMultiScaleTool.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "CurveEditorToolsModule"

class FCurveEditorToolsModule : public IModuleInterface, public TSharedFromThis<FCurveEditorToolsModule>
{
	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// ~IModuleInterface

private:
	static TSharedRef<ICurveEditorExtension> CreateFocusExtension(TWeakPtr<FCurveEditor> InCurveEditor);
	static TUniquePtr<ICurveEditorToolExtension> CreateTransformToolExtension(TWeakPtr<FCurveEditor> InCurveEditor);
	static TUniquePtr<ICurveEditorToolExtension> CreateRetimeToolExtension(TWeakPtr<FCurveEditor> InCurveEditor);
	static TUniquePtr<ICurveEditorToolExtension> CreateMultiScaleToolExtension(TWeakPtr<FCurveEditor> InCurveEditor);

protected:
	TSharedRef<FExtender> ExtendCurveEditorToolbarMenu(const TSharedRef<FUICommandList> CommandList);


private:
	FDelegateHandle FocusExtensionsHandle;
	FDelegateHandle TransformToolHandle;
	FDelegateHandle RetimeToolHandle;
	FDelegateHandle MultiScaleToolHandle;
};

IMPLEMENT_MODULE(FCurveEditorToolsModule, CurveEditorTools)

void FCurveEditorToolsModule::StartupModule()
{
	FCurveEditorToolCommands::Register();

	ICurveEditorModule& CurveEditorModule = FModuleManager::Get().LoadModuleChecked<ICurveEditorModule>("CurveEditor");

	// Register Editor Extensions
	FocusExtensionsHandle = CurveEditorModule.RegisterEditorExtension(FOnCreateCurveEditorExtension::CreateStatic(&FCurveEditorToolsModule::CreateFocusExtension));

	// Register Tool Extensions
	TransformToolHandle = CurveEditorModule.RegisterToolExtension(FOnCreateCurveEditorToolExtension::CreateStatic(&FCurveEditorToolsModule::CreateTransformToolExtension));
	RetimeToolHandle = CurveEditorModule.RegisterToolExtension(FOnCreateCurveEditorToolExtension::CreateStatic(&FCurveEditorToolsModule::CreateRetimeToolExtension));
	MultiScaleToolHandle = CurveEditorModule.RegisterToolExtension(FOnCreateCurveEditorToolExtension::CreateStatic(&FCurveEditorToolsModule::CreateMultiScaleToolExtension));

	auto ToolbarExtender = ICurveEditorModule::FCurveEditorMenuExtender::CreateRaw(this, &FCurveEditorToolsModule::ExtendCurveEditorToolbarMenu);
	auto& MenuExtenders = CurveEditorModule.GetAllToolBarMenuExtenders();
	MenuExtenders.Add(ToolbarExtender);
}

TSharedRef<FExtender> FCurveEditorToolsModule::ExtendCurveEditorToolbarMenu(const TSharedRef<FUICommandList> CommandList)
{
	struct Local
	{
		static void FillToolbarTools(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.AddToolBarButton(FCurveEditorToolCommands::Get().ActivateTransformTool);
			ToolbarBuilder.AddToolBarButton(FCurveEditorToolCommands::Get().ActivateRetimeTool);
			ToolbarBuilder.AddToolBarButton(FCurveEditorToolCommands::Get().ActivateMultiScaleTool);
		}

		static void FillToolbarFraming(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.AddToolBarButton(FCurveEditorToolCommands::Get().SetFocusPlaybackTime);
			ToolbarBuilder.AddToolBarButton(FCurveEditorToolCommands::Get().SetFocusPlaybackRange);
		}
	};

	TSharedRef<FExtender> Extender = MakeShared<FExtender>();
	Extender->AddToolBarExtension("Tools",
	EExtensionHook::After,
	CommandList,
	FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbarTools));
	
	Extender->AddToolBarExtension("Framing",
	EExtensionHook::After,
	CommandList,
	FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbarFraming));

	return Extender;
}
void FCurveEditorToolsModule::ShutdownModule()
{
	ICurveEditorModule& CurveEditorModule = FModuleManager::Get().LoadModuleChecked<ICurveEditorModule>("CurveEditor");

	// Unregister Editor Extensions
	CurveEditorModule.UnregisterEditorExtension(FocusExtensionsHandle);

	// Unregister Tool Extensions
	CurveEditorModule.UnregisterToolExtension(TransformToolHandle);
	CurveEditorModule.UnregisterToolExtension(RetimeToolHandle);
	CurveEditorModule.UnregisterEditorExtension(MultiScaleToolHandle);

	FCurveEditorToolCommands::Unregister();
}

TSharedRef<ICurveEditorExtension> FCurveEditorToolsModule::CreateFocusExtension(TWeakPtr<FCurveEditor> InCurveEditor) 
{
	return MakeShared<FCurveEditorFocusExtension>(InCurveEditor);
}

TUniquePtr<ICurveEditorToolExtension> FCurveEditorToolsModule::CreateTransformToolExtension(TWeakPtr<FCurveEditor> InCurveEditor)
{
	return MakeUnique<FCurveEditorTransformTool>(InCurveEditor);
}

TUniquePtr<ICurveEditorToolExtension> FCurveEditorToolsModule::CreateRetimeToolExtension(TWeakPtr<FCurveEditor> InCurveEditor)
{
	return MakeUnique<FCurveEditorRetimeTool>(InCurveEditor);
}

TUniquePtr<ICurveEditorToolExtension> FCurveEditorToolsModule::CreateMultiScaleToolExtension(TWeakPtr<FCurveEditor> InCurveEditor)
{
	return MakeUnique<FCurveEditorMultiScaleTool>(InCurveEditor);
}

#undef LOCTEXT_NAMESPACE
