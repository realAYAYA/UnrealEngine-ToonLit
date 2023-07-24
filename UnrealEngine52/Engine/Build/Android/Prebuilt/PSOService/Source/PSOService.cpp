// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PSOService.cpp: Vulkan PSO compilation service
=============================================================================*/

#include <jni.h>
#include <android/log.h>
#include "vulkan/vulkan.h"
#include <vector>
#include <list>
#include <string>

#define APPNAME "UEPSOService"

#define JNI_METHOD __attribute__ ((visibility ("default"))) extern "C"

VKAPI_ATTR VkBool32 VKAPI_CALL VKValidationCallback(
	VkDebugReportFlagsEXT       flags,
	VkDebugReportObjectTypeEXT  objectType,
	uint64_t                    object,
	size_t                      location,
	int32_t                     messageCode,
	const char* pLayerPrefix,
	const char* pMessage,
	void* pUserData)
{
	__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, "VK Validation: %s", pMessage);
	return VK_FALSE;
}

class FVulkanPSOCompiler
{
	bool bInitialized = false;
	VkDevice Device = VK_NULL_HANDLE;
	VkInstance Instance = VK_NULL_HANDLE;

	std::vector<VkPhysicalDevice> devices;
	PFN_vkCreateRenderPass2KHR vkCreateRenderPass2;

	size_t PSOCacheSize = 0;
	char * PSOCacheData = nullptr;
	bool bSinglePSOPerCache = true;
	VkPipelineCache PipelineCache = VK_NULL_HANDLE;

public:
	static FVulkanPSOCompiler& Get()
	{
		static FVulkanPSOCompiler Single;
		return Single;
	}

	void InitDevice(std::vector<const char*>& InstanceLayers, std::vector<const char*>& InstanceExtensions, std::vector<const char*>& DeviceExtensions)
	{
		if (bInitialized)
			return;

		bInitialized = true;

		VkApplicationInfo appInfo{};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = APPNAME;
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = nullptr;
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.enabledExtensionCount = InstanceExtensions.size();
		createInfo.ppEnabledExtensionNames = InstanceExtensions.data();
		createInfo.enabledLayerCount = InstanceLayers.size();
		createInfo.ppEnabledLayerNames = InstanceLayers.data();

		bool bEnableValidation = false;
		for (uint32_t Idx; Idx < InstanceLayers.size(); ++Idx)
		{
			bEnableValidation = strcmp(InstanceLayers[Idx], "VK_LAYER_KHRONOS_validation") == 0;

			if (bEnableValidation)
			{
				__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " VK_LAYER_KHRONOS_validation Validation Enabled");
				break;
			}
		}

		VkResult Result;
		Result = vkCreateInstance(&createInfo, NULL, &Instance);

