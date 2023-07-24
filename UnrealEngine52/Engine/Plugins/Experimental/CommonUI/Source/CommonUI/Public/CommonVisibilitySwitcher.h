// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/Overlay.h"

#include "CommonVisibilitySwitcher.generated.h"

/**
 * Basic switcher that toggles visibility on its children to only show one widget at a time. Activates visible widget if possible.
 */
UCLASS(meta = (DisableNativeTick))
class COMMONUI_API UCommonVisibilitySwitcher : public UOverlay
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = CommonVisibilitySwitcher)
	void SetActiveWidgetIndex(int32 Index);

	UFUNCTION(BlueprintCallable, Category = CommonVisibilitySwitcher)
	int32 GetActiveWidgetIndex() const { return ActiveWidgetIndex; }

	UFUNCTION(BlueprintCallable, Category = CommonVisibilitySwitcher)
	UWidget* GetActiveWidget() const;

	UFUNCTION(BlueprintCallable, Category = CommonVisibilitySwitcher)
	void SetActiveWidget(const UWidget* Widget);

	UFUNCTION(BlueprintCallable, Category = CommonVisibilitySwitcher)
	void IncrementActiveWidgetIndex(bool bAllowWrapping = true);

	UFUNCTION(BlueprintCallable, Category = CommonVisibilitySwitcher)
	void DecrementActiveWidgetIndex(bool bAllowWrapping = true);

	UFUNCTION(BlueprintCallable, Category = CommonVisibilitySwitcher)
	void ActivateVisibleSlot();

	UFUNCTION(BlueprintCallable, Category = CommonVisibilitySwitcher)
	void DeactivateVisibleSlot();

	UWidget* GetWidgetAtIndex(int32 Index) const;

	virtual void SynchronizeProperties() override;

	void MoveChild(int32 CurrentIdx, int32 NewIdx);

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnActiveWidgetIndexChanged, int32)
	FOnActiveWidgetIndexChanged& OnActiveWidgetIndexChanged() const { return OnActiveWidgetIndexChangedEvent; }

#if WITH_EDITOR
public:

	virtual const FText GetPaletteCategory() override;
	virtual void OnDescendantSelectedByDesigner(UWidget* DescendantWidget) override;
	virtual void OnDescendantDeselectedByDesigner(UWidget* DescendantWidget) override;

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

protected:

	virtual void ValidateCompiledDefaults(class IWidgetCompilerLog& CompileLog) const override;

private:

	int32 DesignTime_ActiveIndex = INDEX_NONE;
#endif

protected:

	virtual void OnWidgetRebuilt() override;
	virtual UClass* GetSlotClass() const override;
	virtual void OnSlotAdded(UPanelSlot* InSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* InSlot) override;

protected:

	virtual void SetActiveWidgetIndex_Internal(int32 Index, bool bBroadcastChange = true);
	void ResetSlotVisibilities();

	UPROPERTY(EditAnywhere, Category = CommonVisibilitySwitcher)
	ESlateVisibility ShownVisibility = ESlateVisibility::SelfHitTestInvisible;

	UPROPERTY(EditAnywhere, Category = CommonVisibilitySwitcher, meta = (ClampMin = -1))
	int32 ActiveWidgetIndex = 0;

	// Whether or not to automatically activate a slot when it becomes visible
	UPROPERTY(EditAnywhere, Category = CommonVisibilitySwitcher)
	bool bAutoActivateSlot = true;

	// Whether or not to activate the first slot if one is added dynamically
	UPROPERTY(EditAnywhere, Category = CommonVisibilitySwitcher)
	bool bActivateFirstSlotOnAdding = false;

	mutable FOnActiveWidgetIndexChanged OnActiveWidgetIndexChangedEvent;
};
