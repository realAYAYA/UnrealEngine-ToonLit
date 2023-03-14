// Copyright Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsPlatformMemory.h"
#include "Misc/AssertionMacros.h"
#include "Logging/LogMacros.h"
#include "Misc/OutputDevice.h"
#include "Math/NumericLimits.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Misc/Guid.h"
#include "Stats/Stats.h"
#include "GenericPlatform/GenericPlatformMemoryPoolStats.h"

#include "HAL/MallocTBB.h"
#include "HAL/MallocAnsi.h"
#include "HAL/MallocMimalloc.h"
#include "HAL/MallocStomp.h"
#include "HAL/MemoryMisc.h"
#include "HAL/MallocBinned.h"
#include "HAL/MallocBinned2.h"
#include "HAL/MallocBinned3.h"
#include "HAL/MallocStomp2.h"
#include "HAL/MallocDoubleFreeFinder.h"
#include "HAL/IConsoleManager.h"
#include "Windows/WindowsHWrapper.h"

#pragma warning(disable:6250)

#if ENABLE_WIN_ALLOC_TRACKING
#include <crtdbg.h>
#endif // ENABLE_WIN_ALLOC_TRACKING

#include "Windows/AllowWindowsPlatformTypes.h"
#include <Psapi.h>
#pragma comment(lib, "psapi.lib")

DECLARE_MEMORY_STAT(TEXT("Windows Specific Memory Stat"),	STAT_WindowsSpecificMemoryStat, STATGROUP_MemoryPlatform);


static int32 GWindowsPlatformMemoryGetStatsLimitTotalGB = 0;
static FAutoConsoleVariableRef CVarLogPlatformMemoryStats(
	TEXT("memory.WindowsPlatformMemoryGetStatsLimitTotalGB"),
	GWindowsPlatformMemoryGetStatsLimitTotalGB,
	TEXT("Set a synthetic platform total memory size (in GB) which will be returned as Total and Available memory from GetStats\n"),
	ECVF_Default);


#if ENABLE_WIN_ALLOC_TRACKING
// This allows tracking of allocations that don't happen within the engine's wrappers.
// You will probably want to set conditional breakpoints here to capture specific allocations
// which aren't related to static initialization, they will happen on the CRT anyway.
int WindowsAllocHook(int nAllocType, void *pvData,
				  size_t nSize, int nBlockUse, long lRequest,
				  const unsigned char * szFileName, int nLine )
{
	return true;
}
#endif // ENABLE_WIN_ALLOC_TRACKING



void FWindowsPlatformMemory::Init()
{
	// Only allow this method to be called once
	{
		static bool bInitDone = false;
		if (bInitDone)
			return;
		bInitDone = true;
	}

	FGenericPlatformMemory::Init();

#if PLATFORM_32BITS
	const int64 GB(1024*1024*1024);
	SET_MEMORY_STAT(MCR_Physical, 2*GB); //2Gb of physical memory on win32
	SET_MEMORY_STAT(MCR_PhysicalLLM, 5*GB);	// no upper limit on Windows. Choose 5GB because it's roughly the same as current consoles.
#endif

	const FPlatformMemoryConstants& MemoryConstants = FPlatformMemory::GetConstants();
#if PLATFORM_32BITS	
	UE_LOG(LogMemory, Log, TEXT("Memory total: Physical=%.1fGB (%dGB approx) Virtual=%.1fGB"), 
		(double)MemoryConstants.TotalPhysical/1024.0/1024.0/1024.0,
		MemoryConstants.TotalPhysicalGB, 
		(double)MemoryConstants.TotalVirtual/1024.0/1024.0/1024.0 );
#else
	// Logging virtual memory size for 64bits is pointless.
	UE_LOG(LogMemory, Log, TEXT("Memory total: Physical=%.1fGB (%dGB approx)"), 
		(double)MemoryConstants.TotalPhysical/1024.0/1024.0/1024.0,
		MemoryConstants.TotalPhysicalGB );
#endif //PLATFORM_32BITS

	// program size is hard to ascertain and isn't so relevant on Windows. For now just set to zero.
	LLM(FLowLevelMemTracker::Get().SetProgramSize(0));

	DumpStats( *GLog );
}

