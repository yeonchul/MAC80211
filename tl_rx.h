//Modified_150628_YCSHIN

#ifndef __MWNL_TL_RX_H
#define __MWNL_TL_RX_H

#include <linux/skbuff.h>
#include <linux/jiffies.h>
#include <linux/time.h>
#include <linux/netdevice.h>

#define TR_SIZE 49
#define TF_SIZE 1370
#define SNDTF_SIZE 19
#define SETRELAY_SIZE 17

#define MNPRELAY 0

#define ETHERHEADLEN 14
#define NUM_MCS 8

#define WLAN_MODE 0 //0: 11g, 1: 11a

void tl_receive_skb_src(struct sk_buff *skb);
void tl_receive_skb_dst(struct sk_buff *skb);

enum tr_type {
	TypeOneTF 	= 1,
	TypeTwoTF 	= 2,
	TypeOneTR 	= 3,
	TypeTwoTR 	= 4,
	SendTF 		= 5,
	NbrRPT		= 6,
	SetRelay	= 7,
};



struct tr_param{
	bool src;
	bool sys;
	unsigned char data_k;
	unsigned char data_n;
	unsigned int tf_k;
	unsigned int tf_thre;
	unsigned char max_relay_n; 
};

struct tr_info_list{
	struct tr_info *next;
	unsigned int qlen;
};

struct tr_info{
	unsigned char addr[ETH_ALEN];
	struct net_device *dev;
	unsigned int total_num;
	unsigned int rcv_num[NUM_MCS];
	unsigned char rssi;
	unsigned char batt;
	bool tf_cnt;
	bool nr_cnt;
	struct tr_info_list nbr_list;
	struct tr_info *next;
	struct tr_info *prev;
	struct tr_info_list *head;
};	

struct dst_info_list{
	struct dst_info *next;
	unsigned int qlen;
};

struct dst_info{
	unsigned char addr[ETH_ALEN];
	unsigned char raddr1[ETH_ALEN];
	unsigned char raddr2[ETH_ALEN];
	unsigned char raddr3[ETH_ALEN];
	unsigned char min_clout1;
	unsigned char min_clout2;
	unsigned char min_clout3;
	unsigned char pr_dof;
	struct dst_info *next;
	struct dst_info *prev;
	struct dst_info_list *head;
};

struct relay_info{
	unsigned char addr[ETH_ALEN];
	unsigned int clout_sum;
	unsigned char max_clout;
};

unsigned int cal_tx_time(unsigned char mcs, unsigned char num, unsigned int len);
/*
unsigned int cal_tx_time(unsigned char mcs, unsigned char num, unsigned int len){
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
		
};
*/
bool tl_start_check(struct sk_buff *skb);
void tl_start_time(void);
struct sk_buff *tl_alloc_skb(struct net_device *dev, unsigned char daddr[], unsigned char saddr[], unsigned int size, enum tr_type type);
void tl_select_relay(struct tr_info_list *list);

void tr_info_list_init(struct tr_info_list *list);
struct tr_info *tr_info_create(unsigned char addr[], struct net_device *dev, unsigned int total_num, unsigned int rcv_num[], unsigned char rssi, unsigned char batt);
void tr_info_list_purge(struct tr_info_list *list);
void tr_info_free(struct tr_info *info);
void tr_info_insert(struct tr_info *newinfo, struct tr_info_list *list);
struct tr_info *tr_info_find_addr(struct tr_info_list *list, unsigned char addr[]);
bool tr_info_check_nr_cnt(struct tr_info_list *list);

void dst_info_list_init(struct dst_info_list *list);
struct dst_info *dst_info_create(unsigned char addr[], unsigned char pr_dof);
void dst_info_list_purge(struct dst_info_list *list);
void dst_info_free(struct dst_info *info);
void dst_info_insert(struct dst_info *newinfo, struct dst_info_list *list);
struct dst_info *dst_info_find_addr(struct dst_info_list *list, unsigned char addr[]);

void tr_set_param(bool src, bool sys, unsigned char data_k, unsigned char data_n, unsigned int tf_k, unsigned int tf_thre, unsigned char max_relay_n);
bool tr_get_src(void);
bool tr_get_sys(void);
unsigned char tr_get_data_k(void);
unsigned char tr_get_data_n(void);
unsigned int tr_get_tf_k(void);
unsigned int tr_get_tf_thre(void);
unsigned char tr_get_max_relay_n(void);
void trinfo_print(struct tr_info *info);
unsigned int get_tot_rcv(struct tr_info* info);

#endif
