// Copyright Epic Games, Inc. All Rights Reserved.

#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdio.h>
#include <dlfcn.h>

#if PLATFORM_LINUX
#include <link.h>
#include <dlfcn.h>
#else
#include <mach-o/dyld.h>
#endif


int LogError(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	char buffer[1024];
	snprintf(buffer, 1024, format, args);
	printf("%s\n", buffer);
	va_end(args);
	return -1;
}

bool IsLibraryLoaded(const char* libraryToMatch)
	{
		bool foundLib = false;
#if PLATFORM_LINUX
		// The dl_iterate_phdr() function walks through the list of an
		// application's shared objects and calls the function callback once
		// for each object, until either all shared objects have been
		// processed or callback returns a nonzero value.
		// The last iteration of the callbacks return is propigated all the way up
		foundLib = dl_iterate_phdr([](struct dl_phdr_info* info, size_t size, void* libraryToMatch)
				{
					return strstr(info->dlpi_name, (const char*)libraryToMatch) != 0 ? 1 : 0;
				}, (void*)libraryToMatch);
#elif PLATFORM_MAC

		bool foundDetoursLib = false;
		unsigned int count = _dyld_image_count();
		for (int i = 0; i < count; i++)
		{
			const char* ImageName = _dyld_get_image_name(i);
			if (strstr(ImageName, libraryToMatch))
			{
				foundLib = true;
				break;
			}
		}
#endif
		return foundLib;
	}


/******************** WARNING ********************/
// UbaTestAppPosix cannot be run standalone. It is extremely
// dependent on the UbaTest runner. Please see details in:
// UbaTestSession.h
int main(int argc, char* argv[])
{
	bool runningRemote = false;
	char* tmp = getenv("UBA_REMOTE");
	if (tmp && tmp[0] == '1')
		runningRemote = true;

	// Make the assumption that if we're running remote the detour lib will be there.
	bool foundDetoursLib = IsLibraryLoaded(UBA_DETOURS_LIBRARY) || runningRemote;


	if (!foundDetoursLib)
		return LogError("libUbaDetours not loaded. This app is designed to only start from inside UnrealBuildAccelerator.");

	if (argc == 1)
	{
		char cwd[1024];
		if (!getcwd(cwd, sizeof(cwd)))
			return LogError("getcwd failed");

		struct stat attrR;
		if (stat("FileR.h", &attrR) == -1)
			return LogError("stat for FileR.h failed");
		if (S_ISREG(attrR.st_mode) == 0)
			return LogError("stat for FileR.h did not return normal file");

		int fdr = open("FileR.h", O_RDONLY);
		if (fdr == -1)
			return LogError("open FileR.h failed");
	
		char buf[4];
		if (read(fdr, buf, 4) != 4)
			return LogError("Failed to read FileR.h");

		if (strcmp(buf, "Foo") != 0)
			return LogError("FileR.h content was wrong");

		if (close(fdr) == -1)
			return LogError("close FileR.h failed");

		int fdw = open("FileW", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
		if (fdw == -1)
			return LogError("open FileW failed");

		if (write(fdw, "hello", 6) == -1)
			return LogError("write FileW failed");

		struct stat attrW1;
		if (fstat(fdw, &attrW1) == -1)
			return LogError("fstat FileW failed");

		if (close(fdw) == -1)
			return LogError("closed FileW failed");

		struct stat attrW2;
		if (stat("FileW", &attrW2) == -1)
			return LogError("stat for FileW failed");
		if (S_ISREG(attrW2.st_mode) == 0)
			return LogError("stat for FileW did not return normal file");

		if (rename("FileW", "FileW2") == -1)
			return LogError("rename for FileW to FileW2 failed");


		struct stat attrD1;
		if (stat("Dir1", &attrD1) == -1)
			return LogError("stat for FileW failed");
		if (S_ISREG(attrD1.st_mode) != 0)
			return LogError("stat for FileW did not return directory");

		struct stat attr3;
		if (stat("/usr", &attr3) == -1)
			return LogError("stat for /usr failed");

		FILE* f = fopen("FileWF", "w+");
		if (f == nullptr)
			return LogError("fopen FileWF failed");
		if (fwrite("Hello", 1, 6, f) != 6)
			return LogError("fwrite FileWF failed");
		if (fclose(f) != 0)
			return LogError("fclose FileWF failed");
		struct stat attrWF;
		if (stat("FileWF", &attrWF) == -1)
			return LogError("stat for FileW failed");

	#if !PLATFORM_LINUX // Farm linux machines do not have clang installed... need to revisit this
		char fullPath[PATH_MAX];
		if (realpath("/usr/bin/clang", fullPath) == nullptr)
			return LogError("realpath for 'clang' failed");
	#endif

		struct stat attrRoot;
		if (stat("/", &attrRoot) != 0)
			return LogError("stat for '/' failed");
		return 0;
	}
	else if (strncmp(argv[1], "-file=", 6) == 0)
	{
		void* detoursHandle = dlopen(UBA_DETOURS_LIBRARY, RTLD_LAZY);
		if (!detoursHandle)
			return -3;
		using UbaRequestNextProcessFunc = bool(unsigned int prevExitCode, char* outArguments, unsigned int outArgumentsCapacity);
		static UbaRequestNextProcessFunc* requestNextProcess = (UbaRequestNextProcessFunc*)(void*)dlsym(detoursHandle, "UbaRequestNextProcess");
		if (!requestNextProcess)
			return -8;

		char arguments[1024];
		const char* file = argv[1] + 6;
		while (true)
		{
			int rh = open(file, O_RDONLY);
			if (rh == -1)
				return LogError("Failed to open file %s", file);
			if (close(rh) == -1)
				return LogError("Failed to close file %s", file);

			srand(getpid());
			int milliseconds = rand() % 2000;
			struct timespec ts;
			ts.tv_sec = milliseconds / 1000;
			ts.tv_nsec = (milliseconds % 1000) * 1000000;
			nanosleep(&ts, NULL);

			char outFile[1024];
			strcpy(outFile, file);
			outFile[strlen(file)-3] = 0;
			strcat(outFile, ".out");

			int wh = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
			if (wh == -1)
				return LogError("Failed to create file %s", file);
			if (close(wh) == -1)
				return LogError("Failed to close created file %s", file);

			// Request new process
			if (!requestNextProcess(0, arguments, 1024))
				break; // No process available, exit loop
			file = arguments + 6;
		}

		return 0;
	}
	return -2;
}
