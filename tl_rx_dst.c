//Modified_130619_GJLEE

#include "tl_rx.h"
#include <net/mac80211.h>

//struct tr_info *tf1_info = NULL;
//struct tr_info *tf2_info = NULL;
struct tr_info *sdf_info = NULL;

static struct timer_list tl_rx_tf1_timer;
static struct timer_list tl_rx_tf2_timer;
static struct timer_list tl_rx_tr2_timer;
static struct timer_list tl_feedback_timer;

static struct timer_list tl_2hop_mcs_send_timer;
static void tl_periodic_feedback(unsigned long data);
static void tl_2hop_mcs_send_timer_func(unsigned long data);


static unsigned char multicast_addr[ETH_ALEN] = {0x01, 0x00, 0x5e, 0x00, 0x00, 0x01};
static unsigned char parent_addr[ETH_ALEN] = {0};
static unsigned char tf1_addr[ETH_ALEN]={0};
static unsigned char tf2_addr[ETH_ALEN]={0};

static unsigned int tf1_prev_rcv_num = 0;
static unsigned int tf2_prev_rcv_num = 0;

static unsigned int last_tf2_id = 0;
static struct net_device * dev_send2;
static struct batt_info batt_i;
static int rssi_avg[NUM_MCS] = {0};

static struct tr_info_list *snd_list = NULL;
static struct tr_info_list *rcv_list = NULL;

static unsigned char feedback_counter = 0;

static unsigned char child_name[20][ETH_ALEN];
static unsigned char n_child;


