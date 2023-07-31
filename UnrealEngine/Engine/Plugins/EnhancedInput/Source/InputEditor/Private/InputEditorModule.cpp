// Copyright Epic Games, Inc. All Rights Reserved.

#include "InputEditorModule.h"

#include "AssetTypeActions_Base.h"
#include "EnhancedInputModule.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformFileManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "PlayerMappableInputConfig.h"
#include "InputCustomizations.h"
#include "InputModifiers.h"
#include "ISettingsModule.h"
#include "ToolMenuSection.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_GetInputActionValue.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "SSettingsEditorCheckoutNotice.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AssetTypeActions/AssetTypeActions_DataAsset.h"
#include "EnhancedInputDeveloperSettings.h"
#include "Styling/SlateStyle.h"
#include "Styling/StyleColors.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "GameFramework/InputSettings.h"
#include "EnhancedInputComponent.h"
#include "EnhancedPlayerInput.h"
#include "Interfaces/IMainFrameModule.h"
#include "SourceControlHelpers.h"
#include "HAL/FileManager.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InputEditorModule)

#define LOCTEXT_NAMESPACE "InputEditor"

DEFINE_LOG_CATEGORY(LogEnhancedInputEditor);

EAssetTypeCategories::Type FInputEditorModule::InputAssetsCategory;

namespace UE::Input
{
	static bool bEnableAutoUpgradeToEnhancedInput = true;
	static FAutoConsoleVariableRef CVarEnableAutoUpgradeToEnhancedInput(
		TEXT("EnhancedInput.bEnableAutoUpgrade"),
		bEnableAutoUpgradeToEnhancedInput,
		TEXT("Should your project automatically be set to use Enhanced Input if it is currently using the legacy input system?"),
		ECVF_Default);
}

IMPLEMENT_MODULE(FInputEditorModule, InputEditor)

// Asset factories

// InputContext
UInputMappingContext_Factory::UInputMappingContext_Factory(const class FObjectInitializer& OBJ) : Super(OBJ) {
	SupportedClass = UInputMappingContext::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UInputMappingContext_Factory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UInputMappingContext::StaticClass()));
	UInputMappingContext* IMC = NewObject<UInputMappingContext>(InParent, Class, Name, Flags | RF_Transactional, Context);

	// Populate the IMC with some initial input actions if they were specified. This will be the case if the IMC is being created from the FAssetTypeActions_InputAction
	for (TWeakObjectPtr<UInputAction> WeakIA : InitialActions)
	{
		if (UInputAction* IA = WeakIA.Get())
		{
			IMC->MapKey(IA, FKey());
		}
	}
	
	return IMC;
}

void UInputMappingContext_Factory::SetInitialActions(TArray<TWeakObjectPtr<UInputAction>> InInitialActions)
{
	InitialActions.Empty();
	InitialActions = InInitialActions;
}

// InputAction
UInputAction_Factory::UInputAction_Factory(const class FObjectInitializer& OBJ)
	: Super(OBJ)
{
	SupportedClass = UInputAction::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UInputAction_Factory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UInputAction::StaticClass()));
	return NewObject<UInputAction>(InParent, Class, Name, Flags | RF_Transactional, Context);
}

// UPlayerMappableInputConfig_Factory
UPlayerMappableInputConfig_Factory::UPlayerMappableInputConfig_Factory(const class FObjectInitializer& OBJ)
	: Super(OBJ)
{
	SupportedClass = UPlayerMappableInputConfig::StaticClass();
	bEditAfterNew = true;
	bCreateNew = true;
}

UObject* UPlayerMappableInputConfig_Factory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn)
{
	check(Class->IsChildOf(UPlayerMappableInputConfig::StaticClass()));
	return NewObject<UPlayerMappableInputConfig>(InParent, Class, Name, Flags | RF_Transactional, Context);
}

// Asset type actions

class FAssetTypeActions_InputContext : public FAssetTypeActions_DataAsset
{
public:
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_InputMappingContext", "Input Mapping Context"); }
	virtual uint32 GetCategories() override { return FInputEditorModule::GetInputAssetsCategory(); }
	virtual FColor GetTypeColor() const override { return FColor(255, 255, 127); }
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_InputContextDesc", "A collection of device input to action mappings."); }
	virtual UClass* GetSupportedClass() const override { return UInputMappingContext::StaticClass(); }
};

