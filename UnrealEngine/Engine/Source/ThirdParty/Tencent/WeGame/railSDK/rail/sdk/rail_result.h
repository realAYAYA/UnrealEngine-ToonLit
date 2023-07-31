// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

#ifndef RAIL_SDK_RAIL_RESULT_H
#define RAIL_SDK_RAIL_RESULT_H

namespace rail {

// error codes of result
enum RailResult {
    kSuccess = 0,  // success.

    //
    // 1000000 - 1000999 will be the network error code.
    //

    kFailure = 1010001,                // general failure error code.
    kErrorInvalidParam = 1010002,      // input parameter is invalid.
    kErrorImageNotFound = 1010003,     // image not found.
    kErrorBufferTooSmall = 1010004,    // input buffer is too small.
    kErrorNetworkError = 1010005,      // network is unavailable.
    kErrorUnimplemented = 1010006,     // called interface is not implemented.
    kErrorInvalidCustomKey = 1010007,  // custom key used in game can not start with "rail_" prefix.
    kErrorClientInOfflineMode = 1010008,  // client is in off-line mode, all asynchronous interface
                                          // will return this error code.
    kErrorParameterLengthTooLong = 1010009,       // parameter length is too much long.
    kErrorWebApiKeyNoAccessOnThisGame = 1010010,  // Web API key has no access to this game.
    kErrorOperationTimeout = 1010011,             // operations timeout, might caused by network
                                                  // issues.
    kErrorServerResponseInvalid = 1010012,        // response from server is invalid.
    kErrorServerInternalError = 1010013,          // server internal error.

    // rail storage
    kErrorFileNotFound = 1011000,              // cant not find file.
    kErrorAccessDenied = 1011001,              // cant access file.
    kErrorOpenFileFailed = 1011002,            // cant open file.
    kErrorCreateFileFailed = 1011003,          // create file failed.
    kErrorReadFailed = 1011004,                // read file failed.
    kErrorWriteFailed = 1011005,               // write file failed.
    kErrorFileDestroyed = 1011006,             // file have been destroyed.
    kErrorFileDelete = 1011007,                // delete file failed.
    kErrorFileQueryIndexOutofRange = 1011008,  // param index of GetFileNameAndSize out of range.
    kErrorFileAvaiableQuotaMoreThanTotal = 1011009,  // cloud file size bigger than quota.
    kErrorFileGetRemotePathError = 1011010,      // get local cloud path failed when query quota.
    kErrorFileIllegal = 1011011,                 // file is illegal.
    kErrorStreamFileWriteParamError = 1011012,   // passing wrong param to AsyncWrite in StreamFile
    kErrorStreamFileReadParamError = 1011013,    // passing wrong param to AsyncRead in StreamFile.
    kErrorStreamCloseErrorIOWritting = 1011014,  // close writing stream file failed.
    kErrorStreamCloseErrorIOReading = 1011015,   // close reading stream file failed.
    kErrorStreamDeleteFileOpenFileError = 1011016,  // open stream file failed when delete stream
                                                    // file.
    kErrorStreamRenameFileOpenFileError = 1011017,  // open stream file failed when Rename stream
                                                    // file.
    kErrorStreamReadOnlyError = 1011018,            // write to a stream file when the stream file
                                                    // is read only.

    kErrorStreamCreateFileRemoveOldFileError = 1011019,  // delete an old stream file when truncate
                                                         // a stream file.
    kErrorStreamCreateFileNameNotAvailable = 1011020,    // file name size bigger than 256 when open
                                                         // stream file.
    kErrorStreamOpenFileErrorCloudStorageDisabledByPlatform = 1011021,  // app cloud storage is
                                                                        // disabled.
    kErrorStreamOpenFileErrorCloudStorageDisabledByPlayer = 1011022,    // player's cloud storage is
                                                                        // disabled.
    kErrorStoragePathNotFound = 1011023,                 // file path is not available.
    kErrorStorageFileCantOpen = 1011024,                 // cant open file.
    kErrorStorageFileRefuseAccess = 1011025,             // cant open file because of access.
    kErrorStorageFileInvalidHandle = 1011026,            // read or write to a file that file
                                                         // handle is not available.
    kErrorStorageFileInUsingByAnotherProcess = 1011027,  // cant open file because it's using by
                                                         // another process.
    kErrorStorageFileLockedByAnotherProcess = 1011028,   // cant lock file because it's locked by
                                                         // another process.
    kErrorStorageFileWriteDiskNotEnough = 1011029,       // cant write to disk because it's not
                                                         // enough.
    kErrorStorageFileCantCreateFileOrPath = 1011030,     // path is not available when create file.

