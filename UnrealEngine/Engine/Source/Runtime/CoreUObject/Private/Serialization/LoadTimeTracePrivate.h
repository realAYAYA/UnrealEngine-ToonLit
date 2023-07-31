// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/LoadTimeTrace.h"

#if LOADTIMEPROFILERTRACE_ENABLED

class UObject;
class FName;

struct FLoadTimeProfilerTracePrivate
{
	static void OutputStartAsyncLoading();
	static void OutputSuspendAsyncLoading();
	static void OutputResumeAsyncLoading();
	static void OutputNewAsyncPackage(const void* AsyncPackage);
	static void OutputBeginLoadAsyncPackage(const void* AsyncPackage);
	static void OutputEndLoadAsyncPackage(const void* AsyncPackage);
	static void OutputDestroyAsyncPackage(const void* AsyncPackage);
	static void OutputBeginRequest(uint64 RequestId);
	static void OutputEndRequest(uint64 RequestId);
	static void OutputPackageSummary(const void* AsyncPackage, const FName& PackageName, uint32 TotalHeaderSize, uint32 ImportCount, uint32 ExportCount);
	static void OutputAsyncPackageImportDependency(const void* Package, const void* ImportedPackage);
	static void OutputAsyncPackageRequestAssociation(const void* AsyncPackage, uint64 RequestId);
	static void OutputClassInfo(const UClass* Class, const FName& Name);
	static void OutputClassInfo(const UClass* Class, const TCHAR* Name);

	struct FCreateExportScope
	{
		FCreateExportScope(const void* AsyncPackage, const UObject* const* InObject);
		~FCreateExportScope();

	private:
		const UObject* const* Object;
	};

	struct FSerializeExportScope
	{
		FSerializeExportScope(const UObject* Object, uint64 SerialSize);
		~FSerializeExportScope();
	};

	struct FPostLoadExportScope
	{
		FPostLoadExportScope(const UObject* Object);
		~FPostLoadExportScope();
	};

};

#define TRACE_LOADTIME_START_ASYNC_LOADING() \
	FLoadTimeProfilerTracePrivate::OutputStartAsyncLoading();

#define TRACE_LOADTIME_SUSPEND_ASYNC_LOADING() \
	FLoadTimeProfilerTracePrivate::OutputSuspendAsyncLoading();

#define TRACE_LOADTIME_RESUME_ASYNC_LOADING() \
	FLoadTimeProfilerTracePrivate::OutputResumeAsyncLoading();

#define TRACE_LOADTIME_BEGIN_REQUEST(RequestId) \
	FLoadTimeProfilerTracePrivate::OutputBeginRequest(RequestId);

#define TRACE_LOADTIME_END_REQUEST(RequestId) \
	FLoadTimeProfilerTracePrivate::OutputEndRequest(RequestId);

#define TRACE_LOADTIME_NEW_ASYNC_PACKAGE(AsyncPackage) \
	FLoadTimeProfilerTracePrivate::OutputNewAsyncPackage(AsyncPackage)

#define TRACE_LOADTIME_BEGIN_LOAD_ASYNC_PACKAGE(AsyncPackage) \
	FLoadTimeProfilerTracePrivate::OutputBeginLoadAsyncPackage(AsyncPackage)

#define TRACE_LOADTIME_END_LOAD_ASYNC_PACKAGE(AsyncPackage) \
	FLoadTimeProfilerTracePrivate::OutputEndLoadAsyncPackage(AsyncPackage)

#define TRACE_LOADTIME_DESTROY_ASYNC_PACKAGE(AsyncPackage) \
	FLoadTimeProfilerTracePrivate::OutputDestroyAsyncPackage(AsyncPackage);

#define TRACE_LOADTIME_PACKAGE_SUMMARY(AsyncPackage, PackageName, TotalHeaderSize, ImportCount, ExportCount) \
	FLoadTimeProfilerTracePrivate::OutputPackageSummary(AsyncPackage, PackageName, TotalHeaderSize, ImportCount, ExportCount);

#define TRACE_LOADTIME_ASYNC_PACKAGE_REQUEST_ASSOCIATION(AsyncPackage, RequestId) \
	FLoadTimeProfilerTracePrivate::OutputAsyncPackageRequestAssociation(AsyncPackage, RequestId);

#define TRACE_LOADTIME_ASYNC_PACKAGE_LINKER_ASSOCIATION(AsyncPackage, Linker) \
	FLoadTimeProfilerTracePrivate::OutputAsyncPackageLinkerAssociation(AsyncPackage, Linker);

#define TRACE_LOADTIME_ASYNC_PACKAGE_IMPORT_DEPENDENCY(AsyncPackage, ImportedAsyncPackage) \
	FLoadTimeProfilerTracePrivate::OutputAsyncPackageImportDependency(AsyncPackage, ImportedAsyncPackage);

#define TRACE_LOADTIME_CREATE_EXPORT_SCOPE(AsyncPackage, Object) \
	FLoadTimeProfilerTracePrivate::FCreateExportScope __LoadTimeTraceCreateExportScope(AsyncPackage, Object);

#define TRACE_LOADTIME_SERIALIZE_EXPORT_SCOPE(Object, SerialSize) \
	FLoadTimeProfilerTracePrivate::FSerializeExportScope __LoadTimeTraceSerializeExportScope(Object, SerialSize);

#define TRACE_LOADTIME_POSTLOAD_EXPORT_SCOPE(Object) \
	FLoadTimeProfilerTracePrivate::FPostLoadExportScope __LoadTimeTracePostLoadExportScope(Object);

#define TRACE_LOADTIME_CLASS_INFO(Class, Name) \
	FLoadTimeProfilerTracePrivate::OutputClassInfo(Class, Name);

#else

#define TRACE_LOADTIME_START_ASYNC_LOADING(...)
#define TRACE_LOADTIME_SUSPEND_ASYNC_LOADING(...)
#define TRACE_LOADTIME_RESUME_ASYNC_LOADING(...)
#define TRACE_LOADTIME_BEGIN_REQUEST(...)
#define TRACE_LOADTIME_END_REQUEST(...)
#define TRACE_LOADTIME_NEW_ASYNC_PACKAGE(...)
#define TRACE_LOADTIME_BEGIN_LOAD_ASYNC_PACKAGE(...)
#define TRACE_LOADTIME_END_LOAD_ASYNC_PACKAGE(...)
#define TRACE_LOADTIME_DESTROY_ASYNC_PACKAGE(...)
#define TRACE_LOADTIME_PACKAGE_SUMMARY(...)
#define TRACE_LOADTIME_ASYNC_PACKAGE_REQUEST_ASSOCIATION(...)
#define TRACE_LOADTIME_ASYNC_PACKAGE_IMPORT_DEPENDENCY(...)
#define TRACE_LOADTIME_CREATE_EXPORT_SCOPE(...)
#define TRACE_LOADTIME_SERIALIZE_EXPORT_SCOPE(...)
#define TRACE_LOADTIME_POSTLOAD_EXPORT_SCOPE(...)
#define TRACE_LOADTIME_CLASS_INFO(...)

#endif
