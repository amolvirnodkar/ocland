/*
 *  This file is part of ocland, a free CFD program based on SPH.
 *  Copyright (C) 2012  Jose Luis Cercos Pita <jl.cercos@upm.es>
 *
 *  ocland is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  ocland is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with ocland.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>

#include <CL/cl.h>
#include <CL/cl_ext.h>

#include <ocland/common/dataExchange.h>
#include <ocland/server/ocland_cl.h>

#ifndef OCLAND_PORT
    #define OCLAND_PORT 51000u
#endif

#ifndef BUFF_SIZE
    #define BUFF_SIZE 1025u
#endif

int ocland_clGetPlatformIDs(int* clientfd, char* buffer, validator v)
{
    cl_uint num_entries = 0, n;
    cl_int flag;
    /* Request number of entries. If number of entries is
     * greather than zero, we assume that platforms array
     * is required, so must be the client who analyze if
     * arguments are right therefore.
     */
    Recv(clientfd, &num_entries, sizeof(cl_uint), MSG_WAITALL);
    // Get OpenCL platforms
    cl_uint num_platforms = 0;
    cl_platform_id *platforms = NULL;
    flag      = clGetPlatformIDs(0, NULL, &num_platforms);
    if(flag  != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &num_platforms, sizeof(cl_uint), 0);
        return 1;
    }
    platforms = (cl_platform_id*) malloc(num_platforms*sizeof(cl_platform_id));
    flag      = clGetPlatformIDs(num_platforms, platforms, NULL);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &num_platforms, sizeof(cl_uint), 0);
    if( (!num_entries) || (flag != CL_SUCCESS) ){
        if(platforms) free(platforms); platforms=NULL;
        return 1;
    }
    // Send paltforms array (number of platforms sent following OpenCL specification)
    n = num_entries; if(n > num_platforms) n=num_platforms;
    /** ocland sent to client the pointers of platforms on server.
     * This usually a wrong way to work on netwroks because
     * pointers transfered redirect to invalid memory address, but
     * since OpenCL users/developers don't know the content of
     * _cl_platform_id, they simply can't access it, so they are
     * forced to use OpenCL specification methods, that will be
     * transfered to server again, where pointers still being valid. \n
     * Same method will used for other types.
     */
    Send(clientfd, platforms, n*sizeof(cl_platform_id), 0);
    if(platforms) free(platforms); platforms=NULL;
    return 1;
}

int ocland_clGetPlatformInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_platform_id platform;
    cl_platform_info param_name;
    size_t param_value_size;
    Recv(clientfd, &platform, sizeof(cl_platform_id), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_platform_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    // Ensure that platform is valid
    flag = isPlatform(v, platform);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    flag = clGetPlatformInfo(platform, param_name, param_value_size, buffer, &param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    if(flag != CL_SUCCESS){
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Set vendor, name, and ICD suffix, ocland prefixes
    char param_value[BUFF_SIZE]; strcpy(param_value, buffer);
    if( (param_name == CL_PLATFORM_NAME) ||
        (param_name == CL_PLATFORM_VENDOR) ||
        (param_name == CL_PLATFORM_ICD_SUFFIX_KHR) ){
        struct sockaddr_in adr_inet;
        socklen_t len_inet;
        len_inet = sizeof(adr_inet);
        getsockname(*clientfd, (struct sockaddr*)&adr_inet, &len_inet);
        strcpy(param_value, "ocland(");
        strcat(param_value, inet_ntoa(adr_inet.sin_addr));
        strcat(param_value, ") ");
        strcat(param_value, buffer);
        param_value_size_ret += (9+strlen(inet_ntoa(adr_inet.sin_addr)))*sizeof(char);
    }
    // Send data
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    Send(clientfd, param_value, param_value_size_ret, 0);
    return 1;
}

int ocland_clGetDeviceIDs(int *clientfd, char* buffer, validator v)
{
    cl_uint num_devices;
    cl_device_id *devices=NULL;
    // Get parameters.
    cl_platform_id platform;
    cl_device_type device_type;
    cl_uint num_entries, n;
    cl_int flag;
    Recv(clientfd, &platform, sizeof(cl_platform_id), MSG_WAITALL);
    Recv(clientfd, &device_type, sizeof(cl_device_type), MSG_WAITALL);
    Recv(clientfd, &num_entries, sizeof(cl_uint), MSG_WAITALL);
    // Ensure that platform is valid
    flag = isPlatform(v, platform);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &num_entries, sizeof(size_t), 0);
        return 1;
    }
    // Get OpenCL devices
    flag     = clGetDeviceIDs (platform, device_type, 0, NULL, &num_devices);
    if(flag  != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &num_devices, sizeof(cl_uint), 0);
        return 1;
    }
    devices  = (cl_device_id*)malloc(num_devices*sizeof(cl_device_id));
    flag     = clGetDeviceIDs(platform, device_type, num_devices, devices, &num_devices);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &num_devices, sizeof(cl_uint), 0);
    if( (!num_entries) || (flag != CL_SUCCESS) ){
        if(devices) free(devices); devices=NULL;
        return 1;
    }
    // Send devices array
    n = num_entries; if(n > num_devices) n=num_devices;
    Send(clientfd, devices, n*sizeof(cl_device_id), 0);
    // Register devices
    registerDevices(v, num_devices, devices);
    if(devices) free(devices); devices=NULL;
    return 1;
}

int ocland_clGetDeviceInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_device_id device;
    cl_device_info param_name;
    size_t param_value_size;
    Recv(clientfd, &device, sizeof(cl_device_id), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_device_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    // Ensure that device is valid
    flag = isDevice(v, device);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    flag = clGetDeviceInfo(device, param_name, param_value_size, buffer, &param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    if(flag != CL_SUCCESS){
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Send data
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    Send(clientfd, buffer, param_value_size_ret, 0);
    return 1;
}

int ocland_clCreateContext(int* clientfd, char* buffer, validator v)
{
    cl_uint i;
    // Get parameters.
    size_t sProps;
    Recv(clientfd, &sProps, sizeof(size_t), MSG_WAITALL);
    cl_context_properties *properties = NULL;
    if(sProps){
        properties = (cl_context_properties*)malloc(sProps);
        Recv(clientfd, properties, sProps, MSG_WAITALL);
    }
    cl_uint num_devices = 0;
    Recv(clientfd, &num_devices, sizeof(cl_uint), MSG_WAITALL);
    cl_device_id devices[num_devices];
    Recv(clientfd, devices, num_devices*sizeof(cl_device_id), MSG_WAITALL);
    /// pfn_notify can't be implmented trought network, so will simply ignored.
    cl_context context=NULL;
    cl_int errcode_ret;
    if(sProps){
        // Ensure that if platform is provided, already exist on server
        if(properties[0] & CL_CONTEXT_PLATFORM){
            errcode_ret = isPlatform(v, (cl_platform_id)properties[1]);
            if(errcode_ret != CL_SUCCESS){
                Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
                Send(clientfd, &context, sizeof(cl_context), 0);
                return 1;
            }
        }
    }
    // Ensure that devices are present on the server
    for(i=0;i<num_devices;i++){
        errcode_ret = isDevice(v, devices[i]);
        if(errcode_ret != CL_SUCCESS){
            Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
            Send(clientfd, &context, sizeof(cl_context), 0);
            return 1;
        }
    }
    context = clCreateContext(properties, num_devices, devices, NULL, NULL, &errcode_ret);
    if(properties) free(properties); properties=NULL;
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &context, sizeof(cl_context), 0);
    if(errcode_ret == CL_SUCCESS){
        struct sockaddr_in adr_inet;
        socklen_t len_inet;
        len_inet = sizeof(adr_inet);
        getsockname(*clientfd, (struct sockaddr*)&adr_inet, &len_inet);
        printf("%s has built a context with %u devices\n", inet_ntoa(adr_inet.sin_addr), num_devices);
        // Register the new context
        registerContext(v,context);
    }
    return 1;
}

int ocland_clCreateContextFromType(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    size_t sProps;
    Recv(clientfd, &sProps, sizeof(size_t), MSG_WAITALL);
    cl_context_properties *properties = NULL;
    if(sProps){
        properties = (cl_context_properties*)malloc(sProps);
        Recv(clientfd, properties, sProps, MSG_WAITALL);
    }
    cl_device_type device_type = 0;
    Recv(clientfd, &device_type, sizeof(cl_device_type), MSG_WAITALL);
    /// pfn_notify can't be implmented trought network, so will simply ignored.
    cl_context context;
    cl_int errcode_ret;
    if(sProps){
        // Ensure that if platform is provided, already exist on server
        if(properties[0] & CL_CONTEXT_PLATFORM){
            errcode_ret = isPlatform(v, (cl_platform_id)properties[1]);
            if(errcode_ret != CL_SUCCESS){
                Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
                Send(clientfd, &context, sizeof(cl_context), 0);
                return 1;
            }
        }
    }
    context = clCreateContextFromType(properties, device_type, NULL, NULL, &errcode_ret);
    if(properties) free(properties); properties=NULL;
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &context, sizeof(cl_context), 0);
    if(errcode_ret == CL_SUCCESS){
        struct sockaddr_in adr_inet;
        socklen_t len_inet;
        len_inet = sizeof(adr_inet);
        getsockname(*clientfd, (struct sockaddr*)&adr_inet, &len_inet);
        printf("%s has built a context\n", inet_ntoa(adr_inet.sin_addr));
        // Register the new context
        registerContext(v,context);
    }
    return 1;
}

