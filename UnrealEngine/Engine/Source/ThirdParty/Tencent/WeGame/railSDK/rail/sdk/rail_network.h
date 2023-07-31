// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_NETWORK_H
#define RAIL_SDK_RAIL_NETWORK_H

#include "rail/sdk/rail_network_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

// IRailNetwork has a collection of network APIs to support P2P communications.
// The platform may use P2SP technologies for acceleration, which is transparent to developers.

class IRailNetwork {
  public:
    // @desc Accept a network session request from a remote player in a same game. When another
    // player requests a network session with you, you will receive the callback event
    // CreateSessionRequest with info of the remote player's RailID and the local player's RailID.
    // Only after calling AcceptSessionRequest, can you establish a network session with the peer.
    // @param local_peer RailID of the local user
    // @param remote_peer RailID of the remote player
    // @return Returns kSuccess on success
    virtual RailResult AcceptSessionRequest(const RailID& local_peer,
                        const RailID& remote_peer) = 0;

    // @desc Send data to the specified player in a same game. To create a network session with a
    // peer player in a same game, you can directly call SendData to send data to the peer. If the
    // request to establish a network session in a same game fails, you will receive the callback
    // event CreateSessionFailed.
    // *NOTE: this API is in unreliable mode. The maximum of 'data_len' is 1200 bytes.
    // @param local_peer RailID of the local user
    // @param remote_peer RailID of the remote player
    // @param data_buf Pointer to the data buffer to read
    // @param data_len The length of the data to read
    // @param message_type Type of the message to read. The default is 0.
    // @return Returns kSuccess on success
    virtual RailResult SendData(const RailID& local_peer,
                        const RailID& remote_peer,
                        const void* data_buf,
                        uint32_t data_len,
                        uint32_t message_type = 0) = 0;

    // @desc Send data in reliable mode. The maximum of 'data_len' is 1M bytes.
    // CloseSession should be called after the session ends.
    // @param local_peer RailID of the local user
    // @param remote_peer RailID of the remote player
    // @param data_buf Pointer to the data buffer
    // @param data_len The length of data to send
    // @param message_type Type of message to read. The default is 0.
    // @return Returns kSuccess on success
    virtual RailResult SendReliableData(const RailID& local_peer,
                        const RailID& remote_peer,
                        const void* data_buf,
                        uint32_t data_len,
                        uint32_t message_type = 0) = 0;

    // @desc Checks whether readable data is available. You need to call the 'IsDataReady' in
    // each frame cycle to constantly check whether you have the received the data.
    // @param local_peer For output. RailID of the local player. When the data is ready, you can
    // call ReadData to get the remote player's ID. The parameter 'local_peer' for the function
    // ReadData is for input rather than output in IsDataReady.
    // @param data_len length of data to send
    // @param message_type Type of message with the default as NULL
    // @return Returns true if the data exists
    virtual bool IsDataReady(RailID* local_peer,
                    uint32_t* data_len,
                    uint32_t* message_type = NULL) = 0;

    // @desc Reads data sent by a remote player.
    // @param local_peer RailID of the local user
    // @param remote_peer RailID of the remote player
    // @param data_buf Pointer to the data buffer to read
    // @param data_len The length of the data to read
    // @param message_type Type of the message to read. The default is 0.
    // @return Returns kSuccess on success
    virtual RailResult ReadData(const RailID& local_peer,
                        RailID* remote_peer,
                        void* data_buf,
                        uint32_t data_len,
                        uint32_t message_type = 0) = 0;

    // @desc Blocks a certain type of message.
    // @param local_peer RailID of the local user
    // @param message_type The message type to be blocked.
    // @return Returns kSuccess on success
    virtual RailResult BlockMessageType(const RailID& local_peer, uint32_t message_type) = 0;

    // @desc Restores receiving a certain type of messages.
    // @param local_peer RailID of the local user
    // @param message_type The type of messages to restore receiving
    // @return Returns kSuccess on success
    virtual RailResult UnblockMessageType(const RailID& local_peer, uint32_t message_type) = 0;

    // @desc When a session ends, you should call this API. Otherwise the next next session may
    // not work as expected.
    // @param local_peer RailID of the local user
    // @param remote_peer RailID of the remote user
    // @return Returns kSuccess on success
    virtual RailResult CloseSession(const RailID& local_peer, const RailID& remote_peer) = 0;

    // @desc Resolves the hostname to get a list of IP addresses
    // @param domain Name of the domain to be resolved
    // @param ip_list A list of IP addresses resolved from the given domain name
    // @return Returns kSuccess on success
    virtual RailResult ResolveHostname(const RailString& domain,
                        RailArray<RailString>* ip_list) = 0;

    // @desc Checks session status with a remote peer
    // @param remote_peer RailID of the remote peer
    // @param session_state The session's state
    // @return Returns kSuccess on success
    virtual RailResult GetSessionState(const RailID& remote_peer,
                        RailNetworkSessionState* session_state) = 0;

    // @desc Allow users decide whether to use server relay. This must be called before
    // calling AcceptSessionRequest or AcceptRawSessionRequest to take effect, and can
    // only be called by the receiver.
    // @param forbid_relay Is session transform without server relay. Param forbid_relay
    // is set as false by default. If forbid_relay is true, sessions created afterwards
    // will transform without server relay. If forbid_relay is false or this function not
    // called, P2SP will decide whether to transform with server.
    // @return Returns kSuccess on success
    virtual RailResult ForbidSessionRelay(bool forbid_relay) = 0;

    // @desc Send data to the specified player in a specified game. To create a network session
    // with a peer player, you can directly call SendRawData to send data to the peer. If the
    // request to establish a network session between different games fails, you will receive
    // the callback event NetworkCreateRawSessionFailed. If the request to establish a network
    // session in a same game fails, you will receive the callback event CreateSessionFailed
    // @param local_peer RailID of the local user
    // @param remote_game_peer RailID and RailGameID of the remote player
    // @param data_buf Pointer to the data buffer to read
    // @param data_len The length of the data to read
    // @param reliable The data send mode. If it's value is true, the maximum of 'data_len'
    // is 1M bytes; otherwise, the maximum of 'data_len' is 1200 bytes.
    // @param message_type Type of the message to read. The default is 0.
    // @return Returns kSuccess on success
    virtual RailResult SendRawData(const RailID& local_peer,
                        const RailGamePeer& remote_game_peer,
                        const void* data_buf,
                        uint32_t data_len,
                        bool reliable,
                        uint32_t message_type) = 0;

    // @desc Accept a network session request from a remote player between different games.
    // When another player in another game requests a network session with you, you will receive
    // the callback event NetworkCreateRawSessionRequest with info of the remote player's RailID,
    // GameID and the local player's RailID. Only after calling AcceptRawSessionRequest, can you
    // establish a network session with the peer between different games.
    // @param local_peer RailID of the local user
    // @param remote_game_peer RailID and RailGameID of the remote player
    // @return Returns kSuccess on success
    virtual RailResult AcceptRawSessionRequest(const RailID& local_peer,
                        const RailGamePeer& remote_game_peer) = 0;

    // @desc Reads data sent by a remote player.
    // @param local_peer RailID of the local user
    // @param remote_game_peer RailID and RailGameID of the remote player
    // @param data_buf Pointer to the data buffer to read
    // @param data_len The length of the data to read
    // @param message_type Type of the message to read. The default is 0.
    // @return Returns kSuccess on success
    virtual RailResult ReadRawData(const RailID& local_peer,
                        RailGamePeer* remote_game_peer,
                        void* data_buf,
                        uint32_t data_len,
                        uint32_t message_type = 0) = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_NETWORK_H
