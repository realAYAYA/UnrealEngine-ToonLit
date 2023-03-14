// Copyright Epic Games, Inc. All Rights Reserved.

#include "DefaultPluginWizardDefinition.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "PluginStyle.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/App.h"
#include "PluginBrowserModule.h"

#define LOCTEXT_NAMESPACE "NewPluginWizard"

FDefaultPluginWizardDefinition::FDefaultPluginWizardDefinition(bool bContentOnlyProject)
	: bIsContentOnlyProject(bContentOnlyProject)
{
	PluginBaseDir = IPluginManager::Get().FindPlugin(TEXT("PluginBrowser"))->GetBaseDir();

	PopulateTemplatesSource();
}

void FDefaultPluginWizardDefinition::PopulateTemplatesSource()
{
	const FText BlankTemplateName = LOCTEXT("BlankLabel", "Blank");
	const FText ContentOnlyTemplateName = LOCTEXT("ContentOnlyLabel", "Content Only");
	const FText BasicTemplateName = LOCTEXT("BasicTemplateTabLabel", "Editor Toolbar Button");
	const FText AdvancedTemplateName = LOCTEXT("AdvancedTemplateTabLabel", "Editor Standalone Window");
	const FText BlueprintLibTemplateName = LOCTEXT("BlueprintLibTemplateLabel", "Blueprint Library");
	const FText EditorModeTemplateName = LOCTEXT("EditorModeTemplateLabel", "Editor Mode");
	const FText ThirdPartyTemplateName = LOCTEXT("ThirdPartyTemplateLabel", "Third Party Library");

	const FText BlankDescription = LOCTEXT("BlankTemplateDesc", "Create a blank plugin with a minimal amount of code.\n\nChoose this if you want to set everything up from scratch or are making a non-visual plugin.\nA plugin created with this template will appear in the Editor's plugin list but will not register any buttons or menu entries.");
	const FText ContentOnlyDescription = LOCTEXT("ContentOnlyTemplateDesc", "Create a blank plugin that can only contain content.");
	const FText BasicDescription = LOCTEXT("BasicTemplateDesc", "Create a plugin that will add a button to the toolbar in the Level Editor.\n\nStart by implementing something in the created \"OnButtonClick\" event.");
	const FText AdvancedDescription = LOCTEXT("AdvancedTemplateDesc", "Create a plugin that will add a button to the toolbar in the Level Editor that summons an empty standalone tab window when clicked.");
	const FText BlueprintLibDescription = LOCTEXT("BPLibTemplateDesc", "Create a plugin that will contain Blueprint Function Library.\n\nChoose this if you want to create static blueprint nodes.");
	const FText EditorModeDescription = LOCTEXT("EditorModeDesc", "Create a plugin that will have an editor mode.\n\nThis will include a toolkit example to specify UI that will appear in \"Modes\" tab (next to Foliage, Landscape etc).\nIt will also include very basic UI that demonstrates editor interaction and undo/redo functions usage.");
	const FText ThirdPartyDescription = LOCTEXT("ThirdPartyDesc", "Create a plugin that uses an included third party library.\n\nThis can be used as an example of how to include, load and use a third party library yourself.");

	TSharedRef<FPluginTemplateDescription> ContentOnlyTemplate = MakeShareable(new FPluginTemplateDescription(ContentOnlyTemplateName, ContentOnlyDescription, PluginBaseDir / TEXT("Templates") / TEXT("ContentOnly"), true, EHostType::Runtime));
	ContentOnlyTemplate->SortPriority = 1;
	TemplateDefinitions.Add(ContentOnlyTemplate);

	if (!bIsContentOnlyProject)
	{
		// Insert the blank template to make sure it appears before the content only template.

		TSharedRef<FPluginTemplateDescription> BlankTemplate = MakeShareable(new FPluginTemplateDescription(BlankTemplateName, BlankDescription, PluginBaseDir / TEXT("Templates") / TEXT("Blank"), true, EHostType::Runtime));
		BlankTemplate->SortPriority = 2;
		TemplateDefinitions.Add(BlankTemplate);

		TemplateDefinitions.Add(MakeShareable(new FPluginTemplateDescription(BlueprintLibTemplateName, BlueprintLibDescription, PluginBaseDir / TEXT("Templates") / TEXT("BlueprintLibrary"), true, EHostType::Runtime, ELoadingPhase::PreLoadingScreen)));
		TemplateDefinitions.Add(MakeShareable(new FPluginTemplateDescription(BasicTemplateName, BasicDescription, PluginBaseDir / TEXT("Templates") / TEXT("Basic"), false, EHostType::Editor)));
		TemplateDefinitions.Add(MakeShareable(new FPluginTemplateDescription(AdvancedTemplateName, AdvancedDescription, PluginBaseDir / TEXT("Templates") / TEXT("Advanced"), false, EHostType::Editor)));
		TemplateDefinitions.Add(MakeShareable(new FPluginTemplateDescription(EditorModeTemplateName, EditorModeDescription, PluginBaseDir / TEXT("Templates") / TEXT("EditorMode"), false, EHostType::Editor)));
		TemplateDefinitions.Add(MakeShareable(new FPluginTemplateDescription(ThirdPartyTemplateName, ThirdPartyDescription, PluginBaseDir / TEXT("Templates") / TEXT("ThirdPartyLibrary"), true, EHostType::Runtime)));
	}

	// Add external templates that came from the modular feature interface (e.g., from another plugin like Game Features)
	TemplateDefinitions.Append(FPluginBrowserModule::Get().GetAddedPluginTemplates());

	// Don't show the option to make an engine plugin in installed builds
	const bool bAllowEnginePlugins = !FApp::IsEngineInstalled();
	for (const TSharedRef<FPluginTemplateDescription>& Template : TemplateDefinitions)
	{
		Template->bCanBePlacedInEngine = Template->bCanBePlacedInEngine && bAllowEnginePlugins;
	}

	TemplateDefinitions.Sort([](const TSharedRef<FPluginTemplateDescription>& A, const TSharedRef<FPluginTemplateDescription>& B)
	{
		if (A->SortPriority != B->SortPriority)
		{
			return A->SortPriority > B->SortPriority;
		}
		else
		{
			return A->Name.CompareTo(B->Name) <= 0;
		}
	});
}

