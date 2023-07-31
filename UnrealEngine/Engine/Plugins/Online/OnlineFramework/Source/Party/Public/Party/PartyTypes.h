// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocialTypes.h"

#include "PartyTypes.generated.h"

class USocialParty;
class UPartyMember;

class IOnlinePartySystem;
class FOnlinePartyData;

enum class EJoinPartyCompletionResult : int8;

UENUM()
enum class EPartyType : uint8
{
	/** This party is public (not really supported right now) */
	Public,
	/** This party is joinable by friends */
	FriendsOnly,
	/** This party requires an invite from someone within the party */
	Private
};

UENUM()
enum class EPartyInviteRestriction : uint8
{
	/** Any party member can send invites */
	AnyMember,
	/** Only the leader can send invites */
	LeaderOnly,
	/** Nobody can invite anyone to this party */
	NoInvites
};

UENUM()
enum class EPartyJoinDenialReason : uint8
{
	/** Framework-level denial reasons */

	/** No denial, matches success internally */
	NoReason = 0,
	/** The local player aborted the join attempt */
	JoinAttemptAborted,
	/** Party leader is busy or at inopportune time to allow joins - to be used as a fallback when there isn't a more specific reason (more specific reasons are preferred) */
	Busy,
	/** Either the necessary OSS itself or critical element thereof (PartyInterface, SessionInterface, etc.) is missing. */
	OssUnavailable,
	/** Party is full */
	PartyFull,
	/** Game is full, but not party */
	GameFull,
	/** Asked a non party leader to join game, shouldn't happen */
	NotPartyLeader,
	/** Party has been marked as private and the join request is revoked */
	PartyPrivate,
	/** Player has crossplay restriction that would be violated */
	JoinerCrossplayRestricted,
	/** Party member has crossplay restriction that would be violated */
	MemberCrossplayRestricted,
	/** Player is in a game mode that restricts joining */
	GameModeRestricted,
	/** Player is currently banned */
	Banned,
	/** Player is not yet logged in */
	NotLoggedIn,
	/** Unable to start joining - we are checking for a session to rejoin */
	CheckingForRejoin,
	/** The target user is missing presence info */
	TargetUserMissingPresence,
	/** The target user's presence says the user is unjoinable */
	TargetUserUnjoinable,
	/** The target user is currently Away */
	TargetUserAway,
	/** We found ourself to be the leader of the friend's party according to the console session */
	AlreadyLeaderInPlatformSession,
	/** The target user is not playing the same game as us */
	TargetUserPlayingDifferentGame,
	/** The target user's presence does not have any information about their party session (platform friends only) */
	TargetUserMissingPlatformSession,
	/** There is no party join info available in the target user's platform session */
	PlatformSessionMissingJoinInfo,
	/** We were unable to launch the query to find the platform friend's session (platform friends only) */
	FailedToStartFindConsoleSession,
	/** The party is of a type that the game does not support (it specified nullptr for the USocialParty class) */
	MissingPartyClassForTypeId,
	/** The target user is blocked by the local user on one or more of the active subsystems */
	TargetUserBlocked,

	/**
	* Customizable denial reasons.
	* Expected usage is to assign the entries in the custom enum to the arbitrary custom entry placeholders below.
	* App level users of the system can then cast to/from their custom enum as desired.
	*/
	CustomReason0,
	CustomReason1,
	CustomReason2,
	CustomReason3,
	CustomReason4,
	CustomReason5,
	CustomReason6,
	CustomReason7,
	CustomReason8,
	CustomReason9,
	CustomReason10,
	CustomReason11,
	CustomReason12,
	CustomReason13,
	CustomReason14,
	CustomReason15,
	CustomReason16,
	CustomReason17,
	CustomReason18,
	CustomReason19,
	CustomReason20,
	CustomReason21,
	CustomReason22,
	CustomReason23,
	CustomReason24,
	CustomReason25,
	CustomReason26,
	CustomReason27,
	CustomReason28,
	CustomReason29,
	CustomReason30,
	CustomReason31,
	CustomReason32,
	CustomReason33,
	CustomReason34,
	CustomReason35,
	CustomReason36,
	CustomReason37,
	CustomReason38,
	CustomReason39,

	MAX
};

