//Modified_130619_GJLEE

#include "tl_rx.h"
#include <net/mac80211.h>

struct tr_info *tf1_info = NULL;
struct tr_info *tf2_info = NULL;
struct tr_info *sdf_info = NULL;

static struct timer_list tl_rx_tf1_timer;
static struct timer_list tl_rx_tf2_timer;
static struct timer_list tl_rx_tr2_timer;

static unsigned char multicast_addr[ETH_ALEN] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x01};

static unsigned int tf1_prev_rcv_num = 0;
static unsigned int tf2_prev_rcv_num = 0;

static void tl_rx_tr2_timer_func(unsigned long data){
	struct tr_info_list *list = (struct tr_info_list *) data;
	unsigned int qlen;	
	unsigned int size;
	struct tr_info *info;
	struct sk_buff *rpt = NULL;
	int i;
	//struct tr_info *test_info;

	//printk("tr2_timer_func\n");

	if(list == NULL){
		printk("NULL list\n");
	}

	qlen = list->qlen;	
	size = ETHERHEADLEN + 6 + ((ETH_ALEN + 2 + 4*NUM_MCS) * qlen);
	info = list->next;
	
	/*
	printk("qlen = %d, size = %d\n", qlen, size);
	test_info = list->next;
	while(test_info != NULL){
		struct tr_info *test_src_nbr_list_info = (&(test_info->nbr_list))->next;
		printk(KERN_INFO "Relay candidate %x:%x:%x:%x:%x:%x, %d/%d, %d, %d\n", test_info->addr[0], test_info->addr[1], test_info->addr[2], test_info->addr[3], test_info->addr[4], test_info->addr[5], test_info->total_num, test_info->rcv_num, test_info->tf_cnt, test_info->nr_cnt);
		while(test_src_nbr_list_info != NULL){
			printk(KERN_INFO "-> %x:%x:%x:%x:%x:%x, %d/%d, %d, %d\n", test_src_nbr_list_info->addr[0], test_src_nbr_list_info->addr[1], test_src_nbr_list_info->addr[2], test_src_nbr_list_info->addr[3], test_src_nbr_list_info->addr[4], test_src_nbr_list_info->addr[5], test_src_nbr_list_info->total_num, test_src_nbr_list_info->rcv_num, test_src_nbr_list_info->tf_cnt, test_src_nbr_list_info->nr_cnt);
			test_src_nbr_list_info = test_src_nbr_list_info->next;
		}
		test_info = test_info->next;
	}
	*/
	
	if(info == NULL){
		printk("Empty list\n");
		return;
	}

	if(sdf_info == NULL){
		printk("Empty sdf_info\n");
		return;
	}
	//printk("sdf_info addr = %x:%x:%x:%x:%x:%x\n", sdf_info->addr[0], sdf_info->addr[1], sdf_info->addr[2], sdf_info->addr[3], sdf_info->addr[4], sdf_info->addr[5]);

	rpt = tl_alloc_skb(info->dev, sdf_info->addr, info->dev->dev_addr, size, NbrRPT);
	
	if(rpt != NULL){
		rpt->data[ETHERHEADLEN + 1] = (sdf_info->total_num >> 24) & 0xff;
		rpt->data[ETHERHEADLEN + 2] = (sdf_info->total_num >> 16) & 0xff;
		rpt->data[ETHERHEADLEN + 3] = (sdf_info->total_num >> 8) & 0xff;
		rpt->data[ETHERHEADLEN + 4] = sdf_info->total_num & 0xff;
		//rpt->data[ETHERHEADLEN + 1] = sdf_info->total_num;
		rpt->data[ETHERHEADLEN + 5] = qlen; //may incur a bug
		printk("Send NbrRPT Message(%d); SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", NbrRPT, info->dev->dev_addr[0], info->dev->dev_addr[1], info->dev->dev_addr[2], info->dev->dev_addr[3], info->dev->dev_addr[4], info->dev->dev_addr[5], sdf_info->addr[0], sdf_info->addr[1], sdf_info->addr[2], sdf_info->addr[3], sdf_info->addr[4], sdf_info->addr[5]);
		for(i = 0; i < qlen; i++){
			int ii = i * (ETH_ALEN + 2 + 4*NUM_MCS);
			int j=0;
			printk("Addr%d = %x:%x:%x:%x:%x:%x, rssi = %x batt = %x n0 = %d n1 = %d n2 = %d n3 = %d n4 = %d n5 = %d n6 = %d n7 = %d\n", i, info->addr[0], info->addr[1], info->addr[2], info->addr[3], info->addr[4], info->addr[5], info->rssi, info->batt, info->rcv_num[0], info->rcv_num[1], info->rcv_num[2], info->rcv_num[3], info->rcv_num[4], info->rcv_num[5], info->rcv_num[6], info->rcv_num[7]);
			memcpy(&(rpt->data[ETHERHEADLEN + 6 + ii]), info->addr, ETH_ALEN);
			rpt->data[ETHERHEADLEN + 6 + ii + ETH_ALEN] = info->rssi; 
			rpt->data[ETHERHEADLEN + 6 + ii + ETH_ALEN + 1] = info->batt; 

			for (j=0; j < NUM_MCS; j++){	
				rpt->data[ETHERHEADLEN + 6 + ii + ETH_ALEN + j*4 + 2] = (info->rcv_num[j] >> 24) & 0xff;
				rpt->data[ETHERHEADLEN + 6 + ii + ETH_ALEN + j*4 + 3] = (info->rcv_num[j] >> 16) & 0xff;
				rpt->data[ETHERHEADLEN + 6 + ii + ETH_ALEN + j*4 + 4] = (info->rcv_num[j] >> 8) & 0xff;
				rpt->data[ETHERHEADLEN + 6 + ii + ETH_ALEN + j*4 + 5] = info->rcv_num[j] & 0xff;
			}
			//rpt->data[ETHERHEADLEN + 6 + ii + ETH_ALEN] = info->rcv_num;
			tr_info_free(info);
			info = list->next;

			if(info == NULL){
				printk("Empty info\n");
				break;
			}
		}

		if(qlen > 0) dev_queue_xmit(rpt);
		else dev_kfree_skb(rpt);
	}
	else{
		printk("Fail in tl_alloc_skb!!\n");
	}

	tr_info_free(sdf_info);
	sdf_info = NULL;
}

