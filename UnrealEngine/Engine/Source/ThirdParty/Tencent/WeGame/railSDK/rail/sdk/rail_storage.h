// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_STORAGE_H
#define RAIL_SDK_RAIL_STORAGE_H

#include "rail/sdk/base/rail_component.h"
#include "rail/sdk/base/rail_string.h"
#include "rail/sdk/rail_result.h"
#include "rail/sdk/rail_storage_define.h"

// @desc Two methods can be used to support cloud save
// 1. Simply configure a save subfolder on Developer Portal with no need to integrate Rail SDK.
// Save files in the configured folder will be synchronized automatically. You will need to
// handle save file conflicts for different players on the same computer.
// 2. Integrate Rail SDK for save file I/O. The SDK will automatically create an
// independent subfolder for each player.
// Two sets of interfaces, IRailStreamFile and IRailFile are provided in the SDK
// a) IRailFile is recommended for games that do not require network connection
// b) Both IRailFile and IRailStreamFile can be used for games that require network connection.
// IRailStreamFile will only save files on the cloud. If your game requires network connection and
// you want to avoid the risk of anyone meddling with the local files, just use IRailStreamFile.

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

class IRailFile;
class IRailStreamFile;
// storage helper singleton
class IRailStorageHelper {
  public:
    virtual ~IRailStorageHelper() {}

    // @desc Open a file and get a file pointer.
    // When the file is no longer needed, please call Release()
    // @param filename File's name. You may include a relative path.
    // @param result Will be kSuccess on success
    // @return Pointer to the file opened
    virtual IRailFile* OpenFile(const RailString& filename, RailResult* result = NULL) = 0;

    // @desc Create a file and get a pointer to the file. Existing file of the same name will be
    // overwritten. When the file is no longer needed, please call Release().
    // @param filename File's name. You may include a relative path.
    // @param result Will be kSuccess on success
    // @return Pointer to the file created
    virtual IRailFile* CreateFile(const RailString& filename, RailResult* result = NULL) = 0;

    // @desc Check whether a file exists. A relative path might be included in 'filename'
    // @param filename File's name
    // @return Returns true if the file exists
    virtual bool IsFileExist(const RailString& filename) = 0;

    // @desc Get a list of all the files at local.
    // @param filelist All the files at local
    // @return Returns true if the list is retrieved successfully
    virtual bool ListFiles(RailArray<RailString>* filelist) = 0;

    // @desc Delete the file of name 'filename'
    // @param filename File's name
    // @return Returns kSuccess on success
    virtual RailResult RemoveFile(const RailString& filename) = 0;

    // @desc Check whether the file has been synchronized with the version on the cloud
    // @param filename File's name
    // @return True if the synchronization is done
    virtual bool IsFileSyncedToCloud(const RailString& filename) = 0;

    // @desc Get a file's timestamp
    // @param filename File's name
    // @param time_stamp File's timestamp to be retrieved
    // @return Returns kSuccess on success
    virtual RailResult GetFileTimestamp(const RailString& filename, uint64_t* time_stamp) = 0;

    // @desc Get the number of save files at local
    // @return The number of save files at local
    virtual uint32_t GetFileCount() = 0;

    // @desc Get file info for the file of index 'file_index' in the list of all save files
    // @param file_index The index
    // @param filename Pointer to the filename
    // @param file_size Size of the file
    // @return Returns kSuccess on success
    virtual RailResult GetFileNameAndSize(uint32_t file_index,
                        RailString* filename,
                        uint64_t* file_size) = 0;

    // @desc Get the quota the player has got for save files. The quota for each game is 100MB
    // for a player
    // @return Returns kSuccess on success
    virtual RailResult AsyncQueryQuota() = 0;

    // @desc Configure whether or not you would like to synchronize the local file to the cloud
    // @param filename Name of the file
    // @param The option used for synchronization
    // @return Returns kSuccess on success
    virtual RailResult SetSyncFileOption(const RailString& filename,
                        const RailSyncFileOption& option) = 0;

    // @desc Check whether cloud save is enabled on Developer Portal
    // @return True if cloud save is enabled
    virtual bool IsCloudStorageEnabledForApp() = 0;

    // @desc Check whether the current player has enabled cloud save
    // By default the feature is enabled, but the player can manually disable it on the client.
    // @return Returns true is player has not disabled the feature.
    virtual bool IsCloudStorageEnabledForPlayer() = 0;

    // @desc Used for player modifications. This will also consume the player space quota as
    // other save files
    // @param option See rail_storage_define.h for the definition
    // @param user_data Will be copied to the asynchronous result.
    // @return Returns kSuccess on success
    virtual RailResult AsyncPublishFileToUserSpace(const RailPublishFileToUserSpaceOption& option,
                        const RailString& user_data) = 0;

