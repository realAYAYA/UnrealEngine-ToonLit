// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/SortedMap.h"

#include "ToolMenuContext.generated.h"

struct FUIAction;
class FUICommandInfo;
class FUICommandList;
class FTabManager;
class FExtender;

UCLASS(BlueprintType, Abstract)
class TOOLMENUS_API UToolMenuContextBase : public UObject
{
	GENERATED_BODY()
};

UCLASS()
class TOOLMENUS_API USlateTabManagerContext : public UToolMenuContextBase
{
	GENERATED_BODY()
public:

	TWeakPtr<FTabManager> TabManager;
};

USTRUCT(BlueprintType)
struct TOOLMENUS_API FToolMenuContext
{
	GENERATED_BODY()
public:

	using FContextObjectCleanup = TFunction<void(UObject*)>;
	using FContextCleanup = TFunction<void()>;

	FToolMenuContext() = default;
	FToolMenuContext(UObject* InContext);
	FToolMenuContext(UObject* InContext, FContextObjectCleanup&& InCleanup);
	FToolMenuContext(TSharedPtr<FUICommandList> InCommandList, TSharedPtr<FExtender> InExtender = TSharedPtr<FExtender>(), UObject* InContext = nullptr);

	template <typename TContextType>
	TContextType* FindContext() const
	{
		for (UObject* Object : ContextObjects)
		{
			if (TContextType* Result = Cast<TContextType>(Object))
			{
				return Result;
			}
		}

		return nullptr;
	}

	template <typename TContextType>
	UE_DEPRECATED(4.27, "Find is deprecated. Use the FindContext instead.")
	TContextType* Find() const
	{
		return FindContext<TContextType>();
	}

	UObject* FindByClass(UClass* InClass) const;

	void AppendCommandList(const TSharedRef<FUICommandList>& InCommandList);
	void AppendCommandList(const TSharedPtr<FUICommandList>& InCommandList);
	const FUIAction* GetActionForCommand(TSharedPtr<const FUICommandInfo> Command, TSharedPtr<const FUICommandList>& OutCommandList) const;
	const FUIAction* GetActionForCommand(TSharedPtr<const FUICommandInfo> Command) const;

	void AddExtender(const TSharedPtr<FExtender>& InExtender);
	TSharedPtr<FExtender> GetAllExtenders() const;
	void ResetExtenders();

	void AppendObjects(const TArray<UObject*>& InObjects);
	void AddObject(UObject* InObject);
	void AddObject(UObject* InObject, FContextObjectCleanup&& InCleanup);

	void AddCleanup(FContextCleanup&& InCleanup);

	void CleanupObjects();

	friend class UToolMenus;
	friend class UToolMenu;
	friend struct FToolMenuEntry;

	bool IsEditing() const { return bIsEditing; }
	void SetIsEditing(bool InIsEditing) { bIsEditing = InIsEditing; }

private:

	void Empty();

	bool bIsEditing = false;

	UPROPERTY()
	TArray<TObjectPtr<UObject>> ContextObjects;

	TSortedMap<TObjectPtr<UObject>, FContextObjectCleanup> ContextObjectCleanupFuncs;

	TArray<FContextCleanup> ContextCleanupFuncs;

	TArray<TSharedPtr<FUICommandList>> CommandLists;

	TSharedPtr<FUICommandList> CommandList;

	TArray<TSharedPtr<FExtender>> Extenders;
};
