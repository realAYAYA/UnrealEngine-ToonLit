// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DataSourceFilter.h"
#include "IDataSourceFilterSetInterface.h"

#include "DataSourceFilterSet.generated.h"

/** Engine implementation of IDataSourceFilterSetInterface */
UCLASS(NotBlueprintable)
class SOURCEFILTERINGTRACE_API UDataSourceFilterSet : public UDataSourceFilter, public IDataSourceFilterSetInterface
{
	friend class USourceFilterCollection;
	friend class FSourceFilterManager;
	friend class FSourceFilterSetup;

	GENERATED_BODY()
public:	
	const TArray<TObjectPtr<UDataSourceFilter>>& GetFilters() const { return Filters; }
	void SetFilterMode(EFilterSetMode InMode);

	/** Begin IDataSourceFilterSetInterface overrides */
	virtual EFilterSetMode GetFilterSetMode() const override;
	/** Begin IDataSourceFilterSetInterface overrides */

	/** Begin UDataSourceFilter overrides */
	virtual void SetEnabled(bool bState) override;
protected:
	virtual bool DoesActorPassFilter_Internal(const AActor* InActor) const override;
	virtual void GetDisplayText_Internal(FText& OutDisplayText) const override;
	/** End UDataSourceFilter overrides */

protected:
	/** Contained Filter instance */
	UPROPERTY()
	TArray<TObjectPtr<UDataSourceFilter>> Filters;

	/** Current Filter set operation */
	UPROPERTY()
	EFilterSetMode Mode;
};
