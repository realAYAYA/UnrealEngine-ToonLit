/**
 * Copyright (c) 2021-2022 LunarG, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * DO NOT EDIT: This file is generated.
 */

#pragma warning(push)
#pragma warning(disable : 4191) // warning C4191: 'type cast': unsafe conversion

#ifndef VULKAN_PROFILES_HPP_
#define VULKAN_PROFILES_HPP_ 1

#define VPAPI_ATTR inline

#include <vulkan/vulkan.h>
#include <cstddef>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <vector>
#include <algorithm>

#if defined(VK_VERSION_1_1)
#define VP_UE_Vulkan_ES3_1_Android 1
#define VP_UE_VULKAN_ES3_1_ANDROID_NAME "VP_UE_Vulkan_ES3_1_Android"
#define VP_UE_VULKAN_ES3_1_ANDROID_SPEC_VERSION 1
#define VP_UE_VULKAN_ES3_1_ANDROID_MIN_API_VERSION VK_MAKE_VERSION(1, 1, 0)
#endif

#if defined(VK_VERSION_1_1)
#define VP_UE_Vulkan_SM5 1
#define VP_UE_VULKAN_SM5_NAME "VP_UE_Vulkan_SM5"
#define VP_UE_VULKAN_SM5_SPEC_VERSION 1
#define VP_UE_VULKAN_SM5_MIN_API_VERSION VK_MAKE_VERSION(1, 1, 0)
#endif

#if defined(VK_VERSION_1_1)
#define VP_UE_Vulkan_SM5_Android 1
#define VP_UE_VULKAN_SM5_ANDROID_NAME "VP_UE_Vulkan_SM5_Android"
#define VP_UE_VULKAN_SM5_ANDROID_SPEC_VERSION 1
#define VP_UE_VULKAN_SM5_ANDROID_MIN_API_VERSION VK_MAKE_VERSION(1, 1, 0)
#endif

#if defined(VK_VERSION_1_2) && \
    defined(VK_EXT_scalar_block_layout) && \
    defined(VK_KHR_acceleration_structure) && \
    defined(VK_KHR_buffer_device_address) && \
    defined(VK_KHR_deferred_host_operations) && \
    defined(VK_KHR_ray_query) && \
    defined(VK_KHR_shader_float_controls) && \
    defined(VK_KHR_spirv_1_4)
#define VP_UE_Vulkan_SM5_Android_RT 1
#define VP_UE_VULKAN_SM5_ANDROID_RT_NAME "VP_UE_Vulkan_SM5_Android_RT"
#define VP_UE_VULKAN_SM5_ANDROID_RT_SPEC_VERSION 1
#define VP_UE_VULKAN_SM5_ANDROID_RT_MIN_API_VERSION VK_MAKE_VERSION(1, 2, 0)
#endif

#if defined(VK_VERSION_1_2) && \
    defined(VK_EXT_scalar_block_layout) && \
    defined(VK_KHR_acceleration_structure) && \
    defined(VK_KHR_buffer_device_address) && \
    defined(VK_KHR_deferred_host_operations) && \
    defined(VK_KHR_ray_query) && \
    defined(VK_KHR_ray_tracing_pipeline) && \
    defined(VK_KHR_shader_float_controls) && \
    defined(VK_KHR_spirv_1_4)
#define VP_UE_Vulkan_SM5_RT 1
#define VP_UE_VULKAN_SM5_RT_NAME "VP_UE_Vulkan_SM5_RT"
#define VP_UE_VULKAN_SM5_RT_SPEC_VERSION 1
#define VP_UE_VULKAN_SM5_RT_MIN_API_VERSION VK_MAKE_VERSION(1, 2, 0)
#endif

#if defined(VK_VERSION_1_3) && \
    defined(VK_EXT_descriptor_indexing) && \
    defined(VK_EXT_scalar_block_layout) && \
    defined(VK_EXT_shader_image_atomic_int64)
#define VP_UE_Vulkan_SM6 1
#define VP_UE_VULKAN_SM6_NAME "VP_UE_Vulkan_SM6"
#define VP_UE_VULKAN_SM6_SPEC_VERSION 1
#define VP_UE_VULKAN_SM6_MIN_API_VERSION VK_MAKE_VERSION(1, 3, 0)
#endif

#if defined(VK_VERSION_1_3) && \
    defined(VK_EXT_descriptor_indexing) && \
    defined(VK_EXT_scalar_block_layout) && \
    defined(VK_EXT_shader_image_atomic_int64) && \
    defined(VK_KHR_acceleration_structure) && \
    defined(VK_KHR_buffer_device_address) && \
    defined(VK_KHR_deferred_host_operations) && \
    defined(VK_KHR_ray_query) && \
    defined(VK_KHR_ray_tracing_pipeline) && \
    defined(VK_KHR_shader_float_controls) && \
    defined(VK_KHR_spirv_1_4)
#define VP_UE_Vulkan_SM6_RT 1
#define VP_UE_VULKAN_SM6_RT_NAME "VP_UE_Vulkan_SM6_RT"
#define VP_UE_VULKAN_SM6_RT_SPEC_VERSION 1
#define VP_UE_VULKAN_SM6_RT_MIN_API_VERSION VK_MAKE_VERSION(1, 3, 0)
#endif

#define VP_MAX_PROFILE_NAME_SIZE 256U

typedef struct VpProfileProperties {
    char        profileName[VP_MAX_PROFILE_NAME_SIZE];
    uint32_t    specVersion;
} VpProfileProperties;

typedef enum VpInstanceCreateFlagBits {
    // Default behavior:
    // - profile extensions are used (application must not specify extensions)

    // Merge application provided extension list and profile extension list
    VP_INSTANCE_CREATE_MERGE_EXTENSIONS_BIT = 0x00000001,

    // Use application provided extension list
    VP_INSTANCE_CREATE_OVERRIDE_EXTENSIONS_BIT = 0x00000002,

    VP_INSTANCE_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VpInstanceCreateFlagBits;
typedef VkFlags VpInstanceCreateFlags;

typedef struct VpInstanceCreateInfo {
    const VkInstanceCreateInfo* pCreateInfo;
    const VpProfileProperties*  pProfile;
    VpInstanceCreateFlags       flags;
} VpInstanceCreateInfo;

typedef enum VpDeviceCreateFlagBits {
    // Default behavior:
    // - profile extensions are used (application must not specify extensions)
    // - profile feature structures are used (application must not specify any of them) extended
    //   with any other application provided struct that isn't defined by the profile

    // Merge application provided extension list and profile extension list
    VP_DEVICE_CREATE_MERGE_EXTENSIONS_BIT = 0x00000001,

    // Use application provided extension list
    VP_DEVICE_CREATE_OVERRIDE_EXTENSIONS_BIT = 0x00000002,

    // Merge application provided versions of feature structures with the profile features
    // Currently unsupported, but is considered for future inclusion in which case the
    // default behavior could potentially be changed to merging as the currently defined
    // default behavior is forward-compatible with that
    // VP_DEVICE_CREATE_MERGE_FEATURES_BIT = 0x00000004,

    // Use application provided versions of feature structures but use the profile feature
    // structures when the application doesn't provide one (robust access disable flags are
    // ignored if the application overrides the corresponding feature structures)
    VP_DEVICE_CREATE_OVERRIDE_FEATURES_BIT = 0x00000008,

    // Only use application provided feature structures, don't add any profile specific
    // feature structures (robust access disable flags are ignored in this case and only the
    // application provided structures are used)
    VP_DEVICE_CREATE_OVERRIDE_ALL_FEATURES_BIT = 0x00000010,

    VP_DEVICE_CREATE_DISABLE_ROBUST_BUFFER_ACCESS_BIT = 0x00000020,
    VP_DEVICE_CREATE_DISABLE_ROBUST_IMAGE_ACCESS_BIT = 0x00000040,
    VP_DEVICE_CREATE_DISABLE_ROBUST_ACCESS =
        VP_DEVICE_CREATE_DISABLE_ROBUST_BUFFER_ACCESS_BIT | VP_DEVICE_CREATE_DISABLE_ROBUST_IMAGE_ACCESS_BIT,

    VP_DEVICE_CREATE_FLAG_BITS_MAX_ENUM = 0x7FFFFFFF
} VpDeviceCreateFlagBits;
typedef VkFlags VpDeviceCreateFlags;

typedef struct VpDeviceCreateInfo {
    const VkDeviceCreateInfo*   pCreateInfo;
    const VpProfileProperties*  pProfile;
    VpDeviceCreateFlags         flags;
} VpDeviceCreateInfo;

// Query the list of available profiles in the library
VPAPI_ATTR VkResult vpGetProfiles(uint32_t *pPropertyCount, VpProfileProperties *pProperties);

// List the recommended fallback profiles of a profile
VPAPI_ATTR VkResult vpGetProfileFallbacks(const VpProfileProperties *pProfile, uint32_t *pPropertyCount, VpProfileProperties *pProperties);

// Check whether a profile is supported at the instance level
VPAPI_ATTR VkResult vpGetInstanceProfileSupport(const char *pLayerName, const VpProfileProperties *pProfile, VkBool32 *pSupported);

// Create a VkInstance with the profile instance extensions enabled
VPAPI_ATTR VkResult vpCreateInstance(const VpInstanceCreateInfo *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator, VkInstance *pInstance);

// Check whether a profile is supported by the physical device
VPAPI_ATTR VkResult vpGetPhysicalDeviceProfileSupport(VkInstance instance, VkPhysicalDevice physicalDevice,
                                                      const VpProfileProperties *pProfile, VkBool32 *pSupported);

// Create a VkDevice with the profile features and device extensions enabled
VPAPI_ATTR VkResult vpCreateDevice(VkPhysicalDevice physicalDevice, const VpDeviceCreateInfo *pCreateInfo,
                                   const VkAllocationCallbacks *pAllocator, VkDevice *pDevice);

// Query the list of instance extensions of a profile
VPAPI_ATTR VkResult vpGetProfileInstanceExtensionProperties(const VpProfileProperties *pProfile, uint32_t *pPropertyCount,
                                                            VkExtensionProperties *pProperties);

// Query the list of device extensions of a profile
VPAPI_ATTR VkResult vpGetProfileDeviceExtensionProperties(const VpProfileProperties *pProfile, uint32_t *pPropertyCount,
                                                          VkExtensionProperties *pProperties);

// Fill the feature structures with the requirements of a profile
VPAPI_ATTR void vpGetProfileFeatures(const VpProfileProperties *pProfile, void *pNext);

// Query the list of feature structure types specified by the profile
VPAPI_ATTR VkResult vpGetProfileFeatureStructureTypes(const VpProfileProperties *pProfile, uint32_t *pStructureTypeCount,
                                                      VkStructureType *pStructureTypes);

// Fill the property structures with the requirements of a profile
VPAPI_ATTR void vpGetProfileProperties(const VpProfileProperties *pProfile, void *pNext);

// Query the list of property structure types specified by the profile
VPAPI_ATTR VkResult vpGetProfilePropertyStructureTypes(const VpProfileProperties *pProfile, uint32_t *pStructureTypeCount,
                                                       VkStructureType *pStructureTypes);

// Query the requirements of queue families by a profile
VPAPI_ATTR VkResult vpGetProfileQueueFamilyProperties(const VpProfileProperties *pProfile, uint32_t *pPropertyCount,
                                                      VkQueueFamilyProperties2KHR *pProperties);

// Query the list of query family structure types specified by the profile
VPAPI_ATTR VkResult vpGetProfileQueueFamilyStructureTypes(const VpProfileProperties *pProfile, uint32_t *pStructureTypeCount,
                                                          VkStructureType *pStructureTypes);

// Query the list of formats with specified requirements by a profile
VPAPI_ATTR VkResult vpGetProfileFormats(const VpProfileProperties *pProfile, uint32_t *pFormatCount, VkFormat *pFormats);

// Query the requirements of a format for a profile
VPAPI_ATTR void vpGetProfileFormatProperties(const VpProfileProperties *pProfile, VkFormat format, void *pNext);

// Query the list of format structure types specified by the profile
VPAPI_ATTR VkResult vpGetProfileFormatStructureTypes(const VpProfileProperties *pProfile, uint32_t *pStructureTypeCount,
                                                     VkStructureType *pStructureTypes);

#include <stdio.h>

#ifndef VP_DEBUG_MESSAGE_CALLBACK
#if defined(ANDROID) || defined(__ANDROID__)
#include <android/log.h>
#define VP_DEBUG_MESSAGE_CALLBACK(MSG)     __android_log_print(ANDROID_LOG_ERROR, "Profiles ERROR", "%s", MSG); \
    __android_log_print(ANDROID_LOG_DEBUG, "Profiles WARNING", "%s", MSG)
#else
#define VP_DEBUG_MESSAGE_CALLBACK(MSG) fprintf(stderr, "%s\n", MSG)
#endif
#else
void VP_DEBUG_MESSAGE_CALLBACK(const char*);
#endif

#define VP_DEBUG_MSG(MSG) VP_DEBUG_MESSAGE_CALLBACK(MSG)
#define VP_DEBUG_MSGF(MSGFMT, ...) { char msg[1024]; snprintf(msg, sizeof(msg) - 1, (MSGFMT), __VA_ARGS__); VP_DEBUG_MESSAGE_CALLBACK(msg); }
#define VP_DEBUG_COND_MSG(COND, MSG) if (COND) VP_DEBUG_MSG(MSG)
#define VP_DEBUG_COND_MSGF(COND, MSGFMT, ...) if (COND) VP_DEBUG_MSGF(MSGFMT, __VA_ARGS__)

#include <string>

namespace detail {

VPAPI_ATTR std::string vpGetDeviceAndDriverInfoString(VkPhysicalDevice physicalDevice,
                                                      PFN_vkGetPhysicalDeviceProperties2KHR pfnGetPhysicalDeviceProperties2) {
    VkPhysicalDeviceDriverPropertiesKHR driverProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES_KHR };
    VkPhysicalDeviceProperties2KHR deviceProps{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR, &driverProps };
    pfnGetPhysicalDeviceProperties2(physicalDevice, &deviceProps);
    return std::string("deviceName=") + std::string(&deviceProps.properties.deviceName[0])
                    + ", driverName=" + std::string(&driverProps.driverName[0])
                    + ", driverInfo=" + std::string(&driverProps.driverInfo[0]);
}

}

