// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDataSourceFilterInterface.h"

#include "DataSourceFilter.generated.h"

UCLASS(Blueprintable)
class SOURCEFILTERINGTRACE_API UDataSourceFilter : public UObject, public IDataSourceFilterInterface
{
	friend class FSourceFilterManager;
	friend class FSourceFilterSetup;

	GENERATED_BODY()
public:
	UDataSourceFilter();
	virtual ~UDataSourceFilter();

	UFUNCTION(BlueprintNativeEvent, Category = TraceSourceFiltering)
	bool DoesActorPassFilter(const AActor* InActor) const;
	virtual bool DoesActorPassFilter_Implementation(const AActor* InActor) const;

	/** Begin IDataSourceFilterInterface overrides */
	virtual void SetEnabled(bool bState) override;	
	virtual bool IsEnabled() const final;
	virtual const FDataSourceFilterConfiguration& GetConfiguration() const final;
protected:
	virtual void GetDisplayText_Internal(FText& OutDisplayText) const override;
	/** End IDataSourceFilterInterface overrides */

	virtual bool DoesActorPassFilter_Internal(const AActor* InActor) const;
protected:
	/** Filter specific settings */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Filtering)
	FDataSourceFilterConfiguration Configuration;

	/** Whether or not this filter is enabled */
	bool bIsEnabled;
};


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#endif
