// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "DerivedDataBuildTypes.h"
#include "DerivedDataSharedStringFwd.h"
#include "Logging/LogMacros.h"

struct FGuid;

template <typename FuncType> class TUniqueFunction;

namespace UE::DerivedData { class FBuildAction; }
namespace UE::DerivedData { class FBuildActionBuilder; }
namespace UE::DerivedData { class FBuildDefinition; }
namespace UE::DerivedData { class FBuildDefinitionBuilder; }
namespace UE::DerivedData { class FBuildInputsBuilder; }
namespace UE::DerivedData { class FBuildOutputBuilder; }
namespace UE::DerivedData { class FBuildPolicy; }
namespace UE::DerivedData { class FBuildSession; }
namespace UE::DerivedData { class FOptionalBuildInputs; }
namespace UE::DerivedData { class IBuild; }
namespace UE::DerivedData { class IBuildFunctionRegistry; }
namespace UE::DerivedData { class IBuildInputResolver; }
namespace UE::DerivedData { class IBuildScheduler; }
namespace UE::DerivedData { class IBuildWorkerRegistry; }
namespace UE::DerivedData { class ICache; }
namespace UE::DerivedData { class IRequestOwner; }
namespace UE::DerivedData { struct FBuildKey; }
namespace UE::DerivedData { struct FValueId; }

namespace UE::DerivedData::Private
{

DECLARE_LOG_CATEGORY_EXTERN(LogDerivedDataBuild, Log, All);

// Implemented in DerivedDataBuild.cpp
IBuild* CreateBuild(ICache& Cache);

// Implemented in DerivedDataBuildFunctionRegistry.cpp
IBuildFunctionRegistry* CreateBuildFunctionRegistry();
bool IsValidBuildFunctionName(FUtf8StringView Function);
void AssertValidBuildFunctionName(FUtf8StringView Function, FStringView Name);

// Implemented in DerivedDataBuildWorkerRegistry.cpp
IBuildWorkerRegistry* CreateBuildWorkerRegistry();

// Implemented in DerivedDataBuildScheduler.cpp
IBuildScheduler* CreateBuildScheduler();

// Implemented in DerivedDataBuildDefinition.cpp
FBuildDefinitionBuilder CreateBuildDefinition(const FSharedString& Name, const FUtf8SharedString& Function);

// Implemented in DerivedDataBuildAction.cpp
FBuildActionBuilder CreateBuildAction(const FSharedString& Name, const FUtf8SharedString& Function, const FGuid& FunctionVersion, const FGuid& BuildSystemVersion);

// Implemented in DerivedDataBuildInput.cpp
FBuildInputsBuilder CreateBuildInputs(const FSharedString& Name);

// Implemented in DerivedDataBuildOutput.cpp
FBuildOutputBuilder CreateBuildOutput(const FSharedString& Name, const FUtf8SharedString& Function);
FValueId GetBuildOutputValueId();

// Implemented in DerivedDataBuildSession.cpp
FBuildSession CreateBuildSession(
	const FSharedString& Name,
	ICache& Cache,
	IBuild& BuildSystem,
	IBuildScheduler& Scheduler,
	IBuildInputResolver* InputResolver);

// Implemented in DerivedDataBuildJob.cpp
struct FBuildJobCreateParams
{
	ICache& Cache;
	IBuild& BuildSystem;
	IBuildScheduler& Scheduler;
	IBuildInputResolver* InputResolver{};
	IRequestOwner& Owner;
};
void CreateBuildJob(
	const FBuildJobCreateParams& Params,
	const FBuildKey& Key,
	const FBuildPolicy& Policy,
	FOnBuildComplete&& OnComplete);
void CreateBuildJob(
	const FBuildJobCreateParams& Params,
	const FBuildDefinition& Definition,
	const FOptionalBuildInputs& Inputs,
	const FBuildPolicy& Policy,
	FOnBuildComplete&& OnComplete);
void CreateBuildJob(
	const FBuildJobCreateParams& Params,
	const FBuildAction& Action,
	const FOptionalBuildInputs& Inputs,
	const FBuildPolicy& Policy,
	FOnBuildComplete&& OnComplete);

} // UE::DerivedData::Private
