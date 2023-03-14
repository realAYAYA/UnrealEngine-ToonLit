// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_EDITOR

#include "Actions/GLTFProxyAssetActions.h"
#include "Actions/GLTFEditorStyle.h"
#include "Options/GLTFProxyOptions.h"
#include "UI/GLTFProxyOptionsWindow.h"
#include "Converters/GLTFMaterialProxyFactory.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "GLTFProxyAssetActions"

FGLTFProxyAssetActions::FGLTFProxyAssetActions(const TSharedRef<IAssetTypeActions>& OriginalActions)
	: OriginalActions(OriginalActions)
{
}

void FGLTFProxyAssetActions::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	OriginalActions->GetActions(InObjects, Section);
	GetProxyActions(InObjects, Section);
}

void FGLTFProxyAssetActions::GetProxyActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	const TArray<FWeakObjectPtr> Objects(InObjects);

	Section.AddMenuEntry(
		"MenuEntry_CreateProxy",
		LOCTEXT("MenuEntry_CreateProxy", "Create glTF Proxy Material"),
		LOCTEXT("MenuEntry_CreateProxyTooltip", "Creates a proxy version of this material for glTF export."),
		FSlateIcon(FGLTFEditorStyle::Get().GetStyleSetName(), "Icon16"),
		FUIAction(FExecuteAction::CreateSP(this, &FGLTFProxyAssetActions::OnCreateProxy, Objects))
		);
}

void FGLTFProxyAssetActions::OnCreateProxy(TArray<FWeakObjectPtr> Objects) const
{
	UGLTFProxyOptions* Options = NewObject<UGLTFProxyOptions>();

	if (!SGLTFProxyOptionsWindow::ShowDialog(Options))
	{
		return;
	}

	FGLTFMaterialProxyFactory ProxyFactory(Options);

	for (const FWeakObjectPtr& Object : Objects)
	{
		if (UMaterialInterface* Material = Cast<UMaterialInterface>(Object.Get()))
		{
			ProxyFactory.RootPath = FPaths::GetPath(Material->GetPathName()) / TEXT("GLTF");
			ProxyFactory.Create(Material);
		}
	}

	ProxyFactory.OpenLog();
}

#undef LOCTEXT_NAMESPACE

#endif
