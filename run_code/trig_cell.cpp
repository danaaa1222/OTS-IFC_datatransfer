#define _CRT_SECURE_NO_WARNINGS // This define removes warnings for printf
#define _CRT_SECURE_NO_DEPRECATE

#include <iostream>
#include <fstream>
#include <thread>
#include <ctime> 
#include<vector>
#include <string>
#include <queue>
#include <cstdlib>
#include <sstream>
#include <cstring>
#include<windows.h>
#include "concurrentqueue.h"
#include "blockingconcurrentqueue.h"
#include <stdio.h>
#include "adq_transfer_test.h"
#include "setting.h"

#include "../ADQAPI/ADQAPI.h"
#include "../ADQAPI/os.h"
#include <time.h>
using namespace std;

//#define VERBOSE

#ifdef LINUX
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define Sleep(interval) usleep(1000*interval)
static struct timespec tsref[10];
void timer_start(unsigned int index) {
	if (clock_gettime(CLOCK_REALTIME, &tsref[index]) < 0) {
		printf("\nFailed to start timer.");
		return;
	}
}
unsigned int timer_time_ms(unsigned int index) {
	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) < 0) {
		printf("\nFailed to get system time.");
		return -1;
	}
	return (unsigned int)((int)(ts.tv_sec - tsref[index].tv_sec) * 1000 +
		(int)(ts.tv_nsec - tsref[index].tv_nsec) / 1000000);
}
#else
clock_t tref[10];
void timer_start(unsigned int index) {
	tref[index] = clock();
	if (tref[index] < 0) {
		printf("\nFailed to start timer.");
		return;
	}
}
unsigned int timer_time_ms(unsigned int index) {
	clock_t t = clock();
	if (t < 0) {
		printf("\nFailed to get system time.");
		return -1;
	}
	return (unsigned int)((float)(t - tref[index]) * 1000.0f / CLOCKS_PER_SEC);
}
#endif

unsigned int stream_error[8];

#define PRINT_EVERY_N_MBYTES_DEFAULT (1024ULL) // Default is to print transfer stats every GByte of data
//#define RUN_TIME_DEFAULT              30 // Default is to run forever
unsigned int periodic_report_interval = PRINT_EVERY_N_MBYTES_DEFAULT;

double expected_data_prod_rate = -1.0;
unsigned long long tr_bytes = 0;
unsigned long long tr_bytes_since_last_print;
unsigned long long nof_buffers_fetched = 0;
unsigned int time_stamped = 0;
int time_diff;
double tr_speed;
double tr_speed_now;
double tr_speed_raw;
unsigned int dram_wrcnt_max = 0;
unsigned int max_used_tr_buf = 0;
unsigned int end_condition = 0;
unsigned int expected_transfer_performance_in_mbytes = 0;
unsigned int pcie_lanes = 0;
unsigned int pcie_gen = 0;
unsigned long long nof_received_records_sum = 0;
unsigned int parse_mode = 1;
void* raw_data_ptr = NULL;

#define ADQ14_DRAM_SIZE_BYTES (2ULL*1024ULL*1024ULL*1024ULL)
#define ADQ14_DRAM_SIZE_PER_COUNT (128ULL)

#define ADQ7_DRAM_SIZE_BYTES (4ULL*1024ULL*1024ULL*1024ULL)
#define ADQ7_DRAM_SIZE_PER_COUNT (128ULL)

long long adq_dram_size_bytes = ADQ14_DRAM_SIZE_BYTES;
long long adq_dram_size_per_count = ADQ14_DRAM_SIZE_PER_COUNT;

void adq_perform_transfer_test(void* adq_cu, int adq_num, int adq_type);

#define CHECKADQ(f) if(!(f)){printf("Error in " #f "\n"); goto error;}

#define MIN(a,b) ((a) > (b) ? (b) : (a))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

// THE FOLLOWING DEFINE IS ONLY AVAILABLE FOR USB DEVICES
// #define NONPOLLING_TRANSFER_STATUS

//===========================================
struct nodes
{
	short* data_address = NULL;
	struct ADQRecordHeader* head_address = NULL;
	int64_t data_bytes;
	int64_t cell_size;
	int64_t header_number;
	int64_t file_number;
};
nodes alt_node;

int count_record = 0;//产生了多少个rcd

moodycamel::BlockingConcurrentQueue<nodes> q;

bool popOver[4] = { 0,0,0,0 };//每个consumer有没有结束 


void* produceItem(void* adq_cu, int adq_num, int adq_type)//adq_data_tsfer
{
	adq_perform_transfer_test(adq_cu, adq_num, adq_type);
	return((void*)0);
}

