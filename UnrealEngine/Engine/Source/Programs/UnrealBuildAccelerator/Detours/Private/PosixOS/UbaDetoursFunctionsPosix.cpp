// Copyright Epic Games, Inc. All Rights Reserved.

#include "UbaDetoursFileMappingTable.h"
#include "UbaBinaryReaderWriter.h"
#include "UbaDirectoryTable.h"
#include "UbaPlatform.h"
#include "UbaProcessStats.h"
#include "UbaProtocol.h"
#include "UbaStringBuffer.h"
#include "UbaTimer.h"
#include <sys/wait.h>
#include <dirent.h> 
#include <glob.h>
#include <dlfcn.h>
#include <sys/stat.h>

// undefs for mimalloc
#undef realpath
#undef malloc

#if PLATFORM_LINUX
#include <sys/prctl.h>
#else
#include <crt_externs.h>
#endif

namespace uba
{
	constexpr bool g_logToScreen = false;

	bool g_isDetouring;
	bool g_isInitialized;
	void Deinit();
}

using namespace uba;

#define UBA_EXPORT __attribute__((visibility("default"))) 

#define DETOURED_FUNCTIONS \
	DETOURED_FUNCTION(chdir) \
	DETOURED_FUNCTION(fchdir) \
	DETOURED_FUNCTION(chroot) \
	DETOURED_FUNCTION(getcwd) \
	DETOURED_FUNCTION(getenv) \
	DETOURED_FUNCTION(setenv) \
	DETOURED_FUNCTION(realpath) \
	DETOURED_FUNCTION(readlink) \
	DETOURED_FUNCTION(readlinkat) \
	DETOURED_FUNCTION(read) \
	DETOURED_FUNCTION(pread) \
	DETOURED_FUNCTION(open) \
	DETOURED_FUNCTION(dup) \
	DETOURED_FUNCTION(dup2) \
	DETOURED_FUNCTION(close) \
	DETOURED_FUNCTION(fopen) \
	DETOURED_FUNCTION(fdopen) \
	DETOURED_FUNCTION(fchmod) \
	DETOURED_FUNCTION(fchmodat) \
	DETOURED_FUNCTION(fstat) \
	DETOURED_FUNCTION(futimens) \
	DETOURED_FUNCTION(fclose) \
	DETOURED_FUNCTION(opendir) \
	DETOURED_FUNCTION(fdopendir) \
	DETOURED_FUNCTION(dirfd) \
	DETOURED_FUNCTION(readdir) \
	DETOURED_FUNCTION(rewinddir) \
	DETOURED_FUNCTION(scandir) \
	DETOURED_FUNCTION(seekdir) \
	DETOURED_FUNCTION(telldir) \
	DETOURED_FUNCTION(closedir) \
	DETOURED_FUNCTION(stat) \
	DETOURED_FUNCTION(truncate) \
	DETOURED_FUNCTION(lstat) \
	DETOURED_FUNCTION(glob) \
	DETOURED_FUNCTION(chmod) \
	DETOURED_FUNCTION(rename) \
	DETOURED_FUNCTION(renameat) \
	DETOURED_FUNCTION(utimensat) \
	DETOURED_FUNCTION(remove) \
	DETOURED_FUNCTION(symlink) \
	DETOURED_FUNCTION(access) \
	DETOURED_FUNCTION(posix_spawn) \
	DETOURED_FUNCTION(posix_spawnp) \
	DETOURED_FUNCTION(wait) \
	DETOURED_FUNCTION(waitpid) \
	DETOURED_FUNCTION(waitid) \
	DETOURED_FUNCTION(wait3) \
	DETOURED_FUNCTION(wait4) \
	DETOURED_FUNCTION(system) \
	DETOURED_FUNCTION(execv) \
	DETOURED_FUNCTION(execve) \
	DETOURED_FUNCTION(execvp) \
	DETOURED_FUNCTION(fork) \
	DETOURED_FUNCTION(popen) \
	DETOURED_FUNCTION(exit) \
	DETOURED_FUNCTION(_exit) \
	DETOURED_FUNCTION(_Exit) \
	DETOURED_FUNCTION_DEBUG \
	DETOURED_FUNCTION_LINUX \
	DETOURED_FUNCTION_MACOS \

	//DETOURED_FUNCTION(write)
	//DETOURED_FUNCTION(fwrite)

#if UBA_DEBUG && PLATFORM_LINUX
#define DETOURED_FUNCTION_DEBUG \

#else
#define DETOURED_FUNCTION_DEBUG
#endif

#if PLATFORM_LINUX
#define DETOURED_FUNCTION_MACOS
#define DETOURED_FUNCTION_LINUX \
	DETOURED_FUNCTION(get_current_dir_name) \
	DETOURED_FUNCTION(fopen64) \
	DETOURED_FUNCTION(fcntl) \
	DETOURED_FUNCTION(__xstat) \
	DETOURED_FUNCTION(__xstat64) \
	DETOURED_FUNCTION(__fxstat) \
	DETOURED_FUNCTION(__fxstat64) \
	DETOURED_FUNCTION(__fxstatat) \
	DETOURED_FUNCTION(__fxstatat64) \
	DETOURED_FUNCTION(open64) \
	DETOURED_FUNCTION(fstatat64) \
	DETOURED_FUNCTION(vfork) \

#else
#define DETOURED_FUNCTION_LINUX
#define DETOURED_FUNCTION_MACOS \
	DETOURED_FUNCTION(_NSGetExecutablePath) \

#endif

#if (PLATFORM_MAC)
	// On Apple platforms when interposing, we need to have a unique name of our function
	// While on Linux we need to use original name
	// This macro will prepend "uba_" to the mac versions of the functions
	// so they can be used with the interpose macro 
	#define UBA_WRAPPER(func) uba_##func
	#define TRUE_WRAPPER(func) func

	// This magic macro that actually does the hooking, aka interposing on Apple platforms
	#ifndef DYLD_INTERPOSE
	#define DYLD_INTERPOSE(_replacement,_replacee) \
		__attribute__((used)) static struct{ const void* replacement; const void* replacee; } _interpose_##_replacee \
			__attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_replacement, (const void*)(unsigned long)&_replacee };
	#endif // DYLD_INTERPOSE

#else
	#define UBA_WRAPPER(func) func
	#define TRUE_WRAPPER(func) True_##func

	#define DETOURED_FUNCTION(func) \
		using Symbol_##func = decltype(func); \
		Symbol_##func* True_##func; 
	DETOURED_FUNCTIONS
	#undef DETOURED_FUNCTION

#endif

#if 0
#define	EPERM		 1	/* Operation not permitted */
#define	ENOENT		 2	/* No such file or directory */
#define	ESRCH		 3	/* No such process */
#define	EINTR		 4	/* Interrupted system call */
#define	EIO			 5	/* I/O error */
#define	ENXIO		 6	/* No such device or address */
#define	E2BIG		 7	/* Argument list too long */
#define	ENOEXEC		 8	/* Exec format error */
#define	EBADF		 9	/* Bad file number */
#define	ECHILD		10	/* No child processes */
#define	EAGAIN		11	/* Try again */
#define	ENOMEM		12	/* Out of memory */
#define	EACCES		13	/* Permission denied */
#define	EFAULT		14	/* Bad address */
#define	ENOTBLK		15	/* Block device required */
#define	EBUSY		16	/* Device or resource busy */
#define	EEXIST		17	/* File exists */
#define	EXDEV		18	/* Cross-device link */
#define	ENODEV		19	/* No such device */
#define	ENOTDIR		20	/* Not a directory */
#define	EISDIR		21	/* Is a directory */
#define	EINVAL		22	/* Invalid argument */
#define	ENFILE		23	/* File table overflow */
#define	EMFILE		24	/* Too many open files */
#define	ENOTTY		25	/* Not a typewriter */
#define	ETXTBSY		26	/* Text file busy */
#define	EFBIG		27	/* File too large */
#define	ENOSPC		28	/* No space left on device */
#define	ESPIPE		29	/* Illegal seek */
#define	EROFS		30	/* Read-only file system */
#define	EMLINK		31	/* Too many links */
#define	EPIPE		32	/* Broken pipe */
#define	EDOM		33	/* Math argument out of domain of func */
#define	ERANGE		34	/* Math result not representable */
#endif

