#define _CRT_SECURE_NO_WARNINGS // This define removes warnings for printf
#define _CRT_SECURE_NO_DEPRECATE

/*-----------------------------------------------------------------------------------
	功能：对 targetbuffer内存池、head动态分配、【head,buffer】写到同一文件中，做切分

	【已验证正确】
-----------------------------------------------------------------------------------------*/
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
#include <stdio.h>
#include "setting.h"
#include <io.h>
#include <stdio.h>
#include <direct.h>

#include "../ADQAPI/ADQAPI.h"
#include "../ADQAPI/os.h"
#include <time.h>
#define DIV_FILE_DATA "F:Data/data"	//分文件的路径。 "./data/data"

#define CellSizeThreshold  0  //区分细胞大小的阈值
#define FILENAME_RECORD_big  "F:record/record"  //把targetbuffer分成record，存于此文件夹下
#define FILENAME_RECORD_small  "F:record/record"//把targetbuffer分成record，存于此文件夹下

using namespace std;
clock_t tref[20];

void timer_start(unsigned int index) {
	tref[index] = clock();
	if (tref[index] < 0) {
		printf("\nFailed to start timer.");
		return;
	}
}

double timer_time_ms(unsigned int index) {
	clock_t t = clock();
	if (t < 0) {
		printf("\nFailed to get system time.");
		return -1;
	}
	return ((double)(t - tref[index]) * 1000 / CLOCKS_PER_SEC);
}
#define data_total  50
#define record_lenth_estimate 5*1024*1024  //80k字节  需要比每个细胞大小的文件要大
short* target_buffers_extradata = NULL;
short* record = NULL;
int samples_extradata = 0;
int samples_remaining = 0;
int samples_add = 0;
int cell_all = 0;


int total_file_num(char* root)
{
	struct _finddata_t file;
	intptr_t   hFile;

	int total_file = 1;

	if (_chdir(root))
	{
		printf("打开文件夹失败: %s\n", root);
		return 1;
	}

	hFile = _findfirst("*.bin", &file);
	while (_findnext(hFile, &file) == 0)
	{
		total_file++;
	}
	printf("总有文件  %d  个\n", total_file);
	_findclose(hFile);
	return total_file;
}
long get_len(FILE* fp)
{
	fseek(fp, 0, SEEK_END);
	long ssize = ftell(fp);
	rewind(fp);
	return ssize;
}
static short* read_DATA(int tBuf_num)
{
	char filename[256] = "";
	FILE* fp = NULL;
	short* pread;
	sprintf(filename, "%s_r%d.bin", DIV_FILE_DATA, tBuf_num);

	fp = fopen(filename, "rb");
	if (fp == NULL) { printf("Failed to open the file '%s' .\n", filename); return NULL; }

	long lsize = get_len(fp);
	pread = (short*)malloc(lsize);
	if (pread == NULL) { printf("there is no memory!!!!!!!!!!\n"); fclose(fp); return NULL; }
	fread(pread, 2, lsize / 2, fp);
	fclose(fp);
	//samples_add = lsize / 2;//buffer里面有效的点数
	return pread;
}

static int write_record(short* tBuf, ADQRecordHeader* rhead, int NextCellSize)
{
	char filename[256] = "";
	FILE* fp = NULL;
	int written = 0;
	/// /////////////////

	if ((rhead->GeneralPurpose1 == rhead->GeneralPurpose0) && (NextCellSize > CellSizeThreshold))
		sprintf(filename, "%s_r%d_c%d.bin", FILENAME_RECORD_big, rhead->RecordNumber, rhead->GeneralPurpose1);//写数据
	else if (rhead->GeneralPurpose1 > CellSizeThreshold)
		sprintf(filename, "%s_r%d_c%d.bin", FILENAME_RECORD_big, rhead->RecordNumber, rhead->GeneralPurpose1);//写数据
	else
		sprintf(filename, "%s_r%d_c%d.bin", FILENAME_RECORD_small, rhead->RecordNumber, rhead->GeneralPurpose1);//写数据

	//sprintf(filename, "%s_r%d_c%d.bin", FILENAME_RECORD, rhead->RecordNumber,rhead->GeneralPurpose1);//写数据

	fp = fopen(filename, "wb");
	if (fp == NULL)
	{
		printf("Failed to open the file '%s' for writing.\n", filename);
		return 0;
	}
	written = fwrite(tBuf, 2, rhead->RecordLength, fp);//写多少个点
	if (written != rhead->RecordLength)
	{
		printf("Failed to write %u bytes to the file '%s', wrote %zu bytes.\n", rhead->RecordLength * 2, filename, written);
		fclose(fp);
		return 0;
	}
	fclose(fp);

	return 1;
}

