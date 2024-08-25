// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Customizations/OperatorStackEditorStackCustomization.h"
#include "ActorModifierCoreEditorStackCustomization.generated.h"

enum class EItemDropZone;
class UActorModifierCoreStack;
class UActorModifierCoreBase;
class UToolMenu;

/** Struct used for copy pasting modifiers in clipboard */
USTRUCT()
struct FActorModifierCoreEditorPropertiesWrapper
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FName ModifierName;

	UPROPERTY()
	TMap<FName, FString> PropertiesHandlesAsStringMap;
};

/** Modifier customization for stack tab */
UCLASS()
class UActorModifierCoreEditorStackCustomization : public UOperatorStackEditorStackCustomization
{
	GENERATED_BODY()

public:
	static inline const FString PropertiesWrapperPrefix = TEXT("ActorModifierCoreEditorPropertiesWrapper");
	static inline const FString PropertiesWrapperEntry  = TEXT("PropertiesWrapper");

	UActorModifierCoreEditorStackCustomization();

	//~ Begin UOperatorStackEditorStackCustomization
	virtual bool TransformContextItem(const FOperatorStackEditorItemPtr& InItem, TArray<FOperatorStackEditorItemPtr>& OutTransformedItems) const override;
	virtual void CustomizeStackHeader(const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorHeaderBuilder& InHeaderBuilder) override;
	virtual void CustomizeItemHeader(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorHeaderBuilder& InHeaderBuilder) override;
	virtual void CustomizeItemBody(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorBodyBuilder& InBodyBuilder) override;
	virtual void CustomizeItemFooter(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorFooterBuilder& InFooterBuilder) override;
	virtual bool OnIsItemDraggable(const FOperatorStackEditorItemPtr& InDragItem) override;
	virtual TOptional<EItemDropZone> OnItemCanAcceptDrop(const TArray<FOperatorStackEditorItemPtr>& InDraggedItems, const FOperatorStackEditorItemPtr& InDropZoneItem, EItemDropZone InZone) override;
	virtual void OnDropItem(const TArray<FOperatorStackEditorItemPtr>& InDraggedItems, const FOperatorStackEditorItemPtr& InDropZoneItem, EItemDropZone InZone) override;
	//~ UOperatorStackEditorStackCustomization

protected:
	/** Populates the header menu of the whole customization stack */
	void FillStackHeaderMenu(UToolMenu* InToolMenu) const;

	/** Populates the header action menu for items */
	void FillItemHeaderActionMenu(UToolMenu* InToolMenu) const;

	/** Populates the context action menu for item */
	void FillItemContextActionMenu(UToolMenu* InToolMenu) const;

	/** Remove modifier action */
	bool CanRemoveModifier(UActorModifierCoreBase* InModifier) const;
	void RemoveModifierAction(UActorModifierCoreBase* InModifier) const;

	/** Copy modifier action */
	bool CanCopyModifier(UActorModifierCoreBase* InModifier) const;
	void CopyModifierAction(UActorModifierCoreBase* InModifier) const;

	/** Paste modifier action */
	bool CanPasteModifier(UActorModifierCoreBase* InModifier) const;
	void PasteModifierAction(UActorModifierCoreBase* InModifier) const;

	/** Toggle the profiling for a whole stack */
	bool IsModifierProfiling(UActorModifierCoreStack* InStack) const;
	void ToggleModifierProfilingAction(UActorModifierCoreStack* InStack) const;

	TSharedRef<FUICommandList> CreateModifierCommands(UActorModifierCoreBase* InModifier);

	bool CreatePropertiesHandlesMapFromModifier(UActorModifierCoreBase* InModifier, TMap<FName, FString>& OutModifierPropertiesHandlesMap) const;
	bool GetModifierPropertiesWrapperFromClipboard(FActorModifierCoreEditorPropertiesWrapper& OutPropertiesWrapper) const;
	bool UpdateModifierFromPropertiesHandlesMap(UActorModifierCoreBase* InModifier, const TMap<FName, FString>& InModifierPropertiesHandlesMap) const;
	bool AddModifierFromClipboard(TSet<AActor*>& InActors) const;
};