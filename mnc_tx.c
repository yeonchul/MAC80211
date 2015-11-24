// Modified_130507_GJLEE_YCSHIN

#include <linux/skbuff.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/random.h>

#include "mnc_tx.h"
#include "gf.h"
#include "tl_rx.h"

//#define ETHERLEN 14	// Header length in sk_buff data field
//#define K 10		// The number of original packets to encode
//#define N 20		// The number of created packets
#define P 2		// Matrix network coding coefficient size
#define TIMEOUT 0x100
//#define SYSTEMATIC 0	// 0 : Non-systematic code, 1 : Systematic code 

static struct sk_buff_head skbs_original;
static unsigned char eid = 1;
static struct timer_list mnc_tx_timer;
static struct net_device *dev_prev;

void cal_data(unsigned char *data, unsigned int p, unsigned char coefficient[][p], unsigned char *data_new);
struct sk_buff *make_skb_new(const struct sk_buff_head *skbs, unsigned int p, unsigned char id, unsigned char seq);
void enqueue_skb_original(const struct sk_buff_head *skbs, unsigned int p, unsigned char id, struct sk_buff_head *skbs_result);
static void mnc_tx_timer_func(unsigned long data);

netdev_tx_t ieee80211_matrix_network_coding(struct sk_buff *skb, struct net_device *dev){
	static int i = 0;
	int udp_pos;
	unsigned int portnum;

	udp_pos = skb_transport_header(skb) - skb->data;
	portnum = (skb->data[udp_pos+2] << 8)|skb->data[udp_pos+3];

	if((portnum != 5555) && (portnum != 5550) && (portnum!=9997) ) return ieee80211_subif_start_xmit(skb, dev);

	else if(portnum == 5550){
		if(tl_start_check(skb)){
			kfree_skb(skb);
			return NETDEV_TX_OK;
		}
		else{
			return ieee80211_subif_start_xmit(skb, dev);
		}

	}
	else if (portnum == 9997){
		if(set_batt_info(skb)){
			kfree_skb(skb);
			return NETDEV_TX_OK;	
		}
		else{
			return ieee80211_subif_start_xmit(skb, dev);
		}
	}

	else{
		if(i == 0){
			printk(KERN_INFO "Initialize MNC_TX, HZ = %d\n", HZ);
			skb_queue_head_init(&skbs_original);
			setup_timer(&mnc_tx_timer, &mnc_tx_timer_func, 0);
			i++;
			/*
			   printk(KERN_INFO "sizeof(sk_buff_data_t): %d\n", sizeof(sk_buff_data_t));
			   printk(KERN_INFO "skb->len: %d\n", skb->len);
			   printk(KERN_INFO "skb->data_len: %d\n", skb->data_len);
			   printk(KERN_INFO "skb->mac_len: %d\n", skb->mac_len);
			   printk(KERN_INFO "skb->hdr_len: %d\n", skb->hdr_len);
			   printk(KERN_INFO "skb->ithdr: %x\n", skb->inner_transport_header);
			   printk(KERN_INFO "skb->inhdr: %x\n", skb->inner_network_header);
			   printk(KERN_INFO "skb->th: %d\n", skb->transport_header);
			   printk(KERN_INFO "skb->nh: %d\n", skb->network_header);
			   printk(KERN_INFO "skb->mh: %x\n", skb->mac_header);
			   printk(KERN_INFO "skb->truesize: %d\n", skb->truesize);
			   printk(KERN_INFO "skb->tail: %x\n", skb->tail);
			   printk(KERN_INFO "skb->end: %x\n", skb->end);
			   printk(KERN_INFO "skb->head: %x\n", skb->head);
			   printk(KERN_INFO "skb->data: %x\n", skb->data);

			   int j;
			   for(j = 0; j < ETHERHEADLEN; j++) printk(KERN_INFO "ETHER[%d]: %x\n", j, skb->data[j]);
			   for(j = 14; j < 34; j++) printk(KERN_INFO "IP[%d]: %x\n", j, skb->data[j]);
			   for(j = 34; j < 42; j++) printk(KERN_INFO "UDP[%d]: %x\n", j, skb->data[j]);

			   for(j = 0; j < ETH_ALEN; j++) printk(KERN_INFO "dev->dev_addr[%d]: %x\n", j, dev->dev_addr[j]);
			   for(j = 0; j < ETH_ALEN; j++) printk(KERN_INFO "dev->broadcast[%d]: %x\n", j, dev->broadcast[j]);
			   */
		}

		dev_prev = dev;
		skb_queue_tail(&skbs_original, skb);

		//printk(KERN_INFO "Packet coming: eid = %x, jiffies = %lx, msecs = %x\n", eid, jiffies, jiffies_to_msecs(jiffies));

		if(skb_queue_len(&skbs_original) == tr_get_data_k()){
			netdev_tx_t ret_temp;
			del_timer(&mnc_tx_timer);
			ret_temp = mnc_encoding_tx(&skbs_original, dev, eid);
			skb_queue_purge(&skbs_original);
			return ret_temp;

		}

		else{
			mod_timer(&mnc_tx_timer, jiffies + HZ);
			return NETDEV_TX_OK;
		}
	}
}

