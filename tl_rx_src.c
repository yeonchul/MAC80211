//Modified_130619_GJLEE

#include "tl_rx.h"
#include <net/mac80211.h>

static struct tr_info_list src_nbr_list;
static struct relay_info_list relay_list;
static struct dst_info_list dst_list;

static struct timer_list tl_rx_tr1_timer;
static struct timer_list tl_rx_sndtf_timer;
static struct timer_list tl_mcs_send_timer;

static void tl_rx_tr1_timer_func(unsigned long data);
static void tl_rx_sndtf_timer_func(unsigned long data);
static void tl_mcs_send_timer_func(unsigned long data);

static bool polling_state = false;
static unsigned char polling_addr[ETH_ALEN] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
static unsigned char multicast_addr[ETH_ALEN] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x01};

static unsigned int last_tr_id = 0;

static struct net_device * dev_send;
static unsigned char src_batt = 0xe4;

bool tl_start_check(struct sk_buff *skb){
	unsigned char *port = skb_transport_header(skb);
	unsigned char *data = skb->data + 42;
	unsigned int i;
	printk("tl_start_check\n");

	if(skb->len < 42+16+6){
		printk("exception size tl_start\n");
		return false;
	}

	for(i = 0; i < 8; i++){
		unsigned int ii = i*2;
		if(memcmp(port, data + ii, 2)) return false;
	}

	tl_start_time();

	if(polling_state){
		printk("Polling now!!!\n");
		return false;
	}

	else{
		dev_send = skb->dev;
		tr_set_param(true, data[i*2], data[i*2+1], data[i*2+2], data[i*2+3], data[i*2+4], data[i*2+5]);
		tr_info_list_purge(&src_nbr_list);
		relay_info_list_purge(&relay_list);
		dst_info_list_purge(&dst_list);
		
		test_relay();

		printk("Set Param, src = %d, sys = %d, data_k = %d, data_n = %d, tf_k = %d, tf_thre = %d, max_relay_n = %d\n", tr_get_src(), tr_get_sys(), tr_get_data_k(), tr_get_data_n(), tr_get_tf_k(), tr_get_tf_thre(), tr_get_max_relay_n());
	
		return true;
		//tl_mcs_send_timer_func(0);

	}
	return true;
}



static void tl_mcs_send_timer_func(unsigned long data){
	static unsigned char mcs = 0;
	static unsigned int tr_id = 0;
	unsigned int i = 0;
	
	if (mcs == 0){	
		setup_timer(&tl_mcs_send_timer, &tl_mcs_send_timer_func, 0 );
		get_random_bytes(&tr_id, 4);
		if (tr_id == last_tr_id){
			get_random_bytes(&tr_id, 4);
		}
		last_tr_id = tr_id;
	}

	printk("Send TypeOneTF Message(%d), tf_k = %d with rate %x\n", TypeOneTF, tr_get_tf_k(), mcs);
	for(i = 0; i < tr_get_tf_k(); i++){
		struct sk_buff *otf = tl_alloc_skb(dev_send, multicast_addr, dev_send->dev_addr, TF_SIZE, TypeOneTF);
		if(otf != NULL){
			struct ieee80211_tx_info *info = IEEE80211_SKB_CB(otf);
			otf->data[ETHERHEADLEN + 1] = (tr_get_tf_k() >> 24) & 0xff;
			otf->data[ETHERHEADLEN + 2] = (tr_get_tf_k() >> 16) & 0xff;
			otf->data[ETHERHEADLEN + 3] = (tr_get_tf_k() >> 8) & 0xff;
			otf->data[ETHERHEADLEN + 4] = tr_get_tf_k() & 0xff;
			otf->data[ETHERHEADLEN + 5] = (i >> 24) & 0xff;
			otf->data[ETHERHEADLEN + 6] = (i >> 16) & 0xff;
			otf->data[ETHERHEADLEN + 7] = (i >> 8) & 0xff;
			otf->data[ETHERHEADLEN + 8] = i & 0xff;
			otf->data[ETHERHEADLEN + 9] = (tr_id >> 24) & 0xff;
			otf->data[ETHERHEADLEN + 10] = (tr_id >> 16) & 0xff;
			otf->data[ETHERHEADLEN + 11] = (tr_id >> 8) & 0xff;
			otf->data[ETHERHEADLEN + 12] = tr_id & 0xff;
			otf->data[ETHERHEADLEN+13] =  mcs;
			get_random_bytes(&(otf->data[ETHERHEADLEN + 14]), TF_SIZE - (ETHERHEADLEN + 14));
			info->control.rates[0].idx = mcs;
			dev_queue_xmit(otf);
		}
	}

	if (mcs < NUM_MCS-1)
	{
		unsigned int tx_time = cal_tx_time (mcs, tr_get_tf_k(), TF_SIZE)/1000;
		printk("Schedule next TX after %d ms with MCS %d\n", tx_time, mcs);	
		if(!timer_pending(&tl_mcs_send_timer))	
			mod_timer(&tl_mcs_send_timer, jiffies + msecs_to_jiffies(tx_time));
		
		mcs++;
	}
	else
	{
		if(timer_pending(&tl_mcs_send_timer))	
			del_timer(&tl_mcs_send_timer);
		
		mcs = 0;
	}
}