		if (Result != VK_SUCCESS)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " Failed to Create VKInstance %d ", Result);
			exit(-1);
		}

		/* Load VK_EXT_debug_report entry points in debug builds */
		if (bEnableValidation)
		{
			PFN_vkCreateDebugReportCallbackEXT vkCreateDebugReportCallbackEXT = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(Instance, "vkCreateDebugReportCallbackEXT"));
			PFN_vkDebugReportMessageEXT vkDebugReportMessageEXT = reinterpret_cast<PFN_vkDebugReportMessageEXT>(vkGetInstanceProcAddr(Instance, "vkDebugReportMessageEXT"));
			PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(Instance, "vkDestroyDebugReportCallbackEXT"));

			VkDebugReportCallbackCreateInfoEXT CallbackCreateInfo;
			CallbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
			CallbackCreateInfo.pNext = nullptr;
			CallbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
			CallbackCreateInfo.pfnCallback = &VKValidationCallback;
			CallbackCreateInfo.pUserData = nullptr;

			/* Register the callback */
			VkDebugReportCallbackEXT Callback;
			Result = vkCreateDebugReportCallbackEXT(Instance, &CallbackCreateInfo, nullptr, &Callback);
			__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " Created Debug Hooks %d ", Result);
		}

		// Get the number of devices (GPUs) available.
		uint32_t gpu_count = 0;
		Result = vkEnumeratePhysicalDevices(Instance, &gpu_count, NULL);

		if (Result != VK_SUCCESS)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " Failed to Enumerate Physical Devices 1 %d ", Result);
			exit(-1);
		}
		
		// Allocate space and get the list of devices.
		devices.resize(gpu_count);
		Result = vkEnumeratePhysicalDevices(Instance, &gpu_count, devices.data());

		if (Result != VK_SUCCESS)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " Failed to Enumerate Physical Devices 2 %d ", Result);
		}

		uint32_t queue_count = 0;

		vkGetPhysicalDeviceQueueFamilyProperties(devices[0], &queue_count, nullptr);

		std::vector<VkQueueFamilyProperties> queues(queue_count);
		vkGetPhysicalDeviceQueueFamilyProperties(devices[0], &queue_count, queues.data());

		uint32_t gfx_queue_idx = 0;

		bool found = false;
		for (unsigned int i = 0; i < queue_count; i++)
		{
			if (queues[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				gfx_queue_idx = i;
				found = true;
				break;
			}
		}

		VkPhysicalDeviceFeatures PhysicalFeatures;
		vkGetPhysicalDeviceFeatures(devices[0], &PhysicalFeatures);

		PhysicalFeatures.shaderResourceResidency = VK_FALSE;
		PhysicalFeatures.shaderResourceMinLod = VK_FALSE;
		PhysicalFeatures.sparseBinding = VK_FALSE;
		PhysicalFeatures.sparseResidencyBuffer = VK_FALSE;
		PhysicalFeatures.sparseResidencyImage2D = VK_FALSE;
		PhysicalFeatures.sparseResidencyImage3D = VK_FALSE;
		PhysicalFeatures.sparseResidency2Samples = VK_FALSE;
		PhysicalFeatures.sparseResidency4Samples = VK_FALSE;
		PhysicalFeatures.sparseResidency8Samples = VK_FALSE;
		PhysicalFeatures.sparseResidencyAliased = VK_FALSE;

		VkDeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = gfx_queue_idx;
		queueCreateInfo.queueCount = queues[gfx_queue_idx].queueCount;
		float* QueuePriorities = (float*)alloca(queues[gfx_queue_idx].queueCount * sizeof(float));
		memset(QueuePriorities, 0, queues[gfx_queue_idx].queueCount * sizeof(float));

		queueCreateInfo.pQueuePriorities = QueuePriorities;

		VkDeviceCreateInfo device_info = {};
		device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_info.pNext = NULL;
		device_info.queueCreateInfoCount = 1;
		device_info.pQueueCreateInfos = &queueCreateInfo;
		device_info.enabledLayerCount = 0;
		device_info.ppEnabledLayerNames = NULL;
		device_info.enabledExtensionCount = DeviceExtensions.size();
		device_info.ppEnabledExtensionNames = DeviceExtensions.data();
		device_info.pEnabledFeatures = &PhysicalFeatures;

		Result = vkCreateDevice(devices[0], &device_info, NULL, &Device);

		if (Result != VK_SUCCESS)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " Failed to Create Device %d ", Result);
		}

		vkCreateRenderPass2 = (PFN_vkCreateRenderPass2KHR)vkGetDeviceProcAddr(Device, "vkCreateRenderPass2KHR");

		if (vkCreateRenderPass2 == nullptr)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, "Failed getting pointer to vkCreateRenderPass2 ");
		}

	}

	void ShutDownDevice()
	{
		if (Device == VK_NULL_HANDLE)
		{
			return;
		}

		if (PipelineCache != VK_NULL_HANDLE)
		{
			vkDestroyPipelineCache(Device, PipelineCache, nullptr);
			PipelineCache = VK_NULL_HANDLE;
		}

		vkDestroyDevice(Device, nullptr);
		vkDestroyInstance(Instance, nullptr);
	}

	struct GraphicsPipelineCreateInfo
	{
		VkPipelineCreateFlags PipelineCreateFlags;
		uint32_t StageCount;

		bool bHasVkPipelineVertexInputStateCreateInfo;
		bool bHasVkPipelineInputAssemblyStateCreateInfo;
		bool bHasVkPipelineTessellationStateCreateInfo;
		bool bHasVkPipelineViewportStateCreateInfo;
		bool bHasVkPipelineRasterizationStateCreateInfo;
		bool bHasVkPipelineMultisampleStateCreateInfo;
		bool bHasVkPipelineDepthStencilStateCreateInfo;
		bool bHasVkPipelineColorBlendStateCreateInfo;
		bool bHasVkPipelineDynamicStateCreateInfo;

		uint32_t subpass;
	};