void cal_data(unsigned char *data, unsigned int p, unsigned char coefficient[][p], unsigned char *data_new){
	int i, j;
	unsigned char mul;

	for(i = 0; i < p; i++){
		for(j = 0; j < p; j++){
			mul = gmul(*(data + j), coefficient[i][j]);
			*(data_new + i) = gadd(*(data_new + i), mul);
		}
	}
}
struct sk_buff *make_skb_new(const struct sk_buff_head *skbs, unsigned int p, unsigned char id, unsigned char seq){
	struct sk_buff *skb;
	struct sk_buff *skb_new;

	unsigned char *data;
	unsigned char *data_new;
	unsigned int i, j;
	unsigned int l = 0;

	unsigned int k = skb_queue_len(skbs); // # of original packets
	unsigned char coefficient[p][p];

	unsigned int coding_len, skb_len1;
	unsigned int mnc_hdrlen = p*p*k+4;

	int nh_pos, h_pos;
	
	if(skb_queue_empty(skbs)){
		printk(KERN_ERR "Empty sk_buff_head!\n");
		return NULL;
	}

	if(k >= 0x1000000){
		printk(KERN_ERR "Too many packets\n");
		return NULL;
	}

	if(p >= 0x100){
		printk(KERN_ERR "Too big matrix\n");
		return NULL;
	}

	skb = skb_peek(skbs);

	if(tr_get_src()){
		coding_len = skb->len - ETHERHEADLEN;
	}
	else{
		coding_len = skb->len;
	}

	skb_len1 = skb->len;

	// Making new sk_buff: skb_new
	skb_new = skb_copy(skb, GFP_ATOMIC);
	if(skb_new == NULL){
		printk(KERN_ERR "Memory Allocation Error!\n");
		return NULL;
	}

	// Setting memory for mnc header	
	skb_put(skb_new, mnc_hdrlen);

	// Initializing skb_new
	if(tr_get_src()){
		nh_pos = skb_network_header(skb_new) - skb_new->data;
		h_pos = skb_transport_header(skb_new) - skb_new->data;

		nh_pos += mnc_hdrlen;
		h_pos += mnc_hdrlen;

		skb_set_network_header(skb_new, nh_pos);
		skb_set_transport_header(skb_new, h_pos);
	}
	else{
		// memcpy(skb_mac_header(skb_new) + 6, skb_new->dev->dev_addr, ETH_ALEN);
		skb_new->priority += 256;
		skb_new->protocol = htons(ETH_P_802_3);
		skb_reset_network_header(skb_new);
		skb_reset_mac_header(skb_new);
		skb_push(skb_new, ETHERHEADLEN);
	}
	
	data_new = skb_new->data + ETHERHEADLEN;
	for(i = 0; i < skb_new->len - ETHERHEADLEN; i++){
		*(data_new + i) = 0;
	}

	*(data_new) = (k << 2) | p;
	*(data_new+1) = id;
	*(data_new+2) = (tr_get_src() ? 0xfc : 0xfd);
	*(data_new+3) = seq;
	*(data_new-2) = 0x08;
	*(data_new-1) = 0x10;

	while (1)
	{
		if(skb == (struct sk_buff *)skbs) break;
		
		if(skb_len1 != skb->len){
			printk(KERN_ERR "sk_buff length is different!\n");
			continue;
		}

		data = skb->data + (tr_get_src() ? ETHERHEADLEN : 0);
		data_new = skb_new->data + ETHERHEADLEN;

		// initialize coefficient
		for(i = 0; i < p; i++){
			for(j = 0; j < p; j++){
				get_random_bytes(&coefficient[i][j], 1);
				*(data_new + 4 + l) = coefficient[i][j];
				l++;
			}
		}
		data_new += mnc_hdrlen;
		
		// calculate sk_buff data
		for(i = 0; i < coding_len ; i += p){
			cal_data(data, p, coefficient, data_new);
			data += p;
			data_new += p;
		}
		skb = skb->next;
	}
	return skb_new;
}

