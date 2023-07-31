/*
 * Copyright 2019 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "SwappyVk.h"

namespace swappy {

class DefaultSwappyVkFunctionProvider {
  public:

    static bool Init() {
        if (!mLibVulkan) {
            // This is the first time we've been called
            mLibVulkan = dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
            if (!mLibVulkan)
            {
                // If Vulkan doesn't exist, bail-out early:
                return false;
            }
        }
        return true;
    }
    static void* GetProcAddr(const char* name) {
        if (!mLibVulkan && !Init())
                return nullptr;
        return dlsym(mLibVulkan, name);
    }
    static void Close() {
        if (mLibVulkan) {
            dlclose(mLibVulkan);
            mLibVulkan = nullptr;
        }
    }
  private:
    static void* mLibVulkan;
};

void* DefaultSwappyVkFunctionProvider::mLibVulkan = nullptr;

bool SwappyVk::InitFunctions() {
    if (pFunctionProvider == nullptr) {
        static SwappyVkFunctionProvider c_provider;
        c_provider.init = &DefaultSwappyVkFunctionProvider::Init;
        c_provider.getProcAddr = &DefaultSwappyVkFunctionProvider::GetProcAddr;
        c_provider.close = &DefaultSwappyVkFunctionProvider::Close;
        pFunctionProvider = &c_provider;
    }
    if (pFunctionProvider->init()) {
        LoadVulkanFunctions(pFunctionProvider);
        return true;
    } else {
        return false;
    }
}
void SwappyVk::SetFunctionProvider(const SwappyVkFunctionProvider* functionProvider) {
    if (pFunctionProvider!=nullptr)
        pFunctionProvider->close();
    pFunctionProvider = functionProvider;
}

/**
 * Generic/Singleton implementation of swappyVkDetermineDeviceExtensions.
 */
void SwappyVk::swappyVkDetermineDeviceExtensions(
    VkPhysicalDevice       physicalDevice,
    uint32_t               availableExtensionCount,
    VkExtensionProperties* pAvailableExtensions,
    uint32_t*              pRequiredExtensionCount,
    char**                 pRequiredExtensions)
{
#if (not defined ANDROID_NDK_VERSION) || ANDROID_NDK_VERSION>=15
    // TODO: Refactor this to be more concise:
    if (!pRequiredExtensions) {
        for (uint32_t i = 0; i < availableExtensionCount; i++) {
            if (!strcmp(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME,
                        pAvailableExtensions[i].extensionName)) {
                (*pRequiredExtensionCount)++;
            }
        }
    } else {
        doesPhysicalDeviceHaveGoogleDisplayTiming[physicalDevice] = false;
        for (uint32_t i = 0, j = 0; i < availableExtensionCount; i++) {
            if (!strcmp(VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME,
                        pAvailableExtensions[i].extensionName)) {
                if (j < *pRequiredExtensionCount) {
                    strcpy(pRequiredExtensions[j++], VK_GOOGLE_DISPLAY_TIMING_EXTENSION_NAME);
                    doesPhysicalDeviceHaveGoogleDisplayTiming[physicalDevice] = true;
                }
            }
        }
    }
#else
    doesPhysicalDeviceHaveGoogleDisplayTiming[physicalDevice] = false;
#endif
}

void SwappyVk::SetQueueFamilyIndex(VkDevice   device,
                                   VkQueue    queue,
                                   uint32_t   queueFamilyIndex)
{
    perQueueFamilyIndex[queue] = {device, queueFamilyIndex};
}


/**
 * Generic/Singleton implementation of swappyVkGetRefreshCycleDuration.
 */
bool SwappyVk::GetRefreshCycleDuration(JNIEnv           *env,
                                       jobject          jactivity,
                                       VkPhysicalDevice physicalDevice,
                                       VkDevice         device,
                                       VkSwapchainKHR   swapchain,
                                       uint64_t*        pRefreshDuration)
{
    auto& pImplementation = perDeviceImplementation[device];
    if (!pImplementation) {

        if (!InitFunctions()) {
            // If Vulkan doesn't exist, bail-out early
            return false;
        }

#if (not defined ANDROID_NDK_VERSION) || ANDROID_NDK_VERSION>=15
        // First, based on whether VK_GOOGLE_display_timing is available
        // (determined and cached by swappyVkDetermineDeviceExtensions),
        // determine which derived class to use to implement the rest of the API
        if (doesPhysicalDeviceHaveGoogleDisplayTiming[physicalDevice]) {
            pImplementation = std::make_shared<SwappyVkGoogleDisplayTiming>
                    (env, jactivity, physicalDevice, device, pFunctionProvider);
            ALOGV("SwappyVk initialized for VkDevice %p using VK_GOOGLE_display_timing on Android", device);
        } else
#endif
        {
            pImplementation = std::make_shared<SwappyVkFallback>
                    (env, jactivity, physicalDevice, device, pFunctionProvider);
            ALOGV("SwappyVk initialized for VkDevice %p using Android fallback", device);
        }

        if(!pImplementation){  // should never happen
            ALOGE("SwappyVk could not find or create correct implementation for the current environment: "
                  "%p, %p", physicalDevice, device);
            return false;
        }
    }

    // Cache the per-swapchain pointer to the derived class:
    perSwapchainImplementation[swapchain] = pImplementation;

    // Now, call that derived class to get the refresh duration to return
    return pImplementation->doGetRefreshCycleDuration(swapchain,
                                                      pRefreshDuration);
}


