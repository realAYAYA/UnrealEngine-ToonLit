// Copyright Epic Games, Inc. All Rights Reserved.

#include "DerivedDataBuildSession.h"

#include "DerivedDataBuildAction.h"
#include "DerivedDataBuildDefinition.h"
#include "DerivedDataBuildJob.h"
#include "DerivedDataBuildOutput.h"
#include "DerivedDataBuildPrivate.h"
#include "DerivedDataSharedString.h"
#include "DerivedDataValue.h"

namespace UE::DerivedData::Private
{

class FBuildSessionInternal final : public IBuildSessionInternal
{
public:
	FBuildSessionInternal(
		const FSharedString& InName,
		ICache& InCache,
		IBuild& InBuildSystem,
		IBuildScheduler& InScheduler,
		IBuildInputResolver* InInputResolver)
		: Name(InName)
		, Cache(InCache)
		, BuildSystem(InBuildSystem)
		, Scheduler(InScheduler)
		, InputResolver(InInputResolver)
	{
	}

	const FSharedString& GetName() const final { return Name; }

	void Build(
		const FBuildKey& Key,
		const FBuildPolicy& Policy,
		IRequestOwner& Owner,
		FOnBuildComplete&& OnComplete) final;

	void Build(
		const FBuildDefinition& Definition,
		const FOptionalBuildInputs& Inputs,
		const FBuildPolicy& Policy,
		IRequestOwner& Owner,
		FOnBuildComplete&& OnComplete) final;

	void Build(
		const FBuildAction& Action,
		const FOptionalBuildInputs& Inputs,
		const FBuildPolicy& Policy,
		IRequestOwner& Owner,
		FOnBuildComplete&& OnComplete) final;

	FSharedString Name;
	ICache& Cache;
	IBuild& BuildSystem;
	IBuildScheduler& Scheduler;
	IBuildInputResolver* InputResolver;
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FBuildSessionInternal::Build(
	const FBuildKey& Key,
	const FBuildPolicy& Policy,
	IRequestOwner& Owner,
	FOnBuildComplete&& OnComplete)
{
	CreateBuildJob({Cache, BuildSystem, Scheduler, InputResolver, Owner}, Key, Policy,
		OnComplete ? MoveTemp(OnComplete) : FOnBuildComplete([](FBuildCompleteParams&&){}));
}

void FBuildSessionInternal::Build(
	const FBuildDefinition& Definition,
	const FOptionalBuildInputs& Inputs,
	const FBuildPolicy& Policy,
	IRequestOwner& Owner,
	FOnBuildComplete&& OnComplete)
{
	CreateBuildJob({Cache, BuildSystem, Scheduler, InputResolver, Owner}, Definition, Inputs, Policy,
		OnComplete ? MoveTemp(OnComplete) : FOnBuildComplete([](FBuildCompleteParams&&){}));
}

void FBuildSessionInternal::Build(
	const FBuildAction& Action,
	const FOptionalBuildInputs& Inputs,
	const FBuildPolicy& Policy,
	IRequestOwner& Owner,
	FOnBuildComplete&& OnComplete)
{
	CreateBuildJob({Cache, BuildSystem, Scheduler, InputResolver, Owner}, Action, Inputs, Policy,
		OnComplete ? MoveTemp(OnComplete) : FOnBuildComplete([](FBuildCompleteParams&&){}));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FBuildSession CreateBuildSession(IBuildSessionInternal* Session)
{
	return FBuildSession(Session);
}

FBuildSession CreateBuildSession(
	const FSharedString& Name,
	ICache& Cache,
	IBuild& BuildSystem,
	IBuildScheduler& Scheduler,
	IBuildInputResolver* InputResolver)
{
	return CreateBuildSession(new FBuildSessionInternal(Name, Cache, BuildSystem, Scheduler, InputResolver));
}

} // UE::DerivedData::Private
