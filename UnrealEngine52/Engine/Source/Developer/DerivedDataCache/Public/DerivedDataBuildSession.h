// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "DerivedDataBuildTypes.h"
#include "DerivedDataSharedStringFwd.h"
#include "Templates/Function.h"
#include "Templates/UniquePtr.h"
#include "Templates/UnrealTemplate.h"

namespace UE::DerivedData { class FBuildAction; }
namespace UE::DerivedData { class FBuildDefinition; }
namespace UE::DerivedData { class FBuildSession; }
namespace UE::DerivedData { class FOptionalBuildInputs; }
namespace UE::DerivedData { class IRequestOwner; }
namespace UE::DerivedData { struct FBuildKey; }

namespace UE::DerivedData::Private
{

class IBuildSessionInternal
{
public:
	virtual ~IBuildSessionInternal() = default;
	virtual const FSharedString& GetName() const = 0;
	virtual void Build(
		const FBuildKey& Key,
		const FBuildPolicy& Policy,
		IRequestOwner& Owner,
		FOnBuildComplete&& OnComplete) = 0;
	virtual void Build(
		const FBuildDefinition& Definition,
		const FOptionalBuildInputs& Inputs,
		const FBuildPolicy& Policy,
		IRequestOwner& Owner,
		FOnBuildComplete&& OnComplete) = 0;
	virtual void Build(
		const FBuildAction& Action,
		const FOptionalBuildInputs& Inputs,
		const FBuildPolicy& Policy,
		IRequestOwner& Owner,
		FOnBuildComplete&& OnComplete) = 0;
};

FBuildSession CreateBuildSession(IBuildSessionInternal* Session);

} // UE::DerivedData::Private

namespace UE::DerivedData
{

/**
 * A build session is the main point to the build scheduler.
 *
 * The purpose of a session is to group together related builds that use the same input resolver,
 * such as grouping builds by target platform. A request to build one definition can lead to more
 * builds being scheduled if the definition references values from other builds as inputs.
 */
class FBuildSession
{
public:
	/** Returns the name by which to identify this session for logging and profiling. */
	inline const FSharedString& GetName() const
	{
		return Session->GetName();
	}

	/**
	 * Asynchronous request to execute a build according to the policy.
	 *
	 * The callback will always be called, and may be called from an arbitrary thread.
	 *
	 * @param Key          The key of the build definition to resolve and build.
	 * @param Policy       Flags to control the behavior of the request. See FBuildPolicy.
	 * @param Owner        The owner to execute the build within.
	 * @param OnComplete   A callback invoked when the build completes or is canceled.
	 */
	inline void Build(
		const FBuildKey& Key,
		const FBuildPolicy& Policy,
		IRequestOwner& Owner,
		FOnBuildComplete&& OnComplete)
	{
		Session->Build(Key, Policy, Owner, MoveTemp(OnComplete));
	}

	/**
	 * Asynchronous request to execute a build according to the policy.
	 *
	 * The callback will always be called, and may be called from an arbitrary thread.
	 *
	 * @param Definition   The build function to execute and references to its inputs.
	 * @param Inputs       The build inputs referenced by the definition, if it has any. Optional.
	 * @param Policy       Flags to control the behavior of the request. See FBuildPolicy.
	 * @param Owner        The owner to execute the build within.
	 * @param OnComplete   A callback invoked when the build completes or is canceled.
	 */
	inline void Build(
		const FBuildDefinition& Definition,
		const FOptionalBuildInputs& Inputs,
		const FBuildPolicy& Policy,
		IRequestOwner& Owner,
		FOnBuildComplete&& OnComplete)
	{
		Session->Build(Definition, Inputs, Policy, Owner, MoveTemp(OnComplete));
	}

	/**
	 * Asynchronous request to execute a build according to the policy.
	 *
	 * The callback will always be called, and may be called from an arbitrary thread.
	 *
	 * @param Action       The build function to execute and references to its inputs.
	 * @param Inputs       The build inputs referenced by the action, if it has any. Optional.
	 * @param Policy       Flags to control the behavior of the request. See FBuildPolicy.
	 * @param Owner        The owner to execute the build within.
	 * @param OnComplete   A callback invoked when the build completes or is canceled.
	 */
	inline void Build(
		const FBuildAction& Action,
		const FOptionalBuildInputs& Inputs,
		const FBuildPolicy& Policy,
		IRequestOwner& Owner,
		FOnBuildComplete&& OnComplete)
	{
		Session->Build(Action, Inputs, Policy, Owner, MoveTemp(OnComplete));
	}

private:
	friend class FOptionalBuildSession;
	friend FBuildSession Private::CreateBuildSession(Private::IBuildSessionInternal* Session);

	/** Construct a build session. Use IBuild::CreateSession(). */
	inline explicit FBuildSession(Private::IBuildSessionInternal* InSession)
		: Session(InSession)
	{
	}

	TUniquePtr<Private::IBuildSessionInternal> Session;
};

/**
 * A build session that can be null.
 *
 * @see FBuildSession
 */
class FOptionalBuildSession : private FBuildSession
{
public:
	inline FOptionalBuildSession() : FBuildSession(nullptr) {}

	inline FOptionalBuildSession(FBuildSession&& InSession) : FBuildSession(MoveTemp(InSession)) {}
	inline FOptionalBuildSession& operator=(FBuildSession&& InSession) { FBuildSession::operator=(MoveTemp(InSession)); return *this; }

	inline FOptionalBuildSession(const FBuildSession& InSession) = delete;
	inline FOptionalBuildSession& operator=(const FBuildSession& InSession) = delete;

	/** Returns the build session. The caller must check for null before using this accessor. */
	inline FBuildSession& Get() & { return *this; }
	inline FBuildSession Get() && { return MoveTemp(*this); }

	inline bool IsNull() const { return !IsValid(); }
	inline bool IsValid() const { return Session.IsValid(); }
	inline explicit operator bool() const { return IsValid(); }

	inline void Reset() { *this = FOptionalBuildSession(); }
};

} // UE::DerivedData