    // room
    kErrorRoomCreateFailed = 1012000,        // create room failed.
    kErrorKickOffFailed = 1012001,           // kickoff failed.
    kErrorKickOffPlayerNotInRoom = 1012002,  // the player not in room.
    kErrorKickOffNotRoomOwner = 1012003,     // only the room owner can kick off others.
    kErrorKickOffPlayingGame = 1012004,      // same game can't kick off player who is playing game.
    kErrorRoomServerRequestInvalidData = 1012005,     // the request parameter is invalid.
    kErrorRoomServerConnectTcaplusFail = 1012006,     // the back end server connects db failed.
    kErrorRoomServerConnectTcaplusTimeOut = 1012007,  // the back end server connects db timeout.
    kErrorRoomServerWrongDataInTcaplus = 1012008,     // the data in back end db is wrong.
    kErrorRoomServerNoDataInTcaplus = 1012009,        // no related data found in back end db.
    kErrorRoomServerAllocateRoomIdFail = 1012010,     // allocate room id failed when creating room.
    kErrorRoomServerCreateGroupInImCloudFail = 1012011,  // allocate room resource failed when
                                                         // creating room.

    kErrorRoomServerUserAlreadyInGame = 1012012,       // player already join one room of the game.
    kErrorRoomServerQueryResultEmpty = 1012013,        // the query result is empty.
    kErrorRoomServerRoomFull = 1012014,                // the joined room is full.
    kErrorRoomServerRoomNotExist = 1012015,            // the room doesn't exist.
    kErrorRoomServerUserAlreadyInRoom = 1012016,       // player already join the room.
    // kErrorRoomServerRoomGroupFull = 1012017,        // not used.
    kErrorRoomServerQueryRailIdServiceFail = 1012018,  // query rail id failed.
    kErrorRoomServerImCloudFail = 1012019,             // system error.
    kErrorRoomServerPbSerializeFail = 1012020,         // system error.
    kErrorRoomServerDirtyWord = 1012021,               // the input content includes dirty word.
    kErrorRoomServerNoPermission = 1012022,            // no permission.
    kErrorRoomServerLeaveUserNotInRoom = 1012023,      // the leaving player is not in the room.
    kErrorRoomServerDestroiedRoomNotExist = 1012024,   // the room to destroy doesn't exist.
    kErrorRoomServerUserIsNotRoomMember = 1012025,     // the player is not the room member.
    kErrorRoomServerLockFailed = 1012026,              // system error.
    kErrorRoomServerRouteMiss = 1012027,               // system error.
    kErrorRoomServerRetry = 1012028,                   // need retry.
    kErrorRoomSendDataNotImplemented = 1012029,        // this method is not supported.
    kErrorRoomInvokeFailed = 1012030,                  // network error.
    kErrorRoomServerPasswordIncorrect = 1012031,       // room password doesn't match.
    kErrorRoomServerRoomIsNotJoinable = 1012032,       // the room is not joinable.

    // stats
    kErrorStats = 1013000,
    kErrorStatsDontSetOtherPlayerStat = 1013001,  // can't set other player's statistics.

    // achievement
    kErrorAchievement = 1014000,
    kErrorAchievementOutofRange = 1014001,        // not any more achievement.
    kErrorAchievementNotMyAchievement = 1014002,  // can't set other player's achievement.

    // leaderboard
    kErrorLeaderboard = 1015000,
    kErrorLeaderboardNotExists = 1015001,      // leaderboard not exists.
    kErrorLeaderboardNotBePrepared = 1015002,  // leaderboard not be prepared, call
                                               // IRailLeaderboard::AsyncGetLeaderboard first.
    kErrorLeaderboardCreattionNotSupport = 1015003,  // not support AsyncCreateLeaderboard
                                                     // because your game doesn't configure
                                                     // leaderboard.

