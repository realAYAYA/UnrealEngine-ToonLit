// Copyright Epic Games, Inc. All Rights Reserved.

#include "DynamicMaterialEditorModule.h"
#include "AssetToolsModule.h"
#include "Components/DMMaterialComponent.h"
#include "Components/DMMaterialEffectFunction.h"
#include "Components/DMMaterialStageSource.h"
#include "Components/DMMaterialStageThroughput.h"
#include "Components/DMTextureUV.h"
#include "Components/MaterialStageInputs/DMMSIFunction.h"
#include "Components/MaterialStageInputs/DMMSIThroughput.h"
#include "Components/MaterialValues/DMMaterialValueBool.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "Components/MaterialValues/DMMaterialValueFloat2.h"
#include "Components/MaterialValues/DMMaterialValueFloat3RGB.h"
#include "Components/MaterialValues/DMMaterialValueFloat3RPY.h"
#include "Components/MaterialValues/DMMaterialValueFloat3XYZ.h"
#include "Components/MaterialValues/DMMaterialValueFloat4.h"
#include "Components/MaterialValues/DMMaterialValueTexture.h"
#include "Components/PrimitiveComponent.h"
#include "DMMaterialFunctionLibrary.h"
#include "DMWorldSubsystem.h"
#include "DetailLayoutBuilder.h"
#include "DetailsPanel/DMMaterialInterfaceTypeCustomizer.h"
#include "DetailsPanel/DMPropertyTypeCustomizer.h"
#include "DetailsPanel/Slate/SDMMaterialListExtensionWidget.h"
#include "DynamicMaterialEditorCommands.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Engine/World.h"
#include "LevelEditor/DMLevelEditorIntegration.h"
#include "Material/DynamicMaterialInstance.h"
#include "MaterialList.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Slate/Properties/Editors/SDMPropertyEditBoolValue.h"
#include "Slate/Properties/Editors/SDMPropertyEditFloat1Value.h"
#include "Slate/Properties/Editors/SDMPropertyEditFloat2Value.h"
#include "Slate/Properties/Editors/SDMPropertyEditFloat3RGBValue.h"
#include "Slate/Properties/Editors/SDMPropertyEditFloat3RPYValue.h"
#include "Slate/Properties/Editors/SDMPropertyEditFloat3XYZValue.h"
#include "Slate/Properties/Editors/SDMPropertyEditFloat4Value.h"
#include "Slate/Properties/Editors/SDMPropertyEditTextureValue.h"
#include "Slate/Properties/Generators/DMComponentPropertyRowGenerator.h"
#include "Slate/Properties/Generators/DMInputThroughputPropertyRowGenerator.h"
#include "Slate/Properties/Generators/DMMaterialEffectFunctionPropertyRowGenerator.h"
#include "Slate/Properties/Generators/DMMaterialStageFunctionPropertyRowGenerator.h"
#include "Slate/Properties/Generators/DMMaterialValuePropertyRowGenerator.h"
#include "Slate/Properties/Generators/DMStagePropertyRowGenerator.h"
#include "Slate/Properties/Generators/DMTextureUVPropertyRowGenerator.h"
#include "Slate/Properties/Generators/DMThroughputPropertyRowGenerator.h"
#include "Slate/SDMEditor.h"

DEFINE_LOG_CATEGORY(LogDynamicMaterialEditor);

namespace UE::DynamicMaterialEditor::Private
{
	FDelegateHandle MateriaListWidgetsDelegate;

	void AddMaterialListWidgets(const TSharedRef<FMaterialItemView>& InMaterialItemView,
		UActorComponent* InCurrentComponent,
		IDetailLayoutBuilder& InDetailBuilder,
		TArray<TSharedPtr<SWidget>>& OutExtensions)
	{
		if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(InCurrentComponent))
		{
			OutExtensions.Add(SNew(SDMMaterialListExtensionWidget,
				InMaterialItemView,
				PrimitiveComponent,
				InDetailBuilder
			));
		}
	}
}

const FName FDynamicMaterialEditorModule::TabId = TEXT("MaterialDesigner");
TMap<UClass*, FDMCreateValueEditWidgetDelegate> FDynamicMaterialEditorModule::ValueEditWidgetDelegates;
TMap<UClass*, FDMComponentPropertyRowGeneratorDelegate> FDynamicMaterialEditorModule::ComponentPropertyRowGenerators;
TMap<UClass*, FDMGetObjectMaterialPropertiesDelegate> FDynamicMaterialEditorModule::CustomMaterialPropertyGenerators;
FDMOnUIValueUpdate FDynamicMaterialEditorModule::OnUIValueUpdate;