static void tl_rx_sndtf_timer_func(unsigned long data){
	struct tr_info *info = (struct tr_info *) data;
	printk("Don't receive neighbor report\n");
	if(info->tf_cnt != true){
		printk("Didn't transmit SendTF\n");
		return;
	}
	/*
	else{
		struct sk_buff *sndtf = tl_alloc_skb(info->dev, info->addr, info->dev->dev_addr, SNDTF_SIZE, SendTF);
		if(sndtf != NULL){
			sndtf->data[ETHERHEADLEN+1] = tr_get_tf_k();
			printk("Send Duplicate SendTF Message(%d); k = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", SendTF, tr_get_tf_k(), info->dev->dev_addr[0], info->dev->dev_addr[1], info->dev->dev_addr[2], info->dev->dev_addr[3], info->dev->dev_addr[4], info->dev->dev_addr[5], info->addr[0], info->addr[1], info->addr[2], info->addr[3], info->addr[4], info->addr[5]);

			dev_queue_xmit(sndtf);
			info->tf_cnt = true;
			info->nr_cnt = false;
			tr_info_list_print(&src_nbr_list);
		}
	}
	*/
	info->nr_cnt = true;
	if(tr_info_check_nr_cnt(&src_nbr_list)){
		polling_state = false;
		printk("Relay Selection Start\n");
		tl_select_relay(&src_nbr_list, &relay_list, &dst_list, src_batt);
		send_setrelay();
	}
	else{
		tl_rx_tr1_timer_func(0);
	}
}


static void tl_rx_tr1_timer_func(unsigned long data){
	struct tr_info *info = (&src_nbr_list)->next;

	printk("1-hop training end\n");
	polling_state = true;

	while(info != NULL){
		if(info->tf_cnt == false){
			if(info->rcv_num[0] >= tr_get_tf_thre()){
				struct sk_buff *sndtf = tl_alloc_skb(info->dev, info->addr, info->dev->dev_addr, SNDTF_SIZE, SendTF);
				if(sndtf != NULL){
					memcpy(polling_addr, info->addr, ETH_ALEN);
					
					//sndtf->data[ETHERHEADLEN+1] = tr_get_tf_k();
					sndtf->data[ETHERHEADLEN + 1] = (tr_get_tf_k() >> 24) & 0xff;
					sndtf->data[ETHERHEADLEN + 2] = (tr_get_tf_k() >> 16) & 0xff;
					sndtf->data[ETHERHEADLEN + 3] = (tr_get_tf_k() >> 8) & 0xff;
					sndtf->data[ETHERHEADLEN + 4] = tr_get_tf_k() & 0xff;
					printk("Send SendTF Message(%d); k = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", SendTF, tr_get_tf_k(), info->dev->dev_addr[0], info->dev->dev_addr[1], info->dev->dev_addr[2], info->dev->dev_addr[3], info->dev->dev_addr[4], info->dev->dev_addr[5], info->addr[0], info->addr[1], info->addr[2], info->addr[3], info->addr[4], info->addr[5]);

					dev_queue_xmit(sndtf);
					
					info->tf_cnt = true;
					info->nr_cnt = false;

					setup_timer(&tl_rx_sndtf_timer, &tl_rx_sndtf_timer_func, (unsigned long) info);
					mod_timer(&tl_rx_sndtf_timer, jiffies + 10*HZ);
					
					tr_info_list_print(&src_nbr_list);
					return;
				}
				else{
					printk("Fail in tl_alloc_skb!!\n");
				}
			}
			else{
				printk("Isn't satisfied threshold in %x:%x:%x:%x:%x:%x\n", info->addr[0], info->addr[1], info->addr[2], info->addr[3], info->addr[4], info->addr[5]);
				info->tf_cnt = true;
				info->nr_cnt = true;
				if(tr_info_check_nr_cnt(&src_nbr_list)){
					polling_state = false;
					printk("Relay Selection Start\n");
					tl_select_relay(&src_nbr_list, &relay_list, &dst_list, src_batt);
					send_setrelay();
					return;
				}
			}
		}
		info = info->next;
	}
}




