#ifndef __MPL_VkRenderParameters_H_
#define __MPL_VkRenderParameters_H_

#ifdef EASYBLENDSDK_GRAPHICS_API_VK

#include "vulkan/vulkan.h"

const unsigned int EasyBlendSDK_VK_SDKVersion = 1;

enum EasyBlendSDK_VK_InputType
{
  EASYBLENDSDK_VK_INPUT_TYPE_UNDEFINED = 0,
  EASYBLENDSDK_VK_INPUT_TYPE_TRANSFER,
  EASYBLENDSDK_VK_INPUT_TYPE_SAMPLE
};

struct EasyBlendSDK_VK_InputTransferData
{
  // We need to know your rendered images so we can copy them to be warped.
  int      sourceImageCount;
  VkImage* sourceImages;
  // We want to make sure we use pipeline barriers transition the source images to legal layouts
  // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT is required
  // VK_IMAGE_USAGE_TRANSFER_SRC_BIT is required
  VkImageUsageFlags sourceImageUsageFlags;
};

struct EasyBlendSDK_VK_InputSampleData
{
  // Images to sample
  int          sourceSampledImageViewCount;
  VkImageView* sourceSampledImageViews;
  // Optional: the images bound to the image views
  // if given, we will add pipeline barriers to transition images if not in the correct layout
  // We will expect the images to be in the VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  VkImage*     sourceImages;
  // We want to make sure we use pipeline barriers transition the source images to legal layouts
  // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT is required
  // VK_IMAGE_USAGE_SAMPLED_BIT is required
  VkImageUsageFlags sourceImageUsageFlags;
};

enum EasyBlendSDK_VK_OutputType
{
  EASYBLENDSDK_VK_OUTPUT_TYPE_UNDEFINED = 0,
  EASYBLENDSDK_VK_OUTPUT_TYPE_TRANSFER,
  EASYBLENDSDK_VK_OUTPUT_TYPE_FRAMEBUFFER
};

struct EasyBlendSDK_VK_OutputTransferData
{
  // The images to transfer out to 
  // this will be assumed to be an array of the same size as the input count
  VkImage* dstImages;

  // Mark if it's the same as the source images.
  // This is because we'll need to add some PipelineBarriers to transition to the correct layout.
  bool isTheSameAsSourceImages;

  // We want to make sure we transition the destination images to legal layouts
  // VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT is required
  // VK_IMAGE_USAGE_TRANSFER_DST_BIT is required
  VkImageUsageFlags dstImageUsageFlags;
};

struct EasyBlendSDK_VK_OutputFramebufferData
{
  // Framebuffers to render to
  // Please have a depth image attached.
  // this will be assumed to be an array of the same size as the input count
  VkFramebuffer* framebuffers;

  // Let us know if framebuffers you provided have the source images attached.
  // This lets us know that our pipeline will change the layout of the source images
  bool framebuffersHaveSourceImagesBound;

  // If Framebuffers are used, please specify the layout you'd like your framebuffers to be in after rendering
  VkImageLayout framebufferOutputLayout;
};


struct EasyBlendSDK_VK_InputInfo
{
  EasyBlendSDK_VK_InputType inputType;

  union
  {
    EasyBlendSDK_VK_InputTransferData inputTransferData;
    EasyBlendSDK_VK_InputSampleData inputSampleData;
  } inputData;
};


struct EasyBlendSDK_VK_OutputInfo
{
  EasyBlendSDK_VK_OutputType outputType;

  union
  {
    EasyBlendSDK_VK_OutputTransferData outputTransferData;
    EasyBlendSDK_VK_OutputFramebufferData outputFramebufferData;
  } outputData;
};


struct EasyBlendSDK_VK_InitializeParameters
{
  const unsigned int version = EasyBlendSDK_VK_SDKVersion;

  // We need devices to create our own resources
  VkDevice logicalDevice;
  VkPhysicalDevice phyiscalDevice;

  // To create a renderpass, we need to know what pixel format the images use
  VkFormat imageFormat;
  // To render in a pipeline, we also need to know the extent of the image
  VkExtent2D imageExtent;

  EasyBlendSDK_VK_InputInfo inputInfo;
  EasyBlendSDK_VK_OutputInfo outputInfo;
};

struct EasyBlendSDK_VK_RenderParameters
{
  // the command buffer to write to
  VkCommandBuffer commandBuffer;

  // Which image we should warp, specified by the index into the array of images sent when initializing
  int imageIndex;

  // If you gave us source images, we can transition them to relevant layouts through PipelineBarriers.
  // If the source images are bound in some way to the output data, we WILL apply PipelineBarriers.

  // If the source images are in some way related to the dest images:
  // 
  //    If you are using EASYBLENDSDK_VK_INPUT_TYPE_TRANSFER:
  //           and using EASYBLENDSDK_VK_OUTPUT_TYPE_TRANSFER:
  //               We will expect to receive the source image in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, otherwise we will transition it
  //               We will transition it to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to copy the image out
  //               If no final layout is provided, the image will be handed back to the user in the layout VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
  //
  //    If you are using EASYBLENDSDK_VK_INPUT_TYPE_TRANSFER:
  //           and using EASYBLENDSDK_VK_OUTPUT_TYPE_FRAMEBUFFER:
  //               We will expect to receive the source image in VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, otherwise we will transition it
  //               We will transition the source image when we render to the framebuffer
  //               If no final layout is provided, the underlying image in the Framebuffer will be in the layout specified in the initialization parameters
  //
  //    If you are using EASYBLENDSDK_VK_INPUT_TYPE_SAMPLE:
  //           and using EASYBLENDSDK_VK_OUTPUT_TYPE_TRANSFER:
  //               We will expect to receive the source image in VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, otherwise we will transition it
  //               We will transition it to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL to copy the image out
  //               If no final layout is provided, the image will be handed back to the user in the layout VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
  //
  //    If you are using EASYBLENDSDK_VK_INPUT_TYPE_SAMPLE:
  //           and using EASYBLENDSDK_VK_OUTPUT_TYPE_FRAMEBUFFER:
  //               THIS WILL NOT WORK
  //               VULKAN WILL CRASH
  //               Using the same image as sampled and in a render pass will result in complaints from Vulkan

  // If the source images are in some way related to the destination images:
  // please provide the current layout of the images, so they may be transitioned correctly during the warp

  // We can apply a PipelineBarrier to transition the image layout if it is currently not in the layout we expect
  // if your current layout meets the layout we wish to transition to, no PipelineBarrier will be applied
  bool allowInitialPipelineBarrier;
  // The layout the sourceImage is in when handed to EasyBlendSDK
  VkImageLayout currentLayout;

  // If the output type is EASYBLENDSDK_VK_OUTPUT_TYPE_TRANSFER, we can apply a final pipeline barrier to transition it to a layout of your choosing
  bool allowFinalPipelineBarrier;
  // The layout you want the sourceImage to be in when returned from EasyBlendSDK
  VkImageLayout finalLayout;

  // The Image Layout PipelineBarriers that are currently supported are listed below
  //
  // VK_IMAGE_LAYOUT_PRESENT_SRC_KHR          -> VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
  //                                          -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
  //
  // VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL     -> VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
  // 
  // VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL -> VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
};

#endif // EASYBLENDSDK_GRAPHICS_API_VK

#endif // !__MPL_VkRenderParameters_H_