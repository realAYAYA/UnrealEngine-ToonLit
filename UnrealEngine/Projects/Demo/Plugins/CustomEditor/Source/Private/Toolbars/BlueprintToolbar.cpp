#include "BlueprintToolbar.h"

#include "BlueprintEditorModule.h"

void FBlueprintToolbar::Initialize()
{
	FCustomEditorToolbar::Initialize();

	auto& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
	auto& ExtenderDelegates = BlueprintEditorModule.GetMenuExtensibilityManager()->GetExtenderDelegates();
	ExtenderDelegates.Add(
		FAssetEditorExtender::CreateLambda
		([&](const TSharedRef<FUICommandList>&, const TArray<UObject*>& ContextSensitiveObjects)
		{
			const auto TargetObject = ContextSensitiveObjects.Num() < 1 ? nullptr : Cast<UBlueprint>(ContextSensitiveObjects[0]);
			return GetExtender(TargetObject);
		}));
}

TSharedRef<FExtender> FBlueprintToolbar::GetExtender(UObject* InContextObject)
{
	const auto ExtensionDelegate = FToolBarExtensionDelegate::CreateLambda(
		[this, InContextObject](FToolBarBuilder& ToolbarBuilder)
		{
			BuildToolbar(ToolbarBuilder, InContextObject);
		});
	
	TSharedRef<FExtender> ToolbarExtender(new FExtender());
	if (InContextObject)
		ToolbarExtender->AddToolBarExtension("Debugging", EExtensionHook::After, CommandList, ExtensionDelegate);
	else
		ToolbarExtender->AddToolBarExtension("Compile", EExtensionHook::After, CommandList, ExtensionDelegate);
	
	return ToolbarExtender;
}