void tl_receive_skb_src(struct sk_buff *skb){
	enum tr_type skb_type = skb->data[0];
	
	unsigned char *skb_daddr = skb_mac_header(skb);
	unsigned char *skb_saddr = skb_mac_header(skb) + 6;

	static bool for_src_init = true;
	if(for_src_init == true){
		setup_timer(&tl_rx_tr1_timer, &tl_rx_tr1_timer_func, 0);
		tr_info_list_init(&src_nbr_list);
		relay_info_list_init(&relay_list);
		dst_info_list_init(&dst_list);
	
		for_src_init = false;
	}
	
	//printk(KERN_INFO "receive in src\n");
	if(skb_type == TypeOneTR){
		if(!memcmp(skb->dev->dev_addr, skb_daddr, ETH_ALEN)){
			if(!polling_state){
				struct tr_info *info;
				unsigned int n_rcv[NUM_MCS];
				int i=0;
				char rssi = (char) skb->data[1];
				unsigned char batt = (unsigned char) skb->data[2];
				
				memset(n_rcv, 0, sizeof(unsigned int)*NUM_MCS); //may incur an error
				
				for (i=0; i < NUM_MCS; i++){	
					n_rcv[i] = (unsigned int) skb->data[3+4*i] << 24 | skb->data[4+4*i] << 16 | skb->data[5+4*i] << 8 | skb->data[6+4*i];	
				}

				printk("Receive TypeOneTR Message(%d); rssi = %d, batt = %d, rcv0 = %d, rcv1 = %d, rcv2 = %d, rcv3 = %d, rcv4 = %d, rcv5 = %d, rcv6 = %d, rcv7 = %d, rcv8 = %d, rcv9 = %d, rcv10 = %d, rcv11 = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, rssi, batt, n_rcv[0],  n_rcv[1], n_rcv[2], n_rcv[3], n_rcv[4], n_rcv[5], n_rcv[6], n_rcv[7], n_rcv[8], n_rcv[9], n_rcv[10], n_rcv[11],skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);
				
				// Initialize
				if((info = tr_info_find_addr(&src_nbr_list, skb_saddr)) == NULL){
					tr_info_insert(tr_info_create(skb_saddr, skb->dev, tr_get_tf_k(), n_rcv, rssi, batt), &src_nbr_list);
				}
				else{
					info->dev = skb->dev;
					info->total_num = tr_get_tf_k();
					//info->rcv_num = n_rcv;
					memcpy(info->rcv_num, n_rcv, sizeof(unsigned int)*NUM_MCS);
					info->rssi = rssi;
					info->batt = batt;
					info->tf_cnt = false;
					info->nr_cnt = false;
					tr_info_list_purge(&(info->nbr_list));
				}
				//if(skb_n > (tr_get_tf_k()/10)*8) mod_timer(&tl_rx_tr1_timer, jiffies + 70);
				
				mod_timer(&tl_rx_tr1_timer, jiffies + 2*HZ); //should disable comment
				tr_info_list_print(&src_nbr_list);
			}
			else{
				printk("Receive too late TypeOneTR, SA = %x:%x:%x:%x:%x:%x\n", skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5]);
			}
		}
	}

	else if(skb_type == NbrRPT){
		if(!memcmp(skb->dev->dev_addr, skb_daddr, ETH_ALEN)){
			if(polling_state){
				if(!memcmp(polling_addr, skb_saddr, ETH_ALEN)){
					struct tr_info *sa_info = tr_info_find_addr(&src_nbr_list, skb_saddr);
					if(sa_info != NULL){
						if(sa_info->tf_cnt == true){
							struct tr_info_list *nbr_2hop_list = &(sa_info->nbr_list);
							unsigned int skb_k = (unsigned int) skb->data[1] << 24 | skb->data[2] << 16 | skb->data[3] << 8 | skb->data[4];
							//unsigned int skb_k = skb->data[1];
							unsigned char skb_num_nbr = skb->data[5];
							unsigned char i;

							printk("Receive NbrRPT Message(%d); skb_k = %d, skb_num_nbr = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, skb_k, skb_num_nbr, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);

							del_timer(&tl_rx_sndtf_timer);
							sa_info->nr_cnt = true;

							for(i = 0; i < skb_num_nbr; i++){
								struct tr_info *info;
								int ii = i * (ETH_ALEN + 2 + 4*NUM_MCS);
								int j=0;
								unsigned char *nbr_addr = &(skb->data[ii + 6]);
								char rssi = skb->data[ii+ 6 + ETH_ALEN];
								unsigned char batt = skb->data[ii+ 6+ ETH_ALEN+1];
								unsigned int n_rcv[NUM_MCS];
								for (j=0; j < NUM_MCS; j++){
									n_rcv[j] = (unsigned int) skb->data[ii + 6 + ETH_ALEN + 2 + 4*j ] << 24 | skb->data[ii + 6 + ETH_ALEN + 3 + 4*j] << 16 | skb->data[ii + 6 + ETH_ALEN + 4 + 4*j] << 8 | skb->data[ii + 6 + ETH_ALEN + 5 + 4*j];
								}
								//unsigned char skb_n = skb->data[ii + 6 + ETH_ALEN];
								printk("Addr%d = %x:%x:%x:%x:%x:%x, rssi = %x batt = %x n0 = %d n1 = %d n2 = %d n3 = %d n4 = %d n5 = %d n6 = %d n7 = %d n8 = %d n9 = %d n10 = %d n11 = %d\n", i, nbr_addr[0], nbr_addr[1], nbr_addr[2], nbr_addr[3], nbr_addr[4], nbr_addr[5], rssi, batt, n_rcv[0], n_rcv[1], n_rcv[2], n_rcv[3], n_rcv[4], n_rcv[5], n_rcv[6], n_rcv[7], n_rcv[8], n_rcv[9], n_rcv[10], n_rcv[11]);
								// Initialize
								if((info = tr_info_find_addr(nbr_2hop_list, nbr_addr)) == NULL){
									info = tr_info_create(nbr_addr, skb->dev, skb_k, n_rcv, rssi, batt);
									tr_info_insert(info, nbr_2hop_list);
								}
								else{
									info->total_num = skb_k;
									memcpy(info->rcv_num, n_rcv, sizeof(unsigned int)*NUM_MCS);
									info->rssi = rssi;
									info->batt = batt;
									info->dev = skb->dev;
								}
							}
							tr_info_list_print(&src_nbr_list);
							if(tr_info_check_nr_cnt(&src_nbr_list)){
								polling_state = false;
								printk("Relay Selection Start\n");
								tl_select_relay(&src_nbr_list, &relay_list, &dst_list, src_batt);
								send_setrelay();
							}
							else{
								tl_rx_tr1_timer_func(0);
							}
						}
						else{
							printk("%x:%x:%x:%x:%x:%x don't transmit SendTF frame", skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5]);
						}
					}
					else{
						printk("%x:%x:%x:%x:%x:%x don't exist in src_nbr_list", skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5]);
					}
				}// polling_addr
				else{
					printk("Not polled node's NbrRPT!!\n");
				}
			}// polling state
			else{
				printk("Receive too early NbrRpt, SA = %x:%x:%x:%x:%x:%x\n", skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5]);
			}
		}// daddr
	}
	else if(skb_type == TF_RPT || skb_type == U_FB || skb_type == BLOCK_NACK){
		if(!memcmp(skb->dev->dev_addr, skb_daddr, ETH_ALEN) || (skb_type == BLOCK_NACK && !memcmp(multicast_addr, skb_daddr, ETH_ALEN))){
			struct tr_info *sa_info = tr_info_find_addr(&src_nbr_list, skb_saddr);
			if(sa_info != NULL){
				struct tr_info_list *nbr_2hop_list = &(sa_info->nbr_list);
				unsigned char batt = skb->data[1];
				unsigned char num_nbr = skb->data[2];
				unsigned char i;

				sa_info->batt = batt;	
				
				if(skb_type == TF_RPT)
					printk("Receive TF_RPT Message(%d); skb_num_nbr = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, num_nbr, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);
				else if(skb_type == U_FB)
					printk("Receive U_FB Message(%d); skb_num_nbr = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, num_nbr, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);
				else
					printk("Receive BLOCK_NACK Message(%d); skb_num_nbr = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, num_nbr, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);

				for(i = 0; i < num_nbr; i++){
					struct tr_info *info;
					int ii = 3 + i * (ETH_ALEN + 2);
					unsigned char *nbr_addr = &(skb->data[ii]);
					char rssi = skb->data[ii + ETH_ALEN];
					unsigned char batt = skb->data[ii + ETH_ALEN + 1];

					// Initialize
					if((info = tr_info_find_addr(nbr_2hop_list, nbr_addr)) == NULL){
						unsigned int rcv[NUM_MCS]={0};
						info = tr_info_create(nbr_addr, skb->dev, 0, rcv, rssi, batt);
						tr_info_insert(info, nbr_2hop_list);
					}
					else{
						info->rssi = rssi;
						info->batt = batt;
						info->dev = skb->dev;
					}
					printk("Addr%d = %x:%x:%x:%x:%x:%x, rssi = %d batt = %d\n", i, nbr_addr[0], nbr_addr[1], nbr_addr[2], nbr_addr[3], nbr_addr[4], nbr_addr[5], rssi, batt);
				}
				//tr_info_list_print(&src_nbr_list);
			}
			else{
				printk("%x:%x:%x:%x:%x:%x don't exist in src_nbr_list", skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5]);
			}
		}// daddr
	}
	else{
		//printk(KERN_INFO "other types\n");
	}
	//netif_receive_skb(skb);	
	dev_kfree_skb(skb);
}

//Runtime functions

void send_tfreq(char *addr)
{
	struct sk_buff *otf = tl_alloc_skb(dev_send, addr, dev_send->dev_addr, TF_SIZE, TF_REQ);
	if(otf!=NULL)
		dev_queue_xmit(otf);
}

void send_setrelay(){
	struct relay_info * rinfo;
	struct relay_info_list src_snd_list;
	
	relay_info_list_init(&src_snd_list);

	for (rinfo = relay_list.next; rinfo != NULL; rinfo = rinfo->next){
		if (rinfo->type == 0){
			relay_info_insert(relay_info_create(rinfo->round, rinfo->type, rinfo->addr, rinfo->clout, rinfo->rate), &src_snd_list);
			//memcpy(cur_addr, dev_send->dev_addr, ETH_ALEN); //init with source addr	
		}
	}

	for (rinfo = relay_list.next; rinfo != NULL; rinfo = rinfo->next){
		struct sk_buff * pkt;
		struct dst_info * dst;
		struct dst_info_list child_list;
		struct relay_info_list cur_list;
		unsigned int size = 0;
		unsigned int offset = 0;
		struct relay_info * same_relay;  

		if (rinfo->type == 0)
			continue;

		if (rinfo->type == 100) //already served relay nodes
			continue;

		relay_info_list_init(&cur_list);
		dst_info_list_init(&child_list);

		rinfo->type = 100;	
		offset = rinfo->offset;	
	
		relay_info_insert(relay_info_create(rinfo->round, rinfo->type, rinfo->addr, rinfo->clout, rinfo->rate), &cur_list);
		
		for (dst = dst_list.next; dst != NULL; dst = dst->next){
			if (dst->round == rinfo->round){
				dst_info_insert(dst_info_create(dst->addr, 10, dst->round), &child_list);
			}
		}
	

		for (same_relay = rinfo->next; same_relay != NULL; same_relay = same_relay->next){
			if (same_relay->type == 100)
				continue;			

			if (!memcmp(rinfo->addr, same_relay->addr, ETH_ALEN)){
				same_relay->type = 100;
				relay_info_insert(relay_info_create(same_relay->round, same_relay->type, same_relay->addr, same_relay->clout, same_relay->rate), &cur_list);
				for (dst = dst_list.next; dst != NULL; dst = dst->next){
					if (dst->round == same_relay->round){
						dst_info_insert(dst_info_create(dst->addr, 10, dst->round), &child_list);
					}
				}
			}
		} 
		
		size = ETHERHEADLEN + 1 + 1 + src_snd_list.qlen*2 + 4 + 1 + cur_list.qlen*2 + 1 + 6*child_list.qlen;		
		pkt = tl_alloc_skb(dev_send, rinfo->addr, dev_send->dev_addr, size, SetRelay);

		if(pkt != NULL){
			unsigned int i = 0;
			struct relay_info * relay; 
			struct dst_info * dst_t;

			pkt->data[ETHERHEADLEN + 1] = tr_get_data_k();
			pkt->data[ETHERHEADLEN + 2] = src_snd_list.qlen;

			for (relay = src_snd_list.next; relay != NULL; relay = relay->next){
				pkt->data[ETHERHEADLEN + 3 + i] = relay->clout;
				pkt->data[ETHERHEADLEN + 4 + i] = relay->rate;
				i += 2;
			}

			pkt->data[ETHERHEADLEN + 3 + i] = (offset >> 24) & 0xff;
			pkt->data[ETHERHEADLEN + 4 + i] = (offset >> 16) & 0xff;
			pkt->data[ETHERHEADLEN + 5 + i] = (offset >> 8) & 0xff;
			pkt->data[ETHERHEADLEN + 6 + i] = offset & 0xff;
			pkt->data[ETHERHEADLEN + 7 + i] = cur_list.qlen;

			for (relay = cur_list.next; relay != NULL; relay = relay->next){
				pkt->data[ETHERHEADLEN + 8 + i] = relay->clout;
				pkt->data[ETHERHEADLEN + 9 + i] = relay->rate;
				i += 2;
			}
			
			pkt->data[ETHERHEADLEN + 8 + i] = child_list.qlen;

			for (dst_t = child_list.next; dst_t != NULL; dst_t = dst_t->next){
				memcpy(&(pkt->data[ETHERHEADLEN + 9 + i]), dst_t->addr, ETH_ALEN);
				i += ETH_ALEN;
			}	

			dev_queue_xmit(pkt);
		}
		else{
			printk("Packet memory assignment fails\n");
		}

		relay_info_list_purge(&cur_list);	
		dst_info_list_purge(&child_list);	
	
	}
}

struct relay_info_list *get_relay_list(){
	return &relay_list; 
}

void test_relay(){
	unsigned char clout = 15;
	unsigned char rate = 7;
	unsigned char round = 2;
	unsigned char relay_addr[6] = {0xa,0xa,0xa,0xa,0xa,0xa};
	unsigned char child_addr[6] = {0xb,0xb,0xb,0xb,0xb,0xb};
 
	struct dst_info_list dsts;
	struct relay_info_list relays;

	struct relay_info * info;
	struct relay_info * rinfo;

	relay_info_list_init(&relays);
	dst_info_list_init(&dsts);
	
	printk(KERN_INFO "Start Test\n");
	
	info = relay_info_create(round, 1, relay_addr, clout, rate);
	info->offset = 100000;	

	relay_info_insert(info, &relays);

	dst_info_insert(dst_info_create(child_addr, 0, round), &dsts);

	for (rinfo = relays.next; rinfo != NULL; rinfo = rinfo->next){
		struct sk_buff * pkt;
		struct dst_info * dst;
		unsigned int size = 0;
		unsigned char n_child = 1;

		if (rinfo->type == 0)
			continue;

		size = ETHERHEADLEN + 9 + 6*n_child;		
		pkt = tl_alloc_skb(dev_send, rinfo->addr, dev_send->dev_addr, size, SetRelay);

		if(pkt != NULL){
			unsigned int i = 0;
			pkt->data[ETHERHEADLEN + 1] = 10;
			pkt->data[ETHERHEADLEN + 2] = rinfo->clout;
			pkt->data[ETHERHEADLEN + 3] = rinfo->rate;
			pkt->data[ETHERHEADLEN + 4] = (rinfo->offset >> 24) & 0xff;
			pkt->data[ETHERHEADLEN + 5] = (rinfo->offset >> 16) & 0xff;
			pkt->data[ETHERHEADLEN + 6] = (rinfo->offset >> 8) & 0xff;
			pkt->data[ETHERHEADLEN + 7] = rinfo->offset & 0xff;
			pkt->data[ETHERHEADLEN + 8] = n_child;

			for (dst = dsts.next; dst != NULL; dst = dst->next){
				if (dst->round == rinfo->round){
					memcpy(&(pkt->data[ETHERHEADLEN + 9 + i]), dst->addr, ETH_ALEN);
					i += ETH_ALEN;
				}
			}	
			dev_queue_xmit(pkt);
		}
	}
	relay_info_list_purge(&relays);
	dst_info_list_purge(&dsts);

}