int ocland_clRetainContext(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_context context;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    cl_int flag;
    // Ensure that context is valid
    flag = isContext(v, context);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clRetainContext(context);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clReleaseContext(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_context context;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    cl_int flag;
    // Ensure that context is valid
    flag = isContext(v, context);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clReleaseContext(context);
    if(flag == CL_SUCCESS){
        struct sockaddr_in adr_inet;
        socklen_t len_inet;
        len_inet = sizeof(adr_inet);
        getsockname(*clientfd, (struct sockaddr*)&adr_inet, &len_inet);
        printf("%s has released a context\n", inet_ntoa(adr_inet.sin_addr));
        // unregister the context
        unregisterContext(v,context);
    }
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clGetContextInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_context context;
    cl_context_info param_name;
    size_t param_value_size;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_context_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    // Ensure that context is valid
    flag = isContext(v, context);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    flag = clGetContextInfo(context, param_name, param_value_size, buffer, &param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        return 1;
    }
    // Send data
    Send(clientfd, buffer, param_value_size_ret, 0);
    return 1;
}

int ocland_clCreateCommandQueue(int* clientfd, char* buffer, validator v)
{
    cl_int errcode_ret;
    cl_command_queue queue = NULL;
    // Get parameters.
    cl_context context;
    cl_device_id device;
    cl_command_queue_properties properties;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    Recv(clientfd, &device, sizeof(cl_device_id), MSG_WAITALL);
    Recv(clientfd, &properties, sizeof(cl_command_queue_properties), MSG_WAITALL);
    // Ensure that context is valid
    errcode_ret = isContext(v, context);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &queue, sizeof(cl_command_queue), 0);
        return 1;
    }
    // Ensure that device is valid
    errcode_ret = isDevice(v, device);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &queue, sizeof(cl_command_queue), 0);
        return 1;
    }
    queue = clCreateCommandQueue(context, device, properties, &errcode_ret);
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &queue, sizeof(cl_command_queue), 0);
    if(errcode_ret == CL_SUCCESS){
        struct sockaddr_in adr_inet;
        socklen_t len_inet;
        len_inet = sizeof(adr_inet);
        getsockname(*clientfd, (struct sockaddr*)&adr_inet, &len_inet);
        printf("%s has built a command queue\n", inet_ntoa(adr_inet.sin_addr));
        // Register new command queue
        registerQueue(v, queue);
    }
    return 1;
}

int ocland_clRetainCommandQueue(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_command_queue queue;
    Recv(clientfd, &queue, sizeof(cl_command_queue), MSG_WAITALL);
    cl_int flag;
    // Ensure that queue is valid
    flag = isQueue(v, queue);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clRetainCommandQueue(queue);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clReleaseCommandQueue(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_command_queue queue;
    Recv(clientfd, &queue, sizeof(cl_command_queue), MSG_WAITALL);
    cl_int flag;
    // Ensure that queue is valid
    flag = isQueue(v, queue);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clReleaseCommandQueue(queue);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    if(flag == CL_SUCCESS){
        struct sockaddr_in adr_inet;
        socklen_t len_inet;
        len_inet = sizeof(adr_inet);
        getsockname(*clientfd, (struct sockaddr*)&adr_inet, &len_inet);
        printf("%s has released a command queue\n", inet_ntoa(adr_inet.sin_addr));
        // unregister the queue
        unregisterQueue(v,queue);
    }
    return 1;
}

int ocland_clGetCommandQueueInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_command_queue queue;
    cl_command_queue_info param_name;
    size_t param_value_size;
    Recv(clientfd, &queue, sizeof(cl_command_queue), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_command_queue_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    // Ensure that queue is valid
    flag = isQueue(v, queue);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    flag = clGetCommandQueueInfo(queue, param_name, param_value_size, buffer, &param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        return 1;
    }
    // Send data
    Send(clientfd, buffer, param_value_size_ret, 0);
    return 1;
}

int ocland_clCreateBuffer(int* clientfd, char* buffer, validator v)
{
    cl_int errcode_ret;
    cl_mem clMem = NULL;
    // Get parameters.
    cl_context context;
    cl_mem_flags flags;
    size_t size;
    void* host_ptr = NULL;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    Recv(clientfd, &flags, sizeof(cl_mem_flags), MSG_WAITALL);
    Recv(clientfd, &size, sizeof(size_t), MSG_WAITALL);
    if(flags & CL_MEM_COPY_HOST_PTR){
        // Really large data, take care here
        host_ptr = (void*)malloc(size);
        size_t buffsize = BUFF_SIZE*sizeof(char);
        if(!host_ptr){
            buffsize = 0;
            Send(clientfd, &buffsize, sizeof(size_t), 0);
            return 0;
        }
        Send(clientfd, &buffsize, sizeof(size_t), 0);
        // Compute the number of packages needed
        unsigned int i,n;
        n = size / buffsize;
        // Receive package by pieces
        for(i=0;i<n;i++){
            Recv(clientfd, host_ptr + i*buffsize, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, host_ptr + n*buffsize, size % buffsize, MSG_WAITALL);
        }
    }
    // Ensure that context is valid
    errcode_ret = isContext(v, context);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &clMem, sizeof(cl_mem), 0);
        return 1;
    }
    clMem = clCreateBuffer(context, flags, size, host_ptr, &errcode_ret);
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &clMem, sizeof(cl_mem), 0);
    // Register new memory object
    if(errcode_ret == CL_SUCCESS)
        registerBuffer(v, clMem);
    return 1;
}

int ocland_clRetainMemObject(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_mem memobj;
    Recv(clientfd, &memobj, sizeof(cl_mem), MSG_WAITALL);
    cl_int flag;
    // Ensure that queue is valid
    flag = isBuffer(v, memobj);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clRetainMemObject(memobj);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clReleaseMemObject(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_mem memobj;
    Recv(clientfd, &memobj, sizeof(cl_mem), MSG_WAITALL);
    cl_int flag;
    // Ensure that memobj is valid
    flag = isBuffer(v, memobj);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clReleaseMemObject(memobj);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    // unregister the memobj
    unregisterBuffer(v,memobj);
    return 1;
}

int ocland_clGetSupportedImageFormats(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_context context;
    cl_mem_flags flags;
    cl_mem_object_type image_type;
    cl_uint num_entries = 0;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    Recv(clientfd, &flags, sizeof(cl_mem_flags), MSG_WAITALL);
    Recv(clientfd, &image_type, sizeof(cl_mem_object_type), MSG_WAITALL);
    Recv(clientfd, &num_entries, sizeof(cl_uint), MSG_WAITALL);
    cl_int flag;
    cl_uint num_image_formats = 0;
    cl_image_format *image_formats = NULL;
    // Ensure that context is valid
    flag = isContext(v, context);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &num_image_formats, sizeof(cl_uint), 0);
        return 1;
    }
    // Build image formats if requested
    if(num_entries){
        image_formats = (cl_image_format*)malloc(num_entries*sizeof(cl_image_format));
        if(!image_formats){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &num_image_formats, sizeof(cl_uint), 0);
            return 0;
        }
    }
    flag = clGetSupportedImageFormats(context,flags,image_type,num_entries,image_formats,&num_image_formats);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &num_image_formats, sizeof(cl_uint), 0);
    if(flag != CL_SUCCESS){
        if(image_formats) free(image_formats); image_formats=NULL;
        return 1;
    }
    // Send data
    if(num_entries){
        Send(clientfd, image_formats, num_entries*sizeof(cl_image_format), 0);
        free(image_formats); image_formats = NULL;
    }
    return 1;
}

int ocland_clGetMemObjectInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_mem memobj;
    cl_mem_info param_name;
    size_t param_value_size;
    cl_uint num_entries = 0;
    Recv(clientfd, &memobj, sizeof(cl_mem), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_mem_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    void* param_value = NULL;
    // Ensure that memobj is valid
    flag = isBuffer(v, memobj);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Build image formats if requested
    if(num_entries){
        param_value = (void*)malloc(param_value_size);
        if(!param_value){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
            return 0;
        }
    }
    flag = clGetMemObjectInfo(memobj,param_name,param_value_size,param_value,&param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        if(param_value) free(param_value); param_value=NULL;
        return 1;
    }
    // Send data
    if(num_entries){
        Send(clientfd, param_value, param_value_size, 0);
        free(param_value); param_value = NULL;
    }
    return 1;
}

int ocland_clGetImageInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_mem memobj;
    cl_mem_info param_name;
    size_t param_value_size;
    cl_uint num_entries = 0;
    Recv(clientfd, &memobj, sizeof(cl_mem), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_mem_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    void* param_value = NULL;
    // Ensure that memobj is valid
    flag = isBuffer(v, memobj);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Build image formats if requested
    if(num_entries){
        param_value = (void*)malloc(param_value_size);
        if(!param_value){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
            return 0;
        }
    }
    flag = clGetImageInfo(memobj,param_name,param_value_size,param_value,&param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        if(param_value) free(param_value); param_value=NULL;
        return 1;
    }
    // Send data
    if(num_entries){
        Send(clientfd, param_value, param_value_size, 0);
        free(param_value); param_value = NULL;
    }
    return 1;
}