namespace detail {


VPAPI_ATTR bool isMultiple(double source, double multiple) {
    double mod = std::fmod(source, multiple);
    return std::abs(mod) < 0.0001; 
}

VPAPI_ATTR bool isPowerOfTwo(double source) {
    double mod = std::fmod(source, 1.0);
    if (std::abs(mod) >= 0.0001) return false;

    std::uint64_t value = static_cast<std::uint64_t>(std::abs(source));
    return !(value & (value - static_cast<std::uint64_t>(1)));
}

using PFN_vpStructFiller = void(*)(VkBaseOutStructure* p);
using PFN_vpStructComparator = bool(*)(VkBaseOutStructure* p);
using PFN_vpStructChainerCb =  void(*)(VkBaseOutStructure* p, void* pUser);
using PFN_vpStructChainer = void(*)(VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb);

struct VpFeatureDesc {
    PFN_vpStructFiller              pfnFiller;
    PFN_vpStructComparator          pfnComparator;
    PFN_vpStructChainer             pfnChainer;
};

struct VpPropertyDesc {
    PFN_vpStructFiller              pfnFiller;
    PFN_vpStructComparator          pfnComparator;
    PFN_vpStructChainer             pfnChainer;
};

struct VpQueueFamilyDesc {
    PFN_vpStructFiller              pfnFiller;
    PFN_vpStructComparator          pfnComparator;
};

struct VpFormatDesc {
    VkFormat                        format;
    PFN_vpStructFiller              pfnFiller;
    PFN_vpStructComparator          pfnComparator;
};

struct VpStructChainerDesc {
    PFN_vpStructChainer             pfnFeature;
    PFN_vpStructChainer             pfnProperty;
    PFN_vpStructChainer             pfnQueueFamily;
    PFN_vpStructChainer             pfnFormat;
};

struct VpProfileDesc {
    VpProfileProperties             props;
    uint32_t                        minApiVersion;

    const VkExtensionProperties*    pInstanceExtensions;
    uint32_t                        instanceExtensionCount;

    const VkExtensionProperties*    pDeviceExtensions;
    uint32_t                        deviceExtensionCount;

    const VpProfileProperties*      pFallbacks;
    uint32_t                        fallbackCount;

    const VkStructureType*          pFeatureStructTypes;
    uint32_t                        featureStructTypeCount;
    VpFeatureDesc                   feature;

    const VkStructureType*          pPropertyStructTypes;
    uint32_t                        propertyStructTypeCount;
    VpPropertyDesc                  property;

    const VkStructureType*          pQueueFamilyStructTypes;
    uint32_t                        queueFamilyStructTypeCount;
    const VpQueueFamilyDesc*        pQueueFamilies;
    uint32_t                        queueFamilyCount;

    const VkStructureType*          pFormatStructTypes;
    uint32_t                        formatStructTypeCount;
    const VpFormatDesc*             pFormats;
    uint32_t                        formatCount;

    VpStructChainerDesc             chainers;
};

template <typename T>
VPAPI_ATTR bool vpCheckFlags(const T& actual, const uint64_t expected) {
    return (actual & expected) == expected;
}

#ifdef VP_UE_Vulkan_ES3_1_Android
namespace VP_UE_VULKAN_ES3_1_ANDROID {

static const VpFeatureDesc featureDesc = {
    [](VkBaseOutStructure* p) {
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
        return ret;
    }
};

static const VpPropertyDesc propertyDesc = {
    [](VkBaseOutStructure* p) {
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
        return ret;
    }
};

static const VpStructChainerDesc chainerDesc = {
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
};

} // namespace VP_UE_VULKAN_ES3_1_ANDROID
#endif

#ifdef VP_UE_Vulkan_SM5
namespace VP_UE_VULKAN_SM5 {

static const VkStructureType featureStructTypes[] = {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
};

static const VkStructureType propertyStructTypes[] = {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
};

static const VpFeatureDesc featureDesc = {
    [](VkBaseOutStructure* p) {
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR: {
                    VkPhysicalDeviceFeatures2KHR* s = static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p));
                    s->features.fragmentStoresAndAtomics = VK_TRUE;
                } break;
                default: break;
            }
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR: {
                    VkPhysicalDeviceFeatures2KHR* prettify_VkPhysicalDeviceFeatures2KHR = static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceFeatures2KHR->features.fragmentStoresAndAtomics == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceFeatures2KHR->features.fragmentStoresAndAtomics == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceFeatures2KHR::features.fragmentStoresAndAtomics == VK_TRUE");
                } break;
                default: break;
            }
        return ret;
    }
};

static const VpPropertyDesc propertyDesc = {
    [](VkBaseOutStructure* p) {
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR: {
                    VkPhysicalDeviceProperties2KHR* s = static_cast<VkPhysicalDeviceProperties2KHR*>(static_cast<void*>(p));
                    s->properties.limits.maxBoundDescriptorSets = 4;
                } break;
                default: break;
            }
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR: {
                    VkPhysicalDeviceProperties2KHR* prettify_VkPhysicalDeviceProperties2KHR = static_cast<VkPhysicalDeviceProperties2KHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceProperties2KHR->properties.limits.maxBoundDescriptorSets >= 4); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceProperties2KHR->properties.limits.maxBoundDescriptorSets >= 4), "Unsupported properties condition: VkPhysicalDeviceProperties2KHR::properties.limits.maxBoundDescriptorSets >= 4");
                } break;
                default: break;
            }
        return ret;
    }
};

static const VpStructChainerDesc chainerDesc = {
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        p->pNext = static_cast<VkBaseOutStructure*>(static_cast<void*>(nullptr));
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        p->pNext = static_cast<VkBaseOutStructure*>(static_cast<void*>(nullptr));
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
};

} // namespace VP_UE_VULKAN_SM5
#endif

#ifdef VP_UE_Vulkan_SM5_Android
namespace VP_UE_VULKAN_SM5_ANDROID {

static const VpFeatureDesc featureDesc = {
    [](VkBaseOutStructure* p) {
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
        return ret;
    }
};

static const VpPropertyDesc propertyDesc = {
    [](VkBaseOutStructure* p) {
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
        return ret;
    }
};

static const VpStructChainerDesc chainerDesc = {
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
};

} // namespace VP_UE_VULKAN_SM5_ANDROID
#endif

#ifdef VP_UE_Vulkan_SM5_Android_RT
namespace VP_UE_VULKAN_SM5_ANDROID_RT {

static const VkExtensionProperties deviceExtensions[] = {
    VkExtensionProperties{ VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_RAY_QUERY_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_SPIRV_1_4_EXTENSION_NAME, 1 },
};

static const VkStructureType featureStructTypes[] = {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
};

static const VkStructureType propertyStructTypes[] = {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
};

static const VpFeatureDesc featureDesc = {
    [](VkBaseOutStructure* p) {
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR: {
                    VkPhysicalDeviceRayQueryFeaturesKHR* s = static_cast<VkPhysicalDeviceRayQueryFeaturesKHR*>(static_cast<void*>(p));
                    s->rayQuery = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR: {
                    VkPhysicalDeviceAccelerationStructureFeaturesKHR* s = static_cast<VkPhysicalDeviceAccelerationStructureFeaturesKHR*>(static_cast<void*>(p));
                    s->accelerationStructure = VK_TRUE;
                    s->descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT: {
                    VkPhysicalDeviceScalarBlockLayoutFeaturesEXT* s = static_cast<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT*>(static_cast<void*>(p));
                    s->scalarBlockLayout = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR: {
                    VkPhysicalDeviceFeatures2KHR* s = static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p));
                    s->features.fragmentStoresAndAtomics = VK_TRUE;
                } break;
                default: break;
            }
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR: {
                    VkPhysicalDeviceRayQueryFeaturesKHR* prettify_VkPhysicalDeviceRayQueryFeaturesKHR = static_cast<VkPhysicalDeviceRayQueryFeaturesKHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceRayQueryFeaturesKHR->rayQuery == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceRayQueryFeaturesKHR->rayQuery == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceRayQueryFeaturesKHR::rayQuery == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR: {
                    VkPhysicalDeviceAccelerationStructureFeaturesKHR* prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR = static_cast<VkPhysicalDeviceAccelerationStructureFeaturesKHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructure == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructure == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->descriptorBindingAccelerationStructureUpdateAfterBind == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->descriptorBindingAccelerationStructureUpdateAfterBind == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceAccelerationStructureFeaturesKHR::descriptorBindingAccelerationStructureUpdateAfterBind == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT: {
                    VkPhysicalDeviceScalarBlockLayoutFeaturesEXT* prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT = static_cast<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT->scalarBlockLayout == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT->scalarBlockLayout == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceScalarBlockLayoutFeaturesEXT::scalarBlockLayout == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR: {
                    VkPhysicalDeviceFeatures2KHR* prettify_VkPhysicalDeviceFeatures2KHR = static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceFeatures2KHR->features.fragmentStoresAndAtomics == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceFeatures2KHR->features.fragmentStoresAndAtomics == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceFeatures2KHR::features.fragmentStoresAndAtomics == VK_TRUE");
                } break;
                default: break;
            }
        return ret;
    }
};

static const VpPropertyDesc propertyDesc = {
    [](VkBaseOutStructure* p) {
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR: {
                    VkPhysicalDeviceProperties2KHR* s = static_cast<VkPhysicalDeviceProperties2KHR*>(static_cast<void*>(p));
                    s->properties.limits.maxBoundDescriptorSets = 7;
                } break;
                default: break;
            }
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR: {
                    VkPhysicalDeviceProperties2KHR* prettify_VkPhysicalDeviceProperties2KHR = static_cast<VkPhysicalDeviceProperties2KHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceProperties2KHR->properties.limits.maxBoundDescriptorSets >= 7); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceProperties2KHR->properties.limits.maxBoundDescriptorSets >= 7), "Unsupported properties condition: VkPhysicalDeviceProperties2KHR::properties.limits.maxBoundDescriptorSets >= 7");
                } break;
                default: break;
            }
        return ret;
    }
};

static const VpStructChainerDesc chainerDesc = {
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        VkPhysicalDeviceRayQueryFeaturesKHR physicalDeviceRayQueryFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, nullptr };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR physicalDeviceAccelerationStructureFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, &physicalDeviceRayQueryFeaturesKHR };
        VkPhysicalDeviceScalarBlockLayoutFeaturesEXT physicalDeviceScalarBlockLayoutFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT, &physicalDeviceAccelerationStructureFeaturesKHR };
        p->pNext = static_cast<VkBaseOutStructure*>(static_cast<void*>(&physicalDeviceScalarBlockLayoutFeaturesEXT));
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        p->pNext = static_cast<VkBaseOutStructure*>(static_cast<void*>(nullptr));
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
};

} // namespace VP_UE_VULKAN_SM5_ANDROID_RT
#endif

