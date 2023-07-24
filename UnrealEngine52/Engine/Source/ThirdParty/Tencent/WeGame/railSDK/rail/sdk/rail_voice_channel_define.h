// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_VOICE_CHANNEL_DEFINE_H
#define RAIL_SDK_RAIL_VOICE_CHANNEL_DEFINE_H

#include "rail/sdk/base/rail_define.h"
#include "rail/sdk/rail_event.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

enum EnumRailVoiceChannelSpeakerState {
    kRailVoiceChannelSpeakerStateUnknown = 0,
    kRailVoiceChannelSpeakerStateMuted = 1,
    kRailVoiceChannelSpeakerStateSpeakable = 2,
};

enum EnumRailVoiceCaptureFormat {
    kRailVoiceCaptureFormatPCM = 0,
    kRailVoiceCaptureFormatOPUS = 1,
};

enum EnumRailVoiceCaptureChannel {
    kRailVoiceCaptureChannelMono = 0,
    kRailVoiceCaptureChannelStereo = 1,
};

enum EnumRailVoiceChannelJoinState {
    kRailVoiceChannelJoinStateUnknown = 0,
    kRailVoiceChannelJoinStateCreating = 1,      // haven't joined
    kRailVoiceChannelJoinStateCreated = 2,       // haven't joined
    kRailVoiceChannelJoinStateCreateFailed = 3,  // haven't joined
    kRailVoiceChannelJoinStateJoining = 4,
    kRailVoiceChannelJoinStateJoined = 5,
    kRailVoiceChannelJoinStateJoinFailed = 6,
    kRailVoiceChannelJoinStateLeaving = 7,
    kRailVoiceChannelJoinStateLeft = 8,
    kRailVoiceChannelJoinStateLeaveFailed = 9,
    kRailVoiceChannelJoinStateLostConnection = 10,
};

enum EnumRailVoiceLeaveChannelReason {
    kRailVoiceLeaveChannelReasonUnknown = 0,
    kRailVoiceLeaveChannelReasonChannelClosed = 1,
    kRailVoiceLeaveChannelReasonKickedByPlayer = 2,
    kRailVoiceLeaveChannelReasonExitedBySelf = 3,
};

enum EnumRailVoiceChannelUserSpeakingLimit {
    kRailVoiceChannelUserSpeakingStateNoLimit = 0,          // could speak to all
    kRailVoiceChannelUserSpeakingStateCannotSpeakToMe = 1,  // could not speak to me
};

struct RailVoiceCaptureOption {
    RailVoiceCaptureOption() { voice_data_format = kRailVoiceCaptureFormatPCM; }

    EnumRailVoiceCaptureFormat voice_data_format;
};

struct RailVoiceCaptureSpecification {
    RailVoiceCaptureSpecification() {
        capture_format = kRailVoiceCaptureFormatPCM;
        bits_per_sample = 16;
        samples_per_second = 11025;
        channels = kRailVoiceCaptureChannelMono;
    }
    EnumRailVoiceCaptureFormat capture_format;
    uint32_t bits_per_sample;
    uint32_t samples_per_second;
    EnumRailVoiceCaptureChannel channels;
};

typedef void (*RailCaptureVoiceCallback)(EnumRailVoiceCaptureFormat fmt,
                                        bool is_last_package,
                                        const void* encoded_buffer,
                                        uint32_t encoded_length);

struct CreateVoiceChannelOption {
    CreateVoiceChannelOption() { join_channel_after_created = false; }
    // if you set it to true
    // you will firstly receive CreateVoiceChannelResult, if result is kSuccess
    // you will then receive JoinVoiceChannelResult
    bool join_channel_after_created;
};

struct RailVoiceChannelUserSpeakingState {
    RailVoiceChannelUserSpeakingState() {
        speaking_limit = kRailVoiceChannelUserSpeakingStateNoLimit;
    }

    RailID user_id;
    EnumRailVoiceChannelUserSpeakingLimit speaking_limit;
};

namespace rail_event {

struct CreateVoiceChannelResult : public RailEvent<kRailEventVoiceChannelCreateResult> {
    RailVoiceChannelID voice_channel_id;
};

struct JoinVoiceChannelResult : public RailEvent<kRailEventVoiceChannelJoinedResult> {
    RailVoiceChannelID voice_channel_id;
    // this will store the id of voice channel player currently joined now
    // if result is kErrorVoiceChannelAlreadyJoinedAnotherChannel
    // you need leave this channel before joining other channels
    RailVoiceChannelID already_joined_channel_id;
};

struct LeaveVoiceChannelResult : public RailEvent<kRailEventVoiceChannelLeaveResult> {
    LeaveVoiceChannelResult() { reason = kRailVoiceLeaveChannelReasonUnknown; }
    RailVoiceChannelID voice_channel_id;
    EnumRailVoiceLeaveChannelReason reason;
};

struct VoiceChannelAddUsersResult : public RailEvent<kRailEventVoiceChannelAddUsersResult> {
    RailVoiceChannelID voice_channel_id;
    RailArray<RailID> success_ids;
    RailArray<RailID> failed_ids;
};

struct VoiceChannelRemoveUsersResult : public RailEvent<kRailEventVoiceChannelRemoveUsersResult> {
    RailVoiceChannelID voice_channel_id;
    RailArray<RailID> success_ids;
    // rail sdk won't check whether the id is in this channel
    // so even the id is not in this channel, AsyncRemoveUsers also mark it as success id
    RailArray<RailID> failed_ids;
};

struct VoiceChannelInviteEvent : public RailEvent<kRailEventVoiceChannelInviteEvent> {
    RailString inviter_name;
    RailString channel_name;
    RailVoiceChannelID voice_channel_id;
};

struct VoiceChannelMemeberChangedEvent
    : public RailEvent<kRailEventVoiceChannelMemberChangedEvent> {
    RailVoiceChannelID voice_channel_id;
    RailArray<RailID> member_ids;
};

// player may change the hot key on overlay UI
// game should listen to this event if game want to handle it
struct VoiceChannelPushToTalkKeyChangedEvent
    : public RailEvent<kRailEventVoiceChannelPushToTalkKeyChangedEvent> {
    VoiceChannelPushToTalkKeyChangedEvent() { push_to_talk_hot_key = 0; }
    uint32_t push_to_talk_hot_key;
};

struct VoiceChannelUsersSpeakingStateChangedEvent
    : public RailEvent<kRailEventVoiceChannelUsersSpeakingStateChangedEvent> {
    RailVoiceChannelID voice_channel_id;
    RailArray<RailVoiceChannelUserSpeakingState> users_speaking_state;
};

struct VoiceChannelSpeakingUsersChangedEvent
    : public RailEvent<kRailEventVoiceChannelSpeakingUsersChangedEvent> {
    RailVoiceChannelID voice_channel_id;
    RailArray<RailID> speaking_users;
    RailArray<RailID> not_speaking_users;
};

struct VoiceDataCapturedEvent : public RailEvent<kRailEventVoiceChannelDataCaptured> {
    VoiceDataCapturedEvent() { is_last_package = false; }
    bool is_last_package;
};
}  // namespace rail_event

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_VOICE_CHANNEL_DEFINE_H
