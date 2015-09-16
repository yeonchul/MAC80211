#include "tl_rx.h"

static struct tr_param param = {false, false, 10, 0, 0, 0, 0, 0};

void tr_set_param(bool src, bool sys, unsigned char data_k, unsigned char data_n, unsigned int tf_k, unsigned int tf_thre, unsigned char max_relay_n, unsigned char mcs, unsigned int offset){
	param.src = src;
	param.sys = sys;
	param.data_k = data_k;
	param.data_n = data_n;
	param.tf_k = tf_k;
	param.tf_thre = tf_thre;
	param.max_relay_n = max_relay_n;
	param.mcs = mcs;
	param.offset = offset;
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

unsigned char tr_get_data_n(void){
	return param.data_n;
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

unsigned char tr_get_mcs(void){
	return param.mcs;
}