#ifdef VP_UE_Vulkan_SM5_RT
namespace VP_UE_VULKAN_SM5_RT {

static const VkExtensionProperties deviceExtensions[] = {
    VkExtensionProperties{ VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_RAY_QUERY_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_SPIRV_1_4_EXTENSION_NAME, 1 },
};

static const VkStructureType featureStructTypes[] = {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT,
};

static const VkStructureType propertyStructTypes[] = {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
};

static const VpFeatureDesc featureDesc = {
    [](VkBaseOutStructure* p) {
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR: {
                    VkPhysicalDeviceFeatures2KHR* s = static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p));
                    s->features.fragmentStoresAndAtomics = VK_TRUE;
                    s->features.shaderInt64 = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR: {
                    VkPhysicalDeviceRayQueryFeaturesKHR* s = static_cast<VkPhysicalDeviceRayQueryFeaturesKHR*>(static_cast<void*>(p));
                    s->rayQuery = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR: {
                    VkPhysicalDeviceRayTracingPipelineFeaturesKHR* s = static_cast<VkPhysicalDeviceRayTracingPipelineFeaturesKHR*>(static_cast<void*>(p));
                    s->rayTracingPipeline = VK_TRUE;
                    s->rayTraversalPrimitiveCulling = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR: {
                    VkPhysicalDeviceAccelerationStructureFeaturesKHR* s = static_cast<VkPhysicalDeviceAccelerationStructureFeaturesKHR*>(static_cast<void*>(p));
                    s->accelerationStructure = VK_TRUE;
                    s->descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT: {
                    VkPhysicalDeviceScalarBlockLayoutFeaturesEXT* s = static_cast<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT*>(static_cast<void*>(p));
                    s->scalarBlockLayout = VK_TRUE;
                } break;
                default: break;
            }
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR: {
                    VkPhysicalDeviceFeatures2KHR* prettify_VkPhysicalDeviceFeatures2KHR = static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceFeatures2KHR->features.fragmentStoresAndAtomics == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceFeatures2KHR->features.fragmentStoresAndAtomics == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceFeatures2KHR::features.fragmentStoresAndAtomics == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceFeatures2KHR->features.shaderInt64 == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceFeatures2KHR->features.shaderInt64 == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceFeatures2KHR::features.shaderInt64 == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR: {
                    VkPhysicalDeviceRayQueryFeaturesKHR* prettify_VkPhysicalDeviceRayQueryFeaturesKHR = static_cast<VkPhysicalDeviceRayQueryFeaturesKHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceRayQueryFeaturesKHR->rayQuery == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceRayQueryFeaturesKHR->rayQuery == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceRayQueryFeaturesKHR::rayQuery == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR: {
                    VkPhysicalDeviceRayTracingPipelineFeaturesKHR* prettify_VkPhysicalDeviceRayTracingPipelineFeaturesKHR = static_cast<VkPhysicalDeviceRayTracingPipelineFeaturesKHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceRayTracingPipelineFeaturesKHR->rayTracingPipeline == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceRayTracingPipelineFeaturesKHR->rayTracingPipeline == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceRayTracingPipelineFeaturesKHR->rayTraversalPrimitiveCulling == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceRayTracingPipelineFeaturesKHR->rayTraversalPrimitiveCulling == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTraversalPrimitiveCulling == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR: {
                    VkPhysicalDeviceAccelerationStructureFeaturesKHR* prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR = static_cast<VkPhysicalDeviceAccelerationStructureFeaturesKHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructure == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructure == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->descriptorBindingAccelerationStructureUpdateAfterBind == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->descriptorBindingAccelerationStructureUpdateAfterBind == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceAccelerationStructureFeaturesKHR::descriptorBindingAccelerationStructureUpdateAfterBind == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT: {
                    VkPhysicalDeviceScalarBlockLayoutFeaturesEXT* prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT = static_cast<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT->scalarBlockLayout == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT->scalarBlockLayout == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceScalarBlockLayoutFeaturesEXT::scalarBlockLayout == VK_TRUE");
                } break;
                default: break;
            }
        return ret;
    }
};

static const VpPropertyDesc propertyDesc = {
    [](VkBaseOutStructure* p) {
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR: {
                    VkPhysicalDeviceProperties2KHR* s = static_cast<VkPhysicalDeviceProperties2KHR*>(static_cast<void*>(p));
                    s->properties.limits.maxBoundDescriptorSets = 8;
                } break;
                default: break;
            }
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR: {
                    VkPhysicalDeviceProperties2KHR* prettify_VkPhysicalDeviceProperties2KHR = static_cast<VkPhysicalDeviceProperties2KHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceProperties2KHR->properties.limits.maxBoundDescriptorSets >= 8); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceProperties2KHR->properties.limits.maxBoundDescriptorSets >= 8), "Unsupported properties condition: VkPhysicalDeviceProperties2KHR::properties.limits.maxBoundDescriptorSets >= 8");
                } break;
                default: break;
            }
        return ret;
    }
};

static const VpStructChainerDesc chainerDesc = {
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        VkPhysicalDeviceRayQueryFeaturesKHR physicalDeviceRayQueryFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, nullptr };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR physicalDeviceRayTracingPipelineFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, &physicalDeviceRayQueryFeaturesKHR };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR physicalDeviceAccelerationStructureFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, &physicalDeviceRayTracingPipelineFeaturesKHR };
        VkPhysicalDeviceScalarBlockLayoutFeaturesEXT physicalDeviceScalarBlockLayoutFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT, &physicalDeviceAccelerationStructureFeaturesKHR };
        p->pNext = static_cast<VkBaseOutStructure*>(static_cast<void*>(&physicalDeviceScalarBlockLayoutFeaturesEXT));
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        p->pNext = static_cast<VkBaseOutStructure*>(static_cast<void*>(nullptr));
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
};

} // namespace VP_UE_VULKAN_SM5_RT
#endif

