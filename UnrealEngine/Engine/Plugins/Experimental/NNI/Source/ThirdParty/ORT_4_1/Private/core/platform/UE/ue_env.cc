// Original from ../windows/env.cc

/* Copyright 2015 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/
// Portions Copyright (c) Microsoft Corporation

#include "core/platform/env.h"

#ifdef PLATFORM_NNI_MICROSOFT

#include "ThirdPartyWarningDisabler.h" // WITH_UE
NNI_THIRD_PARTY_INCLUDES_START
#undef check
#undef TEXT
#include <Windows.h>
NNI_THIRD_PARTY_INCLUDES_END // WITH_UE
#undef CreateDirectory
#undef GetEnvironmentVariable
#include <string>
#include <fcntl.h>
#include <io.h>

#include "core/common/logging/logging.h"
#include "core/platform/env.h"
#include "core/platform/scoped_resource.h"
#include "unsupported/Eigen/CXX11/src/ThreadPool/ThreadPoolInterface.h"
#include <wil/Resource.h>

#include "core/platform/path_lib.h"  // for LoopDir()

EXTERN_C IMAGE_DOS_HEADER __ImageBase;

#else

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>

#ifndef __PROSPERO__
#include <dlfcn.h>
#include <ftw.h>
#endif

#include <string.h>
#include <thread>
#include <utility>  // for std::forward
#include <vector>
#include <assert.h>

#include "core/common/common.h"
#include "core/common/logging/logging.h"
#include "core/platform/scoped_resource.h"
#include "core/platform/EigenNonBlockingThreadPool.h"

#endif

#undef Yield

#include "Containers/StringConv.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/AssertionMacros.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h"

namespace onnxruntime {

namespace {

#ifndef PLATFORM_NNI_MICROSOFT
  class UnmapFileParam {
  public:
    void* addr;
    size_t len;
  };

  static void UnmapFile(void* param) noexcept {
    UnmapFileParam* p = reinterpret_cast<UnmapFileParam*>(param);
    int ret = munmap(p->addr, p->len);
    if (ret != 0) {
      int err = errno;
      LOGS_DEFAULT(ERROR) << "munmap failed. error code: " << err;
    }
    delete p;
  }

  struct FileDescriptorTraits {
    using Handle = int;
    static Handle GetInvalidHandleValue() { return -1; }
    static void CleanUp(Handle h) {
      if (close(h) == -1) {
        const int err = errno;
        LOGS_DEFAULT(ERROR) << "Failed to close file descriptor " << h << " - error code: " << err;
      }
    }
  };

  // Note: File descriptor cleanup may fail but this class doesn't expose a way to check if it failed.
  //       If that's important, consider using another cleanup method.
  using ScopedFileDescriptor = ScopedResource<FileDescriptorTraits>;

  // non-macro equivalent of TEMP_FAILURE_RETRY, described here:
  // https://www.gnu.org/software/libc/manual/html_node/Interrupted-Primitives.html
  template <typename TFunc, typename... TFuncArgs>
  long int TempFailureRetry(TFunc retriable_operation, TFuncArgs&&... args) {
    long int result;
    do {
      result = retriable_operation(std::forward<TFuncArgs>(args)...);
    } while (result == -1 && errno == EINTR);
    return result;
  }

  // nftw() callback to remove a file
  int nftw_remove(
    const char* fpath, const struct stat* /*sb*/,
    int /*typeflag*/, struct FTW* /*ftwbuf*/) {
    const auto result = remove(fpath);
    if (result != 0) {
      const int err = errno;
      LOGS_DEFAULT(WARNING) << "remove() failed. Error code: " << err
        << ", path: " << fpath;
    }
    return result;
  }

  template <typename T>
  struct Freer {
    void operator()(T* p) { ::free(p); }
  };

  using MallocdStringPtr = std::unique_ptr<char, Freer<char> >;

#endif


