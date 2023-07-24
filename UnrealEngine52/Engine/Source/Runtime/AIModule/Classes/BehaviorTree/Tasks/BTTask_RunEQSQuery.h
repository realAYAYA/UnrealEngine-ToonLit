// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "BehaviorTree/Tasks/BTTask_BlackboardBase.h"
#include "BTTask_RunEQSQuery.generated.h"

class UBehaviorTree;
class UEnvQuery;

struct FBTEnvQueryTaskMemory
{
	/** Query request ID */
	int32 RequestID;
};

/**
 * Run Environment Query System Query task node.
 * Runs the specified environment query when executed.
 */
UCLASS()
class AIMODULE_API UBTTask_RunEQSQuery : public UBTTask_BlackboardBase
{
	GENERATED_UCLASS_BODY()

	/** query to run */
	UE_DEPRECATED_FORGAME(5.0, "UBTTask_RunEQSQuery.QueryTemplate has been deprecated for a long while now. Will be removed in the next engine version.")
	UPROPERTY()
	TObjectPtr<UEnvQuery> QueryTemplate;

	/** optional parameters for query */
	UE_DEPRECATED_FORGAME(5.0, "UBTTask_RunEQSQuery.QueryParams has been deprecated for a long while now. Will be removed in the next engine version.")
	UPROPERTY()
	TArray<FEnvNamedValue> QueryParams;

	UE_DEPRECATED_FORGAME(5.0, "UBTTask_RunEQSQuery.QueryConfig has been deprecated for a long while now. Will be removed in the next engine version.")
	UPROPERTY()
	TArray<FAIDynamicParam> QueryConfig;

	/** determines which item will be stored (All = only first matching) */
	UE_DEPRECATED_FORGAME(5.0, "UBTTask_RunEQSQuery.RunMode has been deprecated for a long while now. Will be removed in the next engine version.")
	UPROPERTY()
	TEnumAsByte<EEnvQueryRunMode::Type> RunMode;

	/** blackboard key storing an EQS query template */
	UE_DEPRECATED_FORGAME(5.0, "UBTTask_RunEQSQuery.EQSQueryBlackboardKey been deprecated for a long while now. Will be removed in the next engine version.")
	UPROPERTY()
	struct FBlackboardKeySelector EQSQueryBlackboardKey;

	UPROPERTY(EditAnywhere, Category=Node, meta=(InlineEditConditionToggle))
	bool bUseBBKey;

	UPROPERTY(Category = EQS, EditAnywhere)
	FEQSParametrizedQueryExecutionRequest EQSRequest;

	UPROPERTY(Category = EQS, EditAnywhere)
	bool bUpdateBBOnFail = false;

	FQueryFinishedSignature QueryFinishedDelegate;

	virtual void InitializeFromAsset(UBehaviorTree& Asset) override;
	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;
	virtual uint16 GetInstanceMemorySize() const override;

	/** finish task */
	void OnQueryFinished(TSharedPtr<FEnvQueryResult> Result);

	/** Convert QueryParams to QueryConfig */
	virtual void PostLoad() override;

#if WITH_EDITOR
	/** prepare query params */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FName GetNodeIconName() const override;
#endif

protected:

	/** gather all filters from existing EnvQueryItemTypes */
	void CollectKeyFilters();
};