static int read_HEAD(short* tBuf, int tBuf_num, int head_file_num)
{
	short* alt_Buf = tBuf;
	int64_t alt_file_inform[2];
	memcpy(alt_file_inform, alt_Buf, 16);

	int cell_count = alt_file_inform[1] / 40;
	ADQRecordHeader* head_address = NULL;
	head_address = (ADQRecordHeader*)malloc(alt_file_inform[1]);
	if (head_address == NULL) { printf("head_address malloc error\n "); return -1; }
	memcpy(head_address, alt_Buf + 8, alt_file_inform[1]);

	short* buf_address = NULL;
	buf_address = (short*)malloc(alt_file_inform[0]);
	if (buf_address == NULL) { printf("buf_address malloc error\n "); return -1; }
	memcpy(buf_address, alt_Buf + 8 + alt_file_inform[1] / 2, alt_file_inform[0]);
	samples_add = alt_file_inform[0] / 2;//buffer里面有效的点数
	//char filename[256] = "";
	//FILE* fp = NULL;

	//sprintf(filename, "%s_r%d.bin", FILENAME_HEAD, head_file_num);

	//fp = fopen(filename, "rb");
	//if (fp == NULL) { printf("Failed to open the file '%s'.\n", filename); return -1; }
	//long file_bytes = get_len(fp);
	//cell_count = file_bytes / (sizeof(ADQRecordHeader));


		//到这里为止得到了heads,buf,smp_remain(buf里面多少个点)
	samples_remaining = samples_add;
	if (samples_extradata > 0)
	{
		memcpy(record,
			target_buffers_extradata,
			sizeof(short) * samples_extradata);
		memcpy(record + samples_extradata, buf_address, sizeof(short) * (head_address[0].RecordLength - samples_extradata));

		samples_remaining -= head_address[0].RecordLength - samples_extradata;
		samples_extradata = 0;
		head_address[0].RecordNumber = cell_all;
		write_record(record, &head_address[0], head_address[1].GeneralPurpose1);
		cell_all += 1;
		//printf("aaaa%d\n", cell_all);
	}
	else
	{
		memcpy(record, buf_address, sizeof(short) * head_address[0].RecordLength);
		samples_remaining -= head_address[0].RecordLength;

		write_record(record, &head_address[0], head_address[1].GeneralPurpose1);
		cell_all += 1;
	}
	for (int i = 1; i < cell_count; ++i)
	{
		memcpy(record,
			(&buf_address[samples_add - samples_remaining]),
			sizeof(short) * head_address[i].RecordLength);

		samples_remaining -= head_address[i].RecordLength;
		if (i == cell_count - 1)
			write_record(record, &head_address[i], head_address[i].GeneralPurpose1);
		else
			write_record(record, &head_address[i], head_address[i + 1].GeneralPurpose1);
		cell_all += 1;
		//printf("  %d  %d  %d\n", samples_remaining,i, cell_all);
	}
	if (samples_remaining > 0)
	{
		memcpy(target_buffers_extradata,
			&buf_address[samples_add - samples_remaining],
			sizeof(short) * samples_remaining);
		samples_extradata = samples_remaining;
		samples_remaining = 0;
	}
	free(tBuf);
	tBuf = NULL;
	free(buf_address); buf_address = NULL;
	free(head_address);
	head_address = NULL;
	return 1;
}
int write_file(double* data, int count, int mode)
{
	char filename[256];
	sprintf(filename, "./data/delay_div_r%d.txt", mode);

	FILE* fp = NULL;
	size_t written = 0;

	fp = fopen(filename, "w");
	if (fp == NULL)
	{
		printf("Failed to open the file '%s' for writing.\n", filename);
		return -1;
	}
	char  b[200];
	int mmmm = 0;
	for (int i = 0; i < count; i++)
	{
		mmmm = sprintf(b, "%.2f\n", data[i]);
		fwrite(b, 1, mmmm, fp);
		//printf("is  %s\n", b);
	}

	//fwrite(speed_add, 4, count, fp);
	fclose(fp);
	return 0;
}

int main()
{
	target_buffers_extradata = (short*)malloc(record_lenth_estimate);
	record = (short*)malloc(record_lenth_estimate);
	int num = 0;
	if (data_total == 0)
	{
		char aaa[100] = FILENAME_DATA_total;
		num = total_file_num(aaa);
	}
	else num = data_total;

	double* delay_div_time = (double*)malloc(sizeof(double) * 1000);
	int delay_div_time_num = 0;

	for (int i = 0; i < num; i++)
	{
		timer_start(1);
		short* data = read_DATA(i);
		read_HEAD(data, i, i);
		delay_div_time[delay_div_time_num] = timer_time_ms(1);
		delay_div_time_num++;
	}
	write_file(delay_div_time, delay_div_time_num, 0);

	free(delay_div_time);
	return 0;
}