    // assets
    kErrorAssets = 1016000,
    kErrorAssetsPending = 1016001,                    // assets still in pending.
    kErrorAssetsOK = 1016002,
    kErrorAssetsExpired = 1016003,                    // assets expired.
    kErrorAssetsInvalidParam = 1016004,               // passing invalid param.
    kErrorAssetsServiceUnavailable = 1016005,         // service unavailable.
    kErrorAssetsLimitExceeded = 1016006,              // assets exceed limit.
    kErrorAssetsFailed = 1016007,
    kErrorAssetsRailIdInvalid = 1016008,              // rail id invalid.
    kErrorAssetsGameIdInvalid = 1016009,              // game id invalid.
    kErrorAssetsRequestInvokeFailed = 1016010,        // request failed.
    kErrorAssetsUpdateConsumeProgressNull = 1016011,  // progress is null when update consume
                                                      // progress.
    kErrorAssetsCanNotFindAssetId = 1016012,  // cant file asset when do split or exchange or merge.
    kErrorAssetInvalidRequest = 1016013,      // invalid request.
    kErrorAssetDBFail = 1016014,              // query db failed in server back end.
    kErrorAssetDataTooOld = 1016015,          // local asset data too old.
    kErrorAssetInConsume = 1016016,           // asset still in consuming.
    kErrorAssetNotExist = 1016017,            // asset not exist.
    kErrorAssetExchangNotMatch = 1016018,     // asset exchange not match.
    kErrorAssetSystemError = 1016019,         // asset system error back end.
    kErrorAssetBadJasonData = 1016020,
    kErrorAssetStateNotConsuing = 1016021,    // asset state is not consuming.
    kErrorAssetStateConsuing = 1016022,       // asset state is consuming.
    kErrorAssetDifferentProductId = 1016023,  // exchange asset error with different product.
    kErrorAssetConsumeQuantityTooBig = 1016024,  // consume quantity bigger than exists.
    kErrorAssetMissMatchRailId = 1016025,        // rail id miss match the serialized buffer.
    kErrorAssetProductInfoNotReady = 1016026,    // IRailInGamePurchase::AsyncRequestAllProducts
                                                 // should be called to cache product info to
                                                 // local memory.

    // item purchase
    kErrorInGamePurchase = 1017000,
    kErrorInGamePurchaseProductInfoExpired = 1017001,  // product information in client is expired.
    kErrorInGamePurchaseAcquireSessionTicketFailed = 1017002,  // acquire session ticket failed.
    kErrorInGamePurchaseParseWebContentFaild = 1017003,        // parse product information from web
                                                               // content failed.
    kErrorInGamePurchaseProductIsNotExist = 1017004,           // product information is not exist.
    kErrorInGamePurchaseOrderIDIsNotExist = 1017005,           // order id is not exist.
    kErrorInGamePurchasePreparePaymentRequestTimeout = 1017006,  // prepare payment request timeout.
    kErrorInGamePurchaseCreateOrderFailed = 1017007,             // create order failed.
    kErrorInGamePurchaseQueryOrderFailed = 1017008,              // query order failed.
    kErrorInGamePurchaseFinishOrderFailed = 1017009,             // finish order failed.
    kErrorInGamePurchasePaymentFailed = 1017010,                 // payment is failed.
    kErrorInGamePurchasePaymentCancle = 1017011,                 // payment is canceled.
    kErrorInGamePurchaseCreatePaymentBrowserFailed = 1017012,    // create payment browser failed.
    kErrorInGamePurchaseExceededProductPurchaseLimit = 1017013,  // exceeded product purchase limit.
    kErrorInGamePurchaseExceededPurchaseCountLimit = 1017014,    // exceeded purchase count limit.

    // in-game store
    kErrorInGameStorePurchase = 1017500,
    kErrorInGameStorePurchasePaymentSuccess = 1017501,  // payment is success.
    kErrorInGameStorePurchasePaymentFailure = 1017502,  // payment is failed.
    kErrorInGameStorePurchasePaymentCancle = 1017503,   // payment is canceled.

    // player
    kErrorPlayer = 1018000,
    kErrorPlayerUserFolderCreateFailed = 1018001,     // create user data folder failed.
    kErrorPlayerUserFolderCanntFind = 1018002,        // can't find user data folder.
    kErrorPlayerUserNotFriend = 1018003,              // player is not friend.
    kErrorPlayerGameNotSupportPurchaseKey = 1018004,  // not support purchase key.
    kErrorPlayerGetAuthenticateURLFailed = 1018005,   // get authenticate url failed.
    kErrorPlayerGetAuthenticateURLServerError = 1018006,  // server error while get authenticate
                                                          // url.
    kErrorPlayerGetAuthenticateURLInvalidURL = 1018007,   // input url is not in the white list.