class FORTUEThread : public EnvThread {
private:
  struct Param {
    const ORTCHAR_T* name_prefix;
    int index;
    unsigned (*start_address)(int id, Eigen::ThreadPoolInterface* param);
    Eigen::ThreadPoolInterface* param;
    const ThreadOptions& thread_options;
  };

  class FORTUERunnable : public FRunnable {
  public:
    FORTUERunnable(const Param& ParamSet) : ParamSet(ParamSet) {
      size_t ThreadIdx = ParamSet.index;
      uint32 IntStackSize = ParamSet.thread_options.stack_size;
      EThreadPriority InThreadPri = ParamSet.thread_options.ThreadPri;

      uint64 AffinityMask = (!ParamSet.thread_options.affinity.empty()) ?
        ParamSet.thread_options.affinity[ThreadIdx] :
        FGenericPlatformAffinity::GetNoAffinityMask();

      std::string ThreadName("FORTUERunnable ");
      ThreadName.append(std::to_string(ThreadIdx));
      FString UEThreadName(ThreadName.c_str());
      Thread = FRunnableThread::Create(
        this,
        *UEThreadName,
        IntStackSize,
        InThreadPri,
        AffinityMask
      );
    };

    virtual ~FORTUERunnable() override {
      if (Thread) {
        // Kill() is a blocking call, it waits for the thread to finish.
        // Hopefully that doesn't take too long
        Thread->Kill();
        delete Thread;
      }
    };

    bool Init() override {
      return true;
    };

    uint32 Run() override {
      ORT_TRY {
        // Ignore the returned value for now
        ParamSet.start_address(ParamSet.index, ParamSet.param);
      }
      ORT_CATCH(const std::exception&) {
        ParamSet.param->Cancel();
      }
      return 0;
    };

    void Stop() override {
      bRunThread = false;
    };

  private:
    // Thread handle. Control the thread using this, with operators like kill
    // and suspend
    FRunnableThread* Thread = nullptr;
    Param ParamSet;
    bool bRunThread;
  };

public:
  FORTUEThread(
    const ORTCHAR_T* name_prefix,
    int index,
    unsigned (*start_address)(int id, Eigen::ThreadPoolInterface* param),
    Eigen::ThreadPoolInterface* param,
    const ThreadOptions& thread_options) {
    Param ParamForRunnable{ name_prefix, index, start_address, param, thread_options };
    RunnableThread = std::make_unique<FORTUERunnable>(ParamForRunnable);
  }

private:
  std::unique_ptr<FORTUERunnable> RunnableThread;
};


class UnrealEngineEnv : public Env {
 public:
  EnvThread* CreateThread(_In_opt_z_ const ORTCHAR_T* name_prefix, int index,
                          unsigned (*start_address)(int id, Eigen::ThreadPoolInterface* param),
                          Eigen::ThreadPoolInterface* param, const ThreadOptions& thread_options) {
    return new FORTUEThread(name_prefix, index, start_address, param, thread_options);
  }

  void SleepForMicroseconds(int64_t micros) const override {
#ifndef WITH_UE
    Sleep(static_cast<DWORD>(micros) / 1000);
#else //WITH_UE
   // UE Code
   constexpr float OneMillion = 1000000.f;
   const float Seconds = static_cast<float>(micros) / OneMillion;
   FPlatformProcess::Sleep(Seconds);
#endif //WITH_UE
  }