// Set rather to use BinnedMalloc2 for binned malloc, can be overridden below
#define USE_MALLOC_BINNED2 (1)
#if !defined(USE_MALLOC_BINNED3)
	#define USE_MALLOC_BINNED3 (0)
#endif

FMalloc* FWindowsPlatformMemory::BaseAllocator()
{
#if ENABLE_WIN_ALLOC_TRACKING
	// This allows tracking of allocations that don't happen within the engine's wrappers.
	// This actually won't be compiled unless bDebugBuildsActuallyUseDebugCRT is set in the
	// build configuration for UBT.
	_CrtSetAllocHook(WindowsAllocHook);
#endif // ENABLE_WIN_ALLOC_TRACKING

	if (FORCE_ANSI_ALLOCATOR) //-V517
	{
		AllocatorToUse = EMemoryAllocatorToUse::Ansi;
	}
#if PLATFORM_64BITS
	// Mimalloc is now the default allocator for editor and programs because it has shown
	// both great performance and as much as half the memory usage of TBB after
	// heavy editor workloads. See CL 15887498 description for benchmarks.
	else if ((WITH_EDITORONLY_DATA || IS_PROGRAM) && MIMALLOC_ALLOCATOR_ALLOWED) //-V517
	{
		AllocatorToUse = EMemoryAllocatorToUse::Mimalloc;
	}
#endif
	else if ((WITH_EDITORONLY_DATA || IS_PROGRAM) && TBB_ALLOCATOR_ALLOWED) //-V517
	{
		AllocatorToUse = EMemoryAllocatorToUse::TBB;
	}
#if PLATFORM_64BITS
	else if (USE_MALLOC_BINNED3)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned3;
	}
#endif
	else if (USE_MALLOC_BINNED2)
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned2;
	}
	else
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned;
	}
	
#if !UE_BUILD_SHIPPING
	// If not shipping, allow overriding with command line options, this happens very early so we need to use windows functions
	const TCHAR* CommandLine = ::GetCommandLineW();

	if (FCString::Stristr(CommandLine, TEXT("-ansimalloc")))
	{
		// see FPlatformMisc::GetProcessDiagnostics()
		AllocatorToUse = EMemoryAllocatorToUse::Ansi;
	}
#if TBB_ALLOCATOR_ALLOWED
	else if (FCString::Stristr(CommandLine, TEXT("-tbbmalloc")))
	{
		AllocatorToUse = EMemoryAllocatorToUse::TBB;
	}
#endif
#if MIMALLOC_ALLOCATOR_ALLOWED
	else if (FCString::Stristr(CommandLine, TEXT("-mimalloc")))
	{
		AllocatorToUse = EMemoryAllocatorToUse::Mimalloc;
	}
#endif
#if PLATFORM_64BITS
	else if (FCString::Stristr(CommandLine, TEXT("-binnedmalloc3")))
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned3;
	}
#endif
	else if (FCString::Stristr(CommandLine, TEXT("-binnedmalloc2")))
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned2;
	}
	else if (FCString::Stristr(CommandLine, TEXT("-binnedmalloc")))
	{
		AllocatorToUse = EMemoryAllocatorToUse::Binned;
	}
#if WITH_MALLOC_STOMP
	else if (FCString::Stristr(CommandLine, TEXT("-stompmalloc")))
	{
		// see FPlatformMisc::GetProcessDiagnostics()
		AllocatorToUse = EMemoryAllocatorToUse::Stomp;
	}
#endif // WITH_MALLOC_STOMP
#if WITH_MALLOC_STOMP2
	if (FCString::Stristr(CommandLine, TEXT("-stomp2malloc")))
	{
		GMallocStomp2Enabled = true;
	}
#endif // WITH_MALLOC_STOMP2

	if (FCString::Stristr(CommandLine, TEXT("-doublefreefinder")))
	{
		GMallocDoubleFreeFinderEnabled = true;
	}

