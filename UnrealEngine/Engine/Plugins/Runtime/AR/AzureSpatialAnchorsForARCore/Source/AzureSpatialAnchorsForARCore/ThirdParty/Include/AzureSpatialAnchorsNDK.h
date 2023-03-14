//
// AzureSpatialAnchors
// This file was auto-generated from SscApiModelDirect.cs.
//

#pragma once

#include <algorithm>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <arcore_c_api.h>

#include "AzureSpatialAnchorsLibrary_Exports.h"

namespace Microsoft { namespace Azure { namespace SpatialAnchors
{
    class AnchorLocateCriteria;
    class AnchorLocatedEventArgs;
    class CloudSpatialAnchor;
    class CloudSpatialAnchorMap;
    class CloudSpatialAnchorSessionDeferral;
    class CloudSpatialAnchorSessionDiagnostics;
    class CloudSpatialAnchorSession;
    class CloudSpatialAnchorWatcher;
    class GeoLocation;
    class LocateAnchorsCompletedEventArgs;
    class NearAnchorCriteria;
    class NearDeviceCriteria;
    class OnLogDebugEventArgs;
    class PlatformLocationProvider;
    class SensorCapabilities;
    class SensorFingerprintEventArgs;
    class SessionConfiguration;
    class SessionErrorEventArgs;
    class SessionStatus;
    class SessionUpdatedEventArgs;
    class TokenRequiredEventArgs;
} } }

namespace Microsoft { namespace Azure { namespace SpatialAnchors
{
    struct event_token
    {
        int64_t value{};

        explicit operator bool() const noexcept
        {
            return value != 0;
        }

        void invalidate()
        {
            value = 0;
        }
    };

    inline auto operator==(event_token const& left, event_token const& right) noexcept -> bool
    {
        return left.value == right.value;
    }

    template <typename Delegate>
    struct event
    {
        using delegate_type = Delegate;

        event() = default;
        event(event<Delegate> const&) = delete;
        auto operator =(event<Delegate> const&) -> event<Delegate>& = delete;

        auto add(delegate_type const& delegate) -> event_token
        {
            event_token token{};

            {
                std::unique_lock<std::mutex> lock(m_change);

                token = get_next_token();
                m_targets.emplace_back(token, delegate);
            }

            return token;
        }

        auto remove(event_token& token) -> void
        {
            std::unique_lock<std::mutex> lock(m_change);

            const auto iterator = std::find_if(m_targets.begin(), m_targets.end(), [&](const std::pair<event_token, Delegate>& item) {
                return item.first == token;
            });

            if (iterator != m_targets.end())
            {
                m_targets.erase(iterator);
            }

            token.invalidate();
        }

        template<typename...TArgs>
        auto operator()(TArgs&&... args) -> void
        {
            delegate_array temp_targets;
            {
                std::unique_lock<std::mutex> lock(m_change);
                temp_targets = m_targets;
            }

            for (auto const& element : temp_targets)
            {
                std::get<1>(element)(std::forward<TArgs>(args)...);
            }
        }

    private:

        auto get_next_token() noexcept -> event_token
        {
            return event_token{ m_next_token++ };
        }

        using delegate_array = std::vector<std::pair<event_token, Delegate>>;

        int64_t m_next_token{ 0 };
        delegate_array m_targets;
        std::mutex m_change;
    };

    template <typename T>
    struct fast_iterator
    {
        using iterator_category = std::input_iterator_tag;
        using value_type = T;
        using difference_type = ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        fast_iterator(T const& collection, uint32_t const index) noexcept :
            m_collection(&collection),
            m_index(index)
        {}

        auto operator++() noexcept -> fast_iterator&
        {
            ++m_index;
            return*this;
        }

        auto operator*() const
        {
            return m_collection->GetAt(m_index);
        }

        auto operator==(fast_iterator const& other) const noexcept -> bool
        {
            return m_collection == other.m_collection && m_index == other.m_index;
        }

        auto operator!=(fast_iterator const& other) const noexcept -> bool
        {
            return !(*this == other);
        }

    private:

        T const* m_collection = nullptr;
        uint32_t m_index = 0;
    };

    template <typename T>
    auto begin(T const& collection) noexcept -> fast_iterator<T>
    {
        return fast_iterator<T>(collection, 0);
    }

    template <typename T>
    auto end(T const& collection) -> fast_iterator<T>
    {
        return fast_iterator<T>(collection, collection.Size());
    }

    template <typename T>
    struct IVector
    {
        virtual auto GetAt(uint32_t const index) const -> T = 0;
        virtual auto SetAt(uint32_t const index, T const& value) -> void = 0;
        virtual auto InsertAt(uint32_t const index, T const& value) -> void = 0;
        virtual auto Append(T const& value) -> void = 0;
        virtual auto RemoveAt(uint32_t const index) -> void = 0;
        virtual auto Clear() -> void = 0;
        virtual auto Size() const -> uint32_t = 0;
        virtual ~IVector() = default;
    };

    template <typename K, typename V>
    struct key_value_pair
    {
        key_value_pair(K key, V value) :
            m_key(std::move(key)),
            m_value(std::move(value))
        {
        }

        auto Key() const -> K
        {
            return m_key;
        }

        auto Value() const -> V
        {
            return m_value;
        }

    private:

        K const m_key;
        V const m_value;
    };

    template <typename K, typename V>
    struct IMap
    {
        virtual auto Lookup(K const& key) const -> V = 0;
        virtual auto Clear() -> void = 0;
        virtual auto Size() const -> uint32_t = 0;
        virtual auto GetAt(uint32_t index) const -> key_value_pair<K, V> = 0;
        virtual auto HasKey(K const& key) const -> bool = 0;
        virtual auto Insert(K const& key, V const& value) -> bool = 0;
        virtual auto Remove(K const& key) -> void = 0;
        virtual ~IMap() = default;
    };

    template<typename T>
    class CookieTracker
    {
    public:
        auto getCookie(const std::shared_ptr<T>& instance) -> ssc_callback_cookie
        {
            auto pair = m_tracked.emplace(reinterpret_cast<ssc_callback_cookie>(instance.get()), instance);
            return pair.first->first;
        }