  int GetNumCpuCores() const override {
#ifdef PLATFORM_NNI_MICROSOFT
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer[256];
    DWORD returnLength = sizeof(buffer);
    if (GetLogicalProcessorInformation(buffer, &returnLength) == FALSE) {
      // try GetSystemInfo
      SYSTEM_INFO sysInfo;
      GetSystemInfo(&sysInfo);
      if (sysInfo.dwNumberOfProcessors <= 0) {
        ORT_THROW("Fatal error: 0 count processors from GetSystemInfo");
      }
      // This is the number of logical processors in the current group
      return sysInfo.dwNumberOfProcessors;
    }
    int processorCoreCount = 0;
    int count = (int)(returnLength / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
    for (int i = 0; i != count; ++i) {
      if (buffer[i].Relationship == RelationProcessorCore) {
        ++processorCoreCount;
      }
    }
    if (!processorCoreCount)
      ORT_THROW("Fatal error: 0 count processors from GetLogicalProcessorInformation");
    return processorCoreCount;
#else //PLATFORM_NNI_MICROSOFT
  // This is the simplest way to do it without writing platform dependent
  // code.
  int32 ProcessorCoreCount = 0;
  ProcessorCoreCount = FGenericPlatformMisc::NumberOfCoresIncludingHyperthreads();

  if (!ProcessorCoreCount) {
    ORT_THROW("Fatal error: 0 count processors from GetLogicalProcessorInformation");
  }
  return ProcessorCoreCount;
#endif //PLATFORM_NNI_MICROSOFT
  }

  std::vector<size_t> GetThreadAffinityMasks() const override {
#ifdef PLATFORM_NNI_MICROSOFT
    auto generate_vector_of_n = [](int n) {
      std::vector<size_t> ret(n);
      std::iota(ret.begin(), ret.end(), 0);
      return ret;
    };
    // Indeed 64 should be enough. However, it's harmless to have a little more.
    SYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer[256];
    DWORD returnLength = sizeof(buffer);
    if (GetLogicalProcessorInformation(buffer, &returnLength) == FALSE) {
      return generate_vector_of_n(std::thread::hardware_concurrency());
    }
    std::vector<size_t> ret;
    int count = (int)(returnLength / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION));
    for (int i = 0; i != count; ++i) {
      if (buffer[i].Relationship == RelationProcessorCore) {
        ret.push_back(buffer[i].ProcessorMask);
      }
    }
    if (ret.empty())
      return generate_vector_of_n(std::thread::hardware_concurrency());
    return ret;
#else //PLATFORM_NNI_MICROSOFT
  // Reusing POSIX code
  std::vector<size_t> ret(std::thread::hardware_concurrency() / 2);
  std::iota(ret.begin(), ret.end(), 0);
  return ret;
#endif //PLATFORM_NNI_MICROSOFT
  }

  static UnrealEngineEnv& Instance() {
    static UnrealEngineEnv default_env;
    return default_env;
  }

  PIDType GetSelfPid() const override {
#ifndef WITH_UE
    return GetCurrentProcessId();
#else //WITH_UE
  // Calling internal UE Function
  return FGenericPlatformProcess::GetCurrentProcessId();
#endif //WITH_UE
  }

  Status GetFileLength(_In_z_ const ORTCHAR_T* file_path, size_t& length) const override {
#ifdef PLATFORM_NNI_MICROSOFT
#if WINVER >= _WIN32_WINNT_WIN8
    wil::unique_hfile file_handle{
        CreateFile2(file_path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, OPEN_EXISTING, NULL)};
#else
    wil::unique_hfile file_handle{
        CreateFileW(file_path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)};
#endif
    if (file_handle.get() == INVALID_HANDLE_VALUE) {
      const int err = GetLastError();
      return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "open file ", ToUTF8String(file_path), " fail, errcode = ", err);
    }
    LARGE_INTEGER filesize;
    if (!GetFileSizeEx(file_handle.get(), &filesize)) {
      const int err = GetLastError();
      return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "GetFileSizeEx ", ToUTF8String(file_path), " fail, errcode = ", err);
    }
    if (static_cast<ULONGLONG>(filesize.QuadPart) > std::numeric_limits<size_t>::max()) {
      return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "GetFileLength: File is too large");
    }
    length = static_cast<size_t>(filesize.QuadPart);
    return Status::OK();
