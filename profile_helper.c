#include <stdio.h>
#include <string.h>
#include <stdint.h>
#ifdef PROFILING
#elif M5_SE
#include "gem5/m5ops.h"
#elif M5_FS
#include "gem5/m5ops.h"
#include "m5_mmap.h"
#else
#include "papi.h"
#endif

#ifdef PROFILING

char filename[] = "profiler_output.txt";
FILE *fptr = NULL;
uint64_t is_profiling = 0;

__attribute__((profiler_helper))
void write_single_data(char varName[], uint64_t var) {
    if(is_profiling != 0) {
        is_profiling ++;
        fprintf(fptr, "Region%lu %s: %ld\n", is_profiling-2, varName, var);
    }
}

__attribute__((profiler_helper))
void write_array_data(char varName[], uint64_t* arr, uint32_t n) {
    if (is_profiling != 0) {
        fprintf(fptr, "Region%lu %s: \n", is_profiling-2, varName);
        for (uint32_t i = 0; i < n; i++) {
            fprintf(fptr, "%d:%ld ", i, arr[i]);
        }
        fprintf(fptr, "\n");
    }
}

__attribute__((profiler_helper))
void increment_array_element_at(uint64_t* arr, int index) {
    arr[index]++;
}

__attribute__((profiler_helper))
void reset_array_element_at(uint64_t* arr, int index) {
    arr[index] = 0;
}

__attribute__((profiler_helper))
void increase_array_by(uint64_t* arr, int n, uint64_t inst) {
    for (int i = 0; i < n; i++) {
        arr[i] += inst;
    }
}

__attribute__((profiler_helper))
void reset_array(uint64_t* arr, int n) {
    for (int i = 0; i < n; i++) {
        arr[i] = 0;
    }
}

#endif

__attribute__((profiler_helper))
void roi_begin_() {
#ifdef PROFILING
    is_profiling = 1;
    fptr = fopen(filename, "a");
    printf("Profiling started\n");
#elif M5_SE
    printf("M5_SE ROI started\n");
#elif M5_FS
    m5op_addr = 0x10010000;
    map_m5_mem();
    printf("M5_FS ROI started\n");
#else
    int retval = PAPI_library_init(PAPI_VER_CURRENT);
    if (retval != PAPI_VER_CURRENT) {
        printf("PAPI_library_init failed due to %d.\n", retval);
    }
    retval = PAPI_set_domain(PAPI_DOM_ALL);
    if (retval != PAPI_OK) {
        printf("PAPI_set_domain failed due to %d.\n", retval);
    }
    printf("ROI started\n");
    printf("PAPI initialized\n");
#endif
}

__attribute__((profiler_helper))
void roi_end_() {
#ifdef PROFILING
    is_profiling = 0;
    fclose(fptr);
    printf("Profiling ended\n");
#elif M5_SE
    printf("M5_SE ROI ended\n");
#elif M5_FS
    unmap_m5_mem();
    printf("M5_FS ROI ended\n");
#else
    printf("ROI ended\n");
#endif
}

#ifndef PROFILING

__attribute__((profiler_helper))
void start_marker() {
#ifdef M5_SE
    printf("M5_SE Start marker\n");
    m5_work_begin(0, 0);
#elif M5_FS
    printf("M5_FS Start marker\n");
    m5_work_begin_addr(0,0);
#else
    printf("Start marker\n");
    char str[] = "0";
    int retval = PAPI_hl_region_begin(str);
    if (retval != PAPI_OK) {
        printf("PAPI_hl_region_begin failed due to %d.\n", retval);
    }
#endif
}

__attribute__((profiler_helper))
void end_marker() {
#ifdef M5_SE
    printf("M5_SE End marker\n");
    m5_work_end(0, 0);
#elif M5_FS
    printf("M5_FS End marker\n");
    m5_work_end(0,0);
#else
    char str[] = "0";
    int retval = PAPI_hl_region_end(str);
    if (retval != PAPI_OK) {
        printf("PAPI_hl_region_end failed due to %d.\n", retval);
    }
    printf("End marker\n");
#endif
}

#endif

