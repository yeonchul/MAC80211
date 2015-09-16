// Modified_130422_GJLEE_YCSHIN
#ifndef __MWNL_MNC_RX_H
#define __MWNL_MNC_RX_H

#include <linux/netdevice.h>

struct mnc_queue{
	unsigned char eid;
	unsigned long ejiffies;
	unsigned char kp;
	bool sys;
	struct sk_buff_head skbs;
	struct mnc_queue *next;
	struct mnc_queue *prev;
	struct mnc_queue_head *list;
};

struct mnc_queue_head{
	struct mnc_queue *next;
	unsigned int qlen;
};

void decoding_try(struct sk_buff *skb, char rssi);

void mnc_queue_head_init(struct mnc_queue_head *list);

void mnc_queue_head_check_etime(struct mnc_queue_head *list, unsigned long cjiffies);

struct mnc_queue *mnc_queue_create(unsigned char eid, unsigned long ejiffies, unsigned char kp, struct mnc_queue_head *list);

void mnc_queue_free(struct mnc_queue *mncq);

void mnc_queue_insert(struct mnc_queue_head *list, struct mnc_queue *newmncq);

struct mnc_queue *mnc_queue_head_find_eid(struct mnc_queue_head *list, unsigned char eid);

void mnc_queue_decoding(struct mnc_queue *mncq, struct sk_buff *skb);

void lu_fac(int m, int n, unsigned char Arr[][n], unsigned char U[][n], unsigned char L[][n], unsigned int pivot[]);

void print_matrix(int m, int n, unsigned char M[][n]);

bool skbs_decoding(struct sk_buff_head *skbs, struct sk_buff_head *newskbs, unsigned char kp);

void skb_substi(unsigned int msize, unsigned int k, unsigned int p, unsigned char U[][msize], unsigned char L[][msize], unsigned int pivot[], struct sk_buff *skb[], struct sk_buff *newskb[], unsigned int len, unsigned int mncheadsize, bool is_sys[]);

#endif // __MWNL_MNC_RX_H
