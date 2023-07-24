// Copyright Epic Games, Inc. All Rights Reserved.

#include "Templates/WidgetTemplateBlueprintClass.h"

#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "WidgetBlueprint.h"


#include "Kismet2/BlueprintEditorUtils.h"
#include "Styling/SlateIconFinder.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Editor.h"

#define LOCTEXT_NAMESPACE "UMGEditor"

FWidgetTemplateBlueprintClass::FWidgetTemplateBlueprintClass(const FAssetData& InWidgetAssetData, TSubclassOf<UUserWidget> InUserWidgetClass, bool bInIsBlueprintGeneratedClass)
	: FWidgetTemplateClass(InWidgetAssetData, InUserWidgetClass)
	, bIsBlueprintGeneratedClass(bInIsBlueprintGeneratedClass)
{
	// Blueprints get the class type actions for their parent native class - this avoids us having to load the blueprint
	if (bIsBlueprintGeneratedClass && !InUserWidgetClass && InWidgetAssetData.IsValid())
	{
		FString ParentClassName;
		if (!WidgetAssetData.GetTagValue(FBlueprintTags::NativeParentClassPath, ParentClassName))
		{
			WidgetAssetData.GetTagValue(FBlueprintTags::ParentClassPath, ParentClassName);
		}
		if (!ParentClassName.IsEmpty())
		{
			CachedParentClass = UClass::TryFindTypeSlow<UClass>(FPackageName::ExportTextPathToObjectPath(ParentClassName));
		}
	}
}

FWidgetTemplateBlueprintClass::~FWidgetTemplateBlueprintClass()
{
}

FText FWidgetTemplateBlueprintClass::GetCategory() const
{
	if ( WidgetClass.Get() )
	{
		UUserWidget* DefaultUserWidget = WidgetClass->GetDefaultObject<UUserWidget>();
		return DefaultUserWidget->GetPaletteCategory();
	}
	else
	{
		//If the blueprint is unloaded we need to extract it from the asset metadata.
		FText FoundPaletteCategoryText = WidgetAssetData.GetTagValueRef<FText>(GET_MEMBER_NAME_CHECKED(UWidgetBlueprint, PaletteCategory));
		if (!FoundPaletteCategoryText.IsEmpty())
		{
			return FoundPaletteCategoryText;
		}
		else if (CachedParentClass.IsValid() && CachedParentClass->IsChildOf(UWidget::StaticClass()) && !CachedParentClass->IsChildOf(UUserWidget::StaticClass()))
		{
			return CachedParentClass->GetDefaultObject<UWidget>()->GetPaletteCategory();
		}
		else
		{
			auto DefaultUserWidget = UUserWidget::StaticClass()->GetDefaultObject<UUserWidget>();
			return DefaultUserWidget->GetPaletteCategory();
		}
	}
}

UWidget* FWidgetTemplateBlueprintClass::Create(UWidgetTree* Tree)
{
	// Load the blueprint asset or blueprint generated class if needed
	if (!WidgetClass.Get())
	{
		FString AssetPath = WidgetAssetData.GetObjectPathString();

		if (bIsBlueprintGeneratedClass)
		{
			WidgetClass = LoadObject<UBlueprintGeneratedClass>(nullptr, *AssetPath);
		}
		else if (UBlueprint* LoadedWidget = LoadObject<UBlueprint>(nullptr, *AssetPath))
		{
			WidgetClass = *LoadedWidget->GeneratedClass;
 		}
	}

	return FWidgetTemplateClass::CreateNamed(Tree, FName(*FBlueprintEditorUtils::GetClassNameWithoutSuffix(WidgetClass.Get())));
}

const FSlateBrush* FWidgetTemplateBlueprintClass::GetIcon() const
{
	if (CachedParentClass.IsValid() && CachedParentClass->IsChildOf(UWidget::StaticClass()) && !CachedParentClass->IsChildOf(UUserWidget::StaticClass()))
	{
		return FSlateIconFinder::FindIconBrushForClass(CachedParentClass.Get());
	}
	else
	{
		return FSlateIconFinder::FindIconBrushForClass(UUserWidget::StaticClass());
	}
	
}

TSharedRef<IToolTip> FWidgetTemplateBlueprintClass::GetToolTip() const
{
	FText Description;

	FString DescriptionStr = WidgetAssetData.GetTagValueRef<FString>( GET_MEMBER_NAME_CHECKED( UBlueprint, BlueprintDescription ) );
	if ( !DescriptionStr.IsEmpty() )
	{
		DescriptionStr.ReplaceInline( TEXT( "\\n" ), TEXT( "\n" ) );
		Description = FText::FromString( MoveTemp(DescriptionStr) );
	}
	else
	{
		Description = Name;
	}

	return IDocumentation::Get()->CreateToolTip( Description, nullptr, FString( TEXT( "Shared/Types/" ) ) + Name.ToString(), TEXT( "Class" ) );
}

FReply FWidgetTemplateBlueprintClass::OnDoubleClicked()
{
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset( WidgetAssetData.GetAsset() );
	return FReply::Handled();
}

bool FWidgetTemplateBlueprintClass::Supports(UClass* InClass)
{
	return InClass != nullptr && InClass->IsChildOf(UWidgetBlueprint::StaticClass());
}

#undef LOCTEXT_NAMESPACE