int ocland_clCreateSampler(int* clientfd, char* buffer, validator v)
{
    cl_int errcode_ret;
    cl_sampler sampler = NULL;
    // Get parameters.
    cl_context context;
    cl_bool normalized_coords;
    cl_addressing_mode addressing_mode;
    cl_filter_mode filter_mode;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    Recv(clientfd, &normalized_coords, sizeof(cl_bool), MSG_WAITALL);
    Recv(clientfd, &addressing_mode, sizeof(cl_addressing_mode), MSG_WAITALL);
    Recv(clientfd, &filter_mode, sizeof(cl_filter_mode), MSG_WAITALL);
    // Ensure that context is valid
    errcode_ret = isContext(v, context);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &sampler, sizeof(cl_sampler), 0);
        return 1;
    }
    sampler = clCreateSampler(context,normalized_coords,addressing_mode,filter_mode,&errcode_ret);
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &sampler, sizeof(cl_sampler), 0);
    if(errcode_ret == CL_SUCCESS){
        // Register new command queue
        registerSampler(v, sampler);
    }
    return 1;
}

int ocland_clRetainSampler(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_sampler sampler;
    Recv(clientfd, &sampler, sizeof(cl_sampler), MSG_WAITALL);
    cl_int flag;
    // Ensure that pointer is valid
    flag = isSampler(v, sampler);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clRetainSampler(sampler);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clReleaseSampler(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_sampler sampler;
    Recv(clientfd, &sampler, sizeof(cl_sampler), MSG_WAITALL);
    cl_int flag;
    // Ensure that pointer is valid
    flag = isSampler(v, sampler);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clReleaseSampler(sampler);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    // unregister the sampler
    unregisterSampler(v,sampler);
    return 1;
}

int ocland_clGetSamplerInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_sampler sampler;
    cl_sampler_info param_name;
    size_t param_value_size;
    cl_uint num_entries = 0;
    Recv(clientfd, &sampler, sizeof(cl_sampler), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_sampler_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    void* param_value = NULL;
    // Ensure that pointer is valid
    flag = isSampler(v, sampler);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Build image formats if requested
    if(num_entries){
        param_value = (void*)malloc(param_value_size);
        if(!param_value){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
            return 0;
        }
    }
    flag = clGetSamplerInfo(sampler,param_name,param_value_size,param_value,&param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        if(param_value) free(param_value); param_value=NULL;
        return 1;
    }
    // Send data
    if(num_entries){
        Send(clientfd, param_value, param_value_size, 0);
        free(param_value); param_value = NULL;
    }
    return 1;
}

int ocland_clCreateProgramWithSource(int* clientfd, char* buffer, validator v)
{
    unsigned int i;
    cl_int errcode_ret = CL_SUCCESS;
    cl_program program = NULL;
    // Get parameters.
    cl_context context;
    cl_uint count;
    char **strings = NULL;
    size_t *lengths = NULL;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    Recv(clientfd, &count, sizeof(cl_uint), MSG_WAITALL);
    size_t buffsize = BUFF_SIZE*sizeof(char);
    Send(clientfd, &buffsize, sizeof(size_t), 0);
    if(!count){
        errcode_ret = CL_INVALID_VALUE;
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &program, sizeof(cl_program), 0);
        return 1;
    }
    strings = (char**)malloc(count*sizeof(char*));
    lengths = (size_t*)malloc(count*sizeof(size_t));
    if( !strings || !lengths ){
        // We cannot stop the execution because client
        // is expecting send data
        errcode_ret = CL_OUT_OF_RESOURCES;
    }
    for(i=0;i<count;i++){
        size_t size;
        Recv(clientfd, &size, sizeof(size_t), MSG_WAITALL);
        if(lengths)
            lengths[i] = size;
        // Compute the number of packages needed
        unsigned int j,n;
        n = size / buffsize;
        // Allocate memory
        if(strings){
            strings[i] = (char*)malloc(size);
            if(!strings[i])
                errcode_ret = CL_OUT_OF_RESOURCES;
        }
        // Receive package by pieces
        if(errcode_ret != CL_SUCCESS){
            char dummy[buffsize];
            // Receive package by pieces
            for(j=0;j<n;j++){
                Recv(clientfd, dummy, buffsize, MSG_WAITALL);
            }
            if(size % buffsize){
                // Remains some data to arrive
                Recv(clientfd, dummy, size % buffsize, MSG_WAITALL);
            }
            continue;
        }
        for(j=0;j<n;j++){
            Recv(clientfd, strings[i] + j*buffsize, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, strings[i] + n*buffsize, size % buffsize, MSG_WAITALL);
        }
    }
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &program, sizeof(cl_program), 0);
        for(i=0;i<count;i++){
            if(strings){
                if(strings[i]) free(strings[i]); strings[i]=NULL;
            }
        }
        if(strings) free(strings); strings=NULL;
        if(lengths) free(lengths); lengths=NULL;
        return 1;
    }
    // Ensure that context is valid
    errcode_ret = isContext(v, context);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &program, sizeof(cl_program), 0);
        for(i=0;i<count;i++){
            if(strings){
                if(strings[i]) free(strings[i]); strings[i]=NULL;
            }
        }
        if(strings) free(strings); strings=NULL;
        if(lengths) free(lengths); lengths=NULL;
        return 1;
    }
    program = clCreateProgramWithSource(context,count,(const char**)strings,lengths,&errcode_ret);
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &program, sizeof(cl_program), 0);
    for(i=0;i<count;i++){
        if(strings){
            if(strings[i]) free(strings[i]); strings[i]=NULL;
        }
    }
    if(strings) free(strings); strings=NULL;
    if(lengths) free(lengths); lengths=NULL;
    // Register new memory object
    registerProgram(v, program);
    return 1;
}

int ocland_clCreateProgramWithBinary(int* clientfd, char* buffer, validator v)
{
    unsigned int i;
    cl_int errcode_ret = CL_SUCCESS;
    cl_program program = NULL;
    // Get parameters.
    cl_context context;
    cl_uint num_devices;
    cl_device_id *device_list = NULL;
    size_t *lengths = NULL;
    unsigned char **binaries = NULL;
    cl_int *binary_status = NULL;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    Recv(clientfd, &num_devices, sizeof(cl_uint), MSG_WAITALL);
    size_t buffsize = BUFF_SIZE*sizeof(char);
    Send(clientfd, &buffsize, sizeof(size_t), 0);
    if(!num_devices){
        errcode_ret = CL_INVALID_VALUE;
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &program, sizeof(cl_program), 0);
        Send(clientfd, binary_status, 0, 0);
        return 1;
    }
    device_list = (cl_device_id*)malloc(num_devices*sizeof(cl_device_id));
    lengths = (size_t*)malloc(num_devices*sizeof(size_t));
    binaries = (unsigned char**)malloc(num_devices*sizeof(unsigned char*));
    binary_status = (cl_int*)malloc(num_devices*sizeof(cl_int));
    if( !binaries || !lengths || !device_list || !binary_status ){
        // We cannot stop the execution because client
        // is expecting send data
        errcode_ret = CL_OUT_OF_RESOURCES;
    }
    for(i=0;i<num_devices;i++){
        // Device
        cl_device_id device=NULL;
        Recv(clientfd, &device, sizeof(cl_device_id), MSG_WAITALL);
        if(device_list)
            device_list[i] = device;
        // Ensure that device is valid
        cl_int status = isDevice(v, device);
        if(binary_status) binary_status[i] = CL_SUCCESS;
        if(status != CL_SUCCESS){
            if(binary_status) binary_status[i] = status;
            if(errcode_ret == CL_SUCCESS) errcode_ret = status;
        }
        // Binary length
        size_t size;
        Recv(clientfd, &size, sizeof(size_t), MSG_WAITALL);
        if(lengths)
            lengths[i] = size;
        // Compute the number of packages needed
        unsigned int j,n;
        n = size / buffsize;
        // Allocate memory
        if(binaries){
            binaries[i] = (unsigned char*)malloc(size);
            if(!binaries[i])
                errcode_ret = CL_OUT_OF_RESOURCES;
        }
        // Receive package by pieces
        if(errcode_ret != CL_SUCCESS){
            char dummy[buffsize];
            // Receive package by pieces
            for(j=0;j<n;j++){
                Recv(clientfd, dummy, buffsize, MSG_WAITALL);
            }
            if(size % buffsize){
                // Remains some data to arrive
                Recv(clientfd, dummy, size % buffsize, MSG_WAITALL);
            }
            continue;
        }
        for(j=0;j<n;j++){
            Recv(clientfd, binaries[i] + j*buffsize, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, binaries[i] + n*buffsize, size % buffsize, MSG_WAITALL);
        }
    }
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &program, sizeof(cl_program), 0);
        if(binary_status)
            Send(clientfd, binary_status, num_devices*sizeof(cl_int), 0);
        else
            Send(clientfd, binary_status, 0, 0);
        for(i=0;i<num_devices;i++){
            if(binaries){
                if(binaries[i]) free(binaries[i]); binaries[i]=NULL;
            }
        }
        if(device_list) free(device_list); device_list=NULL;
        if(lengths) free(lengths); lengths=NULL;
        if(binaries) free(binaries); binaries=NULL;
        if(binary_status) free(binary_status); binary_status=NULL;
        return 1;
    }
    // Ensure that context is valid
    errcode_ret = isContext(v, context);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &program, sizeof(cl_program), 0);
        Send(clientfd, binary_status, num_devices*sizeof(cl_int), 0);
        for(i=0;i<num_devices;i++){
            if(binaries){
                if(binaries[i]) free(binaries[i]); binaries[i]=NULL;
            }
        }
        if(device_list) free(device_list); device_list=NULL;
        if(lengths) free(lengths); lengths=NULL;
        if(binaries) free(binaries); binaries=NULL;
        if(binary_status) free(binary_status); binary_status=NULL;
        return 1;
    }
    program = clCreateProgramWithBinary(context,num_devices,device_list,lengths,(const unsigned char**)binaries,binary_status,&errcode_ret);
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &program, sizeof(cl_program), 0);
    Send(clientfd, binary_status, num_devices*sizeof(cl_int), 0);
    for(i=0;i<num_devices;i++){
        if(binaries){
            if(binaries[i]) free(binaries[i]); binaries[i]=NULL;
        }
    }
    if(device_list) free(device_list); device_list=NULL;
    if(lengths) free(lengths); lengths=NULL;
    if(binaries) free(binaries); binaries=NULL;
    if(binary_status) free(binary_status); binary_status=NULL;
    // Register new memory object
    registerProgram(v, program);
    return 1;
}

