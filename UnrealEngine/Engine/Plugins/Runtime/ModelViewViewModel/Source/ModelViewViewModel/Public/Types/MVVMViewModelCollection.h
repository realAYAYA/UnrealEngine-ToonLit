// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "Types/MVVMViewModelContext.h"
#include "Types/MVVMViewModelContextInstance.h"

#include "MVVMViewModelCollection.generated.h"

class UMVVMViewModelBase;

/** */
USTRUCT()
struct MODELVIEWVIEWMODEL_API FMVVMViewModelCollection
{
	GENERATED_BODY()

public:
	UMVVMViewModelBase* FindViewModelInstance(FMVVMViewModelContext Context) const;

	bool AddInstance(FMVVMViewModelContext Context, UMVVMViewModelBase* ViewModel);
	bool RemoveInstance(FMVVMViewModelContext Context);
	int32 RemoveAllInstances(UMVVMViewModelBase* ViewModel);

	void Reset();

	FSimpleMulticastDelegate& OnCollectionChanged()
	{
		return OnCollectionChangedDelegate;
	}

private:
	UPROPERTY()
	mutable TArray<FMVVMViewModelContextInstance> ViewModelInstances;

	FSimpleMulticastDelegate OnCollectionChangedDelegate;
};


/** */
UCLASS(meta = (DisplayName = "MVVM View Model Collection Object"))
class UMVVMViewModelCollectionObject : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "MVVM")
	UMVVMViewModelBase* FindViewModelInstance(FMVVMViewModelContext Context) const
	{
		return ViewModelCollection.FindViewModelInstance(Context);
	}
	
	UFUNCTION(BlueprintCallable, Category = "MVVM")
	bool AddViewModelInstance(FMVVMViewModelContext Context, UMVVMViewModelBase* ViewModel)
	{
		return ViewModelCollection.AddInstance(Context, ViewModel);
	}

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	bool RemoveViewModel(FMVVMViewModelContext Context)
	{
		return ViewModelCollection.RemoveInstance(Context);
	}

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	int32 RemoveAllViewModelInstance(UMVVMViewModelBase* ViewModel)
	{
		return ViewModelCollection.RemoveAllInstances(ViewModel);
	}

	FSimpleMulticastDelegate& OnCollectionChanged()
	{
		return ViewModelCollection.OnCollectionChanged();
	}

private:
	UPROPERTY(Transient)
	FMVVMViewModelCollection ViewModelCollection;
};