#else //PLATFORM_NNI_MICROSOFT
  // Path to file, conversion to FString
  FString FilePathUE(file_path);
  // Creating the FileManager
  IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
  // Getting the file size and storing it
  int64 FileSize = FileManager.FileSize(*FilePathUE);
  length = static_cast<size_t>(FileSize);
  return Status::OK();
#endif //PLATFORM_NNI_MICROSOFT
  }

  common::Status GetFileLength(int fd, /*out*/ size_t& file_size) const override {
    using namespace common;
    if (fd < 0) {
      return ORT_MAKE_STATUS(ONNXRUNTIME, INVALID_ARGUMENT, "Invalid fd was supplied: ", fd);
    }

#ifdef PLATFORM_NNI_MICROSOFT
    struct _stat buf;
    int rc = _fstat(fd, &buf);
    if (rc < 0) {
      return Status(SYSTEM, errno);
    }

#else //PLATFORM_NNI_MICROSOFT
  struct stat buf;
  int rc = fstat(fd, &buf);
  if (rc < 0) {
    return ReportSystemError("fstat", "");
  }
#endif //PLATFORM_NNI_MICROSOFT

    if (buf.st_size < 0) {
      return ORT_MAKE_STATUS(SYSTEM, FAIL, "Received negative size from stat call");
    }

    if (static_cast<unsigned long long>(buf.st_size) > std::numeric_limits<size_t>::max()) {
      return ORT_MAKE_STATUS(SYSTEM, FAIL, "File is too large.");
    }

    file_size = static_cast<size_t>(buf.st_size);
    return Status::OK();
  }

  common::Status ReadFileIntoBuffer(_In_z_ const ORTCHAR_T* const file_path, const FileOffsetType offset, const size_t length,
                            const gsl::span<char> buffer) const override {
    ORT_RETURN_IF_NOT(file_path, "file_path == nullptr");
    ORT_RETURN_IF_NOT(offset >= 0, "offset < 0");
    ORT_RETURN_IF_NOT(length <= buffer.size(), "length > buffer.size()");

#ifdef PLATFORM_NNI_MICROSOFT
#if WINVER >= _WIN32_WINNT_WIN8
    wil::unique_hfile file_handle{
        CreateFile2(file_path, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL)};
#else
    wil::unique_hfile file_handle{
        CreateFileW(file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)};
#endif
    if (file_handle.get() == INVALID_HANDLE_VALUE) {
      const int err = GetLastError();
      return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "open file ", ToUTF8String(file_path), " fail, errcode = ", err);
    }

    if (length == 0)
      return Status::OK();

    if (offset > 0) {
      LARGE_INTEGER current_position;
      current_position.QuadPart = offset;
      if (!SetFilePointerEx(file_handle.get(), current_position, &current_position, FILE_BEGIN)) {
        const int err = GetLastError();
        return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "SetFilePointerEx ", ToUTF8String(file_path), " fail, errcode = ", err);
      }
    }

    size_t total_bytes_read = 0;
    while (total_bytes_read < length) {
      constexpr DWORD k_max_bytes_to_read = 1 << 30;  // read at most 1GB each time
      const size_t bytes_remaining = length - total_bytes_read;
      const DWORD bytes_to_read = static_cast<DWORD>(std::min<size_t>(bytes_remaining, k_max_bytes_to_read));
      DWORD bytes_read;

      if (!ReadFile(file_handle.get(), buffer.data() + total_bytes_read, bytes_to_read, &bytes_read, nullptr)) {
        const int err = GetLastError();
        return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "ReadFile ", ToUTF8String(file_path), " fail, errcode = ", err);
      }

      if (bytes_read != bytes_to_read) {
        return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "ReadFile ", ToUTF8String(file_path), " fail: unexpected end");
      }

      total_bytes_read += bytes_read;
    }

    return Status::OK();

