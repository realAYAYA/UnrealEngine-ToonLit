// Copyright Epic Games, Inc. All Rights Reserved.

#include "opencv2/unreal.hpp"

#include <atomic>

namespace cv 
{ 
	namespace unreal 
	{
		/** Keeps pointer to Unreal's FMemory::Malloc */
		static std::atomic<TUnrealMalloc> UnrealMalloc(nullptr);

		/** Keeps pointer to Unreal's FMemory::Free */
		static std::atomic<TUnrealFree> UnrealFree(nullptr);

		void SetMallocAndFree(TUnrealMalloc InUnrealMalloc, TUnrealFree InUnrealFree)
		{
			// It is important to assign free first because if it tries to deallocate memory
			// that was allocated with the previous allocator, Unreal will know not to deallocate it.
			
			UnrealFree = InUnrealFree;
			UnrealMalloc = InUnrealMalloc;
		}
	}
}

/** operator new overrides */

void* operator new(std::size_t size)
{
	const cv::unreal::TUnrealMalloc UnrealMalloc = cv::unreal::UnrealMalloc;
	return UnrealMalloc ? UnrealMalloc(size, 0) : malloc(size);
}

void* operator new(std::size_t size, const std::nothrow_t&) noexcept
{
	const cv::unreal::TUnrealMalloc UnrealMalloc = cv::unreal::UnrealMalloc;
	return UnrealMalloc ? UnrealMalloc(size, 0) : malloc(size);
}

void* operator new[](std::size_t size)
{
	const cv::unreal::TUnrealMalloc UnrealMalloc = cv::unreal::UnrealMalloc;
	return UnrealMalloc ? UnrealMalloc(size, 0) : malloc(size);
}

void* operator new[](std::size_t size, const std::nothrow_t&) noexcept
{
	const cv::unreal::TUnrealMalloc UnrealMalloc = cv::unreal::UnrealMalloc;
	return UnrealMalloc ? UnrealMalloc(size, 0) : malloc(size);
}

/** operator delete overrides */

void  operator delete(void* p) noexcept
{
	const cv::unreal::TUnrealFree UnrealFree = cv::unreal::UnrealFree;
	UnrealFree ? UnrealFree(p) : free(p);
}

void  operator delete(void* p, const std::nothrow_t&) noexcept
{
	const cv::unreal::TUnrealFree UnrealFree = cv::unreal::UnrealFree;
	UnrealFree ? UnrealFree(p) : free(p);
}

void  operator delete[](void* p) noexcept
{
	const cv::unreal::TUnrealFree UnrealFree = cv::unreal::UnrealFree;
	UnrealFree ? UnrealFree(p) : free(p);
}

void  operator delete[](void* p, const std::nothrow_t&) noexcept
{
	const cv::unreal::TUnrealFree UnrealFree = cv::unreal::UnrealFree;
	UnrealFree ? UnrealFree(p) : free(p);
}