#endif // !UE_BUILD_SHIPPING

	switch (AllocatorToUse)
	{
	case EMemoryAllocatorToUse::Ansi:
		return new FMallocAnsi();
#if WITH_MALLOC_STOMP
	case EMemoryAllocatorToUse::Stomp:
		return new FMallocStomp();
#endif
#if TBB_ALLOCATOR_ALLOWED
	case EMemoryAllocatorToUse::TBB:
		return new FMallocTBB();
#endif
#if MIMALLOC_ALLOCATOR_ALLOWED && PLATFORM_SUPPORTS_MIMALLOC && PLATFORM_BUILDS_MIMALLOC
	case EMemoryAllocatorToUse::Mimalloc:
		return new FMallocMimalloc();
#endif
	case EMemoryAllocatorToUse::Binned2:
		return new FMallocBinned2();
#if PLATFORM_64BITS
	case EMemoryAllocatorToUse::Binned3:
		return new FMallocBinned3();
#endif
	default:	// intentional fall-through
	case EMemoryAllocatorToUse::Binned:
		return new FMallocBinned((uint32)(GetConstants().BinnedPageSize&MAX_uint32), (uint64)MAX_uint32 + 1);
	}
}

FPlatformMemoryStats FWindowsPlatformMemory::GetStats()
{
	// release mimalloc held pages back to OS before getting stats
	// can't do this, it calls me :
	//FMemory::Trim(true);

	/**
	 *	GlobalMemoryStatusEx 
	 *	MEMORYSTATUSEX 
	 *		ullTotalPhys
	 *		ullAvailPhys
	 *		ullTotalVirtual
	 *		ullAvailVirtual
	 *		
	 *	GetProcessMemoryInfo
	 *	PROCESS_MEMORY_COUNTERS
	 *		WorkingSetSize
	 *		UsedVirtual
	 *		PeakUsedVirtual
	 *		
	 *	GetPerformanceInfo
	 *		PPERFORMANCE_INFORMATION 
	 *		PageSize
	 */

	// This method is slow, do not call it too often.
	// #TODO Should be executed only on the background thread.

	FPlatformMemoryStats MemoryStats;

	// Gather platform memory stats.
	MEMORYSTATUSEX MemoryStatusEx;
	FPlatformMemory::Memzero( &MemoryStatusEx, sizeof( MemoryStatusEx ) );
	MemoryStatusEx.dwLength = sizeof( MemoryStatusEx );
	::GlobalMemoryStatusEx( &MemoryStatusEx );

	PROCESS_MEMORY_COUNTERS ProcessMemoryCounters;
	FPlatformMemory::Memzero( &ProcessMemoryCounters, sizeof( ProcessMemoryCounters ) );
	::GetProcessMemoryInfo( ::GetCurrentProcess(), &ProcessMemoryCounters, sizeof(ProcessMemoryCounters) );

	MemoryStats.TotalPhysical = MemoryStatusEx.ullTotalPhys;
	MemoryStats.AvailablePhysical = MemoryStatusEx.ullAvailPhys;
	MemoryStats.AvailableVirtual = MemoryStatusEx.ullAvailVirtual;

	// On Windows, ullTotalVirtual is artificial and represent the SKU limitation (128TB on Win10Pro) instead of the commit limit of the system we're after.
	// ullTotalPageFile represents PhysicalMemory + Disk Swap Space, which is the value we care about
	MemoryStats.TotalVirtual = MemoryStatusEx.ullTotalPageFile;

	// On Windows, Virtual Memory is limited per process to the address space (e.g. 47 bits (128Tb)), but is additionally limited by the sum of used virtual memory across all processes
	// must be less than PhysicalMemory plus the Virtual Memory Page Size. The remaining virtual memory space given this system-wide limit is stored in ullAvailPageSize
	MemoryStats.AvailableVirtual = FMath::Min(MemoryStats.AvailableVirtual, MemoryStatusEx.ullAvailPageFile);

	MemoryStats.UsedPhysical = ProcessMemoryCounters.WorkingSetSize;
	MemoryStats.PeakUsedPhysical = ProcessMemoryCounters.PeakWorkingSetSize;
	// PagefileUsage = commit charge
	MemoryStats.UsedVirtual = ProcessMemoryCounters.PagefileUsage;
	MemoryStats.PeakUsedVirtual = ProcessMemoryCounters.PeakPagefileUsage;

	if ( GWindowsPlatformMemoryGetStatsLimitTotalGB > 0 )
	{
		// if GWindowsPlatformMemoryGetStatsLimitTotalGB is set
		// apply a synthetic limit to the Total memory on the system for testing
		// note the first couple of calls to this function are before the cvar for GWindowsPlatformMemoryGetStatsLimitTotalGB
		//	is processed (eg. for LLM init), and that's okay
		const uint64 TestHardLimit = (int64)GWindowsPlatformMemoryGetStatsLimitTotalGB << 30;

		// "Available" is the amount free on the whole system
		// "Used" is only what our app has used
		// the synthetic limit here pretends that our app is the only thing on the system
		//   eg. we get to use up to 100% of TestHardLimit, usage by other apps is not counted
		//   except when your real system runs out of memory

		if ( MemoryStats.UsedVirtual > TestHardLimit )
		{
			MemoryStats.TotalVirtual = MemoryStats.UsedVirtual;
			MemoryStats.AvailableVirtual = 0;
		}
		else
		{
			MemoryStats.TotalVirtual = TestHardLimit;
			MemoryStats.AvailableVirtual = FMath::Min(MemoryStats.AvailableVirtual, TestHardLimit - MemoryStats.UsedVirtual);	
		}
		
		if ( MemoryStats.UsedPhysical > TestHardLimit )
		{
			MemoryStats.TotalPhysical = MemoryStats.UsedPhysical;
			MemoryStats.AvailablePhysical = 0;
		}
		else
		{
			MemoryStats.TotalPhysical = TestHardLimit;
			MemoryStats.AvailablePhysical = FMath::Min(MemoryStats.AvailablePhysical,TestHardLimit - MemoryStats.UsedPhysical);
		}
	}

	return MemoryStats;
}

