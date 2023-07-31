// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_VOICE_CHANNEL_H
#define RAIL_SDK_RAIL_VOICE_CHANNEL_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/rail_voice_channel_define.h"

// @desc To use the IRailVoiceChannel APIs, you will first enable the feature by contacting the
// platform. In future, enabling the feature might be configurable on the developer portal.

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailVoiceChannel;
class IRailVoiceHelper {
  public:
    virtual ~IRailVoiceHelper() {}

    // create an empty channel and won't join it
    // trigger event CreateVoiceChannelResult
    // @param options creation options
    // @param channel_name used to show the readabe name when inviting others
    //        maintained by game itself
    // @param result return the request result, the final creation result will be sent by callback
    // @return IRailVoiceChannel NULL if failed
    virtual IRailVoiceChannel* AsyncCreateVoiceChannel(const CreateVoiceChannelOption& options,
                                const RailString& channel_name,
                                const RailString& user_data,
                                RailResult* result) = 0;

    // open an existing voice channel, but won't join it
    // @param voice_channel_id an existing voice channel id
    // @param channel_name used to show the readabe name when inviting others
    //        maintained by game itself
    // @param result return the open result
    // @return IRailVoiceChannel NULL if failed, it will still return the IRailVoiceChannel even if
    // the voice_channel_id is not exist, in this case, all function of IRailVoiceChannel will fail
    virtual IRailVoiceChannel* OpenVoiceChannel(const RailVoiceChannelID& voice_channel_id,
                                const RailString& channel_name,
                                RailResult* result) = 0;

    // return the current state of speaker
    virtual EnumRailVoiceChannelSpeakerState GetSpeakerState() = 0;

    // request to mute the speaker
    virtual RailResult MuteSpeaker() = 0;

    // request to resume the speaker
    virtual RailResult ResumeSpeaker() = 0;

    // -/////////////////////////////////////////-

    // data format kRailVoiceCaptureFormatPCM is data is 16-bit, signed integer, 11025Hz PCM.
    // with a 8 bytes header, offset 8 bytes to get the raw PCM or via DecodeVoice().
    // data format kRailVoiceCaptureFormatOPUS is encoded by opus.
    // kRailVoiceCaptureFormatPCM is the default format.
    // if provided a callback , callback will be call from another thread as soon as data is ready,
    // GetCapturedVoiceData() this situation will always get nothing.
    // You could get notify from the event VoiceDataCapturedEvent while there is voice data,
    // and then get the voice data via GetCapturedVoiceData().
    virtual RailResult SetupVoiceCapture(const RailVoiceCaptureOption& options,
                        RailCaptureVoiceCallback callback = NULL) = 0;

    // begin capture voice data from microphone.
    // duration must be multiple of 40 milliseconds and between 40 and 400.
    virtual RailResult StartVoiceCapturing(uint32_t duration_milliseconds = 120,
                        bool repeat = true) = 0;

    // if you are not using callback style then you should repeatly GetCapturedVoiceData()
    // until it returns kErrorVoiceCaptureNotRecording,
    // or refer to the is_last_package field in the event  VoiceDataCapturedEvent
    virtual RailResult StopVoiceCapturing() = 0;

    // Gets the latest voice data from the microphone.
    // It is recomanded that GetCapturedVoiceData() should be invoked each frame
    // or at least 5 times per second.
    // The buffer size will not be larger than 22 kilobytes.
    // So it is safely use a static buffer sized 22 kilobytes to improving the performance.
    // get the exactly size from encoded_length_written via setting buffer to NULL
    // for next call using dynamic alloc memory.
    // returning kErrorVoiceCaptureMoreData indicates that success and there is more data to get.
    virtual RailResult GetCapturedVoiceData(void* buffer,
                        uint32_t buffer_length,
                        uint32_t* encoded_bytes_written) = 0;

    // decode data to raw pcm data
    // Unencoded data is 16 - bit, signed integer, 11025Hz PCM format.
    // @encoded_buffer     : buffer got by GetCapturedVoiceData() or RailCaptureVoiceCallback
    // @encoded_length     : encoded_buffer's length (in bytes)
    // @pcm_buffer         : buffer to store the pcm audio data
    // @pcm_buffer_length  : buffer size of the pcm data, suggested size is 22 kilobytes.
    // @pcm_buffer_written : actually bytes used
    // if put pcm_buffer to null, pcm_buffer_written will indicate actually size will be used.
    virtual RailResult DecodeVoice(const void* encoded_buffer,
                        uint32_t encoded_length,
                        void* pcm_buffer,
                        uint32_t pcm_buffer_length,
                        uint32_t* pcm_buffer_written) = 0;

    // get the configuration of captured voice
    virtual RailResult GetVoiceCaptureSpecification(RailVoiceCaptureSpecification* spec) = 0;

    virtual RailResult EnableInGameVoiceSpeaking(bool can_speaking) = 0;

    // set the nickname used to show on the overlay UI
    // if you don't call it, the name will be the platform nickname
    // so you must call this if players have a different in-game nickname
    virtual RailResult SetPlayerNicknameInVoiceChannel(const RailString& nickname) = 0;