class FAssetTypeActions_InputAction : public FAssetTypeActions_DataAsset
{
public:
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_InputAction", "Input Action"); }
	virtual uint32 GetCategories() override { return FInputEditorModule::GetInputAssetsCategory(); }
	virtual FColor GetTypeColor() const override { return FColor(127, 255, 255); }
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_InputActionDesc", "Represents an abstract game action that can be mapped to arbitrary hardware input devices."); }
	virtual UClass* GetSupportedClass() const override { return UInputAction::StaticClass(); }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;

private:

	void GetCreateContextFromActionsMenu(TArray<TWeakObjectPtr<UInputAction>> InActions);
	
};

void FAssetTypeActions_InputAction::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	TArray<TWeakObjectPtr<UInputAction>> InputActions = GetTypedWeakObjectPtrs<UInputAction>(InObjects);
	
	 Section.AddMenuEntry(
	 	"InputAction_CreateContextFromSelection",	
	 	LOCTEXT("InputAction_CreateContextFromSelection", "Create an Input Mapping Context"),
	 	LOCTEXT("InputAction_CreateContextFromSelectionTooltip", "Create an Input Mapping Context that is filled with the selected Input Actions"),
	 	FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.InputMappingContext"),
	 	FUIAction(FExecuteAction::CreateSP(this, &FAssetTypeActions_InputAction::GetCreateContextFromActionsMenu, InputActions))
	 );
}

void FAssetTypeActions_InputAction::GetCreateContextFromActionsMenu(TArray<TWeakObjectPtr<UInputAction>> InActions)
{
	if (InActions.IsEmpty())
	{
		return;
	}

	static const FString MappingContextPrefix(TEXT("IMC_"));
	static const FString ActionPrefixToIgnore(TEXT("IA_"));
	
	if (UInputAction* FirstIA = InActions[0].Get())
	{
		FString EffectiveIMCName = FirstIA->GetName();
		EffectiveIMCName.RemoveFromStart(ActionPrefixToIgnore);
		// Have the IMC_ prefix at the front of the new asset
		EffectiveIMCName.InsertAt(0, MappingContextPrefix);
		
		const FString ActionPathName = FirstIA->GetOutermost()->GetPathName();
    	const FString LongPackagePath = FPackageName::GetLongPackagePath(ActionPathName);
		const FString NewIMCDefaultPath = LongPackagePath / EffectiveIMCName;

		FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		
		// Make sure the name is unique
		FString AssetName;
		FString DefaultSuffix;
		FString PackageName;
		AssetToolsModule.Get().CreateUniqueAssetName(NewIMCDefaultPath, DefaultSuffix, /*out*/ PackageName, /*out*/ AssetName);
		const FString PackagePath = FPackageName::GetLongPackagePath(PackageName);

		// Create the new IMC
		UInputMappingContext_Factory* IMCFactory = NewObject<UInputMappingContext_Factory>();
		IMCFactory->SetInitialActions(InActions);
		ContentBrowserModule.Get().CreateNewAsset(AssetName, PackagePath, UInputMappingContext::StaticClass(), IMCFactory);
	}
}

class FAssetTypeActions_PlayerMappableInputConfig : public FAssetTypeActions_DataAsset
{
public:
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PlayerMappableInputConfig", "Player Mappable Input Config"); }
	virtual uint32 GetCategories() override { return FInputEditorModule::GetInputAssetsCategory(); }
	virtual FColor GetTypeColor() const override { return FColor(127, 255, 255); }
	virtual FText GetAssetDescription(const FAssetData& AssetData) const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_PlayerBindableInputConfigDesc", "Represents one set of Player Mappable controller/keymappings"); }
	virtual UClass* GetSupportedClass() const override { return UPlayerMappableInputConfig::StaticClass(); }
};

