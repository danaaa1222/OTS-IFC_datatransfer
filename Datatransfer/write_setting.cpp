#define _CRT_SECURE_NO_WARNINGS // This define removes warnings for printf
#define _CRT_SECURE_NO_DEPRECATE

/*-----------------------------------------------------------------------------------
	功能：写参数setting.h到文件《setting.txt》中

-----------------------------------------------------------------------------------------*/
#include <iostream>
#include <stdio.h>
#include "setting.h"
using namespace std;
#define file_dir "./data/setting"

int write_setting_tofile()
{
	char set[512] = "";
	sprintf(set,
		"trig_frequency____________%d\nsamples_per_record_________%d\nmax_run_time_________%d\nsel_tr_buf_no_________%d\nsel_tr_buf_size_________%d\nREDUCE_PULSE_________%d\nCONTORL_INVERSE_________%d\nCELL_EXTEND_PERIOD_________%d\nPulse_threhold_________%d\nCELL_COMEING_THRESHOLD_________%d\nCONTORL_LATENCY_________%d\nRemove_value _________%d\nNUM_OF_SAMPLE_IN_PULSE _________%d\nPOSITION_OF_PULSE_END _________%d\n ",
		trig_frequency, samples_per_record, max_run_time, sel_tr_buf_no, sel_tr_buf_size, REDUCE_PULSE, CONTORL_INVERSE, CELL_EXTEND_PERIOD, Pulse_threhold, CELL_COMEING_THRESHOLD, CONTORL_LATENCY, Remove_value, NUM_OF_SAMPLE_IN_PULSE, POSITION_OF_PULSE_END);//写数据

	char filename[256] = "";
	FILE* fp = NULL;
	int written = 0;

	sprintf(filename, "%s.txt", file_dir);//写数据

	fp = fopen(filename, "wb");
	if (fp == NULL)
	{
		printf("Failed to open the file '%s' for writing.\n", filename);
		return 0;
	}
	written = fwrite(set, 1, sizeof(set), fp);//写多少个点

	fclose(fp);

	return 1;
}

//void main()
//{
//	if(write_setting_tofile())
//		printf("writing setting.h sucess!\n");
//	else printf("writing setting.h error!\n");

//}