const TArray<TSharedRef<FPluginTemplateDescription>>& FDefaultPluginWizardDefinition::GetTemplatesSource() const
{
	return TemplateDefinitions;
}


void FDefaultPluginWizardDefinition::OnTemplateSelectionChanged(TSharedPtr<FPluginTemplateDescription> InSelectedItem, ESelectInfo::Type SelectInfo)
{
	CurrentTemplateDefinition = InSelectedItem;
}

TSharedPtr<FPluginTemplateDescription> FDefaultPluginWizardDefinition::GetSelectedTemplate() const
{
	return CurrentTemplateDefinition;
}

bool FDefaultPluginWizardDefinition::HasValidTemplateSelection() const
{
	return CurrentTemplateDefinition.IsValid();
}

void FDefaultPluginWizardDefinition::ClearTemplateSelection()
{
	CurrentTemplateDefinition.Reset();
}

bool FDefaultPluginWizardDefinition::HasModules() const
{
	FString SourceFolderPath = GetPluginFolderPath() / TEXT("Source");
	
	return FPaths::DirectoryExists(SourceFolderPath);
}

bool FDefaultPluginWizardDefinition::IsMod() const
{
	return false;
}

FText FDefaultPluginWizardDefinition::GetInstructions() const
{
	return LOCTEXT("ChoosePluginTemplate", "Choose a template and then specify a name to create a new plugin.");
}

bool FDefaultPluginWizardDefinition::GetPluginIconPath(FString& OutIconPath) const
{
	return GetTemplateIconPath(CurrentTemplateDefinition.ToSharedRef(), OutIconPath);
}

EHostType::Type FDefaultPluginWizardDefinition::GetPluginModuleDescriptor() const
{
	EHostType::Type ModuleDescriptorType = EHostType::Runtime;

	if (CurrentTemplateDefinition.IsValid())
	{
		ModuleDescriptorType = CurrentTemplateDefinition->ModuleDescriptorType;
	}

	return ModuleDescriptorType;
}

ELoadingPhase::Type FDefaultPluginWizardDefinition::GetPluginLoadingPhase() const
{
	ELoadingPhase::Type Phase = ELoadingPhase::Default;

	if (CurrentTemplateDefinition.IsValid())
	{
		Phase = CurrentTemplateDefinition->LoadingPhase;
	}

	return Phase;
}

bool FDefaultPluginWizardDefinition::GetTemplateIconPath(TSharedRef<FPluginTemplateDescription> Template, FString& OutIconPath) const
{
	bool bRequiresDefaultIcon = false;

	FString TemplateFolderName = GetFolderForTemplate(Template);

	OutIconPath = TemplateFolderName / TEXT("Resources/Icon128.png");
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*OutIconPath))
	{
		OutIconPath = PluginBaseDir / TEXT("Resources/DefaultIcon128.png");
		bRequiresDefaultIcon = true;
	}

	return bRequiresDefaultIcon;
}

TArray<FString> FDefaultPluginWizardDefinition::GetFoldersForSelection() const
{
	TArray<FString> SelectedFolders;

	if (CurrentTemplateDefinition.IsValid())
	{
		SelectedFolders.Add(GetFolderForTemplate(CurrentTemplateDefinition.ToSharedRef()));
	}

	return SelectedFolders;
}

void FDefaultPluginWizardDefinition::PluginCreated(const FString& PluginName, bool bWasSuccessful) const
{
}

FString FDefaultPluginWizardDefinition::GetPluginFolderPath() const
{
	return GetFolderForTemplate(CurrentTemplateDefinition.ToSharedRef());
}

FString FDefaultPluginWizardDefinition::GetFolderForTemplate(TSharedRef<FPluginTemplateDescription> InTemplate) const
{
	return InTemplate->OnDiskPath;
}

#undef LOCTEXT_NAMESPACE