    // friends
    kErrorFriends = 1019000,
    kErrorFriendsKeyFrontUseRail = 1019001,
    kErrorFriendsMetadataSizeInvalid = 1019002,        // the size of key_values is more than 50.
    kErrorFriendsMetadataKeyLenInvalid = 1019003,      // the length of key is more than 256.
    kErrorFriendsMetadataValueLenInvalid = 1019004,    // the length of Value is more than 256.
    kErrorFriendsMetadataKeyInvalid = 1019005,         // user's key name can not start with "rail".
    kErrorFriendsGetMetadataFailed = 1019006,          // CommonKeyValueNode's error_code is not 0.
    kErrorFriendsSetPlayTogetherSizeZero = 1019007,    // player_list count is 0.
    kErrorFriendsSetPlayTogetherContentSizeInvalid = 1019008,  // the size of user_rich_content is
                                                               // more than 100.
    kErrorFriendsInviteResponseTypeInvalid = 1019009,  // the invite result is invalid.
    kErrorFriendsListUpdateFailed = 1019010,           // the friend_list update failed.
    kErrorFriendsAddFriendInvalidID = 1019011,         // Request sent failed by invalid rail_id.
    kErrorFriendsAddFriendNetworkError = 1019012,      // Request sent failed by network error.
    kErrorFriendsServerBusy = 1019013,                 // server is busy. you could process the
                                                       // same handle later.
    kErrorFriendsUpdateFriendsListTooFrequent = 1019014,  // update friends list too frequent.

    // session ticket
    kErrorSessionTicket = 1020000,
    kErrorSessionTicketGetTicketFailed = 1020001,      // get session ticket failed.
    kErrorSessionTicketAuthFailed = 1020002,           // authenticate the session ticket failed.
    kErrorSessionTicketAuthTicketAbandoned = 1020003,  // the session ticket is abandoned.
    kErrorSessionTicketAuthTicketExpire = 1020004,     // the session ticket expired.
    kErrorSessionTicketAuthTicketInvalid = 1020005,    // the session ticket is invalid.

    // session ticket webAPI
    kErrorSessionTicketInvalidParameter = 1020500,      // the request parameter is invalid.
    kErrorSessionTicketInvalidTicket = 1020501,         // invalid session ticket.
    kErrorSessionTicketIncorrectTicketOwner = 1020502,  // the session ticket owner is not correct.
    kErrorSessionTicketHasBeenCanceledByTicketOwner = 1020503,  // the session ticket has been
                                                                // canceled by owner.
    kErrorSessionTicketExpired = 1020504,  // the session ticket expired

    // float window
    kErrorFloatWindow = 1021000,
    kErrorFloatWindowInitFailed = 1021001,                    // initialize is failed.
    kErrorFloatWindowShowStoreInvalidPara = 1021002,          // input parameter is invalid.
    kErrorFloatWindowShowStoreCreateBrowserFailed = 1021003,  // create store browser window failed.

    // user space
    kErrorUserSpace = 1022000,
    kErrorUserSpaceGetWorkDetailFailed = 1022001,  // unable to query spacework's data.
    kErrorUserSpaceDownloadError = 1022002,    // failed to download at least one file of
                                               // spacework.
    kErrorUserSpaceDescFileInvalid = 1022003,  // spacework maybe broken, re-upload it to repair.
    kErrorUserSpaceReplaceOldFileFailed = 1022004,     // cannot update disk using download files.
    kErrorUserSpaceUserCancelSync = 1022005,           // user canceled the sync.
    kErrorUserSpaceIDorUserdataPathInvalid = 1022006,  // internal error.
    kErrorUserSpaceNoData = 1022007,                   // there is no data in such field.
    kErrorUserSpaceSpaceWorkIDInvalid = 1022008,       // use 0 as spacework id.
    kErrorUserSpaceNoSyncingNow = 1022009,             // there is no syncing to cancel.
    kErrorUserSpaceSpaceWorkAlreadySyncing = 1022010,  // only one syncing is allowed to the
                                                       // same spacework.
    kErrorUserSpaceSubscribePartialSuccess = 1022011,  // not all (un)subscribe operations success.
    kErrorUserSpaceNoVersionField = 1022012,           // missing version field when changing files
                                                       // for spacework.
    kErrorUserSpaceUpdateFailedWhenUploading = 1022013,  // can not query spacework's data when
                                                         // uploading, the spacework may not exist.
    kErrorUserSpaceGetTicketFailed = 1022014,  // can not get ticket when uploading, usually
                                               // network issues.
    kErrorUserSpaceVersionOccupied = 1022015,  // new version value must not equal to the last
                                               // version.
    kErrorUserSpaceCallCreateMethodFailed = 1022016,  // facing network issues when create a
                                                      // spacework.
    kErrorUserSpaceCreateMethodRspFailed = 1022017,   // server failed to create a spacework.
    // kErrorUserSpaceGenerateDescFileFailed = 1022018,  // not used
    // kErrorUserSpaceUploadFailed = 1022019,            // not used
    kErrorUserSpaceNoEditablePermission = 1022020,   // you have no permissions to
                                                     // change this spacework.
    kErrorUserSpaceCallEditMethodFailed = 1022021,   // facing network issues when committing
                                                     // changes of a spacework.
    kErrorUserSpaceEditMethodRspFailed = 1022022,    // server failed to commit changes of a
                                                     // spacework.
    kErrorUserSpaceMetadataHasInvalidKey = 1022023,  // the key of metadata should not start
                                                     // with Rail_(case insensitive).
    kErrorUserSpaceModifyFavoritePartialSuccess = 1022024,  // not all (un)favorite operations
                                                            // success.
    kErrorUserSpaceFilePathTooLong = 1022025,       // the path of file is too long to upload
                                                    // or download.
    kErrorUserSpaceInvalidContentFolder = 1022026,  // the content folder provided is invalid,
                                                    // check the folder path is exist.