static int write_record_to_file(void* pBuf, size_t datasize, int record,int cell_size)  //多少个点
{
	char filename[256] = "";
	FILE* fp = NULL;
	size_t bytes_written = 0;
	sprintf(filename, "%s_r%d_c%d.bin", FILENAME_RECORD, record, cell_size);//写数据

	fp = fopen(filename, "wb");
	if (fp == NULL)
	{
		printf("Failed to open the file '%s' for writing.\n", filename);
		return -1;
	}
	bytes_written = fwrite(pBuf, 2, datasize, fp);//写多少个点
	if (bytes_written != datasize)
	{
		printf("Failed to write %zu bytes to the file '%s', wrote %zu bytes.\n", datasize * 2, filename, bytes_written);
		fclose(fp);
		return -1;
	}
	fclose(fp);

	return 0;
}


void* consumeItem(int num)
{
	while (1)
	{
		nodes tmp;
		q.wait_dequeue(tmp);
		write_record_to_file(tmp.data_address, tmp.data_bytes / 2, tmp.file_number,tmp.cell_size);//-写细胞数据
		//write_header_to_file(tmp.head_address, tmp.header_number, tmp.file_number);//-写头文件
		free(tmp.data_address);
		//free(tmp.head_address);
		tmp.data_address = NULL;
		//tmp.head_address = NULL;
	}
	popOver[num] = TRUE;
	return((void*)0);
}
void adq_process_record(short* record_data, struct ADQRecordHeader* record_header)
{
	short* alt_buffer = (short*)malloc(record_header->RecordLength * 2);
	if (alt_buffer)
	{
		memcpy(alt_buffer, record_data, sizeof(short) * record_header->RecordLength);

		alt_node.data_address = alt_buffer;
		alt_node.data_bytes = record_header->RecordLength * sizeof(short);
		alt_node.file_number = record_header->RecordNumber;
		alt_node.cell_size = record_header->GeneralPurpose1;
		q.enqueue(alt_node);
		count_record++;
	}
	else
		printf("there is no memory!!!!!!!!!!!\n");
}