namespace uba
{
	template<typename TrueFunc>
	void InitDetour(TrueFunc*& trueFunc, const char* func)
	{
		if (trueFunc)
			return;
		trueFunc = (TrueFunc*)dlsym(RTLD_NEXT, func);
		if (trueFunc)
			return;
		printf("dlsym failed on %s: %s\n", func, dlerror());
	}

#if PLATFORM_LINUX
	#define UBA_INIT_DETOUR(func, ...) \
		if (!g_isDetouring || t_disallowDetour) \
		{ \
			InitDetour(True_##func, #func); \
			return TRUE_WRAPPER(func)(__VA_ARGS__); \
		}
#elif PLATFORM_MAC
	#define UBA_INIT_DETOUR(func, ...) \
		if (!g_isDetouring || t_disallowDetour) \
		{ \
			return TRUE_WRAPPER(func)(__VA_ARGS__); \
		}
#endif

	const char* StrError(int res, int error)
	{
		if (res != -1)
			return "Success";
		return strerror(error);
	}

	enum : u8 { AccessFlag_Read = 1, AccessFlag_Write = 2 };
	u8 GetFileAccessFlags(int flags)
	{
		u8 access = 0;
		if (flags & O_RDWR)
			access |= AccessFlag_Read | AccessFlag_Write;
		else if (flags & O_RDONLY)
			access |= AccessFlag_Read;
		else if (flags & O_WRONLY)
			access |= AccessFlag_Write;
		return access;
	}

	struct FileObject
	{
		FileInfo* fileInfo = nullptr;
		u32 refCount = 1;
		u32 closeId = 0;
		u32 desiredAccess = 0;
		bool deleteOnClose = false;
		bool ownsFileInfo = false;
		TString newName;
	};

	// TODO: Change this to same style as windows implementation
	struct DetouredHandle
	{
		FileObject* fileObject = nullptr;
	};

	using FileHandles = UnorderedMap<int, DetouredHandle>;
	VARIABLE_MEM(StringBuffer<MaxPath>, g_exeDir);
	VARIABLE_MEM(FileHandles, g_fileHandles);
	VARIABLE_MEM(ReaderWriterLock, g_fileHandlesLock);


	StringKey ToFilenameKey(const StringBufferBase& b)
	{
		return CaseInsensitiveFs ? ToStringKeyLower(b) : ToStringKey(b);
	}
}

// Shared functions

template<typename TrueOpen>
int Shared_open(const char* funcName, const char* file, int flags, int mode, const TrueOpen& trueOpen)
{
	if (!g_isDetouring || t_disallowDetour || strncmp(file, "/dev/", 5) == 0 || strncmp(file, "/etc/", 5) == 0)
	{
		int res = trueOpen(file, flags, mode);
		DEBUG_LOG_TRUE(funcName, "NODETOUR (%s) -> %i", file, res);
		return res;
	}
	StringBuffer<> fileName;
	FixPath(fileName, file);

	StringKey fileNameKey = ToFilenameKey(fileName);

	bool keepInMemory = false;
	bool isSystemOrTempFile = StartsWith(file, g_systemTemp.data);

	const char* realFileName = fileName.data;
	u64 size = InvalidValue;
	u32 closeId = 0;
	u32 desiredAccess = GetFileAccessFlags(flags);
	bool isWrite = desiredAccess & AccessFlag_Write;
	u32 dirTableOffset = ~u32(0);

#if UBA_DEBUG_LOG_ENABLED
	const char* isWriteStr = isWrite ? " WRITE" : ""; (void)isWriteStr;
	#endif

	if (g_allowDirectoryCache)
	{
		if (!isWrite && !isSystemOrTempFile) // We need to skip SystemTemp.. lots of stuff going on there.
		{
			dirTableOffset = Rpc_GetEntryOffset(fileNameKey, fileName.data, fileName.count, false);
			bool allowEarlyOut = true;
			if (dirTableOffset == ~u32(0))
			{
				// This could be a written file not reported to server yet
				{
					SCOPED_READ_LOCK(g_mappedFileTable.m_lookupLock, lock);
					auto findIt = g_mappedFileTable.m_lookup.find(fileNameKey);
					if (findIt != g_mappedFileTable.m_lookup.end())
						allowEarlyOut = findIt->second.deleted;
				}
				if (allowEarlyOut)
				{
					//SetLastError(ERROR_FILE_NOT_FOUND); // Don't think this is needed
					errno = ENOENT;
					DEBUG_LOG_DETOURED(funcName, "NOTFOUND_USINGTABLE (%s) (%s) -> -1", fileName.data, KeyToString(fileNameKey).data);
					return -1;
				}
			}
			else
			{
				// File could have been deleted.
				DirectoryTable::EntryInformation entryInfo;
				g_directoryTable.GetEntryInformation(entryInfo, dirTableOffset);
				if (entryInfo.attributes == 0)
				{
					errno = ENOENT;
					return -1;
				}
			}
		}
	}


	SCOPED_WRITE_LOCK(g_mappedFileTable.m_lookupLock, _);
	auto insres = g_mappedFileTable.m_lookup.try_emplace(fileNameKey);
	FileInfo& info = insres.first->second;
	FileInfo* fileInfo = &info;
	u32 lastDesiredAccess = info.lastDesiredAccess;
	if (insres.second)
	{
		info.originalName = g_memoryBlock.Strdup(fileName.data);
		info.name = info.originalName;
		if (!keepInMemory && !isSystemOrTempFile)
		{
			char newFileName[512];
			Rpc_CreateFileW(realFileName, fileNameKey, u8(desiredAccess), newFileName, sizeof_array(newFileName), size, closeId, false);
			info.name = g_memoryBlock.Strdup(newFileName);
			realFileName = info.name;
		}

		info.size = size;
		info.fileNameKey = fileNameKey;
		info.lastDesiredAccess = desiredAccess;
	}
	else
	{
		if (!info.originalName)
			info.originalName = g_memoryBlock.Strdup(fileName.data);
		if (isWrite)
		{
			//UBA_ASSERT(!info.isFileMap);
			bool shouldReport = !(info.lastDesiredAccess & AccessFlag_Write) || info.deleted;
			shouldReport = shouldReport && !keepInMemory;
			if (shouldReport)
			{
				info.deleted = false;
				char newFileName[1024];
				Rpc_CreateFileW(realFileName, fileNameKey, u8(desiredAccess), newFileName, sizeof_array(newFileName), size, closeId, false);
				info.name = g_memoryBlock.Strdup(newFileName);
				realFileName = info.name;
			}
			if (desiredAccess == 0 || info.lastDesiredAccess == 0)
				realFileName = info.name;
			info.lastDesiredAccess |= desiredAccess;
		}
		else if (info.deleted)
		{
			realFileName = "";
		}
		else
		{
			size = info.size;
			realFileName = info.name;
		}
	}

	if (realFileName[0] == '$')
	{
		if (realFileName[1] == 'd') // This is a directory and we will need to fake it locally.. 
		{
			int fd = TRUE_WRAPPER(open)("/dev/null", O_RDONLY);
			SCOPED_WRITE_LOCK(g_fileHandlesLock, lock);
			auto insres2 = g_fileHandles.insert({ fd, DetouredHandle() });
			UBA_ASSERTF(insres2.second, "File handle for directory already added");
			DetouredHandle& h = insres2.first->second;
			auto fo = new FileObject();
			fo->closeId = closeId;
			fo->fileInfo = fileInfo;
			fo->desiredAccess = desiredAccess;
			h.fileObject = fo;
			return fd;
		}

		DEBUG_LOG_DETOURED(funcName, "FAILED %s (%s)", fileName.data, realFileName);
		UBA_ASSERTF(false, "unsupported filename %s", realFileName);
		return -1;
	}

	if (realFileName[0] == '^')
	{
		DEBUG_LOG_DETOURED(funcName, "FAILED %s (%s)", fileName.data, realFileName);
		UBA_ASSERTF(false, "^ filenames not implemented");
		return -1;
	}

	if (keepInMemory)// || info.memoryFile)
	{
		DEBUG_LOG_DETOURED(funcName, "FAILED %s (%s)", fileName.data, realFileName);
		UBA_ASSERTF(false, "keepInMemory not implemented");
		return -1;
	}

	const char* tempFileName = realFileName;
	if (tempFileName[0] == '#')
		tempFileName = fileName.data;
	else
		tempFileName = info.name;

	int fd = trueOpen(tempFileName, flags, mode);

	DEBUG_LOG_TRUE(funcName, "%s%s (%s) %i %i -> %i (%s)", file, isWriteStr, tempFileName, flags, mode, fd, StrError(fd, errno));
	if (fd == -1)
		return fd;

	SCOPED_WRITE_LOCK(g_fileHandlesLock, lock);
	auto insres2 = g_fileHandles.insert({ fd, DetouredHandle() });
	UBA_ASSERTF(insres2.second, "File handle already added");
	DetouredHandle& h = insres2.first->second;
	auto fo = new FileObject();
	fo->closeId = closeId;
	fo->fileInfo = fileInfo;
	fo->desiredAccess = desiredAccess;
	h.fileObject = fo;
	return fd;
}

template<typename TrueOpen>
FILE* Shared_fopen(const char* funcName, const char* path, const char* mode, const char* trueOpenName, const TrueOpen& trueOpen)
{
	bool r = strchr(mode, 'r');
	bool w = strchr(mode, 'w');
	bool a = strchr(mode, 'a');
	bool p = strchr(mode, '+');
	//bool b = strchr(mode, 'b');
	//bool t = strchr(mode, 't');
	UBA_ASSERTF(!a, "%s with append not implemented (%s)", funcName, mode);

	int flags = 0;
	if (r)
	{
		UBA_ASSERTF(!p, "%s with + not implemented (%s)", funcName, mode);
		flags = O_NONBLOCK | O_RDONLY;
	}
	else if (w)
	{
		flags = O_CREAT | O_TRUNC;
		if (p)
			flags |= O_RDWR;
		else
			flags |= O_WRONLY;
	}
	int openMode = S_IRUSR | S_IWUSR;
	int fd = Shared_open(trueOpenName, path, flags, openMode, trueOpen);
	if (fd == -1)
	{
		DEBUG_LOG_DETOURED(funcName, "(%s) -> FAILED", path);
		return nullptr;
	}

	FILE* res = TRUE_WRAPPER(fdopen)(fd, mode);
	DEBUG_LOG_TRUE(funcName, "%i (%s  %s) -> %p", fd, path, mode, res);
	return res;
}

template<typename TrueClose>
void Shared_close(int fd, const TrueClose& trueClose)
{
	if (!g_isDetouring)
	{
		trueClose();
		return;
	}

	SCOPED_WRITE_LOCK(g_fileHandlesLock, lock);
	auto findIt = g_fileHandles.find(fd);
	if (findIt == g_fileHandles.end())
	{
		trueClose();
		return;
	}

	DetouredHandle& h = findIt->second;
	FileObject* fo = h.fileObject;
	UBA_ASSERTF(fo->refCount >= 1, "FileObject needs to have ref count when closed");
	g_fileHandles.erase(findIt);
	lock.Leave();

	trueClose();

	if (--fo->refCount)
		return;

	u64 mappingHandle = 0;
	u64 mappingWritten = 0;
	FileInfo& fi = *fo->fileInfo;
	const tchar* path = fi.name;

	if (fo->closeId)
	{
		//for (auto& kv : g_fileHandles)
		//	UBA_ASSERT(&fi != kv.second.fileObject->fileInfo);
		Rpc_UpdateCloseHandle(path, fo->closeId, fo->deleteOnClose, fo->newName.c_str(), mappingHandle, mappingWritten, true);
	}
	delete fo;
}

template<typename True_fstat>
int Shared_fstat(const char* funcName, int fd, struct stat* attr, const True_fstat& trueFstat)
{
	//if (!g_isDetouring || t_disallowDetour)
	//	return trueFstat(fd, attr);

	SCOPED_READ_LOCK(g_fileHandlesLock, lock);
	auto findIt = g_fileHandles.find(fd);
	if (findIt == g_fileHandles.end())
	{
		int res = trueFstat(fd, attr);
		DEBUG_LOG_TRUE(funcName, "(%i) -> %i (%s)", fd, res, StrError(res, errno));
		return res;
	}

	FileObject& fo = *findIt->second.fileObject;
	FileInfo& fi = *fo.fileInfo;

	if (fo.desiredAccess & AccessFlag_Write)
	{
		int res = trueFstat(fd, attr);
		DEBUG_LOG_TRUE(funcName, "(%i) (%s) -> %i (%s)", fd, fi.originalName, res, StrError(res, errno));
		return res;
	}

	FileAttributes fileAttr;
	const char* realName = Shared_GetFileAttributes(fileAttr, fi.originalName);

	if (!fileAttr.useCache)
	{
		lock.Leave();
		int res = trueFstat(fd, attr);
		DEBUG_LOG_TRUE(funcName, "(%i) -> %i (%s)", fd, res, StrError(res, errno));
		return res;
	}


	int res = fileAttr.lastError == 0 ? 0 : -1;

	DEBUG_LOG_DETOURED(funcName, "(%i) (name: %s size: %llu id: %llu dev: %u)-> %i (%s)", fd, fi.originalName, fileAttr.data.st_size, fileAttr.data.st_ino, fileAttr.data.st_dev, res, StrError(res, fileAttr.lastError));

	errno = fileAttr.lastError;
	memcpy(attr, &fileAttr.data, sizeof(struct stat));

	#if UBA_DEBUG_VALIDATE
	if (!g_runningRemote)
	{
		struct stat attr2;
		int res2 = trueFstat(fd, &attr2);
		UBA_ASSERTF(res == res2, "fstat: return value differs for %s (%i vs %i)", fi.originalName, res, res2);
		if (res != -1)
		{
			bool isDir = S_ISDIR(attr->st_mode);
			UBA_ASSERTF(isDir == S_ISDIR(attr2.st_mode), "fstat: isDir not matching");
			//UBA_ASSERT(attr->st_mode == attr2.st_mode);
			//UBA_ASSERT(attr->st_dev == attr2.st_dev)
			UBA_ASSERTF(attr->st_ino == attr2.st_ino, "fstat: st_ino mismatch for %s (%llu vs %llu)", fi.originalName, attr->st_ino, attr2.st_ino);
			UBA_ASSERTF(isDir || attr->st_size == attr2.st_size, "fstat: size not matching");
			UBA_ASSERTF(isDir || FromTimeSpec(attr->st_mtimespec) == FromTimeSpec(attr2.st_mtimespec), "fstat: st_mtim mismatch for %s (%llu vs %llu)", fi.originalName, FromTimeSpec(attr->st_mtimespec), FromTimeSpec(attr2.st_mtimespec));
		}
		else
		{
			UBA_ASSERTF(fileAttr.lastError == errno, "fstat: error not matching");
		}
	}
	#endif

	return res;
}

template<typename True_stat>
int Shared_stat(const char* funcName, const char* file, struct stat* attr, const True_stat& trueStat)
{
	StringBuffer<> fixedFile;
	if (!FixPath(fixedFile, file) || fixedFile.Equals("/") || fixedFile.StartsWith("/etc/") || fixedFile.StartsWith(g_systemTemp.data))//(access(file, F_OK) != 0))
	{
		int res =  trueStat(file, attr);
		return res;
	}

	UBA_ASSERTF(fixedFile.count, "FixPath failed with %s", file);

	StringBuffer<> temp;
	if (g_runningRemote && fixedFile.StartsWith(g_exeDir.data))
	{
		temp.Append(g_virtualApplicationDir).Append(fixedFile.data + g_exeDir.count);
		fixedFile.Clear().Append(temp);
	}

	FileAttributes fileAttr;
	const char* realName = Shared_GetFileAttributes(fileAttr, fixedFile.data);

	if (!fileAttr.useCache)
	{
		int res = trueStat(realName, attr);
		DEBUG_LOG_TRUE(funcName, "%s (%s) -> %i", file, realName, res);
		return res;
	}

	int res = fileAttr.lastError == 0 ? 0 : -1;

	DEBUG_LOG_DETOURED(funcName, "%s (%s size: %llu id: %llu dev: %u)-> %i (%s)", file, realName, fileAttr.data.st_size, fileAttr.data.st_ino, fileAttr.data.st_dev, res, StrError(res, fileAttr.lastError));

	errno = fileAttr.lastError;
	memcpy(attr, &fileAttr.data, sizeof(fileAttr.data));

	#if UBA_DEBUG_VALIDATE
	if (!g_runningRemote)
	{
		struct stat attr2;
		int res2 = trueStat(file, &attr2);
		UBA_ASSERTF(res == res2, "stat: return value differs for %s (cached %i vs actual %i) [fixed: %s]", file, res, res2, fixedFile.data);
		if (res != -1)
		{
			bool isDir = S_ISDIR(attr->st_mode);
			UBA_ASSERT(isDir == S_ISDIR(attr2.st_mode));
			//UBA_ASSERT(attr->st_mode == attr2.st_mode);
			//UBA_ASSERT(attr->st_dev == attr2.st_dev)
			UBA_ASSERTF(attr->st_ino == attr2.st_ino, "stat: st_ino mismatch for %s (%llu vs %llu)", file, attr->st_ino, attr2.st_ino);
			UBA_ASSERT(isDir || attr->st_size == attr2.st_size);
			UBA_ASSERTF(isDir || FromTimeSpec(attr->st_mtimespec) == FromTimeSpec(attr2.st_mtimespec), "stat: st_mtim mismatch for %s (%llu vs %llu)", file, FromTimeSpec(attr->st_mtimespec), FromTimeSpec(attr2.st_mtimespec));
		}
		else
		{
			UBA_ASSERTF(fileAttr.lastError == errno || ((fileAttr.lastError == ENOTDIR || fileAttr.lastError == ENOENT) && (errno == ENOTDIR || errno == ENOENT))
				, "Detoured stat returned a different error. Returned %i (%s) but should return %i (%s)", fileAttr.lastError, strerror(fileAttr.lastError), errno, strerror(errno));
		}
	}
	#endif
	return res;
}




// Detoured functions
#if PLATFORM_MAC
//extern "C" {
//char* realpath$DARWIN_EXTSN(const char* path, char* resolved_path);
//}
UBA_EXPORT int UBA_WRAPPER(_NSGetExecutablePath)(char* buf, uint32_t* bufsize)
{
	if (!g_isDetouring)
	{
		return TRUE_WRAPPER(_NSGetExecutablePath)(buf, bufsize);
	}
	memcpy(buf, g_virtualApplication.data, g_virtualApplication.count + 1);
	return 0;
}

//UBA_EXPORT int UBA_WRAPPER(_NSGetEnviron)(char* buf, uint32_t* bufsize)
//{
//int res = TRUE_WRAPPER(_NSGetEnviron)(buf, bufsize);
//printf("%s for %s\n", __func__, buf);
//return TRUE_WRAPPER(_NSGetEnviron)(buf, bufsize);
//}
//extern "C" char* UBA_WRAPPER(realpath$DARWIN_EXTSN)(const char* path, char* resolved_path)
//{
//printf(">>>>>> uba_realpathDARWIN: %s\n", path);
//char* res = TRUE_WRAPPER(realpath$DARWIN_EXTSN)(path, resolved_path);
//printf("<<<<< uba_realpathDARWIN: %s\n", strlen(resolved_path) > 0 ? resolved_path : "(NULL)");
//
//return res;
//}
#endif


UBA_EXPORT int UBA_WRAPPER(chdir)(const char* path)
{
	UBA_INIT_DETOUR(chdir, path);
	UBA_ASSERTF(false, "chdir not implemented");
	return TRUE_WRAPPER(chdir)(path);
}

UBA_EXPORT int UBA_WRAPPER(fchdir)(int fd)
{
	UBA_INIT_DETOUR(fchdir, fd);
	UBA_ASSERTF(false, "fchdir not implemented");
	return TRUE_WRAPPER(fchdir)(fd);
}

UBA_EXPORT int UBA_WRAPPER(chroot)(const char* path)
{
	UBA_INIT_DETOUR(chroot, path);
	UBA_ASSERTF(false, "chroot not implemented");
	return TRUE_WRAPPER(chroot)(path);
}

UBA_EXPORT char* UBA_WRAPPER(getcwd)(char* buf, size_t size)
{
	UBA_INIT_DETOUR(getcwd, buf, size);
	UBA_ASSERTF(g_virtualWorkingDir.count < size, "getcwd with size smaller than path not implemented");
	memcpy(buf, g_virtualWorkingDir.data, g_virtualWorkingDir.count+1);
	DEBUG_LOG_DETOURED("getcwd", "%s -> %p", buf, buf);
	return buf;
}

UBA_EXPORT char* UBA_WRAPPER(getenv)(const char* name)
{
	UBA_INIT_DETOUR(getenv, name);
	auto res = TRUE_WRAPPER(getenv)(name);
	DEBUG_LOG_TRUE("getenv", "(%s) -> %s", name, res ? res : "<null>");
	return res;
}

UBA_EXPORT int UBA_WRAPPER(setenv)(const char* name, const char* value, int replace)
{
	UBA_INIT_DETOUR(setenv, name, value, replace);
	auto res = TRUE_WRAPPER(setenv)(name, value, replace);
	DEBUG_LOG_TRUE("setenv", "(%s) -> %s (%i)", name, value, res);
	return res;
}

UBA_EXPORT char* UBA_WRAPPER(realpath)(const char* path, char* resolved_path)
{
	UBA_INIT_DETOUR(realpath, path, resolved_path);

	if (!g_runningRemote)
	{
		auto res = TRUE_WRAPPER(realpath)(path, resolved_path);
		DEBUG_LOG_TRUE("realpath", "(%s) -> %s", path, res);
		return res;
	}

	// TODO
	// TODO We know this might blow up... it doesn't really resolve any links
	// TODO
	StringBuffer<> fixedPath;
	FixPath(fixedPath, path);
	if (!resolved_path)
		resolved_path = (char*)malloc(fixedPath.count + 1);
	memcpy(resolved_path, fixedPath.data, fixedPath.count + 1);
	DEBUG_LOG_DETOURED("realpath", "(%s) -> %s", path, resolved_path);
	return resolved_path;
}

UBA_EXPORT ssize_t UBA_WRAPPER(readlink)(const char* pathname, char* buf, size_t bufsiz)
{
	// Beautiful hack. Some of our tools use je_malloc and dlsym do memory allocation so we end up in a deadlock when initializing detour (since detour use dlsym to figure out true function)
	if (!g_isDetouring && Equals(pathname, "/etc/je_malloc.conf"))
	{
		errno = ENOENT;
		return -1;
	}

	UBA_INIT_DETOUR(readlink, pathname, buf, bufsiz);

	if (Equals(pathname, "/proc/self/exe"))
	{
		UBA_ASSERTF(g_virtualApplication.count < bufsiz, "readLink: buffer size smaller than path not implemented");
		memcpy(buf, g_virtualApplication.data, g_virtualApplication.count + 1);
		DEBUG_LOG_DETOURED("readlink", "(%s) (%s) -> %u", pathname, buf, g_virtualApplication.count);
		return g_virtualApplication.count;
	}
	else if (StartsWith(pathname, "/proc/self/fd/"))
	{
		StringBuffer<16> fdStr;
		fdStr.Append(pathname + 14);
		u32 fd;
		if (!fdStr.Parse(fd))
			UBA_ASSERTF(false, "Failed to parse /proc/self/fd");
		SCOPED_READ_LOCK(g_fileHandlesLock, lock);
		auto findIt = g_fileHandles.find(fd);
		if (findIt != g_fileHandles.end())
		{
			DetouredHandle& h = findIt->second;
			FileObject* fo = h.fileObject;
			FileInfo& info = *fo->fileInfo;
			u32 len = TStrlen(info.originalName);
			UBA_ASSERTF(len < bufsiz, "buffer size is smaller than length of name");
			memcpy(buf, info.originalName, len + 1);
			DEBUG_LOG_DETOURED("readlink", "(%s) (%s) -> %u", pathname, buf, len);
			return len;
		}
	}
	auto res = TRUE_WRAPPER(readlink)(pathname, buf, bufsiz);
	DEBUG_LOG_TRUE("readlink", "(%s) (%s) -> %llu", pathname, buf, u64(res));
	return res;
}

ssize_t UBA_WRAPPER(readlinkat)(int dirfd, const char* pathname, char* buf, size_t bufsiz)
{
	UBA_INIT_DETOUR(readlinkat, dirfd, pathname, buf, bufsiz);
	DEBUG_LOG_TRUE("readlinkat","(%s)", pathname);
	UBA_ASSERTF(false, "readlinkat not implemented");
	return TRUE_WRAPPER(readlinkat)(dirfd, pathname, buf, bufsiz);
}

//UBA_EXPORT int UBA_WRAPPER(fcntl)(int fd, int cmd, ...)
//{
//	DEBUG_LOG_TRUE("fcntl", "(%i)", cmd);
//	//UBA_ASSERT cmd == F_GETPATH);
//	UBA_ASSERT(false);
//	return -1;
//}

// [HONK]: This can't be detoured for some odd reason when running certain executables (such as ispc)
// I suspect it is related to libUbaDetours.so actually don't pulling in the right shared libraries.. and if the application such as ispc
// does not depend on a version that contains shm_open this fails... and since we need shm_open for UbaDetours to work it will fail early
//UBA_EXPORT int UBA_WRAPPER(shm_open)(const char* name, int oflag, mode_t mode)
//{
//	UBA_INIT_DETOUR(shm_open, name, oflag, mode);
//	DEBUG_LOG_TRUE("shm_open", "(%s)", name);
//	return TRUE_WRAPPER(shm_open)(name, oflag, mode);
//}

#if PLATFORM_LINUX
UBA_EXPORT int UBA_WRAPPER(open64)(const char* file, int flags, ...)
{
	va_list args;
	va_start(args, flags);
	int mode = va_arg(args, int);
	va_end(args);
	UBA_INIT_DETOUR(open64, file, flags, mode);
	return Shared_open("open64", file, flags, mode, [](const char* realFile, int flags, int mode) { return TRUE_WRAPPER(open64)(realFile, flags, mode); });
}
#endif

UBA_EXPORT int UBA_WRAPPER(open)(const char* file, int flags, ...)
{
	va_list args;
	va_start(args, flags);
	int mode = va_arg(args, int);
	va_end(args);
	UBA_INIT_DETOUR(open, file, flags, mode);
	return Shared_open("open", file, flags, mode, [](const char* realFile, int flags, int mode) { return TRUE_WRAPPER(open)(realFile, flags, mode); });
}

//UBA_EXPORT ssize_t UBA_WRAPPER(write)(int fd, const void* buf, size_t count)
//{
//	UBA_INIT_DETOUR(write, fd, buf, count);
//	if (isatty(fd)) // stdout and stderr
//	{
//		Shared_WriteConsole((const char*)buf, count, fd == fileno(stderr));
//		return count;
//	}
//	//DEBUG_LOG_TRUE("write", "(%i size: %llu)", fd, count);
//	return TRUE_WRAPPER(write)(fd, buf, count);
//}

//UBA_EXPORT int fcntl(int __fd, int __cmd, ...)
//{
//	UBA_STDOUT("fcntl");
//	return -1;
//}

UBA_EXPORT int UBA_WRAPPER(dup)(int oldfd)
{
	UBA_INIT_DETOUR(dup, oldfd);
	auto res = TRUE_WRAPPER(dup)(oldfd);
	DEBUG_LOG_TRUE("dup", "(%i) -> %i", oldfd, res);
	return res;
}

UBA_EXPORT int UBA_WRAPPER(dup2)(int oldfd, int newfd)
{
	UBA_INIT_DETOUR(dup2, oldfd, newfd);
	auto res = TRUE_WRAPPER(dup2)(oldfd, newfd);

	if (res != -1)
	{
		SCOPED_WRITE_LOCK(g_fileHandlesLock, lock);
		auto findIt = g_fileHandles.find(oldfd);
		if (findIt != g_fileHandles.end())
		{
			DetouredHandle& h = findIt->second;
			FileObject* fo = h.fileObject;
			++fo->refCount;
			g_fileHandles[newfd].fileObject = fo;
		}
	}

	DEBUG_LOG_TRUE("dup2", "(%i, %i) -> %i", oldfd, newfd, res);
	return res;
}

UBA_EXPORT int UBA_WRAPPER(close)(int fd)
{
	UBA_INIT_DETOUR(close, fd);
	int res = 0;
	int error = 0;
	Shared_close(fd, [&]() { res = TRUE_WRAPPER(close)(fd); error = errno; });
	DEBUG_LOG_TRUE("close", "(%i) -> %i (%s)", fd, res, StrError(res, error));
	errno = error;
	return res;
}

#if PLATFORM_LINUX
UBA_EXPORT FILE* UBA_WRAPPER(fopen64)(const char* path, const char* mode)
{
	UBA_INIT_DETOUR(fopen64, path, mode);
	return Shared_fopen("fopen64", path, mode, "open64", [](const char* realFile, int flags, int openMode) { return TRUE_WRAPPER(open64)(realFile, flags, openMode); });
}
#endif

UBA_EXPORT FILE* UBA_WRAPPER(fopen)(const char* path, const char* mode)
{
	UBA_INIT_DETOUR(fopen, path, mode);
	return Shared_fopen("fopen", path, mode, "open", [](const char* realFile, int flags, int openMode) { return TRUE_WRAPPER(open)(realFile, flags, openMode); });
}

UBA_EXPORT FILE* UBA_WRAPPER(fdopen)(int fd, const char* mode)
{
	UBA_INIT_DETOUR(fdopen, fd, mode);
	DEBUG_LOG_TRUE("fdopen", "(%i)", fd);
	return TRUE_WRAPPER(fdopen)(fd, mode);
}

UBA_EXPORT int UBA_WRAPPER(fchmod)(int fd, mode_t mode)
{
	UBA_INIT_DETOUR(fchmod, fd, mode);
	auto res = TRUE_WRAPPER(fchmod)(fd, mode);
	DEBUG_LOG_TRUE("fchmod", "(%i) %i -> %i (%s)", fd, mode, res, StrError(res, errno));
	return res;
}

UBA_EXPORT int UBA_WRAPPER(fchmodat)(int dirfd, const char* pathname, mode_t mode, int flags)
{
	UBA_INIT_DETOUR(fchmodat, dirfd, pathname, mode, flags);
	DEBUG_LOG_TRUE("fchmodat", "%i %s %i %i", dirfd, pathname, mode, flags);
	return TRUE_WRAPPER(fchmodat)(dirfd, pathname, mode, flags);
}

//UBA_EXPORT size_t UBA_WRAPPER(fwrite)(const void* ptr, size_t size, size_t nitems, FILE* stream)
//{
//	UBA_INIT_DETOUR(fwrite, ptr, size, nitems, stream);
//	auto res = TRUE_WRAPPER(fwrite)(ptr, size, nitems, stream);
//	//if (stream != stdout)
//	//	DEBUG_LOG_TRUE("fwrite", "(%i) %i", fileno(stream), int(res));
//	return res;
//}

UBA_EXPORT int UBA_WRAPPER(fstat)(int fd, struct stat* buf)
{
	UBA_INIT_DETOUR(fstat, fd, buf);
	return Shared_fstat("fstat", fd, buf, [](int fd, struct stat* buf) { return TRUE_WRAPPER(fstat)(fd, buf); });
}

UBA_EXPORT int UBA_WRAPPER(futimens)(int fd, const struct timespec* times)
{
	UBA_INIT_DETOUR(futimens, fd, times);
	DEBUG_LOG_TRUE("futimens", "(%i)", fd);
	return TRUE_WRAPPER(futimens)(fd, times);
}

UBA_EXPORT int UBA_WRAPPER(fclose)(FILE* stream)
{
	UBA_INIT_DETOUR(fclose, stream);
	int fd = fileno(stream);
	int res = 0;
	int error = 0;
	Shared_close(fd, [&]() { res = TRUE_WRAPPER(fclose)(stream); error = errno; });
	DEBUG_LOG_TRUE("fclose", "(%p) -> %i (%s)", stream, res, StrError(res, error));
	errno = error;
	return res;
}

UBA_EXPORT DIR* UBA_WRAPPER(opendir)(const char* name)
{
	UBA_INIT_DETOUR(opendir, name);
	StringBuffer<> dirName;
	FixPath(dirName, name);

	DIR* res = TRUE_WRAPPER(opendir)(dirName.data);
	DEBUG_LOG_TRUE("opendir", "(%s) -> %p", dirName.data, res);
	if (!res)
		return res;
	res = (DIR*)(u64(res) | 0x1000'0000'0000'0000);

	return res;
}

UBA_EXPORT int UBA_WRAPPER(dirfd)(DIR* dirp)
{
	UBA_INIT_DETOUR(dirfd, dirp);
	dirp = (DIR*)(u64(dirp) & ~0x1000'0000'0000'0000);
	int res = TRUE_WRAPPER(dirfd)(dirp);
	DEBUG_LOG_TRUE("dirfd", "(%p) -> %i", dirp, res);
	return res;
}

UBA_EXPORT dirent* UBA_WRAPPER(readdir)(DIR* dirp)
{
	UBA_INIT_DETOUR(readdir, dirp);
	dirp = (DIR*)(u64(dirp) & ~0x1000'0000'0000'0000);
	auto res = TRUE_WRAPPER(readdir)(dirp);
	DEBUG_LOG_TRUE("readdir", "(%p) -> %p", dirp, res);
	return res;
}

UBA_EXPORT void UBA_WRAPPER(rewinddir)(DIR* dirp)
{
	UBA_INIT_DETOUR(rewinddir, dirp);
	UBA_ASSERTF(false, "rewinddir");
	DEBUG_LOG_TRUE("rewinddir", "(%p)", dirp);
	return TRUE_WRAPPER(rewinddir)(dirp);
}

UBA_EXPORT int UBA_WRAPPER(scandir)(const char* dirp, dirent*** namelist, int (*filter)(const dirent*), int (*compar)(const dirent**, const dirent**))
{
	UBA_INIT_DETOUR(scandir, dirp, namelist, filter, compar);
	UBA_ASSERTF(false, "scandir");
	DEBUG_LOG_TRUE("scandir", "(%p)", dirp);
	return TRUE_WRAPPER(scandir)(dirp, namelist, filter, compar);
}
UBA_EXPORT void UBA_WRAPPER(seekdir)(DIR* dirp, long loc)
{
	UBA_INIT_DETOUR(seekdir, dirp, loc);
	UBA_ASSERTF(false, "seekdir");
	DEBUG_LOG_TRUE("seekdir", "(%p)", dirp);
	return TRUE_WRAPPER(seekdir)(dirp, loc);
}

UBA_EXPORT long UBA_WRAPPER(telldir)(DIR* dirp)
{
	UBA_INIT_DETOUR(telldir, dirp);
	UBA_ASSERTF(false, "telldir");
	DEBUG_LOG_TRUE("telldir", "(%p)", dirp);
	return TRUE_WRAPPER(telldir)(dirp);
}

UBA_EXPORT DIR* UBA_WRAPPER(fdopendir)(int fd)
{
	UBA_INIT_DETOUR(fdopendir, fd);
	UBA_ASSERTF(false, "fdopendir");
	DEBUG_LOG_TRUE("fdopendir", "(%i)", fd);
	return TRUE_WRAPPER(fdopendir)(fd);
}

UBA_EXPORT int UBA_WRAPPER(closedir)(DIR* dirp)
{
	UBA_INIT_DETOUR(closedir, dirp);
	dirp = (DIR*)(u64(dirp) & ~0x1000'0000'0000'0000);
	DEBUG_LOG_TRUE("closedir", "(%p)", dirp);
	return TRUE_WRAPPER(closedir)(dirp);
}

UBA_EXPORT int UBA_WRAPPER(glob)(const char* pattern, int flags, int (*errfunc)(const char* epath, int eerrno), glob_t* pglob)
{
	UBA_INIT_DETOUR(glob, pattern, flags, errfunc, pglob);
	DEBUG_LOG_TRUE("glob", "%s", pattern);
	return TRUE_WRAPPER(glob)(pattern, flags, errfunc, pglob);
}

#if UBA_DEBUG && PLATFORM_LINUX

UBA_EXPORT int UBA_WRAPPER(fstatat64)(int dirfd, const char* pathname, struct stat64* buf, int flags)
{
	UBA_INIT_DETOUR(fstatat64, dirfd, pathname, buf, flags);
	DEBUG_LOG_TRUE("fstatat64", "");
	return TRUE_WRAPPER(fstatat64)(dirfd, pathname, buf, flags);
}
#endif

UBA_EXPORT int UBA_WRAPPER(lstat)(const char *path, struct stat *buf)
{
	UBA_INIT_DETOUR(lstat, path, buf);
	DEBUG_LOG_TRUE("lstat", "");
	return TRUE_WRAPPER(lstat)(path, buf);
}

UBA_EXPORT int UBA_WRAPPER(stat)(const char* file, struct stat* attr)
{
	UBA_INIT_DETOUR(stat, file, attr);
	return Shared_stat("stat", file, attr, [](const char* file, struct stat* attr) { return TRUE_WRAPPER(stat)(file, attr); });
}

UBA_EXPORT int UBA_WRAPPER(truncate)(const char* path, off_t length)
{
	UBA_INIT_DETOUR(truncate, path, length);
	UBA_ASSERTF(!g_runningRemote, "truncate not implemented for remote execution (path: %s)", path); // TODO: Implement this if it is ever called
	return TRUE_WRAPPER(truncate)(path, length);
}

UBA_EXPORT int UBA_WRAPPER(access)(const char* pathname, int mode)
{
	UBA_INIT_DETOUR(access, pathname, mode);

	StringBuffer<> fixedPath;
	if (!FixPath(fixedPath, pathname) || fixedPath.StartsWith(g_systemTemp.data) || fixedPath.StartsWith("/proc"))
	{
		return TRUE_WRAPPER(access)(pathname, mode);
	}

	bool checkIfDir = false;
	StringBuffer<> temp;
	if (g_runningRemote && fixedPath.StartsWith(g_exeDir.data))
	{
		temp.Append(g_virtualApplicationDir).Append(fixedPath.data + g_exeDir.count);

		if (temp.count == g_virtualApplicationDir.count) // Remove last slash from temp if this is a access call for the actual applicaiton directory
		{
			checkIfDir = true;
			temp.Resize(temp.count - 1);
		}
		fixedPath.Clear().Append(temp);
	}

	if (!CanDetour(pathname))
	{
		auto res = TRUE_WRAPPER(access)(pathname, mode);
		DEBUG_LOG_TRUE("access", "%s %i -> %i (%s)", pathname, mode, res, StrError(res, errno));
		return res;
	}

	FileAttributes attr;
	const char* realName = Shared_GetFileAttributes(attr, fixedPath.data, checkIfDir);

	if (!attr.useCache)
	{
		auto res = TRUE_WRAPPER(access)(realName, mode);
		DEBUG_LOG_TRUE("access", "%s %i (%s) -> %i %s", pathname, mode, realName, res, StrError(res, errno));
		return res;
	}

	int res = attr.lastError == 0 ? 0 : -1;
	
	#if UBA_DEBUG_VALIDATE
	if (!g_runningRemote)
	{
		//errno = 0;
		auto res2 = TRUE_WRAPPER(access)(realName, mode);
		//int eo = errno;
		
		DEBUG_LOG_DETOURED("access", "%s %i (%s) -> %i %s", pathname, mode, realName, res, StrError(res, attr.lastError));
		
		UBA_ASSERTF(res2 == res, "MISMATCH OF RESULTS for %s - %i %i (err = %s) (exedir %s)", realName, res2, res, StrError(res, attr.lastError), g_exeDir.data);
		//UBA_ASSERTF(eo == attr.lastError, "MISMATCH OF ERRORS for %s - %i %i", realName, eo, attr.lastError);
	}
	#endif
	
	errno = attr.lastError;
	return res;
}

#if PLATFORM_LINUX
UBA_EXPORT int UBA_WRAPPER(__fxstatat)(int ver, int dirfd, const char* pathname, struct stat* buf, int flags)
{
	UBA_INIT_DETOUR(__fxstatat, ver, dirfd, pathname, buf, flags);
	DEBUG_LOG_TRUE("__fxstatat", "");
	return TRUE_WRAPPER(__fxstatat)(ver, dirfd, pathname, buf, flags);
}

UBA_EXPORT int UBA_WRAPPER(__fxstatat64)(int ver, int dirfd, const char* pathname, struct stat64* buf, int flags)
{
	UBA_INIT_DETOUR(__fxstatat64, ver, dirfd, pathname, buf, flags);
	DEBUG_LOG_TRUE("__fxstatat64", "");
	return TRUE_WRAPPER(__fxstatat64)(ver, dirfd, pathname, buf, flags);
}

UBA_EXPORT int UBA_WRAPPER(__xstat)(int ver, const char* file, struct stat* attr)
{
	UBA_INIT_DETOUR(__xstat, ver, file, attr);
	return Shared_stat("__xstat", file, attr, [ver](const char* file, struct stat* attr) { return TRUE_WRAPPER(__xstat)(ver, file, attr); });
}

UBA_EXPORT int UBA_WRAPPER(__fxstat)(int ver, int fd, struct stat* attr)
{
	UBA_INIT_DETOUR(__fxstat, ver, fd, attr);
	return Shared_fstat("__fxstat", fd, attr, [ver](int fd, struct stat* attr) { return TRUE_WRAPPER(__fxstat)(ver, fd, attr); });
}

#endif

UBA_EXPORT int UBA_WRAPPER(rename)(const char* oldpath, const char* newpath)
{
	UBA_INIT_DETOUR(rename, oldpath, newpath);

	StringBuffer<> fixedOldPath;
	FixPath(fixedOldPath, oldpath);
	StringKey oldKey = ToFilenameKey(fixedOldPath);

	StringBuffer<> fixedNewPath;
	FixPath(fixedNewPath, newpath);
	StringKey newKey = ToFilenameKey(fixedNewPath);

	// TODO: This might be really slow but it seems you can rename files on linux while they are open and they won't be properly renamed until closed
	{
		SCOPED_READ_LOCK(g_fileHandlesLock, lock);
		for (auto& kv : g_fileHandles)
		{
			FileObject& fo = *kv.second.fileObject;
			FileInfo& fi = *fo.fileInfo;
			if (fi.fileNameKey == oldKey)
			{
				UBA_ASSERTF(fo.desiredAccess & AccessFlag_Write, "Unsupported access flags");
				fo.newName = fixedNewPath.data;
				if (!fo.closeId)
				{
					char temp[1024];
					u64 size;
					Rpc_CreateFileW(fixedNewPath.data, newKey, AccessFlag_Write, temp, sizeof_array(temp), size, fo.closeId, true);
				}

				bool wasTempFile = fixedNewPath.StartsWith(g_systemTemp.data);
				if (g_runningRemote && !wasTempFile)
				{
					// TODO: We need to make sure this actually worked
					errno = 0;
					DEBUG_LOG_DETOURED("rename", "IS_OPEN (%i) (from %s to %s) -> %i (%s)", kv.first, fixedOldPath.data, fixedNewPath.data, 0, StrError(0, errno));
					return 0;
				}
				bool isTempFile = fixedOldPath.StartsWith(g_systemTemp.data);
				UBA_ASSERTF(wasTempFile == isTempFile, "File changing from temp to not or vice versa not implemented");

				int res = TRUE_WRAPPER(rename)(oldpath, newpath);
				DEBUG_LOG_DETOURED("rename", "IS_OPEN (%i) (from %s to %s) -> %i (%s)", kv.first, fixedOldPath.data, fixedNewPath.data, 0, StrError(0, errno));
				return res;
			}
		}
	}

	u32 directoryTableSize;
	u32 errorCode;
	bool result;
	{
		TimerScope ts(g_stats.moveFile);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_MoveFile);
		writer.WriteStringKey(oldKey);
		writer.WriteString(fixedOldPath);
		writer.WriteStringKey(newKey);
		writer.WriteString(fixedNewPath);
		writer.WriteU32(0);// dwFlags);
		writer.Flush();
		BinaryReader reader;
		result = reader.ReadBool();
		errorCode = reader.ReadU32();
		directoryTableSize = reader.ReadU32();
		//DEBUG_LOG_PIPE(L"MoveFile", L"%ls to %ls", lpExistingFileName, lpNewFileName);
	}

	//DEBUG_LOG_DETOURED(L"rena", L"(PIPE) (%ls to %ls) -> %ls", lpExistingFileName, lpNewFileName, ToString(result));

	g_directoryTable.ParseDirectoryTable(directoryTableSize);
	g_mappedFileTable.SetDeleted(oldKey, fixedOldPath.data, true);
	g_mappedFileTable.SetDeleted(newKey, fixedNewPath.data, false);

	int res = result ? 0 : -1;
	DEBUG_LOG_DETOURED("rename", "(from %s to %s) -> %i (%s)", fixedOldPath.data, fixedNewPath.data, res, StrError(res, errorCode));

	errno = errorCode;
	return res;
	//return TRUE_WRAPPER(rename)(oldpath, newpath);
}

UBA_EXPORT int UBA_WRAPPER(chmod)(const char* pathname, mode_t mode)
{
	UBA_INIT_DETOUR(chmod, pathname, mode);

	StringBuffer<> fixedName;
	FixPath(fixedName, pathname);
	StringKey key = ToFilenameKey(fixedName);
	u32 errorCode;
	{
		TimerScope ts(g_stats.chmod);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_Chmod);
		writer.WriteStringKey(key);
		writer.WriteString(fixedName);
		writer.WriteU32(mode);
		writer.Flush();
		BinaryReader reader;
		errorCode = reader.ReadU32();
		//DEBUG_LOG_PIPE(L"MoveFile", L"%ls to %ls", lpExistingFileName, lpNewFileName);
	}

	int res = errorCode == 0 ? 0 : -1;
	DEBUG_LOG_DETOURED("chmod", "%s %i -> %i (%s)", pathname, mode, res, StrError(res, errorCode));

	errno = errorCode;
	return res;
	//return TRUE_WRAPPER(chmod)(pathname, mode);
}

UBA_EXPORT int UBA_WRAPPER(renameat)(int olddirfd, const char* oldpath, int newdirfd, const char* newpath)
{
	UBA_INIT_DETOUR(renameat, olddirfd, oldpath, newdirfd, newpath);
	DEBUG_LOG_TRUE("renameat", "(from %s to %s)", oldpath, newpath);
	return TRUE_WRAPPER(renameat)(olddirfd, oldpath, newdirfd, newpath);
}

UBA_EXPORT int UBA_WRAPPER(utimensat)(int dirfd, const char* pathname, const struct timespec* times, int flags)
{
	UBA_INIT_DETOUR(utimensat, dirfd, pathname, times, flags);
	DEBUG_LOG_TRUE("utimensat", "(%s)", pathname);
	return TRUE_WRAPPER(utimensat)(dirfd, pathname, times, flags);
}

UBA_EXPORT int UBA_WRAPPER(symlink)(const char* path1, const char* path2)
{
	UBA_INIT_DETOUR(symlink, path1, path2);
	DEBUG_LOG_TRUE("symlink", "(from %s to %s)", path1, path2);
	return TRUE_WRAPPER(symlink)(path1, path2);
}

UBA_EXPORT ssize_t	 UBA_WRAPPER(pread)(int __fd, void * __buf, size_t __nbyte, off_t __offset)
{
	// char filePath[PATH_MAX];
	// if (fcntl(__fd, F_GETPATH, filePath) != -1)
	// {
	// 	// printf("***** PREAD: %d %s bytes: 0x%lx\n",__fd, filePath, __nbyte);
	// 	// do something with the file path
	// }
	return TRUE_WRAPPER(pread)(__fd, __buf, __nbyte, __offset);
}

UBA_EXPORT ssize_t UBA_WRAPPER(read)(int fd, void *buf, size_t nbyte)
{
	// char filePath[PATH_MAX];
	// if (fcntl(fd, F_GETPATH, filePath) != -1)
	// {
	// 	// printf("***** READ: %d %s bytes: %lu\n",fd, filePath, nbyte);
	// 	// do something with the file path
	// }
	return TRUE_WRAPPER(read)(fd, buf, nbyte);
}


UBA_EXPORT int UBA_WRAPPER(remove)(const char* pathname)
{
	UBA_INIT_DETOUR(remove, pathname);
	StringBuffer<> fixedName;
	FixPath(fixedName, pathname);

	//if (!CanDetour(fixedName.data))
	//{
	//	DEBUG_LOG_TRUE(L"DeleteFile", L"(%ls)", original);
	//	return TRUE_WRAPPER(remove)(pathname);
	//}

	//if (KeepInMemory(fixedName.data, fixedName.count))
	//{
	//	DEBUG_LOG_DETOURED(L"DeleteFile", L"(INMEMORY) (%ls) -> Success", lpFileName);
	//	SetLastError(ERROR_SUCCESS);
	//	return TRUE;
	//}

	StringKey fileNameKey = ToFilenameKey(fixedName);

	u32 directoryTableSize;
	bool result;
	u32 errorCode;
	{
		u32 closeId = 0;
		TimerScope ts(g_stats.deleteFile);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_DeleteFile);
		writer.WriteString(fixedName);
		writer.WriteStringKey(fileNameKey);
		writer.WriteU32(closeId);
		writer.Flush();
		BinaryReader reader;
		result = reader.ReadBool();
		errorCode = reader.ReadU32();
		directoryTableSize = reader.ReadU32();
		pcs.Leave();
		DEBUG_LOG_PIPE(L"DeleteFile", L"%ls", lpFileName);
	}
	//DEBUG_LOG_DETOURED(L"DeleteFile", L"(%ls) -> %ls", pathname, ToString(result));

	g_directoryTable.ParseDirectoryTable(directoryTableSize);
	g_mappedFileTable.SetDeleted(fileNameKey, fixedName.data, true);

	int res = result != 0 ? 0 : -1;
	DEBUG_LOG_DETOURED("remove", "(%s) -> %i (%s)", pathname, res, StrError(res, errorCode));
	errno = errorCode;
	return res;
	//return TRUE_WRAPPER(remove)(pathname);
}

thread_local int t_inVfork;

UBA_EXPORT int UBA_WRAPPER(posix_spawn)(pid_t* pid, const char* path, const posix_spawn_file_actions_t* file_actions, const posix_spawnattr_t* attrp, char* const argv[], char* const envp[])
{
	UBA_INIT_DETOUR(posix_spawn, pid, path, file_actions, attrp, argv, envp);

	t_inVfork = 0;

	TString cmdLine;
	for (u32 i = 0; argv[i]; ++i)
	{
		if (i != 0)
			cmdLine.append(" ");
		cmdLine.append(argv[i]);
	}
	
	TString realApplication;
	TString commandLine;
	u32 processId = 0;
	StringBuffer<512> currentDir;
	StringBuffer<256> comIdVar;
	StringBuffer<32> rulesStr;
	StringBuffer<256> logFile;

	{
		TimerScope ts(g_stats.createProcess);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);

		const char* pwd = "";
		//while (char* env = envp[i++])
		//{
		//	if (StartsWith(env, "PWD="))
		//	{
		//		printf("%s\n", env);
		//		pwd = env + 4;
		//		break;
		//	}
		//}

		BinaryWriter writer;
		writer.WriteByte(MessageType_CreateProcess);
		writer.WriteString(path); // Application
		writer.WriteString(cmdLine); // Full command line
		writer.WriteString(pwd); // Current dir
		writer.Flush();

		BinaryReader reader;
		processId = reader.ReadU32();
		UBA_ASSERTF(processId > 0, "Process id was zero");

		rulesStr.Append("UBA_RULES=").AppendValue(reader.ReadU32());

		u32 dllNameSize = reader.ReadU32();
		reader.Skip(dllNameSize);

		currentDir.Append("UBA_CWD=");
		reader.ReadString(currentDir);

		realApplication = reader.ReadString();
		commandLine = reader.ReadString();

		comIdVar.Append("UBA_COMID=").AppendValue(reader.ReadU64()).Append('+').AppendValue(reader.ReadU32());

		logFile.Append("UBA_LOGFILE=");
		reader.ReadString(logFile);
	}
	
	std::vector<const char*> envvars;
	{
		for (u32 i = 0; envp[i]; ++i)
		{
//			if (envp[i][0] == 'D' || envp[i][0] == 'P')
				envvars.push_back(envp[i]);
		}
		envvars.push_back(comIdVar.data);
		envvars.push_back(currentDir.data);
		envvars.push_back(rulesStr.data);
		if (logFile.count > 12)
			envvars.push_back(logFile.data);
		//envvars.push_back("LD_DEBUG=bindings");
		envvars.push_back(nullptr);
	}

//	int i=0;
//	printf("spawnng %s\n", envvars.data()[i]);
//	while (envvars.data()[i])
//	{
//		printf("env: %s\n", envvars.data()[i]);
//		i++;
//	}
	
	#if UBA_DEBUG_LOG_ENABLED
	DEBUG_LOG_TRUE("posix_spawn", "%s", realApplication.data());
	for (u32 i = 0; argv[i]; ++i)
		DEBUG_LOG("            %s", argv[i]);
	#endif

	int res = TRUE_WRAPPER(posix_spawn)(pid, realApplication.data(), file_actions, attrp, argv, (char**)envvars.data());
	bool success = res == 0;

	{
		TimerScope ts(g_stats.createProcess);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_StartProcess);
		writer.WriteU32(processId);
		writer.WriteBool(success);
		writer.WriteU32(res);
		writer.WriteU64(1); // Process handle
		writer.WriteU32(*pid);
		writer.WriteU64(0); // Thread handle
		writer.Flush();
	}