inline const TCHAR* ToString(EPartyJoinDenialReason Type)
{
	switch (Type)
	{
	case EPartyJoinDenialReason::NoReason:
		return TEXT("NoReason");
		break;
	case EPartyJoinDenialReason::JoinAttemptAborted:
		return TEXT("JoinAttemptAborted");
		break;
	case EPartyJoinDenialReason::Busy:
		return TEXT("Busy");
		break;
	case EPartyJoinDenialReason::OssUnavailable:
		return TEXT("OssUnavailable");
		break;
	case EPartyJoinDenialReason::PartyFull:
		return TEXT("PartyFull");
		break;
	case EPartyJoinDenialReason::GameFull:
		return TEXT("GameFull");
		break;
	case EPartyJoinDenialReason::NotPartyLeader:
		return TEXT("NotPartyLeader");
		break;
	case EPartyJoinDenialReason::PartyPrivate:
		return TEXT("PartyPrivate");
		break;
	case EPartyJoinDenialReason::JoinerCrossplayRestricted:
		return TEXT("JoinerCrossplayRestricted");
		break;
	case EPartyJoinDenialReason::MemberCrossplayRestricted:
		return TEXT("MemberCrossplayRestricted");
		break;
	case EPartyJoinDenialReason::GameModeRestricted:
		return TEXT("GameModeRestricted");
		break;
	case EPartyJoinDenialReason::Banned:
		return TEXT("Banned");
		break;
	case EPartyJoinDenialReason::NotLoggedIn:
		return TEXT("NotLoggedIn");
		break;
	case EPartyJoinDenialReason::CheckingForRejoin:
		return TEXT("CheckingForRejoin");
		break;
	case EPartyJoinDenialReason::TargetUserMissingPresence:
		return TEXT("TargetUserMissingPresence");
		break;
	case EPartyJoinDenialReason::TargetUserUnjoinable:
		return TEXT("TargetUserUnjoinable");
		break;
	case EPartyJoinDenialReason::AlreadyLeaderInPlatformSession:
		return TEXT("AlreadyLeaderInPlatformSession");
		break;
	case EPartyJoinDenialReason::TargetUserPlayingDifferentGame:
		return TEXT("TargetUserPlayingDifferentGame");
		break;
	case EPartyJoinDenialReason::TargetUserMissingPlatformSession:
		return TEXT("TargetUserMissingPlatformSession");
		break;
	case EPartyJoinDenialReason::PlatformSessionMissingJoinInfo:
		return TEXT("PlatformSessionMissingJoinInfo");
		break;
	case EPartyJoinDenialReason::FailedToStartFindConsoleSession:
		return TEXT("FailedToStartFindConsoleSession");
		break;
	case EPartyJoinDenialReason::MissingPartyClassForTypeId:
		return TEXT("MissingPartyClassForTypeId");
		break;
	default:
		return TEXT("CustomReason");
		break;
	}
}

UENUM()
enum class EApprovalAction : uint8
{
	Approve,
	Enqueue,
	EnqueueAndStartBeacon,
	Deny
};

UENUM()
enum class ESocialPartyInviteMethod : uint8 
{
	/** Default value for try invite */
	Other = 0,
	/** Invite was sent from a toast */
	Notification,
	/** Invite was sent with a custom method */
	Custom1
};

UENUM()
enum class ESocialPartyInviteFailureReason : uint8 
{
	Success = 0,
	NotOnline,
	NotAcceptingMembers,
	NotFriends,
	AlreadyInParty,
	OssValidationFailed,
	PlatformInviteFailed,
	PartyInviteFailed,
	InviteRateLimitExceeded
};


inline const TCHAR* LexToString(ESocialPartyInviteFailureReason Type)
{
	switch (Type)
	{
	case ESocialPartyInviteFailureReason::Success: return TEXT("Success");
	case ESocialPartyInviteFailureReason::NotOnline: return TEXT("NotOnline");
	case ESocialPartyInviteFailureReason::NotAcceptingMembers: return TEXT("NotAcceptingMembers");
	case ESocialPartyInviteFailureReason::NotFriends: return TEXT("NotFriends");
	case ESocialPartyInviteFailureReason::AlreadyInParty: return TEXT("AlreadyInParty");
	case ESocialPartyInviteFailureReason::OssValidationFailed: return TEXT("OssValidationFailed");
	case ESocialPartyInviteFailureReason::PlatformInviteFailed: return TEXT("PlatformInviteFailed");
	case ESocialPartyInviteFailureReason::PartyInviteFailed: return TEXT("PartyInviteFailed");
	case ESocialPartyInviteFailureReason::InviteRateLimitExceeded: return TEXT("InviteRateLimitExceeded");
	default:
		checkNoEntry();
		return TEXT("Unknown");
	}
}