void adq_transfer_test(void* adq_cu, int adq_num, int adq_type)
{
	char* serialnumber;
	int* revision = ADQ_GetRevision(adq_cu, adq_num);
	double tlocal, tr1, tr2, tr3, tr4;

	if (adq_type == 714)
	{
		printf("\nConnected to ADQ14 #1\n\n");
		adq_dram_size_bytes = ADQ14_DRAM_SIZE_BYTES;
		adq_dram_size_per_count = ADQ14_DRAM_SIZE_PER_COUNT;
	}
	else if (adq_type == 7)
	{
		printf("\nConnected to ADQ7 #1\n\n");
		adq_dram_size_bytes = ADQ7_DRAM_SIZE_BYTES;
		adq_dram_size_per_count = ADQ7_DRAM_SIZE_PER_COUNT;
	}
	//Print revision information

	printf("FPGA Revision: %d, ", revision[0]);
	if (revision[1])
		printf("Local copy");
	else
		printf("SVN Managed");
	printf(", ");
	if (revision[2])
		printf("Mixed Revision");
	else
		printf("SVN Updated");
	printf("\n\n");

	// Checking for in-compatible firmware
	if (ADQ_HasFeature(adq_cu, adq_num, "FWATD") == 1)
	{
		printf("ERROR: This device is loaded with FWATD firmware and cannot be used for this example. Please see FWATD examples.\n");
		return;
	}
	if (ADQ_HasFeature(adq_cu, adq_num, "FWPD") == 1)
	{
		printf("ERROR: This device is loaded with FWPD firmware and cannot be used for this example. Please see FWPD examples.\n");
		return;
	}


	// Setup pre-init stage and report pre-start status
	if (adq_type == 714)
	{
		ADQ_SetDirectionGPIO(adq_cu, adq_num, 31, 0);
		ADQ_WriteGPIO(adq_cu, adq_num, 31, 0);
	}
	tlocal = (double)ADQ_GetTemperature(adq_cu, adq_num, 0) / 256.0;
	tr1 = (double)ADQ_GetTemperature(adq_cu, adq_num, 1) / 256.0;
	tr2 = (double)ADQ_GetTemperature(adq_cu, adq_num, 2) / 256.0;
	tr3 = (double)ADQ_GetTemperature(adq_cu, adq_num, 3) / 256.0;
	tr4 = (double)ADQ_GetTemperature(adq_cu, adq_num, 4) / 256.0;
	serialnumber = ADQ_GetBoardSerialNumber(adq_cu, adq_num);

	printf("Temperatures\n Local:   %5.2f deg C\n ADC0:    %5.2f deg C\n ADC1:    %5.2f deg C\n FPGA:    %5.2f deg C\n PCB diode: %5.2f deg C\n",
		tlocal, tr1, tr2, tr3, tr4);

	printf("Device Serial Number: %s\n", serialnumber);

	if (ADQ_IsPCIeDevice(adq_cu, adq_num))
	{
		pcie_lanes = ADQ_GetPCIeLinkWidth(adq_cu, adq_num);
		pcie_gen = ADQ_GetPCIeLinkRate(adq_cu, adq_num);
		expected_transfer_performance_in_mbytes = 200 * pcie_lanes * pcie_gen;
		printf("Device interface is PCIe/PXIe (enumerated as x%02dg%02d).\n", pcie_lanes, pcie_gen);
	}
	else if (ADQ_IsUSB3Device(adq_cu, adq_num))
	{
		expected_transfer_performance_in_mbytes = 300;
		printf("Device interface is USB (enumerated as USB3).\n");
	}
	else if (ADQ_IsUSBDevice(adq_cu, adq_num))
	{
		expected_transfer_performance_in_mbytes = 25;
		printf("Device interface is USB (enumerated as USB2).\n");
	}
	else if (ADQ_IsEthernetDevice(adq_cu, adq_num))
	{
		expected_transfer_performance_in_mbytes = 800;
		printf("Device interface is 10GbE.\n");
	}
	else
	{
		expected_transfer_performance_in_mbytes = 1;
		printf("Device interface is unknown. No expected transfer performance set.\n");
	}
	printf("Transfer performance of interface is approximately %4u MByte/sec\n", expected_transfer_performance_in_mbytes);
	//===============================================
   /* Initialize data acquisition parameters. */

//-------------------------------------------------------------------------------------------------------------
	unsigned int retval_u1_reg12;
	unsigned int retval_u1_reg13;
	unsigned int retval_u2_reg11;
	unsigned int retval_u2_reg12;
	unsigned int retval_u2_reg13;
	////unsigned int CellParam = 1 << 10 | 1 << 9 | 1<< 8 | 1 << 7 | 1 << 6 | 1<< 5 | 3 << 1;
	//unsigned int Remove_valiue = 0x0000;  //符号也有用 负数也有用 
	//unsigned int Pulse_threhold = 0x0000;
	//unsigned int CELL_COMEING_THRESHOLD = 0x317A;
	//unsigned int NUM_OF_SAMPLE_IN_PULSE = 0x0001;
	//unsigned int POSITION_OF_PULSE_END = 0x0001;
	unsigned int Reg0x11 = CELL_EXTEND_PERIOD << 20 | CONTORL_LATENCY << 14 | CONTORL_INVERSE << 8 | REDUCE_PULSE;
	ADQ_WriteUserRegister(adq_cu, adq_num, 1, 0x11, 0, Reg0x11, &retval_u2_reg11); //Only CONTORL_INVERSE is valid
	ADQ_WriteUserRegister(adq_cu, adq_num, 2, 0x11, 0, Reg0x11, &retval_u2_reg11);

	unsigned int Reg0x12 = Pulse_threhold << 16 | Remove_value;
	ADQ_WriteUserRegister(adq_cu, adq_num, 1, 0x12, 0, Reg0x12, &retval_u1_reg12);
	ADQ_WriteUserRegister(adq_cu, adq_num, 2, 0x12, 0, Reg0x12, &retval_u2_reg12);

	unsigned int Reg0x13 = POSITION_OF_PULSE_END << 24 | NUM_OF_SAMPLE_IN_PULSE << 16 | CELL_COMEING_THRESHOLD;
	ADQ_WriteUserRegister(adq_cu, adq_num, 1, 0x13, 0, Reg0x13, &retval_u1_reg13);
	ADQ_WriteUserRegister(adq_cu, adq_num, 2, 0x13, 0, Reg0x13, &retval_u2_reg13);
	printf("\nretval_u1_reg12 = %x\n", retval_u1_reg12);
	printf("\nretval_u1_reg13 = %x\n", retval_u1_reg13);
	printf("\nretval_u2_reg12 = %x\n", retval_u2_reg12);
	printf("\nretval_u2_reg13 = %x\n", retval_u2_reg13);

	////Sleep(1000);
	unsigned int Actual_Reg_10;
	ADQ_ReadUserRegister(adq_cu, adq_num, 2, 0x10, &Actual_Reg_10);
	printf("\nActual_Reg_10 = %x\n", Actual_Reg_10);
	unsigned int Actual_Reg_11;
	ADQ_ReadUserRegister(adq_cu, adq_num, 2, 0x11, &Actual_Reg_11);
	printf("\nActual_Reg_11 = %x\n", Actual_Reg_11);

	unsigned int Actual_Reg_12;
	ADQ_ReadUserRegister(adq_cu, adq_num, 2, 0x12, &Actual_Reg_12);
	printf("\nActual_Reg_12 = %x\n", Actual_Reg_12);
	unsigned int Actual_Reg_13;
	ADQ_ReadUserRegister(adq_cu, adq_num, 2, 0x13, &Actual_Reg_13);
	printf("\nActual_Reg_13 = %x\n", Actual_Reg_13);

	//Sleep(1000);

	//--------------------------------------------------------------------------------------------------------------

	thread t(produceItem, adq_cu, adq_num, adq_type);
	thread t0(consumeItem, 0);
	thread t1(consumeItem, 1);
	thread t2(consumeItem, 2);
	thread t3(consumeItem, 3);
	//	adq_perform_transfer_test(adq_cu, adq_num, adq_type);

	t.join();
	t0.join();
	t1.join();
	t2.join();
	t3.join();

}