#ifdef VP_UE_Vulkan_SM6
namespace VP_UE_VULKAN_SM6 {

static const VkExtensionProperties deviceExtensions[] = {
    VkExtensionProperties{ VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME, 1 },
};

static const VkStructureType featureStructTypes[] = {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
};

static const VkStructureType formatStructTypes[] = {
    VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR,
    VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR,
};

static const VpFeatureDesc featureDesc = {
    [](VkBaseOutStructure* p) {
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT: {
                    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT* s = static_cast<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT*>(static_cast<void*>(p));
                    s->shaderImageInt64Atomics = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT: {
                    VkPhysicalDeviceDescriptorIndexingFeaturesEXT* s = static_cast<VkPhysicalDeviceDescriptorIndexingFeaturesEXT*>(static_cast<void*>(p));
                    s->descriptorBindingPartiallyBound = VK_TRUE;
                    s->descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
                    s->descriptorBindingVariableDescriptorCount = VK_TRUE;
                    s->runtimeDescriptorArray = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES: {
                    VkPhysicalDeviceShaderAtomicInt64Features* s = static_cast<VkPhysicalDeviceShaderAtomicInt64Features*>(static_cast<void*>(p));
                    s->shaderBufferInt64Atomics = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES: {
                    VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures* s = static_cast<VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures*>(static_cast<void*>(p));
                    s->separateDepthStencilLayouts = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES: {
                    VkPhysicalDeviceSynchronization2Features* s = static_cast<VkPhysicalDeviceSynchronization2Features*>(static_cast<void*>(p));
                    s->synchronization2 = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES: {
                    VkPhysicalDeviceMaintenance4Features* s = static_cast<VkPhysicalDeviceMaintenance4Features*>(static_cast<void*>(p));
                    s->maintenance4 = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES: {
                    VkPhysicalDeviceBufferDeviceAddressFeatures* s = static_cast<VkPhysicalDeviceBufferDeviceAddressFeatures*>(static_cast<void*>(p));
                    s->bufferDeviceAddress = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT: {
                    VkPhysicalDeviceScalarBlockLayoutFeaturesEXT* s = static_cast<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT*>(static_cast<void*>(p));
                    s->scalarBlockLayout = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR: {
                    VkPhysicalDeviceFeatures2KHR* s = static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p));
                    s->features.fragmentStoresAndAtomics = VK_TRUE;
                    s->features.shaderInt64 = VK_TRUE;
                } break;
                default: break;
            }
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT: {
                    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT* prettify_VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT = static_cast<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT->shaderImageInt64Atomics == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT->shaderImageInt64Atomics == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT::shaderImageInt64Atomics == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT: {
                    VkPhysicalDeviceDescriptorIndexingFeaturesEXT* prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT = static_cast<VkPhysicalDeviceDescriptorIndexingFeaturesEXT*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingPartiallyBound == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingPartiallyBound == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceDescriptorIndexingFeaturesEXT::descriptorBindingPartiallyBound == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingUpdateUnusedWhilePending == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingUpdateUnusedWhilePending == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceDescriptorIndexingFeaturesEXT::descriptorBindingUpdateUnusedWhilePending == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingVariableDescriptorCount == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingVariableDescriptorCount == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceDescriptorIndexingFeaturesEXT::descriptorBindingVariableDescriptorCount == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->runtimeDescriptorArray == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->runtimeDescriptorArray == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceDescriptorIndexingFeaturesEXT::runtimeDescriptorArray == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES: {
                    VkPhysicalDeviceShaderAtomicInt64Features* prettify_VkPhysicalDeviceShaderAtomicInt64Features = static_cast<VkPhysicalDeviceShaderAtomicInt64Features*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceShaderAtomicInt64Features->shaderBufferInt64Atomics == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceShaderAtomicInt64Features->shaderBufferInt64Atomics == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceShaderAtomicInt64Features::shaderBufferInt64Atomics == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES: {
                    VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures* prettify_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures = static_cast<VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures->separateDepthStencilLayouts == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures->separateDepthStencilLayouts == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures::separateDepthStencilLayouts == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES: {
                    VkPhysicalDeviceSynchronization2Features* prettify_VkPhysicalDeviceSynchronization2Features = static_cast<VkPhysicalDeviceSynchronization2Features*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceSynchronization2Features->synchronization2 == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceSynchronization2Features->synchronization2 == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceSynchronization2Features::synchronization2 == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES: {
                    VkPhysicalDeviceMaintenance4Features* prettify_VkPhysicalDeviceMaintenance4Features = static_cast<VkPhysicalDeviceMaintenance4Features*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceMaintenance4Features->maintenance4 == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceMaintenance4Features->maintenance4 == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceMaintenance4Features::maintenance4 == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES: {
                    VkPhysicalDeviceBufferDeviceAddressFeatures* prettify_VkPhysicalDeviceBufferDeviceAddressFeatures = static_cast<VkPhysicalDeviceBufferDeviceAddressFeatures*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceBufferDeviceAddressFeatures->bufferDeviceAddress == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceBufferDeviceAddressFeatures->bufferDeviceAddress == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT: {
                    VkPhysicalDeviceScalarBlockLayoutFeaturesEXT* prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT = static_cast<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT->scalarBlockLayout == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT->scalarBlockLayout == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceScalarBlockLayoutFeaturesEXT::scalarBlockLayout == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR: {
                    VkPhysicalDeviceFeatures2KHR* prettify_VkPhysicalDeviceFeatures2KHR = static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceFeatures2KHR->features.fragmentStoresAndAtomics == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceFeatures2KHR->features.fragmentStoresAndAtomics == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceFeatures2KHR::features.fragmentStoresAndAtomics == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceFeatures2KHR->features.shaderInt64 == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceFeatures2KHR->features.shaderInt64 == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceFeatures2KHR::features.shaderInt64 == VK_TRUE");
                } break;
                default: break;
            }
        return ret;
    }
};

static const VpPropertyDesc propertyDesc = {
    [](VkBaseOutStructure* p) {
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
        return ret;
    }
};

static const VpFormatDesc formatDesc[] = {
    {
        VK_FORMAT_R64_UINT,
        [](VkBaseOutStructure* p) {
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR: {
                    VkFormatProperties2KHR* s = static_cast<VkFormatProperties2KHR*>(static_cast<void*>(p));
                    s->formatProperties.optimalTilingFeatures = (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT);
                } break;
                default: break;
            }
        },
        [](VkBaseOutStructure* p) -> bool {
            bool ret = true;
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR: {
                    VkFormatProperties2KHR* prettify_VkFormatProperties2KHR = static_cast<VkFormatProperties2KHR*>(static_cast<void*>(p));
                    ret = ret && (vpCheckFlags(prettify_VkFormatProperties2KHR->formatProperties.optimalTilingFeatures, (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT))); VP_DEBUG_COND_MSG(!(vpCheckFlags(prettify_VkFormatProperties2KHR->formatProperties.optimalTilingFeatures, (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT))), "Unsupported format condition for VK_FORMAT_R64_UINT: VkFormatProperties2KHR::formatProperties.optimalTilingFeatures contains (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT)");
                } break;
                default: break;
            }
            return ret;
        }
    },
};

static const VpStructChainerDesc chainerDesc = {
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT physicalDeviceShaderImageAtomicInt64FeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT, nullptr };
        VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, &physicalDeviceShaderImageAtomicInt64FeaturesEXT };
        VkPhysicalDeviceShaderAtomicInt64Features physicalDeviceShaderAtomicInt64Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES, &physicalDeviceDescriptorIndexingFeaturesEXT };
        VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures physicalDeviceSeparateDepthStencilLayoutsFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES, &physicalDeviceShaderAtomicInt64Features };
        VkPhysicalDeviceSynchronization2Features physicalDeviceSynchronization2Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES, &physicalDeviceSeparateDepthStencilLayoutsFeatures };
        VkPhysicalDeviceMaintenance4Features physicalDeviceMaintenance4Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES, &physicalDeviceSynchronization2Features };
        VkPhysicalDeviceBufferDeviceAddressFeatures physicalDeviceBufferDeviceAddressFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, &physicalDeviceMaintenance4Features };
        VkPhysicalDeviceScalarBlockLayoutFeaturesEXT physicalDeviceScalarBlockLayoutFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT, &physicalDeviceBufferDeviceAddressFeatures };
        p->pNext = static_cast<VkBaseOutStructure*>(static_cast<void*>(&physicalDeviceScalarBlockLayoutFeaturesEXT));
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        VkFormatProperties3KHR formatProperties3KHR{ VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR, nullptr };
        p->pNext = static_cast<VkBaseOutStructure*>(static_cast<void*>(&formatProperties3KHR));
        pfnCb(p, pUser);
    },
};

} // namespace VP_UE_VULKAN_SM6
#endif

#ifdef VP_UE_Vulkan_SM6_RT
namespace VP_UE_VULKAN_SM6_RT {

static const VkExtensionProperties deviceExtensions[] = {
    VkExtensionProperties{ VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_EXT_SHADER_IMAGE_ATOMIC_INT64_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_RAY_QUERY_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME, 1 },
    VkExtensionProperties{ VK_KHR_SPIRV_1_4_EXTENSION_NAME, 1 },
};

static const VkStructureType featureStructTypes[] = {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
};

static const VkStructureType propertyStructTypes[] = {
    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR,
};

static const VkStructureType formatStructTypes[] = {
    VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR,
    VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR,
};

static const VpFeatureDesc featureDesc = {
    [](VkBaseOutStructure* p) {
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT: {
                    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT* s = static_cast<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT*>(static_cast<void*>(p));
                    s->shaderImageInt64Atomics = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT: {
                    VkPhysicalDeviceDescriptorIndexingFeaturesEXT* s = static_cast<VkPhysicalDeviceDescriptorIndexingFeaturesEXT*>(static_cast<void*>(p));
                    s->descriptorBindingPartiallyBound = VK_TRUE;
                    s->descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
                    s->descriptorBindingVariableDescriptorCount = VK_TRUE;
                    s->runtimeDescriptorArray = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES: {
                    VkPhysicalDeviceShaderAtomicInt64Features* s = static_cast<VkPhysicalDeviceShaderAtomicInt64Features*>(static_cast<void*>(p));
                    s->shaderBufferInt64Atomics = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES: {
                    VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures* s = static_cast<VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures*>(static_cast<void*>(p));
                    s->separateDepthStencilLayouts = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES: {
                    VkPhysicalDeviceSynchronization2Features* s = static_cast<VkPhysicalDeviceSynchronization2Features*>(static_cast<void*>(p));
                    s->synchronization2 = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES: {
                    VkPhysicalDeviceMaintenance4Features* s = static_cast<VkPhysicalDeviceMaintenance4Features*>(static_cast<void*>(p));
                    s->maintenance4 = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES: {
                    VkPhysicalDeviceBufferDeviceAddressFeatures* s = static_cast<VkPhysicalDeviceBufferDeviceAddressFeatures*>(static_cast<void*>(p));
                    s->bufferDeviceAddress = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT: {
                    VkPhysicalDeviceScalarBlockLayoutFeaturesEXT* s = static_cast<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT*>(static_cast<void*>(p));
                    s->scalarBlockLayout = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR: {
                    VkPhysicalDeviceFeatures2KHR* s = static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p));
                    s->features.fragmentStoresAndAtomics = VK_TRUE;
                    s->features.shaderInt64 = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR: {
                    VkPhysicalDeviceRayQueryFeaturesKHR* s = static_cast<VkPhysicalDeviceRayQueryFeaturesKHR*>(static_cast<void*>(p));
                    s->rayQuery = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR: {
                    VkPhysicalDeviceRayTracingPipelineFeaturesKHR* s = static_cast<VkPhysicalDeviceRayTracingPipelineFeaturesKHR*>(static_cast<void*>(p));
                    s->rayTracingPipeline = VK_TRUE;
                    s->rayTraversalPrimitiveCulling = VK_TRUE;
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR: {
                    VkPhysicalDeviceAccelerationStructureFeaturesKHR* s = static_cast<VkPhysicalDeviceAccelerationStructureFeaturesKHR*>(static_cast<void*>(p));
                    s->accelerationStructure = VK_TRUE;
                    s->descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;
                } break;
                default: break;
            }
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT: {
                    VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT* prettify_VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT = static_cast<VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT->shaderImageInt64Atomics == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT->shaderImageInt64Atomics == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT::shaderImageInt64Atomics == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT: {
                    VkPhysicalDeviceDescriptorIndexingFeaturesEXT* prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT = static_cast<VkPhysicalDeviceDescriptorIndexingFeaturesEXT*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingPartiallyBound == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingPartiallyBound == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceDescriptorIndexingFeaturesEXT::descriptorBindingPartiallyBound == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingUpdateUnusedWhilePending == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingUpdateUnusedWhilePending == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceDescriptorIndexingFeaturesEXT::descriptorBindingUpdateUnusedWhilePending == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingVariableDescriptorCount == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->descriptorBindingVariableDescriptorCount == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceDescriptorIndexingFeaturesEXT::descriptorBindingVariableDescriptorCount == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->runtimeDescriptorArray == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceDescriptorIndexingFeaturesEXT->runtimeDescriptorArray == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceDescriptorIndexingFeaturesEXT::runtimeDescriptorArray == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES: {
                    VkPhysicalDeviceShaderAtomicInt64Features* prettify_VkPhysicalDeviceShaderAtomicInt64Features = static_cast<VkPhysicalDeviceShaderAtomicInt64Features*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceShaderAtomicInt64Features->shaderBufferInt64Atomics == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceShaderAtomicInt64Features->shaderBufferInt64Atomics == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceShaderAtomicInt64Features::shaderBufferInt64Atomics == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES: {
                    VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures* prettify_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures = static_cast<VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures->separateDepthStencilLayouts == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures->separateDepthStencilLayouts == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures::separateDepthStencilLayouts == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES: {
                    VkPhysicalDeviceSynchronization2Features* prettify_VkPhysicalDeviceSynchronization2Features = static_cast<VkPhysicalDeviceSynchronization2Features*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceSynchronization2Features->synchronization2 == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceSynchronization2Features->synchronization2 == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceSynchronization2Features::synchronization2 == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES: {
                    VkPhysicalDeviceMaintenance4Features* prettify_VkPhysicalDeviceMaintenance4Features = static_cast<VkPhysicalDeviceMaintenance4Features*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceMaintenance4Features->maintenance4 == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceMaintenance4Features->maintenance4 == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceMaintenance4Features::maintenance4 == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES: {
                    VkPhysicalDeviceBufferDeviceAddressFeatures* prettify_VkPhysicalDeviceBufferDeviceAddressFeatures = static_cast<VkPhysicalDeviceBufferDeviceAddressFeatures*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceBufferDeviceAddressFeatures->bufferDeviceAddress == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceBufferDeviceAddressFeatures->bufferDeviceAddress == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceBufferDeviceAddressFeatures::bufferDeviceAddress == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT: {
                    VkPhysicalDeviceScalarBlockLayoutFeaturesEXT* prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT = static_cast<VkPhysicalDeviceScalarBlockLayoutFeaturesEXT*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT->scalarBlockLayout == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceScalarBlockLayoutFeaturesEXT->scalarBlockLayout == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceScalarBlockLayoutFeaturesEXT::scalarBlockLayout == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR: {
                    VkPhysicalDeviceFeatures2KHR* prettify_VkPhysicalDeviceFeatures2KHR = static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceFeatures2KHR->features.fragmentStoresAndAtomics == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceFeatures2KHR->features.fragmentStoresAndAtomics == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceFeatures2KHR::features.fragmentStoresAndAtomics == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceFeatures2KHR->features.shaderInt64 == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceFeatures2KHR->features.shaderInt64 == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceFeatures2KHR::features.shaderInt64 == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR: {
                    VkPhysicalDeviceRayQueryFeaturesKHR* prettify_VkPhysicalDeviceRayQueryFeaturesKHR = static_cast<VkPhysicalDeviceRayQueryFeaturesKHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceRayQueryFeaturesKHR->rayQuery == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceRayQueryFeaturesKHR->rayQuery == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceRayQueryFeaturesKHR::rayQuery == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR: {
                    VkPhysicalDeviceRayTracingPipelineFeaturesKHR* prettify_VkPhysicalDeviceRayTracingPipelineFeaturesKHR = static_cast<VkPhysicalDeviceRayTracingPipelineFeaturesKHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceRayTracingPipelineFeaturesKHR->rayTracingPipeline == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceRayTracingPipelineFeaturesKHR->rayTracingPipeline == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTracingPipeline == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceRayTracingPipelineFeaturesKHR->rayTraversalPrimitiveCulling == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceRayTracingPipelineFeaturesKHR->rayTraversalPrimitiveCulling == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceRayTracingPipelineFeaturesKHR::rayTraversalPrimitiveCulling == VK_TRUE");
                } break;
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR: {
                    VkPhysicalDeviceAccelerationStructureFeaturesKHR* prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR = static_cast<VkPhysicalDeviceAccelerationStructureFeaturesKHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructure == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->accelerationStructure == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceAccelerationStructureFeaturesKHR::accelerationStructure == VK_TRUE");
                    ret = ret && (prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->descriptorBindingAccelerationStructureUpdateAfterBind == VK_TRUE); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceAccelerationStructureFeaturesKHR->descriptorBindingAccelerationStructureUpdateAfterBind == VK_TRUE), "Unsupported feature condition: VkPhysicalDeviceAccelerationStructureFeaturesKHR::descriptorBindingAccelerationStructureUpdateAfterBind == VK_TRUE");
                } break;
                default: break;
            }
        return ret;
    }
};

static const VpPropertyDesc propertyDesc = {
    [](VkBaseOutStructure* p) {
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR: {
                    VkPhysicalDeviceProperties2KHR* s = static_cast<VkPhysicalDeviceProperties2KHR*>(static_cast<void*>(p));
                    s->properties.limits.maxBoundDescriptorSets = 8;
                } break;
                default: break;
            }
    },
    [](VkBaseOutStructure* p) -> bool {
        bool ret = true;
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR: {
                    VkPhysicalDeviceProperties2KHR* prettify_VkPhysicalDeviceProperties2KHR = static_cast<VkPhysicalDeviceProperties2KHR*>(static_cast<void*>(p));
                    ret = ret && (prettify_VkPhysicalDeviceProperties2KHR->properties.limits.maxBoundDescriptorSets >= 8); VP_DEBUG_COND_MSG(!(prettify_VkPhysicalDeviceProperties2KHR->properties.limits.maxBoundDescriptorSets >= 8), "Unsupported properties condition: VkPhysicalDeviceProperties2KHR::properties.limits.maxBoundDescriptorSets >= 8");
                } break;
                default: break;
            }
        return ret;
    }
};

static const VpFormatDesc formatDesc[] = {
    {
        VK_FORMAT_R64_UINT,
        [](VkBaseOutStructure* p) {
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR: {
                    VkFormatProperties2KHR* s = static_cast<VkFormatProperties2KHR*>(static_cast<void*>(p));
                    s->formatProperties.optimalTilingFeatures = (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT);
                } break;
                default: break;
            }
        },
        [](VkBaseOutStructure* p) -> bool {
            bool ret = true;
            switch (p->sType) {
                case VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR: {
                    VkFormatProperties2KHR* prettify_VkFormatProperties2KHR = static_cast<VkFormatProperties2KHR*>(static_cast<void*>(p));
                    ret = ret && (vpCheckFlags(prettify_VkFormatProperties2KHR->formatProperties.optimalTilingFeatures, (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT))); VP_DEBUG_COND_MSG(!(vpCheckFlags(prettify_VkFormatProperties2KHR->formatProperties.optimalTilingFeatures, (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT))), "Unsupported format condition for VK_FORMAT_R64_UINT: VkFormatProperties2KHR::formatProperties.optimalTilingFeatures contains (VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT)");
                } break;
                default: break;
            }
            return ret;
        }
    },
};

static const VpStructChainerDesc chainerDesc = {
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        VkPhysicalDeviceShaderImageAtomicInt64FeaturesEXT physicalDeviceShaderImageAtomicInt64FeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_IMAGE_ATOMIC_INT64_FEATURES_EXT, nullptr };
        VkPhysicalDeviceDescriptorIndexingFeaturesEXT physicalDeviceDescriptorIndexingFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES_EXT, &physicalDeviceShaderImageAtomicInt64FeaturesEXT };
        VkPhysicalDeviceShaderAtomicInt64Features physicalDeviceShaderAtomicInt64Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_ATOMIC_INT64_FEATURES, &physicalDeviceDescriptorIndexingFeaturesEXT };
        VkPhysicalDeviceSeparateDepthStencilLayoutsFeatures physicalDeviceSeparateDepthStencilLayoutsFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SEPARATE_DEPTH_STENCIL_LAYOUTS_FEATURES, &physicalDeviceShaderAtomicInt64Features };
        VkPhysicalDeviceSynchronization2Features physicalDeviceSynchronization2Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES, &physicalDeviceSeparateDepthStencilLayoutsFeatures };
        VkPhysicalDeviceMaintenance4Features physicalDeviceMaintenance4Features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES, &physicalDeviceSynchronization2Features };
        VkPhysicalDeviceBufferDeviceAddressFeatures physicalDeviceBufferDeviceAddressFeatures{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES, &physicalDeviceMaintenance4Features };
        VkPhysicalDeviceScalarBlockLayoutFeaturesEXT physicalDeviceScalarBlockLayoutFeaturesEXT{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT, &physicalDeviceBufferDeviceAddressFeatures };
        VkPhysicalDeviceRayQueryFeaturesKHR physicalDeviceRayQueryFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, &physicalDeviceScalarBlockLayoutFeaturesEXT };
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR physicalDeviceRayTracingPipelineFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, &physicalDeviceRayQueryFeaturesKHR };
        VkPhysicalDeviceAccelerationStructureFeaturesKHR physicalDeviceAccelerationStructureFeaturesKHR{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR, &physicalDeviceRayTracingPipelineFeaturesKHR };
        p->pNext = static_cast<VkBaseOutStructure*>(static_cast<void*>(&physicalDeviceAccelerationStructureFeaturesKHR));
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        p->pNext = static_cast<VkBaseOutStructure*>(static_cast<void*>(nullptr));
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        pfnCb(p, pUser);
    },
    [](VkBaseOutStructure* p, void* pUser, PFN_vpStructChainerCb pfnCb) {
        VkFormatProperties3KHR formatProperties3KHR{ VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR, nullptr };
        p->pNext = static_cast<VkBaseOutStructure*>(static_cast<void*>(&formatProperties3KHR));
        pfnCb(p, pUser);
    },
};

} // namespace VP_UE_VULKAN_SM6_RT
#endif

static const VpProfileDesc vpProfiles[] = {
#ifdef VP_UE_Vulkan_ES3_1_Android
    VpProfileDesc{
        VpProfileProperties{ VP_UE_VULKAN_ES3_1_ANDROID_NAME, VP_UE_VULKAN_ES3_1_ANDROID_SPEC_VERSION },
        VP_UE_VULKAN_ES3_1_ANDROID_MIN_API_VERSION,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        VP_UE_VULKAN_ES3_1_ANDROID::featureDesc,
        nullptr, 0,
        VP_UE_VULKAN_ES3_1_ANDROID::propertyDesc,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        VP_UE_VULKAN_ES3_1_ANDROID::chainerDesc,
    },
#endif
#ifdef VP_UE_Vulkan_SM5
    VpProfileDesc{
        VpProfileProperties{ VP_UE_VULKAN_SM5_NAME, VP_UE_VULKAN_SM5_SPEC_VERSION },
        VP_UE_VULKAN_SM5_MIN_API_VERSION,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        &VP_UE_VULKAN_SM5::featureStructTypes[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM5::featureStructTypes) / sizeof(VP_UE_VULKAN_SM5::featureStructTypes[0])),
        VP_UE_VULKAN_SM5::featureDesc,
        &VP_UE_VULKAN_SM5::propertyStructTypes[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM5::propertyStructTypes) / sizeof(VP_UE_VULKAN_SM5::propertyStructTypes[0])),
        VP_UE_VULKAN_SM5::propertyDesc,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        VP_UE_VULKAN_SM5::chainerDesc,
    },
#endif
#ifdef VP_UE_Vulkan_SM5_Android
    VpProfileDesc{
        VpProfileProperties{ VP_UE_VULKAN_SM5_ANDROID_NAME, VP_UE_VULKAN_SM5_ANDROID_SPEC_VERSION },
        VP_UE_VULKAN_SM5_ANDROID_MIN_API_VERSION,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        VP_UE_VULKAN_SM5_ANDROID::featureDesc,
        nullptr, 0,
        VP_UE_VULKAN_SM5_ANDROID::propertyDesc,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        VP_UE_VULKAN_SM5_ANDROID::chainerDesc,
    },
#endif
#ifdef VP_UE_Vulkan_SM5_Android_RT
    VpProfileDesc{
        VpProfileProperties{ VP_UE_VULKAN_SM5_ANDROID_RT_NAME, VP_UE_VULKAN_SM5_ANDROID_RT_SPEC_VERSION },
        VP_UE_VULKAN_SM5_ANDROID_RT_MIN_API_VERSION,
        nullptr, 0,
        &VP_UE_VULKAN_SM5_ANDROID_RT::deviceExtensions[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM5_ANDROID_RT::deviceExtensions) / sizeof(VP_UE_VULKAN_SM5_ANDROID_RT::deviceExtensions[0])),
        nullptr, 0,
        &VP_UE_VULKAN_SM5_ANDROID_RT::featureStructTypes[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM5_ANDROID_RT::featureStructTypes) / sizeof(VP_UE_VULKAN_SM5_ANDROID_RT::featureStructTypes[0])),
        VP_UE_VULKAN_SM5_ANDROID_RT::featureDesc,
        &VP_UE_VULKAN_SM5_ANDROID_RT::propertyStructTypes[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM5_ANDROID_RT::propertyStructTypes) / sizeof(VP_UE_VULKAN_SM5_ANDROID_RT::propertyStructTypes[0])),
        VP_UE_VULKAN_SM5_ANDROID_RT::propertyDesc,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        VP_UE_VULKAN_SM5_ANDROID_RT::chainerDesc,
    },
