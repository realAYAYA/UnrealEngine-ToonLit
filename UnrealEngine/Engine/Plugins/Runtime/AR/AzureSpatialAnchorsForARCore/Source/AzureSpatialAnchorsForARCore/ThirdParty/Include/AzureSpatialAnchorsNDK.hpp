//
// AzureSpatialAnchors
// This file was auto-generated from SscApiModelDirect.cs.
//

#include <thread>
#include <vector>
#include <sstream>
#include <algorithm>
#include <atomic>
#include <condition_variable>

#ifndef APIGEN_INLINE_IMPLEMENTATION
#include "AzureSpatialAnchorsNDK.h"
#endif

#ifdef APIGEN_INLINE_IMPLEMENTATION
#define APIGEN_LINKAGE inline
#else
#define APIGEN_LINKAGE
#endif

namespace Microsoft { namespace ApiGen
{
    inline auto check_status(void* handle, ssc_status value) -> void
    {
        if (value == ssc_status_ok)
        {
            return;
        }

        const char * outParam_message = nullptr;
        const char * outParam_requestCorrelationVector = nullptr;
        const char * outParam_responseCorrelationVector = nullptr;

        ssc_status status = ssc_get_error_details(handle, &outParam_message, &outParam_requestCorrelationVector, &outParam_responseCorrelationVector);

        std::string fullMessage;
        if (status == ssc_status_failed)
        {
            throw Microsoft::Azure::SpatialAnchors::runtime_error(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status), "Unexpected error in exception handling.");
        }
        else if (status != ssc_status_ok)
        {
            fullMessage = "Exception thrown and an unexpected error in exception handling.";
        }
        else
        {
            std::ostringstream fullMessageStr;
            fullMessageStr << "Message: " << outParam_message << ". Request CV: " << outParam_requestCorrelationVector << ". Response CV: " << outParam_responseCorrelationVector << ".";
            fullMessage = fullMessageStr.str();
        }

        switch (value)
        {
            case ssc_status_ok: return;
            case ssc_status_out_of_memory: throw std::bad_alloc();
            case ssc_status_out_of_range: throw std::out_of_range(fullMessage);
            case ssc_status_invalid_argument: throw std::invalid_argument(fullMessage);
            default:
                throw Microsoft::Azure::SpatialAnchors::runtime_error(static_cast<Microsoft::Azure::SpatialAnchors::Status>(value), outParam_message ? outParam_message : std::string(), outParam_requestCorrelationVector ? outParam_requestCorrelationVector : std::string(), outParam_responseCorrelationVector ? outParam_responseCorrelationVector : std::string());
        }
    }

    struct IDictionary_String_String : Microsoft::Azure::SpatialAnchors::IMap<std::string, std::string>
    {
        IDictionary_String_String(ssc_idictionary_string_string_handle handle, bool noCopy = true) : m_handle(handle)
        {
            if (!noCopy)
            {
                check_status(m_handle, ssc_idictionary_string_string_addref(m_handle));
            }
        }

        ~IDictionary_String_String()
        {
            check_status(m_handle, ssc_idictionary_string_string_release(m_handle));
        }

        auto Lookup(std::string const& key) const -> std::string override
        {
            const char * outParam;
            auto resultCode = ssc_idictionary_string_string_get_item(m_handle, key.c_str(), &outParam);
            if (resultCode == ssc_status_key_not_found)
            {
                throw std::out_of_range("");
            }
            else
            {
                check_status(m_handle, resultCode);
            }
            std::string result = outParam; free(const_cast<char*>(outParam));

            return result;
        }

        auto Size() const -> uint32_t override
        {
            int size;
            check_status(m_handle, ssc_idictionary_string_string_get_count(m_handle, &size));

            return size;
        }

        auto HasKey(std::string const& key) const -> bool override
        {
            const char * outParam;
            auto resultCode = ssc_idictionary_string_string_get_item(m_handle, key.c_str(), &outParam);
            auto success = resultCode == ssc_status_ok;
            std::string result = outParam; free(const_cast<char*>(outParam));

            return success;
        }

        auto Insert(std::string const& key, std::string const& value) -> bool override
        {
            auto result = HasKey(key);
            check_status(m_handle, ssc_idictionary_string_string_set_item(m_handle, key.c_str(), value.c_str()));
            return result;
        }

        auto Remove(std::string const& key) -> void override
        {
            check_status(m_handle, ssc_idictionary_string_string_remove_key(m_handle, key.c_str()));
        }

        auto Clear() -> void override
        {
            check_status(m_handle, ssc_idictionary_string_string_clear(m_handle));
        }

        auto GetAt(uint32_t index) const -> Microsoft::Azure::SpatialAnchors::key_value_pair<std::string, std::string> override
        {
            if (index >= Size())
            {
                throw std::out_of_range("");
            }

            const char * outParam;
            check_status(m_handle, ssc_idictionary_string_string_get_key(m_handle, index, &outParam));
            std::string key = outParam; free(const_cast<char*>(outParam));
            auto value = Lookup(key);

            auto result = Microsoft::Azure::SpatialAnchors::key_value_pair<std::string, std::string>(key, value);

            return result;
        }

    private:
        ssc_idictionary_string_string_handle m_handle;
    };

    struct IList_String : Microsoft::Azure::SpatialAnchors::IVector<std::string>
    {
        IList_String(ssc_ilist_string_handle handle, bool noCopy = true) : m_handle(handle)
        {
            if (!noCopy)
            {
                check_status(m_handle, ssc_ilist_string_addref(m_handle));
            }
        }

        ~IList_String()
        {
            check_status(m_handle, ssc_ilist_string_release(m_handle));
        }

        auto GetAt(uint32_t const index) const -> std::string override
        {
            if (index >= Size())
            {
                throw std::out_of_range("");
            }

            const char * outParam;
            check_status(m_handle, ssc_ilist_string_get_item(m_handle, static_cast<int>(index), &outParam));
            std::string result = outParam; free(const_cast<char*>(outParam));
            return result;
        }

        auto Size() const -> uint32_t override
        {
            int size;
            check_status(m_handle, ssc_ilist_string_get_count(m_handle, &size));

            return size;
        }

        auto SetAt(uint32_t const index, std::string const& value) -> void override
        {
            if (index >= Size())
            {
                throw std::out_of_range("");
            }

            check_status(m_handle, ssc_ilist_string_set_item(m_handle, static_cast<int>(index), value.c_str()));
        }

        auto InsertAt(uint32_t const index, std::string const& value) -> void override
        {
            if (index >= Size())
            {
                throw std::out_of_range("");
            }

            check_status(m_handle, ssc_ilist_string_set_item(m_handle, static_cast<int>(index), value.c_str()));
        }

        auto RemoveAt(uint32_t const index) -> void override
        {
            if (index >= Size())
            {
                throw std::out_of_range("");
            }

            check_status(m_handle, ssc_ilist_string_remove_item(m_handle, static_cast<int>(index)));
        }

        auto Append(std::string const& value) -> void override
        {
            int size = static_cast<int>(Size());

            check_status(m_handle, ssc_ilist_string_set_item(m_handle, size, value.c_str()));
        }

        auto Clear() -> void override
        {
            int size = static_cast<int>(Size());

            for (int i = size - 1; i >= 0; i--)
            {
                check_status(m_handle, ssc_ilist_string_remove_item(m_handle, i));
            }
        }

    private:
        ssc_ilist_string_handle m_handle;
    };

    inline auto create_vector_CloudSpatialAnchorWatcher(ssc_cloud_spatial_anchor_watcher_handle* values, size_t len) -> std::vector<std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>>
    {
        ::std::vector<std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>> v;
        v.reserve(len);
        for (size_t i = 0; i < len; ++i)
        {
            v.push_back(values[i] ? std::make_shared<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>(values[i], /* transfer */ true) : nullptr);
        }
        return v;
    }

    struct thread_pool
    {
        auto submit_work(const std::function<void(void)>&& work) -> void
        {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_work.emplace_back(work);
            }
            m_condition.notify_one();
        }

        static auto get() -> thread_pool&
        {
            std::call_once(s_poolStarted, []() { s_threadPool.start(); });
            return s_threadPool;
        }

    private:
        thread_pool()
        {
        }

