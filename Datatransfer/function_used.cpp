#define _CRT_SECURE_NO_WARNINGS // This define removes warnings for printf
#define _CRT_SECURE_NO_DEPRECATE

/*-----------------------------------------------------------------------------------
	功能：写参数setting.h到文件《setting.txt》中

-----------------------------------------------------------------------------------------*/
#include <iostream>
#include <stdio.h>
#include "setting.h"
#include "../ADQAPI/ADQAPI.h"
using namespace std;
#define file_dir "./data/setting"

int write_setting_tofile()
{
	char set[1024] = "";
	sprintf(set,
		"trig_frequency____________ %d\n"
		"samples_per_record_________ %d\n"
		"max_run_time_________ %d\n"
		"sel_tr_buf_no_________ %d\n"
		"sel_tr_buf_size_________ %d\n"
		"WHETHER_USE_EXTERNAL_CLOCK_________ % d\n"
		"SET_PULSENUM_IN_CELL_________ % d\n"
		"CONTORL_DATA_OUTPUT_8BIT_POS_________ % d\n"
		"CONTORL_DATA_OUTPUT_MOVE_BG_________ % d\n"		
		"CONTORL_DATA_OUTPUT_8BIT_________ % d\n"
		"SET_CUT_PULSE_WINDOWS_________ % d\n"
		"REDUCE_PULSE_________ % d\n"
		"CONTORL_INVERSE_________ % d\n"
		"CONTORL_LATENCY_________ % d\n"
		"CELL_EXTEND_PERIOD_________ % d\n"
		"REMOVE_VALUE _________ % d\n"
		"PULSE_THRESHOLD_________ % d\n"
		"CELL_COMEING_THRESHOLD_________ % d\n"
		"POS_OF_PULSE_ST_4_PULSEWIN_CELL _________ % d\n"
		"POS_OF_PULSE_END_4_CELL _________ % d\n",
		trig_frequency, samples_per_record, max_run_time, sel_tr_buf_no, sel_tr_buf_size, 
		WHETHER_USE_EXTERNAL_CLOCK, 
		SET_PULSENUM_IN_CELL,
		CONTORL_DATA_OUTPUT_8BIT_POS,
	    CONTORL_DATA_OUTPUT_MOVE_BG,
		CONTORL_DATA_OUTPUT_8BIT, SET_CUT_PULSE_WINDOWS,
		REDUCE_PULSE, CONTORL_INVERSE, CONTORL_LATENCY,
		CELL_EXTEND_PERIOD, REMOVE_VALUE,PULSE_THRESHOLD,CELL_COMEING_THRESHOLD,
		POS_OF_PULSE_ST_4_PULSEWIN_CELL,POS_OF_PULSE_END_4_CELL);//写数据

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

void set_parameters_toFPGA(void* adq_cu, int adq_num)
{
	unsigned int retval_u1_reg11;
	unsigned int retval_u1_reg12;
	unsigned int retval_u1_reg13;
	unsigned int retval_u2_reg10;
	unsigned int retval_u2_reg11;
	unsigned int retval_u2_reg12;
	unsigned int retval_u2_reg13;
	////unsigned int CellParam = 1 << 10 | 1 << 9 | 1<< 8 | 1 << 7 | 1 << 6 | 1<< 5 | 3 << 1;
	//unsigned int REMOVE_VALUE = 0x0000;  //符号也有用 负数也有用 
	//unsigned int PULSE_THRESHOLD = 0x0000;
	//unsigned int CELL_COMEING_THRESHOLD = 0x317A;
	//unsigned int POS_OF_PULSE_ST_4_PULSEWIN_CELL = 0x0001;
	//unsigned int POS_OF_PULSE_END_4_CELL = 0x0001;

	unsigned int Reg0x10 = SET_CUT_PULSE_WINDOWS << 26 | CONTORL_DATA_OUTPUT_8BIT << 23 | CONTORL_DATA_OUTPUT_MOVE_BG << 20 | CONTORL_DATA_OUTPUT_8BIT_POS << 16 | SET_PULSENUM_IN_CELL;
	ADQ_WriteUserRegister(adq_cu, adq_num, 2, 0x10, 0, Reg0x10, &retval_u2_reg10);

	unsigned int Reg0x11 = CELL_EXTEND_PERIOD << 20 | CONTORL_LATENCY << 14 | CONTORL_INVERSE << 8 | REDUCE_PULSE;
	ADQ_WriteUserRegister(adq_cu, adq_num, 1, 0x11, 0, Reg0x11, &retval_u1_reg11); //Only CELL_EXTEND_PERIOD and CONTORL_INVERSE is valid
	ADQ_WriteUserRegister(adq_cu, adq_num, 2, 0x11, 0, Reg0x11, &retval_u2_reg11);

	unsigned int Reg0x12 = PULSE_THRESHOLD << 16 | REMOVE_VALUE;
	ADQ_WriteUserRegister(adq_cu, adq_num, 1, 0x12, 0, Reg0x12, &retval_u1_reg12);
	ADQ_WriteUserRegister(adq_cu, adq_num, 2, 0x12, 0, Reg0x12, &retval_u2_reg12);

	unsigned int Reg0x13 = POS_OF_PULSE_END_4_CELL << 24 | POS_OF_PULSE_ST_4_PULSEWIN_CELL << 16 | CELL_COMEING_THRESHOLD;
	ADQ_WriteUserRegister(adq_cu, adq_num, 1, 0x13, 0, Reg0x13, &retval_u1_reg13);
	ADQ_WriteUserRegister(adq_cu, adq_num, 2, 0x13, 0, Reg0x13, &retval_u2_reg13);
	//printf("\nretval_u1_reg12 = %x\n", retval_u1_reg12);
	//printf("\nretval_u1_reg13 = %x\n", retval_u1_reg13);
	//printf("\nretval_u2_reg12 = %x\n", retval_u2_reg12);
	//printf("\nretval_u2_reg13 = %x\n", retval_u2_reg13);

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

	//unsigned int TargetSampleRate= 2500;   //MHz
	//if (!ADQ_SetTargetSampleRate(adq_cu, adq_num,0 ,ADQ_CLOCK_EXT))  //MHz Valid for (ADQ7 produced post 2021-04-01)
	//{
	//    printf("SetTargetSampleRate Failed.\n");
	//    return -1;
	//}

	//unsigned int ExternalReferenceFrequency = 10;   //MHz
	//if (!ADQ_SetExternalReferenceFrequency(adq_cu, adq_num, ExternalReferenceFrequency))
	//{
	//    printf("SetExternalReferenceFrequency Failed.\n");
	//    return -1;
	//}
}
//void main()
//{
//	if(write_setting_tofile())
//		printf("writing setting.h sucess!\n");
//	else printf("writing setting.h error!\n");
//
//}