#endif
#ifdef VP_UE_Vulkan_SM5_RT
    VpProfileDesc{
        VpProfileProperties{ VP_UE_VULKAN_SM5_RT_NAME, VP_UE_VULKAN_SM5_RT_SPEC_VERSION },
        VP_UE_VULKAN_SM5_RT_MIN_API_VERSION,
        nullptr, 0,
        &VP_UE_VULKAN_SM5_RT::deviceExtensions[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM5_RT::deviceExtensions) / sizeof(VP_UE_VULKAN_SM5_RT::deviceExtensions[0])),
        nullptr, 0,
        &VP_UE_VULKAN_SM5_RT::featureStructTypes[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM5_RT::featureStructTypes) / sizeof(VP_UE_VULKAN_SM5_RT::featureStructTypes[0])),
        VP_UE_VULKAN_SM5_RT::featureDesc,
        &VP_UE_VULKAN_SM5_RT::propertyStructTypes[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM5_RT::propertyStructTypes) / sizeof(VP_UE_VULKAN_SM5_RT::propertyStructTypes[0])),
        VP_UE_VULKAN_SM5_RT::propertyDesc,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        nullptr, 0,
        VP_UE_VULKAN_SM5_RT::chainerDesc,
    },
#endif
#ifdef VP_UE_Vulkan_SM6
    VpProfileDesc{
        VpProfileProperties{ VP_UE_VULKAN_SM6_NAME, VP_UE_VULKAN_SM6_SPEC_VERSION },
        VP_UE_VULKAN_SM6_MIN_API_VERSION,
        nullptr, 0,
        &VP_UE_VULKAN_SM6::deviceExtensions[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM6::deviceExtensions) / sizeof(VP_UE_VULKAN_SM6::deviceExtensions[0])),
        nullptr, 0,
        &VP_UE_VULKAN_SM6::featureStructTypes[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM6::featureStructTypes) / sizeof(VP_UE_VULKAN_SM6::featureStructTypes[0])),
        VP_UE_VULKAN_SM6::featureDesc,
        nullptr, 0,
        VP_UE_VULKAN_SM6::propertyDesc,
        nullptr, 0,
        nullptr, 0,
        &VP_UE_VULKAN_SM6::formatStructTypes[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM6::formatStructTypes) / sizeof(VP_UE_VULKAN_SM6::formatStructTypes[0])),
        &VP_UE_VULKAN_SM6::formatDesc[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM6::formatDesc) / sizeof(VP_UE_VULKAN_SM6::formatDesc[0])),
        VP_UE_VULKAN_SM6::chainerDesc,
    },
#endif
#ifdef VP_UE_Vulkan_SM6_RT
    VpProfileDesc{
        VpProfileProperties{ VP_UE_VULKAN_SM6_RT_NAME, VP_UE_VULKAN_SM6_RT_SPEC_VERSION },
        VP_UE_VULKAN_SM6_RT_MIN_API_VERSION,
        nullptr, 0,
        &VP_UE_VULKAN_SM6_RT::deviceExtensions[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM6_RT::deviceExtensions) / sizeof(VP_UE_VULKAN_SM6_RT::deviceExtensions[0])),
        nullptr, 0,
        &VP_UE_VULKAN_SM6_RT::featureStructTypes[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM6_RT::featureStructTypes) / sizeof(VP_UE_VULKAN_SM6_RT::featureStructTypes[0])),
        VP_UE_VULKAN_SM6_RT::featureDesc,
        &VP_UE_VULKAN_SM6_RT::propertyStructTypes[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM6_RT::propertyStructTypes) / sizeof(VP_UE_VULKAN_SM6_RT::propertyStructTypes[0])),
        VP_UE_VULKAN_SM6_RT::propertyDesc,
        nullptr, 0,
        nullptr, 0,
        &VP_UE_VULKAN_SM6_RT::formatStructTypes[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM6_RT::formatStructTypes) / sizeof(VP_UE_VULKAN_SM6_RT::formatStructTypes[0])),
        &VP_UE_VULKAN_SM6_RT::formatDesc[0], static_cast<uint32_t>(sizeof(VP_UE_VULKAN_SM6_RT::formatDesc) / sizeof(VP_UE_VULKAN_SM6_RT::formatDesc[0])),
        VP_UE_VULKAN_SM6_RT::chainerDesc,
    },
#endif
};
static const uint32_t vpProfileCount = static_cast<uint32_t>(sizeof(vpProfiles) / sizeof(vpProfiles[0]));

VPAPI_ATTR const VpProfileDesc* vpGetProfileDesc(const char profileName[VP_MAX_PROFILE_NAME_SIZE]) {
    for (uint32_t i = 0; i < vpProfileCount; ++i) {
        if (strncmp(vpProfiles[i].props.profileName, profileName, VP_MAX_PROFILE_NAME_SIZE) == 0) return &vpProfiles[i];
    }
    return nullptr;
}

