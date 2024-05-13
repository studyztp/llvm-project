#include <stdio.h>
#include <string.h>
#include <stdint.h>

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

__attribute__((profiler_helper))
void roi_begin_() {
    is_profiling = 1;
    fptr = fopen(filename, "a");
    printf("Profiling started\n");
}

__attribute__((profiler_helper))
void roi_end_() {
    is_profiling = 0;
    fclose(fptr);
    printf("Profiling ended\n");
}


