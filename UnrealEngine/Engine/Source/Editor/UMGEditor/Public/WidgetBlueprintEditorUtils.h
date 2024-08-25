// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WidgetBlueprintEditor.h"
#include "WidgetBlueprint.h"
#include "WidgetReference.h"

class FDragDropOperation;
class FHittestGrid;
class FMenuBuilder;
class UWidgetBlueprint;
class UWidgetSlotPair;
class UWidgetTree;
class SWindow;
class UWidgetEditingProjectSettings;
class FWidgetObjectTextFactory;

//////////////////////////////////////////////////////////////////////////
// FWidgetBlueprintEditorUtils

class UMGEDITOR_API FWidgetBlueprintEditorUtils
{

public:
	struct FWidgetThumbnailProperties
	{
		FVector2D ScaledSize;
		FVector2D Offset;
	};

	static bool VerifyWidgetRename(TSharedRef<class FWidgetBlueprintEditor> BlueprintEditor, FWidgetReference Widget, const FText& NewName, FText& OutErrorMessage);

	static bool RenameWidget(TSharedRef<class FWidgetBlueprintEditor> BlueprintEditor, const FName& OldObjectName, const FString& NewDisplayName);

	static void ReplaceDesiredFocus(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, const FName& OldName, const FName& NewName);
	
	static void SetDesiredFocus(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, const FName DesiredFocusWidgetName);

	static void CreateWidgetContextMenu(FMenuBuilder& MenuBuilder, TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, FVector2D TargetLocation);