        auto start() -> void
        {
            auto threadCount = (std::min)(4U, std::thread::hardware_concurrency());
            for (size_t i = 0; i < threadCount; i++)
            {
                m_threads.emplace_back([this]()
                    {
                        while (!m_stopRequested)
                        {
                            std::vector<std::function<void(void)>> temp_work;
                            {
                                std::unique_lock<std::mutex> lock(m_mutex);
                                m_condition.wait(lock, [this] { return m_stopRequested || !m_work.empty(); });

                                if (m_stopRequested)
                                {
                                    break;
                                }

                                temp_work = std::move(m_work);
                            }

                            for (auto const& work : temp_work)
                            {
                                work();
                            }
                        }
                    });
            }
        }

        ~thread_pool()
        {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_stopRequested = true;
            }
            m_condition.notify_all();
            for (auto& thread : m_threads)
            {
                thread.join();
            }
            m_threads.clear();
        }

    private:
        std::vector<std::thread> m_threads;
        std::condition_variable m_condition;
        std::mutex m_mutex;
        std::vector<std::function<void(void)>> m_work;
        std::atomic_bool m_stopRequested{ false };
        static thread_pool s_threadPool;
        static std::once_flag s_poolStarted;
    };

    thread_pool thread_pool::s_threadPool;
    std::once_flag thread_pool::s_poolStarted;

    template <typename T>
    struct Metadata;

    template <typename TApiType>
    inline auto Convert(typename Metadata<TApiType>::HandleType* first, typename Metadata<TApiType>::HandleType* last) -> std::vector<std::shared_ptr<TApiType>>
    {
        std::vector<std::shared_ptr<TApiType>> result;
        result.reserve(last - first);

        for (auto current = first; current != last; ++current)
        {
            result.emplace_back(std::make_shared<TApiType>(current));
        }

        return result;
    }

    template <typename TApiType>
    inline auto Convert(typename Metadata<TApiType>::Type* first, typename Metadata<TApiType>::Type* last) -> std::vector<TApiType>
    {
        std::vector<TApiType> result;
        result.reserve(last - first);

        for (auto current = first; current != last; ++current)
        {
            result.emplace_back(Convert(*current));
        }

        return result;
    }

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::AnchorLocateCriteria>
    {
        using HandleType = ssc_anchor_locate_criteria_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::AnchorLocatedEventArgs>
    {
        using HandleType = ssc_anchor_located_event_args_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor>
    {
        using HandleType = ssc_cloud_spatial_anchor_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorMap>
    {
        using HandleType = ssc_cloud_spatial_anchor_map_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSessionDeferral>
    {
        using HandleType = ssc_cloud_spatial_anchor_session_deferral_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSessionDiagnostics>
    {
        using HandleType = ssc_cloud_spatial_anchor_session_diagnostics_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSession>
    {
        using HandleType = ssc_cloud_spatial_anchor_session_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>
    {
        using HandleType = ssc_cloud_spatial_anchor_watcher_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::GeoLocation>
    {
        using HandleType = ssc_geo_location_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::LocateAnchorsCompletedEventArgs>
    {
        using HandleType = ssc_locate_anchors_completed_event_args_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::NearAnchorCriteria>
    {
        using HandleType = ssc_near_anchor_criteria_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::NearDeviceCriteria>
    {
        using HandleType = ssc_near_device_criteria_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::OnLogDebugEventArgs>
    {
        using HandleType = ssc_on_log_debug_event_args_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::PlatformLocationProvider>
    {
        using HandleType = ssc_platform_location_provider_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::SensorCapabilities>
    {
        using HandleType = ssc_sensor_capabilities_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::SensorFingerprintEventArgs>
    {
        using HandleType = ssc_sensor_fingerprint_event_args_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::SessionConfiguration>
    {
        using HandleType = ssc_session_configuration_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::SessionErrorEventArgs>
    {
        using HandleType = ssc_session_error_event_args_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::SessionStatus>
    {
        using HandleType = ssc_session_status_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::SessionUpdatedEventArgs>
    {
        using HandleType = ssc_session_updated_event_args_handle;
    };

    template<>
    struct Metadata<Microsoft::Azure::SpatialAnchors::TokenRequiredEventArgs>
    {
        using HandleType = ssc_token_required_event_args_handle;
    };

} }

namespace Microsoft { namespace Azure { namespace SpatialAnchors
{
    APIGEN_LINKAGE AnchorLocateCriteria::AnchorLocateCriteria(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_anchor_locate_criteria_addref(static_cast<ssc_anchor_locate_criteria_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE AnchorLocateCriteria::~AnchorLocateCriteria()
    {
        auto status = ssc_anchor_locate_criteria_release(static_cast<ssc_anchor_locate_criteria_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE AnchorLocateCriteria::AnchorLocateCriteria()
    {
        auto status = ssc_anchor_locate_criteria_create(reinterpret_cast<ssc_anchor_locate_criteria_handle*>(&m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::BypassCache() const -> bool
    {
        ssc_bool outParam_result;
        auto status = ssc_anchor_locate_criteria_get_bypass_cache(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        bool result = static_cast<bool>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::BypassCache(bool value) -> void
    {
        auto status = ssc_anchor_locate_criteria_set_bypass_cache(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), static_cast<bool>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::RequestedCategories() const -> Microsoft::Azure::SpatialAnchors::AnchorDataCategory
    {
        ssc_anchor_data_category outParam_result;
        auto status = ssc_anchor_locate_criteria_get_requested_categories(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = static_cast<Microsoft::Azure::SpatialAnchors::AnchorDataCategory>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::RequestedCategories(Microsoft::Azure::SpatialAnchors::AnchorDataCategory const& value) -> void
    {
        auto status = ssc_anchor_locate_criteria_set_requested_categories(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), static_cast<ssc_anchor_data_category>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::Strategy() const -> Microsoft::Azure::SpatialAnchors::LocateStrategy
    {
        ssc_locate_strategy outParam_result;
        auto status = ssc_anchor_locate_criteria_get_strategy(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = static_cast<Microsoft::Azure::SpatialAnchors::LocateStrategy>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::Strategy(Microsoft::Azure::SpatialAnchors::LocateStrategy const& value) -> void
    {
        auto status = ssc_anchor_locate_criteria_set_strategy(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), static_cast<ssc_locate_strategy>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::Identifiers() const -> std::vector<std::string>
    {
        const char * * outParam_result;
        int outParam_result_count = 0;
        auto status = ssc_anchor_locate_criteria_get_identifiers(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), &outParam_result, &outParam_result_count);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::vector<std::string> result_localArray_tmp;
        for (int i = 0; i < outParam_result_count; i++)
        {
            std::string itrValue = outParam_result[i]; free(const_cast<char*>(outParam_result[i]));
            result_localArray_tmp.emplace_back(itrValue);
        }
        free(const_cast<const char * *>(outParam_result));
        auto result = std::move(result_localArray_tmp);
        return result;
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::Identifiers(std::vector<std::string> const& value) -> void
    {
        std::vector<const char *> value_localArray;
        for (const auto& itrValue : value)
        {
            value_localArray.emplace_back(itrValue.c_str());
        }
        auto status = ssc_anchor_locate_criteria_set_identifiers(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), value_localArray.data(), static_cast<int>(value_localArray.size()));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::NearAnchor() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearAnchorCriteria>
    {
        ssc_near_anchor_criteria_handle outParam_result;
        auto status = ssc_anchor_locate_criteria_get_near_anchor(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearAnchorCriteria> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::NearAnchorCriteria>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::NearAnchor(std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearAnchorCriteria> const& value) -> void
    {
        auto status = ssc_anchor_locate_criteria_set_near_anchor(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), value ? static_cast<ssc_near_anchor_criteria_handle>(value->Handle()) : nullptr);
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::NearDevice() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearDeviceCriteria>
    {
        ssc_near_device_criteria_handle outParam_result;
        auto status = ssc_anchor_locate_criteria_get_near_device(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearDeviceCriteria> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::NearDeviceCriteria>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE auto AnchorLocateCriteria::NearDevice(std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearDeviceCriteria> const& value) -> void
    {
        auto status = ssc_anchor_locate_criteria_set_near_device(static_cast<ssc_anchor_locate_criteria_handle>(m_handle), value ? static_cast<ssc_near_device_criteria_handle>(value->Handle()) : nullptr);
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE AnchorLocatedEventArgs::AnchorLocatedEventArgs(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_anchor_located_event_args_addref(static_cast<ssc_anchor_located_event_args_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE AnchorLocatedEventArgs::~AnchorLocatedEventArgs()
    {
        auto status = ssc_anchor_located_event_args_release(static_cast<ssc_anchor_located_event_args_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto AnchorLocatedEventArgs::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto AnchorLocatedEventArgs::Anchor() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor>
    {
        ssc_cloud_spatial_anchor_handle outParam_result;
        auto status = ssc_anchor_located_event_args_get_anchor(static_cast<ssc_anchor_located_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE auto AnchorLocatedEventArgs::Identifier() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_anchor_located_event_args_get_identifier(static_cast<ssc_anchor_located_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto AnchorLocatedEventArgs::Status() const -> Microsoft::Azure::SpatialAnchors::LocateAnchorStatus
    {
        ssc_locate_anchor_status outParam_result;
        auto status = ssc_anchor_located_event_args_get_status(static_cast<ssc_anchor_located_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = static_cast<Microsoft::Azure::SpatialAnchors::LocateAnchorStatus>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto AnchorLocatedEventArgs::Strategy() const -> Microsoft::Azure::SpatialAnchors::LocateStrategy
    {
        ssc_locate_strategy outParam_result;
        auto status = ssc_anchor_located_event_args_get_strategy(static_cast<ssc_anchor_located_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = static_cast<Microsoft::Azure::SpatialAnchors::LocateStrategy>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto AnchorLocatedEventArgs::Watcher() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>
    {
        ssc_cloud_spatial_anchor_watcher_handle outParam_result;
        auto status = ssc_anchor_located_event_args_get_watcher(static_cast<ssc_anchor_located_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE CloudSpatialAnchor::CloudSpatialAnchor(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_cloud_spatial_anchor_addref(static_cast<ssc_cloud_spatial_anchor_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE CloudSpatialAnchor::~CloudSpatialAnchor()
    {
        auto status = ssc_cloud_spatial_anchor_release(static_cast<ssc_cloud_spatial_anchor_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchor::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE CloudSpatialAnchor::CloudSpatialAnchor()
    {
        auto status = ssc_cloud_spatial_anchor_create(reinterpret_cast<ssc_cloud_spatial_anchor_handle*>(&m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchor::LocalAnchor() const -> ArAnchor*
    {
        ssc_platform_anchor_handle outParam_result;
        auto status = ssc_cloud_spatial_anchor_get_local_anchor(static_cast<ssc_cloud_spatial_anchor_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        ArAnchor* result = reinterpret_cast<ArAnchor*>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchor::LocalAnchor(ArAnchor* const& value) -> void
    {
        auto status = ssc_cloud_spatial_anchor_set_local_anchor(static_cast<ssc_cloud_spatial_anchor_handle>(m_handle), value);
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchor::Expiration() const -> int64_t
    {
        int64_t outParam_result;
        auto status = ssc_cloud_spatial_anchor_get_expiration(static_cast<ssc_cloud_spatial_anchor_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        int64_t result = outParam_result;
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchor::Expiration(int64_t value) -> void
    {
        auto status = ssc_cloud_spatial_anchor_set_expiration(static_cast<ssc_cloud_spatial_anchor_handle>(m_handle), value);
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchor::Identifier() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_cloud_spatial_anchor_get_identifier(static_cast<ssc_cloud_spatial_anchor_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchor::AppProperties() const -> std::shared_ptr<IMap<std::string, std::string>>
    {
        ssc_idictionary_string_string_handle outParam_result;
        auto status = ssc_cloud_spatial_anchor_get_app_properties(static_cast<ssc_cloud_spatial_anchor_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<IMap<std::string, std::string>> result = std::make_shared<Microsoft::ApiGen::IDictionary_String_String>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchor::VersionTag() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_cloud_spatial_anchor_get_version_tag(static_cast<ssc_cloud_spatial_anchor_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE CloudSpatialAnchorMap::CloudSpatialAnchorMap(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_cloud_spatial_anchor_map_addref(static_cast<ssc_cloud_spatial_anchor_map_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE CloudSpatialAnchorMap::~CloudSpatialAnchorMap()
    {
        auto status = ssc_cloud_spatial_anchor_map_release(static_cast<ssc_cloud_spatial_anchor_map_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorMap::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorMap::Identifier() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_cloud_spatial_anchor_map_get_identifier(static_cast<ssc_cloud_spatial_anchor_map_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorMap::Name() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_cloud_spatial_anchor_map_get_name(static_cast<ssc_cloud_spatial_anchor_map_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE CloudSpatialAnchorSessionDeferral::CloudSpatialAnchorSessionDeferral(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_cloud_spatial_anchor_session_deferral_addref(static_cast<ssc_cloud_spatial_anchor_session_deferral_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE CloudSpatialAnchorSessionDeferral::~CloudSpatialAnchorSessionDeferral()
    {
        auto status = ssc_cloud_spatial_anchor_session_deferral_release(static_cast<ssc_cloud_spatial_anchor_session_deferral_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDeferral::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDeferral::Complete() -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_deferral_complete(static_cast<ssc_cloud_spatial_anchor_session_deferral_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE CloudSpatialAnchorSessionDiagnostics::CloudSpatialAnchorSessionDiagnostics(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_cloud_spatial_anchor_session_diagnostics_addref(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE CloudSpatialAnchorSessionDiagnostics::~CloudSpatialAnchorSessionDiagnostics()
    {
        auto status = ssc_cloud_spatial_anchor_session_diagnostics_release(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDiagnostics::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDiagnostics::LogLevel() const -> Microsoft::Azure::SpatialAnchors::SessionLogLevel
    {
        ssc_session_log_level outParam_result;
        auto status = ssc_cloud_spatial_anchor_session_diagnostics_get_log_level(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = static_cast<Microsoft::Azure::SpatialAnchors::SessionLogLevel>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDiagnostics::LogLevel(Microsoft::Azure::SpatialAnchors::SessionLogLevel const& value) -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_diagnostics_set_log_level(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(m_handle), static_cast<ssc_session_log_level>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDiagnostics::LogDirectory() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_cloud_spatial_anchor_session_diagnostics_get_log_directory(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDiagnostics::LogDirectory(std::string const& value) -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_diagnostics_set_log_directory(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(m_handle), value.c_str());
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDiagnostics::MaxDiskSizeInMB() const -> int32_t
    {
        int32_t result;
        auto status = ssc_cloud_spatial_anchor_session_diagnostics_get_max_disk_size_in_mb(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(m_handle), reinterpret_cast<int*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDiagnostics::MaxDiskSizeInMB(int32_t value) -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_diagnostics_set_max_disk_size_in_mb(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(m_handle), static_cast<int32_t>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDiagnostics::ImagesEnabled() const -> bool
    {
        ssc_bool outParam_result;
        auto status = ssc_cloud_spatial_anchor_session_diagnostics_get_images_enabled(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        bool result = static_cast<bool>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDiagnostics::ImagesEnabled(bool value) -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_diagnostics_set_images_enabled(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(m_handle), static_cast<bool>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDiagnostics::CreateManifestAsync(std::string const& param_description, std::function<void(Microsoft::Azure::SpatialAnchors::Status, const std::string &)> callback) -> void
    {
        auto self = shared_from_this();
        std::string description = param_description;
        Microsoft::ApiGen::thread_pool::get().submit_work([self, callback, description]
        {
            const char * outParam_result;
            auto status = ssc_cloud_spatial_anchor_session_diagnostics_create_manifest_async(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(self->m_handle), description.c_str(), &outParam_result);
            std::string result = outParam_result; free(const_cast<char*>(outParam_result));
            callback(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status), result);
        });
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSessionDiagnostics::SubmitManifestAsync(std::string const& param_manifestPath, std::function<void(Microsoft::Azure::SpatialAnchors::Status)> callback) -> void
    {
        auto self = shared_from_this();
        std::string manifestPath = param_manifestPath;
        Microsoft::ApiGen::thread_pool::get().submit_work([self, callback, manifestPath]
        {
            auto status = ssc_cloud_spatial_anchor_session_diagnostics_submit_manifest_async(static_cast<ssc_cloud_spatial_anchor_session_diagnostics_handle>(self->m_handle), manifestPath.c_str());
            callback(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status));
        });
    }

    APIGEN_LINKAGE CloudSpatialAnchorSession::CloudSpatialAnchorSession(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_cloud_spatial_anchor_session_addref(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE CloudSpatialAnchorSession::~CloudSpatialAnchorSession()
    {
        auto status = ssc_cloud_spatial_anchor_session_release(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
        s_cookieTracker.remove(this);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE CloudSpatialAnchorSession::CloudSpatialAnchorSession()
    {
        auto status = ssc_cloud_spatial_anchor_session_create(reinterpret_cast<ssc_cloud_spatial_anchor_session_handle*>(&m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::Configuration() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionConfiguration>
    {
        ssc_session_configuration_handle outParam_result;
        auto status = ssc_cloud_spatial_anchor_session_get_configuration(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionConfiguration> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::SessionConfiguration>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::Diagnostics() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSessionDiagnostics>
    {
        ssc_cloud_spatial_anchor_session_diagnostics_handle outParam_result;
        auto status = ssc_cloud_spatial_anchor_session_get_diagnostics(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSessionDiagnostics> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSessionDiagnostics>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::LogLevel() const -> Microsoft::Azure::SpatialAnchors::SessionLogLevel
    {
        ssc_session_log_level outParam_result;
        auto status = ssc_cloud_spatial_anchor_session_get_log_level(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = static_cast<Microsoft::Azure::SpatialAnchors::SessionLogLevel>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::LogLevel(Microsoft::Azure::SpatialAnchors::SessionLogLevel const& value) -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_set_log_level(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), static_cast<ssc_session_log_level>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::Session() const -> ArSession*
    {
        ssc_platform_session_handle outParam_result;
        auto status = ssc_cloud_spatial_anchor_session_get_session(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        ArSession* result = reinterpret_cast<ArSession*>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::Session(ArSession* const& value) -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_set_session(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), value);
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::LocationProvider() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::PlatformLocationProvider>
    {
        ssc_platform_location_provider_handle outParam_result;
        auto status = ssc_cloud_spatial_anchor_session_get_location_provider(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::PlatformLocationProvider> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::PlatformLocationProvider>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::LocationProvider(std::shared_ptr<Microsoft::Azure::SpatialAnchors::PlatformLocationProvider> const& value) -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_set_location_provider(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), value ? static_cast<ssc_platform_location_provider_handle>(value->Handle()) : nullptr);
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::SessionId() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_cloud_spatial_anchor_session_get_session_id(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::TokenRequired(TokenRequiredDelegate const& handler) -> Microsoft::Azure::SpatialAnchors::event_token
    {
        auto token = m_tokenRequiredEvent.add(handler);

        auto cookie = s_cookieTracker.getCookie(shared_from_this());
        auto status = ssc_cloud_spatial_anchor_session_set_token_required(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), cookie, [](ssc_callback_cookie cookie, ssc_token_required_event_args_handle args)
        {
            auto self = s_cookieTracker.lookup(cookie);
            if (self != nullptr)
            {
                std::shared_ptr<Microsoft::Azure::SpatialAnchors::TokenRequiredEventArgs> args_eventArg = args ? std::make_shared<Microsoft::Azure::SpatialAnchors::TokenRequiredEventArgs>(args, /* transfer */ false) : nullptr;
                self->m_tokenRequiredEvent(self.get(), args_eventArg);
            }
        });
        Microsoft::ApiGen::check_status(m_handle, status);
        return token;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::TokenRequired(Microsoft::Azure::SpatialAnchors::event_token& token) -> void
    {
        m_tokenRequiredEvent.remove(token);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::AnchorLocated(AnchorLocatedDelegate const& handler) -> Microsoft::Azure::SpatialAnchors::event_token
    {
        auto token = m_anchorLocatedEvent.add(handler);

        auto cookie = s_cookieTracker.getCookie(shared_from_this());
        auto status = ssc_cloud_spatial_anchor_session_set_anchor_located(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), cookie, [](ssc_callback_cookie cookie, ssc_anchor_located_event_args_handle args)
        {
            auto self = s_cookieTracker.lookup(cookie);
            if (self != nullptr)
            {
                std::shared_ptr<Microsoft::Azure::SpatialAnchors::AnchorLocatedEventArgs> args_eventArg = args ? std::make_shared<Microsoft::Azure::SpatialAnchors::AnchorLocatedEventArgs>(args, /* transfer */ false) : nullptr;
                self->m_anchorLocatedEvent(self.get(), args_eventArg);
            }
        });
        Microsoft::ApiGen::check_status(m_handle, status);
        return token;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::AnchorLocated(Microsoft::Azure::SpatialAnchors::event_token& token) -> void
    {
        m_anchorLocatedEvent.remove(token);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::LocateAnchorsCompleted(LocateAnchorsCompletedDelegate const& handler) -> Microsoft::Azure::SpatialAnchors::event_token
    {
        auto token = m_locateAnchorsCompletedEvent.add(handler);

        auto cookie = s_cookieTracker.getCookie(shared_from_this());
        auto status = ssc_cloud_spatial_anchor_session_set_locate_anchors_completed(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), cookie, [](ssc_callback_cookie cookie, ssc_locate_anchors_completed_event_args_handle args)
        {
            auto self = s_cookieTracker.lookup(cookie);
            if (self != nullptr)
            {
                std::shared_ptr<Microsoft::Azure::SpatialAnchors::LocateAnchorsCompletedEventArgs> args_eventArg = args ? std::make_shared<Microsoft::Azure::SpatialAnchors::LocateAnchorsCompletedEventArgs>(args, /* transfer */ false) : nullptr;
                self->m_locateAnchorsCompletedEvent(self.get(), args_eventArg);
            }
        });
        Microsoft::ApiGen::check_status(m_handle, status);
        return token;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::LocateAnchorsCompleted(Microsoft::Azure::SpatialAnchors::event_token& token) -> void
    {
        m_locateAnchorsCompletedEvent.remove(token);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::SessionUpdated(SessionUpdatedDelegate const& handler) -> Microsoft::Azure::SpatialAnchors::event_token
    {
        auto token = m_sessionUpdatedEvent.add(handler);

        auto cookie = s_cookieTracker.getCookie(shared_from_this());
        auto status = ssc_cloud_spatial_anchor_session_set_session_updated(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), cookie, [](ssc_callback_cookie cookie, ssc_session_updated_event_args_handle args)
        {
            auto self = s_cookieTracker.lookup(cookie);
            if (self != nullptr)
            {
                std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionUpdatedEventArgs> args_eventArg = args ? std::make_shared<Microsoft::Azure::SpatialAnchors::SessionUpdatedEventArgs>(args, /* transfer */ false) : nullptr;
                self->m_sessionUpdatedEvent(self.get(), args_eventArg);
            }
        });
        Microsoft::ApiGen::check_status(m_handle, status);
        return token;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::SessionUpdated(Microsoft::Azure::SpatialAnchors::event_token& token) -> void
    {
        m_sessionUpdatedEvent.remove(token);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::Error(SessionErrorDelegate const& handler) -> Microsoft::Azure::SpatialAnchors::event_token
    {
        auto token = m_errorEvent.add(handler);

        auto cookie = s_cookieTracker.getCookie(shared_from_this());
        auto status = ssc_cloud_spatial_anchor_session_set_error(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), cookie, [](ssc_callback_cookie cookie, ssc_session_error_event_args_handle args)
        {
            auto self = s_cookieTracker.lookup(cookie);
            if (self != nullptr)
            {
                std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionErrorEventArgs> args_eventArg = args ? std::make_shared<Microsoft::Azure::SpatialAnchors::SessionErrorEventArgs>(args, /* transfer */ false) : nullptr;
                self->m_errorEvent(self.get(), args_eventArg);
            }
        });
        Microsoft::ApiGen::check_status(m_handle, status);
        return token;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::Error(Microsoft::Azure::SpatialAnchors::event_token& token) -> void
    {
        m_errorEvent.remove(token);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::OnLogDebug(OnLogDebugDelegate const& handler) -> Microsoft::Azure::SpatialAnchors::event_token
    {
        auto token = m_onLogDebugEvent.add(handler);

        auto cookie = s_cookieTracker.getCookie(shared_from_this());
        auto status = ssc_cloud_spatial_anchor_session_set_on_log_debug(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), cookie, [](ssc_callback_cookie cookie, ssc_on_log_debug_event_args_handle args)
        {
            auto self = s_cookieTracker.lookup(cookie);
            if (self != nullptr)
            {
                std::shared_ptr<Microsoft::Azure::SpatialAnchors::OnLogDebugEventArgs> args_eventArg = args ? std::make_shared<Microsoft::Azure::SpatialAnchors::OnLogDebugEventArgs>(args, /* transfer */ false) : nullptr;
                self->m_onLogDebugEvent(self.get(), args_eventArg);
            }
        });
        Microsoft::ApiGen::check_status(m_handle, status);
        return token;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::OnLogDebug(Microsoft::Azure::SpatialAnchors::event_token& token) -> void
    {
        m_onLogDebugEvent.remove(token);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::UpdatedSensorFingerprintRequired(UpdatedSensorFingerprintRequiredDelegate const& handler) -> Microsoft::Azure::SpatialAnchors::event_token
    {
        auto token = m_updatedSensorFingerprintRequiredEvent.add(handler);

        auto cookie = s_cookieTracker.getCookie(shared_from_this());
        auto status = ssc_cloud_spatial_anchor_session_set_updated_sensor_fingerprint_required(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), cookie, [](ssc_callback_cookie cookie, ssc_sensor_fingerprint_event_args_handle args)
        {
            auto self = s_cookieTracker.lookup(cookie);
            if (self != nullptr)
            {
                std::shared_ptr<Microsoft::Azure::SpatialAnchors::SensorFingerprintEventArgs> args_eventArg = args ? std::make_shared<Microsoft::Azure::SpatialAnchors::SensorFingerprintEventArgs>(args, /* transfer */ false) : nullptr;
                self->m_updatedSensorFingerprintRequiredEvent(self.get(), args_eventArg);
            }
        });
        Microsoft::ApiGen::check_status(m_handle, status);
        return token;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::UpdatedSensorFingerprintRequired(Microsoft::Azure::SpatialAnchors::event_token& token) -> void
    {
        m_updatedSensorFingerprintRequiredEvent.remove(token);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::Dispose() -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_dispose(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::GetAccessTokenWithAuthenticationTokenAsync(std::string const& param_authenticationToken, std::function<void(Microsoft::Azure::SpatialAnchors::Status, const std::string &)> callback) -> void
    {
        auto self = shared_from_this();
        std::string authenticationToken = param_authenticationToken;
        Microsoft::ApiGen::thread_pool::get().submit_work([self, callback, authenticationToken]
        {
            const char * outParam_result;
            auto status = ssc_cloud_spatial_anchor_session_get_access_token_with_authentication_token_async(static_cast<ssc_cloud_spatial_anchor_session_handle>(self->m_handle), authenticationToken.c_str(), &outParam_result);
            std::string result = outParam_result; free(const_cast<char*>(outParam_result));
            callback(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status), result);
        });
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::GetAccessTokenWithAccountKeyAsync(std::string const& param_accountKey, std::function<void(Microsoft::Azure::SpatialAnchors::Status, const std::string &)> callback) -> void
    {
        auto self = shared_from_this();
        std::string accountKey = param_accountKey;
        Microsoft::ApiGen::thread_pool::get().submit_work([self, callback, accountKey]
        {
            const char * outParam_result;
            auto status = ssc_cloud_spatial_anchor_session_get_access_token_with_account_key_async(static_cast<ssc_cloud_spatial_anchor_session_handle>(self->m_handle), accountKey.c_str(), &outParam_result);
            std::string result = outParam_result; free(const_cast<char*>(outParam_result));
            callback(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status), result);
        });
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::CreateAnchorAsync(std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> const& param_anchor, std::function<void(Microsoft::Azure::SpatialAnchors::Status)> callback) -> void
    {
        auto self = shared_from_this();
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> anchor = param_anchor;
        Microsoft::ApiGen::thread_pool::get().submit_work([self, callback, anchor]
        {
            auto status = ssc_cloud_spatial_anchor_session_create_anchor_async(static_cast<ssc_cloud_spatial_anchor_session_handle>(self->m_handle), anchor ? static_cast<ssc_cloud_spatial_anchor_handle>(anchor->Handle()) : nullptr);
            callback(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status));
        });
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::CreateWatcher(std::shared_ptr<Microsoft::Azure::SpatialAnchors::AnchorLocateCriteria> const& criteria) -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>
    {
        ssc_cloud_spatial_anchor_watcher_handle outParam_result;
        auto status = ssc_cloud_spatial_anchor_session_create_watcher(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), criteria ? static_cast<ssc_anchor_locate_criteria_handle>(criteria->Handle()) : nullptr, &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::GetAnchorPropertiesAsync(std::string const& param_identifier, std::function<void(Microsoft::Azure::SpatialAnchors::Status, const std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> &)> callback) -> void
    {
        auto self = shared_from_this();
        std::string identifier = param_identifier;
        Microsoft::ApiGen::thread_pool::get().submit_work([self, callback, identifier]
        {
            ssc_cloud_spatial_anchor_handle outParam_result;
            auto status = ssc_cloud_spatial_anchor_session_get_anchor_properties_async(static_cast<ssc_cloud_spatial_anchor_session_handle>(self->m_handle), identifier.c_str(), &outParam_result);
            std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor>(outParam_result, /* transfer */ true) : nullptr;
            callback(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status), result);
        });
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::GetNearbyAnchorIdsAsync(std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearDeviceCriteria> const& param_criteria, std::function<void(Microsoft::Azure::SpatialAnchors::Status, std::shared_ptr<IVector<std::string>>)> callback) -> void
    {
        auto self = shared_from_this();
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::NearDeviceCriteria> criteria = param_criteria;
        Microsoft::ApiGen::thread_pool::get().submit_work([self, callback, criteria]
        {
            ssc_ilist_string_handle outParam_result;
            auto status = ssc_cloud_spatial_anchor_session_get_nearby_anchor_ids_async(static_cast<ssc_cloud_spatial_anchor_session_handle>(self->m_handle), criteria ? static_cast<ssc_near_device_criteria_handle>(criteria->Handle()) : nullptr, &outParam_result);
            std::shared_ptr<IVector<std::string>> result = std::make_shared<Microsoft::ApiGen::IList_String>(outParam_result);
            callback(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status), result);
        });
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::GetActiveWatchers() -> std::vector<std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>>
    {
        ssc_cloud_spatial_anchor_watcher_handle * outParam_result;
        int outParam_result_count = 0;
        auto status = ssc_cloud_spatial_anchor_session_get_active_watchers(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), &outParam_result, &outParam_result_count);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = Microsoft::ApiGen::create_vector_CloudSpatialAnchorWatcher(outParam_result, outParam_result_count); free(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::RefreshAnchorPropertiesAsync(std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> const& param_anchor, std::function<void(Microsoft::Azure::SpatialAnchors::Status)> callback) -> void
    {
        auto self = shared_from_this();
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> anchor = param_anchor;
        Microsoft::ApiGen::thread_pool::get().submit_work([self, callback, anchor]
        {
            auto status = ssc_cloud_spatial_anchor_session_refresh_anchor_properties_async(static_cast<ssc_cloud_spatial_anchor_session_handle>(self->m_handle), anchor ? static_cast<ssc_cloud_spatial_anchor_handle>(anchor->Handle()) : nullptr);
            callback(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status));
        });
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::UpdateAnchorPropertiesAsync(std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> const& param_anchor, std::function<void(Microsoft::Azure::SpatialAnchors::Status)> callback) -> void
    {
        auto self = shared_from_this();
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> anchor = param_anchor;
        Microsoft::ApiGen::thread_pool::get().submit_work([self, callback, anchor]
        {
            auto status = ssc_cloud_spatial_anchor_session_update_anchor_properties_async(static_cast<ssc_cloud_spatial_anchor_session_handle>(self->m_handle), anchor ? static_cast<ssc_cloud_spatial_anchor_handle>(anchor->Handle()) : nullptr);
            callback(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status));
        });
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::DeleteAnchorAsync(std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> const& param_anchor, std::function<void(Microsoft::Azure::SpatialAnchors::Status)> callback) -> void
    {
        auto self = shared_from_this();
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> anchor = param_anchor;
        Microsoft::ApiGen::thread_pool::get().submit_work([self, callback, anchor]
        {
            auto status = ssc_cloud_spatial_anchor_session_delete_anchor_async(static_cast<ssc_cloud_spatial_anchor_session_handle>(self->m_handle), anchor ? static_cast<ssc_cloud_spatial_anchor_handle>(anchor->Handle()) : nullptr);
            callback(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status));
        });
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::ProcessFrame(ArFrame* const& frame) -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_process_frame(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle), frame);
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::GetSessionStatusAsync(std::function<void(Microsoft::Azure::SpatialAnchors::Status, const std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionStatus> &)> callback) -> void
    {
        auto self = shared_from_this();
        Microsoft::ApiGen::thread_pool::get().submit_work([self, callback]
        {
            ssc_session_status_handle outParam_result;
            auto status = ssc_cloud_spatial_anchor_session_get_session_status_async(static_cast<ssc_cloud_spatial_anchor_session_handle>(self->m_handle), &outParam_result);
            std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionStatus> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::SessionStatus>(outParam_result, /* transfer */ true) : nullptr;
            callback(static_cast<Microsoft::Azure::SpatialAnchors::Status>(status), result);
        });
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::Start() -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_start(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::Stop() -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_stop(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorSession::Reset() -> void
    {
        auto status = ssc_cloud_spatial_anchor_session_reset(static_cast<ssc_cloud_spatial_anchor_session_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE CloudSpatialAnchorWatcher::CloudSpatialAnchorWatcher(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_cloud_spatial_anchor_watcher_addref(static_cast<ssc_cloud_spatial_anchor_watcher_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE CloudSpatialAnchorWatcher::~CloudSpatialAnchorWatcher()
    {
        auto status = ssc_cloud_spatial_anchor_watcher_release(static_cast<ssc_cloud_spatial_anchor_watcher_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorWatcher::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorWatcher::Identifier() const -> int32_t
    {
        int32_t result;
        auto status = ssc_cloud_spatial_anchor_watcher_get_identifier(static_cast<ssc_cloud_spatial_anchor_watcher_handle>(m_handle), reinterpret_cast<int*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto CloudSpatialAnchorWatcher::Stop() -> void
    {
        auto status = ssc_cloud_spatial_anchor_watcher_stop(static_cast<ssc_cloud_spatial_anchor_watcher_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE GeoLocation::GeoLocation(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_geo_location_addref(static_cast<ssc_geo_location_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE GeoLocation::~GeoLocation()
    {
        auto status = ssc_geo_location_release(static_cast<ssc_geo_location_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto GeoLocation::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE GeoLocation::GeoLocation()
    {
        auto status = ssc_geo_location_create(reinterpret_cast<ssc_geo_location_handle*>(&m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto GeoLocation::Latitude() const -> double
    {
        double result;
        auto status = ssc_geo_location_get_latitude(static_cast<ssc_geo_location_handle>(m_handle), reinterpret_cast<double*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto GeoLocation::Latitude(double value) -> void
    {
        auto status = ssc_geo_location_set_latitude(static_cast<ssc_geo_location_handle>(m_handle), static_cast<double>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto GeoLocation::Longitude() const -> double
    {
        double result;
        auto status = ssc_geo_location_get_longitude(static_cast<ssc_geo_location_handle>(m_handle), reinterpret_cast<double*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto GeoLocation::Longitude(double value) -> void
    {
        auto status = ssc_geo_location_set_longitude(static_cast<ssc_geo_location_handle>(m_handle), static_cast<double>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto GeoLocation::HorizontalError() const -> float
    {
        float result;
        auto status = ssc_geo_location_get_horizontal_error(static_cast<ssc_geo_location_handle>(m_handle), reinterpret_cast<float*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto GeoLocation::HorizontalError(float value) -> void
    {
        auto status = ssc_geo_location_set_horizontal_error(static_cast<ssc_geo_location_handle>(m_handle), static_cast<float>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto GeoLocation::Altitude() const -> float
    {
        float result;
        auto status = ssc_geo_location_get_altitude(static_cast<ssc_geo_location_handle>(m_handle), reinterpret_cast<float*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto GeoLocation::Altitude(float value) -> void
    {
        auto status = ssc_geo_location_set_altitude(static_cast<ssc_geo_location_handle>(m_handle), static_cast<float>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto GeoLocation::VerticalError() const -> float
    {
        float result;
        auto status = ssc_geo_location_get_vertical_error(static_cast<ssc_geo_location_handle>(m_handle), reinterpret_cast<float*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto GeoLocation::VerticalError(float value) -> void
    {
        auto status = ssc_geo_location_set_vertical_error(static_cast<ssc_geo_location_handle>(m_handle), static_cast<float>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE LocateAnchorsCompletedEventArgs::LocateAnchorsCompletedEventArgs(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_locate_anchors_completed_event_args_addref(static_cast<ssc_locate_anchors_completed_event_args_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE LocateAnchorsCompletedEventArgs::~LocateAnchorsCompletedEventArgs()
    {
        auto status = ssc_locate_anchors_completed_event_args_release(static_cast<ssc_locate_anchors_completed_event_args_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto LocateAnchorsCompletedEventArgs::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto LocateAnchorsCompletedEventArgs::Cancelled() const -> bool
    {
        ssc_bool outParam_result;
        auto status = ssc_locate_anchors_completed_event_args_get_cancelled(static_cast<ssc_locate_anchors_completed_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        bool result = static_cast<bool>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto LocateAnchorsCompletedEventArgs::Watcher() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>
    {
        ssc_cloud_spatial_anchor_watcher_handle outParam_result;
        auto status = ssc_locate_anchors_completed_event_args_get_watcher(static_cast<ssc_locate_anchors_completed_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE NearAnchorCriteria::NearAnchorCriteria(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_near_anchor_criteria_addref(static_cast<ssc_near_anchor_criteria_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE NearAnchorCriteria::~NearAnchorCriteria()
    {
        auto status = ssc_near_anchor_criteria_release(static_cast<ssc_near_anchor_criteria_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto NearAnchorCriteria::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE NearAnchorCriteria::NearAnchorCriteria()
    {
        auto status = ssc_near_anchor_criteria_create(reinterpret_cast<ssc_near_anchor_criteria_handle*>(&m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto NearAnchorCriteria::SourceAnchor() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor>
    {
        ssc_cloud_spatial_anchor_handle outParam_result;
        auto status = ssc_near_anchor_criteria_get_source_anchor(static_cast<ssc_near_anchor_criteria_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE auto NearAnchorCriteria::SourceAnchor(std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchor> const& value) -> void
    {
        auto status = ssc_near_anchor_criteria_set_source_anchor(static_cast<ssc_near_anchor_criteria_handle>(m_handle), value ? static_cast<ssc_cloud_spatial_anchor_handle>(value->Handle()) : nullptr);
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto NearAnchorCriteria::DistanceInMeters() const -> float
    {
        float result;
        auto status = ssc_near_anchor_criteria_get_distance_in_meters(static_cast<ssc_near_anchor_criteria_handle>(m_handle), reinterpret_cast<float*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto NearAnchorCriteria::DistanceInMeters(float value) -> void
    {
        auto status = ssc_near_anchor_criteria_set_distance_in_meters(static_cast<ssc_near_anchor_criteria_handle>(m_handle), static_cast<float>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto NearAnchorCriteria::MaxResultCount() const -> int32_t
    {
        int32_t result;
        auto status = ssc_near_anchor_criteria_get_max_result_count(static_cast<ssc_near_anchor_criteria_handle>(m_handle), reinterpret_cast<int*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto NearAnchorCriteria::MaxResultCount(int32_t value) -> void
    {
        auto status = ssc_near_anchor_criteria_set_max_result_count(static_cast<ssc_near_anchor_criteria_handle>(m_handle), static_cast<int32_t>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE NearDeviceCriteria::NearDeviceCriteria(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_near_device_criteria_addref(static_cast<ssc_near_device_criteria_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE NearDeviceCriteria::~NearDeviceCriteria()
    {
        auto status = ssc_near_device_criteria_release(static_cast<ssc_near_device_criteria_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto NearDeviceCriteria::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE NearDeviceCriteria::NearDeviceCriteria()
    {
        auto status = ssc_near_device_criteria_create(reinterpret_cast<ssc_near_device_criteria_handle*>(&m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto NearDeviceCriteria::DistanceInMeters() const -> float
    {
        float result;
        auto status = ssc_near_device_criteria_get_distance_in_meters(static_cast<ssc_near_device_criteria_handle>(m_handle), reinterpret_cast<float*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto NearDeviceCriteria::DistanceInMeters(float value) -> void
    {
        auto status = ssc_near_device_criteria_set_distance_in_meters(static_cast<ssc_near_device_criteria_handle>(m_handle), static_cast<float>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto NearDeviceCriteria::MaxResultCount() const -> int32_t
    {
        int32_t result;
        auto status = ssc_near_device_criteria_get_max_result_count(static_cast<ssc_near_device_criteria_handle>(m_handle), reinterpret_cast<int*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto NearDeviceCriteria::MaxResultCount(int32_t value) -> void
    {
        auto status = ssc_near_device_criteria_set_max_result_count(static_cast<ssc_near_device_criteria_handle>(m_handle), static_cast<int32_t>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE OnLogDebugEventArgs::OnLogDebugEventArgs(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_on_log_debug_event_args_addref(static_cast<ssc_on_log_debug_event_args_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE OnLogDebugEventArgs::~OnLogDebugEventArgs()
    {
        auto status = ssc_on_log_debug_event_args_release(static_cast<ssc_on_log_debug_event_args_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto OnLogDebugEventArgs::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto OnLogDebugEventArgs::Message() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_on_log_debug_event_args_get_message(static_cast<ssc_on_log_debug_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE PlatformLocationProvider::PlatformLocationProvider(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_platform_location_provider_addref(static_cast<ssc_platform_location_provider_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE PlatformLocationProvider::~PlatformLocationProvider()
    {
        auto status = ssc_platform_location_provider_release(static_cast<ssc_platform_location_provider_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto PlatformLocationProvider::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE PlatformLocationProvider::PlatformLocationProvider()
    {
        auto status = ssc_platform_location_provider_create(reinterpret_cast<ssc_platform_location_provider_handle*>(&m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto PlatformLocationProvider::Sensors() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::SensorCapabilities>
    {
        ssc_sensor_capabilities_handle outParam_result;
        auto status = ssc_platform_location_provider_get_sensors(static_cast<ssc_platform_location_provider_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::SensorCapabilities> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::SensorCapabilities>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE auto PlatformLocationProvider::GeoLocationStatus() const -> Microsoft::Azure::SpatialAnchors::GeoLocationStatusResult
    {
        ssc_geo_location_status_result outParam_result;
        auto status = ssc_platform_location_provider_get_geo_location_status(static_cast<ssc_platform_location_provider_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = static_cast<Microsoft::Azure::SpatialAnchors::GeoLocationStatusResult>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto PlatformLocationProvider::WifiStatus() const -> Microsoft::Azure::SpatialAnchors::WifiStatusResult
    {
        ssc_wifi_status_result outParam_result;
        auto status = ssc_platform_location_provider_get_wifi_status(static_cast<ssc_platform_location_provider_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = static_cast<Microsoft::Azure::SpatialAnchors::WifiStatusResult>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto PlatformLocationProvider::BluetoothStatus() const -> Microsoft::Azure::SpatialAnchors::BluetoothStatusResult
    {
        ssc_bluetooth_status_result outParam_result;
        auto status = ssc_platform_location_provider_get_bluetooth_status(static_cast<ssc_platform_location_provider_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = static_cast<Microsoft::Azure::SpatialAnchors::BluetoothStatusResult>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto PlatformLocationProvider::GetLocationEstimate() -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::GeoLocation>
    {
        ssc_geo_location_handle outParam_result;
        auto status = ssc_platform_location_provider_get_location_estimate(static_cast<ssc_platform_location_provider_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::GeoLocation> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::GeoLocation>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE auto PlatformLocationProvider::Start() -> void
    {
        auto status = ssc_platform_location_provider_start(static_cast<ssc_platform_location_provider_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto PlatformLocationProvider::Stop() -> void
    {
        auto status = ssc_platform_location_provider_stop(static_cast<ssc_platform_location_provider_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE SensorCapabilities::SensorCapabilities(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_sensor_capabilities_addref(static_cast<ssc_sensor_capabilities_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE SensorCapabilities::~SensorCapabilities()
    {
        auto status = ssc_sensor_capabilities_release(static_cast<ssc_sensor_capabilities_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SensorCapabilities::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto SensorCapabilities::GeoLocationEnabled() const -> bool
    {
        ssc_bool outParam_result;
        auto status = ssc_sensor_capabilities_get_geo_location_enabled(static_cast<ssc_sensor_capabilities_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        bool result = static_cast<bool>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto SensorCapabilities::GeoLocationEnabled(bool value) -> void
    {
        auto status = ssc_sensor_capabilities_set_geo_location_enabled(static_cast<ssc_sensor_capabilities_handle>(m_handle), static_cast<bool>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SensorCapabilities::WifiEnabled() const -> bool
    {
        ssc_bool outParam_result;
        auto status = ssc_sensor_capabilities_get_wifi_enabled(static_cast<ssc_sensor_capabilities_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        bool result = static_cast<bool>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto SensorCapabilities::WifiEnabled(bool value) -> void
    {
        auto status = ssc_sensor_capabilities_set_wifi_enabled(static_cast<ssc_sensor_capabilities_handle>(m_handle), static_cast<bool>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SensorCapabilities::BluetoothEnabled() const -> bool
    {
        ssc_bool outParam_result;
        auto status = ssc_sensor_capabilities_get_bluetooth_enabled(static_cast<ssc_sensor_capabilities_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        bool result = static_cast<bool>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto SensorCapabilities::BluetoothEnabled(bool value) -> void
    {
        auto status = ssc_sensor_capabilities_set_bluetooth_enabled(static_cast<ssc_sensor_capabilities_handle>(m_handle), static_cast<bool>(value));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SensorCapabilities::KnownBeaconProximityUuids() const -> std::vector<std::string>
    {
        const char * * outParam_result;
        int outParam_result_count = 0;
        auto status = ssc_sensor_capabilities_get_known_beacon_proximity_uuids(static_cast<ssc_sensor_capabilities_handle>(m_handle), &outParam_result, &outParam_result_count);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::vector<std::string> result_localArray_tmp;
        for (int i = 0; i < outParam_result_count; i++)
        {
            std::string itrValue = outParam_result[i]; free(const_cast<char*>(outParam_result[i]));
            result_localArray_tmp.emplace_back(itrValue);
        }
        free(const_cast<const char * *>(outParam_result));
        auto result = std::move(result_localArray_tmp);
        return result;
    }

    APIGEN_LINKAGE auto SensorCapabilities::KnownBeaconProximityUuids(std::vector<std::string> const& value) -> void
    {
        std::vector<const char *> value_localArray;
        for (const auto& itrValue : value)
        {
            value_localArray.emplace_back(itrValue.c_str());
        }
        auto status = ssc_sensor_capabilities_set_known_beacon_proximity_uuids(static_cast<ssc_sensor_capabilities_handle>(m_handle), value_localArray.data(), static_cast<int>(value_localArray.size()));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE SensorFingerprintEventArgs::SensorFingerprintEventArgs(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_sensor_fingerprint_event_args_addref(static_cast<ssc_sensor_fingerprint_event_args_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE SensorFingerprintEventArgs::~SensorFingerprintEventArgs()
    {
        auto status = ssc_sensor_fingerprint_event_args_release(static_cast<ssc_sensor_fingerprint_event_args_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SensorFingerprintEventArgs::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto SensorFingerprintEventArgs::GeoPosition() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::GeoLocation>
    {
        ssc_geo_location_handle outParam_result;
        auto status = ssc_sensor_fingerprint_event_args_get_geo_position(static_cast<ssc_sensor_fingerprint_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::GeoLocation> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::GeoLocation>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE auto SensorFingerprintEventArgs::GeoPosition(std::shared_ptr<Microsoft::Azure::SpatialAnchors::GeoLocation> const& value) -> void
    {
        auto status = ssc_sensor_fingerprint_event_args_set_geo_position(static_cast<ssc_sensor_fingerprint_event_args_handle>(m_handle), value ? static_cast<ssc_geo_location_handle>(value->Handle()) : nullptr);
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE SessionConfiguration::SessionConfiguration(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_session_configuration_addref(static_cast<ssc_session_configuration_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE SessionConfiguration::~SessionConfiguration()
    {
        auto status = ssc_session_configuration_release(static_cast<ssc_session_configuration_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SessionConfiguration::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto SessionConfiguration::AccountDomain() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_session_configuration_get_account_domain(static_cast<ssc_session_configuration_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto SessionConfiguration::AccountDomain(std::string const& value) -> void
    {
        auto status = ssc_session_configuration_set_account_domain(static_cast<ssc_session_configuration_handle>(m_handle), value.c_str());
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SessionConfiguration::AccountId() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_session_configuration_get_account_id(static_cast<ssc_session_configuration_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto SessionConfiguration::AccountId(std::string const& value) -> void
    {
        auto status = ssc_session_configuration_set_account_id(static_cast<ssc_session_configuration_handle>(m_handle), value.c_str());
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SessionConfiguration::AuthenticationToken() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_session_configuration_get_authentication_token(static_cast<ssc_session_configuration_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto SessionConfiguration::AuthenticationToken(std::string const& value) -> void
    {
        auto status = ssc_session_configuration_set_authentication_token(static_cast<ssc_session_configuration_handle>(m_handle), value.c_str());
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SessionConfiguration::AccountKey() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_session_configuration_get_account_key(static_cast<ssc_session_configuration_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto SessionConfiguration::AccountKey(std::string const& value) -> void
    {
        auto status = ssc_session_configuration_set_account_key(static_cast<ssc_session_configuration_handle>(m_handle), value.c_str());
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SessionConfiguration::AccessToken() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_session_configuration_get_access_token(static_cast<ssc_session_configuration_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto SessionConfiguration::AccessToken(std::string const& value) -> void
    {
        auto status = ssc_session_configuration_set_access_token(static_cast<ssc_session_configuration_handle>(m_handle), value.c_str());
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE SessionErrorEventArgs::SessionErrorEventArgs(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_session_error_event_args_addref(static_cast<ssc_session_error_event_args_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE SessionErrorEventArgs::~SessionErrorEventArgs()
    {
        auto status = ssc_session_error_event_args_release(static_cast<ssc_session_error_event_args_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SessionErrorEventArgs::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto SessionErrorEventArgs::ErrorCode() const -> Microsoft::Azure::SpatialAnchors::CloudSpatialErrorCode
    {
        ssc_cloud_spatial_error_code outParam_result;
        auto status = ssc_session_error_event_args_get_error_code(static_cast<ssc_session_error_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = static_cast<Microsoft::Azure::SpatialAnchors::CloudSpatialErrorCode>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE auto SessionErrorEventArgs::ErrorMessage() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_session_error_event_args_get_error_message(static_cast<ssc_session_error_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto SessionErrorEventArgs::Watcher() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>
    {
        ssc_cloud_spatial_anchor_watcher_handle outParam_result;
        auto status = ssc_session_error_event_args_get_watcher(static_cast<ssc_session_error_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorWatcher>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE SessionStatus::SessionStatus(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_session_status_addref(static_cast<ssc_session_status_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE SessionStatus::~SessionStatus()
    {
        auto status = ssc_session_status_release(static_cast<ssc_session_status_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SessionStatus::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto SessionStatus::ReadyForCreateProgress() const -> float
    {
        float result;
        auto status = ssc_session_status_get_ready_for_create_progress(static_cast<ssc_session_status_handle>(m_handle), reinterpret_cast<float*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto SessionStatus::RecommendedForCreateProgress() const -> float
    {
        float result;
        auto status = ssc_session_status_get_recommended_for_create_progress(static_cast<ssc_session_status_handle>(m_handle), reinterpret_cast<float*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto SessionStatus::SessionCreateHash() const -> int32_t
    {
        int32_t result;
        auto status = ssc_session_status_get_session_create_hash(static_cast<ssc_session_status_handle>(m_handle), reinterpret_cast<int*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto SessionStatus::SessionLocateHash() const -> int32_t
    {
        int32_t result;
        auto status = ssc_session_status_get_session_locate_hash(static_cast<ssc_session_status_handle>(m_handle), reinterpret_cast<int*>(&result));
        Microsoft::ApiGen::check_status(m_handle, status);
        return result;
    }

    APIGEN_LINKAGE auto SessionStatus::UserFeedback() const -> Microsoft::Azure::SpatialAnchors::SessionUserFeedback
    {
        ssc_session_user_feedback outParam_result;
        auto status = ssc_session_status_get_user_feedback(static_cast<ssc_session_status_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        auto result = static_cast<Microsoft::Azure::SpatialAnchors::SessionUserFeedback>(outParam_result);
        return result;
    }

    APIGEN_LINKAGE SessionUpdatedEventArgs::SessionUpdatedEventArgs(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_session_updated_event_args_addref(static_cast<ssc_session_updated_event_args_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE SessionUpdatedEventArgs::~SessionUpdatedEventArgs()
    {
        auto status = ssc_session_updated_event_args_release(static_cast<ssc_session_updated_event_args_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto SessionUpdatedEventArgs::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto SessionUpdatedEventArgs::Status() const -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionStatus>
    {
        ssc_session_status_handle outParam_result;
        auto status = ssc_session_updated_event_args_get_status(static_cast<ssc_session_updated_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::SessionStatus> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::SessionStatus>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

    APIGEN_LINKAGE TokenRequiredEventArgs::TokenRequiredEventArgs(void* handle, bool noCopy)
        : m_handle(handle)
    {
        if (!noCopy)
        {
            auto status = ssc_token_required_event_args_addref(static_cast<ssc_token_required_event_args_handle>(m_handle));
            Microsoft::ApiGen::check_status(m_handle, status);
        }
    }

    APIGEN_LINKAGE TokenRequiredEventArgs::~TokenRequiredEventArgs()
    {
        auto status = ssc_token_required_event_args_release(static_cast<ssc_token_required_event_args_handle>(m_handle));
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto TokenRequiredEventArgs::Handle() const -> void*
    {
        return m_handle;
    }

    APIGEN_LINKAGE auto TokenRequiredEventArgs::AccessToken() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_token_required_event_args_get_access_token(static_cast<ssc_token_required_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto TokenRequiredEventArgs::AccessToken(std::string const& value) -> void
    {
        auto status = ssc_token_required_event_args_set_access_token(static_cast<ssc_token_required_event_args_handle>(m_handle), value.c_str());
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto TokenRequiredEventArgs::AuthenticationToken() const -> std::string
    {
        const char * outParam_result;
        auto status = ssc_token_required_event_args_get_authentication_token(static_cast<ssc_token_required_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::string result = outParam_result; free(const_cast<char*>(outParam_result));
        return result;
    }

    APIGEN_LINKAGE auto TokenRequiredEventArgs::AuthenticationToken(std::string const& value) -> void
    {
        auto status = ssc_token_required_event_args_set_authentication_token(static_cast<ssc_token_required_event_args_handle>(m_handle), value.c_str());
        Microsoft::ApiGen::check_status(m_handle, status);
    }

    APIGEN_LINKAGE auto TokenRequiredEventArgs::GetDeferral() -> std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSessionDeferral>
    {
        ssc_cloud_spatial_anchor_session_deferral_handle outParam_result;
        auto status = ssc_token_required_event_args_get_deferral(static_cast<ssc_token_required_event_args_handle>(m_handle), &outParam_result);
        Microsoft::ApiGen::check_status(m_handle, status);
        std::shared_ptr<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSessionDeferral> result = outParam_result ? std::make_shared<Microsoft::Azure::SpatialAnchors::CloudSpatialAnchorSessionDeferral>(outParam_result, /* transfer */ true) : nullptr;
        return result;
    }

} } }

