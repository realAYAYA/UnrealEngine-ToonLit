// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ModularRig.h"
#include "ModularRigRuleManager.generated.h"

// A management class to validate rules and pattern match
UCLASS(BlueprintType)
class CONTROLRIG_API UModularRigRuleManager : public UObject
{
public:

	GENERATED_BODY()

	/***
	 * Returns the possible targets for the given connector in the current resolve stage
	 * Note: This method is thread-safe. 
	 * @param InConnector The connector to resolve
	 * @param InModule The module the connector belongs to
	 * @param InResolvedConnectors A redirect map of the already resolved connectors
	 * @return The resolve result including a list of matches
	 */
	FModularRigResolveResult FindMatches(
		const FRigConnectorElement* InConnector,
		const FRigModuleInstance* InModule,
		const FRigElementKeyRedirector& InResolvedConnectors = FRigElementKeyRedirector()
	) const;

	/***
	 * Returns the possible targets for the given external module connector in the current resolve stage
	 * Note: This method is thread-safe. 
	 * @param InConnector The connector to resolve
	 * @return The resolve result including a list of matches
	 */
	FModularRigResolveResult FindMatches(
		const FRigModuleConnector* InConnector
	) const;

	/***
	 * Returns the possible targets for the primary connector in the current resolve stage
	 * Note: This method is thread-safe. 
	 * @param InModule The module the connector belongs to
	 * @return The resolve result including a list of matches
	 */
	FModularRigResolveResult FindMatchesForPrimaryConnector(
		const FRigModuleInstance* InModule
	) const;

	/***
	 * Returns the possible targets for each secondary connector
	 * Note: This method is thread-safe. 
	 * @param InModule The module the secondary connectors belongs to
	 * @param InResolvedConnectors A redirect map of the already resolved connectors
	 * @return The resolve result including a list of matches for each connector
	 */
	TArray<FModularRigResolveResult> FindMatchesForSecondaryConnectors(
		const FRigModuleInstance* InModule,
		const FRigElementKeyRedirector& InResolvedConnectors = FRigElementKeyRedirector()
	) const;

	/***
	 * Returns the possible targets for each optional connector
	 * Note: This method is thread-safe. 
	 * @param InModule The module the optional connectors belongs to
	 * @param InResolvedConnectors A redirect map of the already resolved connectors
	 * @return The resolve result including a list of matches for each connector
	 */
	TArray<FModularRigResolveResult> FindMatchesForOptionalConnectors(
		const FRigModuleInstance* InModule,
		const FRigElementKeyRedirector& InResolvedConnectors = FRigElementKeyRedirector()
	) const;

private:

	struct FWorkData
	{
		FWorkData()
		: Hierarchy(nullptr)
		, Connector(nullptr)
		, ModuleConnector(nullptr)
		, Module(nullptr)
		, ResolvedConnectors(nullptr)
		, Result(nullptr)
		{
		}

		void Filter(TFunction<void(FRigElementResolveResult&)> PerMatchFunction);

		const URigHierarchy* Hierarchy;
		const FRigConnectorElement* Connector;
		const FRigModuleConnector* ModuleConnector;
		const FRigModuleInstance* Module;
		const FRigElementKeyRedirector* ResolvedConnectors;
		FModularRigResolveResult* Result;
	};

	FModularRigResolveResult FindMatches(FWorkData& InWorkData) const;

	void SetHierarchy(const URigHierarchy* InHierarchy);
	static void ResolveConnector(FWorkData& InOutWorkData);
	static void FilterIncompatibleTypes(FWorkData& InOutWorkData);
	static void FilterInvalidNameSpaces(FWorkData& InOutWorkData);
	static void FilterByConnectorRules(FWorkData& InOutWorkData);
	static void FilterByConnectorEvent(FWorkData& InOutWorkData);

	TWeakObjectPtr<const URigHierarchy> Hierarchy;

	friend class URigHierarchy;
};