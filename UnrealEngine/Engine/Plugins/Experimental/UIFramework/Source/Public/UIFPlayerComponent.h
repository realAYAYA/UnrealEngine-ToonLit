// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/PlayerController.h"
#include "Types/UIFSlotBase.h"
#include "Types/UIFWidgetTree.h"

#include "Types/UIFWidgetTreeOwner.h"
#include "UIFPlayerComponent.generated.h"

class UUIFrameworkPlayerComponent;
class UUIFrameworkPresenter;
class UUIFrameworkWidget;
class UWidget;
struct FStreamableHandle;

DECLARE_MULTICAST_DELEGATE(FOnPendingReplicationProcessed);

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
UENUM(BlueprintType)
enum class EUIFrameworkInputMode : uint8
{
	// Input is received by the UI.
	UI,
	// Input is received by the Game.
	Game,
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
	EUIFrameworkInputMode InputMode = EUIFrameworkInputMode::Game;

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
	void PreReplicatedRemove(const TArrayView<int32>& RemovedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	//~ End of FFastArraySerializer

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FUIFrameworkGameLayerSlot, FUIFrameworkGameLayerSlotList>(Entries, DeltaParms, *this);
	}

	void AddEntry(FUIFrameworkGameLayerSlot Entry);
	bool RemoveEntry(UUIFrameworkWidget* Layer);
	FUIFrameworkGameLayerSlot* FindEntry(FUIFrameworkWidgetId WidgetId);
	const FUIFrameworkGameLayerSlot* FindEntry(FUIFrameworkWidgetId WidgetId) const;

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
class UIFRAMEWORK_API UUIFrameworkPlayerComponent : public UActorComponent, public IUIFrameworkWidgetTreeOwner
{
	GENERATED_BODY()

	friend FUIFrameworkGameLayerSlotList;

	UUIFrameworkPlayerComponent();

public:
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void AddWidget(FUIFrameworkGameLayerSlot Widget);

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void RemoveWidget(UUIFrameworkWidget* Widget);
	
	const FUIFrameworkGameLayerSlotList& GetRootList() const
	{
		return RootList;
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
	virtual void InitializeComponent() override;
	virtual void UninitializeComponent() override;
	virtual bool ReplicateSubobjects(class UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent

	void AuthorityRemoveChild(UUIFrameworkWidget* Widget);
	FOnPendingReplicationProcessed& GetOnPendingReplicationProcessed();

	virtual FUIFrameworkWidgetTree& GetWidgetTree() override;
	virtual FUIFrameworkWidgetOwner GetWidgetOwner() const override;
	virtual void LocalWidgetWasAddedToTree(const FUIFrameworkWidgetTreeEntry& Entry) override;
	virtual void LocalWidgetRemovedFromTree(const FUIFrameworkWidgetTreeEntry& Entry) override;
	virtual void LocalRemoveWidgetRootFromTree(const UUIFrameworkWidget* Widget) override;

private:
	UFUNCTION(Server, Reliable)
	void ServerRemoveWidgetRootFromTree(FUIFrameworkWidgetId WidgetId);

	void LocalOnClassLoaded(TSoftClassPtr<UWidget> WidgetClass);
	void LocalAddChild(FUIFrameworkWidgetId WidgetId);

private:
	UPROPERTY(Replicated)
	FUIFrameworkGameLayerSlotList RootList;

	UPROPERTY(Replicated)
	FUIFrameworkWidgetTree WidgetTree;

	UPROPERTY(Transient)
	TObjectPtr<UUIFrameworkPresenter> Presenter;

	//~ Widget can be net replicated but not constructed yet.
	UPROPERTY(Transient)
	TSet<int32> NetReplicationPending;

	//~ Widgets are created and ready to be added.
	UPROPERTY(Transient)
	TSet<int32> AddPending;

	//~ Once widgets are created and constructed, allow for actions such as focus to occur
	FOnPendingReplicationProcessed OnPendingReplicationProcessed;

	struct FWidgetClassToLoad
	{
		TArray<int32, TInlineAllocator<4>> EntryReplicationIds;
		TSharedPtr<FStreamableHandle> StreamableHandle;
	};

	TMap<TSoftClassPtr<UWidget>, FWidgetClassToLoad> ClassesToLoad;

	bool bAddingWidget = false;
};
