#define _CRT_SECURE_NO_WARNINGS // This define removes warnings for printf
#define _CRT_SECURE_NO_DEPRECATE

#include <iostream>
#include <fstream>
#include <thread>
#include <ctime> 
#include <string>
#include <cstdlib>
#include <sstream>
#include <cstring>
#include<windows.h>
#include <stdio.h>
#include "adq_transfer_test.h"
#include "setting.h"
#include "function_used.cpp"
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
clock_t tref[20];
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

unsigned int periodic_report_interval = PRINT_EVERY_N_MBYTES_DEFAULT;

double expected_data_prod_rate = -1.0;
unsigned long long tr_bytes = 0;
unsigned long long tr_bytes_since_last_print = 0;
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
#include <cstring>//必须引用
#define consume  4  //16
short* starget_buffers[consume];
struct ADQRecordHeader* starget_headers[consume];
int s_flag[consume];
//===========================================
struct nodes
{
	short* data_address = NULL;
	struct ADQRecordHeader* head_address = NULL;
	int64_t data_bytes;
	int64_t header_number;
	int64_t file_number;
};
nodes alt_node[consume];

struct file_inform
{
	int64_t data_bytes;
	int64_t header_bytes;
};

int count_record = 0;
int popOver = 1;
double* speed_store;
int count_speed = 0;

void adq_perform_transfer_test(void* adq_cu, int adq_num, int adq_type, nodes* information_node);
double* MaxDRAMFillLevel;// %8.4f%%
double* maxbufferused;// %4u
void* produceItem(void* adq_cu, int adq_num, int adq_type, nodes* information_node)//adq_data_tsfer
{
	adq_perform_transfer_test(adq_cu, adq_num, adq_type, information_node);
	printf("produceItem end\n");
	popOver = 0;
	return((void*)0);
}
int effect_time_write = 0;
static int write_header_record_to_file(ADQRecordHeader* phead, size_t headnum, void* pBuf, size_t datasize, int record) 
{
	timer_start(15);
	file_inform* alt_file_inform = (file_inform*)malloc(sizeof(file_inform));
	if (alt_file_inform == NULL)
	{
		printf("alt_file_inform error\n");
		return -1;
	}
	alt_file_inform->data_bytes = datasize * 2;
	alt_file_inform->header_bytes = sizeof(ADQRecordHeader) * headnum;

	char filename[256] = "";
	size_t bytes_written = 0;
	sprintf(filename, "%s_r%d.bin", FILENAME_DATA, record);
	fstream file(filename, ios::out | ios::binary);

	file.write(reinterpret_cast<char*>(alt_file_inform), sizeof(file_inform));

	file.write(reinterpret_cast<char*>(phead), sizeof(ADQRecordHeader) * headnum);
	file.write(reinterpret_cast<char*>(pBuf), datasize * 2);
	file.close();
	free(alt_file_inform);
	alt_file_inform = NULL;
	tr_bytes = tr_bytes + datasize * 2 + sizeof(ADQRecordHeader) * headnum;
	tr_bytes_since_last_print = tr_bytes_since_last_print + datasize * 2 + sizeof(ADQRecordHeader) * headnum;
	time_diff = timer_time_ms(1) - time_stamped;

	if (time_diff > 999)
	{
		time_stamped = timer_time_ms(1);
		tr_speed_now = ((double)tr_bytes_since_last_print / (1024.0 * 1024.0)) / ((double)time_diff / 1000.0);
		tr_speed = ((double)tr_bytes / (1024.0 * 1024.0)) / ((double)time_stamped / 1000.0);
		speed_store[count_speed] = tr_speed_now;
		MaxDRAMFillLevel[count_speed] = 100.0 * (double)(adq_dram_size_per_count * (unsigned long long)dram_wrcnt_max) / (double)(adq_dram_size_bytes);
		maxbufferused[count_speed] = max_used_tr_buf;
		count_speed++;
		tr_bytes_since_last_print = 0;
	}
	effect_time_write += timer_time_ms(15);
	return 0;
}
int write_file(double* data, int count, int mode)
{
	char filename[256];
	if (mode == 1)
	{
		strcpy(filename, "./data/speed.txt");
	}
	else if (mode == 2)
	{
		strcpy(filename, "./data/dram.txt");
	}
	else if (mode == 3)
	{
		strcpy(filename, "./data/transbuffer.txt");
	}
	FILE* fp = NULL;
	size_t written = 0;

	fp = fopen(filename, "w");
	if (fp == NULL)
	{
		printf("Failed to open the file '%s' for writing.\n", filename);
		return -1;
	}
	char  b[200];
	for (int i = 0; i < count; i++)
	{
		sprintf(b, "%8.4f\n", data[i]);
		fwrite(b, 1, 10, fp);
		//printf("is  %s\n", b);
	}
	fclose(fp);
	return 0;
}