void adq_perform_transfer_test(void* adq_cu, int adq_num, int adq_type) {
	//Setup ADQ
	int trig_mode;
	double sampleratehz;
	unsigned int transfer_error = 0;
	//unsigned int samples_per_record;
	unsigned int pretrig_samples;
	unsigned int triggerdelay_samples;
	unsigned int success;
	unsigned int nof_records = 0;
	unsigned int records_completed[4] = { 0, 0, 0, 0 };
	unsigned char channelsmask;
	//	unsigned int tr_buf_size = KERNEL_TR_BUF_SIZE;
	//	unsigned int tr_buf_no = KERNEL_TR_BUF_NO;
		//unsigned int trig_freq = 0;
		//unsigned int trig_period = 0;
	unsigned int timeout_ms = 1000;
	int config_mode = 0;

	unsigned int nof_records_sum = 0;
	unsigned int received_all_records = 0;

	unsigned int nof_channels;
	int exit = 0;
	unsigned int ch = 0;
	unsigned int buffers_filled = 0;

	// Buffers to handle the stream output (both headers and data)
	short* target_buffers_extradata[4] = { NULL, NULL, NULL, NULL };
	short* target_buffers[4] = { NULL, NULL, NULL, NULL };
	struct ADQRecordHeader* target_headers[4] = { NULL, NULL, NULL, NULL };
	short* record_data;

	// Variables to handle the stream output (both headers and data)
	unsigned int header_status[4] = { 0, 0, 0, 0 };
	unsigned int samples_added[4] = { 0, 0, 0, 0 };
	unsigned int headers_added[4] = { 0, 0, 0, 0 };
	unsigned int samples_extradata[4] = { 0, 0, 0, 0 };
	unsigned int samples_remaining;
	unsigned int headers_done = 0;

	unsigned int use_nof_channels;
	//	double samples_per_record_d;
	double average_rate_per_channel;
	double max_mbyte_per_second_per_channel;
	double dram_fill_percentage;
	// Bias ADC codes
	int adjustable_bias = 0;

	struct ADQRecordHeader RawHeaderEmulation;

	nof_channels = ADQ_GetNofChannels(adq_cu, adq_num);

	trig_mode = ADQ_INTERNAL_TRIGGER_MODE;

	//printf("\nChoose configuration mode:\n - 0 is run with default settings [DEFAULT]\n - 1 is for enabling configuration settings\n - 2 is for enabling advanced settings\n");
	//scanf(" %d", &config_mode);
	//if ((config_mode < 0) || (config_mode > 2))
	//	config_mode = 0;

	//max_run_time = RUN_TIME_DEFAULT;
	//test_speed_mbyte = 1024; // 1 GByte/sec is default
	//trig_freq = 1000; // Set default 1 kHz trigger
	//if (config_mode > 0)
	//{
	//	printf("\nChoose parsing mode:\n 1 is full parsed mode [DEFAULT]\n 2 is no user-parsing mode\n 3 is RAW mode\n");
	//	scanf("%d", &parse_mode);
	//	if ((parse_mode > 3) && (parse_mode < 1))
	//	{
			//parse_mode = 1;
		//}

		//printf("\nChoose desired run time in seconds. (0 is DEFAULT which is 30 seconds, -1 is infinite run)\n");
		//scanf("%d", &max_run_time);
		//if (max_run_time == 0)
			//max_run_time = RUN_TIME_DEFAULT;

		//printf("\nChoose desired transfer speed in MByte/s to test for (0 is default test)\n");
		//scanf("%d", &test_speed_mbyte);
		//if (test_speed_mbyte == 0)
		//{
		//	test_speed_mbyte = 1024; // 1 GByte/sec is default
		//}

		//printf("\nNOTE: Higher trigger rates will use shorter records, which will in turn create more overhead.\n");
		//printf("\nChoose desired trigger rate used for test (0 is default at 1kHz)\n");
		//scanf("%d", &trig_freq);
		//if (trig_freq == 0)
		//	trig_freq = 1000; // Set default 1 kHz trigger
	//}

	// Max is around 98% duty-cycle per channel
	ADQ_GetSampleRate(adq_cu, adq_num, 0, &sampleratehz);
	max_mbyte_per_second_per_channel = ((((sampleratehz * 98.0) / 100.0) * 2.0) / 1024.0 / 1024.0);
	unsigned int trig_freq = (unsigned int)(sampleratehz / (double)trig_period);
	unsigned int test_speed_mbyte = (unsigned int)(samples_per_record * 2.0 * (double)trig_freq / 1024.0 / 1024.0);

	//unsigned int test_speed_mbyte = (unsigned int)(samples_per_record * 2.0 * (double)trig_freq / 1024.0 / 1024.0);
	use_nof_channels = (unsigned int)(((unsigned long long)test_speed_mbyte / (unsigned long long)max_mbyte_per_second_per_channel) + 1);

	if (use_nof_channels > nof_channels)
	{
		printf("[ERROR]:  Unit cannot produce the amount of data specified in test.\n");
		goto error;
	}

	channelsmask = 0x00;
	if (use_nof_channels > 0)
		channelsmask |= 0x01;
	if (use_nof_channels > 1)
		channelsmask |= 0x02;
	if (use_nof_channels > 2)
		channelsmask |= 0x04;
	if (use_nof_channels > 3)
		channelsmask |= 0x08;

	average_rate_per_channel = (double)test_speed_mbyte / (double)use_nof_channels;

	//if (config_mode == 2)
	//{
	//	printf("\nSet number of kernel buffers (0 is %u [DEFAULT], other values for the number of buffers)\n", tr_buf_no);
	//	scanf("%d", &sel_tr_buf_no);
	//	if (sel_tr_buf_no == 0)
			//sel_tr_buf_no = tr_buf_no;

		//printf("\nSet size of kernel buffers in kb (0 is %u [DEFAULT], other values for the size in kb)\n", tr_buf_size / 1024);
		//scanf("%d", &sel_tr_buf_size);
		//if (sel_tr_buf_size == 0)
		//	sel_tr_buf_size = tr_buf_size;
		//else
			//sel_tr_buf_size *= 1024;

		//printf("\nSet periodic report interval (0 is %u [DEFAULT], other values for the interval in Mbyte)\n", (unsigned int)PRINT_EVERY_N_MBYTES_DEFAULT);
		//scanf("%u", &periodic_report_interval);
		//if (periodic_report_interval == 0)
			//periodic_report_interval = PRINT_EVERY_N_MBYTES_DEFAULT;
	//}

	//samples_per_record_d = (unsigned int)(((double)average_rate_per_channel * 1024.0 * 1024.0) / ((double)trig_freq * 2.0));
	//samples_per_record = ((((unsigned int)samples_per_record_d + 31) / 32) * 32);

	//trig_period = (unsigned int)(sampleratehz / (double)trig_freq);
	printf(" Trigger period calculated to %u clock-cycles.\n", trig_period);
	CHECKADQ(ADQ_SetTriggerMode(adq_cu, adq_num, trig_mode));
	CHECKADQ(ADQ_SetInternalTriggerPeriod(adq_cu, adq_num, trig_period));

	expected_data_prod_rate = 0.0;
	for (ch = 0; ch < 4; ch++)
	{
		if (((1 << ch) & channelsmask))
		{
			expected_data_prod_rate += (trig_freq * samples_per_record * 2); // bytes per second
		}
	}


	printf("-------------------------------------------------------------------------\n");
	printf("Transfer parameters:\n");
	if (max_run_time == -1)
		printf(" Run time                                  : INFINITE.\n");
	else
		printf(" Run time                                  : %8u seconds (%.4f hours).\n", max_run_time, (double)max_run_time / 60.0 / 60.0);
	printf(" Kernel buffers                            : %8u buffers.\n", sel_tr_buf_no);
	printf(" Kernel buffer size (for each)             : %8.3f Mbytes.\n", (double)sel_tr_buf_size / 1024.0 / 1024.0);
	printf(" Kernel buffer allocation (total)          : %8.3f Mbytes.\n", (double)sel_tr_buf_size * (double)sel_tr_buf_no / 1024.0 / 1024.0);
	printf(" Trigger frequency                         : %8u Hz.\n", (unsigned int)trig_freq);
	printf(" Samples per record                        : %8u samples.\n", (unsigned int)samples_per_record);
	printf(" Number of channels to use                 : %8u channel(s).\n", use_nof_channels);
	printf(" Desired data rate for test                : %8.2f Mbyte/s.\n", (double)test_speed_mbyte);
	printf(" Expected interface max performance        : %8.2f Mbyte/s.\n", (double)expected_transfer_performance_in_mbytes);
	printf(" Calculated expected data rate (effective) : %8.2f Mbyte/s.\n", expected_data_prod_rate / 1024.0 / 1024.0);
	printf("-------------------------------------------------------------------------\n");

	if ((expected_data_prod_rate / 1024.0 / 1024.0) > (double)expected_transfer_performance_in_mbytes * 1.01)
	{
		printf("WARNING: Test is expected to FAIL. Test exceeds expected maximum performance capacity.\n");
	}
	else if ((expected_data_prod_rate / 1024.0 / 1024.0) > (double)expected_transfer_performance_in_mbytes * 0.95)
	{
		printf("WARNING: Test is expected to perhaps fail. Test is on the very limit of the expected performance capacity.\n");
	}
	else if ((expected_data_prod_rate / 1024.0 / 1024.0) > 1500)
	{
		printf("WARNING: Test may not pass OK.\n         Effective speeds higher than 1.5GByte/sec puts a lot of requirements on system performance.\n");
	}
	else
	{
		printf("NOTE: Test is expected to pass OK.\n");
	}
	printf("-------------------------------------------------------------------------\n");


	nof_records = -1; // Run infinite mode

	timeout_ms = 5000; // Default to 5 seconds timeout

	// Allocate memory
	for (ch = 0; ch < 4; ch++) {
		if (!((1 << ch) & channelsmask))
			continue;
		target_buffers[ch] = (short int*)malloc((size_t)sel_tr_buf_size);
		if (!target_buffers[ch]) {
			printf("Failed to allocate memory for target_buffers\n");
			goto error;
		}
		target_headers[ch] = (struct ADQRecordHeader*)malloc((size_t)sel_tr_buf_size);
		if (!target_headers[ch]) {
			printf("Failed to allocate memory for target_headers\n");
			goto error;
		}
		target_buffers_extradata[ch] = (short int*)malloc((size_t)(sizeof(short) * samples_per_record));
		if (!target_buffers_extradata[ch]) {
			printf("Failed to allocate memory for target_buffers_extradata\n");
			goto error;
		}
	}

	// Allocate memory for record data (used for ProcessRecord function template)
	record_data = (short int*)malloc((size_t)(sizeof(short) * samples_per_record));
	if (!record_data) {
		printf("Failed to allocate memory for record data\n");
		goto error;
	}

	// Compute the sum of the number of records specified by the user
	for (ch = 0; ch < 4; ++ch) {
		if (!((1 << ch) & channelsmask))
			continue;
		nof_records_sum += nof_records;
	}


	pretrig_samples = 0;
	triggerdelay_samples = 0;

	CHECKADQ(ADQ_SetTestPatternMode(adq_cu, adq_num, 0)); // Disable test pattern

	// Use triggered streaming for data collection.
	CHECKADQ(ADQ_TriggeredStreamingSetup(adq_cu, adq_num,
		nof_records,
		samples_per_record,
		pretrig_samples,
		triggerdelay_samples,
		channelsmask));

	//printf("\nPress ENTER to start test.\n");
	//getchar();
	//getchar();

	// Commands to start the triggered streaming mode after setup
	CHECKADQ(ADQ_SetStreamStatus(adq_cu, adq_num, 1));
	CHECKADQ(ADQ_SetTransferBuffers(adq_cu, adq_num, sel_tr_buf_no, sel_tr_buf_size));
	CHECKADQ(ADQ_StopStreaming(adq_cu, adq_num));

	tr_bytes = 0; // Reset data bytes counter
	timer_start(1); // Start timer
	timer_start(3); // Start timer
	CHECKADQ(ADQ_StartStreaming(adq_cu, adq_num));
	// When StartStreaming is issued device is armed and ready to accept triggers

	// Collection loop
	do {
		buffers_filled = 0;
		success = 1;

		if (ADQ_GetStreamOverflow(adq_cu, adq_num) > 0) {
			printf("\n***********************************************\n[ERROR]  Streaming overflow detected...\n");
			transfer_error = 1;
			goto error;
		}

		// Wait for one or more transfer buffers (polling)
		//printf("W");
		CHECKADQ(ADQ_GetTransferBufferStatus(adq_cu, adq_num, &buffers_filled));

		// Do the following read-out only once every half second to avoid impacting performance
		if (timer_time_ms(3) > 500)
		{
			CHECKADQ(ADQ_GetWriteCountMax(adq_cu, adq_num, &dram_wrcnt_max));
			dram_fill_percentage = (100.0 * ((double)(adq_dram_size_per_count * (unsigned long long)dram_wrcnt_max) / (double)(adq_dram_size_bytes)));

			if (dram_fill_percentage > 25.0)
			{
				printf("\n[NOTE]    Significant DRAM usage.\n");
			}
			else if (dram_fill_percentage > 75.0)
			{
				printf("\n[WARNING] High DRAM usage.\n");
			}
			timer_start(3); // Re-start timer
		}

		// Poll for the transfer buffer status as long as the timeout has not been
		// reached and no buffers have been filled.
		while (!buffers_filled)
		{
			// Mark the loop start
			timer_start(2);
			while (!buffers_filled &&
				timer_time_ms(2) < timeout_ms) {
				CHECKADQ(ADQ_GetTransferBufferStatus(adq_cu, adq_num, &buffers_filled));
				CHECKADQ(ADQ_GetWriteCountMax(adq_cu, adq_num, &dram_wrcnt_max));
				// Sleep to avoid loading the processor too much
				Sleep(1);
			}

			// Timeout reached, flush the transfer buffer to receive data
			if (!buffers_filled) {
				printf("\n[NOTE]   Timeout, flushing DMA...\n");
				CHECKADQ(ADQ_FlushDMA(adq_cu, adq_num));
			}
		}


		if (buffers_filled >= (sel_tr_buf_no - 1))
		{
			printf("\n[WARNING] Maximum buffer fill level detected.\n");
		}
		else if ((sel_tr_buf_no > 4) && (buffers_filled >= (sel_tr_buf_no - 3)))
		{
			printf("\n[WARNING] High buffer fill level detected.\n");
		}

		if (buffers_filled > max_used_tr_buf)
			max_used_tr_buf = buffers_filled;


		while (buffers_filled > 0)
		{
			CHECKADQ(ADQ_GetDataStreaming(adq_cu, adq_num,
				(void**)target_buffers,
				(void**)target_headers,
				channelsmask,
				samples_added,
				headers_added,
				header_status));
			nof_buffers_fetched++;
			buffers_filled--;

			if (parse_mode == 1)
			{
				for (ch = 0; ch < 4; ++ch)
				{
					if (!((1 << ch) & channelsmask))
						continue;

					if (headers_added[ch] > 0)
					{
						if (header_status[ch])
							headers_done = headers_added[ch];
						else
							headers_done = headers_added[ch] - 1;

						// If there is at least one complete header
						records_completed[ch] += headers_done;
					}

					// Parse the added samples
					// Parse the added samples
					if (samples_added[ch] > 0)
					{
						samples_remaining = samples_added[ch];

						// Handle incomplete record at the start of the buffer
						if (samples_extradata[ch] > 0)
						{
							if (headers_done == 0)
							{
								// There is not enough data in the transfer buffer to complete
								// the record. Add all the samples to the extradata buffer.
								memcpy(&target_buffers_extradata[ch][samples_extradata[ch]],
									target_buffers[ch],
									sizeof(short) * samples_added[ch]);
								samples_remaining -= samples_added[ch];
								samples_extradata[ch] += samples_added[ch];
							}
							else
							{
								// There is enough data in the transfer buffer to complete
								// the record. Add RecordLength-samples_extradata samples

								// Move data to record_data
								memcpy((void*)record_data, target_buffers_extradata[ch], sizeof(short) * samples_extradata[ch]);
								memcpy((void*)(record_data + samples_extradata[ch]), target_buffers[ch], sizeof(short) * (target_headers[ch][0].RecordLength - samples_extradata[ch]));

								samples_remaining -= target_headers[ch][0].RecordLength -
									samples_extradata[ch];
								samples_extradata[ch] = 0;

								adq_process_record(record_data, &target_headers[ch][0]);
#ifdef VERBOSE
								printf("Completed record %u on channel %u, %u samples.\n",
									target_headers[ch][0].RecordNumber, ch,
									target_headers[ch][0].RecordLength);
#endif
							}
						}
						else
						{
							if (headers_done == 0)
							{
								// The samples in the transfer buffer begin a new record, this
								// record is incomplete.
								memcpy(target_buffers_extradata[ch],
									target_buffers[ch],
									sizeof(short) * samples_added[ch]);

								samples_remaining -= samples_added[ch];
								samples_extradata[ch] = samples_added[ch];
							}
							else
							{
								// The samples in the transfer buffer begin a new record, this
								// record is complete.

								// Copy data to record buffer
								memcpy((void*)record_data, target_buffers[ch], sizeof(short) * target_headers[ch][0].RecordLength);
								samples_remaining -= target_headers[ch][0].RecordLength;

								adq_process_record(record_data, &target_headers[ch][0]);
#ifdef VERBOSE
								printf("Completed record %u on channel %u, %u samples.\n",
									target_headers[ch][0].RecordNumber, ch,
									target_headers[ch][0].RecordLength);
#endif
							}
						}
						// At this point: the first record in the transfer buffer or the entire
						// transfer buffer has been parsed.

						// Loop through complete records fully inside the buffer
						for (int i = 1; i < headers_done; ++i)
						{
							// Copy data to record buffer
							memcpy((void*)record_data, (&target_buffers[ch][samples_added[ch] - samples_remaining]), sizeof(short) * target_headers[ch][i].RecordLength);

							samples_remaining -= target_headers[ch][i].RecordLength;

							adq_process_record(record_data, &target_headers[ch][i]);
#ifdef VERBOSE
							printf("Completed record %u on channel %u, %u samples.\n",
								target_headers[ch][i].RecordNumber, ch,
								target_headers[ch][i].RecordLength);
#endif
						}

						if (samples_remaining > 0)
						{
							// There is an incomplete record at the end of the transfer buffer
							// Copy the incomplete header to the start of the target_headers buffer
							memcpy(target_headers[ch],
								&target_headers[ch][headers_done],
								sizeof(struct ADQRecordHeader));

							// Copy any remaining samples to the target_buffers_extradata buffer,
							// they belong to the incomplete record
							memcpy(target_buffers_extradata[ch],
								&target_buffers[ch][samples_added[ch] - samples_remaining],
								sizeof(short) * samples_remaining);
							// printf("Incomplete at end of transfer buffer. %u samples.\n", samples_remaining);
							// printf("Copying %u samples to the extradata buffer\n", samples_remaining);
							samples_extradata[ch] = samples_remaining;
							samples_remaining = 0;
						}
					}

				}

				// Update received_all_records
				nof_received_records_sum = 0;
				for (ch = 0; ch < 4; ++ch)
					nof_received_records_sum += records_completed[ch];

				// Determine if collection is completed
				received_all_records = (nof_received_records_sum >= nof_records_sum);
			}
		}
		end_condition = ((timer_time_ms(1) / 1000) > (unsigned int)max_run_time) && (max_run_time != -1);
	} while (!received_all_records && !end_condition);


	if (success)
	{
		printf("\n\nDone.\n\n");
		CHECKADQ(ADQ_GetWriteCountMax(adq_cu, adq_num, &dram_wrcnt_max));
	}
	else
		printf("\n\nError occurred.\n");

error:
	for (ch = 0; ch < nof_channels; ch++)
	{
		ADQ_GetStreamErrors(adq_cu, adq_num, ch + 1, &stream_error[ch]);
	}
	CHECKADQ(ADQ_StopStreaming(adq_cu, adq_num));


	// Report some final stats.
	time_diff = timer_time_ms(1) - time_stamped;
	time_stamped = timer_time_ms(1);
	tr_speed_now = ((double)tr_bytes_since_last_print / (1024.0 * 1024.0)) / ((double)time_diff / 1000.0);
	tr_speed = ((double)tr_bytes / (1024.0 * 1024.0)) / ((double)time_stamped / 1000.0);
	tr_speed_raw = ((double)nof_buffers_fetched * (double)sel_tr_buf_size / (1024.0 * 1024.0)) / ((double)time_stamped / 1000.0);
	printf("-----------------------------------------------------------------------\n");
	printf("Closing up stats.\n");
	if (transfer_error > 0)
		printf(" [RESULT] Test FAILED. Transfer errors occurred.\n");
	else
		printf(" [RESULT] Test OK. All data transferred without detected problems.\n");

	//if (parse_mode == 3)
	//	printf(" Selected mode is RAW mode. Data will not be parsed into records.\n");
	//else if (parse_mode == 2)
	//	printf(" Selected mode is non-user parsed mode. Data will not be parsed into records.\n");
	//else
	//	printf(" Selected mode is full parsed mode.\n");

	printf(" Transfer speed actual effective (%12llu Mbyte, %9d ms, %8.2f Mbyte/sec)\n", tr_bytes_since_last_print / 1024 / 1024, time_diff, tr_speed_now);
	printf(" Transfer speed total effective  (%12llu Mbyte, %9u ms, %8.2f Mbyte/sec)\n", tr_bytes / 1024 / 1024, time_stamped, tr_speed);
	printf(" Transfer speed total raw        (%12llu Mbyte, %9u ms, %8.2f Mbyte/sec)\n", (nof_buffers_fetched * (unsigned long long)sel_tr_buf_size) / 1024 / 1024, time_stamped, tr_speed_raw);
	if (tr_speed_raw > 0.0)
	{
		printf(" Overhead is in total                = %8.4f%%.", 100.0 * (1.0 - tr_speed / tr_speed_raw));
		if (parse_mode == 1)
			printf("\n");
		else
			printf(" (N/A for RAW/no-parse modes).\n");
	}
	if (expected_data_prod_rate > 0.0)
		printf(" Expected data production rate       = %8.4f MByte/sec\n", expected_data_prod_rate / 1024.0 / 1024.0);
	printf(" Max DRAM Fill Level reported        = %8.4f%% (cnt=%8u)\n", 100.0 * (double)(adq_dram_size_per_count * (unsigned long long)dram_wrcnt_max) / (double)(adq_dram_size_bytes), dram_wrcnt_max);
	printf(" Max number of transfer buffers used = %4u/%4u.\n", max_used_tr_buf, sel_tr_buf_no);
	printf(" Total number of records received    = %12llu", nof_received_records_sum);
	if (parse_mode == 1)
		printf("\n");
	else
		printf(" (N/A for RAW/no-parse modes).\n");
	printf(" GetStreamError for \n");
	for (ch = 0; ch < nof_channels; ch++)
	{
		printf("                   channel %1u = %08X (", ch + 1, stream_error[ch]);
		if (!((1 << ch) & channelsmask))
			printf("channel not used)\n");
		else
			printf("active)\n");
	}
	printf("------------------------------------------------------------------------\n");

	for (ch = 0; ch < 4; ch++) {
		if (target_buffers[ch])
			free(target_buffers[ch]);
		if (target_headers[ch])
			free(target_headers[ch]);
		if (target_buffers_extradata[ch])
			free(target_buffers_extradata[ch]);
	}

	return;
}



