#include "tl_rx.h"


unsigned int cal_tx_time(unsigned char mcs, unsigned char num, unsigned int len){
	unsigned int rate11g[12] = {1, 2, 5.5, 11, 6, 9, 12, 18, 24, 36, 48, 54};
	unsigned int rate11a[8] = {6, 9, 12, 18, 24, 36, 48, 54};
	unsigned int rate=0;	
	unsigned int len_psdu = len - 12 + 30;	

	if (WLAN_MODE){
		rate = rate11a[mcs];	
		return ((36+(((len_psdu*8+22)/rate)/4)*4)+103)*num;
	}
	else{
		rate = rate11g[mcs];	
		return ((192+((len_psdu*8)/rate))+103)*num;
	}
		
}

unsigned int get_tot_rcv(struct tr_info* info){
	unsigned int ret = 0;
	unsigned int i=0;
	
	for (i=0; i<NUM_MCS; i++){
		ret += info->rcv_num[i];
	}
	return ret;
}