void FDynamicMaterialEditorModule::RegisterValueEditWidgetDelegate(UClass* InClass, FDMCreateValueEditWidgetDelegate ValueEditBodyDelegate)
{
	ValueEditWidgetDelegates.Emplace(InClass, ValueEditBodyDelegate);
}

FDMCreateValueEditWidgetDelegate FDynamicMaterialEditorModule::GetValueEditWidgetDelegate(UClass* InClass)
{
	FDMCreateValueEditWidgetDelegate* Delegate = ValueEditWidgetDelegates.Find(InClass);

	if (Delegate)
	{
		return *Delegate;
	}

	return FDMCreateValueEditWidgetDelegate::CreateLambda([](TSharedPtr<SDMComponentEdit>, UDMMaterialValue*)->TSharedPtr<SWidget> { return nullptr; });
}

TSharedPtr<SWidget> FDynamicMaterialEditorModule::CreateEditWidgetForValue(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget, UDMMaterialValue* InValue)
{
	if (!IsValid(InValue))
	{
		return SNullWidget::NullWidget;
	}

	FDMCreateValueEditWidgetDelegate ValueDelegate = GetValueEditWidgetDelegate(InValue->GetClass());

	if (ValueDelegate.IsBound())
	{
		return ValueDelegate.Execute(InComponentEditWidget, InValue);
	}

	return SNullWidget::NullWidget;
}

void FDynamicMaterialEditorModule::RegisterComponentPropertyRowGeneratorDelegate(UClass* InClass, FDMComponentPropertyRowGeneratorDelegate InComponentPropertyRowGeneratorDelegate)
{
	ComponentPropertyRowGenerators.Emplace(InClass, InComponentPropertyRowGeneratorDelegate);
}

FDMComponentPropertyRowGeneratorDelegate FDynamicMaterialEditorModule::GetComponentPropertyRowGeneratorDelegate(UClass* InClass)
{
	UClass* FoundClass = nullptr;

	for (const TPair<UClass*, FDMComponentPropertyRowGeneratorDelegate>& Pair : ComponentPropertyRowGenerators)
	{
		// If we have an exact match, return the delegate immediately.
		if (Pair.Key == InClass)
		{
			return Pair.Value;
		}

		if (InClass->IsChildOf(Pair.Key))
		{
			// If we haven't got anything or the current class is a more specific generator.
			if (FoundClass == nullptr || Pair.Key->IsChildOf(FoundClass))
			{
				FoundClass = Pair.Key;
			}
		}
	}

	if (!FoundClass)
	{
		// Return an invalid lambda
		static FDMComponentPropertyRowGeneratorDelegate NullLambda = FDMComponentPropertyRowGeneratorDelegate::CreateLambda(
			[](const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent, TArray<FDMPropertyHandle>& InOutPropertyRows,
				TSet<UDMMaterialComponent*>& InOutProcessedObjects)
			{
			});

		return NullLambda;
	}

	return ComponentPropertyRowGenerators[FoundClass];
}

void FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(const TSharedRef<SDMComponentEdit>& InComponentEditWidget, UDMMaterialComponent* InComponent,
	TArray<FDMPropertyHandle>& InOutPropertyRows, TSet<UDMMaterialComponent*>& InOutProcessedObjects)
{
	if (!IsValid(InComponent))
	{
		return;
	}

	if (InOutProcessedObjects.Contains(InComponent))
	{
		return;
	}

	FDMComponentPropertyRowGeneratorDelegate RowGenerator = GetComponentPropertyRowGeneratorDelegate(InComponent->GetClass());
	RowGenerator.ExecuteIfBound(InComponentEditWidget, InComponent, InOutPropertyRows, InOutProcessedObjects);
}

void FDynamicMaterialEditorModule::RegisterCustomMaterialPropertyGenerator(UClass* InClass, FDMGetObjectMaterialPropertiesDelegate InGenerator)
{
	if (!InClass || !InGenerator.IsBound())
	{
		return;
	}

	CustomMaterialPropertyGenerators.FindOrAdd(InClass) = InGenerator;
}