    // @desc Open a stream file to read or write. If the object is no longer needed, just
    // call Release().
    // If the file already exists, a new file will be created with the name as 'filename'
    // @param filename File's name, which may include a relative path
    // @param option See rail_storage_define.h for definition
    // @param result Will be kSuccess on success
    // @return Pointer to the stream file created. Returns NULL if it fails.
    virtual IRailStreamFile* OpenStreamFile(const RailString& filename,
                                const RailStreamFileOption& option,
                                RailResult* result = NULL) = 0;

    // @desc Get a list of files on the cloud
    // @param contents File name filter. For example, "*", "*.dat" and "sav*" etc.
    // @param option See rail_storage_define.h for details
    // @param user_data Will be copied to the asynchronous result.
    // @return Returns kSuccess on success
    virtual RailResult AsyncListStreamFiles(const RailString& contents,
                        const RailListStreamFileOption& option,
                        const RailString& user_data) = 0;

    // @desc Rename a file
    // @param old_filename The original file name
    // @param new_filename The new file name
    // @param user_data Will be copied to the asynchronous result.
    // @return Returns kSuccess on success
    virtual RailResult AsyncRenameStreamFile(const RailString& old_filename,
                        const RailString& new_filename,
                        const RailString& user_data) = 0;

    // @desc Delete a file on the cloud
    // @param filename Name of the file to be deleted
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncDeleteStreamFile(const RailString& filename,
                        const RailString& user_data) = 0;

    // @desc Get types of operating systems that supports IRailFile
    // For now, only Windows is supported. So, no need to use the interface at the moment.
    // @return Returns enumerate value of type EnumRailStorageFileEnabledOS
    virtual uint32_t GetRailFileEnabledOS(const RailString& filename) = 0;

    // @desc Enable operation systems that support IRailFile. Since Rail SDK only supports
    // Windows now, there is no need to use this interface now.
    // @param filename File name for the file that needs synchronization on the cloud
    // @sync_os See rail_storage_define.h for the definition
    // @return Returns kSuccess on success
    virtual RailResult SetRailFileEnabledOS(const RailString& filename,
                        EnumRailStorageFileEnabledOS sync_os) = 0;
};

// IRailFile class
class IRailFile : public IRailComponent {
  public:
    virtual ~IRailFile() {}

    // @desc Get the file name for this file object
    // @return Returns file's name
    virtual const RailString& GetFilename() = 0;

    // @desc Read file content to 'buff'
    // @param buff Data in the file will be copied to 'buff'
    // @param bytes_to_read Bytes to read
    // @param result Will be kSuccess if the bytes are read successfully
    // @return Bytes actually read
    virtual uint32_t Read(void* buff, uint32_t bytes_to_read, RailResult* result = NULL) = 0;

    // @desc Write to the file
    // @param buff Data to write to the file
    // @param bytes_to_write Bytes intended to write
    // @return Bytes actually written
    virtual uint32_t Write(const void* buff,
                        uint32_t bytes_to_write,
                        RailResult* result = NULL) = 0;

    // @desc The asynchronous version of the interface Read
    // @param bytes_to_read Bytes to read
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncRead(uint32_t bytes_to_read, const RailString& user_data) = 0;

    // @desc The acynchronous version of the interface Write
    // @param bytes_to_write Bytes intended to write
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncWrite(const void* buffer,
                        uint32_t bytes_to_write,
                        const RailString& user_data) = 0;

    // @desc Get the file size
    // @return Returns the file's size
    virtual uint32_t GetSize() = 0;

    // @desc Close the file
    virtual void Close() = 0;
};

// IRailStreamFile class
class IRailStreamFile : public IRailComponent {
  public:
    virtual ~IRailStreamFile() {}

    // @desc Get the file's name
    // @return The file's name
    virtual const RailString& GetFilename() = 0;

    // @desc Read file content from the cloud
    // @param offset Set the beginning position in the file to start reading
    // @param bytes_to_read Bytes to read
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncRead(int32_t offset,
                        uint32_t bytes_to_read,
                        const RailString& user_data) = 0;

    // @desc Write to the file with data in 'buff'
    // @param bytes_to_write Bytes to be written to the file
    // @param user_data Will be copied to the asynchronous result
    // @return Returns kSuccess on success
    virtual RailResult AsyncWrite(const void* buff,
                        uint32_t bytes_to_write,
                        const RailString& user_data) = 0;

    // @desc Get the file size
    // @return The file size in bytes
    virtual uint64_t GetSize() = 0;

    // @desc Close file. If AsyncWrite was called previously, you will need to wait till it returns
    // successfully at the event kRailEventStorageAsyncWriteFileResult.
    // Release() needs to be called after Close() if the file object is no longer needed.
    // @return Returns kSuccess on success
    virtual RailResult Close() = 0;

    // @desc Cancel sending the remaining data to cloud if not all data are sent, and close
    // the file.
    virtual void Cancel() = 0;
};

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_RAIL_STORAGE_H