unsigned int mnn_thread[consume];
unsigned int mnn_thread_w[consume];
void* consumeItem1(int ID, nodes* information)
{
	mnn_thread[ID] = 0;
	mnn_thread_w[ID] = 0;
	while (popOver)
	{
		if (s_flag[ID] == 0)
		{
			write_header_record_to_file(information[ID].head_address, information[ID].header_number, information[ID].data_address, information[ID].data_bytes / 2, information[ID].file_number);//-写细胞数据
			s_flag[ID] = 1;
			mnn_thread_w[ID]++;
		}
		mnn_thread[ID]++;
	}
	popOver = 0;
	printf("consumeItem %d end\n", ID);
	printf("The efficiency of thread [%d] is %d / %d\n", ID, mnn_thread_w[ID], mnn_thread[ID]);
	return((void*)0);
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
	set_parameters_toFPGA(adq_cu, adq_num);

	if (WHETHER_USE_EXTERNAL_CLOCK == 1)
	{
		if (!ADQ_SetClockSource(adq_cu, adq_num, ADQ_CLOCK_EXT)) //ADQ_CLOCK_EXT  ADQ_CLOCK_INT_EXTREF  ADQ_CLOCK_INT_RESERVED
		{
			printf("Set External Clock Source Failed.\n");
			//return -1;
		}
		else
			printf("Set External Clock Source Successfully.\n");
		printf("The Clock Source is External.\n");

	}
	else
		printf("The Clock Source is Internal.\n");
	//--------------------------------------------------------------------------------------------------------------
	unsigned int asizem = sel_tr_buf_size * filefactor;

	speed_store = (double*)malloc(1024 * 4);
	MaxDRAMFillLevel = (double*)malloc(1024 * 4);
	maxbufferused = (double*)malloc(1024 * 4);
	//Allocate memory
	for (int mmm = 0; mmm < consume; mmm++) {
		s_flag[mmm] = 1;//1空闲
		starget_buffers[mmm] = (short*)malloc(asizem);
		if (starget_buffers[mmm] == NULL) {
			printf("Failed to allocate memory for target_buffers\n");
		}
		starget_headers[mmm] = (struct ADQRecordHeader*)malloc(target_header_malloc_byte);
		if (!starget_headers[mmm]) {
			printf("Failed to allocate memory for target_headers\n");
		}
		printf("starget_buffers[%d]  %p target_headers[0] %p,asizem %d\n", mmm, starget_buffers[mmm], starget_headers[mmm], asizem);

	}

	thread t(produceItem, adq_cu, adq_num, adq_type, alt_node);
	thread t0(consumeItem1, 0, alt_node);
	thread t1(consumeItem1, 1, alt_node);
	thread t2(consumeItem1, 2, alt_node);
	thread t3(consumeItem1, 3, alt_node); 

	t.join();
	t0.join();
	t1.join();
	t2.join();
	t3.join();

	printf("The whole project is completed!\n");
	write_file(speed_store, count_speed, 1);
	write_file(MaxDRAMFillLevel, count_speed, 2);
	write_file(maxbufferused, count_speed, 3);
	printf("write_speed success!!!\n");
	free(speed_store);
	free(MaxDRAMFillLevel);
	free(maxbufferused);
	for (int ch = 0; ch < consume; ch++) {
		free(starget_buffers[ch]);
		free(starget_headers[ch]);
	}

}