FDMGetObjectMaterialPropertiesDelegate FDynamicMaterialEditorModule::GetCustomMaterialPropertyGenerator(UClass* InClass)
{
	if (InClass)
	{
		if (CustomMaterialPropertyGenerators.Contains(InClass))
		{
			return CustomMaterialPropertyGenerators[InClass];
		}
	}

	return FDMGetObjectMaterialPropertiesDelegate();
}

FDynamicMaterialEditorModule& FDynamicMaterialEditorModule::Get()
{
	return FModuleManager::LoadModuleChecked<FDynamicMaterialEditorModule>(ModuleName);
}

FDynamicMaterialEditorModule::FDynamicMaterialEditorModule()
	: CommandList(MakeShared<FUICommandList>())
{
}

void FDynamicMaterialEditorModule::StartupModule()
{
	FDynamicMaterialEditorStyle::Initialize();
	FDynamicMaterialEditorCommands::Register();
	MapCommands();

	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	PropertyModule.RegisterCustomPropertyTypeLayout(UDynamicMaterialModelEditorOnlyData::StaticClass()->GetFName()
		, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDMPropertyTypeCustomizer::MakeInstance));

	PropertyModule.RegisterCustomPropertyTypeLayout(UMaterialInterface::StaticClass()->GetFName()
		, FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FDMMaterialInterfaceTypeCustomizer::MakeInstance), MakeShared<FDMMaterialInterfaceTypeIdentifier>());

	using namespace UE::DynamicMaterialEditor::Private;

	MateriaListWidgetsDelegate = FMaterialList::OnAddMaterialItemViewExtraBottomWidget.AddStatic(&AddMaterialListWidgets);

	FDMLevelEditorIntegration::Initialize();

	RegisterValueEditWidgetDelegate<UDMMaterialValueBool,      SDMPropertyEditBoolValue>();
	RegisterValueEditWidgetDelegate<UDMMaterialValueFloat1,    SDMPropertyEditFloat1Value>();
	RegisterValueEditWidgetDelegate<UDMMaterialValueFloat2,    SDMPropertyEditFloat2Value>();
	RegisterValueEditWidgetDelegate<UDMMaterialValueFloat3RGB, SDMPropertyEditFloat3RGBValue>();
	RegisterValueEditWidgetDelegate<UDMMaterialValueFloat3RPY, SDMPropertyEditFloat3RPYValue>();
	RegisterValueEditWidgetDelegate<UDMMaterialValueFloat3XYZ, SDMPropertyEditFloat3XYZValue>();
	RegisterValueEditWidgetDelegate<UDMMaterialValueFloat4,    SDMPropertyEditFloat4Value>();
	RegisterValueEditWidgetDelegate<UDMMaterialValueTexture,   SDMPropertyEditTextureValue>();

	RegisterComponentPropertyRowGeneratorDelegate<UDMMaterialComponent,            FDMComponentPropertyRowGenerator>();
	RegisterComponentPropertyRowGeneratorDelegate<UDMMaterialStage,                FDMStagePropertyRowGenerator>();
	RegisterComponentPropertyRowGeneratorDelegate<UDMMaterialValue,                FDMMaterialValuePropertyRowGenerator>();
	RegisterComponentPropertyRowGeneratorDelegate<UDMTextureUV,                    FDMTextureUVPropertyRowGenerator>();
	RegisterComponentPropertyRowGeneratorDelegate<UDMMaterialStageThroughput,      FDMThroughputPropertyRowGenerator>();
	RegisterComponentPropertyRowGeneratorDelegate<UDMMaterialStageInputThroughput, FDMInputThroughputPropertyRowGenerator>();
	RegisterComponentPropertyRowGeneratorDelegate<UDMMaterialEffectFunction,       FDMMaterialEffectFunctionPropertyRowGenerator>();
	RegisterComponentPropertyRowGeneratorDelegate<UDMMaterialStageInputFunction,   FDMMaterialStageFunctionPropertyRowGenerator>();

	FDynamicMaterialModule::GetCreateEditorOnlyDataDelegate().BindLambda([](UDynamicMaterialModel* InMaterialModel) -> TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface>
		{
			return TScriptInterface<IDynamicMaterialModelEditorOnlyDataInterface>(NewObject<UDynamicMaterialModelEditorOnlyData>(InMaterialModel));
		});

	BuildRequestList.Empty();

	UDMMaterialValueTexture::GetDefaultRGBTexture.BindLambda([]()
		{
			return UDynamicMaterialEditorSettings::Get()->DefaultRGBTexture.LoadSynchronous();
		});
}

