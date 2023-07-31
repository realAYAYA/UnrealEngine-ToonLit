/*
 * Copyright 2017-2018 NVIDIA Corporation.  All rights reserved.
 *
 * NOTICE TO LICENSEE:
 *
 * This source code and/or documentation ("Licensed Deliverables") are
 * subject to NVIDIA intellectual property rights under U.S. and
 * international Copyright laws.
 *
 * These Licensed Deliverables contained herein is PROPRIETARY and
 * CONFIDENTIAL to NVIDIA and is being provided under the terms and
 * conditions of a form of NVIDIA software license agreement by and
 * between NVIDIA and Licensee ("License Agreement") or electronically
 * accepted by Licensee.  Notwithstanding any terms or conditions to
 * the contrary in the License Agreement, reproduction or disclosure
 * of the Licensed Deliverables to any third party without the express
 * written consent of NVIDIA is prohibited.
 *
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, NVIDIA MAKES NO REPRESENTATION ABOUT THE
 * SUITABILITY OF THESE LICENSED DELIVERABLES FOR ANY PURPOSE.  IT IS
 * PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY OF ANY KIND.
 * NVIDIA DISCLAIMS ALL WARRANTIES WITH REGARD TO THESE LICENSED
 * DELIVERABLES, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY,
 * NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE.
 * NOTWITHSTANDING ANY TERMS OR CONDITIONS TO THE CONTRARY IN THE
 * LICENSE AGREEMENT, IN NO EVENT SHALL NVIDIA BE LIABLE FOR ANY
 * SPECIAL, INDIRECT, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, OR ANY
 * DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THESE LICENSED DELIVERABLES.
 *
 * U.S. Government End Users.  These Licensed Deliverables are a
 * "commercial item" as that term is defined at 48 C.F.R. 2.101 (OCT
 * 1995), consisting of "commercial computer software" and "commercial
 * computer software documentation" as such terms are used in 48
 * C.F.R. 12.212 (SEPT 1995) and is provided to the U.S. Government
 * only as a commercial end item.  Consistent with 48 C.F.R.12.212 and
 * 48 C.F.R. 227.7202-1 through 227.7202-4 (JUNE 1995), all
 * U.S. Government End Users acquire the Licensed Deliverables with
 * only those rights set forth herein.
 *
 * Any use of the Licensed Deliverables in individual and commercial
 * software must include, in the user documentation and internal
 * comments to the code, the above Disclaimer and U.S. Government End
 * Users Notice.
 */

#ifndef __DVAPI_VULKAN_H_
#define __DVAPI_VULKAN_H_

#include <vulkan/vulkan.h>
#include "DVPAPI.h"

DVPAPI_INTERFACE
dvpInitVkDevice(VkDevice device,
                uint32_t flags,
                void* pDeviceUUID);

DVPAPI_INTERFACE
dvpCloseVkDevice(VkDevice device);

DVPAPI_INTERFACE
dvpBindToVkDevice(DVPBufferHandle hBuf, 
                  VkDevice device);

DVPAPI_INTERFACE
dvpUnbindFromVkDevice(DVPBufferHandle hBuf, 
                      VkDevice device);

DVPAPI_INTERFACE
dvpCreateGPUExternalResourceVkDevice(VkDevice device,
                                     DVPGpuExternalResourceDesc *desc,
                                     DVPBufferHandle *bufferHandle);

DVPAPI_INTERFACE
dvpGetRequiredConstantsVkDevice(uint32_t *bufferAddrAlignment,
                                uint32_t *bufferGPUStrideAlignment,
                                uint32_t *semaphoreAddrAlignment,
                                uint32_t *semaphoreAllocSize,
                                uint32_t *semaphorePayloadOffset,
                                uint32_t *semaphorePayloadSize,
                                VkDevice device);

//------------------------------------------------------------------------
// Function:      dvpMapBufferWaitVk
//
// Description:   dvpMapBufferWaitVk peforms the same function as
//                dvpMapBufferWaitAPI, but allows the specification of which
//                vulkan device queue will next use the buffer.
//
// Parameters:    gpuBufferHandle[IN]       - buffer to synchorise with DVP
//                queue[IN]             -     device queue in which buffer 
//                                            is used
//
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//------------------------------------------------------------------------

DVPAPI_INTERFACE
dvpMapBufferWaitVk(DVPBufferHandle gpuBufferHandle,
                   VkQueue queue);

//------------------------------------------------------------------------
// Function:      dvpMapBufferEndVk
//
// Description:   dvpMapBufferEndVk peforms the same function as
//                dvpMapBufferEndAPI, but allows the specification of which
//                vulkan device queue the buffer has been used.
//
// Parameters:    gpuBufferHandle[IN]       - buffer to synchorise with DVP
//                queue[IN]             -     device queue in which buffer 
//                                            has been used
//
// Returns:       DVP_STATUS_OK
//                DVP_STATUS_INVALID_PARAMETER
//                DVP_STATUS_ERROR
//------------------------------------------------------------------------

DVPAPI_INTERFACE
dvpMapBufferEndVk(DVPBufferHandle gpuBufferHandle,
                  VkQueue queue);

#endif

