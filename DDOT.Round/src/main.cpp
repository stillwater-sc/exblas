/*
 * Copyright 1993-2010 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

// standard utilities and systems includes
#include "common.hpp"

#define NUM_ITER  20
////////////////////////////////////////////////////////////////////////////////
// Variables used in the program 
////////////////////////////////////////////////////////////////////////////////
cl_platform_id    cpPlatform;        //OpenCL platform
cl_device_id      cdDevice;          //OpenCL device list
cl_context        cxGPUContext;      //OpenCL context
cl_command_queue  cqCommandQueue;    //OpenCL command que
cl_mem            d_a, d_b;          //OpenCL memory buffer objects
cl_mem            d_Res;
void              *h_a, *h_b;
double            h_Res;

static uint __nbElements = 0;
static uint __range      = 0;
static uint __nbfpe      = 0;
static uint __alg        = 0;

static void __usage(int argc __attribute__((unused)), char **argv) {
    fprintf(stderr, "Usage: %s [-n number of elements,\n", argv[0]);
    printf("                -r range,\n"); 
    printf("                -e nbfpe,\n");
    printf("                -a alg (0-ddot, 1-acc, 2-fpe, 3-fpeex4, 4-fpeex6, 5-fpeex8)] \n");
    printf("       -?, -h:    Display this help and exit\n");
}

static void __parse_args(int argc, char **argv) {
    int i;

    for (i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-n") == 0)) {
             __nbElements = atoi(argv[++i]);
        } if ((strcmp(argv[i], "-r") == 0)) {
            __range = atoi(argv[++i]);
        } if ((strcmp(argv[i], "-e") == 0)) {
            __nbfpe = atoi(argv[++i]);
        } if ((strcmp(argv[i], "-a") == 0)) {
            __alg = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-h") || strcmp(argv[i], "-?")) == 0) {
            __usage(argc, argv);
            exit(-1);
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option %s\n", argv[i]);
            __usage(argc, argv);
            exit(-1);
        }
    }

    if (__nbElements <= 0) {
        __usage(argc, argv);
        exit(-1);
    }
    if (__alg > 5) {
        __usage(argc, argv);
        exit(-1);
    }
}

int cleanUp (
    int exitCode
);

////////////////////////////////////////////////////////////////////////////////
//Test driver
////////////////////////////////////////////////////////////////////////////////
int main(int argc, char **argv)
{
    __parse_args(argc, argv);
    printf("Starting with two vectors with  %i double elements\n\n", __nbElements); 

    if (__alg == 0) {
        runDDOTStandard("../src/DDOT.Standard.cl");
    } else if (__alg == 1) {
        runDDOT("../src/DDOT.Superacc.cl");
    } else if (__alg == 2) {
        runDDOT("../src/DDOT.FPE.cl");
    } else if (__alg == 3) {
        __nbfpe = 8;
        runDDOT("../src/DDOT.FPE.EX.4.cl");
    } else if (__alg == 4) {
        __nbfpe = 4;
        runDDOT("../src/DDOT.FPE.EX.6.cl");
    } else if (__alg == 5) {
        __nbfpe = 6;
        runDDOT("../src/DDOT.FPE.EX.8.cl");
    }
}

int runDDOT(const char* program_file){
    cl_int ciErrNum;
    bool PassFailFlag = false;

    printf("Initializing data...\n");
        PassFailFlag  = posix_memalign(&h_a, 64, __nbElements * sizeof(double));
        PassFailFlag |= posix_memalign(&h_b, 64, __nbElements * sizeof(double));
        if (PassFailFlag != 0) {
            printf("ERROR: could not allocate memory with posix_memalign!\n");
            exit(1);
        }

        // init data
        int emax = E_BITS - log2(__nbElements);// use log in order to stay within [emin, emax]
        init_fpuniform((double *) h_a, __nbElements, __range, emax);
        init_fpuniform((double *) h_b, __nbElements, __range, emax);

    printf("Initializing OpenCL...\n");
        char platform_name[64];
#ifdef AMD
        strcpy(platform_name, "AMD Accelerated Parallel Processing");
#else
        strcpy(platform_name, "NVIDIA CUDA");
#endif
        //setenv("CUDA_CACHE_DISABLE", "1", 1);
        cpPlatform = GetOCLPlatform(platform_name);
        if (cpPlatform == NULL) {
            printf("ERROR: Failed to find the platform '%s' ...\n", platform_name);
            return -1;
        }

        //Get a GPU device
        cdDevice = GetOCLDevice(cpPlatform);
        if (cdDevice == NULL) {
            printf("Error in clGetDeviceIDs, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            return -1;
        }

        //Create the context
        cxGPUContext = clCreateContext(0, 1, &cdDevice, NULL, NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS) {
            printf("Error in clCreateContext, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            cleanUp(EXIT_FAILURE);
        }

        //Create a command-queue
        cqCommandQueue = clCreateCommandQueue(cxGPUContext, cdDevice, CL_QUEUE_PROFILING_ENABLE, &ciErrNum);
        if (ciErrNum != CL_SUCCESS) {
            printf("Error in clCreateCommandQueue, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            cleanUp(EXIT_FAILURE);
        }

    printf("Allocating OpenCL memory...\n\n");
        size_t size = __nbElements * sizeof(cl_double);
        d_a = clCreateBuffer(cxGPUContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, size, h_a, &ciErrNum);
        if (ciErrNum != CL_SUCCESS) {
            printf("Error in clCreateBuffer for d_a, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            cleanUp(EXIT_FAILURE);
        }
        d_b = clCreateBuffer(cxGPUContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, size, h_b, &ciErrNum);
        if (ciErrNum != CL_SUCCESS) {
            printf("Error in clCreateBuffer for d_b, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            cleanUp(EXIT_FAILURE);
        }
        d_Res = clCreateBuffer(cxGPUContext, CL_MEM_READ_WRITE, sizeof(cl_double), NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS) {
            printf("Error in clCreateBuffer for d_Res, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            cleanUp(EXIT_FAILURE);
        }

        printf("Initializing OpenCL DDOT...\n");
            ciErrNum = initDDOT(cxGPUContext, cqCommandQueue, cdDevice, program_file, __nbfpe);

            if (ciErrNum != CL_SUCCESS)
                cleanUp(EXIT_FAILURE);

        printf("Running OpenCL DDOT with %u elements...\n\n", __nbElements);
            //Just a single launch or a warmup iteration
            DDOT(NULL, d_Res, d_a, d_b, __nbElements, &ciErrNum);

            if (ciErrNum != CL_SUCCESS)
                cleanUp(EXIT_FAILURE);

#ifdef GPU_PROFILING
        double gpuTime[NUM_ITER];
        cl_event startMark, endMark;

        for(uint iter = 0; iter < NUM_ITER; iter++) {
            ciErrNum = clEnqueueMarker(cqCommandQueue, &startMark);
            ciErrNum |= clFinish(cqCommandQueue);
            if (ciErrNum != CL_SUCCESS) {
                printf("Error in clEnqueueMarker, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
                cleanUp(EXIT_FAILURE);
            }

            DDOT(NULL, d_Res, d_a, d_b, __nbElements, &ciErrNum);

            ciErrNum  = clEnqueueMarker(cqCommandQueue, &endMark);
            ciErrNum |= clFinish(cqCommandQueue);
            if (ciErrNum != CL_SUCCESS) {
                printf("Error in clEnqueueMarker, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
                cleanUp(EXIT_FAILURE);
            }

            //Get OpenCL profiler time
            cl_ulong startTime = 0, endTime = 0;
            ciErrNum  = clGetEventProfilingInfo(startMark, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &startTime, NULL);
            ciErrNum |= clGetEventProfilingInfo(endMark, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, NULL);
            if (ciErrNum != CL_SUCCESS) {
                printf("Error in clGetEventProfilingInfo Line %u in file %s !!!\n\n", __LINE__, __FILE__);
                cleanUp(EXIT_FAILURE);
            }
            gpuTime[iter] = 1.0e-9 * ((unsigned long)endTime - (unsigned long)startTime); // / (double)NUM_ITER;
        }

        double minTime = min(gpuTime, NUM_ITER);
        double throughput = 2.0 * __nbElements * sizeof(double);
        throughput = (throughput / minTime) * 1e-9;
        double perf = 2.0 * __nbElements;
        perf = (perf / minTime) * 1e-9;
        printf("Alg = %u \t NbFPE = %u \t Range = %u \t NbElements = %u \t Size = %lu \t Time = %.8f s \t Throughput = %.4f GB/s\n\n", __alg, __nbfpe, __range, __nbElements, __nbElements * sizeof(double), minTime, throughput);
        printf("Alg = %u \t NbFPE = %u \t Range = %u \t NbElements = %u \t Size = %lu \t Time = %.8f s \t Performance = %.4f GFLOPS\n\n", __alg, __nbfpe, __range, __nbElements, __nbElements * sizeof(double), minTime, perf);
#endif

        printf("Validating DDOT OpenCL results...\n");
            printf(" ...reading back OpenCL results\n");
                ciErrNum = clEnqueueReadBuffer(cqCommandQueue, d_Res, CL_TRUE, 0, sizeof(cl_double), &h_Res, 0, NULL, NULL);
                if (ciErrNum != CL_SUCCESS) {
                    printf("Error in clEnqueueReadBuffer Line %u in file %s !!!\n\n", __LINE__, __FILE__);
                    cleanUp(EXIT_FAILURE);
                }

            printf(" ...comparing the results\n");
                printf("//--------------------------------------------------------\n");
                mpfr_t *ddot_mpfr = ddotWithMPFR((double *) h_a, (double *) h_b, __nbElements);
                PassFailFlag = CompareWithMPFR(ddot_mpfr, h_Res);

        //Release kernels and program
        printf("Shutting down...\n\n");
            closeDDOT();

    // pass or fail
    if (PassFailFlag)
        printf("[DDOT] test results...\tPASSED\n\n");
    else
        printf("[DDOT] test results...\tFAILED\n\n");

    return cleanUp(EXIT_SUCCESS);
}

int runDDOTStandard(const char* program_file) {
    cl_int ciErrNum;
    bool PassFailFlag = false;

    printf("Initializing data...\n");
        PassFailFlag  = posix_memalign(&h_a, 64, __nbElements * sizeof(double));
        PassFailFlag |= posix_memalign(&h_b, 64, __nbElements * sizeof(double));
        if (PassFailFlag != 0) {
            printf("ERROR: could not allocate memory with posix_memalign!\n");
            exit(1);
        }

        // init data
        int emax = E_BITS - log2(__nbElements);// use log in order to stay within [emin, emax]
        init_fpuniform((double *) h_a, __nbElements, __range, emax);
        init_fpuniform((double *) h_b, __nbElements, __range, emax);

    printf("Initializing OpenCL...\n");
        char platform_name[64];
#ifdef AMD
        strcpy(platform_name, "AMD Accelerated Parallel Processing");
#else
        strcpy(platform_name, "NVIDIA CUDA");
#endif
        //setenv("CUDA_CACHE_DISABLE", "1", 1);
        cpPlatform = GetOCLPlatform(platform_name);
        if (cpPlatform == NULL) {
            printf("ERROR: Failed to find the platform '%s' ...\n", platform_name);
            return -1;
        }

        //Get a GPU device
        cdDevice = GetOCLDevice(cpPlatform);
        if (cdDevice == NULL) {
            printf("Error in clGetDeviceIDs, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            return -1;
        }

        //Create the context
        cxGPUContext = clCreateContext(0, 1, &cdDevice, NULL, NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS) {
            printf("Error in clCreateContext, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            cleanUp(EXIT_FAILURE);
        }

        //Create a command-queue
        cqCommandQueue = clCreateCommandQueue(cxGPUContext, cdDevice, CL_QUEUE_PROFILING_ENABLE, &ciErrNum);
        if (ciErrNum != CL_SUCCESS) {
            printf("Error in clCreateCommandQueue, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            cleanUp(EXIT_FAILURE);
        }

    printf("Allocating OpenCL memory...\n\n");
        size_t size = __nbElements * sizeof(cl_double);
        d_a = clCreateBuffer(cxGPUContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, size, h_a, &ciErrNum);
        if (ciErrNum != CL_SUCCESS) {
            printf("Error in clCreateBuffer for d_a, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            cleanUp(EXIT_FAILURE);
        }
        d_b = clCreateBuffer(cxGPUContext, CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, size, h_b, &ciErrNum);
        if (ciErrNum != CL_SUCCESS) {
            printf("Error in clCreateBuffer for d_b, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            cleanUp(EXIT_FAILURE);
        }
        d_Res = clCreateBuffer(cxGPUContext, CL_MEM_READ_WRITE, sizeof(cl_double), NULL, &ciErrNum);
        if (ciErrNum != CL_SUCCESS) {
            printf("Error in clCreateBuffer for d_Res, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
            cleanUp(EXIT_FAILURE);
        }

        printf("Initializing OpenCL DDOT...\n");
            ciErrNum = initDDOTStandard(cxGPUContext, cqCommandQueue, cdDevice, program_file);

            if (ciErrNum != CL_SUCCESS)
                cleanUp(EXIT_FAILURE);

        printf("Running OpenCL DDOT with %u elements...\n\n", __nbElements);
            //Just a single launch or a warmup iteration
            DDOTStandard(NULL, d_Res, d_a, d_b, __nbElements, &ciErrNum);
            if (ciErrNum != CL_SUCCESS)
                cleanUp(EXIT_FAILURE);

#ifdef GPU_PROFILING
        double gpuTime[NUM_ITER];
        cl_event startMark, endMark;

        for(uint iter = 0; iter < NUM_ITER; iter++) {
            ciErrNum = clEnqueueMarker(cqCommandQueue, &startMark);
            ciErrNum |= clFinish(cqCommandQueue);
            if (ciErrNum != CL_SUCCESS) {
                printf("Error in clEnqueueMarker, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
                cleanUp(EXIT_FAILURE);
            }

            DDOTStandard(NULL, d_Res, d_a, d_b, __nbElements, &ciErrNum);

            ciErrNum  = clEnqueueMarker(cqCommandQueue, &endMark);
            ciErrNum |= clFinish(cqCommandQueue);
            if (ciErrNum != CL_SUCCESS) {
                printf("Error in clEnqueueMarker, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
                cleanUp(EXIT_FAILURE);
            }

            //Get OpenCL profiler time
            cl_ulong startTime = 0, endTime = 0;
            ciErrNum  = clGetEventProfilingInfo(startMark, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &startTime, NULL);
            ciErrNum |= clGetEventProfilingInfo(endMark, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &endTime, NULL);
            if (ciErrNum != CL_SUCCESS) {
                printf("Error in clGetEventProfilingInfo Line %u in file %s !!!\n\n", __LINE__, __FILE__);
                cleanUp(EXIT_FAILURE);
            }
            gpuTime[iter] = 1.0e-9 * ((unsigned long)endTime - (unsigned long)startTime); // / (double)NUM_ITER;
        }

        double minTime = min(gpuTime, NUM_ITER);
        double throughput = 2.0 * __nbElements * sizeof(double);
        throughput = (throughput / minTime) * 1e-9;
        double perf = 2.0 * __nbElements;
        perf = (perf / minTime) * 1e-9;
        printf("Alg = %u \t Range = %u \t NbElements = %u \t Size = %lu \t Time = %.8f s \t Throughput = %.4f GB/s\n\n", __alg, __range, __nbElements, __nbElements * sizeof(double), minTime, throughput);
        printf("Alg = %u \t Range = %u \t NbElements = %u \t Size = %lu \t Time = %.8f s \t Performance = %.4f GFLOPS\n\n", __alg, __range, __nbElements, __nbElements * sizeof(double), minTime, perf);
#endif

        printf("Validating DDOT OpenCL results...\n");
            printf(" ...reading back OpenCL results\n");
                ciErrNum = clEnqueueReadBuffer(cqCommandQueue, d_Res, CL_TRUE, 0, sizeof(double), (void *)&h_Res, 0, NULL, NULL);
                if (ciErrNum != CL_SUCCESS) {
                    printf("Error in clEnqueueReadBuffer Line %u in file %s !!!\n\n", __LINE__, __FILE__);
                    cleanUp(EXIT_FAILURE);
                }

            printf(" ...comparing the results\n");
                printf("//--------------------------------------------------------\n");
                mpfr_t *ddot_mpfr = ddotWithMPFR((double *) h_a, (double *) h_b, __nbElements);
                PassFailFlag = CompareWithMPFR(ddot_mpfr, h_Res);

         //Release kernels and program
         printf("Shutting down...\n\n");
             closeDDOTStandard();

    // pass or fail
    if (PassFailFlag)
        printf("[DDOT] test results...\tPASSED\n\n");
    else
        printf("[DDOT] test results...\tFAILED\n\n");

    return cleanUp(EXIT_SUCCESS);
}

int cleanUp (int exitCode) {
    //Release other OpenCL Objects
    if(d_a)
        clReleaseMemObject(d_a);
    if(d_b)
        clReleaseMemObject(d_b);
    if(d_Res)
        clReleaseMemObject(d_Res);
    if(cqCommandQueue)
        clReleaseCommandQueue(cqCommandQueue);
    if(cxGPUContext)
        clReleaseContext(cxGPUContext);

    //Release host buffers
    free(h_a);
    free(h_b);

    return exitCode;
}