// Gives a smidge more meaning to the intended use for the string. These should just be UniqueId's (and are), but not reliably allocated as shared ptrs, so they cannot be replicated via FUniqueNetIdRepl.
using FSessionId = FString;

USTRUCT()
struct FPartyPlatformSessionInfo
{
	GENERATED_BODY();

public:

	/** The platform session type (because in a crossplay party, members can be in different session types) */
	UPROPERTY()
	FString SessionType;

	/** The platform session id. Will be unset if it is not yet available to be joined. */
	UPROPERTY()
	FString SessionId;

	/** Primary OSS ID of the player that owns this console session */
	UPROPERTY()
	FUniqueNetIdRepl OwnerPrimaryId;

	bool operator==(const FString& InSessionType) const;
	bool operator==(const FPartyPlatformSessionInfo& Other) const;
	bool operator!=(const FPartyPlatformSessionInfo& Other) const { return !(*this == Other); }

	FString ToDebugString() const;
	bool IsSessionOwner(const UPartyMember& PartyMember) const;
	bool IsInSession(const UPartyMember& PartyMember) const;
};

USTRUCT()
struct FPartyPrivacySettings
{
	GENERATED_BODY()

public:
	/** The type of party in terms of advertised joinability restrictions */
	UPROPERTY()
	EPartyType PartyType = EPartyType::Private;

	/** Who is allowed to send invitataions to the party? */
	UPROPERTY()
	EPartyInviteRestriction PartyInviteRestriction = EPartyInviteRestriction::NoInvites;

	/** True to restrict the party exclusively to friends of the party leader */
	UPROPERTY()
	bool bOnlyLeaderFriendsCanJoin = true;

	bool PARTY_API operator==(const FPartyPrivacySettings& Other) const;
	bool PARTY_API operator!=(const FPartyPrivacySettings& Other) const { return !operator==(Other); }

	FPartyPrivacySettings() {}
};


/** Companion to EPartyJoinDenialReason to lessen the hassle of working with a "customized" enum */
struct PARTY_API FPartyJoinDenialReason
{
public:
	FPartyJoinDenialReason() {}
	FPartyJoinDenialReason(int32 DenialReasonCode)
	{
		if (ensure(DenialReasonCode >= 0 && DenialReasonCode < (uint8)EPartyJoinDenialReason::MAX))
		{
			DenialReason = (EPartyJoinDenialReason)DenialReasonCode;
		}
	}

	template <typename CustomReasonEnumT>
	FPartyJoinDenialReason(CustomReasonEnumT CustomReason)
		: FPartyJoinDenialReason((int32)CustomReason)
	{
	}

	bool operator==(const FPartyJoinDenialReason& Other) const { return DenialReason == Other.DenialReason; }
	bool operator==(int32 ReasonCode) const { return (int32)DenialReason == ReasonCode; }

	template <typename CustomReasonEnumT>
	bool operator==(CustomReasonEnumT CustomReason) const { return (uint8)DenialReason == (uint8)CustomReason; }

	operator int32() const { return (int32)DenialReason; }

	bool HasAnyReason() const { return DenialReason != EPartyJoinDenialReason::NoReason; }
	bool HasCustomReason() const { return DenialReason >= EPartyJoinDenialReason::CustomReason0; }
	EPartyJoinDenialReason GetReason() const { return DenialReason; }

	template <typename CustomReasonEnumT>
	CustomReasonEnumT GetCustomReason() const
	{
		checkf(HasCustomReason(), TEXT("Cannot convert a non-custom disapproval reason to a custom one. Always check HasCustomReason() first."));
		return static_cast<CustomReasonEnumT>((uint8)DenialReason);
	}