int ocland_clRetainProgram(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_program program;
    Recv(clientfd, &program, sizeof(cl_program), MSG_WAITALL);
    cl_int flag;
    // Ensure that program is valid
    flag = isProgram(v, program);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clRetainProgram(program);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clReleaseProgram(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_program program;
    Recv(clientfd, &program, sizeof(cl_program), MSG_WAITALL);
    cl_int flag;
    // Ensure that program is valid
    flag = isProgram(v, program);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clReleaseProgram(program);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    if(flag == CL_SUCCESS){
        // unregister the program
        unregisterProgram(v,program);
    }
    return 1;
}

int ocland_clBuildProgram(int* clientfd, char* buffer, validator v)
{
    unsigned int i,n;
    cl_int flag = CL_SUCCESS;
    // Get parameters.
    cl_program program;
    cl_uint num_devices;
    cl_device_id *device_list = NULL;
    size_t size;
    Recv(clientfd, &program, sizeof(cl_program), MSG_WAITALL);
    Recv(clientfd, &num_devices, sizeof(cl_uint), MSG_WAITALL);
    if(num_devices){
        device_list = (cl_device_id*)malloc(num_devices*sizeof(cl_device_id));
        if(!device_list){
            // We cannot stop the execution because client
            // is expecting send data
            flag = CL_OUT_OF_RESOURCES;
            cl_device_id dummy[num_devices];
            Recv(clientfd, dummy, num_devices*sizeof(cl_device_id), MSG_WAITALL);
        }
        else{
            Recv(clientfd, device_list, num_devices*sizeof(cl_device_id), MSG_WAITALL);
        }
    }
    Recv(clientfd, &size, sizeof(size_t), MSG_WAITALL);
    char* options = (char*)malloc(size);
    if( !options ){
        // We cannot stop the execution because client
        // is expecting send data
        flag = CL_OUT_OF_RESOURCES;
    }
    size_t buffsize = BUFF_SIZE*sizeof(char);
    Send(clientfd, &buffsize, sizeof(size_t), 0);
    // Compute the number of packages needed
    n = size / buffsize;
    // Receive package by pieces
    if(flag != CL_SUCCESS){
        char dummy[buffsize];
        // Receive package by pieces
        for(i=0;i<n;i++){
            Recv(clientfd, dummy, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, dummy, size % buffsize, MSG_WAITALL);
        }
    }
    else{
        for(i=0;i<n;i++){
            Recv(clientfd, options + i*buffsize, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, options + n*buffsize, size % buffsize, MSG_WAITALL);
        }
    }
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        if(device_list) free(device_list); device_list=NULL;
        if(options) free(options); options=NULL;
        return 0;
    }
    // Ensure that program is valid
    flag = isProgram(v, program);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        if(device_list) free(device_list); device_list=NULL;
        if(options) free(options); options=NULL;
        return 1;
    }
    // Ensure that devices are valid
    for(i=0;i<num_devices;i++){
        flag = isDevice(v, device_list[i]);
        if(flag != CL_SUCCESS){
            Send(clientfd, &flag, sizeof(cl_int), 0);
            if(device_list) free(device_list); device_list=NULL;
            if(options) free(options); options=NULL;
            return 1;
        }
    }
    flag = clBuildProgram(program,num_devices,device_list,options,NULL,NULL);
    // Write output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    if(device_list) free(device_list); device_list=NULL;
    if(options) free(options); options=NULL;
    return 1;
}

int ocland_clGetProgramInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_program program;
    cl_program_info param_name;
    size_t param_value_size;
    Recv(clientfd, &program, sizeof(cl_program), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_program_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    void* param_value = NULL;
    // Ensure that pointer is valid
    flag = isProgram(v, program);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Build image formats if requested
    if(param_value_size){
        param_value = (void*)malloc(param_value_size);
        if(!param_value){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
            return 0;
        }
    }
    flag = clGetProgramInfo(program,param_name,param_value_size,param_value,&param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        if(param_value) free(param_value); param_value=NULL;
        return 1;
    }
    // Send data
    if(param_value_size){
        Send(clientfd, param_value, param_value_size, 0);
        free(param_value); param_value = NULL;
    }
    return 1;
}

int ocland_clGetProgramBuildInfo(int* clientfd, char* buffer, validator v)
{
    unsigned int i,n;
    // Get parameters.
    cl_program program;
    cl_device_id device;
    cl_program_build_info param_name;
    size_t param_value_size=0;
    Recv(clientfd, &program, sizeof(cl_program), MSG_WAITALL);
    Recv(clientfd, &device, sizeof(cl_device_id), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_program_build_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    void* param_value = NULL;
    // Ensure that pointer is valid
    flag = isProgram(v, program);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Allocate if requested
    if(param_value_size){
        param_value = (void*)malloc(param_value_size);
        if(!param_value){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
            return 0;
        }
    }
    flag = clGetProgramBuildInfo(program,device,param_name,param_value_size,param_value,&param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        if(param_value) free(param_value); param_value=NULL;
        return 1;
    }
    // Send data
    if(param_value_size){
        size_t buffsize = BUFF_SIZE*sizeof(char);
        Send(clientfd, &buffsize, sizeof(size_t), 0);
        // Compute the number of packages needed
        n = param_value_size / buffsize;
        // Send package by pieces
        for(i=0;i<n;i++){
            Send(clientfd, param_value + i*buffsize, buffsize, 0);
        }
        if(param_value_size % buffsize){
            // Remains some data to arrive
            Send(clientfd, param_value + n*buffsize, param_value_size % buffsize, 0);
        }
        free(param_value); param_value = NULL;
    }
    return 1;
}

int ocland_clCreateKernel(int* clientfd, char* buffer, validator v)
{
    cl_int errcode_ret;
    cl_kernel kernel = NULL;
    // Get parameters.
    cl_program program;
    size_t size;
    Recv(clientfd, &program, sizeof(cl_program), MSG_WAITALL);
    Recv(clientfd, &size, sizeof(size_t), MSG_WAITALL);
    char kernel_name[size];
    Recv(clientfd, kernel_name, size, MSG_WAITALL);
    // Ensure that program is valid
    errcode_ret = isProgram(v, program);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &kernel, sizeof(cl_kernel), 0);
        return 1;
    }
    kernel = clCreateKernel(program, kernel_name, &errcode_ret);
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &kernel, sizeof(cl_kernel), 0);
    if(errcode_ret == CL_SUCCESS){
        // Register the new kernel
        registerKernel(v, kernel);
    }
    return 1;
}

int ocland_clCreateKernelsInProgram(int* clientfd, char* buffer, validator v)
{
    unsigned int i;
    cl_int flag = CL_SUCCESS;
    cl_kernel *kernels = NULL;
    cl_uint num_kernels_ret = 0;
    // Get parameters.
    cl_program program;
    cl_uint num_kernels;
    Recv(clientfd, &program, sizeof(cl_program), MSG_WAITALL);
    Recv(clientfd, &num_kernels, sizeof(cl_uint), MSG_WAITALL);
    if(num_kernels){
        kernels = (cl_kernel*)malloc(num_kernels*sizeof(cl_kernel));
        if(!kernels){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &num_kernels_ret, sizeof(cl_uint), 0);
            return 0;
        }
    }
    // Ensure that program is valid
    flag = isProgram(v, program);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &num_kernels_ret, sizeof(cl_uint), 0);
        return 1;
    }
    flag = clCreateKernelsInProgram(program, num_kernels, kernels, &num_kernels_ret);
    // Write output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &num_kernels_ret, sizeof(cl_uint), 0);
    if(num_kernels && (flag == CL_SUCCESS)){
        Send(clientfd, kernels, num_kernels*sizeof(cl_kernel), 0);
        // Register the new kernels
        for(i=0;i<num_kernels_ret;i++)
            registerKernel(v, kernels[i]);
        free(kernels); kernels = NULL;
    }
    return 1;
}

