// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UIFWidget.h"
#include "Types/UIFSlotBase.h"

#include "UIFUserWidget.generated.h"

struct FUIFrameworkWidgetId;

class UUserWidget;
class UUIFrameworkUserWidget;

/**
 *
 */
USTRUCT(BlueprintType)
struct FUIFrameworkUserWidgetSlot : public FUIFrameworkSlotBase
{
	GENERATED_BODY()

	/** The name of the NamedSlot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UI Framework")
	FName SlotName;
};

/**
 *
 */
USTRUCT()
struct UIFRAMEWORK_API FUIFrameworkUserWidgetSlotList : public FFastArraySerializer
{
	GENERATED_BODY()

	FUIFrameworkUserWidgetSlotList() = default;
	FUIFrameworkUserWidgetSlotList(UUIFrameworkUserWidget* InOwner)
		: Owner(InOwner)
	{}

public:
	//~ Begin of FFastArraySerializer
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	//~ End of FFastArraySerializer

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

	void AddEntry(FUIFrameworkUserWidgetSlot Entry);
	bool RemoveEntry(UUIFrameworkWidget* Widget);
	FUIFrameworkUserWidgetSlot* FindEntry(FUIFrameworkWidgetId WidgetId);
	void ForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func);

private:
	UPROPERTY()
	TArray<FUIFrameworkUserWidgetSlot> Slots;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UUIFrameworkUserWidget> Owner;
};


template<>
struct TStructOpsTypeTraits<FUIFrameworkUserWidgetSlotList> : public TStructOpsTypeTraitsBase2<FUIFrameworkUserWidgetSlotList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
 *
 */
UCLASS(DisplayName = "UserWidget UIFramework")
class UIFRAMEWORK_API UUIFrameworkUserWidget : public UUIFrameworkWidget
{
	GENERATED_BODY()

public:
	UUIFrameworkUserWidget();

public:

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetWidgetClass(TSoftClassPtr<UWidget> Value);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetNamedSlot(FName SlotName, UUIFrameworkWidget* Widget);

public:
	void LocalOnUMGWidgetCreated() override;
	virtual bool LocalIsReplicationReady() const;

	virtual void AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func) override;
	virtual void AuthorityRemoveChild(UUIFrameworkWidget* Widget) override;
	virtual void LocalAddChild(FUIFrameworkWidgetId ChildId) override;

private:
	void AddEntry(FUIFrameworkUserWidgetSlot Entry);
	bool RemoveEntry(UUIFrameworkWidget* Widget);
	FUIFrameworkUserWidgetSlot* FindEntry(FUIFrameworkWidgetId WidgetId);

private:
	UPROPERTY(Replicated)
	FUIFrameworkUserWidgetSlotList ReplicatedSlotList;
};