#else //PLATFORM_NNI_MICROSOFT
  ScopedFileDescriptor file_descriptor{ open(file_path, O_RDONLY) };
  if (!file_descriptor.IsValid()) {
    return ReportSystemError("open", file_path);
  }

  if (length == 0)
    return Status::OK();

  if (offset > 0) {
    const FileOffsetType seek_result = lseek(file_descriptor.Get(), offset, SEEK_SET);
    if (seek_result == -1) {
      return ReportSystemError("lseek", file_path);
    }
  }

  size_t total_bytes_read = 0;
  while (total_bytes_read < length) {
    constexpr size_t k_max_bytes_to_read = 1 << 30;  // read at most 1GB each time
    const size_t bytes_remaining = length - total_bytes_read;
    const size_t bytes_to_read = std::min(bytes_remaining, k_max_bytes_to_read);

    const ssize_t bytes_read =
      TempFailureRetry(read, file_descriptor.Get(), buffer.data() + total_bytes_read, bytes_to_read);

    if (bytes_read == -1) {
      return ReportSystemError("read", file_path);
    }

    if (bytes_read == 0) {
      return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "ReadFileIntoBuffer - unexpected end of file. ", "File: ", file_path,
        ", offset: ", offset, ", length: ", length);
    }

    total_bytes_read += bytes_read;
  }

  return Status::OK();
#endif //PLATFORM_NNI_MICROSOFT
  }


#if defined(PLATFORM_NNI_MICROSOFT) || defined(__PROSPERO__) // WITH_UE

  Status MapFileIntoMemory(_In_z_ const ORTCHAR_T*, FileOffsetType, size_t, MappedMemoryPtr&) const override {
    return ORT_MAKE_STATUS(ONNXRUNTIME, NOT_IMPLEMENTED, "MapFileIntoMemory is not implemented on Windows or PS5.");
  }

#else // WITH_UE
  Status MapFileIntoMemory(
    const ORTCHAR_T* file_path,
    FileOffsetType offset,
    size_t length,
        MappedMemoryPtr& mapped_memory) const override {

  ORT_RETURN_IF_NOT(file_path, "file_path == nullptr");
  ORT_RETURN_IF_NOT(offset >= 0, "offset < 0");

  ScopedFileDescriptor file_descriptor{ open(file_path, O_RDONLY) };
  if (!file_descriptor.IsValid()) {
    return ReportSystemError("open", file_path);
  }

  if (length == 0) {
    mapped_memory = MappedMemoryPtr{};
    return Status::OK();
  }

#ifdef __PROSPERO__
  static const long page_size = PAGE_SIZE;
#else //__PROSPERO__
  static const long page_size = sysconf(_SC_PAGESIZE);
#endif //__PROSPERO__

  const FileOffsetType offset_to_page = offset % static_cast<FileOffsetType>(page_size);
  const size_t mapped_length = length + offset_to_page;
  const FileOffsetType mapped_offset = offset - offset_to_page;
  void* const mapped_base =
    mmap(nullptr, mapped_length, PROT_READ | PROT_WRITE, MAP_PRIVATE, file_descriptor.Get(), mapped_offset);

  if (mapped_base == MAP_FAILED) {
    return ReportSystemError("mmap", file_path);
  }

  mapped_memory =
    MappedMemoryPtr{ reinterpret_cast<char*>(mapped_base) + offset_to_page,
              OrtCallbackInvoker{OrtCallback{UnmapFile, new UnmapFileParam{mapped_base, mapped_length}}} };

  return Status::OK();
  }
#endif // WITH_UE



#ifndef PLATFORM_NNI_MICROSOFT
  static common::Status ReportSystemError(const char* operation_name, const std::string& path) {
    auto e = errno;
      char buf[1024];
      const char* msg = "";
      if (e > 0) {
#if defined(__GLIBC__) && defined(_GNU_SOURCE) && !defined(__ANDROID__)
        msg = strerror_r(e, buf, sizeof(buf));
#else
        // for Mac OS X and Android lower than API 23
    if (strerror_r(e, buf, sizeof(buf)) != 0) {
        buf[0] = '\0';
      }
      msg = buf;
#endif
      }
      std::ostringstream oss;
      oss << operation_name << " file \"" << path << "\" failed: " << msg;
      return common::Status(common::SYSTEM, e, oss.str());
    }