VPAPI_ATTR bool vpCheckVersion(uint32_t actual, uint32_t expected) {
    uint32_t actualMajor = VK_API_VERSION_MAJOR(actual);
    uint32_t actualMinor = VK_API_VERSION_MINOR(actual);
    uint32_t expectedMajor = VK_API_VERSION_MAJOR(expected);
    uint32_t expectedMinor = VK_API_VERSION_MINOR(expected);
    return actualMajor > expectedMajor || (actualMajor == expectedMajor && actualMinor >= expectedMinor);
}

VPAPI_ATTR bool vpCheckExtension(const VkExtensionProperties *supportedProperties, size_t supportedSize,
                                 const char *requestedExtension) {
    bool found = false;
    for (size_t i = 0, n = supportedSize; i < n; ++i) {
        if (strcmp(supportedProperties[i].extensionName, requestedExtension) == 0) {
            found = true;
            // Drivers don't actually update their spec version, so we cannot rely on this
            // if (supportedProperties[i].specVersion >= expectedVersion) found = true;
        }
    }
    VP_DEBUG_COND_MSGF(!found, "Unsupported extension: %s", requestedExtension);
    return found;
}

VPAPI_ATTR void vpGetExtensions(uint32_t requestedExtensionCount, const char *const *ppRequestedExtensionNames,
                                uint32_t profileExtensionCount, const VkExtensionProperties *pProfileExtensionProperties,
                                std::vector<const char *> &extensions, bool merge, bool override) {
    if (override) {
        for (uint32_t i = 0; i < requestedExtensionCount; ++i) {
            extensions.push_back(ppRequestedExtensionNames[i]);
        }
    } else {
        for (uint32_t i = 0; i < profileExtensionCount; ++i) {
            extensions.push_back(pProfileExtensionProperties[i].extensionName);
        }

        if (merge) {
            for (uint32_t i = 0; i < requestedExtensionCount; ++i) {
                if (vpCheckExtension(pProfileExtensionProperties, profileExtensionCount, ppRequestedExtensionNames[i])) {
                    continue;
                }
                extensions.push_back(ppRequestedExtensionNames[i]);
            }
        }
    }
}

VPAPI_ATTR const void* vpGetStructure(const void* pNext, VkStructureType type) {
    const VkBaseOutStructure *p = static_cast<const VkBaseOutStructure*>(pNext);
    while (p != nullptr) {
        if (p->sType == type) return p;
        p = p->pNext;
    }
    return nullptr;
}

VPAPI_ATTR void* vpGetStructure(void* pNext, VkStructureType type) {
    VkBaseOutStructure *p = static_cast<VkBaseOutStructure*>(pNext);
    while (p != nullptr) {
        if (p->sType == type) return p;
        p = p->pNext;
    }
    return nullptr;
}

} // namespace detail

VPAPI_ATTR VkResult vpGetProfiles(uint32_t *pPropertyCount, VpProfileProperties *pProperties) {
    VkResult result = VK_SUCCESS;

    if (pProperties == nullptr) {
        *pPropertyCount = detail::vpProfileCount;
    } else {
        if (*pPropertyCount < detail::vpProfileCount) {
            result = VK_INCOMPLETE;
        } else {
            *pPropertyCount = detail::vpProfileCount;
        }
        for (uint32_t i = 0; i < *pPropertyCount; ++i) {
            pProperties[i] = detail::vpProfiles[i].props;
        }
    }
    return result;
}

VPAPI_ATTR VkResult vpGetProfileFallbacks(const VpProfileProperties *pProfile, uint32_t *pPropertyCount, VpProfileProperties *pProperties) {
    VkResult result = VK_SUCCESS;

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    if (pProperties == nullptr) {
        *pPropertyCount = pDesc->fallbackCount;
    } else {
        if (*pPropertyCount < pDesc->fallbackCount) {
            result = VK_INCOMPLETE;
        } else {
            *pPropertyCount = pDesc->fallbackCount;
        }
        for (uint32_t i = 0; i < *pPropertyCount; ++i) {
            pProperties[i] = pDesc->pFallbacks[i];
        }
    }
    return result;
}

VPAPI_ATTR VkResult vpGetInstanceProfileSupport(const char *pLayerName, const VpProfileProperties *pProfile, VkBool32 *pSupported) {
    VkResult result = VK_SUCCESS;

    uint32_t apiVersion = VK_MAKE_VERSION(1, 0, 0);
    static PFN_vkEnumerateInstanceVersion pfnEnumerateInstanceVersion =
        (PFN_vkEnumerateInstanceVersion)VulkanRHI::vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion");
    if (pfnEnumerateInstanceVersion != nullptr) {
        result = pfnEnumerateInstanceVersion(&apiVersion);
        if (result != VK_SUCCESS) {
            return result;
        }
    }

    uint32_t extCount = 0;
    result = VulkanRHI::vkEnumerateInstanceExtensionProperties(pLayerName, &extCount, nullptr);
    if (result != VK_SUCCESS) {
        return result;
    }
    std::vector<VkExtensionProperties> ext(extCount);
    result = VulkanRHI::vkEnumerateInstanceExtensionProperties(pLayerName, &extCount, ext.data());
    if (result != VK_SUCCESS) {
        return result;
    }

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    *pSupported = VK_TRUE;

    if (pDesc->props.specVersion < pProfile->specVersion) {
        VP_DEBUG_MSGF("Unsupported profile version: %u", pProfile->specVersion);
        *pSupported = VK_FALSE;
    }

    if (!detail::vpCheckVersion(apiVersion, pDesc->minApiVersion)) {
        VP_DEBUG_MSGF("Unsupported API version: %u.%u.%u", VK_API_VERSION_MAJOR(pDesc->minApiVersion), VK_API_VERSION_MINOR(pDesc->minApiVersion), VK_API_VERSION_PATCH(pDesc->minApiVersion));
        *pSupported = VK_FALSE;
    }

    for (uint32_t i = 0; i < pDesc->instanceExtensionCount; ++i) {
        if (!detail::vpCheckExtension(ext.data(), ext.size(),
            pDesc->pInstanceExtensions[i].extensionName)) {
            *pSupported = VK_FALSE;
        }
    }

    // We require VK_KHR_get_physical_device_properties2 if we are on Vulkan 1.0
    if (apiVersion < VK_API_VERSION_1_1) {
        bool foundGPDP2 = false;
        for (size_t i = 0; i < ext.size(); ++i) {
            if (strcmp(ext[i].extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) {
                foundGPDP2 = true;
                break;
            }
        }
        if (!foundGPDP2) {
            VP_DEBUG_MSG("Unsupported mandatory extension VK_KHR_get_physical_device_properties2 on Vulkan 1.0");
            *pSupported = VK_FALSE;
        }
    }

    return result;
}

VPAPI_ATTR VkResult vpCreateInstance(const VpInstanceCreateInfo *pCreateInfo,
                                     const VkAllocationCallbacks *pAllocator, VkInstance *pInstance) {
    VkInstanceCreateInfo createInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    std::vector<const char*> extensions;
    VkInstanceCreateInfo* pInstanceCreateInfo = nullptr;

    if (pCreateInfo != nullptr && pCreateInfo->pCreateInfo != nullptr) {
        createInfo = *pCreateInfo->pCreateInfo;
        pInstanceCreateInfo = &createInfo;

        const detail::VpProfileDesc* pDesc = nullptr;
        if (pCreateInfo->pProfile != nullptr) {
            pDesc = detail::vpGetProfileDesc(pCreateInfo->pProfile->profileName);
            if (pDesc == nullptr) return VK_ERROR_UNKNOWN;
        }

        if (createInfo.pApplicationInfo == nullptr) {
            appInfo.apiVersion = pDesc->minApiVersion;
            createInfo.pApplicationInfo = &appInfo;
        }

        if (pDesc != nullptr && pDesc->pInstanceExtensions != nullptr) {
            bool merge = (pCreateInfo->flags & VP_INSTANCE_CREATE_MERGE_EXTENSIONS_BIT) != 0;
            bool override = (pCreateInfo->flags & VP_INSTANCE_CREATE_OVERRIDE_EXTENSIONS_BIT) != 0;

            if (!merge && !override && pCreateInfo->pCreateInfo->enabledExtensionCount > 0) {
                // If neither merge nor override is used then the application must not specify its
                // own extensions
                return VK_ERROR_UNKNOWN;
            }

            detail::vpGetExtensions(pCreateInfo->pCreateInfo->enabledExtensionCount,
                                    pCreateInfo->pCreateInfo->ppEnabledExtensionNames,
                                    pDesc->instanceExtensionCount,
                                    pDesc->pInstanceExtensions,
                                    extensions, merge, override);
            {
                bool foundPortEnum = false;
                for (size_t i = 0; i < extensions.size(); ++i) {
                    if (strcmp(extensions[i], VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) {
                        foundPortEnum = true;
                        break;
                    }
                }
                if (foundPortEnum) {
                    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
                }
            }

            // Need to include VK_KHR_get_physical_device_properties2 if we are on Vulkan 1.0
            if (createInfo.pApplicationInfo->apiVersion < VK_API_VERSION_1_1) {
                bool foundGPDP2 = false;
                for (size_t i = 0; i < extensions.size(); ++i) {
                    if (strcmp(extensions[i], VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) == 0) {
                        foundGPDP2 = true;
                        break;
                    }
                }
                if (!foundGPDP2) {
                    extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
                }
            }

            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();
        }
    }

    return VulkanRHI::vkCreateInstance(pInstanceCreateInfo, pAllocator, pInstance);
}

VPAPI_ATTR VkResult vpGetPhysicalDeviceProfileSupport(VkInstance instance, VkPhysicalDevice physicalDevice,
                                                      const VpProfileProperties *pProfile, VkBool32 *pSupported) {
    VkResult result = VK_SUCCESS;

    uint32_t extCount = 0;
    result = VulkanRHI::vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
    if (result != VK_SUCCESS) {
        return result;
    }
    std::vector<VkExtensionProperties> ext;
    if (extCount > 0) {
        ext.resize(extCount);
    }
    result = VulkanRHI::vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, ext.data());
    if (result != VK_SUCCESS) {
        return result;
    }

    // Workaround old loader bug where count could be smaller on the second call to vkEnumerateDeviceExtensionProperties
    if (extCount > 0) {
        ext.resize(extCount);
    }

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    struct GPDP2EntryPoints {
        PFN_vkGetPhysicalDeviceFeatures2KHR                 pfnGetPhysicalDeviceFeatures2;
        PFN_vkGetPhysicalDeviceProperties2KHR               pfnGetPhysicalDeviceProperties2;
        PFN_vkGetPhysicalDeviceFormatProperties2KHR         pfnGetPhysicalDeviceFormatProperties2;
        PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR    pfnGetPhysicalDeviceQueueFamilyProperties2;
    };

    struct UserData {
        VkPhysicalDevice                    physicalDevice;
        const detail::VpProfileDesc*        pDesc;
        GPDP2EntryPoints                    gpdp2;
        uint32_t                            index;
        uint32_t                            count;
        detail::PFN_vpStructChainerCb       pfnCb;
        bool                                supported;
    } userData{ physicalDevice, pDesc };

    // Attempt to load core versions of the GPDP2 entry points
    userData.gpdp2.pfnGetPhysicalDeviceFeatures2 =
        (PFN_vkGetPhysicalDeviceFeatures2KHR)VulkanRHI::vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2");
    userData.gpdp2.pfnGetPhysicalDeviceProperties2 =
        (PFN_vkGetPhysicalDeviceProperties2KHR)VulkanRHI::vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2");
    userData.gpdp2.pfnGetPhysicalDeviceFormatProperties2 =
        (PFN_vkGetPhysicalDeviceFormatProperties2KHR)VulkanRHI::vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties2");
    userData.gpdp2.pfnGetPhysicalDeviceQueueFamilyProperties2 =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR)VulkanRHI::vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties2");

    // If not successful, try to load KHR variant
    if (userData.gpdp2.pfnGetPhysicalDeviceFeatures2 == nullptr) {
        userData.gpdp2.pfnGetPhysicalDeviceFeatures2 =
            (PFN_vkGetPhysicalDeviceFeatures2KHR)VulkanRHI::vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR");
        userData.gpdp2.pfnGetPhysicalDeviceProperties2 =
            (PFN_vkGetPhysicalDeviceProperties2KHR)VulkanRHI::vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR");
        userData.gpdp2.pfnGetPhysicalDeviceFormatProperties2 =
            (PFN_vkGetPhysicalDeviceFormatProperties2KHR)VulkanRHI::vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFormatProperties2KHR");
        userData.gpdp2.pfnGetPhysicalDeviceQueueFamilyProperties2 =
            (PFN_vkGetPhysicalDeviceQueueFamilyProperties2KHR)VulkanRHI::vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties2KHR");
    }

    if (userData.gpdp2.pfnGetPhysicalDeviceFeatures2 == nullptr ||
        userData.gpdp2.pfnGetPhysicalDeviceProperties2 == nullptr ||
        userData.gpdp2.pfnGetPhysicalDeviceFormatProperties2 == nullptr ||
        userData.gpdp2.pfnGetPhysicalDeviceQueueFamilyProperties2 == nullptr) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    *pSupported = VK_TRUE;
    VP_DEBUG_MSGF("Checking device support for profile %s (%s). You may find the details of the capabilities of this device on https://vulkan.gpuinfo.org/", pProfile->profileName, detail::vpGetDeviceAndDriverInfoString(physicalDevice, userData.gpdp2.pfnGetPhysicalDeviceProperties2).c_str());

    if (pDesc->props.specVersion < pProfile->specVersion) {
        VP_DEBUG_MSGF("Unsupported profile version: %u", pProfile->specVersion);
        *pSupported = VK_FALSE;
    }

    {
        VkPhysicalDeviceProperties props{};
        VulkanRHI::vkGetPhysicalDeviceProperties(physicalDevice, &props);
        if (!detail::vpCheckVersion(props.apiVersion, pDesc->minApiVersion)) {
            VP_DEBUG_MSGF("Unsupported API version: %u.%u.%u", VK_API_VERSION_MAJOR(pDesc->minApiVersion), VK_API_VERSION_MINOR(pDesc->minApiVersion), VK_API_VERSION_PATCH(pDesc->minApiVersion));
            *pSupported = VK_FALSE;
        }
    }

    for (uint32_t i = 0; i < pDesc->deviceExtensionCount; ++i) {
        if (!detail::vpCheckExtension(ext.data(), ext.size(),
            pDesc->pDeviceExtensions[i].extensionName)) {
            *pSupported = VK_FALSE;
        }
    }

    {
        VkPhysicalDeviceFeatures2KHR features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
        pDesc->chainers.pfnFeature(static_cast<VkBaseOutStructure*>(static_cast<void*>(&features)), &userData,
            [](VkBaseOutStructure* p, void* pUser) {
                UserData* pUserData = static_cast<UserData*>(pUser);
                pUserData->gpdp2.pfnGetPhysicalDeviceFeatures2(pUserData->physicalDevice,
                                                               static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p)));
                pUserData->supported = true;
                while (p != nullptr) {
                    if (!pUserData->pDesc->feature.pfnComparator(p)) {
                        pUserData->supported = false;
                    }
                    p = p->pNext;
                }
            }
        );
        if (!userData.supported) {
            *pSupported = VK_FALSE;
        }
    }

    {
        VkPhysicalDeviceProperties2KHR props{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR };
        pDesc->chainers.pfnProperty(static_cast<VkBaseOutStructure*>(static_cast<void*>(&props)), &userData,
            [](VkBaseOutStructure* p, void* pUser) {
                UserData* pUserData = static_cast<UserData*>(pUser);
                pUserData->gpdp2.pfnGetPhysicalDeviceProperties2(pUserData->physicalDevice,
                                                                 static_cast<VkPhysicalDeviceProperties2KHR*>(static_cast<void*>(p)));
                pUserData->supported = true;
                while (p != nullptr) {
                    if (!pUserData->pDesc->property.pfnComparator(p)) {
                        pUserData->supported = false;
                    }
                    p = p->pNext;
                }
            }
        );
        if (!userData.supported) {
            *pSupported = VK_FALSE;
        }
    }

    for (uint32_t i = 0; i < pDesc->formatCount; ++i) {
        userData.index = i;
        VkFormatProperties2KHR props{ VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR };
        pDesc->chainers.pfnFormat(static_cast<VkBaseOutStructure*>(static_cast<void*>(&props)), &userData,
            [](VkBaseOutStructure* p, void* pUser) {
                UserData* pUserData = static_cast<UserData*>(pUser);
                pUserData->gpdp2.pfnGetPhysicalDeviceFormatProperties2(pUserData->physicalDevice,
                                                                       pUserData->pDesc->pFormats[pUserData->index].format,
                                                                       static_cast<VkFormatProperties2KHR*>(static_cast<void*>(p)));
                pUserData->supported = true;
                while (p != nullptr) {
                    if (!pUserData->pDesc->pFormats[pUserData->index].pfnComparator(p)) {
                        pUserData->supported = false;
                    }
                    p = p->pNext;
                }
            }
        );
        if (!userData.supported) {
            *pSupported = VK_FALSE;
        }
    }

    {
        userData.gpdp2.pfnGetPhysicalDeviceQueueFamilyProperties2(physicalDevice, &userData.count, nullptr);
        std::vector<VkQueueFamilyProperties2KHR> props(userData.count, { VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2_KHR });
        userData.index = 0;

        detail::PFN_vpStructChainerCb callback = [](VkBaseOutStructure* p, void* pUser) {
            UserData* pUserData = static_cast<UserData*>(pUser);
            VkQueueFamilyProperties2KHR* pProps = static_cast<VkQueueFamilyProperties2KHR*>(static_cast<void*>(p));
            if (++pUserData->index < pUserData->count) {
                pUserData->pDesc->chainers.pfnQueueFamily(static_cast<VkBaseOutStructure*>(static_cast<void*>(++pProps)),
                                                          pUser, pUserData->pfnCb);
            } else {
                pProps -= pUserData->count - 1;
                pUserData->gpdp2.pfnGetPhysicalDeviceQueueFamilyProperties2(pUserData->physicalDevice,
                                                                            &pUserData->count,
                                                                            pProps);
                pUserData->supported = true;

                // Check first that each queue family defined is supported by the device
                for (uint32_t i = 0; i < pUserData->pDesc->queueFamilyCount; ++i) {
                    bool found = false;
                    for (uint32_t j = 0; j < pUserData->count; ++j) {
                        bool propsMatch = true;
                        p = static_cast<VkBaseOutStructure*>(static_cast<void*>(&pProps[j]));
                        while (p != nullptr) {
                            if (!pUserData->pDesc->pQueueFamilies[i].pfnComparator(p)) {
                                propsMatch = false;
                                break;
                            }
                            p = p->pNext;
                        }
                        if (propsMatch) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        VP_DEBUG_MSGF("Unsupported queue family defined at profile data index #%u", i);
                        pUserData->supported = false;
                        return;
                    }
                }

                // Then check each permutation to ensure that while order of the queue families
                // doesn't matter, each queue family property criteria is matched with a separate
                // queue family of the actual device
                std::vector<uint32_t> permutation(pUserData->count);
                for (uint32_t i = 0; i < pUserData->count; ++i) {
                    permutation[i] = i;
                }
                bool found = false;
                do {
                    bool propsMatch = true;
                    for (uint32_t i = 0; i < pUserData->pDesc->queueFamilyCount && propsMatch; ++i) {
                        p = static_cast<VkBaseOutStructure*>(static_cast<void*>(&pProps[permutation[i]]));
                        while (p != nullptr) {
                            if (!pUserData->pDesc->pQueueFamilies[i].pfnComparator(p)) {
                                propsMatch = false;
                                break;
                            }
                            p = p->pNext;
                        }
                    }
                    if (propsMatch) {
                        found = true;
                        break;
                    }
                } while (std::next_permutation(permutation.begin(), permutation.end()));

                if (!found) {
                    VP_DEBUG_MSG("Unsupported combination of queue families");
                    pUserData->supported = false;
                }
            }
        };
        userData.pfnCb = callback;

        if (userData.count >= userData.pDesc->queueFamilyCount) {
            pDesc->chainers.pfnQueueFamily(static_cast<VkBaseOutStructure*>(static_cast<void*>(props.data())), &userData, callback);
            if (!userData.supported) {
                *pSupported = VK_FALSE;
            }
        } else {
            VP_DEBUG_MSGF("Unsupported number of queue families: device has fewer (%u) than what the profile defines (%u)", userData.count, userData.pDesc->queueFamilyCount);
            *pSupported = VK_FALSE;
        }
    }

    return result;
}