void FDynamicMaterialEditorModule::ShutdownModule()
{
	FDynamicMaterialEditorCommands::Unregister();
	FDynamicMaterialEditorStyle::Shutdown();

	if (UObjectInitialized() && !IsEngineExitRequested() && FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(UDynamicMaterialModelEditorOnlyData::StaticClass()->GetFName());
	}

	using namespace UE::DynamicMaterialEditor::Private;
	FMaterialList::OnAddMaterialItemViewExtraBottomWidget.Remove(MateriaListWidgetsDelegate);

	FDMLevelEditorIntegration::Shutdown();

	BuildRequestList.Empty();

	UDMMaterialValueTexture::GetDefaultRGBTexture.Unbind();
}

void FDynamicMaterialEditorModule::SetDynamicMaterialModel(UDynamicMaterialModel* InMaterialModel, UWorld* InWorld, bool bInInvokeTab)
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (TSharedPtr<SDMEditor> Editor = FDMLevelEditorIntegration::GetEditorForWorld(InWorld))
	{
		if (bInInvokeTab)
		{
			FDMLevelEditorIntegration::InvokeTabForWorld(InWorld);
		}

		Editor->SetMaterialModel(InMaterialModel);
	}
	else if (IsValid(InWorld))
	{
		if (UDMWorldSubsystem* DMWorldSubsystem = InWorld->GetSubsystem<UDMWorldSubsystem>())
		{
			if (bInInvokeTab)
			{
				DMWorldSubsystem->GetInvokeTabDelegate().ExecuteIfBound();
			}

			DMWorldSubsystem->GetSetCustomEditorModelDelegate().ExecuteIfBound(InMaterialModel);
		}
	}
}

void FDynamicMaterialEditorModule::SetDynamicMaterialObjectProperty(const FDMObjectMaterialProperty& InObjectProperty,
	UWorld* InWorld, bool bInInvokeTab)
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (TSharedPtr<SDMEditor> Editor = FDMLevelEditorIntegration::GetEditorForWorld(InWorld))
	{
		if (bInInvokeTab)
		{
			FDMLevelEditorIntegration::InvokeTabForWorld(InWorld);
		}

		Editor->SetMaterialObjectProperty(InObjectProperty);
	}
	else if (IsValid(InWorld))
	{
		if (UDMWorldSubsystem* DMWorldSubsystem = InWorld->GetSubsystem<UDMWorldSubsystem>())
		{
			if (bInInvokeTab)
			{
				DMWorldSubsystem->GetInvokeTabDelegate().ExecuteIfBound();
			}

			DMWorldSubsystem->GetCustomObjectPropertyEditorDelegate().ExecuteIfBound(InObjectProperty);
		}
	}
}

void FDynamicMaterialEditorModule::SetDynamicMaterialInstance(UDynamicMaterialInstance* InInstance, UWorld* InWorld, 
	bool bInInvokeTab)
{
	if (IsValid(InInstance))
	{
		if (UDynamicMaterialModel* InstanceModel = InInstance->GetMaterialModel())
		{
			SetDynamicMaterialModel(InstanceModel, InWorld, bInInvokeTab);
		}
	}
}

void FDynamicMaterialEditorModule::SetDynamicMaterialActor(AActor* InActor, UWorld* InWorld, bool bInInvokeTab)
{
	using namespace UE::DynamicMaterialEditor::Private;

	if (TSharedPtr<SDMEditor> Editor = FDMLevelEditorIntegration::GetEditorForWorld(InWorld))
	{
		if (bInInvokeTab)
		{
			FDMLevelEditorIntegration::InvokeTabForWorld(InWorld);
		}

		Editor->SetMaterialActor(InActor);
	}
	else if (IsValid(InWorld))
	{
		if (UDMWorldSubsystem* DMWorldSubsystem = InWorld->GetSubsystem<UDMWorldSubsystem>())
		{
			if (bInInvokeTab)
			{
				DMWorldSubsystem->GetInvokeTabDelegate().ExecuteIfBound();
			}

			DMWorldSubsystem->GetSetCustomEditorActorDelegate().ExecuteIfBound(InActor);
		}
	}
}

