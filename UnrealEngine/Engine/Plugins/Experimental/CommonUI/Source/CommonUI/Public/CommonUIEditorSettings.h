// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "Templates/SubclassOf.h"
#include "InputCoreTypes.h"
#include "Engine/StreamableManager.h"
#include "Templates/SharedPointer.h"

#include "ICommonUIModule.h"
#include "CommonUITypes.h"
#include "CommonTextBlock.h"
#include "CommonButtonBase.h"
#include "CommonBorder.h"
#include "CommonUIEditorSettings.generated.h"

UCLASS(config = Editor, defaultconfig)
class COMMONUI_API UCommonUIEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UCommonUIEditorSettings(const FObjectInitializer& Initializer);
#if WITH_EDITOR
	/* Called to load CommonUIEditorSettings data */
	void LoadData();

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	
	/*  Template Styles - Only accessible in editor builds should be transfered to new widgets in OnCreationFromPalette() overrides */
	const TSubclassOf<UCommonTextStyle>& GetTemplateTextStyle() const;

	const TSubclassOf<UCommonButtonStyle>& GetTemplateButtonStyle() const;

	const TSubclassOf<UCommonBorderStyle>& GetTemplateBorderStyle() const;

private:
	void LoadEditorData();
#endif

private:
	/** Newly created CommonText Widgets will use this style. */
	UPROPERTY(config, EditAnywhere, Category = "Text")
	TSoftClassPtr<UCommonTextStyle> TemplateTextStyle;

	/** Newly created CommonButton Widgets will use this style. */
	UPROPERTY(config, EditAnywhere, Category = "Buttons")
	TSoftClassPtr<UCommonButtonStyle> TemplateButtonStyle;

	/** Newly created CommonBorder Widgets will use this style. */
	UPROPERTY(config, EditAnywhere, Category = "Border")
	TSoftClassPtr<UCommonBorderStyle> TemplateBorderStyle;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TSubclassOf<UCommonTextStyle> TemplateTextStyleClass;

	UPROPERTY(Transient)
	TSubclassOf<UCommonButtonStyle> TemplateButtonStyleClass;

	UPROPERTY(Transient)
	TSubclassOf<UCommonBorderStyle> TemplateBorderStyleClass;
#endif
private:
	bool bDefaultDataLoaded;
};