#endif //PLATFORM_NNI_MICROSOFT

    bool FolderExists(const std::string& path) const override {
    FString FilePathUE(path.c_str());
    IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
    const bool bExist = FileManager.DirectoryExists(*FilePathUE);
    return bExist;
  }

  common::Status CreateFolder(const std::string& path) const override {
    FString FilePathUE(path.c_str());
    IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
    bool bIsSuccess = FileManager.CreateDirectory(*FilePathUE);
    common::Status RetVal = (bIsSuccess) ? common::Status::OK() : ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "Creating Folder: ", ToUTF8String(path));

    return RetVal;
  }

  common::Status DeleteFolder(const PathString& path) const override {
    FString FilePathUE(path.c_str());
    IPlatformFile& FileManager = FPlatformFileManager::Get().GetPlatformFile();
    bool bIsSuccess = FileManager.DeleteDirectoryRecursively(*FilePathUE);
    common::Status RetVal = (bIsSuccess) ? common::Status::OK() : ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "Deleting Folder: ", ToUTF8String(path));

    return RetVal;
  }

  common::Status FileOpenRd(const std::string& path, /*out*/ int& fd) const override {
#ifdef PLATFORM_NNI_MICROSOFT
    _sopen_s(&fd, path.c_str(), _O_RDONLY | _O_SEQUENTIAL | _O_BINARY, _SH_DENYWR, _S_IREAD | _S_IWRITE);
#else //PLATFORM_NNI_MICROSOFT
   fd = open(path.c_str(), O_RDONLY);
#endif //PLATFORM_NNI_MICROSOFT

    if (0 > fd) {
      return common::Status(common::SYSTEM, errno);
    }
    return Status::OK();
  }

  common::Status FileOpenWr(const std::string& path, /*out*/ int& fd) const override {

#ifdef PLATFORM_NNI_MICROSOFT
    _sopen_s(&fd, path.c_str(), _O_CREAT | _O_TRUNC | _O_SEQUENTIAL | _O_BINARY | _O_WRONLY, _SH_DENYWR,
             _S_IREAD | _S_IWRITE);
#else //PLATFORM_NNI_MICROSOFT
  fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
#endif //PLATFORM_NNI_MICROSOFT

    if (0 > fd) {
      return common::Status(common::SYSTEM, errno);
    }
    return Status::OK();
  }

#ifdef PLATFORM_NNI_MICROSOFT
  bool FolderExists(const std::wstring& path) const override {
    const std::string path_str(path.begin(), path.end());
    const bool bExist = FolderExists(path_str);

    return bExist;
  }

  common::Status CreateFolder(const std::wstring& path) const override {
    const std::string path_str(path.begin(), path.end());
    common::Status RetVal = CreateFolder(path_str);

    return RetVal;
  }

  common::Status FileOpenRd(const std::wstring& path, /*out*/ int& fd) const override {
    _wsopen_s(&fd, path.c_str(), _O_RDONLY | _O_SEQUENTIAL | _O_BINARY, _SH_DENYWR, _S_IREAD | _S_IWRITE);
    if (0 > fd) {
      return common::Status(common::SYSTEM, errno);
    }
    return Status::OK();
  }

  common::Status FileOpenWr(const std::wstring& path, /*out*/ int& fd) const override {
    _wsopen_s(&fd, path.c_str(), _O_CREAT | _O_TRUNC | _O_SEQUENTIAL | _O_BINARY | _O_WRONLY, _SH_DENYWR,
      _S_IREAD | _S_IWRITE);
    if (0 > fd) {
      return common::Status(common::SYSTEM, errno);
    }
    return Status::OK();
  }
