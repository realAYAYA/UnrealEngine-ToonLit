// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "Templates/SharedPointer.h"

#include "ICommonUIModule.h"
#include "CommonUITypes.h"
#include "CommonUIRichTextData.h"
#include "CommonTextBlock.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "CommonUISettings.generated.h"

class UMaterial;

COMMONUI_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_PlatformTrait_PlayInEditor);

UCLASS(config = Game, defaultconfig)
class COMMONUI_API UCommonUISettings : public UObject
{
	GENERATED_BODY()

public:
	UCommonUISettings(const FObjectInitializer& Initializer = FObjectInitializer::Get());
	UCommonUISettings(FVTableHelper& Helper);
	~UCommonUISettings();

	// Called to load CommonUISetting data, if bAutoLoadData if set to false then game code must call LoadData().
	void LoadData();

	//~UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	virtual void PostReloadConfig(FProperty* PropertyThatWasLoaded) override;
	virtual void PostInitProperties() override;
	//~End of UObject interface

	// Called by the module startup to auto load CommonUISetting data if bAutoLoadData is true.
	void AutoLoadData();

	UCommonUIRichTextData* GetRichTextData() const;
	const FSlateBrush& GetDefaultThrobberBrush() const;
	UObject* GetDefaultImageResourceObject() const;
	const FGameplayTagContainer& GetPlatformTraits() const;

private:

	/** Controls if the data referenced is automatically loaded.
	 *  If False then game code must call LoadData() on it's own.
	 */
	UPROPERTY(config, EditAnywhere, Category = "Default")
	bool bAutoLoadData;

	/** The Default Image Resource, newly created CommonImage Widgets will use this style. */
	UPROPERTY(config, EditAnywhere, Category = "Image", meta = (AllowedClasses = "/Script/Engine.Texture2D,/Script/Engine.MaterialInterface"))
	TSoftObjectPtr<UObject> DefaultImageResourceObject;

	/** The Default Throbber Material, newly created CommonLoadGuard Widget will use this style. */
	UPROPERTY(config, EditAnywhere, Category = "Throbber")
	TSoftObjectPtr<UMaterialInterface> DefaultThrobberMaterial;

	/** The Default Data for rich text to show inline icon and others. */
	UPROPERTY(config, EditAnywhere, Category = "RichText", meta=(AllowAbstract=false))
	TSoftClassPtr<UCommonUIRichTextData> DefaultRichTextDataClass;

	/** The set of traits defined per-platform (e.g., the default input mode, whether or not you can exit the application, etc...) */
	UPROPERTY(config, EditAnywhere, Category = "Visibility", meta=(Categories="Platform.Trait", ConfigHierarchyEditable))
	TArray<FGameplayTag> PlatformTraits;

private:
	void LoadEditorData();
	void RebuildTraitContainer();

	bool bDefaultDataLoaded;

	// Merged version of PlatformTraits
	// This is not the config property because there is no direct ini inheritance for structs
	// (even ones like tag containers that represent a set), unlike arrays
	FGameplayTagContainer PlatformTraitContainer;

	UPROPERTY(Transient)
	TObjectPtr<UObject> DefaultImageResourceObjectInstance;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DefaultThrobberMaterialInstance;

	UPROPERTY(Transient)
	FSlateBrush DefaultThrobberBrush;

	UPROPERTY(Transient)
	TObjectPtr<UCommonUIRichTextData> RichTextDataInstance;
};