static void tl_rx_tf_timer_func(unsigned long data){
	enum tr_type rpt_type = (enum tr_type) data;

	struct tr_info *info;
	unsigned int *prev_rcv_num;
	struct timer_list *tf_timer;
	struct sk_buff *rpt = NULL;
	int i = 0;

	switch(rpt_type){
		case TypeOneTR : info = tf1_info; prev_rcv_num = &tf1_prev_rcv_num; tf_timer = &tl_rx_tf1_timer; break;
		case TypeTwoTR : info = tf2_info; prev_rcv_num = &tf2_prev_rcv_num; tf_timer = &tl_rx_tf2_timer; break;
		default : return;
	}
	
	if(*prev_rcv_num < info->rcv_num[0]){
		printk("Set timer, prev_rcv_num = %d, info->rcv_num = %d\n", *prev_rcv_num, info->rcv_num[0]);
		*prev_rcv_num = info->rcv_num[0];
		mod_timer(tf_timer, jiffies + HZ);
	}
	else{
		printk("Del timer, prev_rcv_num = %d, info->rcv_num = %d\n", *prev_rcv_num, info->rcv_num[0]);
		return;
	}
	
	if(info != NULL){
		rpt = tl_alloc_skb(info->dev, info->addr, info->dev->dev_addr, TR_SIZE, rpt_type);
	}
	else{
		printk("NULL info\n");
	}
	
	if(rpt != NULL){
		rpt->data[ETHERHEADLEN + 1] = info->rssi;
		rpt->data[ETHERHEADLEN + 2] = info->batt;
		for (i=0; i < NUM_MCS; i++){
			rpt->data[ETHERHEADLEN + 3 + i*4] = (info->rcv_num[i] >> 24) & 0xff;
			rpt->data[ETHERHEADLEN + 4 + i*4] = (info->rcv_num[i] >> 16) & 0xff;
			rpt->data[ETHERHEADLEN + 5 + i*4] = (info->rcv_num[i] >> 8) & 0xff;
			rpt->data[ETHERHEADLEN + 6 + i*4] = info->rcv_num[i] & 0xff;
		}
		printk(KERN_INFO "send 1-hop training report \n rssi = %d, batt = %d, rcv0 = %d, rcv1 = %d  rcv2 = %d rcv3 = %d rcv4 = %d rcv5 = %d rcv6 = %d rcv7 = %d\n", info->rssi, info->batt, info->rcv_num[0], info->rcv_num[1], info->rcv_num[2], info->rcv_num[3], info->rcv_num[4], info->rcv_num[5], info->rcv_num[6], info->rcv_num[7] );
		dev_queue_xmit(rpt);
		//printk("Send TypeTR Message(%d); rcv = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", rpt_type, info->rcv_num, info->dev->dev_addr[0], info->dev->dev_addr[1], info->dev->dev_addr[2], info->dev->dev_addr[3], info->dev->dev_addr[4], info->dev->dev_addr[5], info->addr[0], info->addr[1], info->addr[2], info->addr[3], info->addr[4], info->addr[5]);
	}
	else{
		printk("Fail in tl_alloc_skb!!\n");
	}
}

