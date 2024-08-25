// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Providers/IAdvancedRenamerProvider.h"
#include "Containers/Array.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class FString;
class UObject;

class FAdvancedRenamerObjectProvider : public IAdvancedRenamerProvider
{
public:
	FAdvancedRenamerObjectProvider();
	virtual ~FAdvancedRenamerObjectProvider() override;

	void SetObjectList(const TArray<TWeakObjectPtr<UObject>>& InObjectList);
	void AddObjectList(const TArray<TWeakObjectPtr<UObject>>& InObjectList);
	void AddObjectData(UObject* InObject);
	UObject* GetObject(int32 Index) const;

protected:
	//~ Begin IAdvancedRenamerProvider
	virtual int32 Num() const override;
	virtual bool IsValidIndex(int32 Index) const override;
	virtual uint32 GetHash(int32 Index) const override;;
	virtual FString GetOriginalName(int32 Index) const override;
	virtual bool RemoveIndex(int32 Index) override;
	virtual bool CanRename(int32 Index) const override;
	virtual bool ExecuteRename(int32 Index, const FString& NewName) override;
	//~ End IAdvancedRenamerProvider

	TArray<TWeakObjectPtr<UObject>> ObjectList;
};