        auto lookup(ssc_callback_cookie cookie) -> std::shared_ptr<T>
        {
            auto itr = m_tracked.find(cookie);
            if (itr == m_tracked.end())
            {
                return nullptr;
            }
            auto result = itr->second.lock();
            if (result == nullptr)
            {
                m_tracked.erase(cookie);
            }
            return result;
        }

        void remove(T* instance)
        {
            m_tracked.erase(reinterpret_cast<ssc_callback_cookie>(instance));
        }

    private:
        std::unordered_map<ssc_callback_cookie, std::weak_ptr<T>> m_tracked;
    };

    /**
     * Defines logging severity levels.
     */
    enum class SessionLogLevel : int32_t
    {
        /**
         * Specifies that logging should not write any messages.
         */
        None = 0,
        /**
         * Specifies logs that indicate when the current flow of execution stops due to a failure.
         */
        Error = 1,
        /**
         * Specifies logs that highlight an abnormal or unexpected event, but do not otherwise cause execution to stop.
         */
        Warning = 2,
        /**
         * Specifies logs that track the general flow.
         */
        Information = 3,
        /**
         * Specifies logs used for interactive investigation during development.
         */
        Debug = 4,
        /**
         * Specifies all messages should be logged.
         */
        All = 5,
    };

    /**
     * Use this enumeration to determine whether an anchor was located, and the reason why it may have failed.
     */
    enum class LocateAnchorStatus : int32_t
    {
        /**
         * The anchor was already being tracked.
         */
        AlreadyTracked = 0,
        /**
         * The anchor was found.
         */
        Located = 1,
        /**
         * The anchor was not found.
         */
        NotLocated = 2,
        /**
         * The anchor cannot be found - it was deleted or the identifier queried for was incorrect.
         */
        NotLocatedAnchorDoesNotExist = 3,
    };

    /**
     * Use this enumeration to indicate the method by which anchors can be located.
     */
    enum class LocateStrategy : int32_t
    {
        /**
         * Indicates that any method is acceptable.
         */
        AnyStrategy = 0,
        /**
         * Indicates that anchors will be located primarily by visual information.
         */
        VisualInformation = 1,
        /**
         * Indicates that anchors will be located primarily by relationship to other anchors.
         */
        Relationship = 2,
    };

    /**
     * Possible values returned when querying PlatformLocationProvider for GeoLocation capabilities
     */
    enum class GeoLocationStatusResult : int32_t
    {
        /**
         * GeoLocation data is available.
         */
        Available = 0,
        /**
         * GeoLocation was disabled in the SensorCapabilities.
         */
        DisabledCapability = 1,
        /**
         * No sensor fingerprint provider has been created.
         */
        MissingSensorFingerprintProvider = 2,
        /**
         * No GPS data has been received.
         */
        NoGPSData = 3,
    };

    /**
     * Possible values returned when querying PlatformLocationProvider for Wifi capabilities
     */
    enum class WifiStatusResult : int32_t
    {
        /**
         * Wifi data is available.
         */
        Available = 0,
        /**
         * Wifi was disabled in the SensorCapabilities.
         */
        DisabledCapability = 1,
        /**
         * No sensor fingerprint provider has been created.
         */
        MissingSensorFingerprintProvider = 2,
        /**
         * No Wifi access points have been found.
         */
        NoAccessPointsFound = 3,
    };

    /**
     * Possible values returned when querying PlatformLocationProvider for Bluetooth capabilities
     */
    enum class BluetoothStatusResult : int32_t
    {
        /**
         * Bluetooth beacons data is available.
         */
        Available = 0,
        /**
         * Bluetooth was disabled in the SensorCapabilities.
         */
        DisabledCapability = 1,
        /**
         * No sensor fingerprint provider has been created.
         */
        MissingSensorFingerprintProvider = 2,
        /**
         * No bluetooth beacons have been found.
         */
        NoBeaconsFound = 3,
    };

    /**
     * Use this enumeration to describe the kind of feedback that can be provided to the user about data
     */
    enum class SessionUserFeedback : int32_t
    {
        /**
         * No specific feedback is available.
         */
        None = 0,
        /**
         * Device is not moving enough to create a neighborhood of key-frames.
         */
        NotEnoughMotion = 1,
        /**
         * Device is moving too quickly for stable tracking.
         */
        MotionTooQuick = 2,
        /**
         * The environment doesn't have enough feature points for stable tracking.
         */
        NotEnoughFeatures = 4,
    };

    /**
     * Identifies the source of an error in a cloud spatial session.
     */
    enum class CloudSpatialErrorCode : int32_t
    {
        /**
         * Amount of Metadata exceeded the allowed limit (currently 4k).
         */
        MetadataTooLarge = 0,
        /**
         * Application did not provide valid credentials and therefore could not authenticate with the Cloud Service.
         */
        ApplicationNotAuthenticated = 1,
        /**
         * Application did not provide any credentials for authorization with the Cloud Service.
         */
        ApplicationNotAuthorized = 2,
        /**
         * Multiple stores (on the same device or different devices) made concurrent changes to the same Spatial Entity and so this particular change was rejected.
         */
        ConcurrencyViolation = 3,
        /**
         * Not enough Neighborhood Spatial Data was available to complete the desired Create operation.
         */
        NotEnoughSpatialData = 4,
        /**
         * No Spatial Location Hint was available (or it was not specific enough) to support rediscovery from the Cloud at a later time.
         */
        NoSpatialLocationHint = 5,
        /**
         * Application cannot connect to the Cloud Service.
         */
        CannotConnectToServer = 6,
        /**
         * Cloud Service returned an unspecified error.
         */
        ServerError = 7,
        /**
         * The Spatial Entity has already been associated with a different Store object, so cannot be used with this current Store object.
         */
        AlreadyAssociatedWithADifferentStore = 8,
        /**
         * SpatialEntity already exists in a Store but TryCreateAsync was called.
         */
        AlreadyExists = 9,
        /**
         * A locate operation was requested, but the criteria does not specify anything to look for.
         */
        NoLocateCriteriaSpecified = 10,
        /**
         * An access token was required but not specified; handle the TokenRequired event on the session to provide one.
         */
        NoAccessTokenSpecified = 11,
        /**
         * The session was unable to obtain an access token and so the creation could not proceed.
         */
        UnableToObtainAccessToken = 12,
        /**
         * There were too many requests made from this Account ID, so it is being throttled.
         */
        TooManyRequests = 13,
        /**
         * The LocateCriteria options that were specified are not valid because they're missing a required value.
         */
        LocateCriteriaMissingRequiredValues = 14,
        /**
         * The LocateCriteria options that were specified are not valid because they're in conflict with settings for another mode.
         */
        LocateCriteriaInConflict = 15,
        /**
         * The LocateCriteria options that were specified are not valid values.
         */
        LocateCriteriaInvalid = 16,
        /**
         * The LocateCriteria options that were specified are not valid because they're not currently supported.
         */
        LocateCriteriaNotSupported = 17,
        /**
         * Encountered an unknown error on the session.
         */
        Unknown = 19,
        /**
         * The Http request timed out.
         */
        HttpTimeout = 20,
    };