void FWindowsPlatformMemory::GetStatsForMallocProfiler( FGenericMemoryStats& out_Stats )
{
#if	STATS
	FGenericPlatformMemory::GetStatsForMallocProfiler( out_Stats );

	FPlatformMemoryStats Stats = GetStats();

	// Windows specific stats.
	out_Stats.Add( GET_STATDESCRIPTION( STAT_WindowsSpecificMemoryStat ), Stats.WindowsSpecificMemoryStat );
#endif // STATS
}

const FPlatformMemoryConstants& FWindowsPlatformMemory::GetConstants()
{
	static FPlatformMemoryConstants MemoryConstants;

	if( MemoryConstants.TotalPhysical == 0 )
	{
		// Gather platform memory constants.
		MEMORYSTATUSEX MemoryStatusEx;
		FPlatformMemory::Memzero( &MemoryStatusEx, sizeof( MemoryStatusEx ) );
		MemoryStatusEx.dwLength = sizeof( MemoryStatusEx );
		::GlobalMemoryStatusEx( &MemoryStatusEx );

		SYSTEM_INFO SystemInfo;
		FPlatformMemory::Memzero( &SystemInfo, sizeof( SystemInfo ) );
		::GetSystemInfo(&SystemInfo);

		MemoryConstants.TotalPhysical = MemoryStatusEx.ullTotalPhys;
		// On Windows, ullTotalVirtual is artificial and represent the SKU limitation (128TB on Win10Pro) instead of the commit limit of the system we're after.
		// ullTotalPageFile represents PhysicalMemory + Disk Swap Space, which is the value we care about
		MemoryConstants.TotalVirtual = MemoryStatusEx.ullTotalPageFile;
		MemoryConstants.BinnedPageSize = SystemInfo.dwAllocationGranularity;	// Use this so we get larger 64KiB pages, instead of 4KiB
		MemoryConstants.BinnedAllocationGranularity = SystemInfo.dwPageSize; // Use 4KiB pages for more efficient use of memory - 64KiB pages don't really exist on this CPU
		MemoryConstants.OsAllocationGranularity = SystemInfo.dwAllocationGranularity;	// VirtualAlloc cannot allocate memory less than that
		MemoryConstants.PageSize = SystemInfo.dwPageSize;
		MemoryConstants.AddressLimit = FPlatformMath::RoundUpToPowerOfTwo64(MemoryConstants.TotalPhysical);

		MemoryConstants.TotalPhysicalGB = (uint32)((MemoryConstants.TotalPhysical + 1024 * 1024 * 1024 - 1) / 1024 / 1024 / 1024);
	}

	return MemoryConstants;	
}

