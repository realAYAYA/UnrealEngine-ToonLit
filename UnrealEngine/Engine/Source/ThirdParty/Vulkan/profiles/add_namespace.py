# Use the generated header in the "debug" folder because we want the option to print specific errors
with open(r'./debug/vulkan_profiles.hpp', 'r') as f:
    file_data = f.read()

    # Add pragma used everywhere to skip about casting Vulkan function pointers
    insert_index = file_data.find("#ifndef VULKAN_PROFILES_HPP_")
    data = file_data[:insert_index] + "#pragma warning(push)\n#pragma warning(disable : 4191) // warning C4191: 'type cast': unsafe conversion\n\n" + file_data[insert_index:] + "\n#pragma warning(pop) // restore 4191\n"
    
    # Add our namespace in front of functions assumed by the generation scripts
    data = data.replace("vkGetInstanceProcAddr(","VulkanRHI::vkGetInstanceProcAddr(")
    data = data.replace("vkEnumerateInstanceExtensionProperties(","VulkanRHI::vkEnumerateInstanceExtensionProperties(")
    data = data.replace("vkCreateInstance(","VulkanRHI::vkCreateInstance(")
    data = data.replace("vkEnumerateDeviceExtensionProperties(","VulkanRHI::vkEnumerateDeviceExtensionProperties(")
    data = data.replace("vkGetPhysicalDeviceProperties(","VulkanRHI::vkGetPhysicalDeviceProperties(")
    data = data.replace("vkCreateDevice(","VulkanRHI::vkCreateDevice(")

# Write final header with text namespaces added
with open(r'./include/vulkan_profiles_ue.h', 'w') as f:
    f.write(data)