    /**
     * Use the data category values to determine what data is returned in an AnchorLocateCriteria object.
     */
    enum class AnchorDataCategory : int32_t
    {
        /**
         * No data is returned.
         */
        None = 0,
        /**
         * Returns Anchor properties including AppProperties.
         */
        Properties = 1,
        /**
         * Returns spatial information about an Anchor.
         *
         * Returns a LocalAnchor for any returned CloudSpatialAnchors from the service.
         */
        Spatial = 2,
    };

    enum class Status : int32_t
    {
        /**
         * Success
         */
        OK = 0,
        /**
         * Failed
         */
        Failed = 1,
        /**
         * Cannot access a disposed object.
         */
        ObjectDisposed = 2,
        /**
         * Out of memory.
         */
        OutOfMemory = 12,
        /**
         * Invalid argument.
         */
        InvalidArgument = 22,
        /**
         * The value is out of range.
         */
        OutOfRange = 34,
        /**
         * Not implemented.
         */
        NotImplemented = 38,
        /**
         * The key does not exist in the collection.
         */
        KeyNotFound = 77,
        /**
         * Amount of Metadata exceeded the allowed limit (currently 4k).
         */
        MetadataTooLarge = 78,
        /**
         * Application did not provide valid credentials and therefore could not authenticate with the Cloud Service.
         */
        ApplicationNotAuthenticated = 79,
        /**
         * Application did not provide any credentials for authorization with the Cloud Service.
         */
        ApplicationNotAuthorized = 80,
        /**
         * Multiple stores (on the same device or different devices) made concurrent changes to the same Spatial Entity and so this particular change was rejected.
         */
        ConcurrencyViolation = 81,
        /**
         * Not enough Neighborhood Spatial Data was available to complete the desired Create operation.
         */
        NotEnoughSpatialData = 82,
        /**
         * No Spatial Location Hint was available (or it was not specific enough) to support rediscovery from the Cloud at a later time.
         */
        NoSpatialLocationHint = 83,
        /**
         * Application cannot connect to the Cloud Service.
         */
        CannotConnectToServer = 84,
        /**
         * Cloud Service returned an unspecified error.
         */
        ServerError = 85,
        /**
         * The Spatial Entity has already been associated with a different Store object, so cannot be used with this current Store object.
         */
        AlreadyAssociatedWithADifferentStore = 86,
        /**
         * SpatialEntity already exists in a Store but TryCreateAsync was called.
         */
        AlreadyExists = 87,
        /**
         * A locate operation was requested, but the criteria does not specify anything to look for.
         */
        NoLocateCriteriaSpecified = 88,
        /**
         * An access token was required but not specified; handle the TokenRequired event on the session to provide one.
         */
        NoAccessTokenSpecified = 89,
        /**
         * The session was unable to obtain an access token and so the creation could not proceed.
         */
        UnableToObtainAccessToken = 90,
        /**
         * There were too many requests made from this Account ID, so it is being throttled.
         */
        TooManyRequests = 91,
        /**
         * The LocateCriteria options that were specified are not valid because they're missing a required value.
         */
        LocateCriteriaMissingRequiredValues = 92,
        /**
         * The LocateCriteria options that were specified are not valid because they're in conflict with settings for another mode.
         */
        LocateCriteriaInConflict = 93,
        /**
         * The LocateCriteria options that were specified are not valid values.
         */
        LocateCriteriaInvalid = 94,
        /**
         * The LocateCriteria options that were specified are not valid because they're not currently supported.
         */
        LocateCriteriaNotSupported = 95,
        /**
         * Encountered an unknown error on the session.
         */
        Unknown = 96,
        /**
         * The Http request timed out.
         */
        HttpTimeout = 97,
    };

    /**
     * Informs the application that a locate operation has completed.
     * 
     * @param sender The session that ran the locate operation.
     * @param args The arguments describing the operation completion.
     */
    using LocateAnchorsCompletedDelegate = std::function<void(void*, const std::shared_ptr<Microsoft::Azure::SpatialAnchors::LocateAnchorsCompletedEventArgs> &)>;
    /**
     * Informs the application that a session requires an updated access token or authentication token.
     * 
     * @param sender The session that requires an updated access token or authentication token.
     * @param args The event arguments that require an AccessToken property or an AuthenticationToken property to be set.
     */
    using TokenRequiredDelegate = std::function<void(void*, const std::shared_ptr<Microsoft::Azure::SpatialAnchors::TokenRequiredEventArgs> &)>;
    /**
     * Informs the application that a session has located an anchor or discovered that it cannot yet be located.
     * 
     * @param sender The session that fires the event.
     * @param args Information about the located anchor.
     */
    using AnchorLocatedDelegate = std::function<void(void*, const std::shared_ptr<Microsoft::Azure::SpatialAnchors::AnchorLocatedEventArgs> &)>;
    /**
     * Informs the application that a session has been updated with new information.
     * 
     * @param sender The session that has been updated.
     * @param args Information about the current session status.
     */
    using SessionUpdatedDelegate = std::function<void(void*, const std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionUpdatedEventArgs> &)>;
    /**
     * Informs the application that an error occurred in a session.
     * 
     * @param sender The session that fired the event.
     * @param args Information about the error.
     */
    using SessionErrorDelegate = std::function<void(void*, const std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionErrorEventArgs> &)>;
    /**
     * Informs the application of a debug log message.
     * 
     * @param sender The session that fired the event.
     * @param args Information about the log.
     */
    using OnLogDebugDelegate = std::function<void(void*, const std::shared_ptr<Microsoft::Azure::SpatialAnchors::OnLogDebugEventArgs> &)>;
    /**
     * Informs the application that a session is requesting an updated sensor fingerprint to help with anchor recall.
     * 
     * @param sender The session that is requesting optional sensor data for improving recall accuracy over time.
     * @param args The event arguments that allow sensor properties to be set.
     */
    using UpdatedSensorFingerprintRequiredDelegate = std::function<void(void*, const std::shared_ptr<Microsoft::Azure::SpatialAnchors::SensorFingerprintEventArgs> &)>;