#endif //PLATFORM_NNI_MICROSOFT

  common::Status FileClose(int fd) const override {
#ifdef PLATFORM_NNI_MICROSOFT
    int ret = _close(fd);
#else //PLATFORM_NNI_MICROSOFT
    int ret = close(fd);
#endif //PLATFORM_NNI_MICROSOFT
    if (0 != ret) {
      return common::Status(common::SYSTEM, errno);
    }
    return Status::OK();
  }

  common::Status GetCanonicalPath(
      const PathString& path,
      PathString& canonical_path) const override {
#ifdef PLATFORM_NNI_MICROSOFT
    // adapted from MSVC STL std::filesystem::canonical() implementation
    // https://github.com/microsoft/STL/blob/ed3cbf36416a385828e7a5987ca52cb42882d84b/stl/inc/filesystem#L2986
#if WINVER >= _WIN32_WINNT_WIN8
    wil::unique_hfile file_handle{CreateFile2(
        path.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        OPEN_EXISTING,
        NULL)};
#else
    wil::unique_hfile file_handle{CreateFileW(
        path.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr)};
#endif

    if (file_handle.get() == INVALID_HANDLE_VALUE) {
      const int err = GetLastError();
      return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "open file ", ToUTF8String(path), " fail, errcode = ", err);
    }

    constexpr DWORD initial_buffer_size = MAX_PATH;
    std::vector<PathChar> result_buffer{};
    result_buffer.resize(initial_buffer_size);

    while (true) {
      const DWORD result_length = GetFinalPathNameByHandleW(
          file_handle.get(),
          result_buffer.data(),
          static_cast<DWORD>(result_buffer.size()),
          0);

      ORT_RETURN_IF_NOT(
          result_length > 0, "GetFinalPathNameByHandle() failed: ", GetLastError());

      if (result_length < result_buffer.size()) {  // buffer is large enough
        canonical_path.assign(result_buffer.data(), result_length);
        break;
      }

      // need larger buffer
      result_buffer.resize(result_length);
    }

    // update prefixes
    if (canonical_path.find(ORT_TSTR(R"(\\?\)")) == 0) {
      if (canonical_path.size() > 6 &&
          (ORT_TSTR('A') <= canonical_path[4] && canonical_path[4] <= ORT_TSTR('Z') ||
           ORT_TSTR('a') <= canonical_path[4] && canonical_path[4] <= ORT_TSTR('z')) &&
          canonical_path[5] == ORT_TSTR(':')) {
        // "\\?\<drive>:" -> "<drive>:"
        canonical_path.erase(0, 4);
      } else if (canonical_path.find(ORT_TSTR(R"(UNC\)"), 4) == 4) {
        // "\\?\UNC\" -> "\\"
        canonical_path.erase(2, 6);
      }
    }

    return Status::OK();

#else //PLATFORM_NNI_MICROSOFT
#ifdef __PROSPERO__ // WITH_UE
    MallocdStringPtr canonical_path_cstr{ strdup(path.c_str()) };
#else //__PROSPERO__
    MallocdStringPtr canonical_path_cstr{ realpath(path.c_str(), nullptr) };
#endif //__PROSPERO__

  if (!canonical_path_cstr) {
    return ReportSystemError("realpath", path);
  }
  canonical_path.assign(canonical_path_cstr.get());
  return Status::OK();
#endif //PLATFORM_NNI_MICROSOFT
  }

#ifdef PLATFORM_NNI_MICROSOFT
  // Return the path of the executable/shared library for the current running code. This is to make it
  // possible to load other shared libraries installed next to our core runtime code.
  std::string GetRuntimePath() const override {
    char buffer[MAX_PATH];
    if (!GetModuleFileNameA(reinterpret_cast<HINSTANCE>(&__ImageBase), buffer, _countof(buffer)))
      return "";

    // Remove the filename at the end, but keep the trailing slash
    std::string path(buffer);
    auto slash_index = path.find_last_of('\\');
    if (slash_index == std::string::npos)
      return "";

    return path.substr(0, slash_index + 1);
  }