bool FWindowsPlatformMemory::PageProtect(void* const Ptr, const SIZE_T Size, const bool bCanRead, const bool bCanWrite)
{
	DWORD flOldProtect;
	uint32 ProtectMode = 0;
	if (bCanRead && bCanWrite)
	{
		ProtectMode = PAGE_READWRITE;
	}
	else if (bCanWrite)
	{
		ProtectMode = PAGE_READWRITE;
	}
	else if (bCanRead)
	{
		ProtectMode = PAGE_READONLY;
	}
	else
	{
		ProtectMode = PAGE_NOACCESS;
	}
	return VirtualProtect(Ptr, Size, ProtectMode, &flOldProtect) != 0;
}
void* FWindowsPlatformMemory::BinnedAllocFromOS( SIZE_T Size )
{
	void* Ptr = VirtualAlloc( NULL, Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE );
	LLM(FLowLevelMemTracker::Get().OnLowLevelAlloc(ELLMTracker::Platform, Ptr, Size));
	return Ptr;
}

void FWindowsPlatformMemory::BinnedFreeToOS( void* Ptr, SIZE_T Size )
{
	LLM(FLowLevelMemTracker::Get().OnLowLevelFree(ELLMTracker::Platform, Ptr));

	CA_SUPPRESS(6001)
	// Windows maintains the size of allocation internally, so Size is unused
	verify(VirtualFree( Ptr, 0, MEM_RELEASE ) != 0);
}


size_t FWindowsPlatformMemory::FPlatformVirtualMemoryBlock::GetVirtualSizeAlignment()
{
	static SIZE_T OsAllocationGranularity = FPlatformMemory::GetConstants().OsAllocationGranularity;
	return OsAllocationGranularity;
}

size_t FWindowsPlatformMemory::FPlatformVirtualMemoryBlock::GetCommitAlignment()
{
	static SIZE_T OSPageSize = FPlatformMemory::GetConstants().PageSize;
	return OSPageSize;
}

FWindowsPlatformMemory::FPlatformVirtualMemoryBlock FWindowsPlatformMemory::FPlatformVirtualMemoryBlock::AllocateVirtual(size_t InSize, size_t InAlignment)
{
	FPlatformVirtualMemoryBlock Result;
	InSize = Align(InSize, GetVirtualSizeAlignment());
	Result.VMSizeDivVirtualSizeAlignment = InSize / GetVirtualSizeAlignment();

	size_t Alignment = FMath::Max(InAlignment, GetVirtualSizeAlignment());
	check(Alignment <= GetVirtualSizeAlignment());

	bool bTopDown = Result.GetActualSize() > 100ll * 1024 * 1024; // this is hacky, but we want to allocate huge VM blocks (like for MB3) top down

	Result.Ptr = VirtualAlloc(NULL, Result.GetActualSize(), MEM_RESERVE | (bTopDown ? MEM_TOP_DOWN : 0), PAGE_NOACCESS);


	if (!LIKELY(Result.Ptr))
	{
		FPlatformMemory::OnOutOfMemory(Result.GetActualSize(), Alignment);
	}
	check(Result.Ptr && IsAligned(Result.Ptr, Alignment));
	return Result;
}



void FWindowsPlatformMemory::FPlatformVirtualMemoryBlock::FreeVirtual()
{
	if (Ptr)
	{
		check(GetActualSize() > 0);

		CA_SUPPRESS(6001)
		// Windows maintains the size of allocation internally, so Size is unused
		verify(VirtualFree(Ptr, 0, MEM_RELEASE) != 0);

		Ptr = nullptr;
		VMSizeDivVirtualSizeAlignment = 0;
	}
}