    /**
     * Specifies a set of criteria for locating anchors.
     *
     * Within the object, properties are combined with the AND operator. For example, if identifiers and nearAnchor are specified, then the filter will look for anchors that are near the nearAnchor and have an identifier that matches any of those identifiers. At least one criterion must be provided.
     */
    class AnchorLocateCriteria final : public std::enable_shared_from_this<AnchorLocateCriteria>
    {
    public:
        /**
         * Initializes a new AnchorLocateCriteria instance.
         */
        AnchorLocateCriteria();
        AnchorLocateCriteria(void* handle, bool noCopy = true);
        ~AnchorLocateCriteria();

        /**
         * Whether locate should bypass the local cache of anchors.
         */
        auto BypassCache() const -> bool;
        auto BypassCache(bool value) -> void;
        /**
         * Categories of data that are requested.
         */
        auto RequestedCategories() const -> Microsoft::Azure::SpatialAnchors::AnchorDataCategory;
        auto RequestedCategories(Microsoft::Azure::SpatialAnchors::AnchorDataCategory const& value) -> void;
        /**
         * Indicates the strategy by which anchors will be located.
         */
        auto Strategy() const -> Microsoft::Azure::SpatialAnchors::LocateStrategy;
        auto Strategy(Microsoft::Azure::SpatialAnchors::LocateStrategy const& value) -> void;
        /**
         * Indicates the CloudSpatialAnchor identifiers to locate. Maximum limit of 35 anchors per watcher.
         *
         * Any anchors within this list will match this criteria.
         */
        auto Identifiers() const -> std::vector<std::string>;
        auto Identifiers(std::vector<std::string> const& value) -> void;
        /**
         * Indicates that anchors to locate should be close to a specific anchor.
         */
        auto NearAnchor() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearAnchorCriteria>;
        auto NearAnchor(std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearAnchorCriteria> const& value) -> void;
        /**
         * Indicates that anchors to locate should be close to the device.
         *
         * Any enabled sensors will be used to help discover anchors around your device. To have the best chance of finding anchors, you should configure the SensorCapabilities to give the session access to all appropriate sensors.
         */
        auto NearDevice() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearDeviceCriteria>;
        auto NearDevice(std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearDeviceCriteria> const& value) -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;
        std::vector<std::string> m_identifiers;
    };

    /**
     * Use this type to determine the status of an anchor after a locate operation.
     */
    class AnchorLocatedEventArgs final : public std::enable_shared_from_this<AnchorLocatedEventArgs>
    {
    public:
        AnchorLocatedEventArgs(void* handle, bool noCopy = true);
        ~AnchorLocatedEventArgs();

        /**
         * The cloud spatial anchor that was located.
         */
        auto Anchor() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor>;
        /**
         * The spatial anchor that was located.
         */
        auto Identifier() const -> std::string;
        /**
         * Specifies whether the anchor was located, or the reason why it may have failed.
         */
        auto Status() const -> Microsoft::Azure::SpatialAnchors::LocateAnchorStatus;
        /**
         * Gets the LocateStrategy that reflects the strategy that was used to find the anchor. Valid only when the anchor was found.
         */
        auto Strategy() const -> Microsoft::Azure::SpatialAnchors::LocateStrategy;
        /**
         * The watcher that located the anchor.
         */
        auto Watcher() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Use this class to represent an anchor in space that can be persisted in a CloudSpatialAnchorSession.
     */
    class CloudSpatialAnchor final : public std::enable_shared_from_this<CloudSpatialAnchor>
    {
    public:
        CloudSpatialAnchor();
        CloudSpatialAnchor(void* handle, bool noCopy = true);
        ~CloudSpatialAnchor();

        /**
         * The anchor in the local mixed reality system.
         */
        auto LocalAnchor() const -> ArAnchor*;
        auto LocalAnchor(ArAnchor* const& value) -> void;
        /**
         * The time the anchor will expire.
         */
        auto Expiration() const -> int64_t;
        auto Expiration(int64_t value) -> void;
        /**
         * The persistent identifier of this spatial anchor in the cloud service.
         */
        auto Identifier() const -> std::string;
        /**
         * A dictionary of application-defined properties that is stored with the anchor.
         */
        auto AppProperties() const -> std::shared_ptr<IMap<std::string, std::string>>;
        /**
         * An opaque version tag that can be used for concurrency control.
         */
        auto VersionTag() const -> std::string;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Use this class to represent a map that was created by TODO.
     */
    class CloudSpatialAnchorMap final : public std::enable_shared_from_this<CloudSpatialAnchorMap>
    {
    public:
        CloudSpatialAnchorMap(void* handle, bool noCopy = true);
        ~CloudSpatialAnchorMap();

        /**
         * The persistent identifier of this map in the cloud service.
         */
        auto Identifier() const -> std::string;
        /**
         * The name of this map in the cloud service.
         */
        auto Name() const -> std::string;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Use this class to defer completing an operation.
     *
     * This is similar to the Windows.Foundation.Deferral class.
     */
    class CloudSpatialAnchorSessionDeferral final : public std::enable_shared_from_this<CloudSpatialAnchorSessionDeferral>
    {
    public:
        CloudSpatialAnchorSessionDeferral(void* handle, bool noCopy = true);
        ~CloudSpatialAnchorSessionDeferral();