VPAPI_ATTR VkResult vpCreateDevice(VkPhysicalDevice physicalDevice, const VpDeviceCreateInfo *pCreateInfo,
                                   const VkAllocationCallbacks *pAllocator, VkDevice *pDevice) {
    if (physicalDevice == VK_NULL_HANDLE || pCreateInfo == nullptr || pDevice == nullptr) {
        return VulkanRHI::vkCreateDevice(physicalDevice, pCreateInfo == nullptr ? nullptr : pCreateInfo->pCreateInfo, pAllocator, pDevice);
    }

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pCreateInfo->pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    struct UserData {
        VkPhysicalDevice                physicalDevice;
        const detail::VpProfileDesc*    pDesc;
        const VpDeviceCreateInfo*       pCreateInfo;
        const VkAllocationCallbacks*    pAllocator;
        VkDevice*                       pDevice;
        VkResult                        result;
    } userData{ physicalDevice, pDesc, pCreateInfo, pAllocator, pDevice };

    VkPhysicalDeviceFeatures2KHR features{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR };
    pDesc->chainers.pfnFeature(static_cast<VkBaseOutStructure*>(static_cast<void*>(&features)), &userData,
        [](VkBaseOutStructure* p, void* pUser) {
            UserData* pUserData = static_cast<UserData*>(pUser);
            const detail::VpProfileDesc* pDesc = pUserData->pDesc;
            const VpDeviceCreateInfo* pCreateInfo = pUserData->pCreateInfo;

            bool merge = (pCreateInfo->flags & VP_DEVICE_CREATE_MERGE_EXTENSIONS_BIT) != 0;
            bool override = (pCreateInfo->flags & VP_DEVICE_CREATE_OVERRIDE_EXTENSIONS_BIT) != 0;

            if (!merge && !override && pCreateInfo->pCreateInfo->enabledExtensionCount > 0) {
                // If neither merge nor override is used then the application must not specify its
                // own extensions
                pUserData->result = VK_ERROR_UNKNOWN;
                return;
            }

            std::vector<const char*> extensions;
            detail::vpGetExtensions(pCreateInfo->pCreateInfo->enabledExtensionCount,
                                    pCreateInfo->pCreateInfo->ppEnabledExtensionNames,
                                    pDesc->deviceExtensionCount,
                                    pDesc->pDeviceExtensions,
                                    extensions, merge, override);

            VkBaseOutStructure profileStructList;
            profileStructList.pNext = p;
            VkPhysicalDeviceFeatures2KHR* pFeatures = static_cast<VkPhysicalDeviceFeatures2KHR*>(static_cast<void*>(p));
            if (pDesc->feature.pfnFiller != nullptr) {
                while (p != nullptr) {
                    pDesc->feature.pfnFiller(p);
                    p = p->pNext;
                }
            }

            if (pCreateInfo->pCreateInfo->pEnabledFeatures != nullptr) {
                pFeatures->features = *pCreateInfo->pCreateInfo->pEnabledFeatures;
            }

            if (pCreateInfo->flags & VP_DEVICE_CREATE_DISABLE_ROBUST_BUFFER_ACCESS_BIT) {
                pFeatures->features.robustBufferAccess = VK_FALSE;
            }

#ifdef VK_EXT_robustness2
            VkPhysicalDeviceRobustness2FeaturesEXT* pRobustness2FeaturesEXT = static_cast<VkPhysicalDeviceRobustness2FeaturesEXT*>(
                detail::vpGetStructure(pFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT));
            if (pRobustness2FeaturesEXT != nullptr) {
                if (pCreateInfo->flags & VP_DEVICE_CREATE_DISABLE_ROBUST_BUFFER_ACCESS_BIT) {
                    pRobustness2FeaturesEXT->robustBufferAccess2 = VK_FALSE;
                }
                if (pCreateInfo->flags & VP_DEVICE_CREATE_DISABLE_ROBUST_IMAGE_ACCESS_BIT) {
                    pRobustness2FeaturesEXT->robustImageAccess2 = VK_FALSE;
                }
            }
#endif

#ifdef VK_EXT_image_robustness
            VkPhysicalDeviceImageRobustnessFeaturesEXT* pImageRobustnessFeaturesEXT = static_cast<VkPhysicalDeviceImageRobustnessFeaturesEXT*>(
                detail::vpGetStructure(pFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_ROBUSTNESS_FEATURES_EXT));
            if (pImageRobustnessFeaturesEXT != nullptr && (pCreateInfo->flags & VP_DEVICE_CREATE_DISABLE_ROBUST_IMAGE_ACCESS_BIT)) {
                pImageRobustnessFeaturesEXT->robustImageAccess = VK_FALSE;
            }
#endif

#ifdef VK_VERSION_1_3
            VkPhysicalDeviceVulkan13Features* pVulkan13Features = static_cast<VkPhysicalDeviceVulkan13Features*>(
                detail::vpGetStructure(pFeatures, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES));
            if (pVulkan13Features != nullptr && (pCreateInfo->flags & VP_DEVICE_CREATE_DISABLE_ROBUST_IMAGE_ACCESS_BIT)) {
                pVulkan13Features->robustImageAccess = VK_FALSE;
            }
#endif

            VkBaseOutStructure* pNext = static_cast<VkBaseOutStructure*>(const_cast<void*>(pCreateInfo->pCreateInfo->pNext));
            if ((pCreateInfo->flags & VP_DEVICE_CREATE_OVERRIDE_ALL_FEATURES_BIT) == 0) {
                for (uint32_t i = 0; i < pDesc->featureStructTypeCount; ++i) {
                    const void* pRequested = detail::vpGetStructure(pNext, pDesc->pFeatureStructTypes[i]);
                    if (pRequested == nullptr) {
                        VkBaseOutStructure* pPrevStruct = &profileStructList;
                        VkBaseOutStructure* pCurrStruct = pPrevStruct->pNext;
                        while (pCurrStruct->sType != pDesc->pFeatureStructTypes[i]) {
                            pPrevStruct = pCurrStruct;
                            pCurrStruct = pCurrStruct->pNext;
                        }
                        pPrevStruct->pNext = pCurrStruct->pNext;
                        pCurrStruct->pNext = pNext;
                        pNext = pCurrStruct;
                    } else
                    if ((pCreateInfo->flags & VP_DEVICE_CREATE_OVERRIDE_FEATURES_BIT) == 0) {
                        // If override is not used then the application must not specify its
                        // own feature structure for anything that the profile defines
                        pUserData->result = VK_ERROR_UNKNOWN;
                        return;
                    }
                }
            }

            VkDeviceCreateInfo createInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
            createInfo.pNext = pNext;
            createInfo.queueCreateInfoCount = pCreateInfo->pCreateInfo->queueCreateInfoCount;
            createInfo.pQueueCreateInfos = pCreateInfo->pCreateInfo->pQueueCreateInfos;
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();
            if (pCreateInfo->flags & VP_DEVICE_CREATE_OVERRIDE_ALL_FEATURES_BIT) {
                createInfo.pEnabledFeatures = pCreateInfo->pCreateInfo->pEnabledFeatures;
            }
            pUserData->result = VulkanRHI::vkCreateDevice(pUserData->physicalDevice, &createInfo, pUserData->pAllocator, pUserData->pDevice);
        }
    );

    return userData.result;
}

