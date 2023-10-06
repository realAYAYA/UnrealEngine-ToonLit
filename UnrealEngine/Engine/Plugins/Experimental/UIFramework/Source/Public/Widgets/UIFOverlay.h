// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Types/UIFSlotBase.h"
#include "UIFWidget.h"

#include "UIFOverlay.generated.h"

struct FUIFrameworkWidgetId;

class UUIFrameworkOverlay;
struct FUIFrameworkOverlaySlotList;

/**
 *
 */
USTRUCT(BlueprintType)
struct FUIFrameworkOverlaySlot : public FUIFrameworkSlotBase
{
	GENERATED_BODY()

	friend UUIFrameworkOverlay;
	friend FUIFrameworkOverlaySlotList;

	/** Distance between that surrounds the widget inside the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Framework")
	FMargin Padding = FMargin(0.f, 0.f, 0.f, 0.f);

	/** Horizontal alignment of the widget inside the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Framework")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment = EHorizontalAlignment::HAlign_Fill;

	/** Vertical alignment of the widget inside the slot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Framework")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment = EVerticalAlignment::VAlign_Fill;

private:
	/** Index in the array the Slot is. The position in the array can change when replicated. */
	UPROPERTY()
	int32 Index = INDEX_NONE;
};


/**
 *
 */
USTRUCT()
struct UIFRAMEWORK_API FUIFrameworkOverlaySlotList : public FFastArraySerializer
{
	GENERATED_BODY()

	FUIFrameworkOverlaySlotList() = default;
	FUIFrameworkOverlaySlotList(UUIFrameworkOverlay* InOwner)
		: Owner(InOwner)
	{}

public:
	//~ Begin of FFastArraySerializer
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	//~ End of FFastArraySerializer

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

	void AddEntry(FUIFrameworkOverlaySlot Entry);
	bool RemoveEntry(UUIFrameworkWidget* Widget);
	FUIFrameworkOverlaySlot* FindEntry(FUIFrameworkWidgetId WidgetId);
	void ForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func);

private:
	UPROPERTY()
	TArray<FUIFrameworkOverlaySlot> Slots;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UUIFrameworkOverlay> Owner;
};


template<>
struct TStructOpsTypeTraits<FUIFrameworkOverlaySlotList> : public TStructOpsTypeTraitsBase2<FUIFrameworkOverlaySlotList>
{
	enum { WithNetDeltaSerializer = true };
};


/**
 *
 */
UCLASS(DisplayName="Overlay UIFramework")
class UIFRAMEWORK_API UUIFrameworkOverlay : public UUIFrameworkWidget
{
	GENERATED_BODY()

	friend FUIFrameworkOverlaySlotList;

public:
	UUIFrameworkOverlay();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void AddWidget(FUIFrameworkOverlaySlot Widget);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void RemoveWidget(UUIFrameworkWidget* Widget);

	virtual void AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func) override;
	virtual void AuthorityRemoveChild(UUIFrameworkWidget* Widget) override;
	virtual void LocalAddChild(FUIFrameworkWidgetId ChildId) override;

private:
	void AddEntry(FUIFrameworkOverlaySlot Entry);
	bool RemoveEntry(UUIFrameworkWidget* Widget);
	FUIFrameworkOverlaySlot* FindEntry(FUIFrameworkWidgetId WidgetId);

	UPROPERTY(Replicated)
	FUIFrameworkOverlaySlotList ReplicatedSlotList;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Widgets/Layout/Anchors.h"
#endif