void tl_receive_skb_dst(struct sk_buff *skb){
	static struct tr_info_list tr2_list;

	enum tr_type skb_type = skb->data[0];

	unsigned char *skb_daddr = skb_mac_header(skb);
	unsigned char *skb_saddr = skb_mac_header(skb) + 6;

	static bool for_init = true;
	

	if(for_init == true){
		tr_info_list_init(&tr2_list);
		
		setup_timer(&tl_rx_tf1_timer, &tl_rx_tf_timer_func, TypeOneTR);
		setup_timer(&tl_rx_tf2_timer, &tl_rx_tf_timer_func, TypeTwoTR);
		setup_timer(&tl_rx_tr2_timer, &tl_rx_tr2_timer_func, (unsigned long)(&tr2_list));

		for_init = false;
	}

	if(skb_type == TypeOneTF){
		static unsigned int tf1_cur_index = 0;
		unsigned int tf1_k = (unsigned int) skb->data[1] << 24 | skb->data[2] << 16 | skb->data[3] << 8 | skb->data[4];
		unsigned int tf1_seq = (unsigned int) skb->data[5] << 24 | skb->data[6] << 16 | skb->data[7] << 8 | skb->data[8];
		unsigned int tf1_index = (unsigned int) skb->data[9] << 24 | skb->data[10] << 16 | skb->data[11] << 8 | skb->data[12];
		unsigned char mcs = (unsigned char) skb->data[13];
		//unsigned int tf1_rest = tf1_k/4 - tf1_seq/4 + 1;
		
		//printk(KERN_INFO "skb type : %d k: %d seq: %d id: %d mcs: %d\n", skb_type, tf1_k, tf1_seq, tf1_index, mcs);
		
		if (mcs > NUM_MCS){
			printk("ERROR, invalid MCS index\n");
			return; 
		}	

		if(tf1_k < tf1_seq){
			printk("ERROR, tf1_k is lower than tf1_seq\n");
		}
		else{
			// Initialize
			if(tf1_info == NULL){
				unsigned int rcv[NUM_MCS];
				int i=0;
				memset(rcv, 0, sizeof(unsigned int)*NUM_MCS); //may incur an error
				tf1_info = tr_info_create(skb_saddr, skb->dev, tf1_k, rcv, 0, 0);
				trinfo_print(tf1_info);
			}

			if(!memcmp(tf1_info->addr, skb_saddr, ETH_ALEN) && tf1_cur_index == tf1_index){
				//printk("tf1_k = %d, tf1_seq = %d rcv[%d] =%d\n", tf1_k, tf1_seq, mcs, tf1_info->rcv_num[mcs]);
				(tf1_info->rcv_num[mcs])++;
				if(!timer_pending(&tl_rx_tf1_timer)) mod_timer(&tl_rx_tf1_timer, jiffies + HZ);
			}
			else{
				unsigned int rcv[NUM_MCS]; //may incur an error
				memset(rcv, 0, sizeof(unsigned int)*NUM_MCS); //may incur an error
				//printk("tf1_info->addr = %x:%x:%x:%x:%x:%x, %d != %d\n", tf1_info->addr[0], tf1_info->addr[1], tf1_info->addr[2], tf1_info->addr[3], tf1_info->addr[4], tf1_info->addr[5], tf1_cur_index, tf1_index);
				//printk("Receive TypeOneTF Message(%d); SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);
				
				tr_set_param(false, false, 10, 0, 0, 0, 0);
				printk(KERN_INFO "Receive first TypeOneTf, set parameter src = %d, sys = %d, data_k = %d, data_n = %d, tf_k = %d, tf_thre = %d, max_relay_n = %d\n", tr_get_src(), tr_get_sys(), tr_get_data_k(), tr_get_data_n(), tr_get_tf_k(), tr_get_tf_thre(), tr_get_max_relay_n());

				tr_info_free(tf2_info);
				tf2_info = NULL;

				tr_info_list_purge(&tr2_list);

				tr_info_free(sdf_info);
				sdf_info = NULL;

				memcpy(tf1_info->addr, skb_saddr, ETH_ALEN);
				tf1_info->dev = skb->dev;
				tf1_info->total_num = tf1_k;
				memcpy(tf1_info->rcv_num, rcv, sizeof(unsigned int)*NUM_MCS);
				trinfo_print(tf1_info);
				
				tf1_prev_rcv_num = 0;
				tf1_cur_index = tf1_index;
			}
		}

	}

	else if(skb_type == TypeTwoTF){
		static unsigned int tf2_cur_index = 0;
		unsigned int tf2_k = (unsigned int) skb->data[1] << 24 | skb->data[2] << 16 | skb->data[3] << 8 | skb->data[4];
		unsigned int tf2_seq = (unsigned int) skb->data[5] << 24 | skb->data[6] << 16 | skb->data[7] << 8 | skb->data[8];
		unsigned int tf2_index = (unsigned int) skb->data[9] << 24 | skb->data[10] << 16 | skb->data[11] << 8 | skb->data[12];
		unsigned char mcs = (unsigned char) skb->data[13];
		//unsigned int tf2_rest = tf2_k/4 - tf2_seq/4 + 1;
		
		if(tf2_k < tf2_seq){
			printk("ERROR, tf2_k is lower than tf2_seq\n");
		}
		else{
			// Initialize
			if(tf2_info == NULL){
				unsigned int rcv[NUM_MCS] = {0}; //may incur an error
				tf2_info = tr_info_create(skb_saddr, skb->dev, tf2_k, rcv, 0, 0);
			}
			if(!memcmp(tf2_info->addr, skb_saddr, ETH_ALEN) && tf2_cur_index == tf2_index){
				//printk("tf2_k = %d, tf2_seq = %d\n", tf2_k, tf2_seq);
				(tf2_info->rcv_num[mcs])++;
				if(!timer_pending(&tl_rx_tf2_timer)) mod_timer(&tl_rx_tf2_timer, jiffies + HZ/10);
			}
			else{
				unsigned int rcv[NUM_MCS] = {0}; //may incur an error
				//printk("tf2_info->addr = %x:%x:%x:%x:%x:%x, %d != %d\n", tf2_info->addr[0], tf2_info->addr[1], tf2_info->addr[2], tf2_info->addr[3], tf2_info->addr[4], tf2_info->addr[5], tf2_cur_index, tf2_index);
			//	printk("Receive TypeTwoTF Message(%d); SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);
				
				tr_set_param(false, false, 10, 0, 0, 0, 0);
				printk("Receive first TypeTwoTf, set parameter src = %d, sys = %d, data_k = %d, data_n = %d, tf_k = %d, tf_thre = %d, max_relay_n = %d\n", tr_get_src(), tr_get_sys(), tr_get_data_k(), tr_get_data_n(), tr_get_tf_k(), tr_get_tf_thre(), tr_get_max_relay_n());
				
				tr_info_free(tf1_info);
				tf1_info = NULL;
				
				memcpy(tf2_info->addr, skb_saddr, ETH_ALEN);
				tf2_info->dev = skb->dev;
				tf2_info->total_num = tf2_k;
				memcpy(tf2_info->rcv_num, rcv, sizeof(unsigned int)*NUM_MCS);
				
				tf2_prev_rcv_num = 0;
				tf2_cur_index = tf2_index;
			}
		}

	}

	else if(skb_type == TypeTwoTR){
		unsigned int n_rcv[NUM_MCS]; 
		unsigned char rssi = (unsigned char) skb->data[1];
		unsigned char batt = (unsigned char) skb->data[2];
		int j = 0;

		
		for (j=0; j<NUM_MCS; j++){
			n_rcv[j]= (unsigned int) skb->data[3+4*j] << 24 | skb->data[4+4*j] << 16 | skb->data[5+4*j] << 8 | skb->data[6+4*j];
		}
		if(!memcmp(skb->dev->dev_addr, skb_daddr, ETH_ALEN)){
			if(sdf_info != NULL){
				struct tr_info *info;
				printk("Receive TypeTwoTR Message(%d); rssi = %x, batt = %x, rcv0 = %d, rcv1 = %d, rcv2 = %d, rcv3 = %d, rcv4 = %d, rcv5 = %d, rcv6 = %d, rcv7 = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, rssi, batt, n_rcv[0],  n_rcv[1], n_rcv[2], n_rcv[3], n_rcv[4], n_rcv[5], n_rcv[6], n_rcv[7], skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);

				if((info = tr_info_find_addr(&tr2_list, skb_saddr)) == NULL){
					tr_info_insert(tr_info_create(skb_saddr, skb->dev, sdf_info->total_num, n_rcv, rssi, batt), &tr2_list);
				}
				else{
					info->dev = skb->dev;
					info->total_num = sdf_info->total_num;
					memcpy(info->rcv_num, n_rcv, sizeof(unsigned int)*NUM_MCS);
					info->rssi = rssi;
					info->batt = batt;
					info->tf_cnt = false;
					info->nr_cnt = false;
					tr_info_list_purge(&(info->nbr_list));
				}
				mod_timer(&tl_rx_tr2_timer, jiffies + HZ/5);
			}
			else{
				printk("sdf_info is NULL when TR2 is arrived\n");
			}
		}
	}

	else if(skb_type == SendTF){
		unsigned int skb_k = (unsigned int) skb->data[1] << 24 | skb->data[2] << 16 | skb->data[3] << 8 | skb->data[4];
		static unsigned int last_tf2_id = 0;
		unsigned int i;
		unsigned int j;
		if(!memcmp(skb->dev->dev_addr, skb_daddr, ETH_ALEN)){
			printk("Receive SendTF Message(%d); SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);
			if(sdf_info == NULL){
				unsigned int tf2_id = 0;
				unsigned int rcv[NUM_MCS] = {0};
				sdf_info = tr_info_create(skb_saddr, skb->dev, skb_k, rcv, 0, 0);
				printk("Send TypeTwoTF Message(%d); k = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", TypeTwoTF, skb_k, skb->dev->dev_addr[0], skb->dev->dev_addr[1], skb->dev->dev_addr[2], skb->dev->dev_addr[3], skb->dev->dev_addr[4], skb->dev->dev_addr[5], multicast_addr[0], multicast_addr[1], multicast_addr[2], multicast_addr[3], multicast_addr[4], multicast_addr[5]);
				
				get_random_bytes(&tf2_id, 4);
				if(tf2_id == last_tf2_id){
					get_random_bytes(&tf2_id, 4);
				}
				
				for (j=0; j < NUM_MCS; j++){
					for(i = 0; i < skb_k; i++){
						struct sk_buff *rpt = tl_alloc_skb(skb->dev, multicast_addr, skb->dev->dev_addr, TF_SIZE, TypeTwoTF);
						if(rpt != NULL){
							struct ieee80211_tx_info *info = IEEE80211_SKB_CB(rpt);
							rpt->data[ETHERHEADLEN + 1] = (skb_k >> 24) & 0xff;
							rpt->data[ETHERHEADLEN + 2] = (skb_k >> 16) & 0xff;
							rpt->data[ETHERHEADLEN + 3] = (skb_k >> 8) & 0xff;
							rpt->data[ETHERHEADLEN + 4] = skb_k & 0xff;
							rpt->data[ETHERHEADLEN + 5] = (i >> 24) & 0xff;
							rpt->data[ETHERHEADLEN + 6] = (i >> 16) & 0xff;
							rpt->data[ETHERHEADLEN + 7] = (i >> 8) & 0xff;
							rpt->data[ETHERHEADLEN + 8] = i & 0xff;
							rpt->data[ETHERHEADLEN + 9] = (tf2_id >> 24) & 0xff;
							rpt->data[ETHERHEADLEN + 10] = (tf2_id >> 16) & 0xff;
							rpt->data[ETHERHEADLEN + 11] = (tf2_id >> 8) & 0xff;
							rpt->data[ETHERHEADLEN + 12] = tf2_id & 0xff;
							rpt->data[ETHERHEADLEN+13] = j;
							get_random_bytes(&(rpt->data[ETHERHEADLEN + 14]), TF_SIZE - (ETHERHEADLEN + 14));
							info->control.rates[0].idx = j;
							dev_queue_xmit(rpt);
						}
						else{
							printk("Fail in tl_alloc_skb!!\n");
						}
					}
				}
				last_tf2_id = tf2_id;
			}
			else{
				printk("Receive duplicate SendTF\n");
				//tl_rx_tr2_timer_func((unsigned long)(&tr2_list));
			}
		}
	}

	else if(skb_type == SetRelay){
		if(!memcmp(skb->dev->dev_addr, skb_daddr, ETH_ALEN)){
			unsigned char skb_k = skb->data[1];
			unsigned char skb_n = skb->data[2];
			tr_set_param(false, false, skb_k, skb_n, 0, 0, 0);
			printk("Receive SetRelay, set parameter src = %d, sys = %d, data_k = %d, data_n = %d, tf_k = %d, tf_thre = %d, max_relay_n = %d\n", tr_get_src(), tr_get_sys(), tr_get_data_k(), tr_get_data_n(), tr_get_tf_k(), tr_get_tf_thre(), tr_get_max_relay_n());
			printk("I AM RELAY, n = %d\n", skb_n);
		}
	}
	else{
		printk("CLIENT DON'T RECEIVE %d!!!; SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);
	}

//	netif_receive_skb(skb);	
	dev_kfree_skb(skb);
}