int ocland_clRetainKernel(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_kernel kernel;
    Recv(clientfd, &kernel, sizeof(cl_kernel), MSG_WAITALL);
    cl_int flag;
    // Ensure that pointer is valid
    flag = isKernel(v, kernel);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clRetainKernel(kernel);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clReleaseKernel(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_kernel kernel;
    Recv(clientfd, &kernel, sizeof(cl_kernel), MSG_WAITALL);
    cl_int flag;
    // Ensure that pointer is valid
    flag = isKernel(v, kernel);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clReleaseKernel(kernel);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    // unregister the pointer
    unregisterKernel(v,kernel);
    return 1;
}

int ocland_clSetKernelArg(int* clientfd, char* buffer, validator v)
{
    cl_int flag;
    // Get parameters.
    cl_kernel kernel;
    cl_uint arg_index;
    size_t arg_size;
    void *arg_value;
    Recv(clientfd, &kernel, sizeof(cl_kernel), MSG_WAITALL);
    Recv(clientfd, &arg_index, sizeof(cl_uint), MSG_WAITALL);
    Recv(clientfd, &arg_size, sizeof(size_t), MSG_WAITALL);
    arg_value = (void*)malloc(arg_size);
    Recv(clientfd, arg_value, arg_size, MSG_WAITALL);
    // Ensure that pointer is valid
    flag = isKernel(v, kernel);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clSetKernelArg(kernel,arg_index,arg_size,arg_value);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    if(arg_value) free(arg_value); arg_value = NULL;
    return 1;
}

int ocland_clSetKernelNullArg(int* clientfd, char* buffer, validator v)
{
    cl_int flag;
    // Get parameters.
    cl_kernel kernel;
    cl_uint arg_index;
    size_t arg_size;
    void *arg_value = NULL;
    Recv(clientfd, &kernel, sizeof(cl_kernel), MSG_WAITALL);
    Recv(clientfd, &arg_index, sizeof(cl_uint), MSG_WAITALL);
    Recv(clientfd, &arg_size, sizeof(size_t), MSG_WAITALL);
    // Ensure that pointer is valid
    flag = isKernel(v, kernel);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clSetKernelArg(kernel,arg_index,arg_size,arg_value);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clGetKernelInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_kernel kernel;
    cl_kernel_info param_name;
    size_t param_value_size;
    Recv(clientfd, &kernel, sizeof(cl_kernel), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_kernel_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    void* param_value = NULL;
    // Ensure that pointer is valid
    flag = isKernel(v, kernel);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Build image formats if requested
    if(param_value_size){
        param_value = (void*)malloc(param_value_size);
        if(!param_value){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
            return 0;
        }
    }
    flag = clGetKernelInfo(kernel,param_name,param_value_size,param_value,&param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        if(param_value) free(param_value); param_value=NULL;
        return 1;
    }
    // Send data
    if(param_value_size){
        Send(clientfd, param_value, param_value_size, 0);
        free(param_value); param_value = NULL;
    }
    return 1;
}

int ocland_clGetKernelWorkGroupInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_kernel kernel;
    cl_device_id device;
    cl_kernel_info param_name;
    size_t param_value_size;
    Recv(clientfd, &kernel, sizeof(cl_kernel), MSG_WAITALL);
    Recv(clientfd, &device, sizeof(cl_device_id), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_kernel_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    void* param_value = NULL;
    // Ensure that pointers are valid
    flag = isKernel(v, kernel);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    flag = isDevice(v, device);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Build image formats if requested
    if(param_value_size){
        param_value = (void*)malloc(param_value_size);
        if(!param_value){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
            return 0;
        }
    }
    flag = clGetKernelWorkGroupInfo(kernel,device,param_name,param_value_size,param_value,&param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        if(param_value) free(param_value); param_value=NULL;
        return 1;
    }
    // Send data
    if(param_value_size){
        Send(clientfd, param_value, param_value_size, 0);
        free(param_value); param_value = NULL;
    }
    return 1;
}

int ocland_clWaitForEvents(int* clientfd, char* buffer, validator v)
{
    unsigned int i;
    // Get parameters.
    cl_uint num_events=0;
    Recv(clientfd, &num_events, sizeof(cl_uint), MSG_WAITALL);
    ocland_event event_list[num_events];
    Recv(clientfd, event_list, num_events*sizeof(ocland_event), MSG_WAITALL);
    cl_int flag;
    // Ensure that pointers are valid
    for(i=0;i<num_events;i++){
        flag = isEvent(v, event_list[i]);
        if(flag != CL_SUCCESS){
            Send(clientfd, &flag, sizeof(cl_int), 0);
            return 1;
        }
    }
    flag = oclandWaitForEvents(num_events, event_list);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clGetEventInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    ocland_event event;
    cl_event_info param_name;
    size_t param_value_size;
    Recv(clientfd, &event, sizeof(ocland_event), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_event_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    void* param_value = NULL;
    // Ensure that pointers are valid
    flag = isEvent(v, event);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Build returned value if requested
    if(param_value_size){
        param_value = (void*)malloc(param_value_size);
        if(!param_value){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
            return 0;
        }
    }
    flag = clGetEventInfo(event->event,param_name,param_value_size,param_value,&param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        if(param_value) free(param_value); param_value=NULL;
        return 1;
    }
    // Send data
    if(param_value_size){
        Send(clientfd, param_value, param_value_size, 0);
        free(param_value); param_value = NULL;
    }
    return 1;
}

int ocland_clRetainEvent(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    ocland_event event;
    Recv(clientfd, &event, sizeof(ocland_event), MSG_WAITALL);
    cl_int flag;
    // Ensure that pointer is valid
    flag = isEvent(v, event);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clRetainEvent(event->event);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clReleaseEvent(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    ocland_event event;
    Recv(clientfd, &event, sizeof(ocland_event), MSG_WAITALL);
    cl_int flag;
    // Ensure that pointer is valid
    flag = isEvent(v, event);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clReleaseEvent(event->event);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    // unregister the event and destroy it
    unregisterEvent(v,event);
    free(event);
    return 1;
}

int ocland_clGetEventProfilingInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    ocland_event event;
    cl_event_info param_name;
    size_t param_value_size;
    Recv(clientfd, &event, sizeof(ocland_event), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_event_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    void* param_value = NULL;
    // Ensure that pointers are valid
    flag = isEvent(v, event);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Build returned value if requested
    if(param_value_size){
        param_value = (void*)malloc(param_value_size);
        if(!param_value){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
            return 0;
        }
    }
    flag = clGetEventProfilingInfo(event->event,param_name,param_value_size,param_value,&param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        if(param_value) free(param_value); param_value=NULL;
        return 1;
    }
    // Send data
    if(param_value_size){
        Send(clientfd, param_value, param_value_size, 0);
        free(param_value); param_value = NULL;
    }
    return 1;
}

int ocland_clFlush(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_command_queue command_queue;
    Recv(clientfd, &command_queue, sizeof(cl_command_queue), MSG_WAITALL);
    cl_int flag;
    // Ensure that pointer is valid
    flag = isQueue(v, command_queue);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clFlush(command_queue);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clFinish(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_command_queue command_queue;
    Recv(clientfd, &command_queue, sizeof(cl_command_queue), MSG_WAITALL);
    cl_int flag;
    // Ensure that pointer is valid
    flag = isQueue(v, command_queue);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clFinish(command_queue);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

/** @struct dataTransfer Vars needed for
 * and asynchronously data transfer.
 */
struct dataTransfer{
    /// Socket
    int fd;
    /// Size of data
    size_t cb;
    /// Data array
    void *ptr;
    /// Event associated to the transmission (can be NULL)
    ocland_event event;
};

/** Thread that sends data from server to client.
 * @param data struct dataTransfer casted variable.
 * @return NULL
 */
void *asyncDataSend_thread(void *data)
{
    unsigned int i,n;
    struct dataTransfer* _data = (struct dataTransfer*)data;
    size_t buffsize = BUFF_SIZE*sizeof(char);
    int *fd = &(_data->fd);
    Send(fd, &buffsize, sizeof(size_t), 0);
    // Compute the number of packages needed
    n = _data->cb / buffsize;
    // Send package by pieces
    for(i=0;i<n;i++){
        Send(fd, _data->ptr + i*buffsize, buffsize, 0);
    }
    if(_data->cb % buffsize){
        // Remains some data to arrive
        Send(fd, _data->ptr + n*buffsize, _data->cb % buffsize, 0);
    }
    free(_data->ptr); _data->ptr = NULL;
    if(_data->event){
        _data->event->status = CL_COMPLETE;
    }
    pthread_exit(NULL);
    return NULL;
}

/** Performs a data send asynchronously on a new thread and socket.
 * @param clientfd Client connection socket.
 * @param buffer Buffer to exchange data.
 * @param v Validator.
 * @param data Data to transfer.
 */
void asyncDataSend(int* clientfd, char* buffer, validator v, struct dataTransfer data)
{
    // -------------------------------------
    // Build server on first available port.
    // -------------------------------------
    unsigned int port = OCLAND_PORT+1;
    int serverfd = -1;
    struct sockaddr_in serv_addr;
    memset(&serv_addr, '0', sizeof(serv_addr));
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if(serverfd < 0){
        // we can't work, disconnect the client
        printf("ERROR: New socket can't be registered for asynchronous data transfer.\n"); fflush(stdout);
        shutdown(*clientfd, 2);
        *clientfd = -1;
        return;
    }
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port        = htons(port);
    while(bind(serverfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr))){
        port++;
        serv_addr.sin_port    = htons(port);
    }
    if(listen(serverfd, 1)){
        // we can't work, disconnect the client
        printf("ERROR: Can't listen on port %u binded.\n", port); fflush(stdout);
        shutdown(serverfd, 2);
        shutdown(*clientfd, 2);
        *clientfd = -1;
        return;
    }
    // -------------------------------------
    // Report open port to client, and wait
    // for connection.
    // -------------------------------------
    Send(clientfd, &port, sizeof(unsigned int), 0);
    int fd = accept(serverfd, (struct sockaddr*)NULL, NULL);
    if(fd < 0){
        // we can't work, disconnect the client
        printf("ERROR: Can't listen on port %u binded.\n", port); fflush(stdout);
        shutdown(serverfd, 2);
        shutdown(*clientfd, 2);
        *clientfd = -1;
        return;
    }
    // -------------------------------------
    // Send the data on another thread.
    // -------------------------------------
    pthread_t thread;
    int rc = pthread_create(&thread, NULL, asyncDataSend_thread, (void *)(&data));
    if(rc){
        // we can't work, disconnect the client
        printf("ERROR: Thread creation has failed with the return code %d\n", rc); fflush(stdout);
        shutdown(serverfd, 2);
        shutdown(*clientfd, 2);
        *clientfd = -1;
        return;
    }
}

int ocland_clEnqueueReadBuffer(int* clientfd, char* buffer, validator v)
{
    unsigned int i;
    cl_int flag;
    // Get parameters.
    cl_command_queue command_queue;
    cl_mem mem;
    cl_bool blocking_read;
    size_t offset;
    size_t cb;
    void *ptr = NULL;
    cl_uint num_events_in_wait_list;
    cl_bool want_event;
    ocland_event event = NULL;
    ocland_event *event_wait_list = NULL;
    cl_event *cl_event_wait_list = NULL;
    Recv(clientfd, &command_queue, sizeof(cl_command_queue), MSG_WAITALL);
    Recv(clientfd, &mem, sizeof(cl_mem), MSG_WAITALL);
    Recv(clientfd, &blocking_read, sizeof(cl_bool), MSG_WAITALL);
    Recv(clientfd, &offset, sizeof(size_t), MSG_WAITALL);
    Recv(clientfd, &cb, sizeof(size_t), MSG_WAITALL);
    Recv(clientfd, &num_events_in_wait_list, sizeof(cl_uint), MSG_WAITALL);
    if(num_events_in_wait_list){
        event_wait_list = (ocland_event*)malloc(num_events_in_wait_list*sizeof(ocland_event));
        cl_event_wait_list = (cl_event*)malloc(num_events_in_wait_list*sizeof(cl_event));
        Recv(clientfd, &event_wait_list, num_events_in_wait_list*sizeof(ocland_event), MSG_WAITALL);
    }
    Recv(clientfd, &want_event, sizeof(cl_bool), MSG_WAITALL);
    // Ensure that objects are valid
    flag = isQueue(v, command_queue);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        if(event_wait_list) free(event_wait_list); event_wait_list=NULL;
        if(cl_event_wait_list) free(cl_event_wait_list); cl_event_wait_list=NULL;
        return 1;
    }
    flag = isBuffer(v, mem);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        if(event_wait_list) free(event_wait_list); event_wait_list=NULL;
        if(cl_event_wait_list) free(cl_event_wait_list); cl_event_wait_list=NULL;
        return 1;
    }
    for(i=0;i<num_events_in_wait_list;i++){
        flag = isEvent(v, event_wait_list[i]);
        if(flag != CL_SUCCESS){
            flag = CL_INVALID_EVENT_WAIT_LIST;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            if(event_wait_list) free(event_wait_list); event_wait_list=NULL;
            if(cl_event_wait_list) free(cl_event_wait_list); cl_event_wait_list=NULL;
            return 1;
        }
        cl_event_wait_list[i] = event_wait_list[i]->event;
    }
    // Try to allocate memory for objects
    ptr   = malloc(cb);
    event = (ocland_event)malloc(sizeof(struct _ocland_event));
    if( (!ptr) || (!event) ){
        flag = CL_MEM_OBJECT_ALLOCATION_FAILURE;
        Send(clientfd, &flag, sizeof(cl_int), 0);
        if(event_wait_list) free(event_wait_list); event_wait_list=NULL;
        if(cl_event_wait_list) free(cl_event_wait_list); cl_event_wait_list=NULL;
        return 1;
    }
    // Set the event as uncompleted
    event->event  = NULL;
    event->status = 1;
    // Call to OpenCL request
    // flag = clEnqueueReadBuffer(command_queue,mem,blocking_read,offset,cb,ptr,num_events_in_wait_list,cl_event_wait_list,&(event->event));
    // Return the flag, and the event if request
    Send(clientfd, &flag, sizeof(cl_int), 0);
    if(flag != CL_SUCCESS){
        if(event_wait_list) free(event_wait_list); event_wait_list=NULL;
        if(cl_event_wait_list) free(cl_event_wait_list); cl_event_wait_list=NULL;
        return 1;
    }
    if(want_event == CL_TRUE){
        Send(clientfd, &event, sizeof(ocland_event), 0);
        registerEvent(v, event);
    }
    else{
        free(event); event = NULL;
    }
    // In case of blocking simply send the data
    if(blocking_read == CL_TRUE){
        size_t buffsize = BUFF_SIZE*sizeof(char);
        Send(clientfd, &buffsize, sizeof(size_t), 0);
        // Compute the number of packages needed
        unsigned int n = cb / buffsize;
        // Send package by pieces
        for(i=0;i<n;i++){
            Send(clientfd, ptr + i*buffsize, buffsize, 0);
        }
        if(cb % buffsize){
            // Remains some data to arrive
            Send(clientfd, ptr + n*buffsize, cb % buffsize, 0);
        }
        free(ptr); ptr = NULL;
        if(event_wait_list) free(event_wait_list); event_wait_list=NULL;
        if(cl_event_wait_list) free(cl_event_wait_list); cl_event_wait_list=NULL;
        return 1;
    }
    // In the non blocking case more complex operations are requested
    struct dataTransfer data;
    data.cb    = cb;
    data.ptr   = ptr;
    data.event = event;
    asyncDataSend(clientfd, buffer, v, data);
    if(event_wait_list) free(event_wait_list); event_wait_list=NULL;
    if(cl_event_wait_list) free(cl_event_wait_list); cl_event_wait_list=NULL;
    return 1;
}

#ifdef CL_API_SUFFIX__VERSION_1_1
int ocland_clCreateSubBuffer(int* clientfd, char* buffer, validator v)
{
    cl_int errcode_ret;
    cl_mem clMem = NULL;
    // Get parameters.
    cl_mem input_buffer;
    cl_mem_flags flags;
    cl_buffer_create_type buffer_create_type;
    cl_buffer_region buffer_create_info;
    Recv(clientfd, &input_buffer, sizeof(cl_mem), MSG_WAITALL);
    Recv(clientfd, &flags, sizeof(cl_mem_flags), MSG_WAITALL);
    Recv(clientfd, &buffer_create_type, sizeof(cl_buffer_create_type), MSG_WAITALL);
    Recv(clientfd, &buffer_create_info, sizeof(cl_buffer_region), MSG_WAITALL);
    // Ensure that memory object is valid
    errcode_ret = isBuffer(v, input_buffer);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &clMem, sizeof(cl_mem), 0);
        return 1;
    }
    clMem = clCreateSubBuffer(input_buffer, flags, buffer_create_type, &buffer_create_info, &errcode_ret);
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &clMem, sizeof(cl_mem), 0);
    // Register new memory object
    registerBuffer(v, clMem);
    return 1;
}