void FWindowsPlatformMemory::FPlatformVirtualMemoryBlock::Commit(size_t InOffset, size_t InSize)
{
	check(IsAligned(InOffset, GetCommitAlignment()) && IsAligned(InSize, GetCommitAlignment()));
	check(InOffset >= 0 && InSize >= 0 && InOffset + InSize <= GetActualSize() && Ptr);

	uint8* UsePtr = ((uint8*)Ptr) + InOffset;
	if (VirtualAlloc(UsePtr, InSize, MEM_COMMIT, PAGE_READWRITE) != UsePtr)
	{
		FPlatformMemory::OnOutOfMemory(InSize, 0);
	}
}

void FWindowsPlatformMemory::FPlatformVirtualMemoryBlock::Decommit(size_t InOffset, size_t InSize)
{
	check(IsAligned(InOffset, GetCommitAlignment()) && IsAligned(InSize, GetCommitAlignment()));
	check(InOffset >= 0 && InSize >= 0 && InOffset + InSize <= GetActualSize() && Ptr);
	uint8* UsePtr = ((uint8*)Ptr) + InOffset;
	VirtualFree(UsePtr, InSize, MEM_DECOMMIT);
}





FPlatformMemory::FSharedMemoryRegion* FWindowsPlatformMemory::MapNamedSharedMemoryRegion(const FString& InName, bool bCreate, uint32 AccessMode, SIZE_T Size, const void* pSecurityAttributes)
{
	FString Name;

	// Use {Guid} as the name for the memory region without prefix.
	FGuid Guid;
	if (FGuid::ParseExact(InName, EGuidFormats::DigitsWithHyphensInBraces, Guid))
	{
		// Only the Guid string is used as the name of the memory region. It works without administrator rights
		Name = Guid.ToString(EGuidFormats::DigitsWithHyphensInBraces);
	}
	else
	{
		// The prefix "Global\\" is used to share this region with other processes. This method requires administrator rights, if pSecurityAttributes not defined
		Name = TEXT("Global\\");
		Name += InName;
	}

	DWORD OpenMappingAccess = FILE_MAP_READ;
	check(AccessMode != 0);
	if (AccessMode == FPlatformMemory::ESharedMemoryAccess::Write)
	{
		OpenMappingAccess = FILE_MAP_WRITE;
	}
	else if (AccessMode == (FPlatformMemory::ESharedMemoryAccess::Write | FPlatformMemory::ESharedMemoryAccess::Read))
	{
		OpenMappingAccess = FILE_MAP_ALL_ACCESS;
	}

	HANDLE Mapping = NULL;
	if (bCreate)
	{
		DWORD CreateMappingAccess = PAGE_READONLY;
		check(AccessMode != 0);
		if (AccessMode == FPlatformMemory::ESharedMemoryAccess::Write)
		{
			CreateMappingAccess = PAGE_WRITECOPY;
		}
		else if (AccessMode == (FPlatformMemory::ESharedMemoryAccess::Write | FPlatformMemory::ESharedMemoryAccess::Read))
		{
			CreateMappingAccess = PAGE_READWRITE;
		}

		DWORD MaxSizeHigh = 
#if PLATFORM_64BITS
			(Size >> 32);
#else
			0;
#endif // PLATFORM_64BITS

		DWORD MaxSizeLow = Size
#if PLATFORM_64BITS
			& 0xFFFFFFFF
#endif // PLATFORM_64BITS
			;

		Mapping = CreateFileMapping(INVALID_HANDLE_VALUE, (SECURITY_ATTRIBUTES*)pSecurityAttributes, CreateMappingAccess, MaxSizeHigh, MaxSizeLow, *Name);

		if (Mapping == NULL)
		{
			DWORD ErrNo = GetLastError();
			UE_LOG(LogHAL, Warning, TEXT("CreateFileMapping(file=INVALID_HANDLE_VALUE, security=NULL, protect=0x%x, MaxSizeHigh=%d, MaxSizeLow=%d, name='%s') failed with GetLastError() = %d"), 
				CreateMappingAccess, MaxSizeHigh, MaxSizeLow, *Name,
				ErrNo
				);
		}
	}
	else
	{
		Mapping = OpenFileMapping(OpenMappingAccess, FALSE, *Name);

		if (Mapping == NULL)
		{
			DWORD ErrNo = GetLastError();
			UE_LOG(LogHAL, Warning, TEXT("OpenFileMapping(access=0x%x, inherit=false, name='%s') failed with GetLastError() = %d"), 
				OpenMappingAccess, *Name,
				ErrNo
				);
		}
	}

	if (Mapping == NULL)
	{
		return NULL;
	}

	void* Ptr = MapViewOfFile(Mapping, OpenMappingAccess, 0, 0, Size);
	if (Ptr == NULL)
	{
		DWORD ErrNo = GetLastError();
		UE_LOG(LogHAL, Warning, TEXT("MapViewOfFile(mapping=0x%x, access=0x%x, OffsetHigh=0, OffsetLow=0, NumBytes=%u) failed with GetLastError() = %d"), 
			Mapping, OpenMappingAccess, Size,
			ErrNo
			);

		CloseHandle(Mapping);
		return NULL;
	}

	return new FWindowsSharedMemoryRegion(Name, AccessMode, Ptr, Size, Mapping);
}

