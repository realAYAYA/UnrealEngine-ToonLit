// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_ROOM_H
#define RAIL_SDK_RAIL_ROOM_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_room_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailRoom;

// @desc IRailRoomHelper singleton
// Before a player starts gaming, he/she needs to create a room or join a room created by another
// player. In the room, the player can configure his/her own metadata, such as the readiness to
// start a game, gaming status or other customized metadata. As a room member, the player can also
// listen to notifications for other members' metadata changes. A room member will also be notified
// when other members leave the room, or the room is destroyed. If a room member is also the room
// owner, he/she will be able to set the metadata for other members, kick other members offline or
// transfer the room ownership to another member.
class IRailRoomHelper {
  public:
    // @desc Create a room directly *synchronously*.
    // You can also do it *asynchronously* with AsyncCreateRoom.
    // Call `room->Release()` if the returned room object is no longer needed
    // @param options Options used when creating a room
    // @param room_name The name of the room
    // @param result kSuccess will be returned on success
    // @return The pointer to the room object created to perform operations later
    virtual IRailRoom* CreateRoom(const RoomOptions& options,
                        const RailString& room_name,
                        RailResult* result) = 0;

    // @desc Create a room *asynchronously*. You cannot use the IRailRoom pointer until
    // the callback CreateRoomResult returns as kSuccess.
    // You will need to destroy the room object with Release() if the creation fails.
    // @param options Options used when creating a room
    // @param room_name The name of the room
    // @param user_data Will be copied to the asynchronous result
    // @callback CreateRoomResult
    // @return Returns a pointer to the room object without ID. You can get the ID after the
    // callback returns successfully.
    virtual IRailRoom* AsyncCreateRoom(const RoomOptions& options,
                        const RailString& room_name,
                        const RailString& user_data) = 0;

    // @desc Get a room object *synchronously*. AsyncOpenRoom is for asynchronous interface.
    // @param room_id The ID of the room
    // @param result The operation result. Will be kSuccess if the call succeeds.
    // @return The pointer to the room created to perform operations later
    virtual IRailRoom* OpenRoom(uint64_t room_id, RailResult* result) = 0;

    // @desc The *asynchronous* interface to open a room. OpenRoom is for *synchronous* interface.
    // Note: you cannot use the IRailRoom pointer until the callback OpenRoomResult
    // returns as kSuccess.
    // @param room_id The ID of the room
    // @param user_data Will be copied to the asynchronous result.
    // @callback OpenRoomResult
    // @return Returns the pointer to the room object for later operations.
    virtual IRailRoom* AsyncOpenRoom(uint64_t room_id, const RailString& user_data) = 0;

    // @desc Get a list of rooms asynchronously, filtered with 'filter'.
    // The result is sorted with the specified sorter with the range as [start_index, end_index).
    // @param start_index The first element is array[start_index]. The minimum possible is 0.
    // @param end_index The last element is array[end_index - 1].
    // @param sorter The rule to sort the result
    // @param filter The filter used to retrieve the rooms
    // @param user_data Will be copied to the asynchronous result.
    // @callback GetRoomListResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetRoomList(uint32_t start_index,
                        uint32_t end_index,
                        const RailArray<RoomInfoListSorter>& sorter,
                        const RailArray<RoomInfoListFilter>& filter,
                        const RailString& user_data) = 0;

    // @desc Get a room list filtered by room tags. You can set the tag of a room when creating it.
    // The interface AsyncSetRoomTag can directly set the tags for a room.
    // @param start_index The first element is array[start_index]. The minimum possible is 0.
    // @param end_index The last element is array[end_index - 1].
    // @param sorter The rule to sort the result
    // @param room_tags Room tags used to filter
    // @param user_data Will be copied to the asynchronous result.
    // @callback GetRoomListResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetRoomListByTags(uint32_t start_index,
                        uint32_t end_index,
                        const RailArray<RoomInfoListSorter>& sorter,
                        const RailArray<RailString>& room_tags,
                        const RailString& user_data) = 0;

    // @desc Get a list of rooms where the current player is located.
    // Under some circumstances, a player can be in multiple rooms for the same game.
    // @param user_data Will be copied to the asynchronous result.
    // @callback GetUserRoomListResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetUserRoomList(const RailString& user_data) = 0;
};

