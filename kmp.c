#include <stdio.h>
#include <CL/cl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/timeb.h>

#define MAX_SOURCE_SIZE (0x100000)
#define MAX_STRING_SIZE 30
#define MAX_PATTERN_SIZE 5
#define MAX_PATTERN_NUM 4
#define ALPHABET_SIZE 10

/*pattern matching 이기 때문에 하나의 character는 pattern으로 분류하지 않음*/

/*
수정사항

1. event추가해서 synch맞춰주기 ==>문제 없음
2. alphabet size추가 ==>역시 문제 없음
3. string size/ pattern num&size추가
4. setArg에서 local item size곱한 것 제거

*/

int main(void){
	/*opencl을 위한 변수*/
	cl_platform_id platform_id = NULL;
	cl_device_id device_id = NULL;
	cl_context context = NULL;
	cl_command_queue command_queue = NULL;
	cl_mem p_obj = NULL;
	cl_mem s_obj = NULL;
	cl_mem pi_obj[2] = {NULL, NULL};
	cl_mem ret_obj = NULL;
	cl_program program= NULL;
	cl_kernel kernel[2] = {NULL, NULL};

	cl_uint ret_num_platforms;
	cl_uint ret_num_devices;
	cl_int ret;	
	cl_event pi_event, kmp_event;

	/*kmp.cl file을 읽기위한 변수*/
	FILE *fp;
	const char fileName[] = "./kmp.cl";
	size_t source_size;
	char *source_str;

	/*pattern과 그 pattern을 검색할 string.*/
	cl_char *pattern = (cl_char *)malloc(MAX_PATTERN_NUM*MAX_PATTERN_SIZE*sizeof(cl_char));
	cl_char *strings = (cl_char *)malloc(MAX_STRING_SIZE);

	int string_size = MAX_STRING_SIZE;
	int pattern_size = MAX_PATTERN_SIZE;
	int pattern_num = MAX_PATTERN_NUM;

	cl_int *pi = (cl_int *)malloc(MAX_PATTERN_NUM*MAX_PATTERN_SIZE*sizeof(cl_int));

	int i, j = 0;
	int *matched;

	size_t global_item_size = 4; //전체 item 갯수
	size_t local_item_size = 1; //한 group에 들어갈 item 갯수

	size_t log_size;
	char* buildLog;

	matched = (int *)malloc(sizeof(int) * MAX_PATTERN_NUM);

	for(i = 0; i < MAX_PATTERN_NUM; i++){
		matched[i] = 0;
	}

	fp = fopen(fileName, "r");

	if(!fp){
		fprintf(stderr, "Failed to load kernel.\n");
		exit(1);
	}

	source_str = (char *)malloc(MAX_SOURCE_SIZE);
	source_size = fread(source_str, 1, MAX_SOURCE_SIZE, fp);
	fclose(fp);

	/*전체 data를 random 생성으로 바꾼 뒤 큰 데이터를 받아와서 처리하도록 변경*/

	printf("Strings:");
	strncpy((char *)strings, "abababababbbbbhhhhhhabcababab", MAX_STRING_SIZE);
	printf("%s\n", strings);

	/*pattern초기화*/
	pattern[0] = 'a';
	pattern[1] = 'b';
	pattern[2] = 'a';
	pattern[3] = 'b';
	pattern[4] = 'a';

	pattern[5] = 'a';
	pattern[6] = 'b';
	pattern[7] = 'c';
	pattern[8] = 'a';
	pattern[9] = 'b';

	pattern[10] = 'h';
	pattern[11] = 'h';
	pattern[12] = 'h';
	pattern[13] = 'h';
	pattern[14] = 'h';

	pattern[15] = 'd';
	pattern[16] = 'o';
	pattern[17] = 'l';
	pattern[18] = 'o';
	pattern[19] = 'd';
	

	/*data parallel. getPi가 완료되고 난 후에 kmp가 실행이 되어야 aba함*/

	ret = clGetPlatformIDs(1, &platform_id, &ret_num_platforms);
	ret = clGetDeviceIDs(platform_id, CL_DEVICE_TYPE_DEFAULT, 1, &device_id, &ret_num_devices);

	context = clCreateContext(NULL, 1, &device_id, NULL, NULL, &ret);
	command_queue = clCreateCommandQueue(context, device_id, 0, &ret);
	
	/*memory object creation. size는 pattern이나 string의 갯수와 길이에 따라 변화시킬 것*/
	pi_obj[0] = clCreateBuffer(context, CL_MEM_READ_WRITE, MAX_PATTERN_SIZE*MAX_PATTERN_NUM*sizeof(cl_int), NULL, &ret);
	p_obj = clCreateBuffer(context, CL_MEM_READ_WRITE, MAX_PATTERN_SIZE*MAX_PATTERN_NUM*sizeof(cl_char), NULL, &ret);

	/*buffer object 생성*/
	ret = clEnqueueWriteBuffer(command_queue, p_obj, CL_TRUE, 0, MAX_PATTERN_SIZE*MAX_PATTERN_NUM*sizeof(cl_char), pattern, 0, NULL, NULL);

	program = clCreateProgramWithSource(context, 1, (const char**)&source_str, (const size_t *)&source_size, &ret);
	
	ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);
	clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, 0, NULL, &log_size);
	buildLog = (char *)malloc(log_size);
	clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, log_size, buildLog, NULL);
	printf("log: %s\n", buildLog);
	free(buildLog);

	/*kernel함수별로 하나의 kernel object가 존재*/
	kernel[0] = clCreateKernel(program, "getPi", &ret);

	/*kmp알고리즘을 이용해 pattern을 찾는 함수*/
	ret = clSetKernelArg(kernel[0], 0, sizeof(cl_mem), (void *)&p_obj);
	ret = clSetKernelArg(kernel[0], 1, sizeof(cl_mem), (void *)&pi_obj[0]);
	ret = clSetKernelArg(kernel[0], 2, sizeof(pattern_size), &pattern_size); //추가 한 부분

	ret = clEnqueueNDRangeKernel(command_queue, kernel[0], 1, NULL, &global_item_size, &local_item_size, 0, NULL, &pi_event);

	clWaitForEvents(1, &pi_event);

	ret = clEnqueueReadBuffer(command_queue, pi_obj[0], CL_TRUE, 0,  MAX_PATTERN_SIZE*MAX_PATTERN_NUM*sizeof(cl_int), pi, 0, NULL, NULL);

	for(i = 0; i < MAX_PATTERN_NUM*MAX_PATTERN_SIZE; i ++){
			printf("pi[%d] : %d\n", i, pi[i]);
	}

	/*kmp함수 실행 부분*/
	s_obj = clCreateBuffer(context, CL_MEM_READ_WRITE, MAX_STRING_SIZE*sizeof(cl_char), NULL, &ret);
	ret_obj = clCreateBuffer(context, CL_MEM_READ_WRITE, MAX_PATTERN_NUM*sizeof(cl_int), NULL, &ret);
	pi_obj[1] = clCreateBuffer(context, CL_MEM_READ_WRITE, MAX_PATTERN_SIZE*MAX_PATTERN_NUM*sizeof(cl_int), NULL, &ret);

	/*할 때마다 해줘야 하는지?*/
	ret = clEnqueueWriteBuffer(command_queue, s_obj, CL_TRUE, 0, MAX_STRING_SIZE* sizeof(cl_char), strings, 0, NULL, NULL);
	ret = clEnqueueWriteBuffer(command_queue, pi_obj[1], CL_TRUE, 0,MAX_PATTERN_NUM*MAX_PATTERN_SIZE*sizeof(cl_int), pi, 0, NULL, NULL);
	ret = clEnqueueWriteBuffer(command_queue, p_obj, CL_TRUE, 0, MAX_PATTERN_SIZE*MAX_PATTERN_NUM*sizeof(cl_char), pattern, 0, NULL, NULL);

	kernel[1] = clCreateKernel(program, "kmp", &ret);
	ret = clSetKernelArg(kernel[1], 0, sizeof(cl_mem), (void *)&p_obj);
	ret = clSetKernelArg(kernel[1], 1, sizeof(cl_mem), (void *)&s_obj);
	ret = clSetKernelArg(kernel[1], 2, sizeof(cl_mem), (void *)&pi_obj[1]);
	ret = clSetKernelArg(kernel[1], 3, sizeof(cl_mem), (void *)&ret_obj);
	ret = clSetKernelArg(kernel[1], 4, sizeof(pattern_size), &pattern_size);
	ret = clSetKernelArg(kernel[1], 5, sizeof(string_size), &string_size);

	ret = clEnqueueNDRangeKernel(command_queue, kernel[1], 1, NULL, &global_item_size, &local_item_size, 0, NULL, &kmp_event);

	clWaitForEvents(1, &kmp_event);

	ret = clEnqueueReadBuffer(command_queue, ret_obj, CL_TRUE, 0, (global_item_size/local_item_size)*sizeof(cl_uint), matched, 0, NULL, NULL);

	for(i = 0; i < MAX_PATTERN_NUM; i++){
		printf("matched[%d]: %d\n", i, matched[i]);
	}
	
	ret = clFlush(command_queue);
	ret = clFinish(command_queue);
	ret = clReleaseKernel(kernel[0]);
	ret = clReleaseKernel(kernel[1]);
	ret = clReleaseProgram(program);
	ret = clReleaseMemObject(s_obj);
	ret = clReleaseMemObject(p_obj);
	ret = clReleaseMemObject(pi_obj[0]);
	ret = clReleaseMemObject(pi_obj[1]);
	ret = clReleaseMemObject(ret_obj);
	ret = clReleaseCommandQueue(command_queue);
	ret = clReleaseContext(context);
	
	free(pattern);
	free(pi);
	free(strings);
	free(source_str);
	free(matched);

	return 0;
}