	DEBUG_LOG("         Child process started %s -> %i", path, res);

	return res;
}

UBA_EXPORT int UBA_WRAPPER(posix_spawnp)(pid_t* pid, const char* file, const posix_spawn_file_actions_t* file_actions, const posix_spawnattr_t* attrp, char* const argv[], char* const envp[])
{
	UBA_INIT_DETOUR(posix_spawnp, pid, file, file_actions, attrp, argv, envp);
	DEBUG_LOG_TRUE("posix_spawnp", "");
	return TRUE_WRAPPER(posix_spawnp)(pid, file, file_actions, attrp, argv, envp);
}

UBA_EXPORT pid_t UBA_WRAPPER(wait)(int* status)
{
	UBA_INIT_DETOUR(wait, status);
	DEBUG_LOG_TRUE("wait", "");
	return TRUE_WRAPPER(wait)(status);
}

UBA_EXPORT pid_t UBA_WRAPPER(waitpid)(pid_t pid, int* status, int options)
{
	UBA_INIT_DETOUR(waitpid, pid, status, options);
	// TODO: Should probably report id to session
	auto res = TRUE_WRAPPER(waitpid)(pid, status, options);
	DEBUG_LOG_TRUE("waitpid", "(%i) -> ", pid, res);
	return res;
}