int datasize = 0;
int headsize = 0;
int memsize = 0;
int memheadsize = 0;
void adq_perform_transfer_test(void* adq_cu, int adq_num, int adq_type, nodes* information_node)
{
	int ii = 0;
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

	unsigned int timeout_ms = 1000;
	int config_mode = 0;

	unsigned int nof_records_sum = 0;
	unsigned int received_all_records = 0;

	unsigned int nof_channels;
	int exit = 0;
	unsigned int ch = 0;
	unsigned int buffers_filled = 0;

	// Buffers to handle the stream output (both headers and data)
	short* target_buffers[4] = { NULL, NULL, NULL, NULL };
	struct ADQRecordHeader* target_headers[4] = { NULL, NULL, NULL, NULL };
	// Variables to handle the stream output (both headers and data)
	unsigned int header_status[4] = { 0, 0, 0, 0 };
	unsigned int samples_added[4] = { 0, 0, 0, 0 };
	unsigned int headers_added[4] = { 0, 0, 0, 0 };
	unsigned int samples_extradata[4] = { 0, 0, 0, 0 };
	//unsigned int samples_remaining;
	unsigned int headers_done = 0;

	unsigned int use_nof_channels;
	//	double samples_per_record_d;
	double average_rate_per_channel;
	double max_mbyte_per_second_per_channel;
	double dram_fill_percentage;
	// Bias ADC codes
	int adjustable_bias = 0;

	//struct ADQRecordHeader RawHeaderEmulation;

	nof_channels = ADQ_GetNofChannels(adq_cu, adq_num);

	trig_mode = ADQ_INTERNAL_TRIGGER_MODE;

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

	// Compute the sum of the number of records specified by the user
	for (ch = 0; ch < 4; ++ch) {
		if (!((1 << ch) & channelsmask))
			continue;
		nof_records_sum += nof_records;
	}


	pretrig_samples = 0;
	triggerdelay_samples = 0;

	CHECKADQ(ADQ_SetTestPatternMode(adq_cu, adq_num, 0)); // Disable testPoint pattern

	// Use triggered streaming for data collection.
	CHECKADQ(ADQ_TriggeredStreamingSetup(adq_cu, adq_num,
		nof_records,
		samples_per_record,
		pretrig_samples,
		triggerdelay_samples,
		channelsmask));

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

		//------------------------
		ch = 0;
		//printf("buffers_filled %d\n", buffers_filled);
		//------------------------
		while (buffers_filled > (filefactor-1))
		{
			if (s_flag[ii % consume] == 1)
			{
				//timer_start(6);
				//printf("ii %d,consume %d\n", ii, ii % consume);
				target_buffers[0] = starget_buffers[ii % consume];
				target_headers[0] = starget_headers[ii % consume];
				for (int k = 0; k < filefactor; k++)
				{
					//printf("datasize %d,headsize %d\n", datasize, headsize);
					target_buffers[0] += datasize;
					target_headers[0] += headsize;
					CHECKADQ(ADQ_GetDataStreaming(adq_cu, adq_num,
						(void**)target_buffers,
						(void**)target_headers,
						channelsmask,
						samples_added,
						headers_added,
						header_status));
					datasize = samples_added[0];
					//printf("target_buffers[0]  %p target_headers[0] %p,datasize %d\n", target_buffers[0], target_headers[0], datasize);
					if (header_status[0])
						headers_done = headers_added[0];
					else
						headers_done = headers_added[0] - 1;
					//printf("headers_added[ch] %d,headers_done %d\n", headers_added[0], headers_done);
					headsize = headers_done;
					memsize += samples_added[0];
					memheadsize += headers_done;
					nof_buffers_fetched++;
					buffers_filled--;
					//printf("buffers_filled %d\n", buffers_filled);
					// If there is at least one complete header
					records_completed[ch] += headers_done;
				}

				information_node[ii % consume].data_address = starget_buffers[ii % consume];
				information_node[ii % consume].data_bytes = sizeof(short) * memsize;
				information_node[ii % consume].file_number = count_record;
				information_node[ii % consume].head_address = starget_headers[ii % consume];
				information_node[ii % consume].header_number = memheadsize;
				s_flag[ii % consume] = 0;
				//printf("s_flag[%d] %d\n", ii % consume, s_flag[ii % consume]);
				datasize = 0;
				headsize = 0;
				memsize = 0;
				memheadsize = 0;
				count_record++;
				//printf("%d\n", timer_time_ms(6));
				ii++;
				nof_received_records_sum = 0;
				nof_received_records_sum += records_completed[ch];
				received_all_records = (nof_received_records_sum >= nof_records_sum);

			}
			else { ii++; if (ii > 12 || ii < 0)ii = 0; }
		}
		end_condition = ((timer_time_ms(1) / 1000) > (unsigned int)max_run_time) && (max_run_time != -1);
	} while (!received_all_records && !end_condition);


	if (success)
	{
		printf("\n\nDone.\n\n");
		printf("fetch data time %d ms record %d\n", timer_time_ms(1),count_record);
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
	popOver = 0;

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
	if (write_setting_tofile())
		printf("writing setting.h sucess!\n");
	else printf("writing setting.h error!\n");

	//for (ch = 0; ch < 4; ch++) {
	//	if (target_buffers[ch])
	//		free(target_buffers[ch]);
	//	if (target_headers[ch])
	//		free(target_headers[ch]);
	//}

	return;
}