    // set the hot key of microphone, player could press hot key to speak in channel
    // if you don't call it, there will be a default hot key could be seen on overlay UI
    // @param push_to_talk_hot_key Virtual-Key Code on windows
    // @see https://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx
    virtual RailResult SetPushToTalkKeyInVoiceChannel(uint32_t push_to_talk_hot_key) = 0;

    // get the hot key of microphone
    // @return Virtual-Key Code on windows, zero if invalid
    virtual uint32_t GetPushToTalkKeyInVoiceChannel() = 0;

    // show the overlay ui provided by WeGame, players could do simple operations of voice features
    // it was not shown by default
    virtual RailResult ShowOverlayUI(bool show) = 0;

    virtual RailResult SetMicroVolume(uint32_t volume) = 0;

    virtual RailResult SetSpeakerVolume(uint32_t volume) = 0;
};

class IRailVoiceChannel : public IRailComponent {
  public:
    virtual ~IRailVoiceChannel() {}

    // get current channel's id, if this channle hasn't been created, it will return an invalid id
    virtual RailVoiceChannelID GetVoiceChannelID() = 0;

    // the name is same to the parameter passed by AsyncCreateVoiceChannel or OpenVoiceChannel
    virtual const RailString& GetVoiceChannelName() = 0;

    // if you don't want to listen events to get the result of async functions' callbacks
    // you could check this to get the real state of current player
    virtual EnumRailVoiceChannelJoinState GetJoinState() = 0;

    // request to join channel, will trigger callback JoinVoiceChannelResult to get the join result
    virtual RailResult AsyncJoinVoiceChannel(const RailString& user_data) = 0;

    // request to leave this channel, will trigger callback LeaveVoiceChannelResult with reason
    // kRailVoiceLeaveChannelReasonExitedBySelf
    virtual RailResult AsyncLeaveVoiceChannel(const RailString& user_data) = 0;

    // get current users in this channel
    // you could listen event VoiceChannelMemeberChangedEvent to get users list more effective
    virtual RailResult GetUsers(RailArray<RailID>* user_list) = 0;

    // request to invite other users to join in this channel
    // callback: VoiceChannelAddUsersResult
    // the other users would reject the invitation even callback return success
    // player be invited will receive the VoiceChannelInviteEvent and need join channel manually
    virtual RailResult AsyncAddUsers(const RailArray<RailID>& user_list,
                        const RailString& user_data) = 0;

    // request to kick out other users in this channel
    // callback: VoiceChannelRemoveUsersResult
    // users kicked will receive the LeaveVoiceChannelResult callback with reason
    // kRailVoiceLeaveChannelReasonKickedByPlayer
    // and VoiceChannelMemeberChangedEvent would be send if member changed
    // if current player remove himself or herself, the reason will be
    // kRailVoiceLeaveChannelReasonExitedBySelf
    // if current player is not the owner of this channel, the api will failed with
    // kErrorVoiceChannelNotTheChannelOwner
    virtual RailResult AsyncRemoveUsers(const RailArray<RailID>& user_list,
                        const RailString& user_data) = 0;

    // request to close this channel
    // will remove all users in this channel and leave
    // all users in this channel will receive the LeaveVoiceChannelResult callback with reason
    // kRailVoiceLeaveChannelReasonChannelClosed
    // Notice1: Realse a channel won't close this channel, it will only call LeaveVoiceChannel
    // Notice2: CloseChannel is not necessary when you don't need this channel
    // if current player is not the owner of this channel, the api will failed with
    // kErrorVoiceChannelNotTheChannelOwner
    virtual RailResult CloseChannel() = 0;

    // set whether current player could speak to others
    // pass true to this api just like pressing the hot key to allow player to speak,
    // and pass false to this api just like releasing the hot key to finish the speaking
    virtual RailResult SetSelfSpeaking(bool speaking) = 0;

    // get whether current player is speaking
    virtual bool IsSelfSpeaking() = 0;

    // set the users' speaking state to you, you could set users' speaking state incrementally
    // retrieve callback to get the real result
    // callback: AsyncSetUsersSpeakingState
    virtual RailResult AsyncSetUsersSpeakingState(
                        const RailArray<RailVoiceChannelUserSpeakingState>& users_speaking_state,
                        const RailString& user_data) = 0;

    // get all users' speaking state in this channel
    virtual RailResult GetUsersSpeakingState(
                        RailArray<RailVoiceChannelUserSpeakingState>* users_speaking_state) = 0;

    // get users currently speaking, you could listen the event
    // VoiceChannelSpeakingUsersChangedEvent to avoid call this api periodically
    // @speaking_users speaking users
    // @others convenient way to get the users not speaking
    virtual RailResult GetSpeakingUsers(RailArray<RailID>* speaking_users,
                        RailArray<RailID>* not_speaking_users) = 0;

    // check whether current user is the owner of this channel
    // only the owner could remove users or close the channel
    // if others try to remove users or close channel, the api will return the error code
    // kErrorVoiceChannelNotTheChannelOwner
    virtual bool IsOwner() = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_VOICE_CHANNEL_H