UBA_EXPORT int UBA_WRAPPER(waitid)(idtype_t idtype, id_t id, siginfo_t* infop, int options)
{
	UBA_INIT_DETOUR(waitid, idtype, id, infop, options);
	DEBUG_LOG_TRUE("waitid", "");
	UBA_ASSERTF(!t_inVfork, "waitid: is in fork");
	// TODO: Should probably report id to session
	return TRUE_WRAPPER(waitid)(idtype, id, infop, options);
}

UBA_EXPORT pid_t UBA_WRAPPER(wait3)(int* status, int options, struct rusage* rusage)
{
	UBA_INIT_DETOUR(wait3, status, options, rusage);
	DEBUG_LOG_TRUE("wait3", "");
	UBA_ASSERTF(!t_inVfork, "wait3: is in fork");
	pid_t res =TRUE_WRAPPER(wait3)(status, options, rusage);
	UBA_ASSERTF(false, "wait3");
	return res;
}

UBA_EXPORT pid_t UBA_WRAPPER(wait4)(pid_t pid, int* status, int options, struct rusage* rusage)
{
	UBA_INIT_DETOUR(wait4, pid, status, options, rusage);
	UBA_ASSERTF(!t_inVfork, "wait4: is in fork");
	pid_t res = TRUE_WRAPPER(wait4)(pid, status, options, rusage);
	DEBUG_LOG_TRUE("wait4", "");
	return res;
}

