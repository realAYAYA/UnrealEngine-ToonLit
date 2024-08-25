// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "HttpPath.h"
#include "HttpRequestHandler.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"
#include "HttpServerRequest.h"
#include "IHttpRouter.h"
#include "Logging/LogMacros.h"
#include "UObject/NameTypes.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "ExternalRpcRegistry.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogExternalRpcRegistry, Log, All);

USTRUCT()
struct FExternalRpcArgumentDesc
{
	GENERATED_BODY()
public:
	FString Name;
	FString Type;
	FString Desc;
	bool bIsOptional = false;

	FExternalRpcArgumentDesc() = default;
	FExternalRpcArgumentDesc(FString InName, FString InType, FString InDesc, bool bInIsOptional = false)
	{
		Name = InName;
		Type = InType;
		Desc = InDesc;
		bIsOptional = bInIsOptional;
	}

	bool operator==(const FExternalRpcArgumentDesc& Other) const
	{
		return
			Name == Other.Name &&
			Type == Other.Type &&
			Desc == Other.Desc &&
			bIsOptional == Other.bIsOptional;
	}

	bool operator!=(const FExternalRpcArgumentDesc& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT()
struct FExternalRouteInfo
{
	GENERATED_BODY()
public:
	FName RouteName;
	FHttpPath RoutePath;
	EHttpServerRequestVerbs RequestVerbs = EHttpServerRequestVerbs::VERB_NONE;
	FString InputContentType;
	TArray<FExternalRpcArgumentDesc> ExpectedArguments;
	FString RpcCategory = TEXT("Unknown");
	bool bAlwaysOn = false;

	FExternalRouteInfo() = default;

	FExternalRouteInfo(FName InRouteName, FHttpPath InRoutePath, EHttpServerRequestVerbs InRequestVerbs, FString InCategory = TEXT("Unknown"), bool bInAlwaysOn = false, FString InContentType = {}, TArray<FExternalRpcArgumentDesc> InArguments = TArray<FExternalRpcArgumentDesc>())
	{
		RouteName = InRouteName;
		RoutePath = InRoutePath;
		RequestVerbs = InRequestVerbs;
		InputContentType = InContentType;
		ExpectedArguments = InArguments;
		RpcCategory = InCategory;
		bAlwaysOn = false;
	}

	bool operator==(const FExternalRouteInfo& Other) const
	{
		return
			RouteName == Other.RouteName &&
			RoutePath == Other.RoutePath &&
			RequestVerbs == Other.RequestVerbs &&
			InputContentType == Other.InputContentType &&
			ExpectedArguments == Other.ExpectedArguments &&
			RpcCategory == Other.RpcCategory &&
			bAlwaysOn == Other.bAlwaysOn;
	}

	bool operator!=(const FExternalRouteInfo& Other) const
	{
		return !(*this == Other);
	}
};

USTRUCT()
struct FExternalRouteDesc
{
	GENERATED_BODY()
public:
	FHttpRouteHandle Handle;
	FString InputContentType;
	TArray<FExternalRpcArgumentDesc> ExpectedArguments;
	FExternalRouteDesc() = default;
	FExternalRouteDesc(FHttpRouteHandle InHandle, FString InContentType, TArray<FExternalRpcArgumentDesc> InArguments)
	{
		Handle = InHandle;
		InputContentType = InContentType;
		ExpectedArguments = InArguments;
	}
};

/**
 * This class is designed to be a singleton that handles registry, maintenance, and cleanup of any REST endpoints exposed on the process 
 * for use in communicating with the process externally. 
 */
UCLASS(MinimalAPI)
class UExternalRpcRegistry : public UObject
{
	GENERATED_BODY()
protected:
	static EXTERNALRPCREGISTRY_API UExternalRpcRegistry * ObjectInstance;
	TMap<FName, FExternalRouteDesc> RegisteredRoutes;
	TArray<FString> ActiveRpcCategories;
public:
	static EXTERNALRPCREGISTRY_API UExternalRpcRegistry * GetInstance();

	EXTERNALRPCREGISTRY_API ~UExternalRpcRegistry();

	int PortToUse = 11223;

	/**
	* Check if this Rpc is from a category that is meant to be enabled.
	*/
	EXTERNALRPCREGISTRY_API bool IsActiveRpcCategory(FString InCategory);

	/**
	 * Try to get a route registered under given friendly name. Returns false if could not be found.
	 */
	EXTERNALRPCREGISTRY_API bool GetRegisteredRoute(FName RouteName, FExternalRouteInfo& OutRouteInfo);

	EXTERNALRPCREGISTRY_API void RegisterNewRoute(FExternalRouteInfo InRouteInfo, const FHttpRequestHandler& Handler, bool bOverrideIfBound = false);

	/**
	 * Register a new route.
	 * Will override existing routes if option is set, otherwise will error and fail to bind.
	 */
	EXTERNALRPCREGISTRY_API void RegisterNewRouteWithArguments(FName RouteName, const FHttpPath& HttpPath, const EHttpServerRequestVerbs& RequestVerbs, const FHttpRequestHandler& Handler, TArray<FExternalRpcArgumentDesc> InArguments, bool bOverrideIfBound = false, bool bIsAlwaysOn = false, FString OptionalCategory = TEXT("Unknown"), FString OptionalContentType = {});

	/**
	* Deprecated way to register a new route.
	* Will override existing routes if option is set, otherwise will error and fail to bind.
	 */
	UE_DEPRECATED(5.0, "RegisterNewRoute is deprecated when needing to add arguments, please use RegisterNewRouteWithArguments instead.")
	EXTERNALRPCREGISTRY_API void RegisterNewRoute(FName RouteName, const FHttpPath& HttpPath, const EHttpServerRequestVerbs& RequestVerbs, const FHttpRequestHandler& Handler, bool bOverrideIfBound = false, bool bIsAlwaysOn = false, FString OptionalCategory = TEXT("Unknown"), FString OptionalContentType = {}, FString OptionalExpectedFormat = {});

	/**
	 * Clean up all routes - generally called as part of the destructor to make sure we don't have any function pointers dangling around.
	 */
	EXTERNALRPCREGISTRY_API void CleanUpAllRoutes();

	/**
	 * Clean up a route.
	 * Can be set to fail if trying to unbind an unbound route.
	 */
	EXTERNALRPCREGISTRY_API void CleanUpRoute(FName RouteName, bool bFailIfUnbound = false);

	/**
	 * Default Route Listing http call. Spits out all registered routes and describes them via a REST API call.
	 * Always registered at /listrpcs GET by default
	 */
	EXTERNALRPCREGISTRY_API bool HttpListOpenRoutes(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);

};
