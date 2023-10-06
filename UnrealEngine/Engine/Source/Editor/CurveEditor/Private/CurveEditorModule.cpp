// Copyright Epic Games, Inc. All Rights Reserved.

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "CoreGlobals.h"
#include "CurveEditorCommands.h"
#include "CurveEditorTypes.h"
#include "CurveEditorViewRegistry.h"
#include "Delegates/Delegate.h"
#include "Filters/CurveEditorBakeFilter.h"
#include "Filters/CurveEditorBakeFilterCustomization.h"
#include "HAL/PlatformCrt.h"
#include "ICurveEditorModule.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"

class FCurveEditorModule : public ICurveEditorModule
{
public:
	virtual void StartupModule() override
	{
		if (GIsEditor)
		{
			if (UToolMenus::TryGet())
			{
				FCurveEditorCommands::Register();
			}
			else
			{
				FCoreDelegates::OnPostEngineInit.AddStatic(&FCurveEditorCommands::Register);
			}
		}

		RegisterCustomizations();
	}

	virtual void ShutdownModule() override
	{
		FCurveEditorCommands::Unregister();

		UnregisterCustomizations();
	}

	virtual FDelegateHandle RegisterEditorExtension(FOnCreateCurveEditorExtension InOnCreateCurveEditorExtension) override
	{
		EditorExtensionDelegates.Add(InOnCreateCurveEditorExtension);
		FDelegateHandle Handle = EditorExtensionDelegates.Last().GetHandle();
		
		return Handle;
	}

	virtual void UnregisterEditorExtension(FDelegateHandle InHandle) override
	{
		EditorExtensionDelegates.RemoveAll([=](const FOnCreateCurveEditorExtension& Delegate) { return Delegate.GetHandle() == InHandle; });
	}

	virtual FDelegateHandle RegisterToolExtension(FOnCreateCurveEditorToolExtension InOnCreateCurveEditorToolExtension) override
	{
		ToolExtensionDelegates.Add(InOnCreateCurveEditorToolExtension);
		FDelegateHandle Handle = ToolExtensionDelegates.Last().GetHandle();

		return Handle;
	}

	virtual void UnregisterToolExtension(FDelegateHandle InHandle) override
	{
		ToolExtensionDelegates.RemoveAll([=](const FOnCreateCurveEditorToolExtension& Delegate) { return Delegate.GetHandle() == InHandle; });
	}

	virtual ECurveEditorViewID RegisterView(FOnCreateCurveEditorView InCreateViewDelegate) override
	{
		return FCurveEditorViewRegistry::Get().RegisterCustomView(InCreateViewDelegate);
	}

	virtual void UnregisterView(ECurveEditorViewID InViewID) override
	{
		return FCurveEditorViewRegistry::Get().UnregisterCustomView(InViewID);
	}

	virtual TArray<FCurveEditorMenuExtender>& GetAllToolBarMenuExtenders() override
	{
		return ToolBarMenuExtenders;
	}

	virtual TArrayView<const FOnCreateCurveEditorExtension> GetEditorExtensions() const override
	{
		return EditorExtensionDelegates;
	}

	virtual TArrayView<const FOnCreateCurveEditorToolExtension> GetToolExtensions() const override
	{
		return ToolExtensionDelegates;
	}

	void RegisterCustomizations()
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditorModule.RegisterCustomClassLayout(UCurveEditorBakeFilter::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCurveEditorBakeFilterCustomization::MakeInstance));
		PropertyEditorModule.NotifyCustomizationModuleChanged();
	}

	void UnregisterCustomizations()
	{
		if (UObjectInitialized() && !IsEngineExitRequested())
		{
			if (FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor"))
			{
				PropertyEditorModule->UnregisterCustomClassLayout(UCurveEditorBakeFilter::StaticClass()->GetFName());
				PropertyEditorModule->NotifyCustomizationModuleChanged();
			}
		}
	}

private:
	/** List of editor extension handler delegates Curve Editors will execute when they are created. */
	TArray<FOnCreateCurveEditorExtension> EditorExtensionDelegates;

	/** List of tool extension handler delegates Curve Editors will execute when they are created. */
	TArray<FOnCreateCurveEditorToolExtension> ToolExtensionDelegates;

	/** List of Extenders that that should be called when building the Curve Editor toolbar. */
	TArray<FCurveEditorMenuExtender> ToolBarMenuExtenders;

};

IMPLEMENT_MODULE(FCurveEditorModule, CurveEditor)