#define COPY_FROM_BUFFER(Dst, Src, Offset, Size) \
		memcpy(Dst, &Src[Offset], Size); \
		Offset += Size;

	void BufferToCharArray(std::vector<const char*>& CharArray, const uint8_t* MemoryStream, uint32_t& MemoryOffset)
	{
		uint32_t Count;
		COPY_FROM_BUFFER(&Count, MemoryStream, MemoryOffset, sizeof(uint32_t));

		for (uint32_t Idx = 0; Idx < Count; ++Idx)
		{
			uint32_t StrLength;
			COPY_FROM_BUFFER(&StrLength, MemoryStream, MemoryOffset, sizeof(uint32_t));
			CharArray.push_back((const char*)&MemoryStream[MemoryOffset]);

			MemoryOffset += StrLength;
		}
	}

	std::string CompileGFXPSO(const uint8_t* VS, uint64_t VSSize, const uint8_t* PS, uint64_t PSSize, const uint8_t* PSO, uint64_t PSOSize)
	{
		std::string errorLog;
		uint32_t MemoryOffset = 0;

		// Read extensions and layers
		std::vector<const char*> InstanceLayers;
		BufferToCharArray(InstanceLayers, PSO, MemoryOffset);
		std::vector<const char*> InstanceExtensions;
		BufferToCharArray(InstanceExtensions, PSO, MemoryOffset);
		std::vector<const char*> DeviceExtensions;
		BufferToCharArray(DeviceExtensions, PSO, MemoryOffset);

		InitDevice(InstanceLayers, InstanceExtensions, DeviceExtensions);

		// Free PSO Cache
		VkResult Result;
		GraphicsPipelineCreateInfo PipelineCreateInfo;
		VkGraphicsPipelineCreateInfo CreateInfo;

		//__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, "CompileGFXPSO: VSSize %d, PSSize %d, PSOSize %d", (uint32_t)VSSize, (uint32_t)PSSize, (uint32_t)PSOSize);

		if (bSinglePSOPerCache && PSOCacheData)
		{
			free(PSOCacheData);
			PSOCacheSize = 0;
			PSOCacheData = nullptr;

			vkDestroyPipelineCache(Device, PipelineCache, nullptr);
			PipelineCache = VK_NULL_HANDLE;
		}

		// Create PSO
		COPY_FROM_BUFFER(&PipelineCreateInfo, PSO, MemoryOffset, sizeof(GraphicsPipelineCreateInfo));

		memset(&CreateInfo, 0, sizeof(VkGraphicsPipelineCreateInfo));
		CreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		CreateInfo.flags = PipelineCreateInfo.PipelineCreateFlags;
		CreateInfo.stageCount = PipelineCreateInfo.StageCount;
		CreateInfo.subpass = PipelineCreateInfo.subpass;

		// FSR
		bool bHasFSRCreateInfo = false;
		COPY_FROM_BUFFER(&bHasFSRCreateInfo, PSO, MemoryOffset, sizeof(bool));

		VkPipelineFragmentShadingRateStateCreateInfoKHR FSRCreateInfo;
		if (bHasFSRCreateInfo)
		{
			COPY_FROM_BUFFER(&FSRCreateInfo, PSO, MemoryOffset, sizeof(VkPipelineFragmentShadingRateStateCreateInfoKHR));
			FSRCreateInfo.pNext = nullptr;
			CreateInfo.pNext = &FSRCreateInfo;
		}

		VkPipelineShaderStageCreateInfo ShaderStages[2];

		// VkPipelineShaderStageCreateInfo
		for (int32_t Idx = 0; Idx < PipelineCreateInfo.StageCount; ++Idx)
		{
			COPY_FROM_BUFFER(&ShaderStages[Idx], PSO, MemoryOffset, sizeof(VkPipelineShaderStageCreateInfo));

			uint32_t NameLength;
			COPY_FROM_BUFFER(&NameLength, PSO, MemoryOffset, sizeof(uint32_t));

			ShaderStages[Idx].pName = (const char*)&PSO[MemoryOffset];
			MemoryOffset += NameLength;
		}
		CreateInfo.pStages = ShaderStages;

		VkPipelineVertexInputStateCreateInfo VertexInputState;
		if (PipelineCreateInfo.bHasVkPipelineVertexInputStateCreateInfo)
		{
			COPY_FROM_BUFFER(&VertexInputState, PSO, MemoryOffset, sizeof(VkPipelineVertexInputStateCreateInfo));

			if (VertexInputState.vertexBindingDescriptionCount > 0)
			{
				uint32_t Length = VertexInputState.vertexBindingDescriptionCount * sizeof(VkVertexInputBindingDescription);
				VertexInputState.pVertexBindingDescriptions = (VkVertexInputBindingDescription*)&PSO[MemoryOffset];
				MemoryOffset += Length;
			}

			if (VertexInputState.vertexAttributeDescriptionCount > 0)
			{
				uint32_t Length = VertexInputState.vertexAttributeDescriptionCount * sizeof(VkVertexInputAttributeDescription);
				VertexInputState.pVertexAttributeDescriptions = (VkVertexInputAttributeDescription*)&PSO[MemoryOffset];
				MemoryOffset += Length;
			}

			CreateInfo.pVertexInputState = &VertexInputState;
		}

		VkPipelineInputAssemblyStateCreateInfo InputAssemblyCreateInfo;
		if (PipelineCreateInfo.bHasVkPipelineInputAssemblyStateCreateInfo)
		{
			COPY_FROM_BUFFER(&InputAssemblyCreateInfo, PSO, MemoryOffset, sizeof(VkPipelineInputAssemblyStateCreateInfo));
			CreateInfo.pInputAssemblyState = &InputAssemblyCreateInfo;
		}

		VkPipelineTessellationStateCreateInfo TesselationCreateInfo;
		if (PipelineCreateInfo.bHasVkPipelineTessellationStateCreateInfo)
		{
			COPY_FROM_BUFFER(&TesselationCreateInfo, PSO, MemoryOffset, sizeof(VkPipelineTessellationStateCreateInfo));
			CreateInfo.pTessellationState = &TesselationCreateInfo;
		}

		VkPipelineViewportStateCreateInfo ViewportState;
		if (PipelineCreateInfo.bHasVkPipelineViewportStateCreateInfo)
		{
			COPY_FROM_BUFFER(&ViewportState, PSO, MemoryOffset, sizeof(VkPipelineViewportStateCreateInfo));

			uint32_t ViewportCount;
			COPY_FROM_BUFFER(&ViewportCount, PSO, MemoryOffset, sizeof(uint32_t));

			if (ViewportCount > 0)
			{
				ViewportState.pViewports = (VkViewport*)&PSO[MemoryOffset];
				MemoryOffset += sizeof(VkViewport) * ViewportCount;
			}

			uint32_t ScissorCount;
			COPY_FROM_BUFFER(&ScissorCount, PSO, MemoryOffset, sizeof(uint32_t));

			if (ScissorCount > 0)
			{
				ViewportState.pScissors = (VkRect2D*)&PSO[MemoryOffset];
				MemoryOffset += sizeof(VkRect2D) * ScissorCount;
			}

			CreateInfo.pViewportState = &ViewportState;
		}

		if (PipelineCreateInfo.bHasVkPipelineRasterizationStateCreateInfo)
		{
			CreateInfo.pRasterizationState = (VkPipelineRasterizationStateCreateInfo*)&PSO[MemoryOffset];
			MemoryOffset += sizeof(VkPipelineRasterizationStateCreateInfo);
		}

		if (PipelineCreateInfo.bHasVkPipelineMultisampleStateCreateInfo)
		{
			CreateInfo.pMultisampleState = (VkPipelineMultisampleStateCreateInfo*)&PSO[MemoryOffset];
			MemoryOffset += sizeof(VkPipelineMultisampleStateCreateInfo);
		}
		
		if (PipelineCreateInfo.bHasVkPipelineDepthStencilStateCreateInfo)
		{
			CreateInfo.pDepthStencilState = (VkPipelineDepthStencilStateCreateInfo*)&PSO[MemoryOffset];
			MemoryOffset += sizeof(VkPipelineDepthStencilStateCreateInfo);
		}

		VkPipelineColorBlendStateCreateInfo ColorBlendState;
		if (PipelineCreateInfo.bHasVkPipelineColorBlendStateCreateInfo)
		{
			COPY_FROM_BUFFER(&ColorBlendState, PSO, MemoryOffset, sizeof(VkPipelineColorBlendStateCreateInfo));

			if (ColorBlendState.attachmentCount > 0)
			{
				ColorBlendState.pAttachments = (VkPipelineColorBlendAttachmentState*)&PSO[MemoryOffset];
				MemoryOffset += sizeof(VkPipelineColorBlendAttachmentState) * ColorBlendState.attachmentCount;
			}

			CreateInfo.pColorBlendState = &ColorBlendState;
		}

		VkPipelineDynamicStateCreateInfo DynamicState;
		if (PipelineCreateInfo.bHasVkPipelineDynamicStateCreateInfo)
		{
			COPY_FROM_BUFFER(&DynamicState, PSO, MemoryOffset, sizeof(VkPipelineDynamicStateCreateInfo));

			if (DynamicState.dynamicStateCount > 0)
			{
				DynamicState.pDynamicStates = (VkDynamicState*)&PSO[MemoryOffset];
				MemoryOffset += sizeof(VkDynamicState) * DynamicState.dynamicStateCount;
			}

			CreateInfo.pDynamicState = &DynamicState;
		}

		VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo;
		COPY_FROM_BUFFER(&PipelineLayoutCreateInfo, PSO, MemoryOffset, sizeof(VkPipelineLayoutCreateInfo));

		VkDescriptorSetLayoutCreateInfo* DescriptorSetLayoutInfos = nullptr;
		VkDescriptorSetLayout* DescriptorSetLayouts = nullptr;

		if (PipelineLayoutCreateInfo.setLayoutCount > 0)
		{
			DescriptorSetLayoutInfos = new VkDescriptorSetLayoutCreateInfo[PipelineLayoutCreateInfo.setLayoutCount];
			DescriptorSetLayouts = new VkDescriptorSetLayout[PipelineLayoutCreateInfo.setLayoutCount];

			for (uint32_t Idx = 0; Idx < PipelineLayoutCreateInfo.setLayoutCount; ++Idx)
			{
				uint32_t SetBindingsCount;
				COPY_FROM_BUFFER(&SetBindingsCount, PSO, MemoryOffset, sizeof(uint32_t));

				DescriptorSetLayoutInfos[Idx].sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
				DescriptorSetLayoutInfos[Idx].pNext = nullptr;
				DescriptorSetLayoutInfos[Idx].flags = 0;
				DescriptorSetLayoutInfos[Idx].bindingCount = SetBindingsCount;
				DescriptorSetLayoutInfos[Idx].pBindings = (VkDescriptorSetLayoutBinding*)&PSO[MemoryOffset];
				MemoryOffset += sizeof(VkDescriptorSetLayoutBinding) * SetBindingsCount;

				vkCreateDescriptorSetLayout(Device, &DescriptorSetLayoutInfos[Idx], nullptr, &DescriptorSetLayouts[Idx]);
			}

			PipelineLayoutCreateInfo.pSetLayouts = DescriptorSetLayouts;
		}

		VkPipelineLayout PipelineLayout;
		Result = vkCreatePipelineLayout(Device, &PipelineLayoutCreateInfo, nullptr, &PipelineLayout);
		if (Result != VK_SUCCESS)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " vkCreatePipelineLayout Failed %d ", Result);
			exit(-1);
		}

		CreateInfo.layout = PipelineLayout;

		VkRenderPass RenderPass;
		bool bUseRenderPass2;
		COPY_FROM_BUFFER(&bUseRenderPass2, PSO, MemoryOffset, sizeof(bool));

		if (bUseRenderPass2)
		{
			// Render pass
			VkRenderPassCreateInfo2KHR RenderPassCreateInfo;

			COPY_FROM_BUFFER(&RenderPassCreateInfo, PSO, MemoryOffset, sizeof(VkRenderPassCreateInfo2KHR));

			// Check for VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT
			bool bHasCreateInfoNext = false;
			COPY_FROM_BUFFER(&bHasCreateInfoNext, PSO, MemoryOffset, sizeof(bool));

			if (bHasCreateInfoNext)
			{
				RenderPassCreateInfo.pNext = &PSO[MemoryOffset];
				MemoryOffset += sizeof(VkRenderPassFragmentDensityMapCreateInfoEXT);
			}

			if (RenderPassCreateInfo.attachmentCount > 0)
			{
				RenderPassCreateInfo.pAttachments = (VkAttachmentDescription2KHR*)&PSO[MemoryOffset];
				MemoryOffset += sizeof(VkAttachmentDescription2KHR) * RenderPassCreateInfo.attachmentCount;
			}

			if (RenderPassCreateInfo.dependencyCount > 0)
			{
				RenderPassCreateInfo.pDependencies = (VkSubpassDependency2KHR*)&PSO[MemoryOffset];
				MemoryOffset += sizeof(VkSubpassDependency2KHR) * RenderPassCreateInfo.dependencyCount;
			}

			VkSubpassDescription2KHR* SubpassDescriptions = new VkSubpassDescription2KHR[RenderPassCreateInfo.subpassCount];
			std::vector<VkFragmentShadingRateAttachmentInfoKHR> FSRAttachmentInfos;

			for (uint32_t Idx = 0; Idx < RenderPassCreateInfo.subpassCount; ++Idx)
			{
				COPY_FROM_BUFFER(&SubpassDescriptions[Idx], PSO, MemoryOffset, sizeof(VkSubpassDescription2KHR));

				// Add additional pNext structs
				// FSR
				bool bHasFSRAttachmentInfo = false;
				COPY_FROM_BUFFER(&bHasFSRAttachmentInfo, PSO, MemoryOffset, sizeof(bool));;

				if (bHasFSRAttachmentInfo)
				{
					FSRAttachmentInfos.push_back(VkFragmentShadingRateAttachmentInfoKHR());
					auto& FSRAttachmentInfo = FSRAttachmentInfos.back();
					FSRAttachmentInfo.pNext = nullptr;
					FSRAttachmentInfo.sType = VK_STRUCTURE_TYPE_FRAGMENT_SHADING_RATE_ATTACHMENT_INFO_KHR;
					FSRAttachmentInfo.pFragmentShadingRateAttachment = (VkAttachmentReference2KHR*)&PSO[MemoryOffset];
					MemoryOffset += sizeof(VkAttachmentReference2KHR);

					FSRAttachmentInfo.shadingRateAttachmentTexelSize = *(VkExtent2D*)&PSO[MemoryOffset];
					MemoryOffset += sizeof(VkExtent2D);

					SubpassDescriptions[Idx].pNext = &FSRAttachmentInfo;
				}

				if (SubpassDescriptions[Idx].colorAttachmentCount > 0)
				{
					SubpassDescriptions[Idx].pColorAttachments = (VkAttachmentReference2KHR*)&PSO[MemoryOffset];
					MemoryOffset += sizeof(VkAttachmentReference2KHR) * SubpassDescriptions[Idx].colorAttachmentCount;
				}

				if (SubpassDescriptions[Idx].inputAttachmentCount > 0)
				{
					SubpassDescriptions[Idx].pInputAttachments = (VkAttachmentReference2KHR*)&PSO[MemoryOffset];
					MemoryOffset += sizeof(VkAttachmentReference2KHR) * SubpassDescriptions[Idx].inputAttachmentCount;
				}

				bool bHasResolveAttachment;
				COPY_FROM_BUFFER(&bHasResolveAttachment, PSO, MemoryOffset, sizeof(bool));

				if (bHasResolveAttachment)
				{
					if (SubpassDescriptions[Idx].colorAttachmentCount > 0)
					{
						SubpassDescriptions[Idx].pResolveAttachments = (VkAttachmentReference2KHR*)&PSO[MemoryOffset];
						MemoryOffset += sizeof(VkAttachmentReference2KHR) * SubpassDescriptions[Idx].colorAttachmentCount;
					}
				}

				bool bHasDepthStencilAttachment;
				COPY_FROM_BUFFER(&bHasDepthStencilAttachment, PSO, MemoryOffset, sizeof(bool));

				if (bHasDepthStencilAttachment)
				{
					SubpassDescriptions[Idx].pDepthStencilAttachment = (VkAttachmentReference2KHR*)&PSO[MemoryOffset];
					MemoryOffset += sizeof(VkAttachmentReference2KHR);
				}
			}
			RenderPassCreateInfo.pSubpasses = SubpassDescriptions;

			if (RenderPassCreateInfo.correlatedViewMaskCount > 0)
			{
				RenderPassCreateInfo.pCorrelatedViewMasks = (uint32_t*)&PSO[MemoryOffset];
				MemoryOffset += sizeof(uint32_t) * RenderPassCreateInfo.correlatedViewMaskCount;
			}

			Result = vkCreateRenderPass2(Device, &RenderPassCreateInfo, nullptr, &RenderPass);
			if (Result != VK_SUCCESS)
			{
				__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " vkCreateRenderPass2 Failed %d ", Result);
				exit(-1);
			}

			delete[] SubpassDescriptions;
		}
		else
		{
			// Render pass
			VkRenderPassCreateInfo RenderPassCreateInfo;
			COPY_FROM_BUFFER(&RenderPassCreateInfo, PSO, MemoryOffset, sizeof(VkRenderPassCreateInfo));

			// Check for VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT
			bool bHasCreateInfoNext = false;
			COPY_FROM_BUFFER(&bHasCreateInfoNext, PSO, MemoryOffset, sizeof(bool));

			if (bHasCreateInfoNext)
			{
				RenderPassCreateInfo.pNext = &PSO[MemoryOffset];
				MemoryOffset += sizeof(VkRenderPassFragmentDensityMapCreateInfoEXT);
			}

			if (RenderPassCreateInfo.attachmentCount > 0)
			{
				RenderPassCreateInfo.pAttachments = (VkAttachmentDescription*)&PSO[MemoryOffset];
				MemoryOffset += sizeof(VkAttachmentDescription) * RenderPassCreateInfo.attachmentCount;
			}

			if (RenderPassCreateInfo.dependencyCount > 0)
			{
				RenderPassCreateInfo.pDependencies = (VkSubpassDependency*)&PSO[MemoryOffset];
				MemoryOffset += sizeof(VkSubpassDependency) * RenderPassCreateInfo.dependencyCount;
			}

			VkSubpassDescription* SubpassDescriptions = new VkSubpassDescription[RenderPassCreateInfo.subpassCount];

			for (uint32_t Idx = 0; Idx < RenderPassCreateInfo.subpassCount; ++Idx)
			{
				COPY_FROM_BUFFER(&SubpassDescriptions[Idx], PSO, MemoryOffset, sizeof(VkSubpassDescription));

				if (SubpassDescriptions[Idx].colorAttachmentCount > 0)
				{
					SubpassDescriptions[Idx].pColorAttachments = (VkAttachmentReference*)&PSO[MemoryOffset];
					MemoryOffset += sizeof(VkAttachmentReference) * SubpassDescriptions[Idx].colorAttachmentCount;
				}

				if (SubpassDescriptions[Idx].inputAttachmentCount > 0)
				{
					SubpassDescriptions[Idx].pInputAttachments = (VkAttachmentReference*)&PSO[MemoryOffset];
					MemoryOffset += sizeof(VkAttachmentReference) * SubpassDescriptions[Idx].inputAttachmentCount;
				}

				bool bHasResolveAttachment;
				COPY_FROM_BUFFER(&bHasResolveAttachment, PSO, MemoryOffset, sizeof(bool));

				if (bHasResolveAttachment)
				{
					if (SubpassDescriptions[Idx].colorAttachmentCount > 0)
					{
						SubpassDescriptions[Idx].pResolveAttachments = (VkAttachmentReference*)&PSO[MemoryOffset];
						MemoryOffset += sizeof(VkAttachmentReference) * SubpassDescriptions[Idx].colorAttachmentCount;
					}
				}

				bool bHasDepthStencilAttachment;
				COPY_FROM_BUFFER(&bHasDepthStencilAttachment, PSO, MemoryOffset, sizeof(bool));

				if (bHasDepthStencilAttachment)
				{
					SubpassDescriptions[Idx].pDepthStencilAttachment = (VkAttachmentReference*)&PSO[MemoryOffset];
					MemoryOffset += sizeof(VkAttachmentReference);
				}
			}
			RenderPassCreateInfo.pSubpasses = SubpassDescriptions;

			Result = vkCreateRenderPass(Device, &RenderPassCreateInfo, nullptr, &RenderPass);
			if (Result != VK_SUCCESS)
			{
				__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " vkCreateRenderPass2 Failed %d ", Result);
				exit(-1);
			}

			delete[] SubpassDescriptions;
		}

		CreateInfo.renderPass = RenderPass;

		VkPipeline Pipeline;
		VkShaderModule VSModule;
		VkShaderModule PSModule;

		{
			VkShaderModuleCreateInfo ModuleCreateInfo;
			ModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			ModuleCreateInfo.pCode = (const uint32_t *)VS;
			ModuleCreateInfo.codeSize = VSSize;
			ModuleCreateInfo.flags = 0;
			ModuleCreateInfo.pNext = nullptr;

			Result = vkCreateShaderModule(Device, &ModuleCreateInfo, nullptr, &VSModule);
			if (Result != VK_SUCCESS)
			{
				__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " vkCreateShaderModule VS Failed %d ", Result);
				exit(-1);
			}

			ShaderStages[0].module = VSModule;
		}

		{
			VkShaderModuleCreateInfo ModuleCreateInfo;
			ModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			ModuleCreateInfo.pCode = (const uint32_t*)PS;
			ModuleCreateInfo.codeSize = PSSize;
			ModuleCreateInfo.flags = 0;
			ModuleCreateInfo.pNext = nullptr;

			Result = vkCreateShaderModule(Device, &ModuleCreateInfo, nullptr, &PSModule);
			if (Result != VK_SUCCESS)
			{
				__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " vkCreateShaderModule PS Failed %d ", Result);
				exit(-1);
			}

			ShaderStages[1].module = PSModule;
		}

		if (PipelineCache == VK_NULL_HANDLE)
		{
			VkPipelineCacheCreateInfo PipelineCacheCreateInfo;
			memset(&PipelineCacheCreateInfo, 0, sizeof(VkPipelineCacheCreateInfo));
			PipelineCacheCreateInfo.flags = 0;
			PipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			Result = vkCreatePipelineCache(Device, &PipelineCacheCreateInfo, nullptr, &PipelineCache);
			if (Result != VK_SUCCESS)
			{
				__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " vkCreatePipelineCache Failed %d ", Result);
				exit(-1);
			}
		}

		Result = vkCreateGraphicsPipelines(Device, PipelineCache, 1, &CreateInfo, nullptr, &Pipeline);
		if (Result != VK_SUCCESS)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " vkCreateGraphicsPipelines Failed %d ", Result);
			exit(-1);
		}

		for (uint32_t Idx = 0; Idx < PipelineLayoutCreateInfo.setLayoutCount; ++Idx)
		{
			vkDestroyDescriptorSetLayout(Device, DescriptorSetLayouts[Idx], nullptr);
		}

		vkDestroyShaderModule(Device, VSModule, nullptr);
		vkDestroyShaderModule(Device, PSModule, nullptr);
		vkDestroyRenderPass(Device, RenderPass, nullptr);
		vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
		vkDestroyPipeline(Device, Pipeline, nullptr);

		if (DescriptorSetLayoutInfos)
		{
			delete[] DescriptorSetLayoutInfos;
		}
		if (DescriptorSetLayouts)
		{
			delete[] DescriptorSetLayouts;
		}

		return errorLog;
	}

	void GetPSOBinary(char* & BinaryData, uint32_t& Size, bool bCompileThread)
	{
		if (bCompileThread && !bSinglePSOPerCache)
		{
			Size = 0;
			BinaryData = nullptr;
			return;
		}

		if (PSOCacheData)
		{
			free(PSOCacheData);
			PSOCacheData = nullptr;
			PSOCacheSize = 0;
		}

		PSOCacheSize = 0;
		VkResult Result = vkGetPipelineCacheData(Device, PipelineCache, &PSOCacheSize, nullptr);
		if (Result != VK_SUCCESS)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " vkGetPipelineCacheData 1 Failed %d ", Result);
			exit(-1);
		}

		PSOCacheData = (char*)malloc(PSOCacheSize);
		Result = vkGetPipelineCacheData(Device, PipelineCache, &PSOCacheSize, PSOCacheData);
		if (Result != VK_SUCCESS)
		{
			__android_log_print(ANDROID_LOG_VERBOSE, APPNAME, " vkGetPipelineCacheData 2 Failed %d ", Result);
			exit(-1);
		}

		Size = PSOCacheSize;
		BinaryData = PSOCacheData;
	}
};