int ocland_clCreateUserEvent(int* clientfd, char* buffer, validator v)
{
    cl_int errcode_ret;
    ocland_event event = NULL;
    // Get parameters.
    cl_context context;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    // Ensure that context is valid
    errcode_ret = isContext(v, context);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &event, sizeof(ocland_event), 0);
        return 1;
    }
    event = (ocland_event)malloc(sizeof(struct _ocland_event));
    if(!event){
        errcode_ret = CL_OUT_OF_RESOURCES;
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &event, sizeof(ocland_event), 0);
        return 0;
    }
    event->event  = NULL;
    event->status = CL_COMPLETE;
    event->event = clCreateUserEvent(context, &errcode_ret);
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &event, sizeof(ocland_event), 0);
    if(errcode_ret == CL_SUCCESS)
        free(event); event=NULL;
    // Register new memory object
    registerEvent(v, event);
    return 1;
}

int ocland_clSetUserEventStatus(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    ocland_event event;
    cl_int execution_status;
    Recv(clientfd, &event, sizeof(ocland_event), MSG_WAITALL);
    Recv(clientfd, &execution_status, sizeof(cl_int), MSG_WAITALL);
    cl_int flag;
    // Ensure that pointer is valid
    flag = isEvent(v, event);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clSetUserEventStatus(event->event, execution_status);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}
#endif

#ifdef CL_API_SUFFIX__VERSION_1_2
int ocland_clCreateSubDevices(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_device_id device;
    size_t sProps;
    Recv(clientfd, &device, sizeof(cl_device_id), MSG_WAITALL);
    Recv(clientfd, &sProps, sizeof(size_t), MSG_WAITALL);
    cl_device_partition_property *properties = NULL;
    if(sProps){
        properties = (cl_device_partition_property*)malloc(sProps);
        Recv(clientfd, properties, sProps, MSG_WAITALL);
    }
    cl_uint num_entries;
    Recv(clientfd, &num_entries, sizeof(cl_uint), MSG_WAITALL);
    cl_int flag;
    cl_device_id *out_devices = NULL;
    cl_uint num_devices_ret=0;
    // Ensure that device is valid
    flag = isDevice(v, device);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &num_devices_ret, sizeof(cl_uint), 0);
        return 1;
    }
    flag = clCreateSubDevices(device, properties, num_entries, out_devices, &num_devices_ret);
    if(properties) free(properties); properties=NULL;
    if(!num_entries || (flag != CL_SUCCESS)){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &num_devices_ret, sizeof(cl_uint), 0);
        return 1;
    }
    // Build devices array
    out_devices = (cl_device_id*)malloc(num_devices_ret*sizeof(cl_device_id));
    flag = clCreateSubDevices(device, properties, num_entries, out_devices, &num_devices_ret);
    // Send data
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &num_devices_ret, sizeof(cl_uint), 0);
    Send(clientfd, out_devices, num_devices_ret*sizeof(cl_device_id), 0);
    // Register new devices
    registerDevices(v, num_devices_ret, out_devices);
    if(out_devices) free(out_devices); out_devices=NULL;
    return 1;
}