static void tl_rx_tr2_timer_func(unsigned long data){
	unsigned int qlen;	
	unsigned int size;
	struct tr_info *info;
	struct sk_buff *rpt = NULL;
	int i;
	//struct tr_info *test_info;

	//printk("tr2_timer_func\n");

	if(snd_list == NULL){
		printk("NULL snd_list\n");
	}

	qlen = snd_list->qlen;	
	size = ETHERHEADLEN + 6 + ((ETH_ALEN + 2 + 4*NUM_MCS) * qlen);
	info = snd_list->next;

	if(info == NULL){
		printk("Empty snd_list\n");
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
			info = snd_list->next;

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

static void tl_rx_tf1_timer_func(unsigned long data){
	struct tr_info *info;
	struct sk_buff *rpt = NULL;
	int i = 0;

	
	if((info = tr_info_find_addr(rcv_list, tf1_addr)) == NULL){
		printk("info is not in the list\n");
		return;
	}
/*		
	switch(rpt_type){
		case TypeOneTR : info = tf1_info; prev_rcv_num = &tf1_prev_rcv_num; tf_timer = &tl_rx_tf1_timer; hop=1; break;
		case TypeTwoTR : info = tf2_info; prev_rcv_num = &tf2_prev_rcv_num; tf_timer = &tl_rx_tf2_timer; hop=2; break;
		default : return;
	}
*/	
	if(tf1_prev_rcv_num < get_tot_rcv(info)){
		printk("Reset timer due to consecutive RX, prev_rcv_num1 = %d, info1->rcv_num = %d\n", tf1_prev_rcv_num, get_tot_rcv(info));
		tf1_prev_rcv_num = get_tot_rcv(info);
		mod_timer(&tl_rx_tf1_timer, jiffies + HZ);
		return;
	}
	else{
		printk("Del timer, prev_rcv_num1 = %d, info1->rcv_num = %d\n", tf1_prev_rcv_num, get_tot_rcv(info));
	//	if (timer_pending(tf_timer))
	//		del_timer_sync(tf_timer);		
	}
	
	if(info != NULL){
		rpt = tl_alloc_skb(info->dev, info->addr, info->dev->dev_addr, TR_SIZE, TypeOneTR);
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

		for (i=0; i < NUM_MCS; i++)
		{
			if(info->rcv_num[i]==0)
				rssi_avg[i]=0;
			else 
				rssi_avg[i] = rssi_avg[i]/(int)info->rcv_num[i];
		}

		printk(KERN_INFO "internal 1-hop training report rssi = %d, batt = %d rcv: (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d) rssi_mcs: (%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d)\n", info->rssi, info->batt, info->rcv_num[0], info->rcv_num[1], info->rcv_num[2], info->rcv_num[3], info->rcv_num[4], info->rcv_num[5], info->rcv_num[6], info->rcv_num[7], info->rcv_num[8], info->rcv_num[9], info->rcv_num[10], info->rcv_num[11], rssi_avg[0], rssi_avg[1], rssi_avg[2], rssi_avg[3], rssi_avg[4], rssi_avg[5], rssi_avg[6], rssi_avg[7], rssi_avg[8], rssi_avg[9], rssi_avg[10], rssi_avg[11]);
		
		dev_queue_xmit(rpt);
	}
	else{
		printk("Fail in tl_alloc_skb!!\n");
	}
}
bool set_batt_info(struct sk_buff *skb)
{	
	int udp_pos;

	if(skb->len > 42+16+6){
		printk("Too Large Packet\n");
		return false;
	}
	
	udp_pos = skb_transport_header(skb) - skb->data;	
	batt_i.m_status = skb->data[udp_pos+8];		
	batt_i.m_capacity = skb->data[udp_pos+9];

	printk("Set battery info, status: %d , capacity: %d\n", batt_i.m_status, batt_i.m_capacity);
	return true;
}

static void tl_rx_tf2_timer_func(unsigned long data){
	struct tr_info *info;
	struct sk_buff *rpt = NULL;
	int i = 0;

	
	if((info = tr_info_find_addr(rcv_list, tf2_addr)) == NULL){
		printk("info is not in the list\n");
		return;
	}
	if(tf2_prev_rcv_num < get_tot_rcv(info)){
		printk("Reset timer due to consecutive RX, prev_rcv_num2 = %d, info2->rcv_num = %d\n", tf2_prev_rcv_num, get_tot_rcv(info));
		tf2_prev_rcv_num = get_tot_rcv(info);
		mod_timer(&tl_rx_tf2_timer, jiffies + HZ);
		return;
	}
	else{
		printk("Del timer, prev_rcv_num2 = %d, info2->rcv_num = %d\n", tf2_prev_rcv_num, get_tot_rcv(info));
	//	if (timer_pending(tf_timer))
	//		del_timer_sync(tf_timer);		
	}
	
	if(info != NULL){
		rpt = tl_alloc_skb(info->dev, info->addr, info->dev->dev_addr, TR_SIZE, TypeTwoTR);
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
		printk(KERN_INFO "send 2-hop training report \n rssi = %d, batt = %d rcv0 = %d, rcv1 = %d  rcv2 = %d rcv3 = %d rcv4 = %d rcv5 = %d rcv6 = %d rcv7 = %d rcv8 = %d rcv9 = %d rcv10 = %d rcv11 = %d\n", info->rssi, info->batt, info->rcv_num[0], info->rcv_num[1], info->rcv_num[2], info->rcv_num[3], info->rcv_num[4], info->rcv_num[5], info->rcv_num[6], info->rcv_num[7], info->rcv_num[8], info->rcv_num[9], info->rcv_num[10], info->rcv_num[11]);
		dev_queue_xmit(rpt);
	}
	else{
		printk("Fail in tl_alloc_skb!!\n");
	}
}
void tl_receive_skb_dst(struct sk_buff *skb, char rssi, unsigned char mcs){
	enum tr_type skb_type = skb->data[0];

	unsigned char *skb_daddr = skb_mac_header(skb);
	unsigned char *skb_saddr = skb_mac_header(skb) + 6;

	static bool for_init = true;
	
	if(memcmp(skb->dev->dev_addr, skb_daddr, ETH_ALEN) && memcmp(skb_daddr, multicast_addr, ETH_ALEN)){
		printk("Invalid destination addr (Neither unicast and multicast)\n");	
		dev_kfree_skb(skb);
		return;
	}
	
	if(for_init == true){
		snd_list = kmalloc(sizeof(struct tr_info_list), GFP_ATOMIC);
		rcv_list = kmalloc(sizeof(struct tr_info_list), GFP_ATOMIC);
		
		tr_info_list_init(snd_list);
		tr_info_list_init(rcv_list);
		
		setup_timer(&tl_rx_tf1_timer, &tl_rx_tf1_timer_func, TypeOneTR);
		setup_timer(&tl_rx_tf2_timer, &tl_rx_tf2_timer_func, TypeTwoTR);
		setup_timer(&tl_rx_tr2_timer, &tl_rx_tr2_timer_func, 0);

		for_init = false;

		printk("Initialization Destination Module\n");
	}


	if(skb_type == TypeOneTF){
		static unsigned int tf1_cur_index = 0;
		unsigned int tf1_k = (unsigned int) skb->data[1] << 24 | skb->data[2] << 16 | skb->data[3] << 8 | skb->data[4];
		unsigned int tf1_seq = (unsigned int) skb->data[5] << 24 | skb->data[6] << 16 | skb->data[7] << 8 | skb->data[8];
		unsigned int tf1_index = (unsigned int) skb->data[9] << 24 | skb->data[10] << 16 | skb->data[11] << 8 | skb->data[12];
		unsigned char mcs = (unsigned char) skb->data[13];
		
		printk(KERN_INFO "skb type: TypeOne k: %d seq: %d id: %d mcs: %d rssi: %d\n", tf1_k, tf1_seq, tf1_index, mcs, rssi);
		//
		
		if (mcs > NUM_MCS){
			printk("ERROR, invalid MCS index\n");
			dev_kfree_skb(skb);
			return; 
		}	

		if(tf1_k < tf1_seq){
			printk("ERROR, tf1_k is lower than tf1_seq\n");
		}
		else{
			struct tr_info *info;
			//struct tr_info *info;
			memcpy(tf1_addr, skb_saddr, ETH_ALEN);
			// Initialize
			if((info = tr_info_find_addr(rcv_list, skb_saddr)) == NULL){
				unsigned int rcv[NUM_MCS];
				unsigned char batt = 0;
				memset(rcv, 0, sizeof(unsigned int)*NUM_MCS); // may incur an error
				memset(rssi_avg, 0, sizeof(int)*NUM_MCS);
				tr_set_param(false, false, 10, 0, 0, 0, 0, 0, 0);
				batt= set_batt(batt_i.m_status, batt_i.m_capacity);
				info = tr_info_create(skb_saddr, skb->dev, tf1_k, rcv, rssi, batt);
				printk(KERN_INFO "Initialize skb type : TypeOne k: %d seq: %d id: %d mcs: %d\n", tf1_k, tf1_seq, tf1_index, mcs);
				(info->rcv_num[mcs])++;
				info->rssi = rssi;
				
				tr_info_insert(info, rcv_list);
				tf1_prev_rcv_num = 1;
				tf1_cur_index = tf1_index;

				memcpy(parent_addr, skb_saddr, ETH_ALEN);
			}
			//else if(tf1_cur_index == tf1_index){
			if(tf1_cur_index == tf1_index){
				(info->rcv_num[mcs])++;
				//printk("prev rssi %d current rssi %d int rssi_old %d int rssi_new %d\n", tf1_info->rssi, rssi, (int)tf1_info->rssi, (int)rssi);
				info->rssi = (char)((16*8*(int)rssi + (16-8)*(int)info->rssi*16)/16/16);
				rssi_avg[mcs] += (int)rssi; 
				if(!timer_pending(&tl_rx_tf1_timer)) mod_timer(&tl_rx_tf1_timer, jiffies + HZ); //debugging
			}
			else{
				unsigned int rcv[NUM_MCS]; //may incur an error
				memset(rcv, 0, sizeof(unsigned int)*NUM_MCS); //may incur an error
				memset(rssi_avg, 0, sizeof(int)*NUM_MCS);
				tr_set_param(false, false, 10, 0, 0, 0, 0, 0, 0);
				printk(KERN_INFO "Reset Receive first TypeOneTf, set parameter src = %d, sys = %d, data_k = %d, data_n = %d, tf_k = %d, tf_thre = %d, max_relay_n = %d\n", tr_get_src(), tr_get_sys(), tr_get_data_k(), tr_get_data_n(), tr_get_tf_k(), tr_get_tf_thre(), tr_get_max_relay_n());
				
				info->total_num = tf1_k;
				memset(info->rcv_num, 0, sizeof(unsigned int)*NUM_MCS);
				(info->rcv_num[mcs])++;
				info->rssi = rssi;
				
				tf1_prev_rcv_num = 1;
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
		
		//printk(KERN_INFO "skb type: TypeTwo  k: %d seq: %d id: %d mcs: %d\n", tf2_k, tf2_seq, tf2_index, mcs);
		
		if(tf2_k < tf2_seq){
			printk("ERROR, tf2_k is lower than tf2_seq\n");
		}
		else{
			struct tr_info *info;
			memcpy(tf2_addr, skb_saddr, ETH_ALEN);
			/// Initialize
			if((info = tr_info_find_addr(rcv_list, skb_saddr)) == NULL){
				unsigned int rcv[NUM_MCS];
				unsigned char batt = 0;
				memset(rcv, 0, sizeof(unsigned int)*NUM_MCS); //may incur an error
				memset(rssi_avg, 0, sizeof(int)*NUM_MCS);
				tr_set_param(false, false, 10, 0, 0, 0, 0, 0, 0);
				batt = set_batt(batt_i.m_status, batt_i.m_capacity);
				info = tr_info_create(skb_saddr, skb->dev, tf2_k, rcv, rssi, batt);
				printk(KERN_INFO "Initialize skb type : TypeOne k: %d seq: %d id: %d mcs: %d\n", tf2_k, tf2_seq, tf2_index, mcs);
				(info->rcv_num[mcs])++;
				info->rssi = rssi;
				
				tr_info_insert(info, rcv_list);
				tf2_prev_rcv_num = 1;
				tf2_cur_index = tf2_index;
			}
			else if(tf2_cur_index == tf2_index){
				(info->rcv_num[mcs])++;
				info->rssi = (char)((16*8*(int)rssi + (16-8)*(int)info->rssi*16)/16/16);
				rssi_avg[mcs] += (int)rssi; 
				if(!timer_pending(&tl_rx_tf2_timer)) mod_timer(&tl_rx_tf2_timer, jiffies + HZ);
			}
			else{
				unsigned int rcv[NUM_MCS]; //may incur an error
				memset(rcv, 0, sizeof(unsigned int)*NUM_MCS); //may incur an error
				memset(rssi_avg, 0, sizeof(int)*NUM_MCS);
				tr_set_param(false, false, 10, 0, 0, 0, 0, 0, 0);
				printk(KERN_INFO "Reset Receive first TypeOneTf, set parameter src = %d, sys = %d, data_k = %d, data_n = %d, tf_k = %d, tf_thre = %d, max_relay_n = %d\n", tr_get_src(), tr_get_sys(), tr_get_data_k(), tr_get_data_n(), tr_get_tf_k(), tr_get_tf_thre(), tr_get_max_relay_n());
				
				info->total_num = tf2_k;
				memcpy(info->rcv_num, rcv, sizeof(unsigned int)*NUM_MCS);
				(info->rcv_num[mcs])++;
				info->rssi = rssi;
				
				tf2_prev_rcv_num = 1;
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
				printk("Receive TypeTwoTR Message(%d); rssi = %x, batt = %x, rcv0 = %d, rcv1 = %d, rcv2 = %d, rcv3 = %d, rcv4 = %d, rcv5 = %d, rcv6 = %d, rcv7 = %d, rcv8 = %d, rcv9 = %d, rcv10 = %d, rcv11 = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, rssi, batt, n_rcv[0],  n_rcv[1], n_rcv[2], n_rcv[3], n_rcv[4], n_rcv[5], n_rcv[6], n_rcv[7], n_rcv[8], n_rcv[9], n_rcv[10], n_rcv[11],skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);

				if((info = tr_info_find_addr(snd_list, skb_saddr)) == NULL){
					tr_info_insert(tr_info_create(skb_saddr, skb->dev, sdf_info->total_num, n_rcv, rssi, batt), snd_list);
				}
				else{
					info->dev = skb->dev;
					info->total_num = sdf_info->total_num;
					memcpy(info->rcv_num, n_rcv, sizeof(unsigned int)*NUM_MCS);
					info->rssi = rssi;
					info->batt = batt;
					info->tf_cnt = false;
					info->nr_cnt = false;
				}
				mod_timer(&tl_rx_tr2_timer, jiffies + HZ);
			}
			else{
				printk("sdf_info is NULL when TR2 is arrived\n");
			}
		}
	}

	else if(skb_type == SendTF){
		unsigned int skb_k = (unsigned int) skb->data[1] << 24 | skb->data[2] << 16 | skb->data[3] << 8 | skb->data[4];
		if(!memcmp(skb->dev->dev_addr, skb_daddr, ETH_ALEN)){
			printk("Receive SendTF Message(%d); SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);
			if(sdf_info == NULL){
				unsigned int rcv[NUM_MCS];
				memset(rcv, 0, sizeof(unsigned int)*NUM_MCS);

				dev_send2 = skb->dev;
				sdf_info = tr_info_create(skb_saddr, skb->dev, skb_k, rcv, 0, 0);
				printk("Send TypeTwoTF Message(%d); k = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", TypeTwoTF, skb_k, skb->dev->dev_addr[0], skb->dev->dev_addr[1], skb->dev->dev_addr[2], skb->dev->dev_addr[3], skb->dev->dev_addr[4], skb->dev->dev_addr[5], multicast_addr[0], multicast_addr[1], multicast_addr[2], multicast_addr[3], multicast_addr[4], multicast_addr[5]);
			
				tl_2hop_mcs_send_timer_func(0);
			}
			else{
				printk("Receive duplicate SendTF\n");
				//tl_rx_tr2_timer_func((unsigned long)(&snd_list));
			}
		}
	}

		else if(skb_type == SetRelay){
			if(!memcmp(skb->dev->dev_addr, skb_daddr, ETH_ALEN)){
				unsigned char skb_k = skb->data[1];
				unsigned char skb_n = skb->data[2];
				unsigned char mcs = skb->data[3];
				unsigned int offset = (unsigned int) skb->data[4] << 24 | skb->data[5] << 16 | skb->data[6] << 8 | skb->data[7];
				unsigned char i;

				n_child = skb->data[8];
				memcpy(parent_addr, skb_saddr, ETH_ALEN);

				tr_set_param(false, false, skb_k, skb_n, 0, 0, 0, mcs, offset);
				printk("Receive SetRelay, set parameter src = %d, sys = %d, data_k = %d, data_n = %d, tf_k = %d, tf_thre = %d, max_relay_n = %d, mcs = %d, offset = %d\n", tr_get_src(), tr_get_sys(), tr_get_data_k(), tr_get_data_n(), tr_get_tf_k(), tr_get_tf_thre(), tr_get_max_relay_n(), tr_get_mcs(), tr_get_offset());

				if(!(n_child == 0 && skb_n == 0)){
					for(i = 0; i < n_child; i++){
						struct sk_buff* rpt;
						int ii = 9 + i * ETH_ALEN;
						memcpy(child_name[i], &(skb->data[ii]), ETH_ALEN);

						rpt = tl_alloc_skb(skb->dev, child_name[i], skb->dev->dev_addr, ETHERHEADLEN+9, SetRelay);
						rpt->data[ETHERHEADLEN + 1] = skb_k;
						rpt->data[ETHERHEADLEN + 2] = 0;
						rpt->data[ETHERHEADLEN + 3] = 0;
						rpt->data[ETHERHEADLEN + 4] = 0;
						rpt->data[ETHERHEADLEN + 5] = 0;
						rpt->data[ETHERHEADLEN + 6] = 0;
						rpt->data[ETHERHEADLEN + 7] = 0;
						rpt->data[ETHERHEADLEN + 8] = 0;
						dev_queue_xmit(rpt);
						printk("Send SetRelay Message(%d); k = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", SetRelay, skb_k, skb->dev->dev_addr[0], skb->dev->dev_addr[1], skb->dev->dev_addr[2], skb->dev->dev_addr[3], skb->dev->dev_addr[4], skb->dev->dev_addr[5], child_name[i][0], child_name[i][1], child_name[i][2], child_name[i][3], child_name[i][4], child_name[i][5]);
					}
				}

				printk("My parent = %x:%x:%x:%x:%x:%x\n", parent_addr[0], parent_addr[1], parent_addr[2], parent_addr[3], parent_addr[4], parent_addr[5]);
				for(i = 0; i < n_child; i++){
					printk("Child[%d][] = %x:%x:%x:%x:%x:%x\n", i, child_name[i][0], child_name[i][1], child_name[i][2], child_name[i][3], child_name[i][4], child_name[i][5]);
				}
			}

		/****** It may be unicast frame *******
		else if(!memcmp(skb_daddr, multicast_addr, ETH_ALEN)){
			if (tr_get_data_n() == 0){
				unsigned char n_child = skb->data[8];
				unsigned char i=0;
				unsigned char j=0;

				for (i=0; i<n_child; i++){
					j = 9 + i*ETH_ALEN;
					if(!memcmp(dev_send2->dev_addr, &(skb->data[j]), ETH_ALEN)){
						memcpy(parent_addr, skb_saddr, ETH_ALEN);
						printk("Set Parent Address [%x:%x:%x:%x:%x:%x]\n", parent_addr[0], parent_addr[1], parent_addr[2], parent_addr[3],parent_addr[4], parent_addr[5]);	
					}
				}
			}
		}
		*/
	}
	else if(skb_type == TF_REQ){
		struct sk_buff *rpt; 
		unsigned int len; 
		unsigned int size;
		
		printk("Receive TF_REQ Message (%d) from [%x:%x:%x:%x:%x:%x]\n", skb_type, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5]);
		
		if(rcv_list == NULL){
			len = 0;
			printk("NULL rcv_list\n");
		}
		else{
			len = rcv_list->qlen;
		}
		
		size =  ETHERHEADLEN + 3 + ((ETH_ALEN + 2)*len); // 3: 1 type 1 batt 1 len
		rpt = tl_alloc_skb(dev_send2, skb_saddr, dev_send2->dev_addr, size, TF_RPT);

		if(rpt != NULL){
			unsigned int i = 0;
			struct tr_info *info = NULL;
			if(rcv_list != NULL)
				info = rcv_list->next;

			rpt->data[ETHERHEADLEN + 1] = set_batt(batt_i.m_status, batt_i.m_capacity);
			rpt->data[ETHERHEADLEN + 2] = len;

			for(i = 0; i < len; i++){
				unsigned int ii = ETHERHEADLEN + 3 + i*(ETH_ALEN+2);

				if(info == NULL){
					printk("Empty info\n");
					break;
				}	

				memcpy(&(rpt->data[ii]), info->addr, ETH_ALEN);
				rpt->data[ii+ETH_ALEN] = info->rssi;
				rpt->data[ii+ETH_ALEN+1] = info->batt;
				info = info->next;
			}
			
			printk("Send TF_RPT(%d), skb_saddr = %x:%x:%x:%x:%x:%x with batt %d, len %d\n", TF_RPT, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], batt_i.m_capacity, len);
			dev_queue_xmit(rpt);
		}
		

		/**** Maybe it will be deleted *****

		if(rcv_list == NULL){
			struct sk_buff *rpt; 
			size = ETHERHEADLEN + 4;
			rpt=tl_alloc_skb(dev_send2, skb_saddr, dev_send2->dev_addr, size, TF_RPT);
			
			printk("NULL snd_list\n");
			printk("Receive TF_REQ Message (%d) from [%x:%x:%x:%x:%x:%x]\n", skb_type, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5]);
			
			if(rpt != NULL){
				rpt->data[ETHERHEADLEN + 1] = rssi;
				rpt->data[ETHERHEADLEN + 2] = set_batt(batt_i.m_status, batt_i.m_capacity);
				rpt->data[ETHERHEADLEN + 3] = 0;
			
				printk("Send TF_RPT Message\n");
				dev_queue_xmit(rpt);
			} 
			else{
				printk("Fail to alloc skb\n");
			}
		}
		else{
			unsigned char len = rcv_list->qlen;	
			struct tr_info *info = rcv_list->next;
			unsigned int size = ETHERHEADLEN + 3 + ((ETH_ALEN + 1) * len);
			struct sk_buff *rpt = tl_alloc_skb(dev_send2, skb_saddr, dev_send2->dev_addr, size, TF_RPT);

			if(rpt!=NULL){
				unsigned int i = 0;
				rpt->data[ETHERHEADLEN + 1] = set_batt(batt_i.m_status, batt_i.m_capacity);
				rpt->data[ETHERHEADLEN + 2] = len;

				for (i=0; i < len; i++){
					unsigned int ii = ETHERHEADLEN + 3 + i*(ETH_ALEN+1);
					memcpy(&(rpt->data[ii]), info->addr, ETH_ALEN);
					rpt->data[ii+ETH_ALEN] = info->rssi;
					info = rcv_list->next;

					if(info == NULL){
						printk("Empty info\n");
						break;
					}	
				}	
				printk("Send TF PRT(%d) with charge %d capa %d\n", TF_RPT, batt_i.m_status, batt_i.m_capacity);
				dev_queue_xmit(rpt);
			}
			else{
				printk("Fail in tl_alloc_skb!!\n");
			}
		}
		*/

	}
	else if(skb_type == TF_RPT){
		if(!memcmp(skb->dev->dev_addr, skb_daddr, ETH_ALEN) && tr_get_data_n() > 0  ){
			struct sk_buff * rpt;
			unsigned int size = 0;
			unsigned char num_nbr = skb->data[2];			

			size = ETHERHEADLEN + 3 + num_nbr*(ETH_ALEN+1);
			rpt=tl_alloc_skb(dev_send2, sdf_info->addr, dev_send2->dev_addr, size, SetRelay);
			if(rpt != NULL){
				memcpy(&(rpt->data[ETHERHEADLEN+1]), &(skb->data[1]), size-ETHERHEADLEN-1);
				dev_queue_xmit(rpt);
				printk("Relay Runtime RPT Packet\n");
			}
			else{
				printk("Fails to alloc skb\n");
			}
		}
		else{
			printk("I am not a relay\n");
		}
	} 
	else if(skb_type == BLOCK_NACK){
		unsigned char * dest_addr = NULL;
		if(!memcmp(skb->dev->dev_addr, skb_daddr, ETH_ALEN)){
			memcpy(dest_addr, sdf_info->addr, ETH_ALEN);
		}
		else if (!memcmp(skb_daddr, multicast_addr, ETH_ALEN)){
			memcpy(dest_addr, parent_addr, ETH_ALEN);
		}
		else{
			printk("Invalid address\n");
		}

		if(dest_addr != NULL){
			struct sk_buff * rpt;
			unsigned int size = 0;
			unsigned char num_nbr = skb->data[2];			

			size = ETHERHEADLEN + 3 + num_nbr*(ETH_ALEN+1);
			rpt=tl_alloc_skb(dev_send2, sdf_info->addr, dev_send2->dev_addr, size, SetRelay);
			if(rpt != NULL){
				memcpy(&(rpt->data[ETHERHEADLEN+1]), &(skb->data[1]), size-ETHERHEADLEN-1);
				dev_queue_xmit(rpt);
				printk("Relay Block NACK Packet\n");
			}
			else{
				printk("Fails to alloc skb\n");
			}
		}
	}
	else{
		printk("CLIENT DON'T RECEIVE %d!!!; SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", skb_type, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5], skb_daddr[0], skb_daddr[1], skb_daddr[2], skb_daddr[3], skb_daddr[4], skb_daddr[5]);
	}

//	netif_receive_skb(skb);	
	dev_kfree_skb(skb);
}


static void tl_2hop_mcs_send_timer_func(unsigned long data){
	static unsigned char mcs = 0;
	static unsigned int tr_id = 0;
	unsigned int i = 0;
	
	if (mcs == 0){	
		setup_timer(&tl_2hop_mcs_send_timer, &tl_2hop_mcs_send_timer_func, 0 );
		get_random_bytes(&tr_id, 4);
		if (tr_id == last_tf2_id){
			get_random_bytes(&tr_id, 4);
		}
		last_tf2_id = tr_id;
	}

		printk("Send TypeTwoTF Message(%d), tf_k = %d with MCS %x\n", TypeTwoTF, sdf_info->total_num, mcs);
	for(i = 0; i < sdf_info->total_num; i++){
		struct sk_buff *otf = tl_alloc_skb(dev_send2, multicast_addr, dev_send2->dev_addr, TF_SIZE, TypeTwoTF);
		if(otf != NULL){
			struct ieee80211_tx_info *info = IEEE80211_SKB_CB(otf);
			otf->data[ETHERHEADLEN + 1] = (sdf_info->total_num >> 24) & 0xff;
			otf->data[ETHERHEADLEN + 2] = (sdf_info->total_num >> 16) & 0xff;
			otf->data[ETHERHEADLEN + 3] = (sdf_info->total_num >> 8) & 0xff;
			otf->data[ETHERHEADLEN + 4] = sdf_info->total_num & 0xff;
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
		unsigned int tx_time = cal_tx_time (mcs, sdf_info->total_num, TF_SIZE)/1000;
		printk("Schedule next TX after %d ms with MCS %d\n", tx_time, mcs);	
		if(!timer_pending(&tl_2hop_mcs_send_timer))	
			mod_timer(&tl_2hop_mcs_send_timer, jiffies + msecs_to_jiffies(tx_time));
		
		mcs++;
	}
	else
	{
		if(timer_pending(&tl_2hop_mcs_send_timer))	
			del_timer(&tl_2hop_mcs_send_timer);
		
		mcs = 0;
	}
}

void update_rssi(struct sk_buff *skb, char rssi){
	unsigned char *skb_saddr = skb_mac_header(skb) + ETH_ALEN;

	if(rcv_list == NULL){
		printk("Error, empty rcv list\n");
	}
	else{
		struct tr_info *info;
		if((info = tr_info_find_addr(rcv_list, skb_saddr)) == NULL){
			//unsigned int rcv[NUM_MCS];
			//memset(rcv, 0, sizeof(unsigned int)*NUM_MCS); //may incur an error
			//info = tr_info_create(skb_saddr, skb->dev, 1, rcv, rssi, set_batt(batt_i.m_status, batt_i.m_capacity));
			//tr_info_insert(info, rcv_list);
			unsigned int rcv[NUM_MCS];
			unsigned char batt = 0;
			memset(rcv, 0, sizeof(unsigned int)*NUM_MCS); //may incur an error
			//memset(rssi_avg, 0, sizeof(int)*NUM_MCS); //gjlee?
			tr_set_param(false, false, 10, 0, 0, 0, 0, 0, 0);
			batt = set_batt(batt_i.m_status, batt_i.m_capacity);
			info = tr_info_create(skb_saddr, skb->dev, 1, rcv, rssi, batt);
			printk("update rssi: %d; New SA = %x:%x:%x:%x:%x:%x\n", rssi, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5]);
			//(info->rcv_num[mcs])++; // gjlee?
			info->rssi = rssi;
			tr_info_insert(info, rcv_list);
		}
		else{
			info->rssi = (char)((16*8*(int)rssi + (16-8)*(int)info->rssi*16)/16/16);
			//printk("update rssi = %d, info->rssi = %d, SA = %x:%x:%x:%x:%x:%x\n", rssi, info->rssi, skb_saddr[0], skb_saddr[1], skb_saddr[2], skb_saddr[3], skb_saddr[4], skb_saddr[5]);
		}
	}		
}

static void tl_periodic_feedback(unsigned long data){
	if(parent_addr == NULL){
		printk("Error, no parent addr\n");
	}
	else if(rcv_list == NULL){
		printk("Error, empty rcv list\n");
	} 
	else if (rcv_list->qlen == 0){
		printk("Zero list\n");
	} 
	else{
		struct tr_info *info = rcv_list->next;
		unsigned int len = rcv_list->qlen; 
		unsigned int size =  ETHERHEADLEN + 3 + ((ETH_ALEN + 2)*len); // 3: 1 type 1 batt 1 len
		struct sk_buff *rpt = tl_alloc_skb(dev_send2, parent_addr, dev_send2->dev_addr, size, U_FB); 
		if(rpt!=NULL){
			unsigned int i = 0;
			rpt->data[ETHERHEADLEN + 1] = set_batt(batt_i.m_status, batt_i.m_capacity);
			rpt->data[ETHERHEADLEN + 2] = len;

			for (i = 0; i < len; i++){
				unsigned int ii = ETHERHEADLEN + 3 + i*(ETH_ALEN+2);
				memcpy(&(rpt->data[ii]), info->addr, ETH_ALEN);
				rpt->data[ii+ETH_ALEN] = info->rssi;
				rpt->data[ii+ETH_ALEN+1] = info->batt;

				info = rcv_list->next; // info = info->next;

				if(info == NULL){
					printk("Empty info\n");
					break;
				}
			}
			printk("Send Periodic Feedback(%d), parent = %x:%x:%x:%x:%x:%x with charge %d batt %d\n", U_FB, parent_addr[0], parent_addr[1], parent_addr[2], parent_addr[3], parent_addr[4], parent_addr[5], batt_i.m_status, batt_i.m_capacity);
			dev_queue_xmit(rpt);
		}		
	}

	if(feedback_counter < 10){
		if(!timer_pending(&tl_feedback_timer))
			mod_timer(&tl_feedback_timer, jiffies + msecs_to_jiffies(FB_PERIOD));
		feedback_counter++;
	}
	else{
		feedback_counter = 0;
	}
}

void send_bnack(unsigned char *dest_addr){
	struct sk_buff *rpt; 
	unsigned int len; 
	unsigned int size;

	if(rcv_list == NULL){
		printk("send_bnack Error, NULL rcv_list\n");
	}
		
	len = rcv_list->qlen; 
	size =  ETHERHEADLEN + 3 + ((ETH_ALEN + 2)*len); // 3: 1 type 1 batt 1 len
	rpt = tl_alloc_skb(dev_send2, dest_addr, dev_send2->dev_addr, size, BLOCK_NACK); 
	
	if(rpt != NULL){
		unsigned int i = 0;
		struct tr_info *info = rcv_list->next;
		
		rpt->data[ETHERHEADLEN + 1] = set_batt(batt_i.m_status, batt_i.m_capacity);
		rpt->data[ETHERHEADLEN + 2] = len;

		for(i = 0; i < len; i++){
			unsigned int ii = ETHERHEADLEN + 3 + i*(ETH_ALEN+2);
			
			if(info == NULL){
				printk("Empty info\n");
				break;
			}	
			
			memcpy(&(rpt->data[ii]), info->addr, ETH_ALEN);
			rpt->data[ii+ETH_ALEN] = info->rssi;
			rpt->data[ii+ETH_ALEN+1] = info->batt;
			info = info->next;
		}

		printk("Send BLOCK NACK(%d), dest_addr = %x:%x:%x:%x:%x:%x with batt %d, len %d\n", BLOCK_NACK, dest_addr[0], dest_addr[1], dest_addr[2], dest_addr[3], dest_addr[4], dest_addr[5], batt_i.m_capacity, len);
		dev_queue_xmit(rpt);
	}

	/***** Maybe it will be deleted

	if(rcv_list == NULL || (rcv_list != NULL && rcv_list->qlen == 0)){
		unsigned int size = ETHERHEADLEN + 3;
		printk("Empty or zero rcv list : SEND NULL BNACK\n");
		rpt = tl_alloc_skb(dev_send2, dest_addr, dev_send2->dev_addr, size, BLOCK_NACK);
		if(rpt!=NULL){
			rpt->data[ETHERHEADLEN + 1] = set_batt(batt_i.m_status, batt_i.m_capacity);
			rpt->data[ETHERHEADLEN + 2] = 0;
			dev_queue_xmit(rpt);
		}
	}
	else{	
		struct tr_info *info = rcv_list->next;
		unsigned int len = rcv_list->qlen; 
		unsigned int size =  ETHERHEADLEN + 3 + ((ETH_ALEN + 1)*len); // 3: 1 type 1 batt 1 len
		rpt = tl_alloc_skb(dev_send2, dest_addr, dev_send2->dev_addr, size, BLOCK_NACK); 
		if(rpt!=NULL){
			unsigned int i = 0;
			rpt->data[ETHERHEADLEN + 1] = set_batt(batt_i.m_status, batt_i.m_capacity);
			rpt->data[ETHERHEADLEN + 2] = len;

			for (i=0; i < len; i++){
				unsigned int ii = ETHERHEADLEN + 3 + i*(ETH_ALEN+1);
				memcpy(&(rpt->data[ii]), info->addr, ETH_ALEN);
				rpt->data[ii+ETH_ALEN] = info->rssi;
				info = rcv_list->next;

				if(info == NULL){
					printk("Empty info\n");
					break;
				}	
			}	
			printk("Send BLOCK NACK(%d), dest_addr = %x:%x:%x:%x:%x:%x with batt %d\n", BLOCK_NACK, dest_addr[0], dest_addr[1], dest_addr[2], dest_addr[3], dest_addr[4], dest_addr[5], batt_i.m_capacity);
			dev_queue_xmit(rpt);

		}
	}
	*/
}

void init_runtime(void){
	static bool run_init = false;
	if(run_init == false){
		setup_timer(&tl_feedback_timer, &tl_periodic_feedback, 0);
		run_init = true;
	}

	if(!timer_pending(&tl_feedback_timer)){
		mod_timer(&tl_feedback_timer, jiffies + msecs_to_jiffies(FB_PERIOD));
	}
	
	feedback_counter = 0;
}
