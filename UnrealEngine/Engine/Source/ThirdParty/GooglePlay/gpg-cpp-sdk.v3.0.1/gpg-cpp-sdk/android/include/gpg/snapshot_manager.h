// Copyright 2017 Google Inc. All rights reserved.
// These files are licensed under the Google Play Games Services Terms of
// Service which can be found here:
// https://developers.google.com/games/services/terms

/**
 * @file gpg/snapshot_manager.h
 *
 * @brief Value object that represents the completion of a task or goal.
 */

#ifndef GPG_SNAPSHOT_MANAGER_H_
#define GPG_SNAPSHOT_MANAGER_H_

#ifndef __cplusplus
#error Header file supports C++ only
#endif  // __cplusplus

#include <memory>
#include <vector>
#include "gpg/common.h"
#include "gpg/game_services.h"
#include "gpg/snapshot_metadata.h"
#include "gpg/snapshot_metadata_change.h"
#include "gpg/types.h"

namespace gpg {

/**
 * For Snapshots to be used, the app must request kSnapshotScope scope (equal to
 * <code>https://www.googleapis.com/auth/drive.appdata</code>) at authentication
 * time.
 * This can be done via {@link GameServices::Builder::EnableSnapshots}.
 */
extern const char *kSnapshotScope;

/**
 * Gets and sets various snapshot-related data. If the app does not enable
 * snapshots at authentication time (see
 * {@link GameServices::Builder::EnableSnapshots}), most methods on
 * <code>SnapshotManager</code> will fail.
 *
 * @ingroup Managers
 */
class GPG_EXPORT SnapshotManager {
 public:
  /**
   * Holds max size for snapshot data and for snapshot cover image.
   * @ingroup ResponseType
   */
  struct MaxSizeResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     * If the response is unsuccessful, <code>maxDataSize</code> and
     * <code>maxCoverImageSize</code> are equal to zero.
     */
    ResponseStatus status;

    /**
     * The maximum data size per snapshot in bytes. Guaranteed to be at least 3
     * MB. May increase in the future.
     */
    size_t maxDataSize;

    /**
     * The maximum data size per snapshot cover image in bytes. Guaranteed to be
     * at least 800 KB. May increase in the future.
     */
    size_t maxCoverImageSize;
  };

  /**
   * Defines a callback type that receives a <code>MaxSizeResponse</code>. This
   * callback type is provided to the {@link #GetMaxSize}.
   *
   * @ingroup Callbacks
   */
  typedef std::function<void(const MaxSizeResponse &)> MaxSizeCallback;

  /**
   * Asynchronously gets the maximum data size and the maximum cover image
   * size per snapshot in bytes.
   *
   * Calls the provided <code>MaxSizeCallback</code> upon operation completion.
   *
   * The maximum data size per snapshot guaranteed to be at least 3 MB. May
   * increase in the future.
   *
   * The maximum data size per snapshot cover image guaranteed to be at least
   * 800 KB. May increase in the future.
   */
  void GetMaxSize(MaxSizeCallback callback) const;

  /**
   * Synchronously gets the maximum data size and the maximum cover image
   * size per snapshot in bytes, directly returning the
   * <code>MaxSizeResponse</code>.
   *
   * The maximum data size per snapshot guaranteed to be at least 3 MB. May
   * increase in the future.
   *
   * The maximum data size per snapshot cover image guaranteed to be at least
   * 800 KB. May increase in the future.
   */
  MaxSizeResponse GetMaxSizeBlocking() const;

  /**
   * Synchronously gets the maximum data size and the maximum cover image
   * size per snapshot in bytes, directly returning the
   * <code>MaxSizeResponse</code>.
   *
   * The maximum data size per snapshot guaranteed to be at least 3 MB. May
   * increase in the future.
   *
   * The maximum data size per snapshot cover image guaranteed to be at least
   * 800 KB. May increase in the future.
   *
   * Specify <code>timeout</code> in milliseconds.
   */
  MaxSizeResponse GetMaxSizeBlocking(Timeout timeout) const;

  /**
   * Holds all data for all snapshots, along with a response status.
   * @ingroup ResponseType
   */
  struct FetchAllResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     * If the response is unsuccessful, <code>FetchAllResponse</code>'s data
     * vector is empty.
     */
    ResponseStatus status;