	static void CopyWidgets(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static TArray<UWidget*> PasteWidgets(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, FWidgetReference ParentWidget, FName SlotName, FVector2D PasteLocation);

	static void DeleteWidgets(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets, bool bSilentDelete = false);

	static void CutWidgets(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static TArray<UWidget*> DuplicateWidgets(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static bool IsAnySelectedWidgetLocked(TSet<FWidgetReference> SelectedWidgets);

	static UWidget* GetWidgetTemplateFromDragDrop(UWidgetBlueprint* Blueprint, UWidgetTree* RootWidgetTree, TSharedPtr<FDragDropOperation>& DragDropOp);

	static bool IsBindWidgetProperty(const FProperty* InProperty);
	static bool IsBindWidgetProperty(const FProperty* InProperty, bool& bIsOptional);

	static bool IsBindWidgetAnimProperty(const FProperty* InProperty);
	static bool IsBindWidgetAnimProperty(const FProperty* InProperty, bool& bIsOptional);

	struct FUsableWidgetClassResult
	{
		const UClass* NativeParentClass = nullptr;
		EClassFlags AssetClassFlags = EClassFlags::CLASS_None;
	};

	UE_DEPRECATED(5.3, "This function has been deprecated. Use the version of IsUsableWidgetClass that takes a second argument of TSharedRef<FWidgetBlueprintEditor>.")
	static bool IsUsableWidgetClass(const UClass* WidgetClass);
	UE_DEPRECATED(5.3, "This function has been deprecated. Use the version of IsUsableWidgetClass that takes a second argument of TSharedRef<FWidgetBlueprintEditor>.")
	static TValueOrError<FUsableWidgetClassResult, void> IsUsableWidgetClass(const FAssetData& WidgetAsset);

	static bool IsUsableWidgetClass(const UClass* WidgetClass, TSharedRef<FWidgetBlueprintEditor> InCurrentActiveBlueprintEditor);
	static TValueOrError<FUsableWidgetClassResult, void> IsUsableWidgetClass(const FAssetData& WidgetAsset, TSharedRef<FWidgetBlueprintEditor> InCurrentActiveBlueprintEditor);

	static void ExportWidgetsToText(TArray<UWidget*> WidgetsToExport, /*out*/ FString& ExportedText);

	static void ImportWidgetsFromText(UWidgetBlueprint* BP, const FString& TextToImport, /*out*/ TSet<UWidget*>& ImportedWidgetSet, /*out*/ TMap<FName, UWidgetSlotPair*>& PastedExtraSlotData);

	/** Exports the individual properties of an object to text and stores them in a map. */
	static void ExportPropertiesToText(UObject* Object, TMap<FName, FString>& ExportedProperties);

	/** Attempts to import any property in the map and apply it to a property with the same name on the object. */
	static void ImportPropertiesFromText(UObject* Object, const TMap<FName, FString>& ExportedProperties);

	static bool DoesClipboardTextContainWidget(UWidgetBlueprint* BP);

	static TScriptInterface<INamedSlotInterface> FindNamedSlotHostForContent(UWidget* WidgetTemplate, UWidgetTree* WidgetTree);

	static UWidget* FindNamedSlotHostWidgetForContent(UWidget* WidgetTemplate, UWidgetTree* WidgetTree);

	static void FindAllAncestorNamedSlotHostWidgetsForContent(TArray<FWidgetReference>& OutSlotHostWidgets, UWidget* WidgetTemplate, TSharedRef<FWidgetBlueprintEditor> BlueprintEditor);

	static bool RemoveNamedSlotHostContent(UWidget* WidgetTemplate, TScriptInterface<INamedSlotInterface> NamedSlotHost);

	static bool ReplaceNamedSlotHostContent(UWidget* WidgetTemplate, TScriptInterface<INamedSlotInterface> NamedSlotHost, UWidget* NewContentWidget);

	static UWidgetTree* FindLatestWidgetTree(UWidgetBlueprint* Blueprint, UUserWidget* UserWidget);

	static int32 UpdateHittestGrid(FHittestGrid& HitTestGrid, TSharedRef<SWindow> Window, float Scale, FVector2D DrawSize, float DeltaTime);

	static TTuple<FVector2D, FVector2D> GetWidgetPreviewAreaAndSize(UUserWidget* UserWidget, FVector2D DesiredSize, FVector2D PreviewSize, EDesignPreviewSizeMode ThumbnailSizeMode, TOptional<FVector2D> ThumbnailCustomSize);

	static float GetWidgetPreviewDPIScale(UUserWidget* UserWidget, FVector2D PreviewSize);

	static EDesignPreviewSizeMode ConvertThumbnailSizeModeToDesignerSizeMode(EThumbnailPreviewSizeMode ThumbnailSizeMode, UUserWidget* WidgetInstance);

	static FVector2D GetWidgetPreviewUnScaledCustomSize(FVector2D DesiredSize, UUserWidget* UserWidget, TOptional<FVector2D> ThumbnailCustomSize, EThumbnailPreviewSizeMode ThumbnailSizeMode = EThumbnailPreviewSizeMode::MatchDesignerMode);

	static TTuple<float, FVector2D> GetThumbnailImageScaleAndOffset(FVector2D WidgetSize, FVector2D ThumbnailSize);

	static void SetTextureAsAssetThumbnail(UWidgetBlueprint* WidgetBlueprint, UTexture2D* ThumbnailTexture);

	static FText GetPaletteCategory(const TSubclassOf<UWidget> Widget);
	static FText GetPaletteCategory(const FAssetData& WidgetAsset, const TSubclassOf<UWidget> NativeClass);

	static TOptional<FWidgetThumbnailProperties> DrawSWidgetInRenderTargetForThumbnail(UUserWidget* WidgetInstance, FRenderTarget* RenderTarget2D, FVector2D ThumbnailSize, TOptional<FVector2D> ThumbnailCustomSize, EThumbnailPreviewSizeMode ThumbnailSizeMode = EThumbnailPreviewSizeMode::MatchDesignerMode);

	static TOptional<FWidgetThumbnailProperties> DrawSWidgetInRenderTargetForThumbnail(UUserWidget* WidgetInstance, UTextureRenderTarget2D* RenderTarget2D, FVector2D ThumbnailSize, TOptional<FVector2D> ThumbnailCustomSize, EThumbnailPreviewSizeMode ThumbnailSizeMode);

	static TOptional<FWidgetThumbnailProperties> DrawSWidgetInRenderTarget(UUserWidget* WidgetInstance, UTextureRenderTarget2D* RenderTarget2D);

	static UWidgetEditingProjectSettings* GetRelevantMutableSettings(TWeakPtr<FWidgetBlueprintEditor> CurrentEditor);
	static const UWidgetEditingProjectSettings* GetRelevantSettings(TWeakPtr<FWidgetBlueprintEditor> CurrentEditor);

private:

	static FString CopyWidgetsInternal(UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static TArray<UWidget*> PasteWidgetsInternal(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, const FString& TextToImport, FWidgetReference ParentWidget, FName SlotName, FVector2D PasteLocation, bool bForceSibling, bool& TransactionSuccesful);

	static bool DisplayPasteWarningAndEarlyExit();

	static void ExecuteOpenSelectedWidgetsForEdit( TSet<FWidgetReference> SelectedWidgets );

	static bool FindAndRemoveNamedSlotContent(UWidget* WidgetTemplate, UWidgetTree* WidgetTree);

	static bool CanOpenSelectedWidgetsForEdit( TSet<FWidgetReference> SelectedWidgets );

	static void BuildWrapWithMenu(FMenuBuilder& Menu, TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static void WrapWidgets(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets, UClass* WidgetClass);

	static void BuildReplaceWithMenu(FMenuBuilder& Menu, TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets);

	static void ReplaceWidgetWithSelectedTemplate(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, FWidgetReference Widget);

	static bool CanBeReplacedWithTemplate(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, FWidgetReference Widget);

	static void ReplaceWidgetWithChildren(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, FWidgetReference Widget);

	static void ReplaceWidgetWithNamedSlot(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, FWidgetReference Widget, FName NamedSlot);

	static void ReplaceWidgets(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidgetBlueprint* BP, TSet<FWidgetReference> Widgets, UClass* WidgetClass);

	static FString FindNextValidName(UWidgetTree* WidgetTree, const FString& Name);

	static void FindUsedVariablesForWidgets(const TSet<FWidgetReference>& Widgets, const UWidgetBlueprint* BP, TArray<UWidget*>& UsedVariables, TArray<FText>& WidgetNames, bool bIncludeVariablesOnChildren);

	static bool ShouldContinueDeleteOperation(UWidgetBlueprint* BP, const TArray<FText>& WidgetNames);

	static bool ShouldContinueReplaceOperation(UWidgetBlueprint* BP, const TArray<FText>& WidgetNames);	

	static TOptional<FWidgetThumbnailProperties> DrawSWidgetInRenderTargetInternal(UUserWidget* WidgetInstance, FRenderTarget* RenderTarget2D, UTextureRenderTarget2D* TextureRenderTarget,FVector2D ThumbnailSize, bool bIsForThumbnail, TOptional<FVector2D> ThumbnailCustomSize, EThumbnailPreviewSizeMode ThumbnailSizeMode);
	
	static bool IsDesiredFocusWiget(TSharedRef<FWidgetBlueprintEditor> BlueprintEditor, UWidget* Widget);

	static FWidgetObjectTextFactory ProcessImportedText(UWidgetBlueprint* BP, const FString& TextToImport, /*out*/ UPackage*& TempPackage);
};