    kErrorUserSpaceInvalidFilePath = 1022027,  // internal error, the upload file path is invalid.
    kErrorUserSpaceUploadFileNotMeetLimit = 1022028,  // file to be uploaded don't meet
                                                      // requirements, such as size and format
                                                      // and so on.
    kErrorUserSpaceCannotReadFileToBeUploaded = 1022029,  // can not read the file need to be
                                                          // uploaded technically won't happen
                                                          // in updating a existing spacework,
                                                          // check whether the file is occupied
                                                          // by other programs.
    kErrorUserSpaceUploadSpaceWorkHasNoVersionField = 1022030,  // usually network issues, try
                                                                // again.
    kErrorUserSpaceDownloadCurrentDescFileFailed = 1022031,  // download current version's
                                                             // description file failed when
                                                             // no file changed in the new version,
                                                             // call StartSync again when
                                                             // facing it in creating spacework.
    kErrorUserSpaceCannotGetSpaceWorkDownloadUrl = 1022032,  // can not get the download url of
                                                             // spacework, this spacework maybe
                                                             // broken.
    kErrorUserSpaceCannotGetSpaceWorkUploadUrl = 1022033,    // can not get the upload url of
                                                             //  spacework, spacework maybe broken.
    kErrorUserSpaceCannotReadFileWhenUploading = 1022034,    // can not read file when uploading,
                                                             // make sure you haven't changed the
                                                             // file when uploading.
    kErrorUserSpaceUploadFileTooLarge = 1022035,    // file uploaded should be smaller than
                                                    // 2^53 - 1 bytes.
    kErrorUserSpaceUploadRequestTimeout = 1022036,  // upload http request time out,
                                                    // check your network connections.
    kErrorUserSpaceUploadRequestFailed = 1022037,   // upload file failed.
    kErrorUserSpaceUploadInternalError = 1022038,   // internal error.
    kErrorUserSpaceUploadCloudServerError = 1022039,  // get error from cloud server or
                                                      // can not get needed data from response.
    kErrorUserSpaceUploadCloudServerRspInvalid = 1022040,  // cloud server response invalid data
    kErrorUserSpaceUploadCopyNoExistCloudFile = 1022041,   // reuse the old version's files need
                                                           // copy them to new version location,
                                                           // the old version file is not exist,
                                                           // it may be cleaned by server
                                                           // if not used for a long time, please
                                                           // re-upload the full content folder.
    kErrorUserSpaceShareLevelNotSatisfied = 1022042,  // there are some limits of this type.
    kErrorUserSpaceHasntBeenApproved = 1022043,       // the spacework has been submit with
                                                      // public share level and hasn't been
                                                      // approved or rejected so you can't submit
                                                      // again until it is approved or rejected.
    kErrorUserSpaceCanNotLoadCacheFromLocalFile = 1022044, // fail to load cache from local file

