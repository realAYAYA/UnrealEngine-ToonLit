// Copyright Epic Games, Inc. All Rights Reserved.

#if UBA_USE_MIMALLOC

#define DETOURED_CALL_MEM(x) DETOURED_CALL(x)

void* Detoured_malloc(size_t size)
{
	DETOURED_CALL_MEM(malloc);
	return mi_malloc(size);
}

void* Detoured_calloc(size_t number, size_t size)
{
	DETOURED_CALL_MEM(calloc);
	return mi_calloc(number, size);
}

void* Detoured__recalloc(void* memblock, size_t num, size_t size)
{
	DETOURED_CALL_MEM(_recalloc);
	return mi_recalloc(memblock, num, size);
}

void* Detoured_realloc(void* memblock, size_t size)
{
	DETOURED_CALL_MEM(realloc);
	return mi_realloc(memblock, size);
}

void* Detoured__expand(void* memblock, size_t size)
{
	DETOURED_CALL_MEM(_expand);
	return mi_expand(memblock, size);
}

size_t Detoured__msize(void* memblock)
{
	DETOURED_CALL_MEM(free);
	return mi_usable_size(memblock);
}

inline bool IsInMiMalloc(void* ptr)
{
	return (((uintptr_t)ptr - 1) & ~MI_SEGMENT_MASK) && mi_is_in_heap_region(ptr);
}

void Detoured_free(void* memblock)
{
	DETOURED_CALL_MEM(free);
	if (IsInMiMalloc(memblock))
		return mi_free(memblock);
	else
		True_free(memblock);
}

char* Detoured__strdup(const char* s)
{
	DETOURED_CALL_MEM(_strdup);
	return mi_strdup(s);
}

wchar_t* Detoured__wcsdup(const wchar_t* s)
{
	DETOURED_CALL_MEM(_wcsdup);
	return (wchar_t*)mi_wcsdup((const unsigned short*)(s));
}

wchar_t* Detoured__mbsdup(const wchar_t* s)
{
	DETOURED_CALL_MEM(_mbsdup);
	return (wchar_t*)mi_mbsdup((const unsigned char*)(s));
}

void* Detoured__aligned_malloc(size_t size, size_t alignment)
{
	DETOURED_CALL_MEM(_aligned_malloc);
	return mi_malloc_aligned(size, alignment);
}

void* Detoured__aligned_recalloc(void* memblock, size_t num, size_t size, size_t alignment)
{
	DETOURED_CALL_MEM(_aligned_recalloc);
	return mi_aligned_recalloc(memblock, num, size, alignment);
}

void* Detoured__aligned_realloc(void* memblock, size_t size, size_t alignment)
{
	DETOURED_CALL_MEM(_aligned_realloc);
	return mi_realloc_aligned(memblock, size, alignment);
}

void Detoured__aligned_free(void* memblock)
{
	DETOURED_CALL_MEM(_aligned_free);
	return mi_free(memblock);
}

void* Detoured__aligned_offset_malloc(size_t size, size_t alignment, size_t offset)
{
	DETOURED_CALL_MEM(_aligned_offset_malloc);
	return mi_malloc_aligned_at(size, alignment, offset);
}

void* Detoured__aligned_offset_recalloc(void* memblock, size_t num, size_t size, size_t alignment, size_t offset)
{
	DETOURED_CALL_MEM(_aligned_offset_recalloc);
	return mi_recalloc_aligned_at(memblock, num, size, alignment, offset);
}

void* Detoured__aligned_offset_realloc(void* memblock, size_t size, size_t alignment, size_t offset)
{
	DETOURED_CALL_MEM(_aligned_offset_realloc);
	return mi_realloc_aligned_at(memblock, size, alignment, offset);
}

errno_t Detoured__wdupenv_s(wchar_t** buffer, size_t* numberOfElements, const wchar_t* varname)
{
	DETOURED_CALL_MEM(_wdupenv_s);
	auto res = mi_wdupenv_s((unsigned short**)(buffer), numberOfElements, (const unsigned short*)(varname));
	DEBUG_LOG_DETOURED(L"_wdupenv_s", L"(%ls) -> %u", varname, res);
	return res;
}

errno_t Detoured__dupenv_s(char** buffer, size_t* numberOfElements, const char* varname)
{
	DETOURED_CALL_MEM(_dupenv_s);
	auto res = mi_dupenv_s(buffer, numberOfElements, varname);
	DEBUG_LOG_DETOURED(L"_dupenv_s", L"(%hs) -> %u", varname, res);
	return res;
}

void* Detoured__malloc_base(size_t _Size)
{
	return mi_malloc(_Size);
}

void* Detoured__calloc_base(size_t _Count, size_t _Size)
{
	return mi_calloc(_Count, _Size);
}

void* Detoured__realloc_base(void* memblock, size_t size)
{
	return mi_realloc(memblock, size);
}

void Detoured__free_base(void* memblock)
{
	if (!memblock)
		return;
	if (IsInMiMalloc(memblock))
		mi_free(memblock);
	else
		True__free_base(memblock);
}

void* Detoured__expand_base(void* memblock, size_t size)
{
	return mi_expand(memblock, size);
}

size_t Detoured__msize_base(void* memblock)
{
	DETOURED_CALL_MEM(_msize_base);
	return mi_usable_size(memblock);
}

void* Detoured__recalloc_base(void* memblock, size_t num, size_t size)
{
	return mi_recalloc(memblock, num, size);
}

#if defined(DETOURED_INCLUDE_DEBUG)

size_t Detoured__aligned_msize(void* p, size_t alignment, size_t offset)
{
	DETOURED_CALL(_aligned_msize);
	return mi_usable_size(p);
}

//void Detoured__free_dbg(void* userData, int blockType)
//{
//	DETOURED_CALL(_free_dbg);
//	//DEBUG_LOG_TRUE(L"_free_dbg", L"");
//	return True__free_dbg(userData, blockType);
//}

#endif

#endif
