// Copyright Epic Games, Inc. All Rights Reserved.
// This file was automatically generated. DO NOT EDIT!
// Generated from cuda.h v11.2.2 sha256:8a2a2402d8e2497e87147b407d2cd15cd51018d050c521bbcd95ed1406203c86

#ifndef _CUDA_WRAPPER_H
#define _CUDA_WRAPPER_H

#include "cuda.h"

#ifdef __cplusplus
extern "C" {
#endif

// Undefine the macros that cuda.h uses to redirect function calls to their versioned counterparts
#ifdef cuArray3DCreate
#undef cuArray3DCreate
#endif
#ifdef cuArray3DGetDescriptor
#undef cuArray3DGetDescriptor
#endif
#ifdef cuArrayCreate
#undef cuArrayCreate
#endif
#ifdef cuArrayGetDescriptor
#undef cuArrayGetDescriptor
#endif
#ifdef cuCtxCreate
#undef cuCtxCreate
#endif
#ifdef cuCtxDestroy
#undef cuCtxDestroy
#endif
#ifdef cuCtxPopCurrent
#undef cuCtxPopCurrent
#endif
#ifdef cuCtxPushCurrent
#undef cuCtxPushCurrent
#endif
#ifdef cuDevicePrimaryCtxRelease
#undef cuDevicePrimaryCtxRelease
#endif
#ifdef cuDevicePrimaryCtxReset
#undef cuDevicePrimaryCtxReset
#endif
#ifdef cuDevicePrimaryCtxSetFlags
#undef cuDevicePrimaryCtxSetFlags
#endif
#ifdef cuDeviceTotalMem
#undef cuDeviceTotalMem
#endif
#ifdef cuEventDestroy
#undef cuEventDestroy
#endif
#ifdef cuEventRecord
#undef cuEventRecord
#endif
#ifdef cuEventRecordWithFlags
#undef cuEventRecordWithFlags
#endif
#ifdef cuGraphInstantiate
#undef cuGraphInstantiate
#endif
#ifdef cuGraphLaunch
#undef cuGraphLaunch
#endif
#ifdef cuGraphUpload
#undef cuGraphUpload
#endif
#ifdef cuGraphicsMapResources
#undef cuGraphicsMapResources
#endif
#ifdef cuGraphicsResourceGetMappedPointer
#undef cuGraphicsResourceGetMappedPointer
#endif
#ifdef cuGraphicsResourceSetMapFlags
#undef cuGraphicsResourceSetMapFlags
#endif
#ifdef cuGraphicsUnmapResources
#undef cuGraphicsUnmapResources
#endif
#ifdef cuIpcOpenMemHandle
#undef cuIpcOpenMemHandle
#endif
#ifdef cuLaunchCooperativeKernel
#undef cuLaunchCooperativeKernel
#endif
#ifdef cuLaunchHostFunc
#undef cuLaunchHostFunc
#endif
#ifdef cuLaunchKernel
#undef cuLaunchKernel
#endif
#ifdef cuLinkAddData
#undef cuLinkAddData
#endif
#ifdef cuLinkAddFile
#undef cuLinkAddFile
#endif
#ifdef cuLinkCreate
#undef cuLinkCreate
#endif
#ifdef cuMemAlloc
#undef cuMemAlloc
#endif
#ifdef cuMemAllocAsync
#undef cuMemAllocAsync
#endif
#ifdef cuMemAllocFromPoolAsync
#undef cuMemAllocFromPoolAsync
#endif
#ifdef cuMemAllocHost
#undef cuMemAllocHost
#endif
#ifdef cuMemAllocPitch
#undef cuMemAllocPitch
#endif
#ifdef cuMemFree
#undef cuMemFree
#endif
#ifdef cuMemFreeAsync
#undef cuMemFreeAsync
#endif
#ifdef cuMemGetAddressRange
#undef cuMemGetAddressRange
#endif
#ifdef cuMemGetInfo
#undef cuMemGetInfo
#endif
#ifdef cuMemHostGetDevicePointer
#undef cuMemHostGetDevicePointer
#endif
#ifdef cuMemHostRegister
#undef cuMemHostRegister
#endif
#ifdef cuMemMapArrayAsync
#undef cuMemMapArrayAsync
#endif
#ifdef cuMemPrefetchAsync
#undef cuMemPrefetchAsync
#endif
#ifdef cuMemcpy
#undef cuMemcpy
#endif
#ifdef cuMemcpy2D
#undef cuMemcpy2D
#endif
#ifdef cuMemcpy2DAsync
#undef cuMemcpy2DAsync
#endif
#ifdef cuMemcpy2DUnaligned
#undef cuMemcpy2DUnaligned
#endif
#ifdef cuMemcpy3D
#undef cuMemcpy3D
#endif
#ifdef cuMemcpy3DAsync
#undef cuMemcpy3DAsync
#endif
#ifdef cuMemcpy3DPeer
#undef cuMemcpy3DPeer
#endif
#ifdef cuMemcpy3DPeerAsync
#undef cuMemcpy3DPeerAsync
#endif
#ifdef cuMemcpyAsync
#undef cuMemcpyAsync
#endif
#ifdef cuMemcpyAtoA
#undef cuMemcpyAtoA
#endif
#ifdef cuMemcpyAtoD
#undef cuMemcpyAtoD
#endif
#ifdef cuMemcpyAtoH
#undef cuMemcpyAtoH
#endif
#ifdef cuMemcpyAtoHAsync
#undef cuMemcpyAtoHAsync
#endif
#ifdef cuMemcpyDtoA
#undef cuMemcpyDtoA
#endif
#ifdef cuMemcpyDtoD
#undef cuMemcpyDtoD
#endif
#ifdef cuMemcpyDtoDAsync
#undef cuMemcpyDtoDAsync
#endif
#ifdef cuMemcpyDtoH
#undef cuMemcpyDtoH
#endif
#ifdef cuMemcpyDtoHAsync
#undef cuMemcpyDtoHAsync
#endif
#ifdef cuMemcpyHtoA
#undef cuMemcpyHtoA
#endif
#ifdef cuMemcpyHtoAAsync
#undef cuMemcpyHtoAAsync
#endif
#ifdef cuMemcpyHtoD
#undef cuMemcpyHtoD
#endif
#ifdef cuMemcpyHtoDAsync
#undef cuMemcpyHtoDAsync
#endif
#ifdef cuMemcpyPeer
#undef cuMemcpyPeer
#endif
#ifdef cuMemcpyPeerAsync
#undef cuMemcpyPeerAsync
#endif
#ifdef cuMemsetD16
#undef cuMemsetD16
#endif
#ifdef cuMemsetD16Async
#undef cuMemsetD16Async
#endif
#ifdef cuMemsetD2D16
#undef cuMemsetD2D16
#endif
#ifdef cuMemsetD2D16Async
#undef cuMemsetD2D16Async
#endif
#ifdef cuMemsetD2D32
#undef cuMemsetD2D32
#endif
#ifdef cuMemsetD2D32Async
#undef cuMemsetD2D32Async
#endif
#ifdef cuMemsetD2D8
#undef cuMemsetD2D8
#endif
#ifdef cuMemsetD2D8Async
#undef cuMemsetD2D8Async
#endif
#ifdef cuMemsetD32
#undef cuMemsetD32
#endif
#ifdef cuMemsetD32Async
#undef cuMemsetD32Async
#endif
#ifdef cuMemsetD8
#undef cuMemsetD8
#endif
#ifdef cuMemsetD8Async
#undef cuMemsetD8Async
#endif
#ifdef cuModuleGetGlobal
#undef cuModuleGetGlobal
#endif
#ifdef cuSignalExternalSemaphoresAsync
#undef cuSignalExternalSemaphoresAsync
#endif
#ifdef cuStreamAddCallback
#undef cuStreamAddCallback
#endif
#ifdef cuStreamAttachMemAsync
#undef cuStreamAttachMemAsync
#endif
#ifdef cuStreamBatchMemOp
#undef cuStreamBatchMemOp
#endif
#ifdef cuStreamBeginCapture
#undef cuStreamBeginCapture
#endif
#ifdef cuStreamCopyAttributes
#undef cuStreamCopyAttributes
#endif
#ifdef cuStreamDestroy
#undef cuStreamDestroy
#endif
#ifdef cuStreamEndCapture
#undef cuStreamEndCapture
#endif
#ifdef cuStreamGetAttribute
#undef cuStreamGetAttribute
#endif
#ifdef cuStreamGetCaptureInfo
#undef cuStreamGetCaptureInfo
#endif
#ifdef cuStreamGetCtx
#undef cuStreamGetCtx
#endif
#ifdef cuStreamGetFlags
#undef cuStreamGetFlags
#endif
#ifdef cuStreamGetPriority
#undef cuStreamGetPriority
#endif
#ifdef cuStreamIsCapturing
#undef cuStreamIsCapturing
#endif
#ifdef cuStreamQuery
#undef cuStreamQuery
#endif
#ifdef cuStreamSetAttribute
#undef cuStreamSetAttribute
#endif
#ifdef cuStreamSynchronize
#undef cuStreamSynchronize
#endif
#ifdef cuStreamWaitEvent
#undef cuStreamWaitEvent
#endif
#ifdef cuStreamWaitValue32
#undef cuStreamWaitValue32
#endif
#ifdef cuStreamWaitValue64
#undef cuStreamWaitValue64
#endif
#ifdef cuStreamWriteValue32
#undef cuStreamWriteValue32
#endif
#ifdef cuStreamWriteValue64
#undef cuStreamWriteValue64
#endif
#ifdef cuTexRefGetAddress
#undef cuTexRefGetAddress
#endif
#ifdef cuTexRefSetAddress
#undef cuTexRefSetAddress
#endif
#ifdef cuTexRefSetAddress2D
#undef cuTexRefSetAddress2D
#endif
#ifdef cuWaitExternalSemaphoresAsync
#undef cuWaitExternalSemaphoresAsync
#endif

// Function pointer types for CUDA Driver API functions
typedef CUresult (CUDAAPI *PFNCUGETERRORSTRING) (CUresult error, const char **pStr);
typedef CUresult (CUDAAPI *PFNCUGETERRORNAME) (CUresult error, const char **pStr);
typedef CUresult (CUDAAPI *PFNCUINIT) (unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUDRIVERGETVERSION) (int *driverVersion);
typedef CUresult (CUDAAPI *PFNCUDEVICEGET) (CUdevice *device, int ordinal);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETCOUNT) (int *count);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETNAME) (char *name, int len, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETUUID) (CUuuid *uuid, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETLUID) (char *luid, unsigned int *deviceNodeMask, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICETOTALMEM) (size_t *bytes, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETTEXTURE1DLINEARMAXWIDTH) (size_t *maxWidthInElements, CUarray_format format, unsigned numChannels, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETATTRIBUTE) (int *pi, CUdevice_attribute attrib, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETNVSCISYNCATTRIBUTES) (void *nvSciSyncAttrList, CUdevice dev, int flags);
typedef CUresult (CUDAAPI *PFNCUDEVICESETMEMPOOL) (CUdevice dev, CUmemoryPool pool);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETMEMPOOL) (CUmemoryPool *pool, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETDEFAULTMEMPOOL) (CUmemoryPool *pool_out, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETPROPERTIES) (CUdevprop *prop, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICECOMPUTECAPABILITY) (int *major, int *minor, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICEPRIMARYCTXRETAIN) (CUcontext *pctx, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICEPRIMARYCTXRELEASE) (CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUDEVICEPRIMARYCTXSETFLAGS) (CUdevice dev, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUDEVICEPRIMARYCTXGETSTATE) (CUdevice dev, unsigned int *flags, int *active);
typedef CUresult (CUDAAPI *PFNCUDEVICEPRIMARYCTXRESET) (CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUCTXCREATE) (CUcontext *pctx, unsigned int flags, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUCTXDESTROY) (CUcontext ctx);
typedef CUresult (CUDAAPI *PFNCUCTXPUSHCURRENT) (CUcontext ctx);
typedef CUresult (CUDAAPI *PFNCUCTXPOPCURRENT) (CUcontext *pctx);
typedef CUresult (CUDAAPI *PFNCUCTXSETCURRENT) (CUcontext ctx);
typedef CUresult (CUDAAPI *PFNCUCTXGETCURRENT) (CUcontext *pctx);
typedef CUresult (CUDAAPI *PFNCUCTXGETDEVICE) (CUdevice *device);
typedef CUresult (CUDAAPI *PFNCUCTXGETFLAGS) (unsigned int *flags);
typedef CUresult (CUDAAPI *PFNCUCTXSYNCHRONIZE) (void);
typedef CUresult (CUDAAPI *PFNCUCTXSETLIMIT) (CUlimit limit, size_t value);
typedef CUresult (CUDAAPI *PFNCUCTXGETLIMIT) (size_t *pvalue, CUlimit limit);
typedef CUresult (CUDAAPI *PFNCUCTXGETCACHECONFIG) (CUfunc_cache *pconfig);
typedef CUresult (CUDAAPI *PFNCUCTXSETCACHECONFIG) (CUfunc_cache config);
typedef CUresult (CUDAAPI *PFNCUCTXGETSHAREDMEMCONFIG) (CUsharedconfig *pConfig);
typedef CUresult (CUDAAPI *PFNCUCTXSETSHAREDMEMCONFIG) (CUsharedconfig config);
typedef CUresult (CUDAAPI *PFNCUCTXGETAPIVERSION) (CUcontext ctx, unsigned int *version);
typedef CUresult (CUDAAPI *PFNCUCTXGETSTREAMPRIORITYRANGE) (int *leastPriority, int *greatestPriority);
typedef CUresult (CUDAAPI *PFNCUCTXRESETPERSISTINGL2CACHE) (void);
typedef CUresult (CUDAAPI *PFNCUCTXATTACH) (CUcontext *pctx, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUCTXDETACH) (CUcontext ctx);
typedef CUresult (CUDAAPI *PFNCUMODULELOAD) (CUmodule *module, const char *fname);
typedef CUresult (CUDAAPI *PFNCUMODULELOADDATA) (CUmodule *module, const void *image);
typedef CUresult (CUDAAPI *PFNCUMODULELOADDATAEX) (CUmodule *module, const void *image, unsigned int numOptions, CUjit_option *options, void **optionValues);
typedef CUresult (CUDAAPI *PFNCUMODULELOADFATBINARY) (CUmodule *module, const void *fatCubin);
typedef CUresult (CUDAAPI *PFNCUMODULEUNLOAD) (CUmodule hmod);
typedef CUresult (CUDAAPI *PFNCUMODULEGETFUNCTION) (CUfunction *hfunc, CUmodule hmod, const char *name);
typedef CUresult (CUDAAPI *PFNCUMODULEGETGLOBAL) (CUdeviceptr *dptr, size_t *bytes, CUmodule hmod, const char *name);
typedef CUresult (CUDAAPI *PFNCUMODULEGETTEXREF) (CUtexref *pTexRef, CUmodule hmod, const char *name);
typedef CUresult (CUDAAPI *PFNCUMODULEGETSURFREF) (CUsurfref *pSurfRef, CUmodule hmod, const char *name);
typedef CUresult (CUDAAPI *PFNCULINKCREATE) (unsigned int numOptions, CUjit_option *options, void **optionValues, CUlinkState *stateOut);
typedef CUresult (CUDAAPI *PFNCULINKADDDATA) (CUlinkState state, CUjitInputType type, void *data, size_t size, const char *name, unsigned int numOptions, CUjit_option *options, void **optionValues);
typedef CUresult (CUDAAPI *PFNCULINKADDFILE) (CUlinkState state, CUjitInputType type, const char *path, unsigned int numOptions, CUjit_option *options, void **optionValues);
typedef CUresult (CUDAAPI *PFNCULINKCOMPLETE) (CUlinkState state, void **cubinOut, size_t *sizeOut);
typedef CUresult (CUDAAPI *PFNCULINKDESTROY) (CUlinkState state);
typedef CUresult (CUDAAPI *PFNCUMEMGETINFO) (size_t *free, size_t *total);
typedef CUresult (CUDAAPI *PFNCUMEMALLOC) (CUdeviceptr *dptr, size_t bytesize);
typedef CUresult (CUDAAPI *PFNCUMEMALLOCPITCH) (CUdeviceptr *dptr, size_t *pPitch, size_t WidthInBytes, size_t Height, unsigned int ElementSizeBytes);
typedef CUresult (CUDAAPI *PFNCUMEMFREE) (CUdeviceptr dptr);
typedef CUresult (CUDAAPI *PFNCUMEMGETADDRESSRANGE) (CUdeviceptr *pbase, size_t *psize, CUdeviceptr dptr);
typedef CUresult (CUDAAPI *PFNCUMEMALLOCHOST) (void **pp, size_t bytesize);
typedef CUresult (CUDAAPI *PFNCUMEMFREEHOST) (void *p);
typedef CUresult (CUDAAPI *PFNCUMEMHOSTALLOC) (void **pp, size_t bytesize, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUMEMHOSTGETDEVICEPOINTER) (CUdeviceptr *pdptr, void *p, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUMEMHOSTGETFLAGS) (unsigned int *pFlags, void *p);
typedef CUresult (CUDAAPI *PFNCUMEMALLOCMANAGED) (CUdeviceptr *dptr, size_t bytesize, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETBYPCIBUSID) (CUdevice *dev, const char *pciBusId);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETPCIBUSID) (char *pciBusId, int len, CUdevice dev);
typedef CUresult (CUDAAPI *PFNCUIPCGETEVENTHANDLE) (CUipcEventHandle *pHandle, CUevent event);
typedef CUresult (CUDAAPI *PFNCUIPCOPENEVENTHANDLE) (CUevent *phEvent, CUipcEventHandle handle);
typedef CUresult (CUDAAPI *PFNCUIPCGETMEMHANDLE) (CUipcMemHandle *pHandle, CUdeviceptr dptr);
typedef CUresult (CUDAAPI *PFNCUIPCOPENMEMHANDLE) (CUdeviceptr *pdptr, CUipcMemHandle handle, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUIPCCLOSEMEMHANDLE) (CUdeviceptr dptr);
typedef CUresult (CUDAAPI *PFNCUMEMHOSTREGISTER) (void *p, size_t bytesize, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUMEMHOSTUNREGISTER) (void *p);
typedef CUresult (CUDAAPI *PFNCUMEMCPY) (CUdeviceptr dst, CUdeviceptr src, size_t ByteCount);
typedef CUresult (CUDAAPI *PFNCUMEMCPYPEER) (CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount);
typedef CUresult (CUDAAPI *PFNCUMEMCPYHTOD) (CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount);
typedef CUresult (CUDAAPI *PFNCUMEMCPYDTOH) (void *dstHost, CUdeviceptr srcDevice, size_t ByteCount);
typedef CUresult (CUDAAPI *PFNCUMEMCPYDTOD) (CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount);
typedef CUresult (CUDAAPI *PFNCUMEMCPYDTOA) (CUarray dstArray, size_t dstOffset, CUdeviceptr srcDevice, size_t ByteCount);
typedef CUresult (CUDAAPI *PFNCUMEMCPYATOD) (CUdeviceptr dstDevice, CUarray srcArray, size_t srcOffset, size_t ByteCount);
typedef CUresult (CUDAAPI *PFNCUMEMCPYHTOA) (CUarray dstArray, size_t dstOffset, const void *srcHost, size_t ByteCount);
typedef CUresult (CUDAAPI *PFNCUMEMCPYATOH) (void *dstHost, CUarray srcArray, size_t srcOffset, size_t ByteCount);
typedef CUresult (CUDAAPI *PFNCUMEMCPYATOA) (CUarray dstArray, size_t dstOffset, CUarray srcArray, size_t srcOffset, size_t ByteCount);
typedef CUresult (CUDAAPI *PFNCUMEMCPY2D) (const CUDA_MEMCPY2D *pCopy);
typedef CUresult (CUDAAPI *PFNCUMEMCPY2DUNALIGNED) (const CUDA_MEMCPY2D *pCopy);
typedef CUresult (CUDAAPI *PFNCUMEMCPY3D) (const CUDA_MEMCPY3D *pCopy);
typedef CUresult (CUDAAPI *PFNCUMEMCPY3DPEER) (const CUDA_MEMCPY3D_PEER *pCopy);
typedef CUresult (CUDAAPI *PFNCUMEMCPYASYNC) (CUdeviceptr dst, CUdeviceptr src, size_t ByteCount, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMCPYPEERASYNC) (CUdeviceptr dstDevice, CUcontext dstContext, CUdeviceptr srcDevice, CUcontext srcContext, size_t ByteCount, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMCPYHTODASYNC) (CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMCPYDTOHASYNC) (void *dstHost, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMCPYDTODASYNC) (CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMCPYHTOAASYNC) (CUarray dstArray, size_t dstOffset, const void *srcHost, size_t ByteCount, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMCPYATOHASYNC) (void *dstHost, CUarray srcArray, size_t srcOffset, size_t ByteCount, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMCPY2DASYNC) (const CUDA_MEMCPY2D *pCopy, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMCPY3DASYNC) (const CUDA_MEMCPY3D *pCopy, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMCPY3DPEERASYNC) (const CUDA_MEMCPY3D_PEER *pCopy, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMSETD8) (CUdeviceptr dstDevice, unsigned char uc, size_t N);
typedef CUresult (CUDAAPI *PFNCUMEMSETD16) (CUdeviceptr dstDevice, unsigned short us, size_t N);
typedef CUresult (CUDAAPI *PFNCUMEMSETD32) (CUdeviceptr dstDevice, unsigned int ui, size_t N);
typedef CUresult (CUDAAPI *PFNCUMEMSETD2D8) (CUdeviceptr dstDevice, size_t dstPitch, unsigned char uc, size_t Width, size_t Height);
typedef CUresult (CUDAAPI *PFNCUMEMSETD2D16) (CUdeviceptr dstDevice, size_t dstPitch, unsigned short us, size_t Width, size_t Height);
typedef CUresult (CUDAAPI *PFNCUMEMSETD2D32) (CUdeviceptr dstDevice, size_t dstPitch, unsigned int ui, size_t Width, size_t Height);
typedef CUresult (CUDAAPI *PFNCUMEMSETD8ASYNC) (CUdeviceptr dstDevice, unsigned char uc, size_t N, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMSETD16ASYNC) (CUdeviceptr dstDevice, unsigned short us, size_t N, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMSETD32ASYNC) (CUdeviceptr dstDevice, unsigned int ui, size_t N, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMSETD2D8ASYNC) (CUdeviceptr dstDevice, size_t dstPitch, unsigned char uc, size_t Width, size_t Height, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMSETD2D16ASYNC) (CUdeviceptr dstDevice, size_t dstPitch, unsigned short us, size_t Width, size_t Height, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMSETD2D32ASYNC) (CUdeviceptr dstDevice, size_t dstPitch, unsigned int ui, size_t Width, size_t Height, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUARRAYCREATE) (CUarray *pHandle, const CUDA_ARRAY_DESCRIPTOR *pAllocateArray);
typedef CUresult (CUDAAPI *PFNCUARRAYGETDESCRIPTOR) (CUDA_ARRAY_DESCRIPTOR *pArrayDescriptor, CUarray hArray);
typedef CUresult (CUDAAPI *PFNCUARRAYGETSPARSEPROPERTIES) (CUDA_ARRAY_SPARSE_PROPERTIES *sparseProperties, CUarray array);
typedef CUresult (CUDAAPI *PFNCUMIPMAPPEDARRAYGETSPARSEPROPERTIES) (CUDA_ARRAY_SPARSE_PROPERTIES *sparseProperties, CUmipmappedArray mipmap);
typedef CUresult (CUDAAPI *PFNCUARRAYGETPLANE) (CUarray *pPlaneArray, CUarray hArray, unsigned int planeIdx);
typedef CUresult (CUDAAPI *PFNCUARRAYDESTROY) (CUarray hArray);
typedef CUresult (CUDAAPI *PFNCUARRAY3DCREATE) (CUarray *pHandle, const CUDA_ARRAY3D_DESCRIPTOR *pAllocateArray);
typedef CUresult (CUDAAPI *PFNCUARRAY3DGETDESCRIPTOR) (CUDA_ARRAY3D_DESCRIPTOR *pArrayDescriptor, CUarray hArray);
typedef CUresult (CUDAAPI *PFNCUMIPMAPPEDARRAYCREATE) (CUmipmappedArray *pHandle, const CUDA_ARRAY3D_DESCRIPTOR *pMipmappedArrayDesc, unsigned int numMipmapLevels);
typedef CUresult (CUDAAPI *PFNCUMIPMAPPEDARRAYGETLEVEL) (CUarray *pLevelArray, CUmipmappedArray hMipmappedArray, unsigned int level);
typedef CUresult (CUDAAPI *PFNCUMIPMAPPEDARRAYDESTROY) (CUmipmappedArray hMipmappedArray);
typedef CUresult (CUDAAPI *PFNCUMEMADDRESSRESERVE) (CUdeviceptr *ptr, size_t size, size_t alignment, CUdeviceptr addr, unsigned long long flags);
typedef CUresult (CUDAAPI *PFNCUMEMADDRESSFREE) (CUdeviceptr ptr, size_t size);
typedef CUresult (CUDAAPI *PFNCUMEMCREATE) (CUmemGenericAllocationHandle *handle, size_t size, const CUmemAllocationProp *prop, unsigned long long flags);
typedef CUresult (CUDAAPI *PFNCUMEMRELEASE) (CUmemGenericAllocationHandle handle);
typedef CUresult (CUDAAPI *PFNCUMEMMAP) (CUdeviceptr ptr, size_t size, size_t offset, CUmemGenericAllocationHandle handle, unsigned long long flags);
typedef CUresult (CUDAAPI *PFNCUMEMMAPARRAYASYNC) (CUarrayMapInfo *mapInfoList, unsigned int count, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMUNMAP) (CUdeviceptr ptr, size_t size);
typedef CUresult (CUDAAPI *PFNCUMEMSETACCESS) (CUdeviceptr ptr, size_t size, const CUmemAccessDesc *desc, size_t count);
typedef CUresult (CUDAAPI *PFNCUMEMGETACCESS) (unsigned long long *flags, const CUmemLocation *location, CUdeviceptr ptr);
typedef CUresult (CUDAAPI *PFNCUMEMEXPORTTOSHAREABLEHANDLE) (void *shareableHandle, CUmemGenericAllocationHandle handle, CUmemAllocationHandleType handleType, unsigned long long flags);
typedef CUresult (CUDAAPI *PFNCUMEMIMPORTFROMSHAREABLEHANDLE) (CUmemGenericAllocationHandle *handle, void *osHandle, CUmemAllocationHandleType shHandleType);
typedef CUresult (CUDAAPI *PFNCUMEMGETALLOCATIONGRANULARITY) (size_t *granularity, const CUmemAllocationProp *prop, CUmemAllocationGranularity_flags option);
typedef CUresult (CUDAAPI *PFNCUMEMGETALLOCATIONPROPERTIESFROMHANDLE) (CUmemAllocationProp *prop, CUmemGenericAllocationHandle handle);
typedef CUresult (CUDAAPI *PFNCUMEMRETAINALLOCATIONHANDLE) (CUmemGenericAllocationHandle *handle, void *addr);
typedef CUresult (CUDAAPI *PFNCUMEMFREEASYNC) (CUdeviceptr dptr, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMALLOCASYNC) (CUdeviceptr *dptr, size_t bytesize, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMPOOLTRIMTO) (CUmemoryPool pool, size_t minBytesToKeep);
typedef CUresult (CUDAAPI *PFNCUMEMPOOLSETATTRIBUTE) (CUmemoryPool pool, CUmemPool_attribute attr, void *value);
typedef CUresult (CUDAAPI *PFNCUMEMPOOLGETATTRIBUTE) (CUmemoryPool pool, CUmemPool_attribute attr, void *value);
typedef CUresult (CUDAAPI *PFNCUMEMPOOLSETACCESS) (CUmemoryPool pool, const CUmemAccessDesc *map, size_t count);
typedef CUresult (CUDAAPI *PFNCUMEMPOOLGETACCESS) (CUmemAccess_flags *flags, CUmemoryPool memPool, CUmemLocation *location);
typedef CUresult (CUDAAPI *PFNCUMEMPOOLCREATE) (CUmemoryPool *pool, const CUmemPoolProps *poolProps);
typedef CUresult (CUDAAPI *PFNCUMEMPOOLDESTROY) (CUmemoryPool pool);
typedef CUresult (CUDAAPI *PFNCUMEMALLOCFROMPOOLASYNC) (CUdeviceptr *dptr, size_t bytesize, CUmemoryPool pool, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMPOOLEXPORTTOSHAREABLEHANDLE) (void *handle_out, CUmemoryPool pool, CUmemAllocationHandleType handleType, unsigned long long flags);
typedef CUresult (CUDAAPI *PFNCUMEMPOOLIMPORTFROMSHAREABLEHANDLE) (CUmemoryPool *pool_out, void *handle, CUmemAllocationHandleType handleType, unsigned long long flags);
typedef CUresult (CUDAAPI *PFNCUMEMPOOLEXPORTPOINTER) (CUmemPoolPtrExportData *shareData_out, CUdeviceptr ptr);
typedef CUresult (CUDAAPI *PFNCUMEMPOOLIMPORTPOINTER) (CUdeviceptr *ptr_out, CUmemoryPool pool, CUmemPoolPtrExportData *shareData);
typedef CUresult (CUDAAPI *PFNCUPOINTERGETATTRIBUTE) (void *data, CUpointer_attribute attribute, CUdeviceptr ptr);
typedef CUresult (CUDAAPI *PFNCUMEMPREFETCHASYNC) (CUdeviceptr devPtr, size_t count, CUdevice dstDevice, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUMEMADVISE) (CUdeviceptr devPtr, size_t count, CUmem_advise advice, CUdevice device);
typedef CUresult (CUDAAPI *PFNCUMEMRANGEGETATTRIBUTE) (void *data, size_t dataSize, CUmem_range_attribute attribute, CUdeviceptr devPtr, size_t count);
typedef CUresult (CUDAAPI *PFNCUMEMRANGEGETATTRIBUTES) (void **data, size_t *dataSizes, CUmem_range_attribute *attributes, size_t numAttributes, CUdeviceptr devPtr, size_t count);
typedef CUresult (CUDAAPI *PFNCUPOINTERSETATTRIBUTE) (const void *value, CUpointer_attribute attribute, CUdeviceptr ptr);
typedef CUresult (CUDAAPI *PFNCUPOINTERGETATTRIBUTES) (unsigned int numAttributes, CUpointer_attribute *attributes, void **data, CUdeviceptr ptr);
typedef CUresult (CUDAAPI *PFNCUSTREAMCREATE) (CUstream *phStream, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUSTREAMCREATEWITHPRIORITY) (CUstream *phStream, unsigned int flags, int priority);
typedef CUresult (CUDAAPI *PFNCUSTREAMGETPRIORITY) (CUstream hStream, int *priority);
typedef CUresult (CUDAAPI *PFNCUSTREAMGETFLAGS) (CUstream hStream, unsigned int *flags);
typedef CUresult (CUDAAPI *PFNCUSTREAMGETCTX) (CUstream hStream, CUcontext *pctx);
typedef CUresult (CUDAAPI *PFNCUSTREAMWAITEVENT) (CUstream hStream, CUevent hEvent, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUSTREAMADDCALLBACK) (CUstream hStream, CUstreamCallback callback, void *userData, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUSTREAMBEGINCAPTURE) (CUstream hStream, CUstreamCaptureMode mode);
typedef CUresult (CUDAAPI *PFNCUTHREADEXCHANGESTREAMCAPTUREMODE) (CUstreamCaptureMode *mode);
typedef CUresult (CUDAAPI *PFNCUSTREAMENDCAPTURE) (CUstream hStream, CUgraph *phGraph);
typedef CUresult (CUDAAPI *PFNCUSTREAMISCAPTURING) (CUstream hStream, CUstreamCaptureStatus *captureStatus);
typedef CUresult (CUDAAPI *PFNCUSTREAMGETCAPTUREINFO) (CUstream hStream, CUstreamCaptureStatus *captureStatus, cuuint64_t *id);
typedef CUresult (CUDAAPI *PFNCUSTREAMATTACHMEMASYNC) (CUstream hStream, CUdeviceptr dptr, size_t length, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUSTREAMQUERY) (CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUSTREAMSYNCHRONIZE) (CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUSTREAMDESTROY) (CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUSTREAMCOPYATTRIBUTES) (CUstream dst, CUstream src);
typedef CUresult (CUDAAPI *PFNCUSTREAMGETATTRIBUTE) (CUstream hStream, CUstreamAttrID attr, CUstreamAttrValue *value_out);
typedef CUresult (CUDAAPI *PFNCUSTREAMSETATTRIBUTE) (CUstream hStream, CUstreamAttrID attr, const CUstreamAttrValue *value);
typedef CUresult (CUDAAPI *PFNCUEVENTCREATE) (CUevent *phEvent, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUEVENTRECORD) (CUevent hEvent, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUEVENTRECORDWITHFLAGS) (CUevent hEvent, CUstream hStream, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUEVENTQUERY) (CUevent hEvent);
typedef CUresult (CUDAAPI *PFNCUEVENTSYNCHRONIZE) (CUevent hEvent);
typedef CUresult (CUDAAPI *PFNCUEVENTDESTROY) (CUevent hEvent);
typedef CUresult (CUDAAPI *PFNCUEVENTELAPSEDTIME) (float *pMilliseconds, CUevent hStart, CUevent hEnd);
typedef CUresult (CUDAAPI *PFNCUIMPORTEXTERNALMEMORY) (CUexternalMemory *extMem_out, const CUDA_EXTERNAL_MEMORY_HANDLE_DESC *memHandleDesc);
typedef CUresult (CUDAAPI *PFNCUEXTERNALMEMORYGETMAPPEDBUFFER) (CUdeviceptr *devPtr, CUexternalMemory extMem, const CUDA_EXTERNAL_MEMORY_BUFFER_DESC *bufferDesc);
typedef CUresult (CUDAAPI *PFNCUEXTERNALMEMORYGETMAPPEDMIPMAPPEDARRAY) (CUmipmappedArray *mipmap, CUexternalMemory extMem, const CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC *mipmapDesc);
typedef CUresult (CUDAAPI *PFNCUDESTROYEXTERNALMEMORY) (CUexternalMemory extMem);
typedef CUresult (CUDAAPI *PFNCUIMPORTEXTERNALSEMAPHORE) (CUexternalSemaphore *extSem_out, const CUDA_EXTERNAL_SEMAPHORE_HANDLE_DESC *semHandleDesc);
typedef CUresult (CUDAAPI *PFNCUSIGNALEXTERNALSEMAPHORESASYNC) (const CUexternalSemaphore *extSemArray, const CUDA_EXTERNAL_SEMAPHORE_SIGNAL_PARAMS *paramsArray, unsigned int numExtSems, CUstream stream);
typedef CUresult (CUDAAPI *PFNCUWAITEXTERNALSEMAPHORESASYNC) (const CUexternalSemaphore *extSemArray, const CUDA_EXTERNAL_SEMAPHORE_WAIT_PARAMS *paramsArray, unsigned int numExtSems, CUstream stream);
typedef CUresult (CUDAAPI *PFNCUDESTROYEXTERNALSEMAPHORE) (CUexternalSemaphore extSem);
typedef CUresult (CUDAAPI *PFNCUSTREAMWAITVALUE32) (CUstream stream, CUdeviceptr addr, cuuint32_t value, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUSTREAMWAITVALUE64) (CUstream stream, CUdeviceptr addr, cuuint64_t value, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUSTREAMWRITEVALUE32) (CUstream stream, CUdeviceptr addr, cuuint32_t value, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUSTREAMWRITEVALUE64) (CUstream stream, CUdeviceptr addr, cuuint64_t value, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUSTREAMBATCHMEMOP) (CUstream stream, unsigned int count, CUstreamBatchMemOpParams *paramArray, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUFUNCGETATTRIBUTE) (int *pi, CUfunction_attribute attrib, CUfunction hfunc);
typedef CUresult (CUDAAPI *PFNCUFUNCSETATTRIBUTE) (CUfunction hfunc, CUfunction_attribute attrib, int value);
typedef CUresult (CUDAAPI *PFNCUFUNCSETCACHECONFIG) (CUfunction hfunc, CUfunc_cache config);
typedef CUresult (CUDAAPI *PFNCUFUNCSETSHAREDMEMCONFIG) (CUfunction hfunc, CUsharedconfig config);
typedef CUresult (CUDAAPI *PFNCULAUNCHKERNEL) (CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes, CUstream hStream, void **kernelParams, void **extra);
typedef CUresult (CUDAAPI *PFNCULAUNCHCOOPERATIVEKERNEL) (CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes, CUstream hStream, void **kernelParams);
typedef CUresult (CUDAAPI *PFNCULAUNCHCOOPERATIVEKERNELMULTIDEVICE) (CUDA_LAUNCH_PARAMS *launchParamsList, unsigned int numDevices, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCULAUNCHHOSTFUNC) (CUstream hStream, CUhostFn fn, void *userData);
typedef CUresult (CUDAAPI *PFNCUFUNCSETBLOCKSHAPE) (CUfunction hfunc, int x, int y, int z);
typedef CUresult (CUDAAPI *PFNCUFUNCSETSHAREDSIZE) (CUfunction hfunc, unsigned int bytes);
typedef CUresult (CUDAAPI *PFNCUPARAMSETSIZE) (CUfunction hfunc, unsigned int numbytes);
typedef CUresult (CUDAAPI *PFNCUPARAMSETI) (CUfunction hfunc, int offset, unsigned int value);
typedef CUresult (CUDAAPI *PFNCUPARAMSETF) (CUfunction hfunc, int offset, float value);
typedef CUresult (CUDAAPI *PFNCUPARAMSETV) (CUfunction hfunc, int offset, void *ptr, unsigned int numbytes);
typedef CUresult (CUDAAPI *PFNCULAUNCH) (CUfunction f);
typedef CUresult (CUDAAPI *PFNCULAUNCHGRID) (CUfunction f, int grid_width, int grid_height);
typedef CUresult (CUDAAPI *PFNCULAUNCHGRIDASYNC) (CUfunction f, int grid_width, int grid_height, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUPARAMSETTEXREF) (CUfunction hfunc, int texunit, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUGRAPHCREATE) (CUgraph *phGraph, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUGRAPHADDKERNELNODE) (CUgraphNode *phGraphNode, CUgraph hGraph, const CUgraphNode *dependencies, size_t numDependencies, const CUDA_KERNEL_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHKERNELNODEGETPARAMS) (CUgraphNode hNode, CUDA_KERNEL_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHKERNELNODESETPARAMS) (CUgraphNode hNode, const CUDA_KERNEL_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHADDMEMCPYNODE) (CUgraphNode *phGraphNode, CUgraph hGraph, const CUgraphNode *dependencies, size_t numDependencies, const CUDA_MEMCPY3D *copyParams, CUcontext ctx);
typedef CUresult (CUDAAPI *PFNCUGRAPHMEMCPYNODEGETPARAMS) (CUgraphNode hNode, CUDA_MEMCPY3D *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHMEMCPYNODESETPARAMS) (CUgraphNode hNode, const CUDA_MEMCPY3D *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHADDMEMSETNODE) (CUgraphNode *phGraphNode, CUgraph hGraph, const CUgraphNode *dependencies, size_t numDependencies, const CUDA_MEMSET_NODE_PARAMS *memsetParams, CUcontext ctx);
typedef CUresult (CUDAAPI *PFNCUGRAPHMEMSETNODEGETPARAMS) (CUgraphNode hNode, CUDA_MEMSET_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHMEMSETNODESETPARAMS) (CUgraphNode hNode, const CUDA_MEMSET_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHADDHOSTNODE) (CUgraphNode *phGraphNode, CUgraph hGraph, const CUgraphNode *dependencies, size_t numDependencies, const CUDA_HOST_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHHOSTNODEGETPARAMS) (CUgraphNode hNode, CUDA_HOST_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHHOSTNODESETPARAMS) (CUgraphNode hNode, const CUDA_HOST_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHADDCHILDGRAPHNODE) (CUgraphNode *phGraphNode, CUgraph hGraph, const CUgraphNode *dependencies, size_t numDependencies, CUgraph childGraph);
typedef CUresult (CUDAAPI *PFNCUGRAPHCHILDGRAPHNODEGETGRAPH) (CUgraphNode hNode, CUgraph *phGraph);
typedef CUresult (CUDAAPI *PFNCUGRAPHADDEMPTYNODE) (CUgraphNode *phGraphNode, CUgraph hGraph, const CUgraphNode *dependencies, size_t numDependencies);
typedef CUresult (CUDAAPI *PFNCUGRAPHADDEVENTRECORDNODE) (CUgraphNode *phGraphNode, CUgraph hGraph, const CUgraphNode *dependencies, size_t numDependencies, CUevent event);
typedef CUresult (CUDAAPI *PFNCUGRAPHEVENTRECORDNODEGETEVENT) (CUgraphNode hNode, CUevent *event_out);
typedef CUresult (CUDAAPI *PFNCUGRAPHEVENTRECORDNODESETEVENT) (CUgraphNode hNode, CUevent event);
typedef CUresult (CUDAAPI *PFNCUGRAPHADDEVENTWAITNODE) (CUgraphNode *phGraphNode, CUgraph hGraph, const CUgraphNode *dependencies, size_t numDependencies, CUevent event);
typedef CUresult (CUDAAPI *PFNCUGRAPHEVENTWAITNODEGETEVENT) (CUgraphNode hNode, CUevent *event_out);
typedef CUresult (CUDAAPI *PFNCUGRAPHEVENTWAITNODESETEVENT) (CUgraphNode hNode, CUevent event);
typedef CUresult (CUDAAPI *PFNCUGRAPHADDEXTERNALSEMAPHORESSIGNALNODE) (CUgraphNode *phGraphNode, CUgraph hGraph, const CUgraphNode *dependencies, size_t numDependencies, const CUDA_EXT_SEM_SIGNAL_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXTERNALSEMAPHORESSIGNALNODEGETPARAMS) (CUgraphNode hNode, CUDA_EXT_SEM_SIGNAL_NODE_PARAMS *params_out);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXTERNALSEMAPHORESSIGNALNODESETPARAMS) (CUgraphNode hNode, const CUDA_EXT_SEM_SIGNAL_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHADDEXTERNALSEMAPHORESWAITNODE) (CUgraphNode *phGraphNode, CUgraph hGraph, const CUgraphNode *dependencies, size_t numDependencies, const CUDA_EXT_SEM_WAIT_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXTERNALSEMAPHORESWAITNODEGETPARAMS) (CUgraphNode hNode, CUDA_EXT_SEM_WAIT_NODE_PARAMS *params_out);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXTERNALSEMAPHORESWAITNODESETPARAMS) (CUgraphNode hNode, const CUDA_EXT_SEM_WAIT_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHCLONE) (CUgraph *phGraphClone, CUgraph originalGraph);
typedef CUresult (CUDAAPI *PFNCUGRAPHNODEFINDINCLONE) (CUgraphNode *phNode, CUgraphNode hOriginalNode, CUgraph hClonedGraph);
typedef CUresult (CUDAAPI *PFNCUGRAPHNODEGETTYPE) (CUgraphNode hNode, CUgraphNodeType *type);
typedef CUresult (CUDAAPI *PFNCUGRAPHGETNODES) (CUgraph hGraph, CUgraphNode *nodes, size_t *numNodes);
typedef CUresult (CUDAAPI *PFNCUGRAPHGETROOTNODES) (CUgraph hGraph, CUgraphNode *rootNodes, size_t *numRootNodes);
typedef CUresult (CUDAAPI *PFNCUGRAPHGETEDGES) (CUgraph hGraph, CUgraphNode *from, CUgraphNode *to, size_t *numEdges);
typedef CUresult (CUDAAPI *PFNCUGRAPHNODEGETDEPENDENCIES) (CUgraphNode hNode, CUgraphNode *dependencies, size_t *numDependencies);
typedef CUresult (CUDAAPI *PFNCUGRAPHNODEGETDEPENDENTNODES) (CUgraphNode hNode, CUgraphNode *dependentNodes, size_t *numDependentNodes);
typedef CUresult (CUDAAPI *PFNCUGRAPHADDDEPENDENCIES) (CUgraph hGraph, const CUgraphNode *from, const CUgraphNode *to, size_t numDependencies);
typedef CUresult (CUDAAPI *PFNCUGRAPHREMOVEDEPENDENCIES) (CUgraph hGraph, const CUgraphNode *from, const CUgraphNode *to, size_t numDependencies);
typedef CUresult (CUDAAPI *PFNCUGRAPHDESTROYNODE) (CUgraphNode hNode);
typedef CUresult (CUDAAPI *PFNCUGRAPHINSTANTIATE) (CUgraphExec *phGraphExec, CUgraph hGraph, CUgraphNode *phErrorNode, char *logBuffer, size_t bufferSize);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXECKERNELNODESETPARAMS) (CUgraphExec hGraphExec, CUgraphNode hNode, const CUDA_KERNEL_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXECMEMCPYNODESETPARAMS) (CUgraphExec hGraphExec, CUgraphNode hNode, const CUDA_MEMCPY3D *copyParams, CUcontext ctx);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXECMEMSETNODESETPARAMS) (CUgraphExec hGraphExec, CUgraphNode hNode, const CUDA_MEMSET_NODE_PARAMS *memsetParams, CUcontext ctx);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXECHOSTNODESETPARAMS) (CUgraphExec hGraphExec, CUgraphNode hNode, const CUDA_HOST_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXECCHILDGRAPHNODESETPARAMS) (CUgraphExec hGraphExec, CUgraphNode hNode, CUgraph childGraph);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXECEVENTRECORDNODESETEVENT) (CUgraphExec hGraphExec, CUgraphNode hNode, CUevent event);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXECEVENTWAITNODESETEVENT) (CUgraphExec hGraphExec, CUgraphNode hNode, CUevent event);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXECEXTERNALSEMAPHORESSIGNALNODESETPARAMS) (CUgraphExec hGraphExec, CUgraphNode hNode, const CUDA_EXT_SEM_SIGNAL_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXECEXTERNALSEMAPHORESWAITNODESETPARAMS) (CUgraphExec hGraphExec, CUgraphNode hNode, const CUDA_EXT_SEM_WAIT_NODE_PARAMS *nodeParams);
typedef CUresult (CUDAAPI *PFNCUGRAPHUPLOAD) (CUgraphExec hGraphExec, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUGRAPHLAUNCH) (CUgraphExec hGraphExec, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXECDESTROY) (CUgraphExec hGraphExec);
typedef CUresult (CUDAAPI *PFNCUGRAPHDESTROY) (CUgraph hGraph);
typedef CUresult (CUDAAPI *PFNCUGRAPHEXECUPDATE) (CUgraphExec hGraphExec, CUgraph hGraph, CUgraphNode *hErrorNode_out, CUgraphExecUpdateResult *updateResult_out);
typedef CUresult (CUDAAPI *PFNCUGRAPHKERNELNODECOPYATTRIBUTES) (CUgraphNode dst, CUgraphNode src);
typedef CUresult (CUDAAPI *PFNCUGRAPHKERNELNODEGETATTRIBUTE) (CUgraphNode hNode, CUkernelNodeAttrID attr, CUkernelNodeAttrValue *value_out);
typedef CUresult (CUDAAPI *PFNCUGRAPHKERNELNODESETATTRIBUTE) (CUgraphNode hNode, CUkernelNodeAttrID attr, const CUkernelNodeAttrValue *value);
typedef CUresult (CUDAAPI *PFNCUOCCUPANCYMAXACTIVEBLOCKSPERMULTIPROCESSOR) (int *numBlocks, CUfunction func, int blockSize, size_t dynamicSMemSize);
typedef CUresult (CUDAAPI *PFNCUOCCUPANCYMAXACTIVEBLOCKSPERMULTIPROCESSORWITHFLAGS) (int *numBlocks, CUfunction func, int blockSize, size_t dynamicSMemSize, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUOCCUPANCYMAXPOTENTIALBLOCKSIZE) (int *minGridSize, int *blockSize, CUfunction func, CUoccupancyB2DSize blockSizeToDynamicSMemSize, size_t dynamicSMemSize, int blockSizeLimit);
typedef CUresult (CUDAAPI *PFNCUOCCUPANCYMAXPOTENTIALBLOCKSIZEWITHFLAGS) (int *minGridSize, int *blockSize, CUfunction func, CUoccupancyB2DSize blockSizeToDynamicSMemSize, size_t dynamicSMemSize, int blockSizeLimit, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUOCCUPANCYAVAILABLEDYNAMICSMEMPERBLOCK) (size_t *dynamicSmemSize, CUfunction func, int numBlocks, int blockSize);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETARRAY) (CUtexref hTexRef, CUarray hArray, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETMIPMAPPEDARRAY) (CUtexref hTexRef, CUmipmappedArray hMipmappedArray, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETADDRESS) (size_t *ByteOffset, CUtexref hTexRef, CUdeviceptr dptr, size_t bytes);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETADDRESS2D) (CUtexref hTexRef, const CUDA_ARRAY_DESCRIPTOR *desc, CUdeviceptr dptr, size_t Pitch);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETFORMAT) (CUtexref hTexRef, CUarray_format fmt, int NumPackedComponents);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETADDRESSMODE) (CUtexref hTexRef, int dim, CUaddress_mode am);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETFILTERMODE) (CUtexref hTexRef, CUfilter_mode fm);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETMIPMAPFILTERMODE) (CUtexref hTexRef, CUfilter_mode fm);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETMIPMAPLEVELBIAS) (CUtexref hTexRef, float bias);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETMIPMAPLEVELCLAMP) (CUtexref hTexRef, float minMipmapLevelClamp, float maxMipmapLevelClamp);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETMAXANISOTROPY) (CUtexref hTexRef, unsigned int maxAniso);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETBORDERCOLOR) (CUtexref hTexRef, float *pBorderColor);
typedef CUresult (CUDAAPI *PFNCUTEXREFSETFLAGS) (CUtexref hTexRef, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETADDRESS) (CUdeviceptr *pdptr, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETARRAY) (CUarray *phArray, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETMIPMAPPEDARRAY) (CUmipmappedArray *phMipmappedArray, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETADDRESSMODE) (CUaddress_mode *pam, CUtexref hTexRef, int dim);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETFILTERMODE) (CUfilter_mode *pfm, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETFORMAT) (CUarray_format *pFormat, int *pNumChannels, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETMIPMAPFILTERMODE) (CUfilter_mode *pfm, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETMIPMAPLEVELBIAS) (float *pbias, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETMIPMAPLEVELCLAMP) (float *pminMipmapLevelClamp, float *pmaxMipmapLevelClamp, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETMAXANISOTROPY) (int *pmaxAniso, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETBORDERCOLOR) (float *pBorderColor, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFGETFLAGS) (unsigned int *pFlags, CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFCREATE) (CUtexref *pTexRef);
typedef CUresult (CUDAAPI *PFNCUTEXREFDESTROY) (CUtexref hTexRef);
typedef CUresult (CUDAAPI *PFNCUSURFREFSETARRAY) (CUsurfref hSurfRef, CUarray hArray, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUSURFREFGETARRAY) (CUarray *phArray, CUsurfref hSurfRef);
typedef CUresult (CUDAAPI *PFNCUTEXOBJECTCREATE) (CUtexObject *pTexObject, const CUDA_RESOURCE_DESC *pResDesc, const CUDA_TEXTURE_DESC *pTexDesc, const CUDA_RESOURCE_VIEW_DESC *pResViewDesc);
typedef CUresult (CUDAAPI *PFNCUTEXOBJECTDESTROY) (CUtexObject texObject);
typedef CUresult (CUDAAPI *PFNCUTEXOBJECTGETRESOURCEDESC) (CUDA_RESOURCE_DESC *pResDesc, CUtexObject texObject);
typedef CUresult (CUDAAPI *PFNCUTEXOBJECTGETTEXTUREDESC) (CUDA_TEXTURE_DESC *pTexDesc, CUtexObject texObject);
typedef CUresult (CUDAAPI *PFNCUTEXOBJECTGETRESOURCEVIEWDESC) (CUDA_RESOURCE_VIEW_DESC *pResViewDesc, CUtexObject texObject);
typedef CUresult (CUDAAPI *PFNCUSURFOBJECTCREATE) (CUsurfObject *pSurfObject, const CUDA_RESOURCE_DESC *pResDesc);
typedef CUresult (CUDAAPI *PFNCUSURFOBJECTDESTROY) (CUsurfObject surfObject);
typedef CUresult (CUDAAPI *PFNCUSURFOBJECTGETRESOURCEDESC) (CUDA_RESOURCE_DESC *pResDesc, CUsurfObject surfObject);
typedef CUresult (CUDAAPI *PFNCUDEVICECANACCESSPEER) (int *canAccessPeer, CUdevice dev, CUdevice peerDev);
typedef CUresult (CUDAAPI *PFNCUCTXENABLEPEERACCESS) (CUcontext peerContext, unsigned int Flags);
typedef CUresult (CUDAAPI *PFNCUCTXDISABLEPEERACCESS) (CUcontext peerContext);
typedef CUresult (CUDAAPI *PFNCUDEVICEGETP2PATTRIBUTE) (int* value, CUdevice_P2PAttribute attrib, CUdevice srcDevice, CUdevice dstDevice);
typedef CUresult (CUDAAPI *PFNCUGRAPHICSUNREGISTERRESOURCE) (CUgraphicsResource resource);
typedef CUresult (CUDAAPI *PFNCUGRAPHICSSUBRESOURCEGETMAPPEDARRAY) (CUarray *pArray, CUgraphicsResource resource, unsigned int arrayIndex, unsigned int mipLevel);
typedef CUresult (CUDAAPI *PFNCUGRAPHICSRESOURCEGETMAPPEDMIPMAPPEDARRAY) (CUmipmappedArray *pMipmappedArray, CUgraphicsResource resource);
typedef CUresult (CUDAAPI *PFNCUGRAPHICSRESOURCEGETMAPPEDPOINTER) (CUdeviceptr *pDevPtr, size_t *pSize, CUgraphicsResource resource);
typedef CUresult (CUDAAPI *PFNCUGRAPHICSRESOURCESETMAPFLAGS) (CUgraphicsResource resource, unsigned int flags);
typedef CUresult (CUDAAPI *PFNCUGRAPHICSMAPRESOURCES) (unsigned int count, CUgraphicsResource *resources, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUGRAPHICSUNMAPRESOURCES) (unsigned int count, CUgraphicsResource *resources, CUstream hStream);
typedef CUresult (CUDAAPI *PFNCUGETEXPORTTABLE) (const void **ppExportTable, const CUuuid *pExportTableId);
typedef CUresult (CUDAAPI *PFNCUFUNCGETMODULE) (CUmodule *hmod, CUfunction hfunc);