void FDynamicMaterialEditorModule::ClearDynamicMaterialModel(UWorld* InWorld)
{
	SetDynamicMaterialModel(nullptr, InWorld, /* Invoke tab */ false);
}

TSharedRef<SWidget> FDynamicMaterialEditorModule::CreateEditor(UDynamicMaterialModel* InMaterialModel, UWorld* InAssetEditorWorld)
{
	TSharedRef<SDMEditor> NewEditor = SNew(SDMEditor, InMaterialModel);

	if (IsValid(InAssetEditorWorld))
	{
		UDMWorldSubsystem* WorldSubsystem = InAssetEditorWorld->GetSubsystem<UDMWorldSubsystem>();

		if (IsValid(WorldSubsystem))
		{
			WorldSubsystem->GetSetCustomEditorModelDelegate().BindSP(NewEditor, &SDMEditor::SetMaterialModel);
			WorldSubsystem->GetCustomObjectPropertyEditorDelegate().BindSP(NewEditor, &SDMEditor::SetMaterialObjectProperty);
			WorldSubsystem->GetSetCustomEditorActorDelegate().BindSP(NewEditor, &SDMEditor::SetMaterialActor);
		}
	}

	return NewEditor;
}

TSharedRef<SWidget> FDynamicMaterialEditorModule::CreateEmptyTabContent()
{
	return SDMEditor::GetEmptyContent();
}

void FDynamicMaterialEditorModule::Tick(float DeltaTime)
{
	if (UDMMaterialComponent::CanClean() == false)
	{
		return;
	}

	for (const FDMBuildRequestEntry& ToBuild : BuildRequestList)
	{
		if (UObject* Object = FindObject<UObject>(nullptr, *ToBuild.AssetPath, false))
		{
			ProcessBuildRequest(Object, ToBuild.bDirtyAssets);
		}
	}

	BuildRequestList.Empty();
}

TStatId FDynamicMaterialEditorModule::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FDynamicMaterialEditorModule, STATGROUP_Tickables);
}

void FDynamicMaterialEditorModule::AddBuildRequest(UObject* InToBuild, bool bInDirtyAssets)
{
	if (!IsValid(InToBuild))
	{
		return;
	}

	FDynamicMaterialEditorModule::Get().BuildRequestList.Add({InToBuild->GetPathName(), bInDirtyAssets});
	
	static const double VeryShortTime = 0.0001;

	// Make sure we don't spam updates on a single tick.
	UDMMaterialComponent::PreventClean(VeryShortTime);
}

void FDynamicMaterialEditorModule::OpenEditor(UWorld* InWorld)
{
	if (!IsValid(InWorld))
	{
		FDMLevelEditorIntegration::InvokeTabForWorld(InWorld);
	}
	else if (UDMWorldSubsystem* DMWorldSubsystem = InWorld->GetSubsystem<UDMWorldSubsystem>())
	{
		DMWorldSubsystem->GetInvokeTabDelegate().ExecuteIfBound();
	}
}

void FDynamicMaterialEditorModule::ProcessBuildRequest(UObject* InToBuild, bool bInDirtyAssets)
{
	if (!IsValid(InToBuild))
	{
		return;
	}

	if (InToBuild->GetClass()->ImplementsInterface(UDMBuildable::StaticClass()) == false)
	{
		return;
	}

	IDMBuildable::Execute_DoBuild(InToBuild, bInDirtyAssets);
}

void FDynamicMaterialEditorModule::MapCommands()
{
	const FDynamicMaterialEditorCommands& DMEditorCommands = FDynamicMaterialEditorCommands::Get();

	UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get();

	CommandList->MapAction(
		DMEditorCommands.OpenEditorSettingsWindow,
		FExecuteAction::CreateUObject(Settings, &UDynamicMaterialEditorSettings::OpenEditorSettingsWindow)
	);
}

void FDynamicMaterialEditorModule::UnmapCommands()
{
	const FDynamicMaterialEditorCommands& DMEditorCommands = FDynamicMaterialEditorCommands::Get();

	auto UnmapAction = [this](const TSharedPtr<const FUICommandInfo>& InCommandInfo)
	{
		if (CommandList->IsActionMapped(InCommandInfo))
		{
			CommandList->UnmapAction(InCommandInfo);
		}
	};

	UnmapAction(DMEditorCommands.OpenEditorSettingsWindow);
}

IMPLEMENT_MODULE(FDynamicMaterialEditorModule, DynamicMaterialEditor)
