#pragma once
#define FILENAME_HEAD "./head/head"    //需要单独写数据头的，写到head文件夹下
#define FILENAME_DATA "./data/data"	//采集targetbuffer存这里。
#define DIV_FILE_DATA "./blood4/data1/data"	//分文件的路径。
#define FILENAME_RECORD  "./record/record"  //使用trig_cell，把每个record写到record文件夹下
#define CellSizeThreshold  0  //区分细胞大小的阈值
#define FILENAME_RECORD_big  "./sample/record"  //把targetbuffer分成record，存于此文件夹下
#define FILENAME_RECORD_small  "./sample/record"//把targetbuffer分成record，存于此文件夹下

#define FILENAME_DATA_total "./data" //没有用，之前用于恢复全部data。
//      trig_period = 10000000000 / 3000;   amples_per_record = 512*1024;    对应3000MB/s      触发频率越高，速度越快，但是应该是有上限的 100000左右应该比较快  
//要改的参数=========================================  1024*79 对应5m/s 5.5ns(40.5um) 1024*395 对应1m/s 5.5ns(40.5um)
unsigned int trig_frequency = 4000;
unsigned int trig_period = 10000000000 / trig_frequency; //(2*79*2 * 10000000000)/(3800*1024);//原始硬件采集时，需要分配每次触发的时间跨度。修改过后的硬件会自动触发，无需理会。
unsigned int samples_per_record = ((((unsigned int)1024 * 39.5 + 31) / 32) * 32);//1024*79;//1024 * 46;//1024*380;// (unsigned int)(((double)test_speed_mbyte * 1024.0 * 1024.0) / ((double)trig_freq * 2.0));
int max_run_time = 30;
unsigned int sel_tr_buf_no = 256;
unsigned int sel_tr_buf_size = 4 * 1024 * 1024 * sizeof(short);
//----------------------------------------
unsigned int REDUCE_PULSE = 0x0003;     //实际减少的为（REDUCE_PULSE-2）最小值为2等于不减少脉冲
unsigned int CONTORL_INVERSE = 0x0001;     //1=yes  0(else)=no
unsigned int CONTORL_LATENCY = 0x0001;     //Control data_out latency
                                           //default=1   0:<-  1:  2:-> 3:-->
unsigned int CELL_EXTEND_PERIOD = 0x0278;

unsigned int Remove_value = 0x0000;  //符号也有用 负数也有用 
unsigned int Pulse_threhold = 0x0FA0;

unsigned int CELL_COMEING_THRESHOLD = 0x1e78;
unsigned int NUM_OF_SAMPLE_IN_PULSE = 0x0000;
unsigned int POSITION_OF_PULSE_END = 0x0001;
//---------------------------------------------------
unsigned int target_header_malloc_byte = 2 * 40 * (REDUCE_PULSE - 1) * (sel_tr_buf_size / samples_per_record + 1);
//target_header_malloc_byte 说明：target_header动态分配，每个record 40Bytes，为了保证减少脉冲依然分配足够空间