UBA_EXPORT int UBA_WRAPPER(execv)(const char* path, char* const argv[])
{
	UBA_INIT_DETOUR(execv, path, argv);
	DEBUG_LOG_TRUE("execv", "");
	return TRUE_WRAPPER(execv)(path, argv);
}

int Internal_execve(const char* pathname, char* const _Nullable argv[], char* const _Nullable envp[])
{
	UBA_INIT_DETOUR(execve, pathname, argv, envp);
	// We are most likely in a vfork/fork here, which means that we won't see the exit call since _exit will drop back out to the entrance of fork.
	DEBUG_LOG_TRUE("execve", "");

	bool inVfork = t_inVfork != 0;
	pid_t pid;
	int res = posix_spawn(&pid, pathname, nullptr, nullptr, argv, envp);
	if (inVfork)
		t_inVfork = pid;

	if (res != 0)
	{
		UBA_ASSERTF(false, "Failed to spawn %s", pathname);
		return -1;
	}

	int status;
	{
		res = TRUE_WRAPPER(waitpid)(pid, &status, WUNTRACED | WCONTINUED);
		DEBUG_LOG_TRUE("waitpid", "(%i) -> %i", pid, res);
		UBA_ASSERTF(res == pid, "execve: wait result was not same as pid");
		UBA_ASSERTF(WIFEXITED(status), "execve: Unsupported status from waitpid");
	}

	{
		TimerScope ts(g_stats.createProcess);
		SCOPED_WRITE_LOCK(g_communicationLock, pcs);
		BinaryWriter writer;
		writer.WriteByte(MessageType_ExitChildProcess);
		writer.WriteU32(pid);
	}

	return WEXITSTATUS(status);
}