	template <typename CustomReasonEnumT>
	const TCHAR* AsString() const
	{
		if (HasCustomReason())
		{
			return ToString(GetCustomReason<CustomReasonEnumT>());
		}
		return ToString(DenialReason);
	}

private:
	EPartyJoinDenialReason DenialReason = EPartyJoinDenialReason::NoReason;
};

struct FPartyJoinApproval
{
	FPartyJoinApproval() {}
	
	void SetDenialReason(FPartyJoinDenialReason InDenialReason)
	{
		DenialReason = InDenialReason;
		if (DenialReason.HasAnyReason())
		{
			ApprovalAction = EApprovalAction::Deny;
		}
	}

	void SetApprovalAction(EApprovalAction InApprovalAction)
	{
		ApprovalAction = InApprovalAction;
		if (ApprovalAction != EApprovalAction::Deny)
		{
			DenialReason = EPartyJoinDenialReason::NoReason;
		}
	}

	EApprovalAction GetApprovalAction() const { return ApprovalAction; }
	FPartyJoinDenialReason GetDenialReason() const { return DenialReason; }

	bool CanJoin() const { return ApprovalAction != EApprovalAction::Deny && ensure(!DenialReason.HasAnyReason()); }

private:
	FPartyJoinDenialReason DenialReason;
	EApprovalAction ApprovalAction = EApprovalAction::Approve;
};

struct PARTY_API FJoinPartyResult
{
public:
	FJoinPartyResult(); 
	FJoinPartyResult(FPartyJoinDenialReason InDenialReason);
	FJoinPartyResult(EJoinPartyCompletionResult InResult);
	FJoinPartyResult(EJoinPartyCompletionResult InResult, FPartyJoinDenialReason InDenialReason);
	FJoinPartyResult(EJoinPartyCompletionResult InResult, int32 InResultSubCode);
	
	void SetDenialReason(FPartyJoinDenialReason InDenialReason);
	void SetResult(EJoinPartyCompletionResult InResult);
	
	bool WasSuccessful() const;

	EJoinPartyCompletionResult GetResult() const { return Result; }
	FPartyJoinDenialReason GetDenialReason() const { return DenialReason; }
	int32 GetResultSubCode() const { return ResultSubCode; }

private:
	EJoinPartyCompletionResult Result;
	/** Denial reason - used if Result is NotApproved */
	FPartyJoinDenialReason DenialReason;
	/** Result sub code - used for any other Result type */
	int32 ResultSubCode = 0;
};

/** Base for all rep data structs */
USTRUCT()
struct PARTY_API FOnlinePartyRepDataBase
{
	GENERATED_USTRUCT_BODY()
	virtual ~FOnlinePartyRepDataBase() {}

protected:
	/**
	 * Compare this data against the given old data, triggering delegates as appropriate.
	 * Intended to be used in concert with EXPOSE_REP_DATA_PROPERTY so that all you need to do is run through each property's CompareX function.
	 * If you need to do more complex comparison logic, you're welcome to do that as well/instead.
	 * @see FPartyRepData::CompareAgainst for an example
	 */
	virtual void CompareAgainst(const FOnlinePartyRepDataBase& OldData) const {}

	/**
	 * Called directly after an updated member state is received and copied into the local state
	 */
	virtual void PostReplication(const FOnlinePartyRepDataBase& OldData) {}
	virtual bool CanEditData() const { return false; }
	virtual const USocialParty* GetOwnerParty() const { return nullptr; }
	virtual const UPartyMember* GetOwningMember() const { return nullptr; }

	void LogSetPropertyFailure(const TCHAR* OwningStructTypeName, const TCHAR* PropertyName) const;
	void LogPropertyChanged(const TCHAR* OwningStructTypeName, const TCHAR* PropertyName, bool bFromReplication) const;

	friend class FPartyDataReplicatorHelper;
	template <typename, class> friend class TPartyDataReplicator;
	FSimpleDelegate OnDataChanged;
};

//////////////////////////////////////////////////////////////////////////
// Boilerplate for exposing RepData properties
//////////////////////////////////////////////////////////////////////////

