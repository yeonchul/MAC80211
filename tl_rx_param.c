#include "tl_rx.h"

static struct tr_param param = {false,	// src
																false,	// sys
																10,			// data_k
																100,			// tf_k
																0,			// tf_thre
																0,			// max_relay_n
																0,			// offset
																0				// relay_rate_num
															 	};

static unsigned char relay_clout[NUM_MCS] = {0};
static unsigned char relay_rate[NUM_MCS] = {0};

void tr_set_param(bool src, bool sys, unsigned char data_k, unsigned int tf_k, unsigned int tf_thre, unsigned char max_relay_n, unsigned int offset){
	param.src = src;
	param.sys = sys;
	param.data_k = data_k;
	param.tf_k = tf_k;
	param.tf_thre = tf_thre;
	param.max_relay_n = max_relay_n;
	param.offset = offset;
}

void tr_add_relay(unsigned char clout, unsigned char rate){
	unsigned char i;

	for(i = 0; i < param.relay_rate_num; i++){
		if(relay_rate[i] == rate){
			relay_clout[i] = clout;
			goto exit;
		}
	}
	param.relay_rate_num++;
	if(param.relay_rate_num >= NUM_MCS){
		printk("ERROR: tr_add_relay, param.relay_rate_num = %d\n", param.relay_rate_num);
		return;
	}
	relay_clout[param.relay_rate_num] = clout;
	relay_rate[param.relay_rate_num] = rate;
	
	i = param.relay_rate_num;

exit:
	printk("tr_add_relay: relay_rate_num = %d, relay_clout[%d] = %d, relay_rate[%d] = %d\n", i, i, relay_clout[i], i, relay_rate[i]);
}

void tr_reset_relay(void){
	param.relay_rate_num = 0;
	memset(relay_clout, 0, NUM_MCS);
	memset(relay_rate, 0, NUM_MCS);
}

unsigned char tr_get_clout(unsigned char relay_rate_num){
	return relay_clout[relay_rate_num];
}

unsigned char tr_get_rate(unsigned char relay_rate_num){
	return relay_rate[relay_rate_num];
}

bool tr_get_src(void){
	return param.src;
}

bool tr_get_sys(void){
	return param.sys;
}

unsigned char tr_get_data_k(void){
	return param.data_k;
}

unsigned int tr_get_tf_k(void){
	return param.tf_k;
}

unsigned int tr_get_tf_thre(void){
	return param.tf_thre;
}

unsigned char tr_get_max_relay_n(void){
	return param.max_relay_n;
}

unsigned int tr_get_offset(void){
	return param.offset;
}

unsigned char tr_get_relay_rate_num(void){
	return param.relay_rate_num;
}