UBA_EXPORT int UBA_WRAPPER(execve)(const char* pathname, char* const _Nullable argv[], char* const _Nullable envp[])
{
	int res = Internal_execve(pathname, argv, envp);
	TRUE_WRAPPER(_exit)(res);
	return 0;
}

UBA_EXPORT int UBA_WRAPPER(execvp)(const char* file, char* const argv[])
{
	UBA_INIT_DETOUR(execvp, file, argv);
	DEBUG_LOG_TRUE("execvp", "");
	return TRUE_WRAPPER(execvp)(file , argv);
}

#if PLATFORM_MAC
UBA_EXPORT pid_t UBA_WRAPPER(fork)(void)
{
	UBA_INIT_DETOUR(fork);
	DEBUG_LOG_TRUE("fork", "");
	return TRUE_WRAPPER(fork)();
}
#endif


#if PLATFORM_LINUX
UBA_EXPORT pid_t UBA_WRAPPER(vfork)(void)
{
	UBA_INIT_DETOUR(vfork);
	DEBUG_LOG_TRUE("vfork", "");
	pid_t pid = fork();
	if (pid == 0)
	{
	#if PLATFORM_LINUX
		prctl(PR_SET_PDEATHSIG, SIGHUP, 0, 0, 0); // We want the process to die if the parent die
	#endif
		t_inVfork = 1;
	}
	return pid;
}
#endif