// Function pointer list for CUDA Driver API functions
struct CUDA_DRIVER_API_FUNCTION_LIST
{
	PFNCUGETERRORSTRING cuGetErrorString;
	PFNCUGETERRORNAME cuGetErrorName;
	PFNCUINIT cuInit;
	PFNCUDRIVERGETVERSION cuDriverGetVersion;
	PFNCUDEVICEGET cuDeviceGet;
	PFNCUDEVICEGETCOUNT cuDeviceGetCount;
	PFNCUDEVICEGETNAME cuDeviceGetName;
	PFNCUDEVICEGETUUID cuDeviceGetUuid;
	PFNCUDEVICEGETLUID cuDeviceGetLuid;
	PFNCUDEVICETOTALMEM cuDeviceTotalMem;
	PFNCUDEVICEGETTEXTURE1DLINEARMAXWIDTH cuDeviceGetTexture1DLinearMaxWidth;
	PFNCUDEVICEGETATTRIBUTE cuDeviceGetAttribute;
	PFNCUDEVICEGETNVSCISYNCATTRIBUTES cuDeviceGetNvSciSyncAttributes;
	PFNCUDEVICESETMEMPOOL cuDeviceSetMemPool;
	PFNCUDEVICEGETMEMPOOL cuDeviceGetMemPool;
	PFNCUDEVICEGETDEFAULTMEMPOOL cuDeviceGetDefaultMemPool;
	PFNCUDEVICEGETPROPERTIES cuDeviceGetProperties;
	PFNCUDEVICECOMPUTECAPABILITY cuDeviceComputeCapability;
	PFNCUDEVICEPRIMARYCTXRETAIN cuDevicePrimaryCtxRetain;
	PFNCUDEVICEPRIMARYCTXRELEASE cuDevicePrimaryCtxRelease;
	PFNCUDEVICEPRIMARYCTXSETFLAGS cuDevicePrimaryCtxSetFlags;
	PFNCUDEVICEPRIMARYCTXGETSTATE cuDevicePrimaryCtxGetState;
	PFNCUDEVICEPRIMARYCTXRESET cuDevicePrimaryCtxReset;
	PFNCUCTXCREATE cuCtxCreate;
	PFNCUCTXDESTROY cuCtxDestroy;
	PFNCUCTXPUSHCURRENT cuCtxPushCurrent;
	PFNCUCTXPOPCURRENT cuCtxPopCurrent;
	PFNCUCTXSETCURRENT cuCtxSetCurrent;
	PFNCUCTXGETCURRENT cuCtxGetCurrent;
	PFNCUCTXGETDEVICE cuCtxGetDevice;
	PFNCUCTXGETFLAGS cuCtxGetFlags;
	PFNCUCTXSYNCHRONIZE cuCtxSynchronize;
	PFNCUCTXSETLIMIT cuCtxSetLimit;
	PFNCUCTXGETLIMIT cuCtxGetLimit;
	PFNCUCTXGETCACHECONFIG cuCtxGetCacheConfig;
	PFNCUCTXSETCACHECONFIG cuCtxSetCacheConfig;
	PFNCUCTXGETSHAREDMEMCONFIG cuCtxGetSharedMemConfig;
	PFNCUCTXSETSHAREDMEMCONFIG cuCtxSetSharedMemConfig;
	PFNCUCTXGETAPIVERSION cuCtxGetApiVersion;
	PFNCUCTXGETSTREAMPRIORITYRANGE cuCtxGetStreamPriorityRange;
	PFNCUCTXRESETPERSISTINGL2CACHE cuCtxResetPersistingL2Cache;
	PFNCUCTXATTACH cuCtxAttach;
	PFNCUCTXDETACH cuCtxDetach;
	PFNCUMODULELOAD cuModuleLoad;
	PFNCUMODULELOADDATA cuModuleLoadData;
	PFNCUMODULELOADDATAEX cuModuleLoadDataEx;
	PFNCUMODULELOADFATBINARY cuModuleLoadFatBinary;
	PFNCUMODULEUNLOAD cuModuleUnload;
	PFNCUMODULEGETFUNCTION cuModuleGetFunction;
	PFNCUMODULEGETGLOBAL cuModuleGetGlobal;
	PFNCUMODULEGETTEXREF cuModuleGetTexRef;
	PFNCUMODULEGETSURFREF cuModuleGetSurfRef;
	PFNCULINKCREATE cuLinkCreate;
	PFNCULINKADDDATA cuLinkAddData;
	PFNCULINKADDFILE cuLinkAddFile;
	PFNCULINKCOMPLETE cuLinkComplete;
	PFNCULINKDESTROY cuLinkDestroy;
	PFNCUMEMGETINFO cuMemGetInfo;
	PFNCUMEMALLOC cuMemAlloc;
	PFNCUMEMALLOCPITCH cuMemAllocPitch;
	PFNCUMEMFREE cuMemFree;
	PFNCUMEMGETADDRESSRANGE cuMemGetAddressRange;
	PFNCUMEMALLOCHOST cuMemAllocHost;
	PFNCUMEMFREEHOST cuMemFreeHost;
	PFNCUMEMHOSTALLOC cuMemHostAlloc;
	PFNCUMEMHOSTGETDEVICEPOINTER cuMemHostGetDevicePointer;
	PFNCUMEMHOSTGETFLAGS cuMemHostGetFlags;
	PFNCUMEMALLOCMANAGED cuMemAllocManaged;
	PFNCUDEVICEGETBYPCIBUSID cuDeviceGetByPCIBusId;
	PFNCUDEVICEGETPCIBUSID cuDeviceGetPCIBusId;
	PFNCUIPCGETEVENTHANDLE cuIpcGetEventHandle;
	PFNCUIPCOPENEVENTHANDLE cuIpcOpenEventHandle;
	PFNCUIPCGETMEMHANDLE cuIpcGetMemHandle;
	PFNCUIPCOPENMEMHANDLE cuIpcOpenMemHandle;
	PFNCUIPCCLOSEMEMHANDLE cuIpcCloseMemHandle;
	PFNCUMEMHOSTREGISTER cuMemHostRegister;
	PFNCUMEMHOSTUNREGISTER cuMemHostUnregister;
	PFNCUMEMCPY cuMemcpy;
	PFNCUMEMCPYPEER cuMemcpyPeer;
	PFNCUMEMCPYHTOD cuMemcpyHtoD;
	PFNCUMEMCPYDTOH cuMemcpyDtoH;
	PFNCUMEMCPYDTOD cuMemcpyDtoD;
	PFNCUMEMCPYDTOA cuMemcpyDtoA;
	PFNCUMEMCPYATOD cuMemcpyAtoD;
	PFNCUMEMCPYHTOA cuMemcpyHtoA;
	PFNCUMEMCPYATOH cuMemcpyAtoH;
	PFNCUMEMCPYATOA cuMemcpyAtoA;
	PFNCUMEMCPY2D cuMemcpy2D;
	PFNCUMEMCPY2DUNALIGNED cuMemcpy2DUnaligned;
	PFNCUMEMCPY3D cuMemcpy3D;
	PFNCUMEMCPY3DPEER cuMemcpy3DPeer;
	PFNCUMEMCPYASYNC cuMemcpyAsync;
	PFNCUMEMCPYPEERASYNC cuMemcpyPeerAsync;
	PFNCUMEMCPYHTODASYNC cuMemcpyHtoDAsync;
	PFNCUMEMCPYDTOHASYNC cuMemcpyDtoHAsync;
	PFNCUMEMCPYDTODASYNC cuMemcpyDtoDAsync;
	PFNCUMEMCPYHTOAASYNC cuMemcpyHtoAAsync;
	PFNCUMEMCPYATOHASYNC cuMemcpyAtoHAsync;
	PFNCUMEMCPY2DASYNC cuMemcpy2DAsync;
	PFNCUMEMCPY3DASYNC cuMemcpy3DAsync;
	PFNCUMEMCPY3DPEERASYNC cuMemcpy3DPeerAsync;
	PFNCUMEMSETD8 cuMemsetD8;
	PFNCUMEMSETD16 cuMemsetD16;
	PFNCUMEMSETD32 cuMemsetD32;
	PFNCUMEMSETD2D8 cuMemsetD2D8;
	PFNCUMEMSETD2D16 cuMemsetD2D16;
	PFNCUMEMSETD2D32 cuMemsetD2D32;
	PFNCUMEMSETD8ASYNC cuMemsetD8Async;
	PFNCUMEMSETD16ASYNC cuMemsetD16Async;
	PFNCUMEMSETD32ASYNC cuMemsetD32Async;
	PFNCUMEMSETD2D8ASYNC cuMemsetD2D8Async;
	PFNCUMEMSETD2D16ASYNC cuMemsetD2D16Async;
	PFNCUMEMSETD2D32ASYNC cuMemsetD2D32Async;
	PFNCUARRAYCREATE cuArrayCreate;
	PFNCUARRAYGETDESCRIPTOR cuArrayGetDescriptor;
	PFNCUARRAYGETSPARSEPROPERTIES cuArrayGetSparseProperties;
	PFNCUMIPMAPPEDARRAYGETSPARSEPROPERTIES cuMipmappedArrayGetSparseProperties;
	PFNCUARRAYGETPLANE cuArrayGetPlane;
	PFNCUARRAYDESTROY cuArrayDestroy;
	PFNCUARRAY3DCREATE cuArray3DCreate;
	PFNCUARRAY3DGETDESCRIPTOR cuArray3DGetDescriptor;
	PFNCUMIPMAPPEDARRAYCREATE cuMipmappedArrayCreate;
	PFNCUMIPMAPPEDARRAYGETLEVEL cuMipmappedArrayGetLevel;
	PFNCUMIPMAPPEDARRAYDESTROY cuMipmappedArrayDestroy;
	PFNCUMEMADDRESSRESERVE cuMemAddressReserve;
	PFNCUMEMADDRESSFREE cuMemAddressFree;
	PFNCUMEMCREATE cuMemCreate;
	PFNCUMEMRELEASE cuMemRelease;
	PFNCUMEMMAP cuMemMap;
	PFNCUMEMMAPARRAYASYNC cuMemMapArrayAsync;
	PFNCUMEMUNMAP cuMemUnmap;
	PFNCUMEMSETACCESS cuMemSetAccess;
	PFNCUMEMGETACCESS cuMemGetAccess;
	PFNCUMEMEXPORTTOSHAREABLEHANDLE cuMemExportToShareableHandle;
	PFNCUMEMIMPORTFROMSHAREABLEHANDLE cuMemImportFromShareableHandle;
	PFNCUMEMGETALLOCATIONGRANULARITY cuMemGetAllocationGranularity;
	PFNCUMEMGETALLOCATIONPROPERTIESFROMHANDLE cuMemGetAllocationPropertiesFromHandle;
	PFNCUMEMRETAINALLOCATIONHANDLE cuMemRetainAllocationHandle;
	PFNCUMEMFREEASYNC cuMemFreeAsync;
	PFNCUMEMALLOCASYNC cuMemAllocAsync;
	PFNCUMEMPOOLTRIMTO cuMemPoolTrimTo;
	PFNCUMEMPOOLSETATTRIBUTE cuMemPoolSetAttribute;
	PFNCUMEMPOOLGETATTRIBUTE cuMemPoolGetAttribute;
	PFNCUMEMPOOLSETACCESS cuMemPoolSetAccess;
	PFNCUMEMPOOLGETACCESS cuMemPoolGetAccess;
	PFNCUMEMPOOLCREATE cuMemPoolCreate;
	PFNCUMEMPOOLDESTROY cuMemPoolDestroy;
	PFNCUMEMALLOCFROMPOOLASYNC cuMemAllocFromPoolAsync;
	PFNCUMEMPOOLEXPORTTOSHAREABLEHANDLE cuMemPoolExportToShareableHandle;
	PFNCUMEMPOOLIMPORTFROMSHAREABLEHANDLE cuMemPoolImportFromShareableHandle;
	PFNCUMEMPOOLEXPORTPOINTER cuMemPoolExportPointer;
	PFNCUMEMPOOLIMPORTPOINTER cuMemPoolImportPointer;
	PFNCUPOINTERGETATTRIBUTE cuPointerGetAttribute;
	PFNCUMEMPREFETCHASYNC cuMemPrefetchAsync;
	PFNCUMEMADVISE cuMemAdvise;
	PFNCUMEMRANGEGETATTRIBUTE cuMemRangeGetAttribute;
	PFNCUMEMRANGEGETATTRIBUTES cuMemRangeGetAttributes;
	PFNCUPOINTERSETATTRIBUTE cuPointerSetAttribute;
	PFNCUPOINTERGETATTRIBUTES cuPointerGetAttributes;
	PFNCUSTREAMCREATE cuStreamCreate;
	PFNCUSTREAMCREATEWITHPRIORITY cuStreamCreateWithPriority;
	PFNCUSTREAMGETPRIORITY cuStreamGetPriority;
	PFNCUSTREAMGETFLAGS cuStreamGetFlags;
	PFNCUSTREAMGETCTX cuStreamGetCtx;
	PFNCUSTREAMWAITEVENT cuStreamWaitEvent;
	PFNCUSTREAMADDCALLBACK cuStreamAddCallback;
	PFNCUSTREAMBEGINCAPTURE cuStreamBeginCapture;
	PFNCUTHREADEXCHANGESTREAMCAPTUREMODE cuThreadExchangeStreamCaptureMode;
	PFNCUSTREAMENDCAPTURE cuStreamEndCapture;
	PFNCUSTREAMISCAPTURING cuStreamIsCapturing;
	PFNCUSTREAMGETCAPTUREINFO cuStreamGetCaptureInfo;
	PFNCUSTREAMATTACHMEMASYNC cuStreamAttachMemAsync;
	PFNCUSTREAMQUERY cuStreamQuery;
	PFNCUSTREAMSYNCHRONIZE cuStreamSynchronize;
	PFNCUSTREAMDESTROY cuStreamDestroy;
	PFNCUSTREAMCOPYATTRIBUTES cuStreamCopyAttributes;
	PFNCUSTREAMGETATTRIBUTE cuStreamGetAttribute;
	PFNCUSTREAMSETATTRIBUTE cuStreamSetAttribute;
	PFNCUEVENTCREATE cuEventCreate;
	PFNCUEVENTRECORD cuEventRecord;
	PFNCUEVENTRECORDWITHFLAGS cuEventRecordWithFlags;
	PFNCUEVENTQUERY cuEventQuery;
	PFNCUEVENTSYNCHRONIZE cuEventSynchronize;
	PFNCUEVENTDESTROY cuEventDestroy;
	PFNCUEVENTELAPSEDTIME cuEventElapsedTime;
	PFNCUIMPORTEXTERNALMEMORY cuImportExternalMemory;
	PFNCUEXTERNALMEMORYGETMAPPEDBUFFER cuExternalMemoryGetMappedBuffer;
	PFNCUEXTERNALMEMORYGETMAPPEDMIPMAPPEDARRAY cuExternalMemoryGetMappedMipmappedArray;
	PFNCUDESTROYEXTERNALMEMORY cuDestroyExternalMemory;
	PFNCUIMPORTEXTERNALSEMAPHORE cuImportExternalSemaphore;
	PFNCUSIGNALEXTERNALSEMAPHORESASYNC cuSignalExternalSemaphoresAsync;
	PFNCUWAITEXTERNALSEMAPHORESASYNC cuWaitExternalSemaphoresAsync;
	PFNCUDESTROYEXTERNALSEMAPHORE cuDestroyExternalSemaphore;
	PFNCUSTREAMWAITVALUE32 cuStreamWaitValue32;
	PFNCUSTREAMWAITVALUE64 cuStreamWaitValue64;
	PFNCUSTREAMWRITEVALUE32 cuStreamWriteValue32;
	PFNCUSTREAMWRITEVALUE64 cuStreamWriteValue64;
	PFNCUSTREAMBATCHMEMOP cuStreamBatchMemOp;
	PFNCUFUNCGETATTRIBUTE cuFuncGetAttribute;
	PFNCUFUNCSETATTRIBUTE cuFuncSetAttribute;
	PFNCUFUNCSETCACHECONFIG cuFuncSetCacheConfig;
	PFNCUFUNCSETSHAREDMEMCONFIG cuFuncSetSharedMemConfig;
	PFNCULAUNCHKERNEL cuLaunchKernel;
	PFNCULAUNCHCOOPERATIVEKERNEL cuLaunchCooperativeKernel;
	PFNCULAUNCHCOOPERATIVEKERNELMULTIDEVICE cuLaunchCooperativeKernelMultiDevice;
	PFNCULAUNCHHOSTFUNC cuLaunchHostFunc;
	PFNCUFUNCSETBLOCKSHAPE cuFuncSetBlockShape;
	PFNCUFUNCSETSHAREDSIZE cuFuncSetSharedSize;
	PFNCUPARAMSETSIZE cuParamSetSize;
	PFNCUPARAMSETI cuParamSeti;
	PFNCUPARAMSETF cuParamSetf;
	PFNCUPARAMSETV cuParamSetv;
	PFNCULAUNCH cuLaunch;
	PFNCULAUNCHGRID cuLaunchGrid;
	PFNCULAUNCHGRIDASYNC cuLaunchGridAsync;
	PFNCUPARAMSETTEXREF cuParamSetTexRef;
	PFNCUGRAPHCREATE cuGraphCreate;
	PFNCUGRAPHADDKERNELNODE cuGraphAddKernelNode;
	PFNCUGRAPHKERNELNODEGETPARAMS cuGraphKernelNodeGetParams;
	PFNCUGRAPHKERNELNODESETPARAMS cuGraphKernelNodeSetParams;
	PFNCUGRAPHADDMEMCPYNODE cuGraphAddMemcpyNode;
	PFNCUGRAPHMEMCPYNODEGETPARAMS cuGraphMemcpyNodeGetParams;
	PFNCUGRAPHMEMCPYNODESETPARAMS cuGraphMemcpyNodeSetParams;
	PFNCUGRAPHADDMEMSETNODE cuGraphAddMemsetNode;
	PFNCUGRAPHMEMSETNODEGETPARAMS cuGraphMemsetNodeGetParams;
	PFNCUGRAPHMEMSETNODESETPARAMS cuGraphMemsetNodeSetParams;
	PFNCUGRAPHADDHOSTNODE cuGraphAddHostNode;
	PFNCUGRAPHHOSTNODEGETPARAMS cuGraphHostNodeGetParams;
	PFNCUGRAPHHOSTNODESETPARAMS cuGraphHostNodeSetParams;
	PFNCUGRAPHADDCHILDGRAPHNODE cuGraphAddChildGraphNode;
	PFNCUGRAPHCHILDGRAPHNODEGETGRAPH cuGraphChildGraphNodeGetGraph;
	PFNCUGRAPHADDEMPTYNODE cuGraphAddEmptyNode;
	PFNCUGRAPHADDEVENTRECORDNODE cuGraphAddEventRecordNode;
	PFNCUGRAPHEVENTRECORDNODEGETEVENT cuGraphEventRecordNodeGetEvent;
	PFNCUGRAPHEVENTRECORDNODESETEVENT cuGraphEventRecordNodeSetEvent;
	PFNCUGRAPHADDEVENTWAITNODE cuGraphAddEventWaitNode;
	PFNCUGRAPHEVENTWAITNODEGETEVENT cuGraphEventWaitNodeGetEvent;
	PFNCUGRAPHEVENTWAITNODESETEVENT cuGraphEventWaitNodeSetEvent;
	PFNCUGRAPHADDEXTERNALSEMAPHORESSIGNALNODE cuGraphAddExternalSemaphoresSignalNode;
	PFNCUGRAPHEXTERNALSEMAPHORESSIGNALNODEGETPARAMS cuGraphExternalSemaphoresSignalNodeGetParams;
	PFNCUGRAPHEXTERNALSEMAPHORESSIGNALNODESETPARAMS cuGraphExternalSemaphoresSignalNodeSetParams;
	PFNCUGRAPHADDEXTERNALSEMAPHORESWAITNODE cuGraphAddExternalSemaphoresWaitNode;
	PFNCUGRAPHEXTERNALSEMAPHORESWAITNODEGETPARAMS cuGraphExternalSemaphoresWaitNodeGetParams;
	PFNCUGRAPHEXTERNALSEMAPHORESWAITNODESETPARAMS cuGraphExternalSemaphoresWaitNodeSetParams;
	PFNCUGRAPHCLONE cuGraphClone;
	PFNCUGRAPHNODEFINDINCLONE cuGraphNodeFindInClone;
	PFNCUGRAPHNODEGETTYPE cuGraphNodeGetType;
	PFNCUGRAPHGETNODES cuGraphGetNodes;
	PFNCUGRAPHGETROOTNODES cuGraphGetRootNodes;
	PFNCUGRAPHGETEDGES cuGraphGetEdges;
	PFNCUGRAPHNODEGETDEPENDENCIES cuGraphNodeGetDependencies;
	PFNCUGRAPHNODEGETDEPENDENTNODES cuGraphNodeGetDependentNodes;
	PFNCUGRAPHADDDEPENDENCIES cuGraphAddDependencies;
	PFNCUGRAPHREMOVEDEPENDENCIES cuGraphRemoveDependencies;
	PFNCUGRAPHDESTROYNODE cuGraphDestroyNode;
	PFNCUGRAPHINSTANTIATE cuGraphInstantiate;
	PFNCUGRAPHEXECKERNELNODESETPARAMS cuGraphExecKernelNodeSetParams;
	PFNCUGRAPHEXECMEMCPYNODESETPARAMS cuGraphExecMemcpyNodeSetParams;
	PFNCUGRAPHEXECMEMSETNODESETPARAMS cuGraphExecMemsetNodeSetParams;
	PFNCUGRAPHEXECHOSTNODESETPARAMS cuGraphExecHostNodeSetParams;
	PFNCUGRAPHEXECCHILDGRAPHNODESETPARAMS cuGraphExecChildGraphNodeSetParams;
	PFNCUGRAPHEXECEVENTRECORDNODESETEVENT cuGraphExecEventRecordNodeSetEvent;
	PFNCUGRAPHEXECEVENTWAITNODESETEVENT cuGraphExecEventWaitNodeSetEvent;
	PFNCUGRAPHEXECEXTERNALSEMAPHORESSIGNALNODESETPARAMS cuGraphExecExternalSemaphoresSignalNodeSetParams;
	PFNCUGRAPHEXECEXTERNALSEMAPHORESWAITNODESETPARAMS cuGraphExecExternalSemaphoresWaitNodeSetParams;
	PFNCUGRAPHUPLOAD cuGraphUpload;
	PFNCUGRAPHLAUNCH cuGraphLaunch;
	PFNCUGRAPHEXECDESTROY cuGraphExecDestroy;
	PFNCUGRAPHDESTROY cuGraphDestroy;
	PFNCUGRAPHEXECUPDATE cuGraphExecUpdate;
	PFNCUGRAPHKERNELNODECOPYATTRIBUTES cuGraphKernelNodeCopyAttributes;
	PFNCUGRAPHKERNELNODEGETATTRIBUTE cuGraphKernelNodeGetAttribute;
	PFNCUGRAPHKERNELNODESETATTRIBUTE cuGraphKernelNodeSetAttribute;
	PFNCUOCCUPANCYMAXACTIVEBLOCKSPERMULTIPROCESSOR cuOccupancyMaxActiveBlocksPerMultiprocessor;
	PFNCUOCCUPANCYMAXACTIVEBLOCKSPERMULTIPROCESSORWITHFLAGS cuOccupancyMaxActiveBlocksPerMultiprocessorWithFlags;
	PFNCUOCCUPANCYMAXPOTENTIALBLOCKSIZE cuOccupancyMaxPotentialBlockSize;
	PFNCUOCCUPANCYMAXPOTENTIALBLOCKSIZEWITHFLAGS cuOccupancyMaxPotentialBlockSizeWithFlags;
	PFNCUOCCUPANCYAVAILABLEDYNAMICSMEMPERBLOCK cuOccupancyAvailableDynamicSMemPerBlock;
	PFNCUTEXREFSETARRAY cuTexRefSetArray;
	PFNCUTEXREFSETMIPMAPPEDARRAY cuTexRefSetMipmappedArray;
	PFNCUTEXREFSETADDRESS cuTexRefSetAddress;
	PFNCUTEXREFSETADDRESS2D cuTexRefSetAddress2D;
	PFNCUTEXREFSETFORMAT cuTexRefSetFormat;
	PFNCUTEXREFSETADDRESSMODE cuTexRefSetAddressMode;
	PFNCUTEXREFSETFILTERMODE cuTexRefSetFilterMode;
	PFNCUTEXREFSETMIPMAPFILTERMODE cuTexRefSetMipmapFilterMode;
	PFNCUTEXREFSETMIPMAPLEVELBIAS cuTexRefSetMipmapLevelBias;
	PFNCUTEXREFSETMIPMAPLEVELCLAMP cuTexRefSetMipmapLevelClamp;
	PFNCUTEXREFSETMAXANISOTROPY cuTexRefSetMaxAnisotropy;
	PFNCUTEXREFSETBORDERCOLOR cuTexRefSetBorderColor;
	PFNCUTEXREFSETFLAGS cuTexRefSetFlags;
	PFNCUTEXREFGETADDRESS cuTexRefGetAddress;
	PFNCUTEXREFGETARRAY cuTexRefGetArray;
	PFNCUTEXREFGETMIPMAPPEDARRAY cuTexRefGetMipmappedArray;
	PFNCUTEXREFGETADDRESSMODE cuTexRefGetAddressMode;
	PFNCUTEXREFGETFILTERMODE cuTexRefGetFilterMode;
	PFNCUTEXREFGETFORMAT cuTexRefGetFormat;
	PFNCUTEXREFGETMIPMAPFILTERMODE cuTexRefGetMipmapFilterMode;
	PFNCUTEXREFGETMIPMAPLEVELBIAS cuTexRefGetMipmapLevelBias;
	PFNCUTEXREFGETMIPMAPLEVELCLAMP cuTexRefGetMipmapLevelClamp;
	PFNCUTEXREFGETMAXANISOTROPY cuTexRefGetMaxAnisotropy;
	PFNCUTEXREFGETBORDERCOLOR cuTexRefGetBorderColor;
	PFNCUTEXREFGETFLAGS cuTexRefGetFlags;
	PFNCUTEXREFCREATE cuTexRefCreate;
	PFNCUTEXREFDESTROY cuTexRefDestroy;
	PFNCUSURFREFSETARRAY cuSurfRefSetArray;
	PFNCUSURFREFGETARRAY cuSurfRefGetArray;
	PFNCUTEXOBJECTCREATE cuTexObjectCreate;
	PFNCUTEXOBJECTDESTROY cuTexObjectDestroy;
	PFNCUTEXOBJECTGETRESOURCEDESC cuTexObjectGetResourceDesc;
	PFNCUTEXOBJECTGETTEXTUREDESC cuTexObjectGetTextureDesc;
	PFNCUTEXOBJECTGETRESOURCEVIEWDESC cuTexObjectGetResourceViewDesc;
	PFNCUSURFOBJECTCREATE cuSurfObjectCreate;
	PFNCUSURFOBJECTDESTROY cuSurfObjectDestroy;
	PFNCUSURFOBJECTGETRESOURCEDESC cuSurfObjectGetResourceDesc;
	PFNCUDEVICECANACCESSPEER cuDeviceCanAccessPeer;
	PFNCUCTXENABLEPEERACCESS cuCtxEnablePeerAccess;
	PFNCUCTXDISABLEPEERACCESS cuCtxDisablePeerAccess;
	PFNCUDEVICEGETP2PATTRIBUTE cuDeviceGetP2PAttribute;
	PFNCUGRAPHICSUNREGISTERRESOURCE cuGraphicsUnregisterResource;
	PFNCUGRAPHICSSUBRESOURCEGETMAPPEDARRAY cuGraphicsSubResourceGetMappedArray;
	PFNCUGRAPHICSRESOURCEGETMAPPEDMIPMAPPEDARRAY cuGraphicsResourceGetMappedMipmappedArray;
	PFNCUGRAPHICSRESOURCEGETMAPPEDPOINTER cuGraphicsResourceGetMappedPointer;
	PFNCUGRAPHICSRESOURCESETMAPFLAGS cuGraphicsResourceSetMapFlags;
	PFNCUGRAPHICSMAPRESOURCES cuGraphicsMapResources;
	PFNCUGRAPHICSUNMAPRESOURCES cuGraphicsUnmapResources;
	PFNCUGETEXPORTTABLE cuGetExportTable;
	PFNCUFUNCGETMODULE cuFuncGetModule;
};

// Attempts to load the shared library for the CUDA Driver API
void* OpenCudaDriverLibrary();

// Attempts to retrieve the list of function pointers for the CUDA Driver API shared library
bool LoadCudaDriverFunctions(void* library, CUDA_DRIVER_API_FUNCTION_LIST* funcList);

// Closes a previously-loaded CUDA shared library
void CloseCudaLibrary(void* library);

#ifdef __cplusplus
}
#endif

#endif /* _CUDA_WRAPPER_H */
