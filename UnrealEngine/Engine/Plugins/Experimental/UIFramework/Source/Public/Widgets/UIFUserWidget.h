// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "INotifyFieldValueChanged.h"
#include "UIFWidget.h"
#include "Types/UIFSlotBase.h"

#include "UIFUserWidget.generated.h"

struct FUIFrameworkWidgetId;

class UUserWidget;
class UUIFrameworkUserWidget;

/**
 *
 */
USTRUCT()
struct FUIFrameworkUserWidgetNamedSlot : public FUIFrameworkSlotBase
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
struct UIFRAMEWORK_API FUIFrameworkUserWidgetNamedSlotList : public FFastArraySerializer
{
	GENERATED_BODY()

	FUIFrameworkUserWidgetNamedSlotList() = default;
	FUIFrameworkUserWidgetNamedSlotList(UUIFrameworkUserWidget* InOwner)
		: Owner(InOwner)
	{}

public:
	//~ Begin of FFastArraySerializer
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	//~ End of FFastArraySerializer

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

	void AuthorityAddEntry(FUIFrameworkUserWidgetNamedSlot Entry);
	bool AuthorityRemoveEntry(UUIFrameworkWidget* Widget);
	FUIFrameworkUserWidgetNamedSlot* FindEntry(FUIFrameworkWidgetId WidgetId);
	const FUIFrameworkUserWidgetNamedSlot* AuthorityFindEntry(FName SlotName) const;
	void AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func);

private:
	UPROPERTY()
	TArray<FUIFrameworkUserWidgetNamedSlot> Slots;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UUIFrameworkUserWidget> Owner;
};


template<>
struct TStructOpsTypeTraits<FUIFrameworkUserWidgetNamedSlotList> : public TStructOpsTypeTraitsBase2<FUIFrameworkUserWidgetNamedSlotList>
{
	enum { WithNetDeltaSerializer = true };
};

/**
*
*/
USTRUCT()
struct UIFRAMEWORK_API FUIFrameworkUserWidgetViewmodel : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FUIFrameworkUserWidgetViewmodel() = default;

public:
	UPROPERTY()
	FName Name;

	UPROPERTY()
	TObjectPtr<UObject> Instance;
};

/**
*
*/
USTRUCT()
struct UIFRAMEWORK_API FUIFrameworkUserWidgetViewmodelList : public FFastArraySerializer
{
	GENERATED_BODY()

	FUIFrameworkUserWidgetViewmodelList() = default;
	FUIFrameworkUserWidgetViewmodelList(UUIFrameworkUserWidget* InOwner)
		: Owner(InOwner)
	{}

public:
	//~ Begin of FFastArraySerializer
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	//~ End of FFastArraySerializer

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms);

	void AuthorityAddEntry(FUIFrameworkUserWidgetViewmodel Entry);
	const FUIFrameworkUserWidgetViewmodel* AuthorityFindEntry(FName ViewmodelName) const;
	void AttachViewmodels();

private:
	UPROPERTY()
	TArray<FUIFrameworkUserWidgetViewmodel> Viewmodels;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UUIFrameworkUserWidget> Owner;
};


template<>
struct TStructOpsTypeTraits<FUIFrameworkUserWidgetViewmodelList> : public TStructOpsTypeTraitsBase2<FUIFrameworkUserWidgetViewmodelList>
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

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	UUIFrameworkWidget* GetNamedSlot(FName SlotName) const;

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetViewmodel(FName ViewmodelName, TScriptInterface<INotifyFieldValueChanged> Viewmodel);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	TScriptInterface<INotifyFieldValueChanged> GetViewmodel(FName ViewmodelName) const;

public:
	void LocalOnUMGWidgetCreated() override;
	virtual bool LocalIsReplicationReady() const;

	virtual void AuthorityForEachChildren(const TFunctionRef<void(UUIFrameworkWidget*)>& Func) override;
	virtual void AuthorityRemoveChild(UUIFrameworkWidget* Widget) override;
	virtual void LocalAddChild(FUIFrameworkWidgetId ChildId) override;

private:
	UPROPERTY(Replicated)
	FUIFrameworkUserWidgetNamedSlotList ReplicatedNamedSlotList;

	UPROPERTY(Replicated)
	FUIFrameworkUserWidgetViewmodelList ReplicatedViewmodelList;
};