    // user space refactoring
    kErrorUserSpaceV2NoDownloadURL = 1022045,           // can not get download url. Please contact
                                                        // with WeGame.
    kErrorUserSpaceV2DownloadDescriptionFilePerformFailed = 1022046, // failed to get description
                                                                     // file because of HTTP error.
    kErrorUserSpaceV2DescriptionFileParseResponseNotJson = 1022047, // HTTP response data is not a
                                                                    // json string. Please contact
                                                                    // with WeGame.
    kErrorUserSpaceV2DescriptionFileParseJsonError = 1022048, // failed to parse json string,
                                                              // please contact with WeGame.
    kErrorUserSpaceV2CheckDownloadContentFailed = 1022049,  // failed to check the
                                                            // downloaded content.
    kErrorUserSpaceV2OfflineMode = 1022050,                 // at offline mode.
    kErrorUserSpaceV2CannotAccessUserDataPath = 1022051,    // can not get userdata path of
                                                            // current user. Please try later.
    kErrorUserSpaceV2CleanInvalidFilesFailed = 1022052,     // failed to clean invalid files,
                                                            // please check whether the files
                                                            // in user data path are occupied.
    kErrorUserSpaceV2GeneratePreviewPathFailed = 1022053,   // failed to generate preview path.
    kErrorUserSpaceV2LocalFilesAreLatestVersion = 1022054,  // local files are the latest, so no
                                                            // need to download.
    kErrorUserSpaceV2DownloadFilesFailed = 1022055,         // failed to start to download
                                                            // preview file or content files.
    kErrorUserSpaceV2HelperInvokeFailed = 1022056,          // RPC request failed.
    kErrorUserSpaceV2HelperInvokeRetFailed = 1022057,       // RPC request return error.
    kErrorUserSpaceV2HelperNoCache = 1022058,               // can not get the cache of spacework.
    kErrorUserSpaceV2HelperQueryDetailInfoFailed = 1022059, // failed to retrieve spacework's
                                                            // detail information, the request
                                                            // unavailable or returns error.
                                                            // please check the network and retry.
    kErrorUserSpaceV2HelperNoFilesNeedDownload = 1022060,   // no file of spacework uploaded,
                                                            // so there is no need to download.
    kErrorUserSpaceV2WriteDescriptionFileFailed = 1022061,  // failed to write description file
                                                            // to local.
    kErrorUserSpaceV2FilePathTooLong = 1022062,             // file path is too long.
    kErrorUserSpaceV2ResumeDownloadFromBreakpointFailed = 1022063,  // failed to resume download
                                                                    // from break-point.
    kErrorUserSpaceV2CannotFindTaskIdInResumeDownloadList = 1022064, // no task found in cache
                                                                     // list of break-point tasks.
    kErrorUserSpaceV2CancelAllDownloadByUser = 1022065, // cancel all tasks by user.
    kErrorUserSpaceV2AlreadyInDownloading = 1022066,   // the spacework is already in downloading.
    kErrorUserSpaceV2TenioDLError = 1022067,       // TenioDL inner error, please
                                                   // contact with WeGame.
    kErrorUserSpaceV2EnumEnd = 1022068,            // not used, only used to calculate the enum
                                                   // length of user space refactoring.

    // game server
    kErrorGameServer = 1023000,
    kErrorGameServerCreateFailed = 1023001,              // create game server failed.
    kErrorGameServerDisconnectedServerlist = 1023002,    // the game server disconnects from
                                                         // game server list.
    kErrorGameServerConnectServerlistFailure = 1023003,  // report game server to game server
                                                         // list failed.
    kErrorGameServerSetMetadataFailed = 1023004,     // set game server meta data failed.
    kErrorGameServerGetMetadataFailed = 1023005,     // get game server meta data failed.
    kErrorGameServerGetServerListFailed = 1023006,   // query game server list failed.
    kErrorGameServerGetPlayerListFailed = 1023007,   // query game server player list failed.
    kErrorGameServerPlayerNotJoinGameserver = 1023008,
    kErrorGameServerNeedGetFovariteFirst = 1023009,  // should get favorite list first.
    kErrorGameServerAddFovariteFailed = 1023010,     // add game server to favorite list failed.
    kErrorGameServerRemoveFovariteFailed = 1023011,  // remove game server from favorite list
                                                     // failed.

    // network
    kErrorNetwork = 1024000,
    kErrorNetworkInitializeFailed = 1024001,       // initialize is failed.
    kErrorNetworkSessionIsNotExist = 1024002,      // session is not exist.
    kErrorNetworkNoAvailableDataToRead = 1024003,  // there is not available data to be read.
    kErrorNetworkUnReachable = 1024004,            // network is unreachable.
    kErrorNetworkRemotePeerOffline = 1024005,      // remote peer is offline.
    kErrorNetworkServerUnavailabe = 1024006,       // network server is unavailable.
    kErrorNetworkConnectionDenied = 1024007,       // connect request is denied.
    kErrorNetworkConnectionClosed = 1024008,       // connected session has been closed by remote
                                                   // peer.
    kErrorNetworkConnectionReset = 1024009,        // connected session has been reset.
    kErrorNetworkSendDataSizeTooLarge = 1024010,   // send data size is too big.
    kErrorNetworkSessioNotRegistered = 1024011,    // remote peer does not register to server.
    kErrorNetworkSessionTimeout = 1024012,         // remote register but no response.