/** Custom style set for Enhanced Input */
class FEnhancedInputSlateStyle final : public FSlateStyleSet
{
public:
	FEnhancedInputSlateStyle()
		: FSlateStyleSet("EnhancedInputEditor")
	{
		SetParentStyleName(FAppStyle::GetAppStyleSetName());

		// The icons are located in /Engine/Plugins/EnhancedInput/Content/Editor/Slate/Icons
		SetContentRoot(FPaths::EnginePluginsDir() / TEXT("EnhancedInput/Content/Editor/Slate"));
		SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

		// Enhanced Input Editor icons
		static const FVector2D Icon16 = FVector2D(16.0f, 16.0f);
		static const FVector2D Icon64 = FVector2D(64.0f, 64.0f);

		Set("EnhancedInputIcon_Small", new IMAGE_BRUSH_SVG("Icons/EnhancedInput_16", Icon16));
		Set("EnhancedInputIcon_Large", new IMAGE_BRUSH_SVG("Icons/EnhancedInput_64", Icon64));
		
		Set("ClassIcon.InputAction", new IMAGE_BRUSH_SVG("Icons/InputAction_16", Icon16));
		Set("ClassThumbnail.InputAction", new IMAGE_BRUSH_SVG("Icons/InputAction_64", Icon64));
		
		Set("ClassIcon.InputMappingContext", new IMAGE_BRUSH_SVG("Icons/InputMappingContext_16", Icon16));
		Set("ClassThumbnail.InputMappingContext", new IMAGE_BRUSH_SVG("Icons/InputMappingContext_64", Icon64));
		
		Set("ClassIcon.PlayerMappableInputConfig", new IMAGE_BRUSH_SVG("Icons/PlayerMappableInputConfig_16", Icon16));
		Set("ClassThumbnail.PlayerMappableInputConfig", new IMAGE_BRUSH_SVG("Icons/PlayerMappableInputConfig_64", Icon64));			
	}
};

void FInputEditorModule::StartupModule()
{
	// Register customizations
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.RegisterCustomClassLayout("InputMappingContext", FOnGetDetailCustomizationInstance::CreateStatic(&FInputContextDetails::MakeInstance));
	PropertyModule.RegisterCustomPropertyTypeLayout("EnhancedActionKeyMapping", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FEnhancedActionMappingCustomization::MakeInstance));
	PropertyModule.RegisterCustomClassLayout(UEnhancedInputDeveloperSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FEnhancedInputDeveloperSettingsCustomization::MakeInstance));
	PropertyModule.NotifyCustomizationModuleChanged();

	// Register input assets
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
	InputAssetsCategory = AssetTools.RegisterAdvancedAssetCategory(FName(TEXT("Input")), LOCTEXT("InputAssetsCategory", "Input"));
	{
		RegisterAssetTypeActions(AssetTools, MakeShareable(new FAssetTypeActions_InputAction));
		RegisterAssetTypeActions(AssetTools, MakeShareable(new FAssetTypeActions_InputContext));
		RegisterAssetTypeActions(AssetTools, MakeShareable(new FAssetTypeActions_PlayerMappableInputConfig));
		// TODO: Build these off a button on the InputContext Trigger/Mapping pickers? Would be good to have both.
		//RegisterAssetTypeActions(AssetTools, MakeShareable(new FAssetTypeActions_InputTrigger));
		//RegisterAssetTypeActions(AssetTools, MakeShareable(new FAssetTypeActions_InputModifier));
	}

	// Make a new style set for Enhanced Input, which will register any custom icons for the types in this plugin
	StyleSet = MakeShared<FEnhancedInputSlateStyle>();
	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());

	// Listen for when the editor is ready and then try to upgrade the input classes. We will send a slate notification
	// if we upgrade the input classes, which is why we need to wait for the editor to be ready
    IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
    MainFrameModule.OnMainFrameCreationFinished().AddRaw(this, &FInputEditorModule::OnMainFrameCreationFinished);
}

void FInputEditorModule::ShutdownModule()
{
	// Unregister input assets
	if (FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools"))
	{
		for (TSharedPtr<IAssetTypeActions>& AssetAction : CreatedAssetTypeActions)
		{
			AssetToolsModule->Get().UnregisterAssetTypeActions(AssetAction.ToSharedRef());
		}
	}
	CreatedAssetTypeActions.Empty();

	// Unregister input settings
	if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
	{
		SettingsModule->UnregisterSettings("Project", "Engine", "Enhanced Input");
	}

	// Unregister customizations
	FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyModule.UnregisterCustomClassLayout("InputContext");
	PropertyModule.UnregisterCustomPropertyTypeLayout("EnhancedActionKeyMapping");
	PropertyModule.UnregisterCustomClassLayout("EnhancedInputDeveloperSettings");
	PropertyModule.NotifyCustomizationModuleChanged();

	// Unregister slate stylings
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
	}

	// Remove any listeners to the editor startup delegate
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule::Get().OnMainFrameCreationFinished().RemoveAll(this);
	}
}

void FInputEditorModule::OnMainFrameCreationFinished(TSharedPtr<SWindow> InRootWindow, bool bIsNewProjectWindow)
{
	AutoUpgradeDefaultInputClasses();
}

