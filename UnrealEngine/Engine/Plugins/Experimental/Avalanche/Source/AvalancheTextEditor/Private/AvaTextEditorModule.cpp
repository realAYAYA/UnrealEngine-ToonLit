// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaTextEditorModule.h"
#include "AvaInteractiveToolsDelegates.h"
#include "AvaText3DComponent.h"
#include "AvaTextActor.h"
#include "AvaTextDefs.h"
#include "AvaTextEditorCommands.h"
#include "ColorPicker/AvaViewportColorPickerActorClassRegistry.h"
#include "ColorPicker/AvaViewportColorPickerAdapter.h"
#include "DMObjectMaterialProperty.h"
#include "DetailsView/AvaLinearGradientSettingsCustomization.h"
#include "DetailsView/AvaTextAlignmentCustomization.h"
#include "DetailsView/AvaTextComponentCustomization.h"
#include "DetailsView/AvaTextFieldCustomization.h"
#include "Font/AvaFontDetailsCustomization.h"
#include "Font/AvaFontManagerSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "IAvalancheComponentVisualizersModule.h"
#include "IDynamicMaterialEditorModule.h"
#include "Misc/CoreDelegates.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Tool/AvaTextActorTool.h"
#include "Visualizer/AvaTextVisualizer.h"

void FAvaTextEditorModule::StartupModule()
{
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FAvaTextEditorModule::PostEngineInit);

	FAvaTextEditorCommands::Register();
	RegisterCustomLayouts();
	FAvaInteractiveToolsDelegates::GetRegisterToolsDelegate().AddRaw(this, &FAvaTextEditorModule::RegisterTools);

	RegisterDynamicMaterialPropertyGenerator();

	FAvaViewportColorPickerActorClassRegistry::RegisterDefaultClassAdapter<AAvaTextActor>();
}

void FAvaTextEditorModule::ShutdownModule()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);

	FAvaTextEditorCommands::Unregister();

	if (UObjectInitialized() && !IsEngineExitRequested())
	{
		UnregisterCustomLayouts();
	}

	FAvaInteractiveToolsDelegates::GetRegisterToolsDelegate().RemoveAll(this);
}

void FAvaTextEditorModule::PostEngineInit()
{
	if (FSlateApplication::IsInitialized())
	{
		RegisterComponentVisualizers();
	}
}

void FAvaTextEditorModule::RegisterTools(IAvalancheInteractiveToolsModule* InModule)
{
	// 2D Tools
	InModule->RegisterTool(
		IAvalancheInteractiveToolsModule::Get().CategoryNameActor,
		GetDefault<UAvaTextActorTool>()->GetToolParameters()
	);
}

void FAvaTextEditorModule::RegisterComponentVisualizers()
{
	IAvalancheComponentVisualizersModule& AvaComponentVisualizerModule = IAvalancheComponentVisualizersModule::Get();

	AvaComponentVisualizerModule.RegisterComponentVisualizer<UAvaText3DComponent, FAvaTextVisualizer>(&Visualizers);
}

void FAvaTextEditorModule::RegisterCustomLayouts()
{
	static FName PropertyEditor("PropertyEditor");

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

	// Text
	PropertyModule.RegisterCustomPropertyTypeLayout(FAvaFont::StaticStruct()->GetFName()
		, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAvaFontDetailsCustomization::MakeInstance));

	PropertyModule.RegisterCustomPropertyTypeLayout(FAvaTextAlignment::StaticStruct()->GetFName()
		, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAvaTextAlignmentCustomization::MakeInstance));

	PropertyModule.RegisterCustomPropertyTypeLayout(FAvaLinearGradientSettings::StaticStruct()->GetFName()
		, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAvaLinearGradientSettingsCustomization::MakeInstance));

	PropertyModule.RegisterCustomPropertyTypeLayout(FAvaTextField::StaticStruct()->GetFName()
		, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FAvaTextFieldCustomization::MakeInstance));

	PropertyModule.RegisterCustomClassLayout(UAvaText3DComponent::StaticClass()->GetFName()
		, FOnGetDetailCustomizationInstance::CreateStatic(&FAvaTextComponentCustomization::MakeInstance));
}

void FAvaTextEditorModule::UnregisterCustomLayouts()
{
	static FName PropertyEditor("PropertyEditor");

	if (FModuleManager::Get().IsModuleLoaded(PropertyEditor))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(PropertyEditor);

		// Text
		PropertyModule.UnregisterCustomPropertyTypeLayout(FAvaFont::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FAvaTextAlignment::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FAvaLinearGradientSettings::StaticStruct()->GetFName());
		PropertyModule.UnregisterCustomPropertyTypeLayout(FAvaTextField::StaticStruct()->GetFName());
	}
}

void FAvaTextEditorModule::RegisterDynamicMaterialPropertyGenerator()
{
	IDynamicMaterialEditorModule::Get().RegisterCustomMaterialPropertyGenerator(
		AAvaTextActor::StaticClass(),
		FDMGetObjectMaterialPropertiesDelegate::CreateLambda(
			[](UObject* InObject)
			{
				static TArray<FProperty*> Text3DProperties = {
					UText3DComponent::StaticClass()->FindPropertyByName("FrontMaterial"),
					UText3DComponent::StaticClass()->FindPropertyByName("BevelMaterial"),
					UText3DComponent::StaticClass()->FindPropertyByName("ExtrudeMaterial"),
					UText3DComponent::StaticClass()->FindPropertyByName("BackMaterial")
				};

				TArray<FDMObjectMaterialProperty> Properties;

				if (AAvaTextActor* AvaTextActor = Cast<AAvaTextActor>(InObject))
				{
					if (UText3DComponent* TextComponent = AvaTextActor->GetText3DComponent())
					{
						Properties.Reserve(Text3DProperties.Num());

						for (FProperty* Property : Text3DProperties)
						{
							if (Property)
							{
								Properties.Emplace(TextComponent, Property);
							}
						}
					}
				}

				return Properties;
			}
		)
	);
}

IMPLEMENT_MODULE(FAvaTextEditorModule, AvalancheTextEditorModule)