VPAPI_ATTR VkResult vpGetProfileInstanceExtensionProperties(const VpProfileProperties *pProfile, uint32_t *pPropertyCount,
                                                            VkExtensionProperties *pProperties) {
    VkResult result = VK_SUCCESS;

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    if (pProperties == nullptr) {
        *pPropertyCount = pDesc->instanceExtensionCount;
    } else {
        if (*pPropertyCount < pDesc->instanceExtensionCount) {
            result = VK_INCOMPLETE;
        } else {
            *pPropertyCount = pDesc->instanceExtensionCount;
        }
        for (uint32_t i = 0; i < *pPropertyCount; ++i) {
            pProperties[i] = pDesc->pInstanceExtensions[i];
        }
    }
    return result;
}

VPAPI_ATTR VkResult vpGetProfileDeviceExtensionProperties(const VpProfileProperties *pProfile, uint32_t *pPropertyCount,
                                                          VkExtensionProperties *pProperties) {
    VkResult result = VK_SUCCESS;

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    if (pProperties == nullptr) {
        *pPropertyCount = pDesc->deviceExtensionCount;
    } else {
        if (*pPropertyCount < pDesc->deviceExtensionCount) {
            result = VK_INCOMPLETE;
        } else {
            *pPropertyCount = pDesc->deviceExtensionCount;
        }
        for (uint32_t i = 0; i < *pPropertyCount; ++i) {
            pProperties[i] = pDesc->pDeviceExtensions[i];
        }
    }
    return result;
}

VPAPI_ATTR void vpGetProfileFeatures(const VpProfileProperties *pProfile, void *pNext) {
    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc != nullptr && pDesc->feature.pfnFiller != nullptr) {
        VkBaseOutStructure* p = static_cast<VkBaseOutStructure*>(pNext);
        while (p != nullptr) {
            pDesc->feature.pfnFiller(p);
            p = p->pNext;
        }
    }
}

VPAPI_ATTR VkResult vpGetProfileFeatureStructureTypes(const VpProfileProperties *pProfile, uint32_t *pStructureTypeCount,
                                                      VkStructureType *pStructureTypes) {
    VkResult result = VK_SUCCESS;

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    if (pStructureTypes == nullptr) {
        *pStructureTypeCount = pDesc->featureStructTypeCount;
    } else {
        if (*pStructureTypeCount < pDesc->featureStructTypeCount) {
            result = VK_INCOMPLETE;
        } else {
            *pStructureTypeCount = pDesc->featureStructTypeCount;
        }
        for (uint32_t i = 0; i < *pStructureTypeCount; ++i) {
            pStructureTypes[i] = pDesc->pFeatureStructTypes[i];
        }
    }
    return result;
}

VPAPI_ATTR void vpGetProfileProperties(const VpProfileProperties *pProfile, void *pNext) {
    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc != nullptr && pDesc->property.pfnFiller != nullptr) {
        VkBaseOutStructure* p = static_cast<VkBaseOutStructure*>(pNext);
        while (p != nullptr) {
            pDesc->property.pfnFiller(p);
            p = p->pNext;
        }
    }
}

VPAPI_ATTR VkResult vpGetProfilePropertyStructureTypes(const VpProfileProperties *pProfile, uint32_t *pStructureTypeCount,
                                                       VkStructureType *pStructureTypes) {
    VkResult result = VK_SUCCESS;

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    if (pStructureTypes == nullptr) {
        *pStructureTypeCount = pDesc->propertyStructTypeCount;
    } else {
        if (*pStructureTypeCount < pDesc->propertyStructTypeCount) {
            result = VK_INCOMPLETE;
        } else {
            *pStructureTypeCount = pDesc->propertyStructTypeCount;
        }
        for (uint32_t i = 0; i < *pStructureTypeCount; ++i) {
            pStructureTypes[i] = pDesc->pPropertyStructTypes[i];
        }
    }
    return result;
}

VPAPI_ATTR VkResult vpGetProfileQueueFamilyProperties(const VpProfileProperties *pProfile, uint32_t *pPropertyCount,
                                                      VkQueueFamilyProperties2KHR *pProperties) {
    VkResult result = VK_SUCCESS;

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    if (pProperties == nullptr) {
        *pPropertyCount = pDesc->queueFamilyCount;
    } else {
        if (*pPropertyCount < pDesc->queueFamilyCount) {
            result = VK_INCOMPLETE;
        } else {
            *pPropertyCount = pDesc->queueFamilyCount;
        }
        for (uint32_t i = 0; i < *pPropertyCount; ++i) {
            VkBaseOutStructure* p = static_cast<VkBaseOutStructure*>(static_cast<void*>(&pProperties[i]));
            while (p != nullptr) {
                pDesc->pQueueFamilies[i].pfnFiller(p);
                p = p->pNext;
            }
        }
    }
    return result;
}

VPAPI_ATTR VkResult vpGetProfileQueueFamilyStructureTypes(const VpProfileProperties *pProfile, uint32_t *pStructureTypeCount,
                                                          VkStructureType *pStructureTypes) {
    VkResult result = VK_SUCCESS;

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    if (pStructureTypes == nullptr) {
        *pStructureTypeCount = pDesc->queueFamilyStructTypeCount;
    } else {
        if (*pStructureTypeCount < pDesc->queueFamilyStructTypeCount) {
            result = VK_INCOMPLETE;
        } else {
            *pStructureTypeCount = pDesc->queueFamilyStructTypeCount;
        }
        for (uint32_t i = 0; i < *pStructureTypeCount; ++i) {
            pStructureTypes[i] = pDesc->pQueueFamilyStructTypes[i];
        }
    }
    return result;
}

VPAPI_ATTR VkResult vpGetProfileFormats(const VpProfileProperties *pProfile, uint32_t *pFormatCount, VkFormat *pFormats) {
    VkResult result = VK_SUCCESS;

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    if (pFormats == nullptr) {
        *pFormatCount = pDesc->formatCount;
    } else {
        if (*pFormatCount < pDesc->formatCount) {
            result = VK_INCOMPLETE;
        } else {
            *pFormatCount = pDesc->formatCount;
        }
        for (uint32_t i = 0; i < *pFormatCount; ++i) {
            pFormats[i] = pDesc->pFormats[i].format;
        }
    }
    return result;
}

VPAPI_ATTR void vpGetProfileFormatProperties(const VpProfileProperties *pProfile, VkFormat format, void *pNext) {
    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return;

    for (uint32_t i = 0; i < pDesc->formatCount; ++i) {
        if (pDesc->pFormats[i].format == format) {
            VkBaseOutStructure* p = static_cast<VkBaseOutStructure*>(static_cast<void*>(pNext));
            while (p != nullptr) {
                pDesc->pFormats[i].pfnFiller(p);
                p = p->pNext;
            }
#if defined(VK_VERSION_1_3) || defined(VK_KHR_format_feature_flags2)
            VkFormatProperties2KHR* fp2 = static_cast<VkFormatProperties2KHR*>(
                detail::vpGetStructure(pNext, VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR));
            VkFormatProperties3KHR* fp3 = static_cast<VkFormatProperties3KHR*>(
                detail::vpGetStructure(pNext, VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR));
            if (fp3 != nullptr) {
                VkFormatProperties2KHR fp{ VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2_KHR };
                pDesc->pFormats[i].pfnFiller(static_cast<VkBaseOutStructure*>(static_cast<void*>(&fp)));
                fp3->linearTilingFeatures = static_cast<VkFormatFeatureFlags2KHR>(fp3->linearTilingFeatures | fp.formatProperties.linearTilingFeatures);
                fp3->optimalTilingFeatures = static_cast<VkFormatFeatureFlags2KHR>(fp3->optimalTilingFeatures | fp.formatProperties.optimalTilingFeatures);
                fp3->bufferFeatures = static_cast<VkFormatFeatureFlags2KHR>(fp3->bufferFeatures | fp.formatProperties.bufferFeatures);
            }
            if (fp2 != nullptr) {
                VkFormatProperties3KHR fp{ VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_3_KHR };
                pDesc->pFormats[i].pfnFiller(static_cast<VkBaseOutStructure*>(static_cast<void*>(&fp)));
                fp2->formatProperties.linearTilingFeatures = static_cast<VkFormatFeatureFlags>(fp2->formatProperties.linearTilingFeatures | fp.linearTilingFeatures);
                fp2->formatProperties.optimalTilingFeatures = static_cast<VkFormatFeatureFlags>(fp2->formatProperties.optimalTilingFeatures | fp.optimalTilingFeatures);
                fp2->formatProperties.bufferFeatures = static_cast<VkFormatFeatureFlags>(fp2->formatProperties.bufferFeatures | fp.bufferFeatures);
            }
#endif
        }
    }
}

VPAPI_ATTR VkResult vpGetProfileFormatStructureTypes(const VpProfileProperties *pProfile, uint32_t *pStructureTypeCount,
                                                     VkStructureType *pStructureTypes) {
    VkResult result = VK_SUCCESS;

    const detail::VpProfileDesc* pDesc = detail::vpGetProfileDesc(pProfile->profileName);
    if (pDesc == nullptr) return VK_ERROR_UNKNOWN;

    if (pStructureTypes == nullptr) {
        *pStructureTypeCount = pDesc->formatStructTypeCount;
    } else {
        if (*pStructureTypeCount < pDesc->formatStructTypeCount) {
            result = VK_INCOMPLETE;
        } else {
            *pStructureTypeCount = pDesc->formatStructTypeCount;
        }
        for (uint32_t i = 0; i < *pStructureTypeCount; ++i) {
            pStructureTypes[i] = pDesc->pFormatStructTypes[i];
        }
    }
    return result;
}

#endif // VULKAN_PROFILES_HPP_

#pragma warning(pop) // restore 4191