bool FWindowsPlatformMemory::UnmapNamedSharedMemoryRegion(FSharedMemoryRegion * MemoryRegion)
{
	bool bAllSucceeded = true;

	if (MemoryRegion)
	{
		FWindowsSharedMemoryRegion * WindowsRegion = static_cast< FWindowsSharedMemoryRegion* >( MemoryRegion );

		if (!UnmapViewOfFile(WindowsRegion->GetAddress()))
		{
			bAllSucceeded = false;

			int ErrNo = GetLastError();
			UE_LOG(LogHAL, Warning, TEXT("UnmapViewOfFile(address=%p) failed with GetLastError() = %d"), 
				WindowsRegion->GetAddress(),
				ErrNo
				);
		}

		if (!CloseHandle(WindowsRegion->GetMapping()))
		{
			bAllSucceeded = false;

			int ErrNo = GetLastError();
			UE_LOG(LogHAL, Warning, TEXT("CloseHandle(handle=0x%x) failed with GetLastError() = %d"), 
				WindowsRegion->GetMapping(),
				ErrNo
				);
		}

		// delete the region
		delete WindowsRegion;
	}

	return bAllSucceeded;
}

void FWindowsPlatformMemory::InternalUpdateStats( const FPlatformMemoryStats& MemoryStats )
{
	// Windows specific stats.
	SET_MEMORY_STAT( STAT_WindowsSpecificMemoryStat, MemoryStats.WindowsSpecificMemoryStat );
}

/**
* LLM uses these low level functions (LLMAlloc and LLMFree) to allocate memory. It grabs
* the function pointers by calling FPlatformMemory::GetLLMAllocFunctions. If these functions
* are not implemented GetLLMAllocFunctions should return false and LLM will be disabled.
*/

#if ENABLE_LOW_LEVEL_MEM_TRACKER

int64 LLMMallocTotal = 0;

const size_t LLMPageSize = 4096;

void* LLMAlloc(size_t Size)
{
	size_t AlignedSize = Align(Size, LLMPageSize);

	off_t DirectMem = 0;
	void* Addr = VirtualAlloc(NULL, Size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

	check(Addr);

	LLMMallocTotal += static_cast<int64>(AlignedSize);

	return Addr;
}

void LLMFree(void* Addr, size_t Size)
{
	VirtualFree(Addr, 0, MEM_RELEASE);

	size_t AlignedSize = Align(Size, LLMPageSize);
	LLMMallocTotal -= static_cast<int64>(AlignedSize);
}


bool FWindowsPlatformMemory::GetLLMAllocFunctions(void*(*&OutAllocFunction)(size_t), void(*&OutFreeFunction)(void*, size_t), int32& OutAlignment)
{
	OutAllocFunction = LLMAlloc;
	OutFreeFunction = LLMFree;
	OutAlignment = LLMPageSize;

	return true;
}

#endif // ENABLE_LOW_LEVEL_MEM_TRACKER

#include "Windows/HideWindowsPlatformTypes.h"
