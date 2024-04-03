#define _CRT_SECURE_NO_WARNINGS // This define removes warnings for printf
#define _CRT_SECURE_NO_DEPRECATE


/*-----------------------------------------------------------------------------------
	功能：对 【head】【buffer】不同文件，但一一对应，做切分
	说明：【head】是一个targetbuffer的头，即head是小文件

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
#include<windows.h>
#include "concurrentqueue.h"
#include "blockingconcurrentqueue.h"
#include <stdio.h>
#include "adq_transfer_test.h"
#include "setting.h"
#include <io.h>
#include <stdio.h>
#include <direct.h>

#include "../ADQAPI/ADQAPI.h"
#include "../ADQAPI/os.h"
#include <time.h>
using namespace std;

#define data_total  30
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
	samples_add = lsize / 2;//buffer里面有效的点数
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
	int cell_count = 0;
	ADQRecordHeader* head_address = NULL;
	char filename[256] = "";
	FILE* fp = NULL;
	short* alt_Buf = tBuf;
	sprintf(filename, "%s_r%d.bin", FILENAME_HEAD, head_file_num);

	fp = fopen(filename, "rb");
	if (fp == NULL) { printf("Failed to open the file '%s'.\n", filename); return -1; }
	long file_bytes = get_len(fp);
	cell_count = file_bytes / (sizeof(ADQRecordHeader));
	head_address = (ADQRecordHeader*)malloc(file_bytes);
	if (head_address)
	{
		fread(head_address, sizeof(ADQRecordHeader), cell_count, fp);
		//到这里为止得到了heads,buf,smp_remain(buf里面多少个点)
		samples_remaining = samples_add;
		if (samples_extradata > 0)
		{
			memcpy(record,
				target_buffers_extradata,
				sizeof(short) * samples_extradata);
			memcpy(record + samples_extradata, alt_Buf, sizeof(short) * (head_address[0].RecordLength - samples_extradata));

			samples_remaining -= head_address[0].RecordLength - samples_extradata;
			samples_extradata = 0;
			head_address[0].RecordNumber = cell_all;
			write_record(record, &head_address[0], head_address[1].GeneralPurpose1);
			cell_all += 1;
			//printf("aaaa%d\n", cell_all);
		}
		else
		{
			memcpy(record, alt_Buf, sizeof(short) * head_address[0].RecordLength);
			samples_remaining -= head_address[0].RecordLength;

			write_record(record, &head_address[0], head_address[1].GeneralPurpose1);
			cell_all += 1;
		}
		for (int i = 1; i < cell_count; ++i)
		{
			memcpy(record,
				(&alt_Buf[samples_add - samples_remaining]),
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
				&alt_Buf[samples_add - samples_remaining],
				sizeof(short) * samples_remaining);
			samples_extradata = samples_remaining;
			samples_remaining = 0;
		}
		free(tBuf);
		tBuf = NULL;
	}
	else
		printf("head_address doesn't have enough memory!!!!!\n");
	fclose(fp);
	free(head_address);
	head_address = NULL;
	return 1;
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
	for (int i = 0; i < num; i++)
	{
		short* data = read_DATA(i);
		read_HEAD(data, i, i);
	}
	return 0;
}

