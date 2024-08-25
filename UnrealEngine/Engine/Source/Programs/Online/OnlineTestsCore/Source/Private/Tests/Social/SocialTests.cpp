// Copyright Epic Games, Inc. All Rights Reserved.
#include "CoreMinimal.h"
#include "TestHarness.h"
#include "OnlineCatchHelper.h"

#define SOCIAL_TAG "[SOCIAL]"  
#define SOCIAL_TEST_CASE(x, ...) ONLINE_TEST_CASE(x, SOCIAL_TAG __VA_ARGS__)

// QueryFriends tests
SOCIAL_TEST_CASE("Verify that QueryFriends returns a fail message if the local user is not logged in")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that QueryFriends caches no Friends if no Friends exist for this user")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that QueryFriends caches one Friend if only one Friend exists for this user")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that QueryFriends caches all Friends if multiple Friends exist for this user")
{
	// TODO
}

// GetFriends tests
SOCIAL_TEST_CASE("Verify that GetFriends returns a fail message if the local user is not logged in")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that GetFriends returns an empty list if there are no cached Friends")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that GetFriends returns a list of 1 Friend if there is 1 cached Friend")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that GetFriends returns a list of all cached Friends if there are multiple cached Friends")
{
	// TODO
}

// GetFriend tests
SOCIAL_TEST_CASE("Verify that GetFriend returns a fail message if the local user is not logged in")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that GetFriend returns a fail message if there are no cached Friends")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that GetFriend returns the correct Friend when given an existing FriendId")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that GetFriend returns a fail message when there are cached Friends but the given a FriendId that matches none of them")
{
	// TODO
}

// SendFriendInvite tests
SOCIAL_TEST_CASE("Verify that SendFriendInvite returns a fail message if the local user is not logged in")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that SendFriendInvite returns a fail message if the given TargetUserId does not exist")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that SendFriendInvite returns fail message if ERelationship with target user is Friend")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that SendFriendInvite completes successfully if ERelationship with target user is NotFriend, ERelationship becomes InviteSent")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that SendFriendInvite completes successfully if ERelationship with target user is InviteSent, ERelationship remains InviteSent")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that SendFriendInvite completes successfully if ERelationship with target user is InviteReceived, ERelationship becomes InviteSent")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that SendFriendInvite returns fail message if ERelationship with target user is Blocked")
{
	// TODO
}

// AcceptFriendInvite tests
SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns a fail message if the local user is not logged in")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns a fail message if ERelationship with target user is Friend")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns a fail message if ERelationship with target user is NotFriend")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns a fail message if ERelationship with target user is InviteSent")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that AcceptFriendInvite successfully completes if ERelationship with target user is InviteReceived, ERelationship becomes Friend")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that AcceptFriendInvite returns a fail message if ERelationship with target user is Blocked")
{
	// TODO
}

// RejectFriendInvite tests
SOCIAL_TEST_CASE("Verify that RejectFriendInvite returns a fail message if the local user is not logged in")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that RejectFriendInvite returns a fail message if ERelationship with target user is Friend")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that RejectFriendInvite returns a fail message if ERelationship with target user is NotFriend")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that RejectFriendInvite returns a fail message if ERelationship with target user is InviteSent")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that RejectFriendInvite successfully completes if ERelationship with target user is InviteReceived, ERelationship becomes NotFriend")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that RejectFriendInvite returns a fail message if ERelationship with target user is Blocked")
{
	// TODO
}

// OnRelationshipUpdated tests
// TODO: Unsure how to verify

// QueryBlockedUsers tests
SOCIAL_TEST_CASE("Verify that QueryBlockedUsers returns a fail message if the local user is not logged in")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that QueryBlockedUsers caches no blocked users if no blocked users exist for this user")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that QueryBlockedUsers caches one blocked user if only one blocked user exists for this user")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that QueryBlockedUsers caches all blocked users if multiple blocked users exist for this user")
{
	// TODO
}

// GetBlockedUsers tests
SOCIAL_TEST_CASE("Verify that GetBlockedUsers returns a fail message if the local user is not logged in")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that GetBlockedUsers returns an empty list if there are no cached blocked users")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that GetBlockedUsers returns a list of 1 blocked user if there is 1 cached bocked user")
{
	// TODO
}

SOCIAL_TEST_CASE("Verify that GetBlockedUsers returns a list of all cached blocked user if there are multiple cached blocked users")
{
	// TODO
}

// OnBlockedUserUpdated tests
// TODO: Unsure how to verify