/**
 * Generic/Singleton implementation of swappyVkSetSwapInterval.
 */
void SwappyVk::SetSwapIntervalNS(VkDevice       device,
                                 VkSwapchainKHR swapchain,
                                 uint64_t       swap_ns)
{
    auto& pImplementation = perDeviceImplementation[device];
    if (!pImplementation) {
        return;
    }
    pImplementation->doSetSwapInterval(swapchain, swap_ns);
}


/**
 * Generic/Singleton implementation of swappyVkQueuePresent.
 */
VkResult SwappyVk::QueuePresent(VkQueue                 queue,
                                const VkPresentInfoKHR* pPresentInfo)
{
    if (perQueueFamilyIndex.find(queue) == perQueueFamilyIndex.end()) {
        ALOGE("Unknown queue %p. Did you call SwappyVkSetQueueFamilyIndex ?", queue);
        return VK_INCOMPLETE;
    }

    // This command doesn't have a VkDevice.  It should have at least one VkSwapchainKHR's.  For
    // this command, all VkSwapchainKHR's will have the same VkDevice and VkQueue.
    if ((pPresentInfo->swapchainCount == 0) || (!pPresentInfo->pSwapchains)) {
        // This shouldn't happen, but if it does, something is really wrong.
        return VK_ERROR_DEVICE_LOST;
    }
    auto& pImplementation = perSwapchainImplementation[*pPresentInfo->pSwapchains];
    if (pImplementation) {
        return pImplementation->doQueuePresent(queue,
                                               perQueueFamilyIndex[queue].queueFamilyIndex,
                                               pPresentInfo);
    } else {
        // This should only happen if the API was used wrong (e.g. they never
        // called swappyVkGetRefreshCycleDuration).
        // NOTE: Technically, a Vulkan library shouldn't protect a user from
        // themselves, but we'll be friendlier
        return VK_ERROR_DEVICE_LOST;
    }
}

void SwappyVk::DestroySwapchain(VkDevice                device,
                                VkSwapchainKHR          swapchain)
{
    auto pImpl = perSwapchainImplementation[swapchain].get();
    // Count the number of swapchains using this implementation.
    // NB std::count_if isn't present in earlier NDKs :(
    int n = 0;
    for (auto& it: perSwapchainImplementation) {
        if (it.second.get()==pImpl) ++n;
    }
    // Remove the device if there are no other swapchains referring to it.
    if (n==1) {
        auto it = perQueueFamilyIndex.begin();
        while (it != perQueueFamilyIndex.end()) {
            if (it->second.device == device) {
                it = perQueueFamilyIndex.erase(it);
            } else {
                ++it;
            }
        }
        perDeviceImplementation.erase(device);
    }
    perSwapchainImplementation.erase(swapchain);
}

void SwappyVk::SetAutoSwapInterval(bool enabled) {
    for (auto i : perSwapchainImplementation) {
        i.second->setAutoSwapInterval(enabled);
    }
}

void SwappyVk::SetAutoPipelineMode(bool enabled) {
    for (auto i : perSwapchainImplementation) {
        i.second->setAutoPipelineMode(enabled);
    }
}

void SwappyVk::SetMaxAutoSwapIntervalNS(std::chrono::nanoseconds maxSwapNS) {
    for (auto i : perSwapchainImplementation) {
        i.second->setMaxAutoSwapIntervalNS(maxSwapNS);
    }
}

void SwappyVk::SetFenceTimeout(std::chrono::nanoseconds t) {
    for(auto i : perDeviceImplementation) {
        i.second->setFenceTimeout(t);
    }
}
std::chrono::nanoseconds SwappyVk::GetFenceTimeout() const {
    auto it = perDeviceImplementation.begin();
    if (it != perDeviceImplementation.end())
        return it->second->getFenceTimeout();
    return std::chrono::nanoseconds(0);
}

void SwappyVk::addTracer(const SwappyTracer *t){
    for (auto i : perSwapchainImplementation) {
        i.second->addTracer(t);
    }
}

}  // namespace swappy
