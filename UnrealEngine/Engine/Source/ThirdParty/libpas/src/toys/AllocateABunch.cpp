#include "bmalloc_heap.h"
#include "pas_reservation.h"
#include "pas_scavenger.h"
#include "pas_status_reporter.h"
#include "pas_utils.h"
#include <stdio.h>
#include <windows.h>
#include <psapi.h>

static size_t memoryUsage()
{
    PROCESS_MEMORY_COUNTERS_EX memoryCounters;
    bool result = GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryCounters), sizeof(memoryCounters));
    PAS_ASSERT(result);
    return memoryCounters.PagefileUsage;
}

int main(int argc, char** argv)
{
    unsigned objectSize;
    unsigned numObjects;
    unsigned repeats;
    void** objectArray;

    if (argc != 4
        || sscanf(argv[1], "%u", &objectSize) != 1
        || sscanf(argv[2], "%u", &numObjects) != 1
        || sscanf(argv[3], "%u", &repeats) != 1) {
        fprintf(stderr, "Usage: AllocateABunch <objectSize> <numObjects> <repeats>\n");
        return 1;
    }

    //pas_reservation_commit_mode = pas_committed;

    printf("objectSize = %u\n", objectSize);
    printf("numObjects = %u\n", numObjects);
    printf("repeats = %u\n", repeats);

    printf("memory usage = %zu\n", memoryUsage());

    objectArray = static_cast<void**>(bmalloc_allocate(sizeof(void*) * numObjects));
    
    printf("After allocating objectArray, memory usage = %zu\n", memoryUsage());

    for (unsigned repeatIndex = 0; repeatIndex < repeats; ++repeatIndex) {
        for (unsigned objectIndex = 0; objectIndex < numObjects; ++objectIndex)
            objectArray[objectIndex] = bmalloc_allocate(objectSize);
        for (unsigned objectIndex = 0; objectIndex < numObjects; ++objectIndex)
            bmalloc_deallocate(objectArray[objectIndex]);
        printf("After repeat %u, memory usage = %zu\n", repeatIndex, memoryUsage());
    }

    pas_scavenger_run_synchronously_now();
    
    printf("After scavenging, memory usage = %zu\n", memoryUsage());

    bmalloc_deallocate(objectArray);
    pas_scavenger_run_synchronously_now();
    
    printf("After freeing the object array and scavenging again, memory usage = %zu\n", memoryUsage());

    pas_status_reporter_print_everything();
    
    printf("After printing everything, memory usage = %zu\n", memoryUsage());
    
    return 0;
}