    // Dlc error code
    kErrorDlc = 1025000,
    kErrorDlcInstallFailed = 1025001,         // install dlc failed.
    kErrorDlcUninstallFailed = 1025002,       // uninstall dlc failed.
    kErrorDlcGetDlcListTimeout = 1025003,     // deprecated.
    kErrorDlcRequestInvokeFailed = 1025004,   // request failed when query dlc authority.
    kErrorDlcRequestToofrequently = 1025005,  // request too frequently when query dlc authority.

    // utils
    kErrorUtils = 1026000,
    kErrorUtilsImagePathNull = 1026001,                 // the image path is null.
    kErrorUtilsImagePathInvalid = 1026002,              // the image path is invalid.
    kErrorUtilsImageDownloadFail = 1026003,             // failed to download the image.
    kErrorUtilsImageOpenLocalFail = 1026004,            // failed to open local image file.
    kErrorUtilsImageBufferAllocateFail = 1026005,       // failed to allocate image buffer.
    kErrorUtilsImageReadLocalFail = 1026006,            // failed to read local image.
    kErrorUtilsImageParseFail = 1026007,                // failed parse the image.
    kErrorUtilsImageScaleFail = 1026008,                // failed to scale the image.
    kErrorUtilsImageUnknownFormat = 1026009,            // image image format is unknown.
    kErrorUtilsImageNotNeedResize = 1026010,            // the image is not need to resize.
    kErrorUtilsImageResizeParameterInvalid = 1026011,   // the parameter used to resize image
                                                      // is invalid.
    kErrorUtilsImageSaveFileFail = 1026012,             // could not save image.
    kErrorUtilsDirtyWordsFilterTooManyInput = 1026013,  // there are too many inputs for dirty
                                                      // words filter.
    kErrorUtilsDirtyWordsHasInvalidString = 1026014,    // there are invalid strings in the
                                                      // dirty words.
    kErrorUtilsDirtyWordsNotReady = 1026015,      // dirty words utility is not ready.
    kErrorUtilsDirtyWordsDllUnloaded = 1026016,   // dirty words library is not loaded.
    kErrorUtilsCrashAllocateFailed = 1026017,     // crash report buffer can not be allocated.
    kErrorUtilsCrashCallbackSwitchOff = 1026018,  // crash report callback switch is currently off.

    // users
    kErrorUsers = 1027000,
    kErrorUsersInvalidInviteCommandLine = 1027001,  // the invite command line provided is invalid.
    kErrorUsersSetCommandLineFailed = 1027002,      // failed to set command line.
    kErrorUsersInviteListEmpty = 1027003,           // the invite user list is empty.
    kErrorUsersGenerateRequestFail = 1027004,       // failed to generate invite request.
    kErrorUsersUnknownInviteType = 1027005,         // the invite type provided is unknown.
    kErrorUsersInvalidInviteUsersSize = 1027006,    // the user count to invite is invalid.

    // screenshot
    kErrorScreenshot = 1028000,
    kErrorScreenshotWorkNotExist = 1028001,    // create space work for the screenshot failed.
    kErrorScreenshotCantConvertPng = 1028002,  // convert the screenshot image to png format failed.
    kErrorScreenshotCopyFileFailed = 1028003,  // copy the screenshot image to publish folder
                                               // failed.
    kErrorScreenshotCantCreateThumbnail = 1028004,  // create a thumbnail image for screenshot
                                                    // failed.

    // voice capture
    kErrorVoiceCapture = 1029000,
    kErrorVoiceCaptureInitializeFailed = 1029001,   // initialized failed.
    kErrorVoiceCaptureDeviceLost = 1029002,         // voice device lost.
    kErrorVoiceCaptureIsRecording = 1029003,        // is already recording.
    kErrorVoiceCaptureNotRecording = 1029004,       // is not recording.
    kErrorVoiceCaptureNoData = 1029005,             // currently no voice data to get.
    kErrorVoiceCaptureMoreData = 1029006,           // there is more data to get.
    kErrorVoiceCaptureDataCorrupted = 1029007,      // illegal data captured.
    kErrorVoiceCapturekUnsupportedCodec = 1029008,  // illegal data to decode.
    kErrorVoiceChannelHelperNotReady = 1029009,  // voice module is not ready now, try again later.
    kErrorVoiceChannelIsBusy = 1029010,          // voice channel is too busy to handle operation,
                                                 // try again later or slow down the operations.