void enqueue_skb_original(const struct sk_buff_head *skbs, unsigned int p, unsigned char id, struct sk_buff_head *skbs_result){
	struct sk_buff *skb;

	unsigned int k = skb_queue_len(skbs); // # of original packets
	unsigned int j = 0;

	const unsigned int mnc_hdrlen_sys = 3;
	unsigned int skb_len1;
	
	int nh_pos, h_pos;

	if(skb_queue_empty(skbs)){
		printk(KERN_ERR "Empty sk_buff_head!\n");
		return;
	}

	if(k >= 0x1000000){
		printk(KERN_ERR "Too many packets\n");
		return;
	}

	if(p >= 0x100){
		printk(KERN_ERR "Too big matrix\n");
		return;
	}

	skb = skb_peek(skbs);
	skb_len1 = skb->len;

	while(1){
		struct sk_buff *skb_new_sys;
		unsigned char *data_new_sys;
		
		unsigned int i;

		if(skb == (struct sk_buff *)skbs) break;
		
		if(skb_len1 != skb->len){
			printk(KERN_ERR "sk_buff length is different!\n");
			continue;
		}

		// make new sk_buff: skb_new_sys
		skb_new_sys = skb_copy(skb, GFP_ATOMIC);
		if(skb_new_sys == NULL){
			printk(KERN_ERR "Memory Allocation Error!\n");
			return;
		}

		skb_push(skb_new_sys, mnc_hdrlen_sys);

		nh_pos = skb_network_header(skb_new_sys) - skb_new_sys->data;
		h_pos = skb_transport_header(skb_new_sys) - skb_new_sys->data;

		nh_pos += mnc_hdrlen_sys;
		h_pos += mnc_hdrlen_sys;

		skb_set_network_header(skb_new_sys, nh_pos);
		skb_set_transport_header(skb_new_sys, h_pos);
		
		// Move ethernet header in skb_new_sys in front of mnc_hdrlen_sys
		data_new_sys = skb_new_sys->data;
		for(i = 0; i < ETHERHEADLEN; i++){
			*(data_new_sys + i) = *(data_new_sys + i + mnc_hdrlen_sys);
		}
		data_new_sys += ETHERHEADLEN;

		*(data_new_sys) = (k << 2) | p;
		*(data_new_sys+1) = id;
		*(data_new_sys+2) = (j << 2);
		*(data_new_sys-2) = 0x08;
		*(data_new_sys-1) = 0x10;

		skb_queue_tail(skbs_result, skb_new_sys);

		j++;
		skb = skb->next;
	}
}

static void mnc_tx_timer_func(unsigned long data){
	//struct net_device *dev = data;
	//printk(KERN_INFO "Timer Function!! cjiffies = %lx, cmsecs = %x\n", jiffies, jiffies_to_msecs(jiffies));
	mnc_encoding_tx(&skbs_original, dev_prev, eid);
	skb_queue_purge(&skbs_original);
}

netdev_tx_t mnc_encoding_tx(struct sk_buff_head *skbs, struct net_device *dev, unsigned char id){
	struct sk_buff_head skbs_result;
	unsigned char m = 0;
	struct sk_buff *skb_temp;
	netdev_tx_t ret = NETDEV_TX_OK;
	netdev_tx_t ret_temp;

	skb_queue_head_init(&skbs_result);

#if 0
	if(tr_get_sys() == 1){
		// Enqueue K original packets
		enqueue_skb_original(skbs, P, id, &skbs_result);

		// Make N-K coded packets
		if(tr_get_data_n() > tr_get_data_k()){
			for(m = 0; m < tr_get_data_n()-tr_get_data_k(); m++){
				//printk(KERN_INFO "Make coded %dth packet, id = %x\n", m, id);
				skb_temp = make_skb_new(skbs, P, id, m + tr_get_data_k());
				if(skb_temp == NULL) printk(KERN_ERR "Error in making skb_new in %x\n", m);
				else skb_queue_tail(&skbs_result, skb_temp);
			}
		}
	}

	else{
		// Make N coded packets
		for(m = 0; m < tr_get_data_n(); m++){
			//printk(KERN_INFO "Make coded %dth packet, id = %x, n = %d\n", m, id, tr_get_data_n());
			skb_temp = make_skb_new(skbs, P, id, m);
			if(skb_temp == NULL) printk(KERN_ERR "Error in making skb_new in %x\n", m);
			else{
				struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb_temp);
				tx_info->control.rates[0].idx = tr_get_mcs();
				skb_queue_tail(&skbs_result, skb_temp);
			}
		}
	}
#endif
	
	for(m = 0; m < tr_get_relay_rate_num(); m++){
		unsigned char clout = tr_get_clout(m);
		unsigned char rate = tr_get_rate(m);
		unsigned char seq;
		for(seq = 0; seq < clout; seq++){
			//printk(KERN_INFO "Make coded %dth packet, id = %x, n = %d\n", m, id, tr_get_data_n());
			skb_temp = make_skb_new(skbs, P, id, seq);
			if(skb_temp == NULL) printk(KERN_ERR "Error in making skb_new in %x\n", seq);
			else{
				struct ieee80211_tx_info *tx_info = IEEE80211_SKB_CB(skb_temp);
				tx_info->control.rates[0].idx = rate;
				skb_queue_tail(&skbs_result, skb_temp);
			}
		}
	}


	/*
	// Drop original packets
	while(!skb_queue_empty(skbs)){
		skb_temp = skb_dequeue(skbs);
		kfree_skb(skb_temp);
	}
	*/
	
	// Transmit new coded packets
	while(!skb_queue_empty(&skbs_result)){
		skb_temp = skb_dequeue(&skbs_result);
		ret_temp = ieee80211_subif_start_xmit(skb_temp, dev);
		//printk(KERN_INFO "Transmit new packets: id = %x, jiffies = %lx, msecs = %x\n", id, jiffies, jiffies_to_msecs(jiffies));
		if(ret_temp != NETDEV_TX_OK) ret = ret_temp;
	}

	if(tr_get_src()){
		eid++;
		if(eid == 0) eid++;
	}
	
	
	return ret;
}