        /**
         * Mark the deferred operation as complete and perform any associated tasks.
         */
        auto Complete() -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Use this class to configure session diagnostics that can be collected and submitted to improve system quality.
     */
    class CloudSpatialAnchorSessionDiagnostics final : public std::enable_shared_from_this<CloudSpatialAnchorSessionDiagnostics>
    {
    public:
        CloudSpatialAnchorSessionDiagnostics(void* handle, bool noCopy = true);
        ~CloudSpatialAnchorSessionDiagnostics();

        /**
         * Level of tracing to log.
         */
        auto LogLevel() const -> Microsoft::Azure::SpatialAnchors::SessionLogLevel;
        auto LogLevel(Microsoft::Azure::SpatialAnchors::SessionLogLevel const& value) -> void;
        /**
         * Directory into which temporary log files and manifests are saved.
         */
        auto LogDirectory() const -> std::string;
        auto LogDirectory(std::string const& value) -> void;
        /**
         * Approximate maximum disk space to be used, in megabytes.
         *
         * When this value is set to zero, no information will be written to disk.
         */
        auto MaxDiskSizeInMB() const -> int32_t;
        auto MaxDiskSizeInMB(int32_t value) -> void;
        /**
         * Whether images should be logged.
         */
        auto ImagesEnabled() const -> bool;
        auto ImagesEnabled(bool value) -> void;
        /**
         * Creates a manifest of log files and submission information to be uploaded.
         * 
         * @param description Description to be added to the diagnostics manifest.
         */
        auto CreateManifestAsync(std::string const& description, std::function<void(Status, const std::string &)> callback) -> void;
        /**
         * Submits a diagnostics manifest and cleans up its resources.
         * 
         * @param manifestPath Path to the manifest file to submit.
         */
        auto SubmitManifestAsync(std::string const& manifestPath, std::function<void(Status)> callback) -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Use this class to create, locate and manage spatial anchors.
     */
    class CloudSpatialAnchorSession final : public std::enable_shared_from_this<CloudSpatialAnchorSession>
    {
    public:
        /**
         * Initializes a new instance with a default configuration.
         */
        CloudSpatialAnchorSession();
        CloudSpatialAnchorSession(void* handle, bool noCopy = true);
        ~CloudSpatialAnchorSession();

        /**
         * The configuration information for the session.
         *
         * Configuration settings take effect when the session is started.
         */
        auto Configuration() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionConfiguration>;
        /**
         * The diagnostics settings for the session, which can be used to collect and submit data for troubleshooting and improvements.
         */
        auto Diagnostics() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSessionDiagnostics>;
        /**
         * Logging level for the session log events.
         */
        auto LogLevel() const -> Microsoft::Azure::SpatialAnchors::SessionLogLevel;
        auto LogLevel(Microsoft::Azure::SpatialAnchors::SessionLogLevel const& value) -> void;
        /**
         * The tracking session used to help locate anchors.
         *
         * This property is not available on the HoloLens platform.
         */
        auto Session() const -> ArSession*;
        auto Session(ArSession* const& value) -> void;
        /**
         * Location provider used to create and locate anchors.
         */
        auto LocationProvider() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::PlatformLocationProvider>;
        auto LocationProvider(std::shared_ptr<Microsoft::Azure::SpatialAnchors::PlatformLocationProvider> const& value) -> void;
        /**
         * The unique identifier for the session.
         */
        auto SessionId() const -> std::string;
        /**
         * Occurs when the session requires an updated access token or authentication token.
         */
        auto TokenRequired(TokenRequiredDelegate const& handler) -> event_token;
        auto TokenRequired(event_token& token) -> void;
        /**
         * Occurs when an anchor's location is determined.
         */
        auto AnchorLocated(AnchorLocatedDelegate const& handler) -> event_token;
        auto AnchorLocated(event_token& token) -> void;
        /**
         * Occurs when all the results from a watcher that is locating anchors are processed.
         */
        auto LocateAnchorsCompleted(LocateAnchorsCompletedDelegate const& handler) -> event_token;
        auto LocateAnchorsCompleted(event_token& token) -> void;
        /**
         * Occurs when all the session state is updated.
         */
        auto SessionUpdated(SessionUpdatedDelegate const& handler) -> event_token;
        auto SessionUpdated(event_token& token) -> void;
        /**
         * Occurs when the session is unable to continue processing.
         */
        auto Error(SessionErrorDelegate const& handler) -> event_token;
        auto Error(event_token& token) -> void;
        /**
         * Occurs when a debug log message is generated.
         */
        auto OnLogDebug(OnLogDebugDelegate const& handler) -> event_token;
        auto OnLogDebug(event_token& token) -> void;
        /**
         * Occurs when the session requests an updated sensor fingerprint from the application.
         */
        auto UpdatedSensorFingerprintRequired(UpdatedSensorFingerprintRequiredDelegate const& handler) -> event_token;
        auto UpdatedSensorFingerprintRequired(event_token& token) -> void;
        /**
         * Stops this session and releases all associated resources.
         */
        auto Dispose() -> void;
        /**
         * Gets the Azure Spatial Anchors access token from authentication token.
         * 
         * @param authenticationToken Authentication token.
         */
        auto GetAccessTokenWithAuthenticationTokenAsync(std::string const& authenticationToken, std::function<void(Status, const std::string &)> callback) -> void;
        /**
         * Gets the Azure Spatial Anchors access token from account key.
         * 
         * @param accountKey Account key.
         */
        auto GetAccessTokenWithAccountKeyAsync(std::string const& accountKey, std::function<void(Status, const std::string &)> callback) -> void;
        /**
         * Creates a new persisted spatial anchor from the specified local anchor and string properties.
         * 
         * @param anchor Anchor to be persisted.
         */
        auto CreateAnchorAsync(std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> const& anchor, std::function<void(Status)> callback) -> void;
        /**
         * Creates a new object that watches for anchors that meet the specified criteria.
         * 
         * @param criteria Criteria for anchors to watch for.
         */
        auto CreateWatcher(std::shared_ptr<Microsoft::Azure::SpatialAnchors::AnchorLocateCriteria> const& criteria) -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>;
        /**
         * Gets a cloud spatial anchor for the given identifier, even if it hasn't been located yet.
         * 
         * @param identifier The identifier to look for.
         */
        auto GetAnchorPropertiesAsync(std::string const& identifier, std::function<void(Status, const std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> &)> callback) -> void;
        /**
         * Gets a list of all nearby cloud spatial anchor ids corresponding to a given criteria.
         * 
         * @param criteria The search criteria.
         */
        auto GetNearbyAnchorIdsAsync(std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearDeviceCriteria> const& criteria, std::function<void(Status, std::shared_ptr<IVector<std::string>>)> callback) -> void;
        /**
         * Gets a list of active watchers.
         */
        auto GetActiveWatchers() -> std::vector<std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>>;
        /**
         * Refreshes properties for the specified spatial anchor.
         * 
         * @param anchor The anchor to refresh.
         */
        auto RefreshAnchorPropertiesAsync(std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> const& anchor, std::function<void(Status)> callback) -> void;
        /**
         * Updates the specified spatial anchor.
         * 
         * @param anchor The anchor to be updated.
         */
        auto UpdateAnchorPropertiesAsync(std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> const& anchor, std::function<void(Status)> callback) -> void;
        /**
         * Deletes a persisted spatial anchor.
         * 
         * @param anchor The anchor to be deleted.
         */
        auto DeleteAnchorAsync(std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> const& anchor, std::function<void(Status)> callback) -> void;
        /**
         * Applications must call this method on platforms where per-frame processing is required.
         *
         * This method is not available on the HoloLens platform.
         * 
         * @param frame AR frame to process.
         */
        auto ProcessFrame(ArFrame* const& frame) -> void;
        /**
         * Gets an object describing the status of the session.
         */
        auto GetSessionStatusAsync(std::function<void(Status, const std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionStatus> &)> callback) -> void;
        /**
         * Begins capturing environment data for the session.
         */
        auto Start() -> void;
        /**
         * Stops capturing environment data for the session and cancels any outstanding locate operations. Environment data is maintained.
         */
        auto Stop() -> void;
        /**
         * Resets environment data that has been captured in this session; applications must call this method when tracking is lost.
         *
         * On any platform, calling the method will clean all internal cached state.
         */
        auto Reset() -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;