    kErrorVoiceChannelNotJoinedChannel = 1029011,  // player haven't joined this channel, you can
                                                   // not do some operations on this channel like
                                                   // leave.
    kErrorVoiceChannelLostConnection = 1029012,    // lost connection to server now,
                                                   // sdk will automatically reconnect later.
    kErrorVoiceChannelAlreadyJoinedAnotherChannel = 1029013,  // player could only join one channel
                                                              // at the same time.
    kErrorVoiceChannelPartialSuccess = 1029014,      // operation is not fully success.
    kErrorVoiceChannelNotTheChannelOwner = 1029015,  // only the owner could remove users.

    // text input
    kErrorTextInputTextInputSendMessageToPlatformFailed = 1040000,
    kErrorTextInputTextInputSendMessageToOverlayFailed = 1040001,
    kErrorTextInputTextInputUserCanceled = 1040002,
    kErrorTextInputTextInputEnableChineseFailed = 1040003,
    kErrorTextInputTextInputShowFailed = 1040004,
    kErrorTextInputEnableIMEHelperTextInputWindowFailed = 1040005,

    // app error
    kErrorApps = 1041000,
    kErrorAppsCountingKeyExists = 1041001,
    kErrorAppsCountingKeyDoesNotExist = 1041002,

    // http session errro
    kErrorHttpSession = 1042000,
    kErrorHttpSessionPostBodyContentConflictWithPostParameter = 1042001,
    kErrorHttpSessionRequestMehotdNotPost = 1042002,

    // small object service
    kErrorSmallObjectService = 1043000,
    kErrorSmallObjectServiceObjectNotExist = 1043001,
    kErrorSmallObjectServiceFailedToRequestDownload = 1043002,
    kErrorSmallObjectServiceDownloadFailed = 1043003,
    kErrorSmallObjectServiceFailedToWriteDisk = 1043004,
    kErrorSmallObjectServiceFailedToUpdateObject = 1043005,
    kErrorSmallObjectServicePartialDownloadSuccess = 1043006,
    kErrorSmallObjectServiceObjectNetworkIssue = 1043007,
    kErrorSmallObjectServiceObjectServerError = 1043008,
    kErrorSmallObjectServiceInvalidBranch = 1043009,

    // zone server
    kErrorZoneServer = 1044000,
    kErrorZoneServerValueDataIsNotExist = 1044001,

    // in-game coin
    kErrorInGameCoin = 1045000,
    kErrorInGameCoinCreatePaymentBrowserFailed = 1045001,  // create payment browser failed.
    kErrorInGameCoinOperationTimeout = 1045002,  // operation time out.
    kErrorInGameCoinPaymentFailed = 1045003,     // payment is failed.
    kErrorInGameCoinPaymentCanceled = 1045004,   // payment is canceled.

    // --------------sdk error code end----------------

    // --------------rail server error code begin----------------
    // RAIL_ERROR_SERVER_BEGIN = 2000000;
    // RAIL_ERROR_SERVER_END = 2999999;
    kRailErrorServerBegin = 2000000,

    // Payment Web-API error code
    kErrorPaymentSystem = 2080001,
    kErrorPaymentParameterIlleage = 2080008,
    kErrorPaymentOrderIlleage = 2080011,

    // Assets Web-API error code
    kErrorAssetsInvalidParameter = 2230001,
    kErrorAssetsSystemError = 2230007,

    // Dirty words filter Web-API error code
    kErrorDirtyWordsFilterNoPermission = 2290028,
    kErrorDirtyWordsFilterCheckFailed = 2290029,
    kErrorDirtyWordsFilterSystemBusy = 2290030,

    // Web-API error code for inner server
    kRailErrorInnerServerBegin = 2500000,

    // GameGray Web-API error code
    kErrorGameGrayCheckSnowError = 2500001,
    kErrorGameGrayParameterIlleage = 2500002,
    kErrorGameGraySystemError = 2500003,
    kErrorGameGrayQQToWegameidError = 2500004,

    kRailErrorInnerServerEnd = 2699999,

    // last rail server error code
    kRailErrorServerEnd = 2999999,
    // --------------rail server error code end----------------

    kErrorUnknown = 0xFFFFFFFF,  // unknown error
};

}  // namespace rail

#endif  // RAIL_SDK_RAIL_RESULT_H