#endif

  virtual Status LoadDynamicLibrary(const std::string& library_filename, bool global_symbols, void** handle) const override {
#ifndef WITH_UE
#if WINAPI_FAMILY == WINAPI_FAMILY_PC_APP
    *handle = ::LoadPackagedLibrary(ToWideString(library_filename).c_str(), 0);
#else
    *handle = ::LoadLibraryExA(library_filename.c_str(), nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
#endif

#else //WITH_UE
    FString PathToDll = FString(library_filename.c_str());
    *handle = FPlatformProcess::GetDllHandle(*PathToDll);
#endif //WITH_UE
    if (!*handle) {
      const auto error_code = FGenericPlatformMisc::GetLastError();
      return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "Failed to load library, error code: ", error_code);
    }

    return Status::OK();
  }

  virtual Status UnloadDynamicLibrary(void* handle) const override {
#ifndef WITH_UE
    if (::FreeLibrary(reinterpret_cast<HMODULE>(handle)) == 0) {
      const auto error_code = GetLastError();
      return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL, "Failed to unload library, error code: ", error_code);
    }
#else //WITH_UE
    FPlatformProcess::FreeDllHandle(handle);
#endif //WITH_UE

    return Status::OK();
  }

  virtual Status GetSymbolFromLibrary(void* handle, const std::string& symbol_name, void** symbol) const override {
#ifndef WITH_UE
    *symbol = ::GetProcAddress(reinterpret_cast<HMODULE>(handle), symbol_name.c_str());
#else //WITH_UE
    FString SymbolNameUE = FString(symbol_name.c_str());
    *symbol = FPlatformProcess::GetDllExport(handle, *SymbolNameUE);
#endif //WITH_UE

    if (!*symbol) {
      const auto error_code = FGenericPlatformMisc::GetLastError();
      return ORT_MAKE_STATUS(ONNXRUNTIME, FAIL,
        "Failed to find symbol in library, error code: ",
        error_code);
    }
    return Status::OK();
  }

  virtual std::string FormatLibraryFileName(const std::string& name, const std::string& version) const override {
    ORT_UNUSED_PARAMETER(name);
    ORT_UNUSED_PARAMETER(version);
    ORT_NOT_IMPLEMENTED(__FUNCTION__, " is not implemented");
  }

  // \brief returns a provider that will handle telemetry on the current platform
  const Telemetry& GetTelemetryProvider() const override {
    return telemetry_provider_;
  }

  // \brief returns a value for the queried variable name (var_name)
  std::string GetEnvironmentVar(const std::string& var_name) const override {

#ifndef WITH_UE
    // Why getenv() should be avoided on Windows:
    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/getenv-wgetenv
    // Instead use the Win32 API: GetEnvironmentVariableA()

    // Max limit of an environment variable on Windows including the null-terminating character
    constexpr DWORD kBufferSize = 32767;

    // Create buffer to hold the result
    char buffer[kBufferSize];

    auto char_count = GetEnvironmentVariableA(var_name.c_str(), buffer, kBufferSize);

    // Will be > 0 if the API call was successful
    if (char_count) {
      return std::string(buffer, buffer + char_count);
    }

    // TODO: Understand the reason for failure by calling GetLastError().
    // If it is due to the specified environment variable being found in the environment block,
    // GetLastError() returns ERROR_ENVVAR_NOT_FOUND.
    // For now, we assume that the environment variable is not found.

    return std::string();

#else //WITH_UE
    FString EnvVarName = FString(var_name.c_str());
    FString VarVal = FGenericPlatformMisc::GetEnvironmentVariable(*EnvVarName);
    std::string RetVal = TCHAR_TO_UTF8(*VarVal);
    return RetVal;
#endif //WITH_UE
  }

 private:
  Telemetry telemetry_provider_;
};
}  // namespace

Env& Env::Default() {
  return UnrealEngineEnv::Instance();
}

}  // namespace onnxruntime