int ocland_clRetainDevice(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_device_id device;
    Recv(clientfd, &device, sizeof(cl_device_id), MSG_WAITALL);
    cl_int flag;
    // Ensure that device is valid
    flag = isDevice(v, device);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clRetainDevice(device);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clReleaseDevice(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_device_id device;
    Recv(clientfd, &device, sizeof(cl_device_id), MSG_WAITALL);
    cl_int flag;
    // Ensure that device is valid
    flag = isDevice(v, device);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 1;
    }
    flag = clReleaseDevice(device);
    Send(clientfd, &flag, sizeof(cl_int), 0);
    // Unregister it
    unregisterDevices(v, 1, &device);
    return 1;
}

int ocland_clCreateImage(int* clientfd, char* buffer, validator v)
{
    cl_int errcode_ret;
    cl_mem clMem = NULL;
    // Get parameters.
    cl_context context;
    cl_mem_flags flags;
    cl_image_format image_format;
    cl_image_desc image_desc;
    void* host_ptr = NULL;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    Recv(clientfd, &flags, sizeof(cl_mem_flags), MSG_WAITALL);
    Recv(clientfd, &image_format, sizeof(cl_image_format), MSG_WAITALL);
    Recv(clientfd, &image_desc, sizeof(cl_image_desc), MSG_WAITALL);
    if(flags & CL_MEM_COPY_HOST_PTR){
        // Compute the image size
        size_t size = 0;
        if( (image_desc.image_type & CL_MEM_OBJECT_IMAGE1D) ||
            (image_desc.image_type & CL_MEM_OBJECT_IMAGE1D_BUFFER) ){
            size = image_desc.image_row_pitch;
        }
        else if(image_desc.image_type & CL_MEM_OBJECT_IMAGE2D){
            size = image_desc.image_row_pitch * image_desc.image_height;
        }
        else if(image_desc.image_type & CL_MEM_OBJECT_IMAGE3D){
            size = image_desc.image_slice_pitch * image_desc.image_depth;
        }
        else if( (image_desc.image_type & CL_MEM_OBJECT_IMAGE1D_ARRAY) ||
                 (image_desc.image_type & CL_MEM_OBJECT_IMAGE2D_ARRAY) ){
            size = image_desc.image_slice_pitch * image_desc.image_array_size;
        }
        // Really large data, take care here
        host_ptr = (void*)malloc(size);
        size_t buffsize = BUFF_SIZE*sizeof(char);
        if(!host_ptr){
            buffsize = 0;
            Send(clientfd, &buffsize, sizeof(size_t), 0);
            return 0;
        }
        Send(clientfd, &buffsize, sizeof(size_t), 0);
        // Compute the number of packages needed
        unsigned int i,n;
        n = size / buffsize;
        // Receive package by pieces
        for(i=0;i<n;i++){
            Recv(clientfd, host_ptr + i*buffsize, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, host_ptr + n*buffsize, size % buffsize, MSG_WAITALL);
        }
    }
    // Ensure that context is valid
    errcode_ret = isContext(v, context);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &clMem, sizeof(cl_mem), 0);
        return 1;
    }
    clMem = clCreateImage(context, flags, &image_format, &image_desc, host_ptr, &errcode_ret);
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &clMem, sizeof(cl_mem), 0);
    // Register new memory object
    registerBuffer(v, clMem);
    return 1;
}

int ocland_clCreateProgramWithBuiltInKernels(int* clientfd, char* buffer, validator v)
{
    unsigned int i,n;
    cl_int errcode_ret = CL_SUCCESS;
    cl_program program = NULL;
    // Get parameters.
    cl_context context;
    cl_uint num_devices;
    size_t size;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    Recv(clientfd, &num_devices, sizeof(cl_uint), MSG_WAITALL);
    cl_device_id device_list[num_devices];
    Recv(clientfd, device_list, num_devices*sizeof(cl_device_id), MSG_WAITALL);
    Recv(clientfd, &size, sizeof(size_t), MSG_WAITALL);
    char* kernel_names = (char*)malloc(size);
    if( !kernel_names ){
        // We cannot stop the execution because client
        // is expecting send data
        errcode_ret = CL_OUT_OF_RESOURCES;
    }
    size_t buffsize = BUFF_SIZE*sizeof(char);
    Send(clientfd, &buffsize, sizeof(size_t), 0);
    // Compute the number of packages needed
    n = size / buffsize;
    // Receive package by pieces
    if(errcode_ret != CL_SUCCESS){
        char dummy[buffsize];
        // Receive package by pieces
        for(i=0;i<n;i++){
            Recv(clientfd, dummy, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, dummy, size % buffsize, MSG_WAITALL);
        }
    }
    else{
        for(i=0;i<n;i++){
            Recv(clientfd, kernel_names + i*buffsize, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, kernel_names + n*buffsize, size % buffsize, MSG_WAITALL);
        }
    }
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &program, sizeof(cl_program), 0);
        if(kernel_names) free(kernel_names); kernel_names=NULL;
        return 1;
    }
    // Ensure that context is valid
    errcode_ret = isContext(v, context);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &program, sizeof(cl_program), 0);
        if(kernel_names) free(kernel_names); kernel_names=NULL;
        return 1;
    }
    // Ensure that devices are valid
    for(i=0;i<num_devices;i++){
        errcode_ret = isDevice(v, device_list[i]);
        if(errcode_ret != CL_SUCCESS){
            Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
            Send(clientfd, &program, sizeof(cl_program), 0);
            if(kernel_names) free(kernel_names); kernel_names=NULL;
            return 1;
        }
    }
    program = clCreateProgramWithBuiltInKernels(context,num_devices,device_list,(const char*)kernel_names,&errcode_ret);
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &program, sizeof(cl_program), 0);
    if(kernel_names) free(kernel_names); kernel_names=NULL;
    // Register new program
    registerProgram(v, program);
    return 1;
}

int ocland_clCompileProgram(int* clientfd, char* buffer, validator v)
{
    unsigned int i,j,n;
    cl_int flag = CL_SUCCESS;
    // Get parameters.
    cl_program program;
    cl_uint num_devices;
    cl_device_id *device_list = NULL;
    size_t size;
    Recv(clientfd, &program, sizeof(cl_program), MSG_WAITALL);
    Recv(clientfd, &num_devices, sizeof(cl_uint), MSG_WAITALL);
    if(num_devices){
        device_list = (cl_device_id*)malloc(num_devices*sizeof(cl_device_id));
        if(!device_list){
            // We cannot stop the execution because client
            // is expecting send data
            flag = CL_OUT_OF_RESOURCES;
            cl_device_id dummy[num_devices];
            Recv(clientfd, dummy, num_devices*sizeof(cl_device_id), MSG_WAITALL);
        }
        else{
            Recv(clientfd, device_list, num_devices*sizeof(cl_device_id), MSG_WAITALL);
        }
    }
    Recv(clientfd, &size, sizeof(size_t), MSG_WAITALL);
    char* options = (char*)malloc(size);
    if( !options ){
        // We cannot stop the execution because client
        // is expecting send data
        flag = CL_OUT_OF_RESOURCES;
    }
    size_t buffsize = BUFF_SIZE*sizeof(char);
    Send(clientfd, &buffsize, sizeof(size_t), 0);
    // Compute the number of packages needed
    n = size / buffsize;
    // Receive package by pieces
    if(flag != CL_SUCCESS){
        char dummy[buffsize];
        // Receive package by pieces
        for(i=0;i<n;i++){
            Recv(clientfd, dummy, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, dummy, size % buffsize, MSG_WAITALL);
        }
    }
    else{
        for(i=0;i<n;i++){
            Recv(clientfd, options + i*buffsize, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, options + n*buffsize, size % buffsize, MSG_WAITALL);
        }
    }
    cl_uint num_input_headers = 0;
    Recv(clientfd, &num_input_headers, sizeof(cl_uint), MSG_WAITALL);
    cl_program *input_headers = NULL;
    char **header_include_names = NULL;
    if(num_input_headers){
        input_headers = (cl_program*)malloc(num_input_headers*sizeof(cl_program));
        if(!input_headers){
            // We cannot stop the execution because client
            // is expecting send data
            flag = CL_OUT_OF_RESOURCES;
            cl_program dummy[num_input_headers];
            Recv(clientfd, dummy, num_input_headers*sizeof(cl_program), MSG_WAITALL);
        }
        else{
            Recv(clientfd, input_headers, num_input_headers*sizeof(cl_program), MSG_WAITALL);
        }
        header_include_names = (char**)malloc(num_input_headers*sizeof(char*));
        if(!header_include_names){
            // We cannot stop the execution because client
            // is expecting send data
            flag = CL_OUT_OF_RESOURCES;
            char dummy[buffsize];
            for(i=0;i<num_input_headers;i++){
                // Compute the number of packages needed
                Recv(clientfd, &size, sizeof(size_t), MSG_WAITALL);
                n = size / buffsize;
                // Receive package by pieces
                for(j=0;j<n;j++){
                    Recv(clientfd, dummy, buffsize, MSG_WAITALL);
                }
                if(size % buffsize){
                    // Remains some data to arrive
                    Recv(clientfd, dummy, size % buffsize, MSG_WAITALL);
                }
            }
        }
        else{
            for(i=0;i<num_input_headers;i++){
                // Compute the number of packages needed
                Recv(clientfd, &size, sizeof(size_t), MSG_WAITALL);
                n = size / buffsize;
                header_include_names[i] = (char*)malloc(size);
                if(!header_include_names[i]){
                    char dummy[buffsize];
                    // Receive package by pieces
                    for(j=0;j<n;j++){
                        Recv(clientfd, dummy, buffsize, MSG_WAITALL);
                    }
                    if(size % buffsize){
                        // Remains some data to arrive
                        Recv(clientfd, dummy, size % buffsize, MSG_WAITALL);
                    }
                }
                else{
                    for(j=0;j<n;j++){
                        Recv(clientfd, header_include_names[i] + j*buffsize, buffsize, MSG_WAITALL);
                    }
                    if(size % buffsize){
                        // Remains some data to arrive
                        Recv(clientfd, header_include_names[i] + n*buffsize, size % buffsize, MSG_WAITALL);
                    }
                }
            }
        }
    }
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        if(header_include_names){
            for(i=0;i<num_input_headers;i++){
                if(header_include_names[i]);free(header_include_names[i]);header_include_names[i]=NULL;
            }
        }
        if(device_list) free(device_list); device_list=NULL;
        if(options) free(options); options=NULL;
        if(input_headers) free(input_headers); input_headers=NULL;
        if(header_include_names) free(header_include_names); header_include_names=NULL;
        return 0;
    }
    // Ensure that program is valid
    flag = isProgram(v, program);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        if(header_include_names){
            for(i=0;i<num_input_headers;i++){
                if(header_include_names[i]);free(header_include_names[i]);header_include_names[i]=NULL;
            }
        }
        if(device_list) free(device_list); device_list=NULL;
        if(options) free(options); options=NULL;
        if(input_headers) free(input_headers); input_headers=NULL;
        if(header_include_names) free(header_include_names); header_include_names=NULL;
        return 0;
    }
    // Ensure that devices are valid
    for(i=0;i<num_devices;i++){
        flag = isDevice(v, device_list[i]);
        if(flag != CL_SUCCESS){
            Send(clientfd, &flag, sizeof(cl_int), 0);
            if(header_include_names){
                for(i=0;i<num_input_headers;i++){
                    if(header_include_names[i]);free(header_include_names[i]);header_include_names[i]=NULL;
                }
            }
            if(device_list) free(device_list); device_list=NULL;
            if(options) free(options); options=NULL;
            if(input_headers) free(input_headers); input_headers=NULL;
            if(header_include_names) free(header_include_names); header_include_names=NULL;
            return 0;
        }
    }
    // Ensure that headers are valid
    for(i=0;i<num_input_headers;i++){
        flag = isProgram(v, input_headers[i]);
        if(flag != CL_SUCCESS){
            Send(clientfd, &flag, sizeof(cl_int), 0);
            if(header_include_names){
                for(i=0;i<num_input_headers;i++){
                    if(header_include_names[i]);free(header_include_names[i]);header_include_names[i]=NULL;
                }
            }
            if(device_list) free(device_list); device_list=NULL;
            if(options) free(options); options=NULL;
            if(input_headers) free(input_headers); input_headers=NULL;
            if(header_include_names) free(header_include_names); header_include_names=NULL;
            return 0;
        }
    }
    flag = clCompileProgram(program,num_devices,device_list,options,num_input_headers,input_headers,(const char**)header_include_names,NULL,NULL);
    // Write output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    if(header_include_names){
        for(i=0;i<num_input_headers;i++){
            if(header_include_names[i]);free(header_include_names[i]);header_include_names[i]=NULL;
        }
    }
    if(device_list) free(device_list); device_list=NULL;
    if(options) free(options); options=NULL;
    if(input_headers) free(input_headers); input_headers=NULL;
    if(header_include_names) free(header_include_names); header_include_names=NULL;
    return 1;
}