    /**
     * A vector containing metadata for all snapshots associated with the
     * application.
     */
    std::vector<SnapshotMetadata> data;
  };

  /**
   * Defines a callback type that receives a <code>FetchAllResponse</code>. This
   * callback type is provided to the <code>FetchAll(*)</code> functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(const FetchAllResponse &)> FetchAllCallback;

  /**
   * Asynchronously loads all snapshot data for the currently signed-in
   * player. Calls the provided <code>FetchAllCallback</code> upon operation
   * completion. Not specifying <code>data_source</code> makes this function
   * call equivalent to calling
   * <code>FetchAll(DataSource data_source, FetchAllCallback callback)</code>,
   * with <code>data_source</code> specified as <code>CACHE_OR_NETWORK.</code>
   */
  void FetchAll(FetchAllCallback callback);

  /**
   * Asynchronously loads all snapshot data for the currently signed-in
   * player. Calls the provided <code>FetchAllCallback</code> upon operation
   * completion. Specify <code>data_source</code> as
   * <code>CACHE_OR_NETWORK</code> or <code>NETWORK_ONLY</code>.
   */
  void FetchAll(DataSource data_source, FetchAllCallback callback);

  /**
   * Synchronously loads all snapshot data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>. Specifying
   * neither <code>data_source</code> nor <code>timeout</code> makes this
   * function call equivalent to calling
   * <code>FetchAllResponse FetchAllBlocking(DataSource data_source, </code>
   * <code>Timeout timeout)</code>,
   * with <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>,
   * and <code>timeout</code> specified as 10 years.
   */
  FetchAllResponse FetchAllBlocking();

  /**
   * Synchronously loads all snapshot data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>.
   * Specify <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Not specifying <code>timeout</code> makes this
   * function call equivalent to calling
   * <code>FetchAllBlocking FetchAllResponse(DataSource data_source, </code>
   * <code>Timeout timeout)</code>,
   * with your specified <code>data_source</code> value, and
   * <code>timeout</code> specified as 10 years.
   */
  FetchAllResponse FetchAllBlocking(DataSource data_source);

  /**
   * Synchronously loads all snapshot data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>. Specify
   * <code>timeout</code> in milliseconds.
   * Not specifying <code>data_source</code> makes this function call equivalent
   * to calling
   * <code>FetchAllResponse FetchAllBlocking(DataSource data_source, </code>
   * <code>Timeout timeout)</code>,
   * with <code>data_source</code> specified as <code>CACHE_OR_NETWORK</code>,
   * and <code>timeout</code> containing your specified value.
   */
  FetchAllResponse FetchAllBlocking(Timeout timeout);

  /**
   * Synchronously loads all snapshot data for the currently signed-in
   * player, directly returning the <code>FetchAllResponse</code>. Specify
   * <code>data_source</code> as <code>CACHE_OR_NETWORK</code> or
   * <code>NETWORK_ONLY</code>. Specify <code>timeout</code> in milliseconds.
   */
  FetchAllResponse FetchAllBlocking(DataSource data_source, Timeout timeout);

  /**
   * Holds the data for a particular requested snapshot along with a response
   * status. If the snapshot is in a conflicted state, the main snapshot
   * <code>data</code> will not be valid and conflict information will be
   * provided in the <code>conflict_id</code>, <code>conflict_original</code>,
   * and <code>conflict_unmerged</code> fields.
   * The conflict must be resolved before the snapshot can receive future
   * modifications.
   *
   * @ingroup ResponseType
   */
  struct OpenResponse {
    /**
     * Can be one of the values enumerated in {@link SnapshotOpenStatus}.
     * If the response is unsuccessful, <code>OpenResponse</code>'s data vector
     * is empty.
     */
    SnapshotOpenStatus status;

    /**
     * A <code>Snapshot</code>. This snapshot will only be valid if there are no
     * errors or conflicts. In the event of an unhandled conflict use
     * <code>conflict_id</code>, <code>conflict_original</code>, and
     * <code>conflict_unmerged</code>.
     */
    SnapshotMetadata data;

    /**
     * The identifier of this conflict. If this string is empty, there is no
     * conflict.
     */
    std::string conflict_id;

    /**
     * Empty if <code>conflict_id</code> is empty. This is the agreed upon
     * current version of the snapshot.
     * Note: previously called <code>conflict_base</code>.
     */
    SnapshotMetadata conflict_original;

    /**
     * Empty if <code>conflict_id</code> is empty. This is the proposed change
     * that failed to be applied due to conflicting operations from another
     * device.
     * Note: previously called <code>conflict_remote</code>.
     */
    SnapshotMetadata conflict_unmerged;
  };

  /**
   * Defines a callback type that receives an <code>OpenResponse</code>. This
   * callback type is provided to the <code>Open(*)</code> functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(const OpenResponse &)> OpenCallback;

  /**
   * Asynchronously opens a snapshot with the given name. The specified snapshot
   * will be created if it does not already exist. Calls the provided
   * <code>OpenCallback</code> upon operation completion.
   *
   * Snapshot names must be between 1 and 100 non-URL-reserved characters
   * (a-z, A-Z, 0-9, or the symbols "-", ".", "_", or "~").
   *
   * Conflicts can occur if another device commits a snapshot between loading
   * and committing a snapshot on the current device. You must resolve these
   * conflicts. See {@link OpenResponse} above for more detail on
   * conflicts.
   *
   * <code>conflict_policy</code> can be one of the following values:
   *
   * <code>SnapshotConflictPolicy::MANUAL</code> - In the event of a conflict,
   * the response has state <code>OpenResponse::VALID_WITH_CONFLICT</code>.
   * You must resolve the conflict using
   * <code>SnapshotManager::ResolveConflict</code>. It is possible to see
   * multiple conficts in a row, so check every time you call <code>Open</code>.
   * This is the only policy where you will see the conflict. The rest handle
   * resolution for you.
   * This policy ensures that no user changes to the state of the save game
   * will ever be lost.
   *
   * <code>SnapshotConflictPolicy::LONGEST_PLAYTIME</code> - In the event of a
   * conflict, the snapshot with the largest playtime value will be used.
   * This policy is a good choice if the length of play time is a reasonable
   * proxy for the "best" save game. Note that you must use
   * <code>SnapshotMetadataChange::Builder::SetPlayedTime()</code> when saving
   * games for this policy to be meaningful.
   *
   * <code>SnapshotConflictPolicy::LAST_KNOWN_GOOD</code> - In the event of a
   * conflict, the base snapshot will be used.
   * This policy is a reasonable choice if your game requires stability from
   * the snapshot data. This policy ensures that only writes which are not
   * contested are seen by the player, which guarantees that all clients
   * converge.
   * Note: previously <code>SnapshotManager::BASE_WINS</code>
   *
   * <code>SnapshotConflictPolicy::MOST_RECENTLY_MODIFIED</code> - In the event
   * of a conflict, the remote will be used. This policy is a reasonable choice
   * if your game can tolerate players on multiple devices clobbering their own
   * changes. Because this policy blindly chooses the most recent data, it is
   * possible that a player's changes may get lost. Note: previously
   * <code>SnapshotManager::REMOTE_WINS</code>
   *
   * <code>SnapshotConflictPolicy::HIGHEST_PROGRESS</code>In the case of a
   * conflict, the snapshot with the highest progress value will be used. In the
   * case of a tie, the last known good snapshot will be chosen instead.
   * This policy is a good choice if your game uses the progress value of the
   * snapshot to determine the best saved game. Note that you must use
   * <code>SnapshotMetadataChange::Builder::SetPlayedTime()</code> when saving
   * games for this policy to be meaningful.
   */
  void Open(const std::string &file_name,
            SnapshotConflictPolicy conflict_policy,
            OpenCallback callback);

  /**
   * Synchronously opens a snapshot with the given name. The specified snapshot
   * will be created if it does not already exist.
   *
   * See {@link #Open} for more details.
   */
  OpenResponse OpenBlocking(const std::string &file_name,
                            SnapshotConflictPolicy conflict_policy);

  /**
   * Synchronously opens a snapshot with the given name. The specified snapshot
   * will be created if it does not already exist.
   *
   * Specify <code>timeout</code> in milliseconds.
   *
   * See {@link #Open} for more details.
   */
  OpenResponse OpenBlocking(Timeout timeout,
                            const std::string &file_name,
                            SnapshotConflictPolicy conflict_policy);

  /**
   * Asynchronously opens a snapshot with the given name.
   * @deprecated <code>data_source</code> is ignored. Use <code>Open(file_name,
   * conflict_policy, callback)</code> instead.
   */
  void Open(DataSource data_source,
            const std::string &file_name,
            SnapshotConflictPolicy conflict_policy,
            OpenCallback callback);

  /**
   * Synchronously opens a snapshot with the given name.
   * @deprecated <code>data_source</code> is ignored. Use
   * <code>OpenBlocking(file_name, conflict_policy)</code> instead.
   */
  OpenResponse OpenBlocking(DataSource data_source,
                            const std::string &file_name,
                            SnapshotConflictPolicy conflict_policy);

  /**
   * Synchronously opens a snapshot with the given name.
   * @deprecated <code>data_source</code> is ignored. Use
   * <code>OpenBlocking(timeout, file_name, conflict_policy)</code> instead.
   */
  OpenResponse OpenBlocking(DataSource data_source,
                            Timeout timeout,
                            const std::string &file_name,
                            SnapshotConflictPolicy conflict_policy);

  /**
   * Holds the data for an updated snapshot, along with a response status.
   * @ingroup ResponseType
   */
  struct CommitResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     * If the response is unsuccessful, <code>CommitResponse</code>'s
     * data is empty.
     */
    ResponseStatus status;

    /**
     * A <code>SnapshotMetadata</code> object.
     */
    SnapshotMetadata data;
  };

  /**
   * Defines a callback type that receives an <code>CommitResponse</code>. This
   * callback type is provided to the <code>Commit(*)</code> and
   * <code>ResolveConflict(*)</code> functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(const CommitResponse &)> CommitCallback;

  /**
   * Asynchronously commits the data provided to the snapshot, and updates the
   * metadata of the snapshot using the provided metadata object.
   * Calls the provided <code>CommitCallback</code> upon operation completion.
   */
  void Commit(const SnapshotMetadata &snapshot_metadata,
              const SnapshotMetadataChange &metadata_change,
              std::vector<uint8_t> data,
              CommitCallback callback);

  /**
   * Synchronously commits the data provided to the snapshot, and updates the
   * metadata of the snapshot using the provided metadata object.
   */
  CommitResponse CommitBlocking(const SnapshotMetadata &snapshot_metadata,
                                const SnapshotMetadataChange &metadata_change,
                                std::vector<uint8_t> contents);

  /**
   * Synchronously commits the data provided to the snapshot and updates the
   * metadata of the snapshot using the provided metadata object.
   * Specify <code>timeout</code> in milliseconds.
   */
  CommitResponse CommitBlocking(Timeout timeout,
                                const SnapshotMetadata &snapshot_metadata,
                                const SnapshotMetadataChange &metadata_change,
                                std::vector<uint8_t> contents);

  /**
   * Asynchronously resolves a conflict using the data from the provided
   * snapshot. This will replace the data on the server with the specified
   * snapshot. Note that it is possible for this operation to result in a
   * conflict itself, in which case resolution should be repeated.
   *
   * Calling this method with a snapshot that has already been committed or that
   * was not opened via {@link #Open} will fail with BaseStatus::ERROR_INTERNAL
   * status.
   *
   * Calls the provided <code>OpenCallback</code> upon operation
   * completion.
   */
  void ResolveConflict(const std::string &conflict_id,
                       const SnapshotMetadata &snapshot_metadata,
                       OpenCallback callback);

  /**
   * Asynchronously resolves a conflict using the provided data. This will
   * replace the data on the server with the specified metadata changes and
   * <code>contents</code>. Note that it is possible for this operation to
   * result in a conflict itself, in which case resolution should be repeated.
   *
   * Values which are not included in the metadata change will be resolved to
   * the version currently on the server.
   *
   * Note that the total size of the <code>contents</code> may not exceed the
   * <code>maxDataSize</code> provided by {@link #GetMaxSize}.
   *
   * Calling this method with a snapshot that has already been committed or that
   * was not opened via {@link #Open} will fail with BaseStatus::ERROR_INTERNAL
   * status.
   *
   * Calls the provided <code>OpenCallback</code> upon operation
   * completion.
   */
  void ResolveConflict(const std::string &conflict_id,
                       const SnapshotMetadata &snapshot_metadata,
                       const SnapshotMetadataChange &metadata_change,
                       std::vector<uint8_t> contents,
                       OpenCallback callback);

  /**
   * Synchronously resolves a conflict using the data from the provided
   * snapshot. This will replace the data on the server with the specified
   * snapshot. Note that it is possible for this operation to result in a
   * conflict itself, in which case resolution should be repeated.
   *
   * Calling this method with a snapshot that has already been committed or that
   * was not opened via {@link #Open} will fail with BaseStatus::ERROR_INTERNAL
   * status.
   */
  OpenResponse ResolveConflictBlocking(
      const std::string &conflict_id,
      const SnapshotMetadata &snapshot_metadata);

  /**
   * Synchronously resolves a conflict using the provided data. This will
   * replace the data on the server with the specified metadata changes and
   * <code>contents</code>. Note that it is possible for this operation to
   * result in a conflict itself, in which case resolution should be repeated.
   *
   * Values which are not included in the metadata change will be resolved to
   * the version currently on the server.
   *
   * Note that the total size of the <code>contents</code> may not exceed the
   * <code>maxDataSize</code> provided by {@link #GetMaxSize}.
   *
   * Calling this method with a snapshot that has already been committed or that
   * was not opened via {@link #Open} will fail with BaseStatus::ERROR_INTERNAL
   * status.
   */
  OpenResponse ResolveConflictBlocking(
      const std::string &conflict_id,
      const SnapshotMetadata &snapshot_metadata,
      const SnapshotMetadataChange &metadata_change,
      std::vector<uint8_t> contents);

  /**
   * Synchronously resolves a conflict using the data from the provided
   * snapshot. This will replace the data on the server with the specified
   * snapshot. Note that it is possible for this operation to result in a
   * conflict itself, in which case resolution should be repeated.
   *
   * Calling this method with a snapshot that has already been committed or that
   * was not opened via {@link #Open} will fail with BaseStatus::ERROR_INTERNAL
   * status.
   *
   * Specify <code>timeout</code> in milliseconds.
   */
  OpenResponse ResolveConflictBlocking(
      Timeout timeout,
      const std::string &conflict_id,
      const SnapshotMetadata &snapshot_metadata);

  /**
   * Synchronously resolves a conflict using the provided data. This will
   * replace the data on the server with the specified metadata changes and
   * <code>contents</code>. Note that it is possible for this operation to
   * result in a conflict itself, in which case resolution should be repeated.
   *
   * Values which are not included in the metadata change will be resolved to
   * the version currently on the server.
   *
   * Note that the total size of the <code>contents</code> may not exceed the
   * <code>maxDataSize</code> provided by {@link #GetMaxSize}.
   *
   * Calling this method with a snapshot that has already been committed or that
   * was not opened via {@link #Open} will fail with BaseStatus::ERROR_INTERNAL
   * status.
   *
   * Specify <code>timeout</code> in milliseconds.
   */
  OpenResponse ResolveConflictBlocking(
      Timeout timeout,
      const std::string &conflict_id,
      const SnapshotMetadata &snapshot_metadata,
      const SnapshotMetadataChange &metadata_change,
      std::vector<uint8_t> contents);

  /**
   * Deletes the specified snapshot. This will delete the data of the snapshot
   * locally and on the server.
   */
  void Delete(const SnapshotMetadata &snapshot_metadata);

  /**
   * Reads response status and snapshot data returned from a snapshot read
   * operation.
   * @ingroup ResponseType
   */
  struct ReadResponse {
    /**
     * Can be one of the values enumerated in {@link ResponseStatus}.
     * If the response is unsuccessful, <code>ReadResponse</code>'s data vector
     * is empty.
     */
    ResponseStatus status;

    /**
     * A vector containing the data in the snapshot.
     */
    std::vector<uint8_t> data;
  };

  /**
   * Defines a callback type that receives a <code>ReadResponse</code>.
   * This callback type is provided to the <code>Read(*)</code> functions below.
   * @ingroup Callbacks
   */
  typedef std::function<void(const ReadResponse &)> ReadCallback;

  /**
   * Asynchronously reads a snapshot off of the disk, and copies it into memory.
   * The data is passed back by value for easier modification. Each call to this
   * function results in a full read. This means that it is typically best only
   * to read a snapshot once.
   * Calls the provided <code>ReadCallback</code> upon operation completion.
   */
  void Read(const SnapshotMetadata &snapshot_metadata, ReadCallback callback);

  /**
   * Synchronously reads a snapshot off of the disk, and copies it into memory.
   * The data is passed back by value for easier modification. Each call to this
   * function results in a full read. This means that it is typically best only
   * to read a snapshot once. Not specifying <code>timeout</code> makes this
   * function call equivalent to calling
   * <code>ReadBlocking ReadBlocking(Timeout timeout, </code>
   * <code>const SnapshotMetadata& snapshot_metadata)</code>,
   * with <code>timeout</code> specified as 10 years.
   */
  ReadResponse ReadBlocking(const SnapshotMetadata &snapshot_metadata);

  /**
   * Synchronously reads a snapshot off of the disk and copies it into memory.
   * The data is passed back by value for easier modification. Each call to this
   * does a full read so typically only read a snapshot once.
   * Specify <code>timeout</code> in milliseconds.
   */
  ReadResponse ReadBlocking(Timeout timeout,
                            const SnapshotMetadata &snapshot_metadata);

  /**
   * <code>Data</code> and <code>ResponseStatus</code> for the
   * <code>ShowSelectUIOperation</code> operation.
   *
   * @ingroup ResponseType
   */
  struct SnapshotSelectUIResponse {
    /**
     * The <code>ResponseStatus</code> of the operation that generated this
     * <code>Response</code>.
     */
    UIStatus status;

    /**
     * The <code>SnapshotMetadata</code> for this response. <code>Valid()</code>
     * only returns true if <code>IsSuccess(status)</code> returns true, and an
     * existing snapshot was selected.
     */
    SnapshotMetadata data;
  };

  /**
   * Defines a callback that can receive a <code>SnapshotSelectUIResponse</code>
   * from <code>ShowSelectUIOperation</code>.
   */
  typedef std::function<void(const SnapshotSelectUIResponse &)>
      SnapshotSelectUICallback;

  /**
   * Asynchronously shows the snapshot UI, allowing the player to select a
   * snapshot or request a new snapshot. Upon completion, the selected snapshot
   * or new snapshot request are returned via the
   * <code>SnapshotSelectUICallback</code>.
   */
  void ShowSelectUIOperation(bool allow_create,
                             bool allow_delete,
                             uint32_t max_snapshots,
                             const std::string &title,
                             SnapshotSelectUICallback callback);

  /**
   * Blocking version of {@link ShowSelectUIOperation}. Allows the caller to
   * specify a timeout in ms. After the specified time elapses, the function
   * returns <code>ERROR_TIMEOUT</code>.
   */
  SnapshotSelectUIResponse ShowSelectUIOperationBlocking(
      Timeout timeout,
      bool allow_create,
      bool allow_delete,
      uint32_t max_snapshots,
      const std::string &title);

  /**
   * Overload of {@link ShowSelectUIOperationBlocking}, which uses a default
   * timeout of 10 years.
   */
  SnapshotSelectUIResponse ShowSelectUIOperationBlocking(
      bool allow_create,
      bool allow_delete,
      uint32_t max_snapshots,
      const std::string &title);

 private:
  friend class GameServicesImpl;
  explicit SnapshotManager(GameServicesImpl *game_services_impl);
  ~SnapshotManager();
  SnapshotManager(const SnapshotManager &) = delete;
  SnapshotManager &operator=(const SnapshotManager &) = delete;

  GameServicesImpl *const impl_;
};

}  // namespace gpg

#endif  // GPG_SNAPSHOT_MANAGER_H_