UBA_EXPORT FILE* UBA_WRAPPER(popen)(const char* command, const char* type)
{
	UBA_INIT_DETOUR(popen, command, type);
	DEBUG_LOG_TRUE("popen", "");
	return TRUE_WRAPPER(popen)(command, type);
}

void CloseCom();

UBA_EXPORT void UBA_WRAPPER(exit)(int status)
{
	//UBA_INIT_DETOUR(exit, status);
	DEBUG_LOG_TRUE("exit", "(%i)", status);
	Deinit();
	CloseCom();
	TRUE_WRAPPER(exit)(status);
}

UBA_EXPORT void UBA_WRAPPER(_exit)(int status)
{
	DEBUG_LOG_TRUE("_exit", "(%i)", status);
	Deinit();
	CloseCom();
	TRUE_WRAPPER(_exit)(status);
}

UBA_EXPORT void UBA_WRAPPER(_Exit)(int status)
{
	DEBUG_LOG_TRUE("_Exit", "(%i)", status);
	if (!t_inVfork)
	{
		Deinit();
		CloseCom();
	}
	TRUE_WRAPPER(_Exit)(status);
}

UBA_EXPORT int UBA_WRAPPER(system)(const char* command)
{
	UBA_INIT_DETOUR(system, command);
	DEBUG_LOG_TRUE("system", "");
	return TRUE_WRAPPER(system)(command);
}

/*

UBA_EXPORT int __fxstat64(int ver, int fd, struct stat64* attr)
{
	UBA_NOT_IMPLEMENTED(__fxstat64);
	return TRUE_WRAPPER(__fxstat64)(ver, fd, attr);

}
UBA_EXPORT int __xstat64(int ver, const char* file, struct stat64* attr)
{
	UBA_NOT_IMPLEMENTED(__xstat64);
	return TRUE_WRAPPER(__xstat64)(ver, file, attr);
}

UBA_EXPORT int openat(int dirfd, const char* pathname, int flags, ...)
{
	UBA_NOT_IMPLEMENTED(openat);
	return -1;
}

UBA_EXPORT int openat(int dirfd, const char* pathname, int flags, mode_t mode, ...)
{
	UBA_NOT_IMPLEMENTED(openat);
	return -1;
}

UBA_EXPORT int openat64(int __fd, const char* __file, int __oflag, ...)
{
	UBA_NOT_IMPLEMENTED(openat64);
	return -1;
}

UBA_EXPORT int openat64(int __fd, const char* __file, int __oflag, mode_t mode, ...)
{
	UBA_NOT_IMPLEMENTED(open64);
	return -1;
}
*/

#if PLATFORM_LINUX

UBA_EXPORT char* UBA_WRAPPER(get_current_dir_name)(void)
{
	UBA_ASSERTF(false, "get_current_dir_name");
	return TRUE_WRAPPER(get_current_dir_name)();
}

#endif

// These macros need to be at the end of the file to avoid having to forward declare
// the Apple versions of these since they are all decorated with uba_<func>.
#if PLATFORM_MAC
#define DETOURED_FUNCTION(func) \
	DYLD_INTERPOSE(UBA_WRAPPER(func), func);
DETOURED_FUNCTIONS
#undef DETOURED_FUNCTION
#endif

