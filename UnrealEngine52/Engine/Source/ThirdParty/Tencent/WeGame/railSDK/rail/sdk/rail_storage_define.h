// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.
#ifndef RAIL_SDK_RAIL_STORAGE_DEFINE_H
#define RAIL_SDK_RAIL_STORAGE_DEFINE_H

#include "rail/sdk/rail_event.h"
#include "rail/sdk/rail_user_space_define.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

struct RailSyncFileOption {
    RailSyncFileOption() { sync_file_not_to_remote = false; }

    bool sync_file_not_to_remote;
};

enum EnumRailStreamOpenFileType {
    kRailStreamOpenFileTypeReadOnly,
    kRailStreamOpenFileTypeTruncateWrite,  // if file exist, truncate content at first,
                                           // will create file if filename not exist
    kRailStreamOpenFileTypeAppendWrite,    // will create file if filename not exist
};

struct RailStreamFileOption {
    RailStreamFileOption() {
        unavaliabe_when_new_file_writing = false;
        open_type = kRailStreamOpenFileTypeReadOnly;
    }
    bool unavaliabe_when_new_file_writing;
    EnumRailStreamOpenFileType open_type;
};

struct RailListStreamFileOption {
    RailListStreamFileOption() {
        start_index = 0;
        num_files = 10;
    }
    uint32_t start_index;  //  list start index
    uint32_t num_files;    //   list how many files for this request
};

struct RailPublishFileToUserSpaceOption {
    RailPublishFileToUserSpaceOption() {
        type = kRailSpaceWorkTypeMod;
        level = kRailSpaceWorkShareLevelPublic;
    }

    EnumRailSpaceWorkType type;
    RailString space_work_name;
    RailString description;
    RailString preview_path_filename;
    RailString version;
    RailArray<RailString> tags;
    EnumRailSpaceWorkShareLevel level;
    RailKeyValue key_value;
};

struct RailStreamFileInfo {
    RailString filename;
    uint64_t file_size;
    RailStreamFileInfo() { file_size = 0; }
};

namespace rail_event {

struct AsyncQueryQuotaResult : public RailEvent<kRailEventStorageQueryQuotaResult> {
    AsyncQueryQuotaResult() {
        total_quota = 0;
        available_quota = 0;
    }

    uint64_t total_quota;
    uint64_t available_quota;
};

struct ShareStorageToSpaceWorkResult : public RailEvent<kRailEventStorageShareToSpaceWorkResult> {
    ShareStorageToSpaceWorkResult() {}

    SpaceWorkID space_work_id;
};

struct AsyncReadFileResult : public RailEvent<kRailEventStorageAsyncReadFileResult> {
    AsyncReadFileResult() {
        offset = 0;
        try_read_length = 0;
    }

    RailString filename;
    RailString data;
    int32_t offset;
    uint32_t try_read_length;
};

struct AsyncWriteFileResult : public RailEvent<kRailEventStorageAsyncWriteFileResult> {
    AsyncWriteFileResult() {
        offset = 0;
        try_write_length = 0;
        written_length = 0;
    }

    RailString filename;
    int32_t offset;
    uint32_t try_write_length;
    uint32_t written_length;
};

struct AsyncReadStreamFileResult : public RailEvent<kRailEventStorageAsyncReadStreamFileResult> {
    AsyncReadStreamFileResult() {
        offset = 0;
        try_read_length = 0;
    }

    RailString filename;
    RailString data;
    int32_t offset;
    uint32_t try_read_length;
};

struct AsyncWriteStreamFileResult : public RailEvent<kRailEventStorageAsyncWriteStreamFileResult> {
    AsyncWriteStreamFileResult() {
        offset = 0;
        try_write_length = 0;
        written_length = 0;
    }

    RailString filename;
    int32_t offset;
    uint32_t try_write_length;
    uint32_t written_length;
};

struct AsyncListFileResult : public RailEvent<kRailEventStorageAsyncListStreamFileResult> {
    AsyncListFileResult() {
        start_index = 0;
        try_list_file_num = 0;
        all_file_num = 0;
    }
    RailArray<RailStreamFileInfo> file_list;
    uint32_t start_index;        //  from RailListStreamFileOption's start_index
    uint32_t try_list_file_num;  //  from RailListStreamFileOption's num_files
    uint32_t all_file_num;  //  all the files' number that is compliance with the rule of contents
};

struct AsyncRenameStreamFileResult
    : public RailEvent<kRailEventStorageAsyncRenameStreamFileResult> {
    AsyncRenameStreamFileResult() {}

    RailString old_filename;
    RailString new_filename;
};

struct AsyncDeleteStreamFileResult
    : public RailEvent<kRailEventStorageAsyncDeleteStreamFileResult> {
    AsyncDeleteStreamFileResult() {}

    RailString filename;
};

}  // namespace rail_event

enum EnumRailStorageFileEnabledOS {
    kRailStorageSyncOSAll = 0,        // storage file is synced on all operation systems
    kRailStorageSyncOSWindows = 0x1,  // synced on windows
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_STORAGE_DEFINE_H
