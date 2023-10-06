// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonActivatableWidget.h"
#include "Engine/DataAsset.h"

#include "XRCreativeToolset.generated.h"


class UCommonButtonStyle;
class UCommonTextStyle;
class UInputMappingContext;


UCLASS()
class UXRCreativePaletteTab : public UCommonActivatableWidget
{
	GENERATED_BODY()
};


UCLASS()
class UXRCreativePalette : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	void SetOwner(class AXRCreativeAvatar* InOwner) { Owner = InOwner; }

protected:
	UPROPERTY(BlueprintReadOnly, Category="XR Creative")
	TArray<TObjectPtr<UXRCreativePaletteTab>> Tabs;

	UPROPERTY(BlueprintReadOnly, Category="XR Creative")
	TObjectPtr<AXRCreativeAvatar> Owner;
};


//////////////////////////////////////////////////////////////////////////


UCLASS(Abstract, BlueprintType)
class UXRCreativeTool : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="XR Creative")
	virtual FName GetToolName() const PURE_VIRTUAL( UXRCreativeTool::GetToolName, return NAME_None; );

	UFUNCTION(BlueprintCallable, Category="XR Creative")
	virtual FText GetDisplayName() const PURE_VIRTUAL( UXRCreativeTool::GetDisplayName, return FText::GetEmpty(); );

	UFUNCTION(BlueprintCallable, Category="XR Creative")
	virtual TSubclassOf<class UXRCreativePaletteToolTab> GetPaletteTabClass() const PURE_VIRTUAL( UXRCreativeTool::GetPaletteTabClass, return nullptr; );
};


UCLASS(Abstract, Blueprintable)
class UXRCreativeBlueprintableTool : public UXRCreativeTool
{
	GENERATED_BODY()

public:
	virtual FName GetToolName() const override { return ToolName; }
	virtual FText GetDisplayName() const override { return DisplayName; }
	virtual TSubclassOf<UXRCreativePaletteToolTab> GetPaletteTabClass() const override { return PaletteTabClass; }

protected:
	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category="XR Creative")
	FName ToolName;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category="XR Creative")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, EditDefaultsOnly, Category="XR Creative")
	TSubclassOf<UXRCreativePaletteToolTab> PaletteTabClass;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="XR Creative")
	TSubclassOf<AActor> ToolActor;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="XR Creative")
	TObjectPtr<UInputMappingContext> ToolInputMappingContext;
	
};


UCLASS()
class UXRCreativePaletteToolTab : public UXRCreativePaletteTab
{
	GENERATED_BODY()

protected:
	UPROPERTY(BlueprintReadOnly, Category="XR Creative")
	TWeakObjectPtr<UXRCreativeTool> Tool;
};


//////////////////////////////////////////////////////////////////////////


UCLASS(BlueprintType)
class UXRCreativeStyle : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="XR Creative")
	TSubclassOf<UCommonTextStyle> RegularTextStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="XR Creative")
	TSubclassOf<UCommonTextStyle> BoldTextStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="XR Creative")
	TSubclassOf<UCommonTextStyle> ItalicTextStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="XR Creative")
	TSubclassOf<UCommonButtonStyle> RegularButtonStyle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="XR Creative")
	TSubclassOf<UCommonButtonStyle> LargeButtonStyle;
};


USTRUCT(BlueprintType)
struct FXRCreativeToolEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="XR Creative")
	TSubclassOf<UXRCreativeTool> ToolClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="XR Creative")
	TObjectPtr<UTexture2D> ToolIcon;
};


UCLASS(BlueprintType)
class XRCREATIVE_API UXRCreativeToolset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="XR Creative")
	TSubclassOf<AXRCreativeAvatar> Avatar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="XR Creative")
	TArray<FXRCreativeToolEntry> Tools;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="XR Creative")
	TSubclassOf<UXRCreativePalette> Palette;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="XR Creative")
	TSoftObjectPtr<UXRCreativeStyle> Style;
};