JNI_METHOD void Java_com_epicgames_unreal_psoservices_PSOProgramService_InitVKDevice(JNIEnv* jenv, jobject thiz)
{

}

JNI_METHOD void Java_com_epicgames_unreal_psoservices_PSOProgramService_ShutdownVKDevice(JNIEnv* jenv, jobject thiz)
{
	FVulkanPSOCompiler::Get().ShutDownDevice();
}

JNI_METHOD jobject Java_com_epicgames_unreal_psoservices_PSOProgramService_CompileVKGFXPSO(JNIEnv* jenv, jobject thiz, jbyteArray jVS, jbyteArray jPS, jbyteArray jPSO)
{
	const uint8_t* VS = (const uint8_t*)jenv->GetByteArrayElements(jVS, nullptr);
	uint64_t VSSize = jenv->GetArrayLength(jVS);
	const uint8_t* PS = (const uint8_t*)jenv->GetByteArrayElements(jPS, nullptr);
	uint64_t PSSize = jenv->GetArrayLength(jPS);
	const uint8_t* PSO = (const uint8_t*)jenv->GetByteArrayElements(jPSO, nullptr);
	uint64_t PSOSize = jenv->GetArrayLength(jPSO);
;	
	FVulkanPSOCompiler::Get().CompileGFXPSO(VS, VSSize, PS, PSSize, PSO, PSOSize);

	char* BinaryData;
	uint32_t Size = 0;

	FVulkanPSOCompiler::Get().GetPSOBinary(BinaryData, Size, true);

	jbyteArray Data = jenv->NewByteArray(Size);

	if (Size > 0)
	{
		jenv->SetByteArrayRegion(Data, 0, Size, (jbyte*)BinaryData);
	}

	return Data;
}

JNI_METHOD jobject Java_com_epicgames_unreal_psoservices_PSOProgramService_GetVKPSOCacheData(JNIEnv* jenv, jobject thiz)
{
	char* BinaryData;
	uint32_t Size = 0;

	FVulkanPSOCompiler::Get().GetPSOBinary(BinaryData, Size, false);

	jbyteArray Data = jenv->NewByteArray(Size);

	if (Size > 0)
	{
		jenv->SetByteArrayRegion(Data, 0, Size, (jbyte*)BinaryData);
	}

	return Data;
}