/** Simplest option - exposes getter and events, but no default setter */
#define EXPOSE_REP_DATA_PROPERTY_NO_SETTER(Owner, PropertyType, PropertyName, PropertyAccess)	\
public:	\
	/** If the property is a POD or ptr type, we'll work with it by copy. Otherwise, by const ref */	\
	using Mutable##PropertyName##Type = typename TRemoveConst<PropertyType>::Type;	\
	using PropertyName##ArgType = typename TChooseClass<TOr<TIsPODType<PropertyType>, TIsPointer<PropertyType>>::Value, PropertyType, const Mutable##PropertyName##Type&>::Result;	\
	\
private:	\
	/** Bummer to have two signatures, but cases that want both the old and new values are much rarer, so most don't want to bother with a handler that takes an extra unused param */	\
	DECLARE_MULTICAST_DELEGATE_OneParam(FOn##PropertyName##Changed, PropertyName##ArgType /*NewValue*/);	\
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOn##PropertyName##ChangedDif, PropertyName##ArgType /*NewValue*/, PropertyName##ArgType /*OldValue*/);	\
public:	\
	/** Bind to receive the new property value only on changes */	\
	FOn##PropertyName##Changed& On##PropertyName##Changed() const { return On##PropertyName##ChangedEvent; }	\
	/** Bind to receive both the new and old property value on changes */	\
	FOn##PropertyName##ChangedDif& On##PropertyName##ChangedDif() const { return On##PropertyName##ChangedDifEvent; }	\
	\
	PropertyName##ArgType Get##PropertyName() const { return PropertyAccess; }	\
private:	\
	void Compare##PropertyName(const Owner& OldData) const	\
	{	\
		Compare##PropertyName(OldData.PropertyAccess); \
	}	\
	void Compare##PropertyName(const typename TRemoveConst<PropertyType>::Type& OldData) const	\
	{	\
		if (PropertyAccess != OldData)	\
		{	\
			LogPropertyChanged(TEXT(#Owner), TEXT(#PropertyName), true);	\
			On##PropertyName##ChangedDif().Broadcast(PropertyAccess, OldData);	\
			On##PropertyName##Changed().Broadcast(PropertyAccess);	\
		}	\
	}	\
	mutable FOn##PropertyName##Changed On##PropertyName##ChangedEvent;	\
	mutable FOn##PropertyName##ChangedDif On##PropertyName##ChangedDifEvent

#define EXPOSE_REP_DATA_PROPERTY_SETTER_ONLY(Owner, PropertyType, PropertyName, PropertyAccess, SetterPrivacy)	\
SetterPrivacy:	\
	void Set##PropertyName(PropertyName##ArgType New##PropertyName)	\
	{	\
		if (CanEditData())	\
		{	\
			if (PropertyAccess != New##PropertyName)	\
			{	\
				LogPropertyChanged(TEXT(#Owner), TEXT(#PropertyName), false);	\
				if (On##PropertyName##ChangedDif().IsBound())	\
				{	\
					PropertyType OldValue = PropertyAccess;	\
					PropertyAccess = New##PropertyName;	\
					On##PropertyName##ChangedDif().Broadcast(PropertyAccess, OldValue);	\
				}	\
				else	\
				{	\
					PropertyAccess = New##PropertyName;	\
				}	\
				On##PropertyName##Changed().Broadcast(PropertyAccess);	\
				OnDataChanged.ExecuteIfBound();	\
			}	\
		}	\
		else	\
		{	\
			LogSetPropertyFailure(TEXT(#Owner), TEXT(#PropertyName));	\
		}	\
	}	\
private: //

/* Helpers for printing a user-friendly field deprecation warning for replicated data. */
#define STRINGIFY_REP_DATA_PROPERTY_REVISION_WARNING_TEXT(WarningText) #WarningText
#define REP_DATA_PROPERTY_REVISION_WARNING(DeprecatedName, RevisedName, DeprecatedVersion) UE_DEPRECATED(DeprecatedVersion, STRINGIFY_REP_DATA_PROPERTY_REVISION_WARNING_TEXT(DeprecatedName) " has been replaced by " STRINGIFY_REP_DATA_PROPERTY_REVISION_WARNING_TEXT(RevisedName) ".")

/**
 * Exposes a rep data property and provides a default property setter.
 * Awkwardly named util - don't bother using directly. Opt for EXPOSE_PRIVATE_REP_DATA_PROPERTY or EXPOSE_REP_DATA_PROPERTY
 */
#define EXPOSE_REP_DATA_PROPERTY_SETTER_ACCESS(Owner, PropertyType, PropertyName, PropertyAccess, SetterPrivacy)	\
EXPOSE_REP_DATA_PROPERTY_NO_SETTER(Owner, PropertyType, PropertyName, PropertyAccess);	\
EXPOSE_REP_DATA_PROPERTY_SETTER_ONLY(Owner, PropertyType, PropertyName, PropertyAccess, SetterPrivacy)

 /** 
 * Simplest option - exposes getter and events, but no default setter
 * Revised version - allows the preservation of a previous property name for making delegates and accessors which will work along-side the new names.
 *                   A compile-time deprecation warning will be printed when the old fields are accessed.
 */
#define EXPOSE_REVISED_REP_DATA_PROPERTY_NO_SETTER(Owner, PropertyType, PropertyName, PropertyAccess, DeprecatedPropertyName, DeprecatedVersion)	\
EXPOSE_REP_DATA_PROPERTY_NO_SETTER(Owner, PropertyType, PropertyName, PropertyAccess); \
public:	\
	/** Bind to receive the new property value only on changes */	\
	REP_DATA_PROPERTY_REVISION_WARNING(On##DeprecatedPropertyName##Changed, On##PropertyName##Changed, DeprecatedVersion)	\
	FOn##PropertyName##Changed& On##DeprecatedPropertyName##Changed() const	{ return On##PropertyName##ChangedEvent; }	\
	/** Bind to receive both the new and old property value on changes */	\
	REP_DATA_PROPERTY_REVISION_WARNING(On##DeprecatedPropertyName##ChangedDif, On##PropertyName##ChangedDif, DeprecatedVersion)	\
	FOn##PropertyName##ChangedDif& On##DeprecatedPropertyName##ChangedDif() const { return On##PropertyName##ChangedDifEvent; }	\
	\
	REP_DATA_PROPERTY_REVISION_WARNING(Get##DeprecatedPropertyName, Get##PropertyName, DeprecatedVersion)	\
	PropertyName##ArgType Get##DeprecatedPropertyName() const { return PropertyAccess; }

/**
 * Exposes a rep data property and provides a default property setter.
 * Awkwardly named util - don't bother using directly. Opt for EXPOSE_PRIVATE_REP_DATA_PROPERTY or EXPOSE_REP_DATA_PROPERTY
 * Revised version - allows the preservation of a previous property name for making delegates and accessors which will work along-side the new names.
 *                   A compile-time deprecation warning will be printed when the old fields are accessed.
 */
#define EXPOSE_REVISED_REP_DATA_PROPERTY_SETTER_ACCESS(Owner, PropertyType, PropertyName, PropertyAccess, SetterPrivacy, DeprecatedPropertyName, DeprecatedVersion)	\
EXPOSE_REVISED_REP_DATA_PROPERTY_NO_SETTER(Owner, PropertyType, PropertyName, PropertyAccess, DeprecatedPropertyName, DeprecatedVersion);	\
EXPOSE_REP_DATA_PROPERTY_SETTER_ONLY(Owner, PropertyType, PropertyName, PropertyAccess, SetterPrivacy)	\
SetterPrivacy:	\
	REP_DATA_PROPERTY_REVISION_WARNING(Set##DeprecatedPropertyName, Set##PropertyName, DeprecatedVersion)	\
	void Set##DeprecatedPropertyName(PropertyName##ArgType New##PropertyName)	\
	{	\
		Set##PropertyName(New##PropertyName);	\
	}	\
private: //

/**
 * Exposes a rep data property with a private setter.
 * The setter only allows modification if the local user has authority to alter the property and will automatically trigger delegates appropriately.
 */
#define EXPOSE_PRIVATE_REP_DATA_PROPERTY(Owner, PropertyType, Property)	\
EXPOSE_REP_DATA_PROPERTY_SETTER_ACCESS(Owner, PropertyType, Property, Property, private);

/**
 * Fully exposes the rep data property with a public setter.
 * The setter only allows modification if the local user has authority to alter the property and will automatically trigger delegates appropriately
 */
#define EXPOSE_REP_DATA_PROPERTY(Owner, PropertyType, Property)	\
EXPOSE_REP_DATA_PROPERTY_SETTER_ACCESS(Owner, PropertyType, Property, Property, public);

/**
* Exposes a rep data property with a private setter for a member within a USTRUCT parameter.
* The setter only allows modification if the local user has authority to alter the property and will automatically trigger delegates appropriately.
*/
#define EXPOSE_PRIVATE_USTRUCT_REP_DATA_PROPERTY(Owner, PropertyType, StructProperty, ChildProperty)	\
EXPOSE_REP_DATA_PROPERTY_SETTER_ACCESS(Owner, PropertyType, StructProperty##ChildProperty, StructProperty.ChildProperty, private);

/**
* Fully exposes the rep data property with a public setter for a member within a USTRUCT parameter.
* The setter only allows modification if the local user has authority to alter the property and will automatically trigger delegates appropriately
*/
#define EXPOSE_USTRUCT_REP_DATA_PROPERTY(Owner, PropertyType, StructProperty, ChildProperty)	\
EXPOSE_REP_DATA_PROPERTY_SETTER_ACCESS(Owner, PropertyType, StructProperty##ChildProperty, StructProperty.ChildProperty, public);

/**
* Exposes a rep data property with a private setter for a member within a USTRUCT parameter.
* The setter only allows modification if the local user has authority to alter the property and will automatically trigger delegates appropriately.
* The deprecated name is used to preserve existing delegates and accessors until a later time.
*/
#define EXPOSE_REVISED_PRIVATE_USTRUCT_REP_DATA_PROPERTY(Owner, PropertyType, StructProperty, ChildProperty, DeprecatedPropertyName, DeprecatedVersion)	\
EXPOSE_REVISED_REP_DATA_PROPERTY_SETTER_ACCESS(Owner, PropertyType, StructProperty##ChildProperty, StructProperty.ChildProperty, private, DeprecatedPropertyName, DeprecatedVersion);

/**
* Fully exposes the rep data property with a public setter for a member within a USTRUCT parameter.
* The setter only allows modification if the local user has authority to alter the property and will automatically trigger delegates appropriately
* The deprecated name is used to preserve existing delegates and accessors until a later time.
*/
#define EXPOSE_REVISED_USTRUCT_REP_DATA_PROPERTY(Owner, PropertyType, StructProperty, ChildProperty, DeprecatedPropertyName, DeprecatedVersion)	\
EXPOSE_REVISED_REP_DATA_PROPERTY_SETTER_ACCESS(Owner, PropertyType, StructProperty##ChildProperty, StructProperty.ChildProperty, public, DeprecatedPropertyName, DeprecatedVersion);

inline const TCHAR* ToString(EPartyType Type)
{
	switch (Type)
	{
	case EPartyType::Public:
	{
		return TEXT("Public");
	}
	case EPartyType::FriendsOnly:
	{
		return TEXT("FriendsOnly");
	}
	case EPartyType::Private:
	{
		return TEXT("Private");
	}
	default:
	{
		return TEXT("Unknown");
	}
	}
}

inline const TCHAR* ToString(EApprovalAction Type)
{
	switch (Type)
	{
	case EApprovalAction::Approve:
	{
		return TEXT("Approve");
	}
	case EApprovalAction::Enqueue:
	{
		return TEXT("Enqueue");
	}
	case EApprovalAction::EnqueueAndStartBeacon:
	{
		return TEXT("EnqueueAndStartBeacon");
	}
	case EApprovalAction::Deny:
	{
		return TEXT("Deny");
	}
	default:
	{
		return TEXT("Unknown");
	}
	}
}

/** Defines the means used to join a party*/
namespace PartyJoinMethod
{
	// User has created the party
	const FName Creation = FName(TEXT("Creation"));
	// User has joined the party via invitation
	const FName Invitation = FName(TEXT("Invitation"));
	// user has joined after requesting access
	const FName RequestToJoin = FName(TEXT("RequestToJoin"));
	// User has joined the party via presence
	const FName Presence = FName(TEXT("Presence"));
	// User has joined the party using a platform option
	const FName PlatformSession = FName(TEXT("PlatformSession"));
	// User has joined the party via command line
	const FName CommandLineJoin = FName(TEXT("CommandLineJoin"));
	// User has joined via unknown/undocumented process
	const FName Unspecified = FName(TEXT("Unspecified"));
	
}
