// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "ProxyTable.h"
#include "Templates/SubclassOf.h"
#include "ProxyTableFunctionLibrary.generated.h"

/**
 * Proxy Table Function Library
 */
UCLASS()
class PROXYTABLE_API UProxyTableFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
		* Temporary backwards compatibility function!  please switch to EvaluateProxyAsset
		*
		* @param ContextObject			(in) An Object from which the parameters to the Chooser Table will be read
		* @param ProxyTable				(in) The ProxyTable asset
		* @param Key					(in) The Key from the ProxyTable asset
	*/
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe, BlueprintInternalUseOnly="true"), Category = "Animation")
	static UObject* EvaluateProxyTable(const UObject* ContextObject, const UProxyTable* ProxyTable, FName Key);
	
	/**
	* Resolve a proxy asset and return the selected UObject, or null
	*
	* @param ContextObject			(in) An Object from which the Proxy Table will be read, and parameters to any nested Chooser Tables that need to evaluate
	* @param ProxyAsset				(in) The ProxyAsset asset
	* @param ObjectClass			(in) Expected type of result objects
	*/
	UFUNCTION(BlueprintPure, meta = (BlueprintThreadSafe, DeterminesOutputType = "ObjectClass", BlueprintInternalUseOnly="true"), Category = "Animation")
	static UObject* EvaluateProxyAsset(const UObject* ContextObject, const UProxyAsset* Proxy, TSubclassOf<UObject> ObjectClass);
	
	/**
	* Create a LookupProxy struct
	*
	* @param Chooser				(in) the ChooserTable asset to evaluate
	*/
	UFUNCTION(BlueprintPure, Category = "Animation", meta = (BlueprintThreadSafe, BlueprintInternalUseOnly="true"))
	static FInstancedStruct MakeLookupProxy(UProxyAsset* Proxy);
	
	UFUNCTION(BlueprintPure, Category = "Animation", meta = (BlueprintThreadSafe, BlueprintInternalUseOnly="true"))
	static FInstancedStruct MakeLookupProxyWithOverrideTable(UProxyAsset* Proxy, UProxyTable* ProxyTable);
};