namespace uba
{
	int GetProcessExecutablePath(tchar* Path, u32 PathSize)
	{
#if PLATFORM_LINUX
		auto res = TRUE_WRAPPER(readlink)("/proc/self/exe", Path, PathSize);
		if (res != -1)
			Path[res] = 0;
		return res;
#elif PLATFORM_MAC
		if (_NSGetExecutablePath(Path, &PathSize) == 0)
			return strlen(Path);
		return -1;
#endif
	}


	void PreInit(const char* logFile)
	{
		g_exeDirMem.Create();
		g_fileHandlesMem.Create();
		g_fileHandlesLockMem.Create();

		SuppressDetourScope s;

		g_systemTemp.Append(getenv("TMPDIR"));
		//g_systemTemp.Append("/tmp/");

		#if PLATFORM_LINUX
		#define DETOURED_FUNCTION(func) \
		if (!(TRUE_WRAPPER(func) = (Symbol_##func*)dlsym(RTLD_NEXT, #func))) \
			;//printf("dlsym failed on %s: %s\n", #func, dlerror());
		DETOURED_FUNCTIONS
		#undef DETOURED_FUNCTION
		#endif

		#if UBA_DEBUG_LOG_ENABLED
		if (g_logToScreen)
			g_debugFile = (FileHandle)open("/dev/tty", O_WRONLY);
		else if (logFile)
			g_debugFile = (FileHandle)open(logFile, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
		if (g_debugFile != InvalidFileHandle)
		{
			char buf[32 * 1024];
#if PLATFORM_LINUX
			int fd = TRUE_WRAPPER(open)("/proc/self/cmdline", O_RDONLY);
			if (fd != -1)
			{
				auto bufSize = read(fd, buf, sizeof(buf));
				TRUE_WRAPPER(close)(fd);
				if (bufSize != -1)
				{
					buf[bufSize] = 0;
					const char* it = buf;
					while (*it)
					{
						if (it == buf)
						{
							DEBUG_LOG("Executable: %s", it);
						}
						else
						{
							DEBUG_LOG("               %s", it);
						}
						it += strlen(it) + 1;
					}
					DEBUG_LOG("", it);
				}
			}
#else
			int ArgC = *_NSGetArgc();
			char** ArgV = *_NSGetArgv();
			DEBUG_LOG("Executable: %s", ArgV[0]);
			for (int Arg = 1; Arg < ArgC; Arg++)
			{
				DEBUG_LOG("               %s", ArgV[Arg]);
			}
			DEBUG_LOG("");
#endif
		}
		#endif

		StringBuffer<> exePath;
		exePath.count = GetProcessExecutablePath(exePath.data, exePath.capacity);
		UBA_ASSERTF(exePath.count > 0, "exePath.count == 0");
		char* lastSlash = strrchr(exePath.data, '/');
		UBA_ASSERTF(lastSlash, "no slash found in %s", exePath.data);
		exePath.Resize(lastSlash - exePath.data);
		FixPath(g_exeDir, exePath.data);
		g_exeDir.EnsureEndsWithSlash();
	}

	void Init()
	{
		UBA_ASSERTF(!g_isInitialized, "Already initialized");
		g_isInitialized = true;

		u64 directoryTableHandle;
		u32 directoryTableSize;
		u32 directoryTableCount;
		u64 mappedFileTableHandle;
		u32 mappedFileTableSize;
		u32 mappedFileTableCount;

		{
			TimerScope ts(g_stats.init);
			SCOPED_WRITE_LOCK(g_communicationLock, pcs);
			BinaryWriter writer;
			writer.WriteByte(MessageType_Init);
			writer.Flush();
			BinaryReader reader;

			bool echoOn = reader.ReadBool();

			reader.ReadString(g_virtualApplication);
			reader.ReadString(g_virtualWorkingDir);

			directoryTableHandle = reader.ReadU64();
			directoryTableSize = reader.ReadU32();
			directoryTableCount = reader.ReadU32();
			mappedFileTableHandle = reader.ReadU64();
			mappedFileTableSize = reader.ReadU32();
			mappedFileTableCount = reader.ReadU32();
			DEBUG_LOG_PIPE(L"Init", L"");
		}

		UBA_ASSERTF(g_virtualApplicationDir.capacity > 0, "g_virtualApplicationDir.capacity > 0");

		const char* lastSlash = strrchr(g_virtualApplication.data, '/');
		UBA_ASSERTF(lastSlash, "Need fullpath for application (%s)", g_virtualApplication.data);
		g_virtualApplicationDir.Append(g_virtualApplication.data, lastSlash - g_virtualApplication.data + 1);

		setenv("PWD", g_virtualWorkingDir.data, 1);
		g_virtualWorkingDir.EnsureEndsWithSlash();

		StringBuffer<128> mappedFileTableUidName;
		GetMappingHandleName(mappedFileTableUidName, mappedFileTableHandle);
		int mappedFileTableFd = shm_open(mappedFileTableUidName.data, O_RDONLY, S_IRUSR | S_IWUSR);
		UBA_ASSERTF(mappedFileTableFd != -1, "mappedFileTableFd == %i", mappedFileTableFd);
		u8* mappedFileTableMem = (u8*)mmap(NULL, FileMappingTableMemSize, PROT_READ, MAP_SHARED, mappedFileTableFd, 0);
		UBA_ASSERTF(mappedFileTableMem != MAP_FAILED, "mmap failed (%s)", strerror(errno));
		g_mappedFileTable.Init(mappedFileTableMem, mappedFileTableCount, mappedFileTableSize);

		StringBuffer<128> dirTableUidName;
		GetMappingHandleName(dirTableUidName, directoryTableHandle);
		int dirTableFd = shm_open(dirTableUidName.data, O_RDONLY, S_IRUSR | S_IWUSR);
		UBA_ASSERTF(dirTableFd != -1, "shm_open failed (%s)", strerror(errno));
		u8* dirTableMem = (u8*)mmap(NULL, DirTableMemSize, PROT_READ, MAP_SHARED, dirTableFd, 0);
		UBA_ASSERTF(dirTableMem != MAP_FAILED, "mmap for dirtable mem failed (%s)", strerror(errno));
		g_directoryTable.Init(dirTableMem, directoryTableCount, directoryTableSize);

		g_isDetouring = true;
		DEBUG_LOG("Detouring enabled");

		//char** envs = environ;
		//while (char* env = *envs++)
		//{
		//	UBA_STDOUT("ENV %s", env);
		//}

	}

	void Deinit()
	{
		if (!g_isInitialized)
			return;
		
		g_isInitialized = false;
		g_isDetouring = false;

		{
			SCOPED_WRITE_LOCK(g_fileHandlesLock, lock);
			for (auto& kv : g_fileHandles)
			{
				TRUE_WRAPPER(close)(kv.first);
				DetouredHandle& h = kv.second;
				FileObject* fo = h.fileObject;
				if (!fo->closeId)
					continue;
				TRUE_WRAPPER(close)(kv.first);
				u64 mappingHandle = 0;
				u64 mappingWritten = 0;
				FileInfo& fi = *fo->fileInfo;
				const tchar* path = fi.name;
				Rpc_UpdateCloseHandle(path, fo->closeId, fo->deleteOnClose, fo->newName.c_str(), mappingHandle, mappingWritten, true);
			}
		}
		
		BinaryWriter writer;
		writer.WriteByte(MessageType_Exit);
		writer.WriteU32(0); // Exit code
		writer.WriteString(""); // Log name
		g_stats.Write(writer);

		// This can't wait for response since the session process might move on and reuse shared memory with someone else
		// Note, if we start using memory mapped files we need to change this to true for child processes since Exit message is writing files to disk..
		// .. and if we don't wait to exit this process until those files are written we might end up in a race condition with the parent using those files
		writer.Flush(false);

		#if UBA_DEBUG_LOG_ENABLED
		if (isLogging())
		{
			DEBUG_LOG("Finished");
			int debugFile = (int)g_debugFile;
			g_debugFile = InvalidFileHandle;
			TRUE_WRAPPER(close)(debugFile);
		}
		#endif
	}

	#if UBA_DEBUG_LOG_ENABLED
	void WriteDebug(const char* str, u32 strLen)
	{
		int t = errno;
		//TRUE_WRAPPER(write)(g_debugFile, str, strLen);
		write(g_debugFile, str, strLen);
		//fsync(g_debugFile);
		errno = t;
	}
	#endif

	ANALYSIS_NORETURN void UbaAssert(const tchar* text, const char* file, u32 line, const char* expr, u32 terminateCode)
	{
		SuppressDetourScope s;
		StringBuffer<8*1024> b;
		WriteAssertInfo(b, text, file, line, expr, 1);
		Rpc_WriteLog(b.data, b.count, true, true);

		BinaryWriter writer;
		writer.WriteByte(MessageType_Exit);
		writer.WriteU32(terminateCode); // Exit code
		writer.WriteString(""); // Log name
		g_stats.Write(writer);
		writer.Flush(false);

		CloseCom();
		TRUE_WRAPPER(_exit)(int(terminateCode));
	}
}

extern "C"
{
	UBA_EXPORT bool UbaRequestNextProcess(u32 prevExitCode, char* outArguments, u32 outArgumentsCapacity)
	{
		#if UBA_DEBUG_LOG_ENABLED
		//FlushDebugLog();
		#endif

		*outArguments = 0;
		bool newProcess;
		{
			SCOPED_WRITE_LOCK(g_communicationLock, pcs);
			BinaryWriter writer;
			writer.WriteByte(MessageType_GetNextProcess);
			writer.WriteU32(prevExitCode);
			g_stats.Write(writer);


			writer.Flush();
			BinaryReader reader;
			newProcess = reader.ReadBool();
			if (newProcess)
			{
				reader.ReadString(outArguments, outArgumentsCapacity);
				reader.SkipString(); // workingDir
				reader.SkipString(); // description
				reader.ReadString(g_logName.Clear());
			}
		}

		if (newProcess)
		{
			g_stats = {};

			//#if UBA_DEBUG_LOG_ENABLED
			//SuppressCreateFileDetourScope scope;
			//HANDLE debugFile = (HANDLE)g_debugFile;
			//g_debugFile = InvalidFileHandle;
			//CloseHandle(debugFile);
			//debugFile = CreateFileW(g_logName.data, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			//g_debugFile = (FileHandle)(u64)debugFile;
			//#endif
		}

		Rpc_UpdateTables();
		return newProcess;
	}
}