// @desc IRailRoom class
class IRailRoom : public IRailComponent {
  public:
    // @desc Join a room with a password. If no password is needed, just use "".
    // If OpenRoom succeeds, you can call AsyncJoinRoom to join the room
    // @param password The password needed to join the room. If no password is needed, just use ""
    // @param user_data Will be copied to the asynchronous result.
    // @callback JoinRoomResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncJoinRoom(const RailString& password,
                        const RailString& user_data) = 0;

    // @desc Get the ID of a room object.
    // @return uint64_t The room's ID
    virtual uint64_t GetRoomID() = 0;

    // @desc Get the name of a room.
    // @param name The room's name
    // @return Returns kSuccess on success
    virtual RailResult GetRoomName(RailString* name) = 0;

    // @desc Get the RailID of the room's owner.
    // @return The RaildID of the room's owner
    virtual RailID GetOwnerID() = 0;

    // @desc Returns whether there is a protection with password.
    // The interface was renamed from GetHasPassword() on 11/29/2018.
    // @return Returns true if the room has a password
    virtual bool HasPassword() = 0;

    // @desc Get the type of a room object.
    // @return The type of the room
    virtual EnumRoomType GetType() = 0;

    // @desc Get the number of members in the room.
    // It was renamed from GetNumOfMembers() on 12/04/2018.
    // @return The number of members in the room
    virtual uint32_t GetMembers() = 0;

    // @desc get a member's Rail ID by index.
    // @return The RailID of the room member with the specified index
    virtual RailID GetMemberByIndex(uint32_t index) = 0;

    // @desc Get a member's name by index.
    // @param index The index of the room member
    // @param name The name of the room member after the call succeeds
    // @return Returns kSuccess on success
    virtual RailResult GetMemberNameByIndex(uint32_t index, RailString* name) = 0;

    // @desc Get the max number of members allowed for the room.
    // @return The maximum of players allowed for the room
    virtual uint32_t GetMaxMembers() = 0;

    // @desc Leave the room *asynchrounously*. Please note it is NOT a synchronous interface.
    // It is recommended to leave the room first before destroying the room object.
    // @callback LeaveRoomResult
    virtual void Leave() = 0;

    // @desc Asynchrounously set a new owner for the room.
    // Used when the room owner wants to transfer the ownership to another room member.
    // If the call succeeds, all the room members will will receive `NotifyRoomOwnerChange`.
    // If the owner gets offline or exits the room, the server will pick a random
    // member as the new owner and notify all the room members.
    // The room's UI for each member will be refreshed to reflect the ownership change.
    // The interface was renamed from SetNewOwner on 11/29/2018.
    // Only the room's owner can call the interface.
    // @param new_owner_id RailID of the new room owner
    // @param user_data Will be copied to the asynchronous result.
    // @callback SetNewRoomOwnerResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncSetNewRoomOwner(const RailID& new_owner_id,
                        const RailString& user_data) = 0;

    // @desc Asynchronously get all the members in the room.
    // @param user_data Will be copied to the asynchronous result.
    // @callback GetRoomMembersResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetRoomMembers(const RailString& user_data) = 0;

    // @desc Get all the data related to the room.
    // @param user_data Will be copied to the asynchronous result.
    // @callback GetAllRoomDataResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetAllRoomData(const RailString& user_data) = 0;

    // @desc Asynchronously kick a member out of the room.
    // Only the room's owner can call the interface.
    // @param member_id The RailID of the member to be kicked out of the room
    // @param user_data Will be copied to the asynchronous result.
    // @callback KickOffMemberResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncKickOffMember(const RailID& member_id, const RailString& user_data) = 0;

    // @desc Asynchronously set the tag of the room.
    // You can use room_tag to put rooms into different groups. You can also filter rooms
    // according to 'room_tag_contained' in the structure RoomInfoListFilter.
    // Only the room's owner can call the interface.
    // @param room_tag The room tag to set
    // @param user_data Will be copied to the asynchronous result.
    // @callback SetRoomTagResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncSetRoomTag(const RailString& room_tag, const RailString& user_data) = 0;

    // @desc Asynchronously get the tag of the room.
    // @param user_data Will be copied to the asynchronous result.
    // @callback GetRoomTagResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetRoomTag(const RailString& user_data) = 0;

    // @desc Asynchronously set multiple metadata values of the room with 'key_values'.
    // The array size of 'key_values' cannot be larger than kRailRoomDataKeyValuePairsLimit.
    // Only the room's owner can call the interface.
    // @param key_values Keys and values data for the room.
    // @param user_data Will be copied to the asynchronous result.
    // @callback SetRoomMetadataResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncSetRoomMetadata(const RailArray<RailKeyValue>& key_values,
                        const RailString& user_data) = 0;

    // @desc Asynchronously get multiple metadata values of the room with keys.
    // The array size of 'keys' cannot be larger than kRailRoomDataKeyValuePairsLimit.
    // @param keys Keys for the values to retrieve
    // @param user_data Will be copied to the asynchronous result.
    // @callback GetRoomMetadataResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetRoomMetadata(const RailArray<RailString>& keys,
                        const RailString& user_data) = 0;

    // @desc Asynchronously clear the room's metadata with keys.
    // The array size of 'keys' cannot be larger than kRailRoomDataKeyValuePairsLimit.
    // Only the room's owner can call the interface.
    // @param keys for the values to clear
    // @param user_data Will be copied to the asynchronous result.
    // @callback ClearRoomMetadataResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncClearRoomMetadata(const RailArray<RailString>& keys,
                        const RailString& user_data) = 0;

    // @desc Asynchrounously set the specified member's metadata with 'key_values'.
    // If a room member is not the room owner, he/she can set only his/her own metadata.
    // The room owner can set any member's metadata.
    // The array size of 'key_values' cannot be larger than kRailRoomDataKeyValuePairsLimit.
    // @param member_id The member's ID
    // @param key_values The keys and values to be set for the member
    // @callback SetMemberMetadataResult.
    // @return Returns kSuccess on success
    virtual RailResult AsyncSetMemberMetadata(const RailID& member_id,
                        const RailArray<RailKeyValue>& key_values,
                        const RailString& user_data) = 0;

    // @desc Asynchronously get multiple metadata values of the specified member with 'keys'.
    // The array size of 'keys' cannot be larger than kRailRoomDataKeyValuePairsLimit.
    // @param member_id The member's RailID
    // @param keys The keys for the values to be retrieved
    // @param user_data Will be copied to the asynchronous result.
    // @callback GetMemberMetadataResult. Callback event ID: kRailEventRoomGetMemberMetadataResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncGetMemberMetadata(const RailID& member_id,
                        const RailArray<RailString>& keys,
                        const RailString& user_data) = 0;

    // @desc Send messages to other members in the room. 'message_type' is a
    // customized parameter. The message will be broadcast to all members except the sender
    // for 'remote_peer' as 0.
    // *NOTE: It is executed in unreliable mode. The maximum of 'data_len' is 1200 bytes.
    // @param remote_peer The destination member for the data to send to
    // @param data_buf The buffer that has the data bits
    // @param data_len The length of the data buffer
    // @param message_type The default type is 0. It will be sent to the destination player.
    // @return Returns kSuccess on success
    virtual RailResult SendDataToMember(const RailID& remote_peer,
                        const void* data_buf,
                        uint32_t data_len,
                        uint32_t message_type = 0) = 0;

    // @desc Set game server RailID for the room.
    // Only the room's owner can call the interface.
    // @param game_server_rail_id The RailID of the game server
    // @return Returns kSuccess on success
    virtual RailResult SetGameServerID(const RailID& game_server_rail_id) = 0;

    // @desc Get the game server's RailID for the room.
    // @return RailID The RailID of the game server
    virtual RailID GetGameServerID() = 0;

    // @desc Set the room's availability for people to join. A room can be joined by default.
    // Only the room's owner can call the interface.
    // @param is_joinable 'true' if players can join the room
    // @return Returns kSuccess on success
    virtual RailResult SetRoomJoinable(bool is_joinable) = 0;

    // @desc Check whether the room is available for players to join
    // The interface was renamed from GetRoomJoinable on 11/29/2018.
    // @return Returns 'true' if players can join the room
    virtual bool IsRoomJoinable() = 0;

    // @desc Get a list of friends in the room. For example, A, B, C and D are in the room,
    // B and C are A's friends and D is not. If A calls the interface, B and C will be returned.
    // @param friend_ids The RailIDs of the friends in the room
    // @return Returns kSuccess on success
    virtual RailResult GetFriendsInRoom(RailArray<RailID>* friend_ids) = 0;

    // @desc Check whether the user of the specified RailID is in the room.
    // @param user_rail_id RailID of the user to check
    // @return Returns true if the user is in the room
    virtual bool IsUserInRoom(const RailID& user_rail_id) = 0;

    // @desc Enable the basic voice support so that each member can talk to other room members
    // For more voice features, please check rail_voice_channel.h for the module RailVoiceChannel
    // @param enable 'true' if voice support is needed. By default, voice support is not enabled.
    // @return Returns kSuccess on success
    virtual RailResult EnableTeamVoice(bool enable) = 0;

    // @desc Asynchronously set the type of the room. After creating the room, you can
    // call this interface to change the type of the room.
    // Only the room's owner can call the interface.
    // @param room_type The room type to set
    // @param user_data Will be copied to the asynchronous result.
    // @callback SetRoomTypeResult. Callback
    // @return Returns kSuccess on success
    virtual RailResult AsyncSetRoomType(EnumRoomType room_type, const RailString& user_data) = 0;

    // @desc Asynchronously set the max number of players allowed for the room.
    // The interface can be called after the room is created successfully.
    // NOTE: 'max_member' MUST be larger than the number of existing room members.
    // Only the room's owner can call the interface.
    // @param max_member The maximum of players allowed in the room
    // @param user_data Will be copied to the asynchronous result.
    // @callback SetRoomMaxMemberResult
    // @return Returns kSuccess on success
    virtual RailResult AsyncSetRoomMaxMember(uint32_t max_member,
                        const RailString& user_data) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_ROOM_H