int ocland_clLinkProgram(int* clientfd, char* buffer, validator v)
{
    unsigned int i,n;
    cl_program program = NULL;
    cl_int errcode_ret = CL_SUCCESS;
    // Get parameters.
    cl_context context;
    cl_uint num_devices;
    cl_device_id *device_list = NULL;
    size_t size;
    cl_uint num_input_programs;
    Recv(clientfd, &context, sizeof(cl_context), MSG_WAITALL);
    Recv(clientfd, &num_devices, sizeof(cl_uint), MSG_WAITALL);
    if(num_devices){
        device_list = (cl_device_id*)malloc(num_devices*sizeof(cl_device_id));
        if(!device_list){
            // We cannot stop the execution because client
            // is expecting send data
            errcode_ret = CL_OUT_OF_RESOURCES;
            cl_device_id dummy[num_devices];
            Recv(clientfd, dummy, num_devices*sizeof(cl_device_id), MSG_WAITALL);
        }
        else{
            Recv(clientfd, device_list, num_devices*sizeof(cl_device_id), MSG_WAITALL);
        }
    }
    Recv(clientfd, &size, sizeof(size_t), MSG_WAITALL);
    char* options = (char*)malloc(size);
    if( !options ){
        // We cannot stop the execution because client
        // is expecting send data
        errcode_ret = CL_OUT_OF_RESOURCES;
    }
    size_t buffsize = BUFF_SIZE*sizeof(char);
    Send(clientfd, &buffsize, sizeof(size_t), 0);
    // Compute the number of packages needed
    n = size / buffsize;
    // Receive package by pieces
    if(errcode_ret != CL_SUCCESS){
        char dummy[buffsize];
        // Receive package by pieces
        for(i=0;i<n;i++){
            Recv(clientfd, dummy, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, dummy, size % buffsize, MSG_WAITALL);
        }
    }
    else{
        for(i=0;i<n;i++){
            Recv(clientfd, options + i*buffsize, buffsize, MSG_WAITALL);
        }
        if(size % buffsize){
            // Remains some data to arrive
            Recv(clientfd, options + n*buffsize, size % buffsize, MSG_WAITALL);
        }
    }
    Recv(clientfd, &num_input_programs, sizeof(cl_uint), MSG_WAITALL);
    cl_program input_programs[num_input_programs];
    Recv(clientfd, input_programs, num_input_programs*sizeof(cl_program), MSG_WAITALL);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &program, sizeof(cl_program), 0);
        if(device_list) free(device_list); device_list=NULL;
        if(options) free(options); options=NULL;
        return 0;
    }
    // Ensure that context is valid
    errcode_ret = isContext(v, context);
    if(errcode_ret != CL_SUCCESS){
        Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
        Send(clientfd, &program, sizeof(cl_program), 0);
        if(device_list) free(device_list); device_list=NULL;
        if(options) free(options); options=NULL;
        return 0;
    }
    // Ensure that devices are valid
    for(i=0;i<num_devices;i++){
        errcode_ret = isDevice(v, device_list[i]);
        if(errcode_ret != CL_SUCCESS){
            Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
            Send(clientfd, &program, sizeof(cl_program), 0);
            if(device_list) free(device_list); device_list=NULL;
            if(options) free(options); options=NULL;
            return 0;
        }
    }
    // Ensure that programs are valid
    for(i=0;i<num_input_programs;i++){
        errcode_ret = isProgram(v, input_programs[i]);
        if(errcode_ret != CL_SUCCESS){
            Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
            Send(clientfd, &program, sizeof(cl_program), 0);
            if(device_list) free(device_list); device_list=NULL;
            if(options) free(options); options=NULL;
            return 0;
        }
    }
    program = clLinkProgram(context,num_devices,device_list,options,num_input_programs,input_programs,NULL,NULL,&errcode_ret);
    // Write output
    Send(clientfd, &errcode_ret, sizeof(cl_int), 0);
    Send(clientfd, &program, sizeof(cl_program), 0);
    if(device_list) free(device_list); device_list=NULL;
    if(options) free(options); options=NULL;
    // Register the new program
    if(errcode_ret == CL_SUCCESS)
        registerProgram(v, program);
    return 1;
}

int ocland_clUnloadPlatformCompiler(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_platform_id platform;
    Recv(clientfd, &platform, sizeof(cl_platform_id), MSG_WAITALL);
    cl_int flag;
    // Ensure that platform is valid
    flag = isPlatform(v, platform);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        return 0;
    }
    flag = clUnloadPlatformCompiler(platform);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    return 1;
}

int ocland_clGetKernelArgInfo(int* clientfd, char* buffer, validator v)
{
    // Get parameters.
    cl_kernel kernel;
    cl_uint arg_index;
    cl_kernel_arg_info param_name;
    size_t param_value_size;
    Recv(clientfd, &kernel, sizeof(cl_kernel), MSG_WAITALL);
    Recv(clientfd, &arg_index, sizeof(cl_uint), MSG_WAITALL);
    Recv(clientfd, &param_name, sizeof(cl_kernel_arg_info), MSG_WAITALL);
    Recv(clientfd, &param_value_size, sizeof(size_t), MSG_WAITALL);
    cl_int flag;
    size_t param_value_size_ret = 0;
    void* param_value = NULL;
    // Ensure that pointer is valid
    flag = isKernel(v, kernel);
    if(flag != CL_SUCCESS){
        Send(clientfd, &flag, sizeof(cl_int), 0);
        Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
        return 1;
    }
    // Build image formats if requested
    if(param_value_size){
        param_value = (void*)malloc(param_value_size);
        if(!param_value){
            flag = CL_OUT_OF_RESOURCES;
            Send(clientfd, &flag, sizeof(cl_int), 0);
            Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
            return 0;
        }
    }
    flag = clGetKernelArgInfo(kernel,arg_index,param_name,param_value_size,param_value,&param_value_size_ret);
    // Write status output
    Send(clientfd, &flag, sizeof(cl_int), 0);
    Send(clientfd, &param_value_size_ret, sizeof(size_t), 0);
    if(flag != CL_SUCCESS){
        if(param_value) free(param_value); param_value=NULL;
        return 1;
    }
    // Send data
    if(param_value_size){
        Send(clientfd, param_value, param_value_size, 0);
        free(param_value); param_value = NULL;
    }
    return 1;
}
#endif