namespace UE::Input
{
	static void ShowToast(const FText& ClassBeingChanged, const FString& NewClassName, const FString& OldClassName)
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ClassBeingChanged"), ClassBeingChanged);
		Args.Add(TEXT("NewClass"), FText::FromString(NewClassName));
		Args.Add(TEXT("OldClass"), FText::FromString(OldClassName));

		const FText Message = FText::Format(LOCTEXT("EnhancedInputUpgradeToast", "Upgrading default {ClassBeingChanged} class from '{OldClass}' to '{NewClass}'"), Args);
		
		FNotificationInfo* Info = new FNotificationInfo(Message);
		Info->ExpireDuration = 5.0f;
		Info->bFireAndForget = true;
		FSlateNotificationManager::Get().QueueNotification(Info);
		
		UE_LOG(LogEnhancedInputEditor, Log, TEXT("Upgrading Default %s class from '%s' to '%s'"), *ClassBeingChanged.ToString(), *OldClassName, *NewClassName);
	}
}

void FInputEditorModule::AutoUpgradeDefaultInputClasses()
{
	if (!UE::Input::bEnableAutoUpgradeToEnhancedInput)
	{
		return;
	}
	
	if (UInputSettings* InputSettings = GetMutableDefault<UInputSettings>())
	{
		const UClass* OriginalInputComponentClass = InputSettings->GetDefaultInputComponentClass();
		const UClass* OriginalPlayerInputClass = InputSettings->GetDefaultPlayerInputClass();
		bool bNeedsConfigSave = false;
		
		if (OriginalInputComponentClass && OriginalInputComponentClass == UInputComponent::StaticClass())
		{
			InputSettings->SetDefaultInputComponentClass(UEnhancedInputComponent::StaticClass());

			static const FText ClassName = LOCTEXT("InputComponentClassLabel", "Input Component");
			UE::Input::ShowToast(ClassName, UEnhancedInputComponent::StaticClass()->GetName(), OriginalInputComponentClass->GetName());
			bNeedsConfigSave = true;
		}

		if (OriginalPlayerInputClass && OriginalPlayerInputClass == UPlayerInput::StaticClass())
		{
			InputSettings->SetDefaultPlayerInputClass(UEnhancedPlayerInput::StaticClass());

			static const FText ClassName = LOCTEXT("PlayerInputClassLabel", "Player Input");
			UE::Input::ShowToast(ClassName, UEnhancedPlayerInput::StaticClass()->GetName(), OriginalPlayerInputClass->GetName());
			bNeedsConfigSave = true;
		}

		// Make sure that the config file gets updated with these new values
		if (bNeedsConfigSave)
		{
			const FString DefaultConfigFile = InputSettings->GetDefaultConfigFilename();
			
			// We can write to the file if it is not read only. If it is read only, then we can write to it if we successfully check it out with source control
			bool bCanWriteToFile = !IFileManager::Get().IsReadOnly(*DefaultConfigFile) || (USourceControlHelpers::IsEnabled() && USourceControlHelpers::CheckOutFile(DefaultConfigFile));
			
			if (bCanWriteToFile)
			{
				InputSettings->TryUpdateDefaultConfigFile(DefaultConfigFile, /* bWarnIfFail */ false);
			}
		}
	}
}

void FInputEditorModule::Tick(float DeltaTime)
{
	// Update any blueprints that are referencing an input action with a modified value type
	if (UInputAction::ActionsWithModifiedValueTypes.Num())
	{
		TSet<UBlueprint*> BPsModified;
		for (TObjectIterator<UK2Node_EnhancedInputAction> NodeIt; NodeIt; ++NodeIt)
		{
			if (UInputAction::ActionsWithModifiedValueTypes.Contains(NodeIt->InputAction))
			{
				NodeIt->ReconstructNode();
				BPsModified.Emplace(NodeIt->GetBlueprint());
			}
		}
		for (TObjectIterator<UK2Node_GetInputActionValue> NodeIt; NodeIt; ++NodeIt)
		{
			if (UInputAction::ActionsWithModifiedValueTypes.Contains(NodeIt->InputAction))
			{
				NodeIt->ReconstructNode();
				BPsModified.Emplace(NodeIt->GetBlueprint());
			}
		}

		if (BPsModified.Num())
		{
			FNotificationInfo Info(FText::Format(LOCTEXT("ActionValueTypeChange", "Changing action value type affected {0} blueprint(s)!"), BPsModified.Num()));
			Info.ExpireDuration = 5.0f;
			FSlateNotificationManager::Get().AddNotification(Info);
		}

		UInputAction::ActionsWithModifiedValueTypes.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
