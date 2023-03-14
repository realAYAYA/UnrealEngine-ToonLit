// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/ActorComponent.h"
#include "GameFramework/PlayerController.h"
#include "Types/UIFSlotBase.h"
#include "Types/UIFWidgetId.h"
#include "Types/UIFWidgetTree.h"

#include "UIFPlayerComponent.generated.h"

class UUIFrameworkPlayerComponent;
class UUIFrameworkWidget;
class UWidget;
struct FStreamableHandle;


/**
 *
 */
UENUM(BlueprintType)
enum class EUIFrameworkGameLayerType : uint8
{
	Viewport,
	PlayerScreen,
};


/**
 *
 */
USTRUCT(BlueprintType)
struct FUIFrameworkGameLayerSlot : public FUIFrameworkSlotBase
{
	GENERATED_BODY()

	FUIFrameworkGameLayerSlot() = default;

	UPROPERTY(BlueprintReadWrite, Category = "UI Framework")
	int32 ZOrder = 0;

	UPROPERTY(BlueprintReadWrite, Category = "UI Framework")
	EUIFrameworkGameLayerType Type = EUIFrameworkGameLayerType::Viewport;
};


/**
 *
 */
USTRUCT()
struct UIFRAMEWORK_API FUIFrameworkGameLayerSlotList : public FFastArraySerializer
{
	GENERATED_BODY()

	friend UUIFrameworkPlayerComponent;

	FUIFrameworkGameLayerSlotList() = default;
	FUIFrameworkGameLayerSlotList(UUIFrameworkPlayerComponent* InOwner)
		: Owner(InOwner)
	{}

public:
	//~ Begin of FFastArraySerializer
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	//~ End of FFastArraySerializer

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FUIFrameworkGameLayerSlot, FUIFrameworkGameLayerSlotList>(Entries, DeltaParms, *this);
	}

	void AddEntry(FUIFrameworkGameLayerSlot Entry);
	bool RemoveEntry(UUIFrameworkWidget* Layer);
	FUIFrameworkGameLayerSlot* FindEntry(FUIFrameworkWidgetId WidgetId);

private:
	UPROPERTY()
	TArray<FUIFrameworkGameLayerSlot> Entries;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UUIFrameworkPlayerComponent> Owner;
};

template<>
struct TStructOpsTypeTraits<FUIFrameworkGameLayerSlotList> : public TStructOpsTypeTraitsBase2<FUIFrameworkGameLayerSlotList>
{
	enum { WithNetDeltaSerializer = true };
};


/**
 * 
 */
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent))
class UIFRAMEWORK_API UUIFrameworkPlayerComponent : public UActorComponent
{
	GENERATED_BODY()

	friend FUIFrameworkGameLayerSlotList;

	UUIFrameworkPlayerComponent();

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void AddWidget(FUIFrameworkGameLayerSlot Widget);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void RemoveWidget(UUIFrameworkWidget* Widget);

	FUIFrameworkWidgetTree& GetWidgetTree()
	{
		return WidgetTree;
	}

	/** Gets the controller that owns the component, this will always be valid during gameplay but can return null in the editor */
	template <class T = APlayerController>
	T* GetPlayerController() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, APlayerController>::Value, "'T' template parameter to GetPlayerController must be derived from APlayerController");
		return Cast<T>(GetOwner());
	}

	template <class T = APlayerController>
	T* GetPlayerControllerChecked() const
	{
		static_assert(TPointerIsConvertibleFromTo<T, APlayerController>::Value, "'T' template parameter to GetPlayerControllerChecked must be derived from APlayerController");
		return CastChecked<T>(GetOwner());
	}

	//~ Begin UActorComponent
	virtual void UninitializeComponent() override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent

	void AuthorityRemoveChild(UUIFrameworkWidget* Widget);
	void LocalWidgetWasAddedToTree(const FUIFrameworkWidgetTreeEntry& Entry);
	void LocalWidgetRemovedFromTree(const FUIFrameworkWidgetTreeEntry& Entry);

private:
	void LocalOnClassLoaded(TSoftClassPtr<UWidget> WidgetClass);
	void LocalAddChild(FUIFrameworkWidgetId WidgetId);

private:
	UPROPERTY(Replicated)
	FUIFrameworkGameLayerSlotList RootList;

	UPROPERTY(Replicated)
	FUIFrameworkWidgetTree WidgetTree;

	//~ Widget can be net replicated but not constructed yet.
	UPROPERTY(Transient)
	TSet<int32> NetReplicationPending;

	//~ Widgets are created and ready to be added.
	UPROPERTY(Transient)
	TSet<int32> AddPending;

	struct FWidgetClassToLoad
	{
		TArray<int32, TInlineAllocator<4>> EntryReplicationIds;
		TSharedPtr<FStreamableHandle> StreamableHandle;
	};

	TMap<TSoftClassPtr<UWidget>, FWidgetClassToLoad> ClassesToLoad;

	bool bAddingWidget = false;
};
