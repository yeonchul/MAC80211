#ifndef ONLINE_TABLE_H
#define ONLINE_TABLE_H

#include "tl_rx.h"

#define NUM_RSSI 50
#define SCALE 1000 // a time SCALE, e.g., 0.1 = 100 (/1000)
#define ALPHA 9 // 0.9
#define THRE_HIGH 950  // IF SCALE changes, it should be also changed
#define THRE_LOW 50    // It is the same 
#define NUM_SAMPLE 40
#define MIN_SAMPLE 5
#define RSSI_TRANSITION 4
#define RSSI_MIN -80

struct mcs_info{
	bool l_exist;
	bool m_exist;
	bool h_exist;
	unsigned int l_idx;
	unsigned int h_idx; 
};


static unsigned int pdr_table[NUM_MCS][NUM_RSSI]; //measured pdr table
static unsigned int res_table[NUM_MCS][NUM_RSSI]; //result table
static struct mcs_info m_info[NUM_MCS];

static bool init = false;
//static bool start_nc = false;


void init_table(void);
void update_table(unsigned int seq, unsigned int id, unsigned char mcs, int rssi, bool start_nc, unsigned int nc_n, unsigned int table_k);
void record_sample(unsigned char mcs, int rssi_sum, unsigned int rcv, unsigned int tot);
void print_table(unsigned int table[][NUM_RSSI]);
void set_training_max(unsigned int max);
unsigned int get_pdr(unsigned char mcs, int rssi);
void filling_blank(unsigned char mcs);
void monotonicity(void);
void print_pdr_table(void);
void print_res_table(void);

//void ref_table(unsigned int seq, unsigned int id, unsigned char mcs, int rssi);


#endif