        event<TokenRequiredDelegate> m_tokenRequiredEvent;
        event<AnchorLocatedDelegate> m_anchorLocatedEvent;
        event<LocateAnchorsCompletedDelegate> m_locateAnchorsCompletedEvent;
        event<SessionUpdatedDelegate> m_sessionUpdatedEvent;
        event<SessionErrorDelegate> m_errorEvent;
        event<OnLogDebugDelegate> m_onLogDebugEvent;
        event<UpdatedSensorFingerprintRequiredDelegate> m_updatedSensorFingerprintRequiredEvent;
        inline static CookieTracker<CloudSpatialAnchorSession> s_cookieTracker;

    };

    /**
     * Use this class to control an object that watches for spatial anchors.
     */
    class CloudSpatialAnchorWatcher final : public std::enable_shared_from_this<CloudSpatialAnchorWatcher>
    {
    public:
        CloudSpatialAnchorWatcher(void* handle, bool noCopy = true);
        ~CloudSpatialAnchorWatcher();

        /**
         * Distinct identifier for the watcher within its session.
         */
        auto Identifier() const -> int32_t;
        /**
         * Stops further activity from this watcher.
         */
        auto Stop() -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Contains optional geographical location information within a sensor fingerprint.
     *
     * If any of the values are unknown, they should not be set or can be set to NaN.
     */
    class GeoLocation final : public std::enable_shared_from_this<GeoLocation>
    {
    public:
        GeoLocation();
        GeoLocation(void* handle, bool noCopy = true);
        ~GeoLocation();

        /**
         * The current latitude of the device in degrees.
         */
        auto Latitude() const -> double;
        auto Latitude(double value) -> void;
        /**
         * The current longitude of the device in degrees.
         */
        auto Longitude() const -> double;
        auto Longitude(double value) -> void;
        /**
         * The horizontal error in meters of the latitude and longitude. This corresponds to the radius of a 68.3% confidence region on the East/North plane. Over many invocations, the true position should be within this number of horizontal meters of the reported position.
         */
        auto HorizontalError() const -> float;
        auto HorizontalError(float value) -> void;
        /**
         * The altitude of the device in meters.
         */
        auto Altitude() const -> float;
        auto Altitude(float value) -> void;
        /**
         * The vertical error of the altitude in meters. This corresponds to a one-sided 68.3% confidence interval along the Up axis. Over many invocations, the true altitude should be within this number of meters of the reported altitude.
         */
        auto VerticalError() const -> float;
        auto VerticalError(float value) -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Use this type to determine when a locate operation has completed.
     */
    class LocateAnchorsCompletedEventArgs final : public std::enable_shared_from_this<LocateAnchorsCompletedEventArgs>
    {
    public:
        LocateAnchorsCompletedEventArgs(void* handle, bool noCopy = true);
        ~LocateAnchorsCompletedEventArgs();

        /**
         * Gets a value indicating whether the locate operation was canceled.
         *
         * When this property is true, the watcher was stopped before completing.
         */
        auto Cancelled() const -> bool;
        /**
         * The watcher that completed the locate operation.
         */
        auto Watcher() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Use this class to describe how anchors to be located should be near a source anchor.
     */
    class NearAnchorCriteria final : public std::enable_shared_from_this<NearAnchorCriteria>
    {
    public:
        NearAnchorCriteria();
        NearAnchorCriteria(void* handle, bool noCopy = true);
        ~NearAnchorCriteria();

        /**
         * Source anchor around which nearby anchors should be located.
         */
        auto SourceAnchor() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor>;
        auto SourceAnchor(std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> const& value) -> void;
        /**
         * Maximum distance in meters from the source anchor (defaults to 5).
         */
        auto DistanceInMeters() const -> float;
        auto DistanceInMeters(float value) -> void;
        /**
         * Maximum desired result count (defaults to 20).
         */
        auto MaxResultCount() const -> int32_t;
        auto MaxResultCount(int32_t value) -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Use this class to describe how anchors to be located should be near the device.
     */
    class NearDeviceCriteria final : public std::enable_shared_from_this<NearDeviceCriteria>
    {
    public:
        NearDeviceCriteria();
        NearDeviceCriteria(void* handle, bool noCopy = true);
        ~NearDeviceCriteria();

