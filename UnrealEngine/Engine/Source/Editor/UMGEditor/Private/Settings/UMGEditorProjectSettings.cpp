// Copyright Epic Games, Inc. All Rights Reserved.

#include "UMGEditorProjectSettings.h"
#include "WidgetBlueprint.h"
#include "WidgetCompilerRule.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

#include "Blueprint/UserWidget.h"
#include "Components/CanvasPanel.h"

UUMGEditorProjectSettings::UUMGEditorProjectSettings()
{
	Version = 0;
	CurrentVersion = 1;
	bShowWidgetsFromEngineContent = false;
	bShowWidgetsFromDeveloperContent = true;
	bGraphEditorHidden = false;
	bHideWidgetAnimationEditor = false;
	bUseEditorConfigPaletteFiltering = false;

	bUseWidgetTemplateSelector = false;
	DefaultRootWidget = nullptr;
	DefaultWidgetParentClass = UUserWidget::StaticClass();
}

#if WITH_EDITOR

FText UUMGEditorProjectSettings::GetSectionText() const
{
	return NSLOCTEXT("UMG", "WidgetDesignerTeamSettingsName", "Widget Designer (Team)");
}

FText UUMGEditorProjectSettings::GetSectionDescription() const
{
	return NSLOCTEXT("UMG", "WidgetDesignerTeamSettingsDescription", "Configure options for the Widget Designer that affect the whole team.");
}

#endif

bool UUMGEditorProjectSettings::CompilerOption_AllowBlueprintTick(const class UWidgetBlueprint* WidgetBlueprint) const
{
	return GetFirstCompilerOption(WidgetBlueprint, &FWidgetCompilerOptions::bAllowBlueprintTick, true);
}

bool UUMGEditorProjectSettings::CompilerOption_AllowBlueprintPaint(const class UWidgetBlueprint* WidgetBlueprint) const
{
	return GetFirstCompilerOption(WidgetBlueprint, &FWidgetCompilerOptions::bAllowBlueprintPaint, true);
}

EPropertyBindingPermissionLevel UUMGEditorProjectSettings::CompilerOption_PropertyBindingRule(const class UWidgetBlueprint* WidgetBlueprint) const
{
	return GetFirstCompilerOption(WidgetBlueprint, &FWidgetCompilerOptions::PropertyBindingRule, EPropertyBindingPermissionLevel::Allow);
}

TArray<UWidgetCompilerRule*> UUMGEditorProjectSettings::CompilerOption_Rules(const class UWidgetBlueprint* WidgetBlueprint) const
{
	TArray<UWidgetCompilerRule*> Rules;
	GetCompilerOptionsForWidget(WidgetBlueprint, [&Rules](const FWidgetCompilerOptions& Options) {
		for (const TSoftClassPtr<UWidgetCompilerRule>& RuleClassPtr : Options.Rules)
		{
			// The compiling rule may not be loaded yet in early loading phases, we'll
			// just have to skip the rules in those cases.
			RuleClassPtr.LoadSynchronous();
			if (RuleClassPtr)
			{
				if (UWidgetCompilerRule* Rule = RuleClassPtr->GetDefaultObject<UWidgetCompilerRule>())
				{
					Rules.Add(Rule);
				}
			}
		}
		return false;
	});
	return Rules;
}

FNamePermissionList& UUMGEditorProjectSettings::GetAllowedPaletteCategories()
{
	return AllowedPaletteCategories;
}

FPathPermissionList& UUMGEditorProjectSettings::GetAllowedPaletteWidgets()
{
	return AllowedPaletteWidgets;
}

void UUMGEditorProjectSettings::GetCompilerOptionsForWidget(const UWidgetBlueprint* WidgetBlueprint, TFunctionRef<bool(const FWidgetCompilerOptions&)> Operator) const
{
	FString AssetPath = WidgetBlueprint->GetOutermost()->GetName();
	FSoftObjectPath SoftObjectPath = WidgetBlueprint->GetPathName();
	
	// Don't apply the rules to the engine widgets.
	if (AssetPath.StartsWith(TEXT("/Engine")))
	{
		return;
	}

	for (int32 DirectoryIndex = DirectoryCompilerOptions.Num() - 1; DirectoryIndex >= 0; DirectoryIndex--)
	{
		const FDirectoryWidgetCompilerOptions& CompilerOptions = DirectoryCompilerOptions[DirectoryIndex];

		const FString& DirectoryPath = CompilerOptions.Directory.Path;
		if (!DirectoryPath.IsEmpty())
		{
			if (AssetPath.StartsWith(DirectoryPath))
			{
				const bool bIgnoreWidget = CompilerOptions.IgnoredWidgets.ContainsByPredicate([&SoftObjectPath](const TSoftObjectPtr<UWidgetBlueprint>& IgnoredWidget) {
					return IgnoredWidget.ToSoftObjectPath() == SoftObjectPath;
				});

				if (bIgnoreWidget)
				{
					continue;
				}

				if (Operator(CompilerOptions.Options))
				{
					return;
				}
			}
		}
	}

	Operator(DefaultCompilerOptions);
}

#if WITH_EDITOR
void UUMGEditorProjectSettings::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	DirectoryCompilerOptions.StableSort([](const FDirectoryWidgetCompilerOptions& A, const FDirectoryWidgetCompilerOptions& B) {
		return A.Directory.Path < B.Directory.Path;
	});

	// If there's a change, we should scan for widgets currently in the error or warning state and mark them as dirty
	// so they get recompiled next time we PIE.  Don't mark all widgets dirty, or we're in for a very large recompile.
	if (PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		for (TObjectIterator<UWidgetBlueprint> BlueprintIt; BlueprintIt; ++BlueprintIt)
		{
			UWidgetBlueprint* Blueprint = *BlueprintIt;
			if (Blueprint->Status == BS_Error || Blueprint->Status == BS_UpToDateWithWarnings)
			{
				Blueprint->Status = BS_Dirty;
			}
		}
	}
}
#endif

void UUMGEditorProjectSettings::PostInitProperties()
{
	Super::PostInitProperties();

	if (Version < CurrentVersion)
	{
		for (int32 FromVersion = Version + 1; FromVersion <= CurrentVersion; FromVersion++)
		{
			PerformUpgradeStepForVersion(FromVersion);
		}

		Version = CurrentVersion;
	}
}

void UUMGEditorProjectSettings::PerformUpgradeStepForVersion(int32 ForVersion)
{
}