        /**
         * Maximum distance in meters from the device (defaults to 5).
         */
        auto DistanceInMeters() const -> float;
        auto DistanceInMeters(float value) -> void;
        /**
         * Maximum desired result count (defaults to 20).
         */
        auto MaxResultCount() const -> int32_t;
        auto MaxResultCount(int32_t value) -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Provides data for the event that fires for logging messages.
     */
    class OnLogDebugEventArgs final : public std::enable_shared_from_this<OnLogDebugEventArgs>
    {
    public:
        OnLogDebugEventArgs(void* handle, bool noCopy = true);
        ~OnLogDebugEventArgs();

        /**
         * The logging message.
         */
        auto Message() const -> std::string;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Use this class to get a location estimate.
     */
    class PlatformLocationProvider final : public std::enable_shared_from_this<PlatformLocationProvider>
    {
    public:
        /**
         * Initializes a new instance with all sensors disabled.
         */
        PlatformLocationProvider();
        PlatformLocationProvider(void* handle, bool noCopy = true);
        ~PlatformLocationProvider();

        /**
         * The sensors used by the session to locate anchors around you and annotate created anchors so that they can be found.
         *
         * On HoloLens and iOS, enabling a sensor for the first time will prompt the user for the necessary permissions. On Android, you need to obtain the relevant permission before enabling a sensor.
         */
        auto Sensors() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::SensorCapabilities>;
        /**
         * Checks whether sufficient sensor data is available to locate or create anchors tagged with geolocation.
         */
        auto GeoLocationStatus() const -> Microsoft::Azure::SpatialAnchors::GeoLocationStatusResult;
        /**
         * Checks whether sufficient sensor data is available to locate or create anchors tagged with Wi-Fi signals.
         */
        auto WifiStatus() const -> Microsoft::Azure::SpatialAnchors::WifiStatusResult;
        /**
         * Checks whether sufficient sensor data is available to locate or create anchors tagged with Bluetooth signals.
         */
        auto BluetoothStatus() const -> Microsoft::Azure::SpatialAnchors::BluetoothStatusResult;
        /**
         * Returns the latest estimate of the device's location.
         */
        auto GetLocationEstimate() -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::GeoLocation>;
        /**
         * Start tracking the device's location.
         */
        auto Start() -> void;
        /**
         * Stop tracking the device's location.
         */
        auto Stop() -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Use this class to give the session access to sensors to help find anchors around you.
     */
    class SensorCapabilities final : public std::enable_shared_from_this<SensorCapabilities>
    {
    public:
        SensorCapabilities(void* handle, bool noCopy = true);
        ~SensorCapabilities();

        /**
         * Whether to use the device's global position to find anchors and improve the locatability of existing anchors.
         *
         * Enabling this option requires extra permissions on each platform: - Android: Declare ACCESS_FINE_LOCATION in AndroidManifest.xml and obtain the permission at run-time by calling ActivityCompat.requestPermissions(). - HoloLens: Add the "location" capability to your app's package manifest. - iOS: Add the "Privacy - Location When In Use Usage Description" key to Info.plist with a short description of what the permission is for.
         */
        auto GeoLocationEnabled() const -> bool;
        auto GeoLocationEnabled(bool value) -> void;
        /**
         * Whether to use WiFi signals to find anchors and improve the locatability of existing anchors.
         *
         * Enabling this option requires extra permissions on each platform: - Android: Declare CHANGE_WIFI_STATE, ACCESS_WIFI_STATE and ACCESS_COARSE_LOCATION in AndroidManifest.xml and obtain the permissions at run-time by calling ActivityCompat.requestPermissions(). - HoloLens: Add the "wiFiControl" capability to your app's package manifest. - iOS: Add the "Privacy - Location When In Use Usage Description" key to Info.plist with a short description of what the permission is for.
         */
        auto WifiEnabled() const -> bool;
        auto WifiEnabled(bool value) -> void;
        /**
         * Whether to use Bluetooth signals to find anchors and improve the locatability of existing anchors.
         *
         * Enabling this option requires extra permissions on each platform: - Android: Declare BLUETOOTH_ADMIN, BLUETOOTH and ACCESS_COARSE_LOCATION in AndroidManifest.xml and obtain the permissions at run-time by calling ActivityCompat.requestPermissions(). - HoloLens: Add the "bluetooth" capability to your app's package manifest. - iOS: Add the "Privacy - Location When In Use Usage Description" key to Info.plist with a short description of what the permission is for.
         */
        auto BluetoothEnabled() const -> bool;
        auto BluetoothEnabled(bool value) -> void;
        /**
         * Controls which Bluetooth beacon devices the session is able to see. Add the proximity UUIDs here for all beacons you want to use to find anchors and improve the locatability of existing anchors.
         *
         * Only Eddystone-Uid and iBeacon UUIDs are supported. If no UUIDs are provided, Bluetooth beacons will not be tracked.
         */
        auto KnownBeaconProximityUuids() const -> std::vector<std::string>;
        auto KnownBeaconProximityUuids(std::vector<std::string> const& value) -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;
        std::vector<std::string> m_knownBeaconProximityUuids;
    };

    /**
     * Informs the application that the service would like an updated sensor fingerprint.
     */
    class SensorFingerprintEventArgs final : public std::enable_shared_from_this<SensorFingerprintEventArgs>
    {
    public:
        SensorFingerprintEventArgs(void* handle, bool noCopy = true);
        ~SensorFingerprintEventArgs();

        /**
         * The current geographical position of the device if available.
         */
        auto GeoPosition() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::GeoLocation>;
        auto GeoPosition(std::shared_ptr<Microsoft::Azure::SpatialAnchors::GeoLocation> const& value) -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Use this class to set up the service configuration for a SpatialAnchorSession.
     */
    class SessionConfiguration final : public std::enable_shared_from_this<SessionConfiguration>
    {
    public:
        SessionConfiguration(void* handle, bool noCopy = true);
        ~SessionConfiguration();

        /**
         * Account domain for the Azure Spatial Anchors service.
         *
         * The default is "mixedreality.azure.com".
         */
        auto AccountDomain() const -> std::string;
        auto AccountDomain(std::string const& value) -> void;
        /**
         * Account-level ID for the Azure Spatial Anchors service.
         */
        auto AccountId() const -> std::string;
        auto AccountId(std::string const& value) -> void;
        /**
         * Authentication token for Azure Active Directory (AAD).
         *
         * If the access token and the account key are missing, the session will obtain an access token based on this value.
         */
        auto AuthenticationToken() const -> std::string;
        auto AuthenticationToken(std::string const& value) -> void;
        /**
         * Account-level key for the Azure Spatial Anchors service.
         */
        auto AccountKey() const -> std::string;
        auto AccountKey(std::string const& value) -> void;
        /**
         * Access token for the Azure Spatial Anchors service.
         */
        auto AccessToken() const -> std::string;
        auto AccessToken(std::string const& value) -> void;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Provides data for the event that fires when errors are thrown.
     */
    class SessionErrorEventArgs final : public std::enable_shared_from_this<SessionErrorEventArgs>
    {
    public:
        SessionErrorEventArgs(void* handle, bool noCopy = true);
        ~SessionErrorEventArgs();

        /**
         * The error code.
         */
        auto ErrorCode() const -> Microsoft::Azure::SpatialAnchors::CloudSpatialErrorCode;
        /**
         * The error message.
         */
        auto ErrorMessage() const -> std::string;
        /**
         * The watcher that found an error, possibly null.
         */
        auto Watcher() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * This type describes the status of spatial data processing.
     */
    class SessionStatus final : public std::enable_shared_from_this<SessionStatus>
    {
    public:
        SessionStatus(void* handle, bool noCopy = true);
        ~SessionStatus();

        /**
         * The level of data sufficiency for a successful operation.
         *
         * This value will be in the [0;1) range when data is insufficient; 1 when data is sufficient for success and greater than 1 when conditions are better than minimally sufficient.
         */
        auto ReadyForCreateProgress() const -> float;
        /**
         * The ratio of data available to recommended data to create an anchor.
         *
         * This value will be in the [0;1) range when data is below the recommended threshold; 1 and greater when the recommended amount of data has been gathered for a creation operation.
         */
        auto RecommendedForCreateProgress() const -> float;
        /**
         * A hash value that can be used to know when environment data that contributes to a creation operation has changed to included the latest input data.
         *
         * If the hash value does not change after new frames were added to the session, then those frames were not deemed as sufficientlyy different from existing environment data and disgarded. This value may be 0 (and should be ignored) for platforms that don't feed frames individually.
         */
        auto SessionCreateHash() const -> int32_t;
        /**
         * A hash value that can be used to know when environment data that contributes to a locate operation has changed to included the latest input data.
         *
         * If the hash value does not change after new frames were added to the session, then those frames were not deemed as sufficiency different from existing environment data and disgarded. This value may be 0 (and should be ignored) for platforms that don't feed frames individually.
         */
        auto SessionLocateHash() const -> int32_t;
        /**
         * Feedback that can be provided to user about data processing status.
         */
        auto UserFeedback() const -> Microsoft::Azure::SpatialAnchors::SessionUserFeedback;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Provides data for the event that fires when the session state is updated.
     */
    class SessionUpdatedEventArgs final : public std::enable_shared_from_this<SessionUpdatedEventArgs>
    {
    public:
        SessionUpdatedEventArgs(void* handle, bool noCopy = true);
        ~SessionUpdatedEventArgs();

        /**
         * Current session status.
         */
        auto Status() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionStatus>;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    /**
     * Informs the application that the service requires an updated access token or authentication token.
     */
    class TokenRequiredEventArgs final : public std::enable_shared_from_this<TokenRequiredEventArgs>
    {
    public:
        TokenRequiredEventArgs(void* handle, bool noCopy = true);
        ~TokenRequiredEventArgs();

        /**
         * The access token to be used by the operation that requires it.
         */
        auto AccessToken() const -> std::string;
        auto AccessToken(std::string const& value) -> void;
        /**
         * The authentication token to be used by the operation that requires it.
         */
        auto AuthenticationToken() const -> std::string;
        auto AuthenticationToken(std::string const& value) -> void;
        /**
         * Returns a deferral object that can be used to provide an updated access token or authentication token from another asynchronous operation.
         */
        auto GetDeferral() -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSessionDeferral>;

        auto Handle() const -> void*;

    private:
        void* m_handle;
    };

    struct runtime_error
    {
        runtime_error() noexcept(noexcept(std::string())) :
            m_status(Status::Failed)
        {
        }

        runtime_error(runtime_error&&) = default;
        auto operator=(runtime_error&&) -> runtime_error& = default;

        runtime_error(runtime_error const& other) noexcept :
            m_status(other.m_status),
            m_message(other.m_message),
            m_requestCorrelationVector(other.m_requestCorrelationVector),
            m_responseCorrelationVector(other.m_responseCorrelationVector)
        {
        }

        auto operator=(runtime_error const& other) noexcept -> runtime_error&
        {
            m_status = other.m_status;
            m_message = other.m_message;
            m_requestCorrelationVector = other.m_requestCorrelationVector;
            m_responseCorrelationVector = other.m_responseCorrelationVector;
            return *this;
        }

        runtime_error(Status const status, std::string const& message, std::string const& requestCorrelationVector, std::string const& responseCorrelationVector) noexcept :
            m_status(status),
            m_message(std::move(message)),
            m_requestCorrelationVector(std::move(requestCorrelationVector)),
            m_responseCorrelationVector(std::move(responseCorrelationVector))
        {
        }

        runtime_error(Status const status, std::string const& message) noexcept
            : m_status(status),
            m_message(std::move(message))
        {
        }

        runtime_error(Status const status) noexcept(noexcept(std::string())) :
            m_status(status)
        {
        }

        auto status() const noexcept -> Status
        {
            return m_status;
        }

        auto message() const noexcept -> const std::string&
        {
            return m_message;
        }

        auto requestCorrelationVector() const noexcept -> const std::string&
        {
            return m_requestCorrelationVector;
        }

        auto responseCorrelationVector() const noexcept -> const std::string&
        {
            return m_responseCorrelationVector;
        }

    private:
        Status m_status;
        std::string m_message;
        std::string m_requestCorrelationVector;
        std::string m_responseCorrelationVector;
    };
} } }

#ifdef APIGEN_INLINE_IMPLEMENTATION
#include "AzureSpatialAnchorsNDK.hpp"
#endif
