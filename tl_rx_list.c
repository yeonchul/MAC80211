////Modified_130619_GJLEE

#include "tl_rx.h"
#define TIME_LIMIT 10000
#define LOOKUP 1

static struct timespec tl_start, tl_end;

void tl_start_time(){
	getrawmonotonic(&tl_start);
}

struct sk_buff *tl_alloc_skb(struct net_device *dev, unsigned char daddr[], unsigned char saddr[], unsigned int size, enum tr_type type){
	struct sk_buff *rpt;

	if(size < ETHERHEADLEN + 1){
		printk("TOO SMALL SIZE\n");
		return NULL;
	}

	rpt = dev_alloc_skb(size);

	if(rpt != NULL){
		rpt->dev = dev;
		rpt->protocol = 0x1180;
		skb_reset_transport_header(rpt);
		skb_reset_network_header(rpt);

		skb_put(rpt, size);

		memcpy(&(rpt->data[0]), daddr, ETH_ALEN);
		memcpy(&(rpt->data[ETH_ALEN]), saddr, ETH_ALEN);
		rpt->data[12] = 0x08;
		rpt->data[13] = 0x11;
		rpt->data[ETHERHEADLEN] = type;
	}

	return rpt;
}

unsigned char n_to_k(unsigned char n, unsigned int total, unsigned int rcv){
	if (n==0)
		return 0;
	else if (!LOOKUP){
		unsigned long result = 0;
		result = (unsigned long) n * (unsigned long) rcv * (unsigned long) rcv * 80 * 1000000;
		result /= total;
		result /= total;
		result /= 100;
		result /= 1000000;

		//printk("n_to_k result = %ld, n = %d, total = %d, rcv = %d\n", result, n, total, rcv);
		return (unsigned char) result;
	}

	else{
		unsigned char result = 0;
		unsigned int p = (rcv*100)/total;
		result = get_n_to_k_table(n, (unsigned char)p);
		//printk("n_to_k result = %d, n = %d, total = %d, rcv = %d\n", result, n, total, rcv);
		return result;
	}
}

unsigned char k_to_n(unsigned char k, unsigned int total, unsigned int rcv){
	if (k==0)
		return 0;	
	else if (!LOOKUP){
		unsigned long result = (unsigned long) k * (unsigned long) total * (unsigned long) total * 100 * 1000000;
		result /= rcv;
		result /= rcv;
		result /= 80;
		if((result % 1000000) != 0) result += 1000000;
		result /= 1000000;
		//printk("k_to_n result = %ld, k = %d, total = %d, rcv = %d\n", result, k, total, rcv);
		return (unsigned char) (result < 255 ? result : 255);
	}
	else{
		unsigned int p = (rcv*100)/total;
		unsigned char result = get_k_to_n_table(k, (unsigned char)p); 
		//printk("k_to_n result = %d, k = %d, total = %d, rcv = %d\n", result, k, total, rcv);
		return result;
	}
}

bool dst_all_zero(struct dst_info_list *list){
	struct dst_info * info;

	if (list == NULL) return true;		
	info = list->next;

	while(info != NULL){
		if(info->pr_dof > 0) return false;
		info = info->next;
	}
	return true;
}

bool vimor_all_zero(struct vimor_info_list *list){
	struct vimor_info * info;

	if (list == NULL) return true;		
	info = list->next;

	while(info != NULL){
		if(info->hop == 0) return false;
		info = info->next;
	}
	return true;
}

unsigned char get_pr_dof(struct dst_info_list *list, unsigned char addr[])
{
		struct dst_info * info;
		info = dst_info_find_addr(list, addr);
		if (info == NULL){
			printk("No Dst info with such addr\n");
			return 0;
		}	
		else{
			return info->pr_dof;
		}
}


unsigned char find_best_mcs(struct tr_info * info, unsigned char dof){
	unsigned char i = 0;
	unsigned int temp_value = 10000000;
	unsigned char ret = 0;
		
	for(i=0; i<NUM_MCS; i++){
		unsigned int time = cal_tx_time(i, 1, 1316);
		unsigned int rcv = info->rcv_num[i];
		unsigned int value = 0;	
		unsigned char n = k_to_n(dof, info->total_num, rcv);

		value = (unsigned int)n*time;	

		if (n ==0 || time==0)
			continue;
		
		//printk("MCS %d Rcv: %d Time: %d Value: %d\n", i, rcv, time, value);
		if (value < temp_value)
		{
			temp_value = value;
			ret = i;
		}
	}

//	printk("Selected MCS: %d\n", ret);
	return ret;
}

unsigned char find_mcs(struct tr_info * info){
	unsigned char i = 0;
	unsigned int temp_value = 0;
	unsigned char ret = 0;
		
	for(i=0; i<NUM_MCS; i++){
		unsigned int time = cal_tx_time(i, 1, 1316);
		unsigned int rcv = info->rcv_num[i];
		unsigned int value = 0;	

		if (time == 0) 
			continue;

		value = rcv*1000000/time;	
		
		//printk("MCS %d Rcv: %d Time: %d Value: %d\n", i, rcv, time, value);
		if (value > temp_value)
		{
			temp_value = value;
			ret = i;
		}
	}
	return ret;
}

unsigned int find_ett(struct tr_info * info){
	unsigned char i = 0;
	unsigned int temp_value = 100000000;
	unsigned char ret = 0;
		
	for(i=0; i<NUM_MCS; i++){
		unsigned int time = cal_tx_time(i, 1, 1316);
		unsigned int rcv = info->rcv_num[i];
		unsigned int value = 0;	
		unsigned int tot = info->total_num;

		if (rcv == 0) 
			continue;

		value = time*tot/rcv;	
		
		//printk("MCS %d Rcv: %d Time: %d Value: %d\n", i, rcv, time, value);
		if (value < temp_value)
		{
			temp_value = value;
			ret = i;
		}
	}
//	printk("Selected MCS: %d\n", ret);
	return temp_value;
}



void tr_info_list_del_nbr(struct tr_info_list *list, unsigned char addr[]){
	unsigned char nbr_addr[ETH_ALEN];
	struct tr_info *temp_info = list->next;
	memcpy(nbr_addr, addr, ETH_ALEN);
	while(temp_info != NULL){
		if(memcmp(nbr_addr, temp_info->addr, ETH_ALEN)){
			struct tr_info *temp_nbr_info = (temp_info->nbr_list).next;
			while(temp_nbr_info != NULL){
				if(!memcmp(nbr_addr, temp_nbr_info->addr, ETH_ALEN)){
					tr_info_free(temp_nbr_info);
					break;
				}
				temp_nbr_info = temp_nbr_info->next;
			}
		}
		temp_info = temp_info->next;
	}
}

void tr_info_list_arrangement(struct tr_info_list *list){
	struct tr_info *info;
	struct tr_info *test_info;

	if(list == NULL) return;

	info = list->next;

	while(info != NULL){
		if(info->rcv_num[0] >= tr_get_tf_thre()){
			tr_info_list_del_nbr(list, info->addr);
		}
		info = info->next;
	}

	printk("-----------1hop_node_list status after arrangement-----------\n");
	test_info = list->next;
	while(test_info != NULL){
		struct tr_info *test_src_nbr_info = (&(test_info->nbr_list))->next;
		printk(KERN_INFO "1hop %x:%x:%x:%x:%x:%x, rssi %d, batt %x, (%d %d %d %d %d %d %d %d)/%d, %d, %d\n", test_info->addr[0], test_info->addr[1], test_info->addr[2], test_info->addr[3], test_info->addr[4], test_info->addr[5], test_info->rssi, test_info->batt, test_info->rcv_num[0], test_info->rcv_num[1], test_info->rcv_num[2], test_info->rcv_num[3], test_info->rcv_num[4], test_info->rcv_num[5], test_info->rcv_num[6], test_info->rcv_num[7],test_info->total_num, test_info->tf_cnt, test_info->nr_cnt);
		while(test_src_nbr_info != NULL){
			printk(KERN_INFO "---> %x:%x:%x:%x:%x:%x, rssi %d, batt %x, (%d %d %d %d %d %d %d %d)/%d, %d, %d\n", test_src_nbr_info->addr[0], test_src_nbr_info->addr[1], test_src_nbr_info->addr[2], test_src_nbr_info->addr[3], test_src_nbr_info->addr[4], test_src_nbr_info->addr[5], test_src_nbr_info->rssi, test_src_nbr_info->batt, test_src_nbr_info->rcv_num[0], test_src_nbr_info->rcv_num[1], test_src_nbr_info->rcv_num[2], test_src_nbr_info->rcv_num[3], test_src_nbr_info->rcv_num[4], test_src_nbr_info->rcv_num[5], test_src_nbr_info->rcv_num[6], test_src_nbr_info->rcv_num[7],test_src_nbr_info->total_num, test_src_nbr_info->tf_cnt, test_src_nbr_info->nr_cnt);
			test_src_nbr_info = test_src_nbr_info->next;
		}
		test_info = test_info->next;
	}
	printk("-----------------------------------------------------------------------\n");
}

unsigned char tr_info_list_pr_dof(struct tr_info_list *list, unsigned char addr[]){
	unsigned char nbr_dof = tr_get_data_k();
	struct tr_info *temp_info = tr_info_find_addr(list, addr);
	if(temp_info != NULL){
		unsigned char src_k = n_to_k(tr_get_data_n(), temp_info->total_num, temp_info->rcv_num[0]);
		nbr_dof = (nbr_dof > src_k ? nbr_dof - src_k : 0);
	}
	return nbr_dof;
}

unsigned char tr_info_max_clout(struct tr_info *info){
	unsigned char max_clout = 0;
	struct tr_info *nbr_info = (info->nbr_list).next;
	while(nbr_info != NULL){
		unsigned char nbr_clout = k_to_n(tr_info_list_pr_dof(info->head, nbr_info->addr), nbr_info->total_num, nbr_info->rcv_num[0]);
		if(max_clout < nbr_clout) max_clout = nbr_clout;
		nbr_info = nbr_info->next;
	}
	return max_clout;
}
void mnp_relay(struct tr_info_list *list){
	printk("MNP relay select function\n");
	/*
	   struct dst_info_list dst_list;
	   struct dst_info *test_dst_info;
	   struct tr_info *info;

	   dst_info_list_init(&dst_list);

	   printk("MNP relay select function start\n");

	// set up 2-hop nodes
	for(info = list->next; info != NULL; info = info->next){
	if(info->rcv_num < tr_get_tf_thre()){
	dst_info_insert(dst_info_create(info->addr, NULL, 0, 0), &dst_list); //modify dst_info_create
	}
	}
	for(info = list->next; info != NULL; info = info->next){
	if(info->rcv_num >= tr_get_tf_thre()){
	struct tr_info *nbr_info;
	for(nbr_info = (info->nbr_list).next; nbr_info != NULL; nbr_info = nbr_info->next){
	if((tr_info_find_addr(list, nbr_info->addr) == NULL) && (dst_info_find_addr(&dst_list, nbr_info->addr) == NULL)){
	dst_info_insert(dst_info_create(nbr_info->addr, NULL, 0, 0), &dst_list);
	}
	}
	}
	}

	printk("-----------dst_info_list status when initializing-----------\n");
	test_dst_info = dst_list.next;
	while(test_dst_info != NULL){
	printk(KERN_INFO "addr = %x:%x:%x:%x:%x:%x, raddr = %x:%x:%x:%x:%x:%x, min_clout = %d, pr_dof = %d\n", test_dst_info->addr[0], test_dst_info->addr[1], test_dst_info->addr[2], test_dst_info->addr[3], test_dst_info->addr[4], test_dst_info->addr[5], test_dst_info->raddr[0], test_dst_info->raddr[1], test_dst_info->raddr[2], test_dst_info->raddr[3], test_dst_info->raddr[4], test_dst_info->raddr[5], test_dst_info->min_clout, test_dst_info->pr_dof);
	test_dst_info = test_dst_info->next;
	}
	printk("-----------------------------------------------------------------------\n");

	while(dst_list.qlen != 0){
	struct tr_info *max_info = NULL;
	int max_nbr_num = 0;
	struct sk_buff *setrelay;
	struct tr_info *test_info;

	for(info = list->next; info != NULL; info = info->next){
	int nbr_num = (info->nbr_list).qlen;
	printk("info->addr = %x:%x:%x:%x:%x:%x, nbr_num = %d\n", info->addr[0], info->addr[1], info->addr[2], info->addr[3], info->addr[4], info->addr[5], nbr_num);
	if(max_nbr_num < nbr_num){
	max_nbr_num = nbr_num;
	max_info = info;
	}
	}
	if(max_info == NULL){
	printk("MNP error1\n");
	return;
	}

	printk("Max nbr_num addr = %x:%x:%x:%x:%x:%x, nbr_num = %d\n", max_info->addr[0], max_info->addr[1], max_info->addr[2], max_info->addr[3], max_info->addr[4], max_info->addr[5], max_nbr_num);
	setrelay = tl_alloc_skb(max_info->dev, max_info->addr, max_info->dev->dev_addr, SETRELAY_SIZE, SetRelay);
	if(setrelay != NULL){
	unsigned char relay_n = tr_info_max_clout(max_info);
	setrelay->data[ETHERHEADLEN + 1] = tr_get_data_k();
	setrelay->data[ETHERHEADLEN + 2] = (relay_n > 20 ? 20 : relay_n);
	dev_queue_xmit(setrelay);
	//dev_kfree_skb(setrelay);
	printk("Send SetRelay Message(%d); k = %d, n = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n", SetRelay, tr_get_data_k(), relay_n, max_info->dev->dev_addr[0], max_info->dev->dev_addr[1], max_info->dev->dev_addr[2], max_info->dev->dev_addr[3], max_info->dev->dev_addr[4], max_info->dev->dev_addr[5], max_info->addr[0], max_info->addr[1], max_info->addr[2], max_info->addr[3], max_info->addr[4], max_info->addr[5]);
	}

	for(info = (max_info->nbr_list).next; info != NULL; info = info->next){
	// Del nbr satisfied already in dst_info_list
	struct dst_info *temp_dst_info = dst_info_find_addr(&dst_list, info->addr);
	struct tr_info *temp_info = tr_info_find_addr(list, info->addr);
	if(temp_dst_info != NULL) dst_info_free(temp_dst_info);
	// Del satisfied nbr already in tr_info_list
	tr_info_list_del_nbr(list, info->addr);
	tr_info_free(temp_info);
}
tr_info_free(max_info);

printk("-----------dst_info_list status when relay select-----------\n");
test_dst_info = dst_list.next;
while(test_dst_info != NULL){
	printk(KERN_INFO "addr = %x:%x:%x:%x:%x:%x, raddr = %x:%x:%x:%x:%x:%x, min_clout = %d, pr_dof = %d\n", test_dst_info->addr[0], test_dst_info->addr[1], test_dst_info->addr[2], test_dst_info->addr[3], test_dst_info->addr[4], test_dst_info->addr[5], test_dst_info->raddr[0], test_dst_info->raddr[1], test_dst_info->raddr[2], test_dst_info->raddr[3], test_dst_info->raddr[4], test_dst_info->raddr[5], test_dst_info->min_clout, test_dst_info->pr_dof);
	test_dst_info = test_dst_info->next;
}
printk("-----------------------------------------------------------------------\n");

printk("-----------src_nbr_list status when relay select-----------\n");
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
printk("-----------------------------------------------------------------------\n");
}
printk("mnp relay select function end\n");
*/
}
/*
void dst_info_list_get_min_dof(struct tr_info_list *list, struct dst_info_list *dst_list){
	struct dst_info *temp_dst_info;

	if((list == NULL) || dst_list == NULL){
		printk("list or dst_list is NULL in get_min_dof\n");
		return;
	}

	temp_dst_info = dst_list->next;
	while(temp_dst_info != NULL){
		//printk(KERN_INFO "addr = %x:%x:%x:%x:%x:%x, min_clout = %d, pr_dof = %d\n", temp_dst_info->addr[0], temp_dst_info->addr[1], temp_dst_info->addr[2], temp_dst_info->addr[3], temp_dst_info->addr[4], temp_dst_info->addr[5], temp_dst_info->min_clout, temp_dst_info->pr_dof);
		if(temp_dst_info->pr_dof != 0){
			struct tr_info *temp_info = list->next;
			while(temp_info != NULL){
				struct tr_info *temp_nbr_info = (temp_info->nbr_list).next;
				while(temp_nbr_info != NULL){
					if(!memcmp(temp_dst_info->addr, temp_nbr_info->addr, ETH_ALEN)){
						unsigned char temp_clout = k_to_n(temp_dst_info->pr_dof, temp_nbr_info->total_num, temp_nbr_info->rcv_num[0]); //ycshin
						if(temp_clout <= temp_dst_info->min_clout3){
							if(temp_clout <= temp_dst_info->min_clout2){
								if(temp_clout <= temp_dst_info->min_clout1){
									memcpy(temp_dst_info->raddr3, temp_dst_info->raddr2, ETH_ALEN);
									temp_dst_info->min_clout3 = temp_dst_info->min_clout2;
									memcpy(temp_dst_info->raddr2, temp_dst_info->raddr1, ETH_ALEN);
									temp_dst_info->min_clout2 = temp_dst_info->min_clout1;
									memcpy(temp_dst_info->raddr1, temp_info->addr, ETH_ALEN);
									temp_dst_info->min_clout1 = temp_clout;
								}
								else{
									memcpy(temp_dst_info->raddr3, temp_dst_info->raddr2, ETH_ALEN);
									temp_dst_info->min_clout3 = temp_dst_info->min_clout2;
									memcpy(temp_dst_info->raddr2, temp_info->addr, ETH_ALEN);
									temp_dst_info->min_clout2 = temp_clout;
								}
							}
							else{
								memcpy(temp_dst_info->raddr3, temp_info->addr, ETH_ALEN);
								temp_dst_info->min_clout3 = temp_clout;
							}
						}
						break;
					}
					temp_nbr_info = temp_nbr_info->next;
				}
				temp_info = temp_info->next;
			}
		}
		temp_dst_info = temp_dst_info->next;
	}
}
*/
bool dst_info_list_all_over_threshold(struct dst_info_list *dst_list, unsigned char thres_n){
	struct dst_info *temp_dst_info;

	if(dst_list == NULL){
		printk("dst_list is NULL in all_over_threshold\n");
		return false;
	}

	temp_dst_info = dst_list->next;

	while(temp_dst_info != NULL){
		if(temp_dst_info->pr_dof != 0){
			return false;
		}
		temp_dst_info = temp_dst_info->next;
	}
	return true;
}

void dst_info_list_del_over_threshold(struct tr_info_list *list, struct dst_info_list *dst_list, unsigned char thres_n){
	/*
	struct dst_info *temp_dst_info;

	if((list == NULL) || dst_list == NULL){
		printk("list or dst_list is NULL in del_over_threshold\n");
		return;
	}

	if(dst_info_list_all_over_threshold(dst_list, thres_n)){
		printk("Every over threshold\n");
		return;
	}
	temp_dst_info = dst_list->next;

	while(temp_dst_info != NULL){
		struct dst_info *next_dst_info = temp_dst_info->next;
		if(temp_dst_info->pr_dof != 0){
			if(temp_dst_info->min_clout3 > thres_n){
				memset(temp_dst_info->raddr3, 0, ETH_ALEN);
				temp_dst_info->min_clout3 = 255;
				printk(KERN_INFO "IGNORE due to thresold(%d), addr = %x:%x:%x:%x:%x:%x, raddr3 = %x:%x:%x:%x:%x:%x, min_clout3 = %d\n", thres_n, temp_dst_info->addr[0], temp_dst_info->addr[1], temp_dst_info->addr[2], temp_dst_info->addr[3], temp_dst_info->addr[4], temp_dst_info->addr[5], temp_dst_info->raddr3[0], temp_dst_info->raddr3[1], temp_dst_info->raddr3[2], temp_dst_info->raddr3[3], temp_dst_info->raddr3[4], temp_dst_info->raddr3[5], temp_dst_info->min_clout3);
			}
			if(temp_dst_info->min_clout2 > thres_n){
				memset(temp_dst_info->raddr2, 0, ETH_ALEN);
				temp_dst_info->min_clout2 = 255;
				printk(KERN_INFO "IGNORE due to thresold(%d), addr = %x:%x:%x:%x:%x:%x, raddr2 = %x:%x:%x:%x:%x:%x, min_clout2 = %d\n", thres_n, temp_dst_info->addr[0], temp_dst_info->addr[1], temp_dst_info->addr[2], temp_dst_info->addr[3], temp_dst_info->addr[4], temp_dst_info->addr[5], temp_dst_info->raddr2[0], temp_dst_info->raddr2[1], temp_dst_info->raddr2[2], temp_dst_info->raddr2[3], temp_dst_info->raddr2[4], temp_dst_info->raddr2[5], temp_dst_info->min_clout2);
			}
			if(temp_dst_info->min_clout1 > thres_n){
				memset(temp_dst_info->raddr1, 0, ETH_ALEN);
				temp_dst_info->min_clout1 = 255;
				printk(KERN_INFO "IGNORE due to thresold(%d), addr = %x:%x:%x:%x:%x:%x, raddr1 = %x:%x:%x:%x:%x:%x, min_clout1 = %d, raddr2 = %x:%x:%x:%x:%x:%x, min_clout2 = %d, raddr3 = %x:%x:%x:%x:%x:%x, min_clout3 = %d, pr_dof = %d\n", thres_n, temp_dst_info->addr[0], temp_dst_info->addr[1], temp_dst_info->addr[2], temp_dst_info->addr[3], temp_dst_info->addr[4], temp_dst_info->addr[5], temp_dst_info->raddr1[0], temp_dst_info->raddr1[1], temp_dst_info->raddr1[2], temp_dst_info->raddr1[3], temp_dst_info->raddr1[4], temp_dst_info->raddr1[5], temp_dst_info->min_clout1, temp_dst_info->raddr2[0], temp_dst_info->raddr2[1], temp_dst_info->raddr2[2], temp_dst_info->raddr2[3], temp_dst_info->raddr2[4], temp_dst_info->raddr2[5], temp_dst_info->min_clout2, temp_dst_info->raddr3[0], temp_dst_info->raddr3[1], temp_dst_info->raddr3[2], temp_dst_info->raddr3[3], temp_dst_info->raddr3[4], temp_dst_info->raddr3[5], temp_dst_info->min_clout3, temp_dst_info->pr_dof);

				tr_info_list_del_nbr(list, temp_dst_info->addr);
				tr_info_free(tr_info_find_addr(list, temp_dst_info->addr));
				dst_info_free(temp_dst_info);
			}
		}
		temp_dst_info = next_dst_info;
	}
	*/
}

bool dst_info_list_all_pr_dof_zero(struct dst_info_list *dst_list){
	struct dst_info *temp_dst_info = dst_list->next;
	while(temp_dst_info != NULL){
		if(temp_dst_info->pr_dof != 0) return false;
		temp_dst_info = temp_dst_info->next;
	}
	return true;
}

void min_max_relay(struct tr_info_list *list){
	/*
	struct dst_info_list dst_list;
	struct relay_info *relay_info_array;
	unsigned int relay_cand_num = 0;
	unsigned int j;
	struct dst_info *test_dst_info;
	unsigned char remain_relay_n = tr_get_max_relay_n();

	struct tr_info *test_info;
	struct tr_info *info;

	dst_info_list_init(&dst_list);

	printk("MIN_MAX relay select function start\n");
	printk("Initial remain_relay_n = %d\n", remain_relay_n);

	// Distinguishing 1-hop node & 2-hop node, calculating the PR-DoF in 2-hop nodes
	for(info = list->next; info != NULL; info = info->next){
		if(info->rcv_num >= tr_get_tf_thre()){
			struct tr_info *nbr_info;
			for(nbr_info = (info->nbr_list).next; nbr_info != NULL; nbr_info = nbr_info->next){
				if((tr_info_find_addr(list, nbr_info->addr) == NULL) && (dst_info_find_addr(&dst_list, nbr_info->addr) == NULL)){
					unsigned char pr_dof = tr_get_data_k();
					dst_info_insert(dst_info_create(nbr_info->addr, pr_dof), &dst_list);
				}
			}
			relay_cand_num++;
		}
		else{
			unsigned char src_k = n_to_k(tr_get_data_n(), info->total_num, info->rcv_num);
			unsigned char pr_dof = (tr_get_data_k() > src_k ? tr_get_data_k() - src_k : 0);
			dst_info_insert(dst_info_create(info->addr, pr_dof), &dst_list);
		}
	}
	printk("-----------dst_info_list status after initializing-----------\n");
	test_dst_info = dst_list.next;
	while(test_dst_info != NULL){
		printk(KERN_INFO "2hop node : addr = %x:%x:%x:%x:%x:%x, raddr1 = %x:%x:%x:%x:%x:%x, min_clout1 = %d, raddr2 = %x:%x:%x:%x:%x:%x, min_clout2 = %d, raddr3 = %x:%x:%x:%x:%x:%x, min_clout3 = %d, pr_dof = %d\n", test_dst_info->addr[0], test_dst_info->addr[1], test_dst_info->addr[2], test_dst_info->addr[3], test_dst_info->addr[4], test_dst_info->addr[5], test_dst_info->raddr1[0], test_dst_info->raddr1[1], test_dst_info->raddr1[2], test_dst_info->raddr1[3], test_dst_info->raddr1[4], test_dst_info->raddr1[5], test_dst_info->min_clout1, test_dst_info->raddr2[0], test_dst_info->raddr2[1], test_dst_info->raddr2[2], test_dst_info->raddr2[3], test_dst_info->raddr2[4], test_dst_info->raddr2[5], test_dst_info->min_clout2, test_dst_info->raddr3[0], test_dst_info->raddr3[1], test_dst_info->raddr3[2], test_dst_info->raddr3[3], test_dst_info->raddr3[4], test_dst_info->raddr3[5], test_dst_info->min_clout3, test_dst_info->pr_dof);
		test_dst_info = test_dst_info->next;
	}
	printk("------------------------------------------------------------\n");

	// set up array of 1-hop nodes
	printk("relay_cand_num = %d\n", relay_cand_num);

	if(relay_cand_num == 0){
		printk("No Relay Candidate\n");
		return;
	}

	relay_info_array = kzalloc((sizeof(struct relay_info))*(relay_cand_num), GFP_ATOMIC);

	for(info = list->next, j = 0; info != NULL; info = info->next){
		if(info->rcv_num >= tr_get_tf_thre()){
			if(j > relay_cand_num){
				printk("MIN_MAX ERROR 1\n");
				return;
			}
			memcpy((relay_info_array[j]).addr, info->addr, ETH_ALEN);
			j++;
		}
	}

	printk("--------------------initial relay_info_array status--------------------\n");
	for(j = 0; j < relay_cand_num; j++){
		printk(KERN_INFO "Relay candidate%d : addr = %x:%x:%x:%x:%x:%x, clout_sum = %d, max_clout = %d\n", j, (relay_info_array[j]).addr[0], (relay_info_array[j]).addr[1], (relay_info_array[j]).addr[2], (relay_info_array[j]).addr[3], (relay_info_array[j]).addr[4], (relay_info_array[j]).addr[5], (relay_info_array[j]).clout_sum, (relay_info_array[j]).max_clout);
	}
	printk("-----------------------------------------------------------------------\n");

	while(remain_relay_n != 0){
		unsigned int max_clout_sum = 0;
		unsigned int max_i = 0;
		struct dst_info *temp_dst_info;
		struct tr_info *max_info;
		unsigned char relay_n;
		struct sk_buff *setrelay;
		unsigned int i;

		if(dst_info_list_all_pr_dof_zero(&dst_list)){
			char extra_relay_num = 1;
			const unsigned char plus_n = remain_relay_n/extra_relay_num;
			char plus_i;
			for(plus_i = 0; plus_i < extra_relay_num; plus_i++){
				struct tr_info *temp_plus_info;
				struct tr_info *plus_info = NULL;
				unsigned char max_2nd_clout_sum = 0;
				for(i = 0; i < relay_cand_num; i++){
					if(max_2nd_clout_sum <= (relay_info_array[i]).clout_sum && (temp_plus_info = tr_info_find_addr(list, (relay_info_array[i]).addr)) != NULL){
						plus_info = temp_plus_info;
						max_2nd_clout_sum = (relay_info_array[i]).clout_sum;
					}
				}
				
				for(temp_plus_info = list->next; temp_plus_info != NULL; temp_plus_info = temp_plus_info->next){
					unsigned int max_rcv_num = 0;
					if(max_rcv_num < temp_plus_info->rcv_num){
						plus_info = temp_plus_info;
						max_rcv_num = temp_plus_info->rcv_num;
					}
				}
				*/
/*
				if(plus_info == NULL){
					printk("Every node is relay\n");
					break;
				}

				printk("\n\nRelay Selection Result ++++++\n");
				printk("Relay = %x:%x:%x:%x:%x:%x, plus_n = %d\n", plus_info->addr[0], plus_info->addr[1], plus_info->addr[2], plus_info->addr[3], plus_info->addr[4], plus_info->addr[5], plus_n);

				setrelay = tl_alloc_skb(plus_info->dev, plus_info->addr, plus_info->dev->dev_addr, SETRELAY_SIZE, SetRelay);
				if(setrelay != NULL){
					setrelay->data[ETHERHEADLEN + 1] = tr_get_data_k();
					setrelay->data[ETHERHEADLEN + 2] = plus_n;
					dev_queue_xmit(setrelay);
					setrelay = NULL;
					//dev_kfree_skb(setrelay);
					printk("Send SetRelay Message(%d); k = %d, n = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n\n\n", SetRelay, tr_get_data_k(), plus_n, plus_info->dev->dev_addr[0], plus_info->dev->dev_addr[1], plus_info->dev->dev_addr[2], plus_info->dev->dev_addr[3], plus_info->dev->dev_addr[4], plus_info->dev->dev_addr[5], plus_info->addr[0], plus_info->addr[1], plus_info->addr[2], plus_info->addr[3], plus_info->addr[4], plus_info->addr[5]);
				}
				tr_info_free(plus_info);
			}
			break;
		}
		dst_info_list_get_min_dof(list, &dst_list);

		printk("-----------dst_info_list status after get_min_dof-----------\n");
		test_dst_info = dst_list.next;
		while(test_dst_info != NULL){
			printk(KERN_INFO "2hop node : addr = %x:%x:%x:%x:%x:%x, raddr1 = %x:%x:%x:%x:%x:%x, min_clout1 = %d, raddr2 = %x:%x:%x:%x:%x:%x, min_clout2 = %d, raddr3 = %x:%x:%x:%x:%x:%x, min_clout3 = %d, pr_dof = %d\n", test_dst_info->addr[0], test_dst_info->addr[1], test_dst_info->addr[2], test_dst_info->addr[3], test_dst_info->addr[4], test_dst_info->addr[5], test_dst_info->raddr1[0], test_dst_info->raddr1[1], test_dst_info->raddr1[2], test_dst_info->raddr1[3], test_dst_info->raddr1[4], test_dst_info->raddr1[5], test_dst_info->min_clout1, test_dst_info->raddr2[0], test_dst_info->raddr2[1], test_dst_info->raddr2[2], test_dst_info->raddr2[3], test_dst_info->raddr2[4], test_dst_info->raddr2[5], test_dst_info->min_clout2, test_dst_info->raddr3[0], test_dst_info->raddr3[1], test_dst_info->raddr3[2], test_dst_info->raddr3[3], test_dst_info->raddr3[4], test_dst_info->raddr3[5], test_dst_info->min_clout3, test_dst_info->pr_dof);
			test_dst_info = test_dst_info->next;
		}
		printk("------------------------------------------------------------\n");

		dst_info_list_del_over_threshold(list, &dst_list, remain_relay_n);

		printk("-----------dst_info_list status after del_over_threshold-----------\n");
		test_dst_info = dst_list.next;
		while(test_dst_info != NULL){
			printk(KERN_INFO "2hop node : addr = %x:%x:%x:%x:%x:%x, raddr1 = %x:%x:%x:%x:%x:%x, min_clout1 = %d, raddr2 = %x:%x:%x:%x:%x:%x, min_clout2 = %d, raddr3 = %x:%x:%x:%x:%x:%x, min_clout3 = %d, pr_dof = %d\n", test_dst_info->addr[0], test_dst_info->addr[1], test_dst_info->addr[2], test_dst_info->addr[3], test_dst_info->addr[4], test_dst_info->addr[5], test_dst_info->raddr1[0], test_dst_info->raddr1[1], test_dst_info->raddr1[2], test_dst_info->raddr1[3], test_dst_info->raddr1[4], test_dst_info->raddr1[5], test_dst_info->min_clout1, test_dst_info->raddr2[0], test_dst_info->raddr2[1], test_dst_info->raddr2[2], test_dst_info->raddr2[3], test_dst_info->raddr2[4], test_dst_info->raddr2[5], test_dst_info->min_clout2, test_dst_info->raddr3[0], test_dst_info->raddr3[1], test_dst_info->raddr3[2], test_dst_info->raddr3[3], test_dst_info->raddr3[4], test_dst_info->raddr3[5], test_dst_info->min_clout3, test_dst_info->pr_dof);
			test_dst_info = test_dst_info->next;
		}
		printk("-------------------------------------------------------------------\n");

		for(i = 0; i < relay_cand_num; i++){
			(relay_info_array[i]).max_clout = 0;
			for(temp_dst_info = dst_list.next; temp_dst_info != NULL; temp_dst_info = temp_dst_info->next){
				if(temp_dst_info->pr_dof != 0){
					if(!memcmp(temp_dst_info->raddr1, (relay_info_array[i]).addr, ETH_ALEN)){
						if((relay_info_array[i]).max_clout < temp_dst_info->min_clout1) (relay_info_array[i]).max_clout = temp_dst_info->min_clout1;
					}
					else if(!memcmp(temp_dst_info->raddr2, (relay_info_array[i]).addr, ETH_ALEN)){
						if((relay_info_array[i]).max_clout < temp_dst_info->min_clout2) (relay_info_array[i]).max_clout = temp_dst_info->min_clout2;
					}
					else if(!memcmp(temp_dst_info->raddr3, (relay_info_array[i]).addr, ETH_ALEN)){
						if((relay_info_array[i]).max_clout < temp_dst_info->min_clout3) (relay_info_array[i]).max_clout = temp_dst_info->min_clout3;
					}
					else{
						//
					}
				}
			}
		}
		printk("-----------relay_info_array status after caculating max_clout-----------\n");
		for(i = 0; i < relay_cand_num; i++){
			printk(KERN_INFO "addr = %x:%x:%x:%x:%x:%x, clout_sum = %d, max_clout = %d\n", (relay_info_array[i]).addr[0], (relay_info_array[i]).addr[1], (relay_info_array[i]).addr[2], (relay_info_array[i]).addr[3], (relay_info_array[i]).addr[4], (relay_info_array[i]).addr[5], (relay_info_array[i]).clout_sum, (relay_info_array[i]).max_clout);
		}
		printk("------------------------------------------------------------------------\n");

		for(i = 0; i < relay_cand_num; i++){
			struct tr_info *temp_info = tr_info_find_addr(list, (relay_info_array[i]).addr);
			(relay_info_array[i]).clout_sum = 0;
			if(temp_info == NULL) continue;
			for(temp_dst_info = dst_list.next; temp_dst_info != NULL; temp_dst_info = temp_dst_info->next){
				if(temp_dst_info->pr_dof != 0){
					struct tr_info *temp_nbr_info = tr_info_find_addr(&(temp_info->nbr_list), temp_dst_info->addr);
					if(temp_nbr_info != NULL){
						unsigned int temp_clout = (unsigned int) n_to_k((relay_info_array[i]).max_clout, temp_nbr_info->total_num, temp_nbr_info->rcv_num);
						unsigned int temp_pr_dof = temp_dst_info->pr_dof;
						(relay_info_array[i]).clout_sum += (temp_clout < temp_pr_dof ? temp_clout : temp_pr_dof);
					}
				}
			}
			if(max_clout_sum < (relay_info_array[i]).clout_sum){
				max_clout_sum = (relay_info_array[i]).clout_sum;
				max_i = i;
			}
		}

		printk("-----------relay_info_array status after calculating clout_sum-----------\n");
		for(i = 0; i < relay_cand_num; i++){
			printk(KERN_INFO "addr = %x:%x:%x:%x:%x:%x, clout_sum = %d, max_clout = %d\n", (relay_info_array[i]).addr[0], (relay_info_array[i]).addr[1], (relay_info_array[i]).addr[2], (relay_info_array[i]).addr[3], (relay_info_array[i]).addr[4], (relay_info_array[i]).addr[5], (relay_info_array[i]).clout_sum, (relay_info_array[i]).max_clout);
		}
		printk("-------------------------------------------------------------------------\n");

		max_info = tr_info_find_addr(list, (relay_info_array[max_i]).addr);

		if(max_info == NULL){
			printk("max_info is NULL\n");
			return;
		}

		relay_n = (relay_info_array[max_i]).max_clout + (relay_info_array[max_i]).max_clout/10;
		if(remain_relay_n > relay_n){
			remain_relay_n -= relay_n;
		}
		else{
			relay_n = remain_relay_n;
			remain_relay_n = 0;
		}
		printk("After relay selection : remain_relay_n = %d\n", remain_relay_n);

		printk("\n\nRelay Selection Result\n");
		printk("Relay = %x:%x:%x:%x:%x:%x, i = %d, max_clout_sum = %d, relay_n = %d\n", max_info->addr[0], max_info->addr[1], max_info->addr[2], max_info->addr[3], max_info->addr[4], max_info->addr[5], max_i, max_clout_sum, relay_n);

		setrelay = tl_alloc_skb(max_info->dev, max_info->addr, max_info->dev->dev_addr, SETRELAY_SIZE, SetRelay);
		if(setrelay != NULL){
			setrelay->data[ETHERHEADLEN + 1] = tr_get_data_k();
			setrelay->data[ETHERHEADLEN + 2] = relay_n;
			dev_queue_xmit(setrelay);
			//dev_kfree_skb(setrelay);
			printk("Send SetRelay Message(%d); k = %d, n = %d, SA = %x:%x:%x:%x:%x:%x, DA = %x:%x:%x:%x:%x:%x\n\n\n", SetRelay, tr_get_data_k(), relay_n, max_info->dev->dev_addr[0], max_info->dev->dev_addr[1], max_info->dev->dev_addr[2], max_info->dev->dev_addr[3], max_info->dev->dev_addr[4], max_info->dev->dev_addr[5], max_info->addr[0], max_info->addr[1], max_info->addr[2], max_info->addr[3], max_info->addr[4], max_info->addr[5]);
		}

		for(temp_dst_info = dst_list.next; temp_dst_info != NULL; temp_dst_info = temp_dst_info->next){
			if(temp_dst_info->pr_dof != 0){
				if(!memcmp(temp_dst_info->raddr1, max_info->addr, ETH_ALEN) || !memcmp(temp_dst_info->raddr2, max_info->addr, ETH_ALEN) || !memcmp(temp_dst_info->raddr3, max_info->addr, ETH_ALEN)){
					temp_dst_info->pr_dof = 0;
					//temp_dst_info->min_clout = 0;
					tr_info_list_del_nbr(list, temp_dst_info->addr);
					tr_info_free(tr_info_find_addr(list, temp_dst_info->addr));
				}
				else{
					struct tr_info *nbr_info = tr_info_find_addr(&(max_info->nbr_list), temp_dst_info->addr);
					if(nbr_info != NULL){
						unsigned char temp_pr_dof = n_to_k(relay_n, nbr_info->total_num, nbr_info->rcv_num);
						temp_dst_info->pr_dof = (temp_pr_dof > temp_dst_info->pr_dof ? 0 : temp_dst_info->pr_dof - temp_pr_dof);
					}
				}
			}
		}
		tr_info_free(max_info);

		printk("-----------src_nbr_list status after relay select-----------\n");
		test_info = list->next;
		while(test_info != NULL){
			struct tr_info *test_src_nbr_list_info = (&(test_info->nbr_list))->next;
			printk(KERN_INFO "1hop %x:%x:%x:%x:%x:%x, %d/%d, %d, %d\n", test_info->addr[0], test_info->addr[1], test_info->addr[2], test_info->addr[3], test_info->addr[4], test_info->addr[5], test_info->total_num, test_info->rcv_num, test_info->tf_cnt, test_info->nr_cnt);
			while(test_src_nbr_list_info != NULL){
				printk(KERN_INFO "-> %x:%x:%x:%x:%x:%x, %d/%d, %d, %d\n", test_src_nbr_list_info->addr[0], test_src_nbr_list_info->addr[1], test_src_nbr_list_info->addr[2], test_src_nbr_list_info->addr[3], test_src_nbr_list_info->addr[4], test_src_nbr_list_info->addr[5], test_src_nbr_list_info->total_num, test_src_nbr_list_info->rcv_num, test_src_nbr_list_info->tf_cnt, test_src_nbr_list_info->nr_cnt);
				test_src_nbr_list_info = test_src_nbr_list_info->next;
			}
			test_info = test_info->next;
		}
		printk("------------------------------------------------------------\n");

		printk("-----------dst_info_list status after relay select-----------\n");
		test_dst_info = dst_list.next;
		while(test_dst_info != NULL){
			printk(KERN_INFO "2hop node : addr = %x:%x:%x:%x:%x:%x, raddr1 = %x:%x:%x:%x:%x:%x, min_clout1 = %d, raddr2 = %x:%x:%x:%x:%x:%x, min_clout2 = %d, raddr3 = %x:%x:%x:%x:%x:%x, min_clout3 = %d, pr_dof = %d\n", test_dst_info->addr[0], test_dst_info->addr[1], test_dst_info->addr[2], test_dst_info->addr[3], test_dst_info->addr[4], test_dst_info->addr[5], test_dst_info->raddr1[0], test_dst_info->raddr1[1], test_dst_info->raddr1[2], test_dst_info->raddr1[3], test_dst_info->raddr1[4], test_dst_info->raddr1[5], test_dst_info->min_clout1, test_dst_info->raddr2[0], test_dst_info->raddr2[1], test_dst_info->raddr2[2], test_dst_info->raddr2[3], test_dst_info->raddr2[4], test_dst_info->raddr2[5], test_dst_info->min_clout2, test_dst_info->raddr3[0], test_dst_info->raddr3[1], test_dst_info->raddr3[2], test_dst_info->raddr3[3], test_dst_info->raddr3[4], test_dst_info->raddr3[5], test_dst_info->min_clout3, test_dst_info->pr_dof);
			test_dst_info = test_dst_info->next;
		}
		printk("-------------------------------------------------------------\n");
		
		printk("-----------relay_info_array status after relay select-----------\n");
		for(i = 0; i < relay_cand_num; i++){
			printk(KERN_INFO "addr = %x:%x:%x:%x:%x:%x, clout_sum = %d, max_clout = %d\n", (relay_info_array[i]).addr[0], (relay_info_array[i]).addr[1], (relay_info_array[i]).addr[2], (relay_info_array[i]).addr[3], (relay_info_array[i]).addr[4], (relay_info_array[i]).addr[5], (relay_info_array[i]).clout_sum, (relay_info_array[i]).max_clout);
		}
		printk("-------------------------------------------------------------------------\n");
	}
	printk("min_max relay select function end\n");
	dst_info_list_purge(&dst_list);
	kfree(relay_info_array);
	
	*/
}

void evcast_relay(struct tr_info_list *list, struct relay_info_list *relay_list, struct dst_info_list *dst_list, unsigned char src_batt){
	//struct dst_info_list dst_list;
	//struct relay_info_list relay_list;
	struct relay_info_list prev_relay_list;
	
	unsigned int bitrate_mbps = 1;
	unsigned int max_time_us = 0;	
	unsigned int used_time_us = 0;
	unsigned int round = 0;
	unsigned int pkt_len = 1400;
	unsigned char omega = 1;
	unsigned char max_round = 20;
	unsigned int k = 10;
	
	struct selected_relay_info relay;

	//struct tr_info *test_info;
	struct tr_info *info;
	//struct dst_info *d_info;

	dst_info_list_init(dst_list);
	relay_info_list_init(relay_list);
	relay_info_list_init(&prev_relay_list);
	
	max_time_us = 8*pkt_len*8*k / 10*bitrate_mbps;  // 8/10 of the allowed time
	//max_time_us = 30000; // delete it !!		
		
	
	printk("Start EV-CAST Algorithm (Max time : %d us)\n", max_time_us);
	
	for(info = list->next; info != NULL; info = info->next){
		struct tr_info *nbr_info;
		struct tr_info_list * nbr_info_list = &(info->nbr_list);
		
		struct tr_info * info2;
			
		if(dst_info_find_addr(dst_list, info->addr)==NULL)
		{
			unsigned char pr_dof = tr_get_data_k();
			dst_info_insert(dst_info_create(info->addr, pr_dof, 0), dst_list);
		}	
		for(nbr_info = nbr_info_list->next; nbr_info != NULL; nbr_info = nbr_info->next){
			if(dst_info_find_addr(dst_list, nbr_info->addr) == NULL){
				unsigned char pr_dof = tr_get_data_k();
				dst_info_insert(dst_info_create(nbr_info->addr, pr_dof, 0), dst_list);
			}
		}
		if ((info->nbr_list).next == NULL)
			continue;
		
		for (info2 = list->next; info2!=NULL; info2 = info2->next){
			bool is_srp = true;
			if (info == info2)
				continue;
			if (tr_info_find_addr(nbr_info_list,info2->addr)!=NULL)
			{
				is_srp = false;
				continue;
			}
 			if (tr_info_find_addr(&(info2->nbr_list), info->addr)!=NULL)
			{
				is_srp = false;	
				continue;
			}
			if ((info2->nbr_list).next == NULL){
				is_srp = false;
				continue;
			}

			for(nbr_info = nbr_info_list->next; nbr_info != NULL; nbr_info = nbr_info->next){
				if (tr_info_find_addr(&(info2->nbr_list), nbr_info->addr)!=NULL){ 
					is_srp = false;
					break;		
				}
			}
			for(nbr_info = (info2->nbr_list).next; nbr_info != NULL; nbr_info = nbr_info->next){
				if (tr_info_find_addr(&(info->nbr_list), nbr_info->addr)!=NULL){ 
					is_srp = false;
					break;		
				}
			}

			if (is_srp == true){
				struct tr_info * srp;
				struct tr_info * nbr;
				printk("%x:%x:%x:%x:%x:%x AND %x:%x:%x:%x:%x:%x are SRP\n", info->addr[0], info->addr[1], info->addr[2], info->addr[3], info->addr[4], info->addr[5], info2->addr[0], info2->addr[1], info2->addr[2], info2->addr[3], info2->addr[4], info2->addr[5]);
				srp = tr_info_create(info2->addr, info2->dev, info2->total_num, info2->rcv_num, info2->rssi, info2->batt);
				
				for (nbr = (info2->nbr_list).next; nbr != NULL; nbr = nbr->next){
					tr_info_insert(tr_info_create(nbr->addr, nbr->dev, nbr->total_num, nbr->rcv_num, nbr->rssi, nbr->batt), &(srp->nbr_list)); 	
				}

				tr_info_insert(srp, &(info->srp_list));
			}
		}
	}
/*	
	for(info = list->next; info != NULL; info = info->next){
		struct tr_info * hop2;
		struct tr_info * srp;
		for (srp = (info->srp_list).next; srp != NULL; srp = srp->next){
			for(hop2 = (srp->nbr_list).next; hop2 != NULL; hop2 = hop2->next){
				printk("hop2 addr: %x:%x:%x:%x:%x:%x\n", hop2->addr[0], hop2->addr[1], hop2->addr[2], hop2->addr[3], hop2->addr[4], hop2->addr[5]);
			}
		}
	}
*/
	#if 1
	printk("Initialization of Destination List Complete, Num of Dest: %d\n", dst_list->qlen);
	dst_info_list_print(dst_list);
	while(!dst_all_zero(dst_list) && (used_time_us < max_time_us) && (round < max_round) ){
		//struct dst_info* dst;
		struct tr_info * hop1;
		struct relay_info * copy_info;
	
		round++;

		relay.utility = 0;
		relay.round= round;	
		relay.rate1 = 0;
		relay.clout1= 0;
		memset(relay.addr2, 0, ETH_ALEN);
		relay.rate2 = 0;
		relay.clout2 = 0;
		memset(relay.addr3, 0, ETH_ALEN);
		relay.rate3 = 0;
		relay.clout3 = 0;
		
		relay_info_list_purge(&prev_relay_list);
		relay_info_list_init(&prev_relay_list);
		
		for (copy_info = relay_list->next; copy_info != NULL; copy_info=copy_info->next){
			relay_info_insert(relay_info_create(copy_info->round, copy_info->type, copy_info->addr, copy_info->clout, copy_info->rate), &prev_relay_list);
		} 	

		printk("===== Round %d =====\n", round);
	
#if 1	
		// 1-hop only
		for(hop1 = list->next; hop1 != NULL; hop1 = hop1->next){
			unsigned char pr_dof = get_pr_dof(dst_list, hop1->addr);
			struct tr_info * hop2;
			struct tr_info * srp;
			unsigned char user = 0;
			unsigned int utility = 0;
			unsigned int total_cost = 0;
			unsigned char rate1 = 0;
			unsigned char clout1 = 0;
			unsigned int time1 = 0;
			unsigned int cost1 = 0;	

			if (pr_dof > 0){
					struct tr_info * other1;
					struct relay_info * r_info;
					unsigned char remain_c = 0;	
					unsigned int time_cost = 0;
					unsigned char charge1 = 0;
					unsigned char capacity1 = 0;
					//unsigned char charge = 0;
					//unsigned char capacity = 0;
					
					printk("Target 1hop addr %x:%x:%x:%x:%x:%x pr_dof: %d\n", hop1->addr[0], hop1->addr[1], hop1->addr[2], hop1->addr[3], hop1->addr[4], hop1->addr[5], pr_dof);			
					rate1 = find_best_mcs(hop1, pr_dof); 			
					if (rate1 >= NUM_MCS){
							printk("Invalid rate\n");
							break;
					}
						
					clout1 = k_to_n(pr_dof, hop1->total_num, hop1->rcv_num[rate1]);
					remain_c = clout1;
					user++;
				
					for(other1 = list->next; other1 != NULL; other1 = other1->next){
						if (hop1 != other1){
							unsigned char pr_dof2 = get_pr_dof(dst_list, other1->addr);
							if (pr_dof2 == 0)
								continue;
							if (n_to_k(clout1, other1->total_num, other1->rcv_num[rate1]) >= pr_dof2){
								user++;
							}
						}	
					}
					for (r_info = relay_list->next; r_info != NULL; r_info = r_info->next){
						if (r_info->type == 0 && r_info->clout > 0 && rate1 < r_info->rate){
							if (r_info->clout >= remain_c){
								time_cost += cal_tx_time(rate1, remain_c, pkt_len)-cal_tx_time(r_info->rate, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time_cost += cal_tx_time(rate1, r_info->clout, pkt_len)-cal_tx_time(r_info->rate, r_info->clout, pkt_len);
								remain_c = remain_c - r_info->clout;
							}
						}
					}

					time_cost+=cal_tx_time(rate1, remain_c, pkt_len);
					time1= time_cost;
					charge1 = get_charge(src_batt);
					capacity1 = get_capa(src_batt);			
					
					if (charge1 == 1){
						total_cost = time_cost*1000/(omega*capacity1); 
					}
					else{
						total_cost = time_cost*1000/(capacity1); 
					}
				
					cost1 = total_cost;
	
					if(total_cost == 0){
						printk("cost is 0\n");
						continue;
					}
					
					utility = user*10000000/total_cost;
					
					printk("Type 1 Clout1: %d Rate1: %d -->  Utility: %d (User: %d Cost: %d Time: %d Charge: %d Capacity: %d)\n", clout1, rate1, utility, user, cost1, time_cost, charge1, capacity1);
					
					//printk(KERN_INFO "Type 1 Utility: %d (Source  Clout: %d Rate: %d) (Relay1 Addr %x:%x:%x:%x:%x:%x Clout: %d Rate: %d) (SRP Addr %x:%x:%x:%x:%x:%x Clout: %d Rate: %d)\n", test_info->round, test_info->clout1, test_info->rate1, test_info->addr2[0], test_info->addr2[1], test_info->addr2[2], test_info->addr2[3], test_info->addr2[4], test_info->addr2[5], test_info->clout2, test_info->rate2, test_info->addr3[0], test_info->addr3[1], test_info->addr3[2], test_info->addr3[3], test_info->addr3[4], test_info->addr3[5], test_info->clout3, test_info->rate3);
					
				
					if (utility >= relay.utility){
						relay.utility = utility;
						relay.round = round;
						relay.rate1 = rate1;
						relay.clout1 = clout1;
						memset(relay.addr2, 0, ETH_ALEN);
						relay.rate2 = 0;
						relay.clout2 = 0;
						memset(relay.addr3, 0, ETH_ALEN);
						relay.rate3 = 0;
						relay.clout3 = 0; 
						
						//printk("Relay sample update\n");
						//selected_relay_info_print(&relay); 
					}
			} // if the 1hop node is unserved
#if 1	
			for(hop2 = (hop1->nbr_list).next; hop2 != NULL; hop2 = hop2->next){
				unsigned char pr_dof2 = 0;
				unsigned char rate2 = 0; 				
				unsigned char clout2 = 0;
				unsigned char remain_c = 0;	
				unsigned int time_cost2 = 0;
				unsigned char charge = 0;
				unsigned char capacity = 0;
				unsigned char user2 = 0;

				struct relay_info * r_info;
				struct tr_info * other2;

				unsigned char dof_by_src = 0;
				struct tr_info * info_from_src;
				
				info_from_src =	tr_info_find_addr(list, hop2->addr);
				if (info_from_src != NULL)
					dof_by_src = n_to_k(clout1, info_from_src->total_num, info_from_src->rcv_num[rate1]);
				else
					dof_by_src = 0;	

				pr_dof2 = get_pr_dof(dst_list, hop2->addr) > dof_by_src ? get_pr_dof(dst_list, hop2->addr) - dof_by_src : 0;
				printk("Target 2hop addr %x:%x:%x:%x:%x:%x pr_dof: %d by_src: %d\n", hop2->addr[0], hop2->addr[1], hop2->addr[2], hop2->addr[3], hop2->addr[4], hop2->addr[5], pr_dof2, dof_by_src);			
	
				if (pr_dof2 == 0)
					continue;			
				
				user2 = 1;
				rate2 = find_best_mcs(hop2, pr_dof2);
				clout2 = k_to_n(pr_dof2, hop2->total_num, hop2->rcv_num[rate2]);
				remain_c = clout2;	
				
				for(other2 = (hop1->nbr_list).next; other2 != NULL; other2 = other2->next){
					if (hop2 != other2){
						unsigned char pr_dof_other2 = get_pr_dof(dst_list, other2->addr);
						info_from_src =	tr_info_find_addr(list, other2->addr);
						if (info_from_src != NULL)
							dof_by_src = n_to_k(clout1, info_from_src->total_num, info_from_src->rcv_num[rate1]);	
						else
							dof_by_src = 0;

						pr_dof_other2 = pr_dof_other2 > dof_by_src ? pr_dof_other2 - dof_by_src : 0;							
						if (pr_dof_other2 == 0)
							continue;

						if (n_to_k(clout2, other2->total_num, other2->rcv_num[rate2]) >= pr_dof_other2){
							user2++;	
						}
					}	
				}
				
				for (r_info = relay_list->next; r_info != NULL; r_info = r_info->next){
					if (!memcmp(r_info->addr, hop1->addr, ETH_ALEN) && r_info->clout > 0 && rate2 < r_info->rate){
						if (r_info->clout >= remain_c){
							time_cost2 += cal_tx_time(rate2, remain_c, pkt_len)-cal_tx_time(r_info->rate, remain_c, pkt_len);
							remain_c = 0;
						}
						else{
							time_cost2 += cal_tx_time(rate2, r_info->clout, pkt_len)-cal_tx_time(r_info->rate, r_info->clout, pkt_len);
							remain_c = remain_c - r_info->clout;
						}
					}
				}
		
				time_cost2 += cal_tx_time(rate2, remain_c, pkt_len);
				charge = get_charge(hop2->batt);
				capacity = get_capa(hop2->batt);			
				
				if (capacity == 0){
					printk("Zero capacity\n");
					continue;
				}
	
				if (charge == 1){
						total_cost = cost1 +  time_cost2*1000 / (omega*capacity);  
				}
				else{
					total_cost = cost1 + time_cost2*1000 / capacity;	
				}

				if(total_cost == 0){
					printk("Zero cost\n");
					continue;
				}
								
				utility = (user+user2)*10000000 / total_cost;					
				
				if (user == 0)
				{
					printk("Type 2 Clout2: %d Rate2: %d -->  Utility: %d (User: %d Cost: %d Time: %d Charge: %d Capacity: %d)\n", clout2, rate2, utility, user2, total_cost,  time_cost2, charge, capacity);
				}
				else 
					printk("Type 3 Clout1: %d Rate1: %d Clout2: %d Rate2: %d -->  Utility: %d (User1: %d User2: %d Cost: %d Time1: %d Time2: %d)\n", clout1, rate1, clout2, rate2, utility, user, user2, total_cost, time1, time_cost2);

				if (utility >= relay.utility){
					relay.utility = utility;
					relay.round = round;
					relay.rate1 = rate1;
					relay.clout1 = clout1;
					memcpy(relay.addr2, hop1->addr, ETH_ALEN);  
					relay.rate2 = rate2;
					relay.clout2 = clout2;
					memset(relay.addr3, 0, ETH_ALEN);  
					relay.rate3 = 0;
					relay.clout3 = 0;
						
					//printk("Relay sample update\n");
					//selected_relay_info_print(&relay); 
				}	
			}//for 2hop nodes of a chosen 1hop node
			
			total_cost = 0;
	#endif
#if 1		
			for (srp = (hop1->srp_list).next; srp != NULL; srp = srp->next){
				unsigned char pr_dof_srp = get_pr_dof(dst_list, srp->addr);
				unsigned char rate1 = 0;
				unsigned char rate2 = 0;
				unsigned char rate3 = 0;
				unsigned char clout1 = 0;
				unsigned char clout2 = 0;
				unsigned char clout3 = 0;
				unsigned char user1 = 0;
				unsigned char user2 = 0;
				unsigned char user3 = 0;
				unsigned int time_cost = 0;
				unsigned int time2 = 0;
				unsigned int time3 = 0;
				unsigned int cost1 = 0;
				unsigned int cost2 = 0;
				struct relay_info * r_info;
				
				printk("SRP Addr %x:%x:%x:%x:%x:%X\n", srp->addr[0], srp->addr[1], srp->addr[2], srp->addr[3], srp->addr[4], srp->addr[5]);
 
				if (pr_dof_srp > 0 || pr_dof > 0){
					unsigned char rate11 = 0;
					unsigned char rate12 = 0;
					unsigned char clout11 = 0;
					unsigned char clout12 = 0;
					unsigned char remain_c = 0;
					unsigned char charge = 0;
					unsigned char capacity = 0;
					struct tr_info * other1;
				
					time_cost = 0;	
					
					rate11 = find_best_mcs(hop1, pr_dof); 				
					rate12 = find_best_mcs(srp, pr_dof_srp);
					
					if (pr_dof == 0)
						rate1 = rate12;
					else if (pr_dof_srp == 0)
						rate1 = rate11;
					else	
						rate1 = rate11 <= rate12 ? rate11 : rate12;
					  				
					clout11 = k_to_n(pr_dof, hop1->total_num, hop1->rcv_num[rate1]);
					clout12 = k_to_n(pr_dof_srp, srp->total_num, srp->rcv_num[rate1]);
					clout1 = clout11 >= clout12 ? clout11 : clout12;

					printk("c1: %d r1: %d c2: %d r2: %d --> c: %d r: %d\n", clout11, rate11, clout12, rate12, clout1, rate1);
					
					remain_c = clout1;
					
					//user1 value is initialized as 0 since for-statement below does not make an excpetion for the target nodes (1-hop or srp)
					for(other1 = list->next; other1 != NULL; other1 = other1->next){
							unsigned char pr_dof2 = get_pr_dof(dst_list, other1->addr);
							if (pr_dof2 == 0)
								continue;
							
							if (n_to_k(clout1, other1->total_num, other1->rcv_num[rate1]) >= pr_dof2){
								user1++;
							}
					}

					for (r_info = relay_list->next; r_info != NULL; r_info = r_info->next){
						if (r_info->type == 0 && r_info->clout > 0 && rate1 < r_info->rate){
							if (r_info->clout >= remain_c){
								time_cost += cal_tx_time(rate1, remain_c, pkt_len)-cal_tx_time(r_info->rate, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time_cost += cal_tx_time(rate1, r_info->clout, pkt_len)-cal_tx_time(r_info->rate, r_info->clout, pkt_len);
								remain_c = remain_c - r_info->clout;
							}
						}
					}

					time_cost+=cal_tx_time(rate1, remain_c, pkt_len);
					charge = get_charge(src_batt);
					capacity = get_capa(src_batt);			

					if (charge == 1){
						total_cost = time_cost*1000/(omega*capacity); 
					}
					else{
						total_cost = time_cost*1000/(capacity); 
					}

					cost1 = total_cost;
				}//if either 1-hop or the chosen srp is unserved
				for(hop2 = (hop1->nbr_list).next; hop2 != NULL; hop2 = hop2->next){
					unsigned char pr_dof2 = 0;  
					unsigned char charge = 0;
					unsigned char capacity = 0;
					unsigned char remain_c = 0;	
					unsigned char dof_by_src = 0;
					struct tr_info * info_from_src;
					struct tr_info * other2;
					struct tr_info * hop22;
				
					time2 = 0;	
					cost2 = 0;	
					info_from_src =	tr_info_find_addr(list, hop2->addr);
					if (info_from_src != NULL)
						dof_by_src = n_to_k(clout1, info_from_src->total_num, info_from_src->rcv_num[rate1]);
					else
						dof_by_src = 0;	
					
					pr_dof2 = get_pr_dof(dst_list, hop2->addr) > dof_by_src ? get_pr_dof(dst_list, hop2->addr) - dof_by_src : 0;

					// if the pr_dof of that node is 0 or 
					if (pr_dof2 == 0)
						continue;
				
					user2 = 1;	
					rate2 = find_best_mcs(hop2, pr_dof2); 				
					clout2 = k_to_n(pr_dof2, hop2->total_num, hop2->rcv_num[rate2]);
					remain_c = clout2;	
				
					for(other2 = (hop1->nbr_list).next; other2 != NULL; other2 = other2->next){
						if (hop2 != other2){
							unsigned char pr_dof_other2 = get_pr_dof(dst_list, other2->addr);
							info_from_src =	tr_info_find_addr(list, other2->addr);
							if (info_from_src != NULL)
								dof_by_src = n_to_k(clout1, info_from_src->total_num, info_from_src->rcv_num[rate1]);	
							else
								dof_by_src = 0;

							pr_dof_other2 = pr_dof_other2 > dof_by_src ? pr_dof_other2 - dof_by_src : 0;							
	
							if (pr_dof_other2 == 0)
								continue;
							
							if (n_to_k(clout2, other2->total_num, other2->rcv_num[rate2]) >= pr_dof_other2){
								user2++;	
							}
						}	
					}
					for (r_info = relay_list->next; r_info != NULL; r_info = r_info->next){
						if (!memcmp(r_info->addr, hop1->addr, ETH_ALEN) && r_info->clout > 0 && rate2 < r_info->rate){
							if (r_info->clout >= remain_c){
								time2 += cal_tx_time(rate2, remain_c, pkt_len)-cal_tx_time(r_info->rate, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time2 += cal_tx_time(rate2, r_info->clout, pkt_len)-cal_tx_time(r_info->rate, r_info->clout, pkt_len);
								remain_c = remain_c - r_info->clout;
							}
						}
					}
					
					time2+=cal_tx_time(rate2, remain_c, pkt_len);
					charge = get_charge(hop1->batt);
					capacity = get_capa(hop1->batt);			
					
					if (charge == 1){
						cost2 = time2*1000 / (omega*capacity);
					}
					else{
						cost2 = time2*1000 / capacity;	
					}
				
					if(cost2 == 0){
						printk("Zero cost2\n");
					}
				#if 1
					// the chosen 2-hop node of the target 1-hop node
				for(hop22 = (srp->nbr_list).next; hop22 != NULL; hop22 = hop22->next){
					struct tr_info * other2;
					unsigned char remain_c = 0;
					unsigned char pr_dof2 = 0;	
					unsigned char dof_by_src = 0;
					struct tr_info * info_from_src;

					time3 = 0;	
					#if 1	
					info_from_src =	tr_info_find_addr(list, hop22->addr);
					if (info_from_src != NULL)
						dof_by_src = n_to_k(clout1, info_from_src->total_num, info_from_src->rcv_num[rate1]);
					else
						dof_by_src = 0;	
					
					pr_dof2 = get_pr_dof(dst_list, hop22->addr) > dof_by_src ? get_pr_dof(dst_list, hop22->addr) - dof_by_src : 0;

					// if the pr_dof of that node is 0 or dst_info is found
					if (pr_dof2 == 0)
						continue;
					
					user3 = 1;
					rate3 = find_best_mcs(hop22, pr_dof2); 				
					clout3 = k_to_n(pr_dof2, hop22->total_num, hop22->rcv_num[rate3]);
					remain_c = clout3;	

					for(other2 = (srp->nbr_list).next; other2 != NULL; other2 = other2->next){
						if (hop22 != other2){
							unsigned char pr_dof_other2 = get_pr_dof(dst_list, other2->addr);
							info_from_src =	tr_info_find_addr(list, other2->addr);
							if (info_from_src != NULL)
								dof_by_src = n_to_k(clout1, info_from_src->total_num, info_from_src->rcv_num[rate1]);	
							else
								dof_by_src = 0;

							pr_dof_other2 = pr_dof_other2 > dof_by_src ? pr_dof_other2 - dof_by_src : 0;							
	
							if (pr_dof_other2 == 0)
								continue;
							
							if (n_to_k(clout2, other2->total_num, other2->rcv_num[rate3]) >= pr_dof_other2){
								user3++;	
							}
						}	
					}
					for (r_info = relay_list->next; r_info != NULL; r_info = r_info->next){
						if (!memcmp(r_info->addr, hop1->addr, ETH_ALEN) && r_info->clout > 0 && rate3 < r_info->rate){
							if (r_info->clout >= remain_c){
								time3 += cal_tx_time(rate3, remain_c, pkt_len)-cal_tx_time(r_info->rate, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time3 += cal_tx_time(rate3, r_info->clout, pkt_len)-cal_tx_time(r_info->rate, r_info->clout, pkt_len);
								remain_c = remain_c - r_info->clout;
							}
						}
					}

					time3 += cal_tx_time(rate3, remain_c, pkt_len);
					charge = get_charge(srp->batt);
					capacity = get_capa(srp->batt);			

					if (charge == 1){
						total_cost = cost1 + cost2 +  time3*1000 / (omega*capacity);
					}
					else{
						total_cost = cost1 + cost2 +  time3*1000 / capacity;	
					}
					utility = (user1+user2+user3)*10000000 / total_cost;					

					if (user1 == 0)
					{
						printk("Type 4 Clout2: %d Rate2: %d Clout3: %d Rate3: %d -->  Utility: %d (User2: %d User3: %d Time2: %d Time3: %d)\n", clout2, rate2, clout3, rate3, utility, user2, user3, time2, time3);
					}
					else 
						printk("Type 5 Clout1: %d Rate1: %d Clout2: %d Rate2: %d Clout3: %d Rate3: %d -->  Utility: %d (User1: %d User2: %d User3: %d Time1: %d Time2: %d Time3: %d)\n", clout1, rate1, clout2, rate2, clout3, rate3, utility, user1, user2, user3, time_cost, time2, time3);
					
						if (utility >= relay.utility){
							relay.utility = utility;
							relay.round = round;
							relay.rate1 = rate1;
							relay.clout1 = clout1;
							memcpy(relay.addr2, hop1->addr, ETH_ALEN);  
							relay.rate2 = rate2;
							relay.clout2 = clout2;
							memcpy(relay.addr3, srp->addr, ETH_ALEN);  
							relay.rate3 = rate3;
							relay.clout3 = clout3; 
					
							//printk("Relay sample update\n");
							//selected_relay_info_print(&relay); 
						}	
#endif
					}// the chosen 2-hop node by the srp of the target node	
#endif
				}// the chosen  chosen 2-hop node of the target 1-hop node 
			}// with the selected srp
#endif
		}//hop1 node
#endif
		
		if (relay.clout1 > 0){
			struct tr_info * by_src;
			unsigned char src_addr[ETH_ALEN] = {0};

			for (by_src = list->next; by_src != NULL; by_src = by_src->next){
				struct dst_info * dst;
				dst = dst_info_find_addr(dst_list, by_src->addr);

				if (dst->pr_dof == 0)
					continue;
				else{
					unsigned char dof_by_src = n_to_k(relay.clout1, by_src->total_num, by_src->rcv_num[relay.rate1]);
					dst->pr_dof = dst->pr_dof > dof_by_src ? dst->pr_dof - dof_by_src : 0;
					if(dst->pr_dof == 0)
						dst->round = round;
				}
			}
			
			relay_info_insert(relay_info_create(round, 0, src_addr, relay.clout1, relay.rate1), relay_list);
		}
		if (relay.clout2 > 0){
			struct tr_info * hop1 = tr_info_find_addr(list, relay.addr2);
			struct tr_info * by_hop1;
			
			if (hop1 != NULL){
				for (by_hop1 = (hop1->nbr_list).next; by_hop1 != NULL; by_hop1 = by_hop1->next){
					struct dst_info * dst;
					dst = dst_info_find_addr(dst_list, by_hop1->addr);

					if (dst->pr_dof == 0)
						continue;
					else{
						unsigned char dof_by_hop1 = n_to_k(relay.clout2, by_hop1->total_num, by_hop1->rcv_num[relay.rate2]);
						dst->pr_dof = dst->pr_dof > dof_by_hop1 ? dst->pr_dof - dof_by_hop1 : 0;
						if(dst->pr_dof == 0)
							dst->round = round;
					}
				}
			}

			relay_info_insert(relay_info_create(round, 1, relay.addr2, relay.clout2, relay.rate2), relay_list);
		}
		if (relay.clout3 > 0){
			struct tr_info * srp = tr_info_find_addr(list, relay.addr3);
			struct tr_info * by_srp;

			for (by_srp = (srp->nbr_list).next; by_srp != NULL; by_srp = by_srp->next){
				struct dst_info * dst;
				dst = dst_info_find_addr(dst_list, by_srp->addr);

				if (dst->pr_dof == 0)
					continue;
				else{
					unsigned char dof_by_srp = n_to_k(relay.clout3, by_srp->total_num, by_srp->rcv_num[relay.rate3]);
					dst->pr_dof = dst->pr_dof > dof_by_srp ? dst->pr_dof - dof_by_srp : 0;
					if(dst->pr_dof == 0)
						dst->round = round;
				}
			}
			
			relay_info_insert(relay_info_create(round, 2, relay.addr3, relay.clout3, relay.rate3), relay_list);
		}
		
		dst_info_list_print(dst_list);
		relay_info_list_print(relay_list);
		
		used_time_us = rsc_adjust(relay_list, dst_list, list, round);

		printk("Selection after round %d used_time %d total_time %d \n", round, used_time_us, max_time_us);	
		
		if (used_time_us > max_time_us){
			struct relay_info * copy_info;
			
			printk(KERN_INFO "Termination of the algorithm due to the lack of airtime\n");
			relay_info_list_purge(relay_list);				
			relay_info_list_init(relay_list);	
		
			for (copy_info = prev_relay_list.next; copy_info != NULL; copy_info=copy_info->next){
				relay_info_insert(relay_info_create(copy_info->round, copy_info->type, copy_info->addr, copy_info->clout, copy_info->rate), relay_list);
			} 	
			break;
		}
	}//end while all zero
	
	#endif	
	relay_info_list_purge(&prev_relay_list);
	relay_info_list_print(relay_list);
	assign_offset(relay_list, list);
}
//evcast_end

void tl_select_relay(struct tr_info_list *list, struct relay_info_list *relay, struct dst_info_list *dst, unsigned char batt){
	struct timespec tl_diff;
	getrawmonotonic(&tl_end);
	tl_diff = timespec_sub(tl_end, tl_start);
	//printk("Topology learning time: %ld\n", timespec_to_ns(&tl_diff));
	if(list == NULL){
		printk("Select Relay error1\n");
		return;
	}

	if(list->next == NULL){
		printk("Select Relay error2\n");
		return;
	}

	tr_info_list_arrangement(list);
	
	evcast_relay(list, relay, dst, batt); 
	//if(MNPRELAY) mnp_relay(list);
	//else min_max_relay(list);
}

void dst_info_list_init(struct dst_info_list *list){
	list->next = NULL;
	list->qlen = 0;
}

struct dst_info *dst_info_create(unsigned char addr[], unsigned char pr_dof, unsigned char round){
	struct dst_info *info;
	info = kmalloc(sizeof(struct dst_info), GFP_ATOMIC);
	memcpy(info->addr, addr, ETH_ALEN);
	info->pr_dof = pr_dof;
	info->round = round;
	info->next = NULL;
	info->prev = NULL;
	info->head = NULL;

	return info;
}

void dst_info_list_purge(struct dst_info_list *list){
	struct dst_info *info;

	if(list == NULL) return;

	info = list->next;

	while(info != NULL){
		dst_info_free(info);
		info = list->next;
	}
}

void dst_info_free(struct dst_info *info){
	if(info == NULL) return;

	else if(info->head == NULL){
	}

	else{
		struct dst_info *next;
		struct dst_info *prev;
		prev = info->prev;
		next = info->next;

		if(prev == NULL){
			if(next == NULL){
				info->head->next = NULL;
			}
			else{
				info->head->next = next;
				next->prev = NULL;
			}
		}
		else{
			if(next == NULL){
				prev->next = NULL;
			}
			else{
				prev->next = next;
				next->prev = prev;
			}
		}
		info->head->qlen--;
	}
	kfree(info);
}

void dst_info_insert(struct dst_info *newinfo, struct dst_info_list *list){
	struct dst_info *info;

	newinfo->head = list;

	if((info = list->next) == NULL){
		list->next = newinfo;
	}

	else{
		while(info->next != NULL){
			info = info->next;
		}

		info->next = newinfo;
		newinfo->prev = info;
	}
	list->qlen++;
}

struct dst_info *dst_info_find_addr(struct dst_info_list *list, unsigned char addr[]){
	struct dst_info *info;

	if(list == NULL) return NULL;

	if((info = list->next) == NULL) return NULL;

	while(info != NULL){
		if(!memcmp(info->addr, addr, ETH_ALEN)) return info;
		info = info->next;
	}

	return NULL;
}

void tr_info_list_init(struct tr_info_list *list){
	list->next = NULL;
	list->qlen = 0;
}

struct tr_info *tr_info_create(unsigned char addr[], struct net_device *dev, unsigned int total_num, unsigned int rcv_num[], unsigned char rssi, unsigned char batt){
	struct tr_info *info;
	info = kmalloc(sizeof(struct tr_info), GFP_ATOMIC);
	memcpy(info->addr, addr, ETH_ALEN);
	info->dev = dev;
	info->total_num = total_num;
	memcpy(info->rcv_num, rcv_num, sizeof(unsigned int)*NUM_MCS);
	info->rssi = rssi;
	info->batt = batt;
	info->tf_cnt = false;
	info->nr_cnt = false;
	tr_info_list_init(&(info->nbr_list));
	tr_info_list_init(&(info->srp_list));
	info->next = NULL;
	info->prev = NULL;
	info->head = NULL;

	return info;
}

void tr_info_list_purge(struct tr_info_list *list){
	struct tr_info *info;

	if(list == NULL) return;

	info = list->next;

	while(info != NULL){
		tr_info_free(info);
		info = list->next;
	}
	// printk("list->next = %x, list->qlen = %d\n", list->next, list->qlen);
}

void tr_info_free(struct tr_info *info){
	if(info == NULL) return;

	else if(info->head == NULL){
	}

	else{
		struct tr_info *next;
		struct tr_info *prev;
		prev = info->prev;
		next = info->next;

		if(prev == NULL){
			if(next == NULL){
				info->head->next = NULL;
			}
			else{
				info->head->next = next;
				next->prev = NULL;
			}
		}
		else{
			if(next == NULL){
				prev->next = NULL;
			}
			else{
				prev->next = next;
				next->prev = prev;
			}
		}

		info->head->qlen--;
	}
	tr_info_list_purge(&(info->nbr_list));
	tr_info_list_purge(&(info->srp_list));
	kfree(info);
}

void tr_info_insert(struct tr_info *newinfo, struct tr_info_list *list){
	struct tr_info *info;
	newinfo->head = list;

	if((info = list->next) == NULL){
		list->next = newinfo;
	}

	else{
		while(info->next != NULL){
			info = info->next;
		}

		info->next = newinfo;
		newinfo->prev = info;
	}
	list->qlen++;
}

struct tr_info *tr_info_find_addr(struct tr_info_list *list, unsigned char addr[]){
	struct tr_info *info;

	if(list == NULL) return NULL;

	if((info = list->next) == NULL) return NULL;

	while(info != NULL){
		if(!memcmp(info->addr, addr, ETH_ALEN)) return info;
		info = info->next;
	}

	return NULL;
}

bool tr_info_check_nr_cnt(struct tr_info_list *list){
	struct tr_info *info;

	if(list == NULL) return false;

	info = list->next;

	while(info != NULL){
		if(info->nr_cnt == false) return false;
		info = info->next;
	}
	return true;
}

void trinfo_print(struct tr_info *info){
	printk(KERN_INFO "Training info: rssi = %d, batt = %d, rcv0 = %d, rcv1 = %d  rcv2 = %d rcv3 = %d rcv4 = %d rcv5 = %d rcv6 = %d rcv7 = %d\n", info->rssi, info->batt, info->rcv_num[0], info->rcv_num[1], info->rcv_num[2], info->rcv_num[3], info->rcv_num[4], info->rcv_num[5], info->rcv_num[6], info->rcv_num[7] );
}
void relay_info_list_init(struct relay_info_list *list){
	list->next = NULL;
	list->qlen = 0;
}

void dst_info_list_print(struct dst_info_list *list){
	struct dst_info *test_info = list->next;
	printk("-------------------------dst_list status---------------------------\n");
	while(test_info != NULL){
		printk(KERN_INFO "Addr %x:%x:%x:%x:%x:%x, pr_dof %d, round: %d\n", test_info->addr[0], test_info->addr[1], test_info->addr[2], test_info->addr[3], test_info->addr[4], test_info->addr[5], test_info->pr_dof, test_info->round);
		test_info = test_info->next;
		}
	
	printk("-----------------------------------------------------------------------\n");

}

void vimor_info_list_print(struct vimor_info_list *list){
	struct vimor_info *test_info = list->next;
	printk("-------------------------ViMOR List---------------------------\n");
	while(test_info != NULL){
		printk(KERN_INFO "Addr %x:%x:%x:%x:%x:%x, hop: %d rate: %d dist: %d parent: %x:%x:%x:%x:%x:%x\n", test_info->addr[0], test_info->addr[1], test_info->addr[2], test_info->addr[3], test_info->addr[4], test_info->addr[5], test_info->hop, test_info->rate, test_info->dist, test_info->parent_addr[0], test_info->parent_addr[1], test_info->parent_addr[2], test_info->parent_addr[3], test_info->parent_addr[4], test_info->parent_addr[5]);
		test_info = test_info->next;
		}
	
	printk("-----------------------------------------------------------------------\n");
}


void tr_info_list_print(struct tr_info_list *list){
	struct tr_info *test_info = list->next;
	printk("-------------------------src_nbr_list status---------------------------\n");
	while(test_info != NULL){
		struct tr_info *test_src_nbr_info = (&(test_info->nbr_list))->next;
		printk(KERN_INFO "1hop %x:%x:%x:%x:%x:%x, rssi %d, batt %x, (%d %d %d %d %d %d %d %d %d %d %d %d)/%d, %d, %d\n", test_info->addr[0], test_info->addr[1], test_info->addr[2], test_info->addr[3], test_info->addr[4], test_info->addr[5], test_info->rssi, test_info->batt, test_info->rcv_num[0], test_info->rcv_num[1], test_info->rcv_num[2], test_info->rcv_num[3], test_info->rcv_num[4], test_info->rcv_num[5], test_info->rcv_num[6], test_info->rcv_num[7],  test_info->rcv_num[8], test_info->rcv_num[9], test_info->rcv_num[10], test_info->rcv_num[11], test_info->total_num, test_info->tf_cnt, test_info->nr_cnt);
		//printk(KERN_INFO "1-hop addr: %x:%x:%x:%x:%x:%x, total_num = %d, rcv_num = %d, tf_cnt = %d, nr_cnt = %d\n", test_info->addr[0], test_info->addr[1], test_info->addr[2], test_info->addr[3], test_info->addr[4], test_info->addr[5], test_info->total_num, test_info->rcv_num, test_info->tf_cnt, test_info->nr_cnt);
		while(test_src_nbr_info != NULL){
			printk(KERN_INFO "  ---> %x:%x:%x:%x:%x:%x, rssi %d, batt %x, (%d %d %d %d %d %d %d %d %d %d %d %d)/%d, %d, %d\n", test_src_nbr_info->addr[0], test_src_nbr_info->addr[1], test_src_nbr_info->addr[2], test_src_nbr_info->addr[3], test_src_nbr_info->addr[4], test_src_nbr_info->addr[5], test_src_nbr_info->rssi, test_src_nbr_info->batt, test_src_nbr_info->rcv_num[0], test_src_nbr_info->rcv_num[1], test_src_nbr_info->rcv_num[2], test_src_nbr_info->rcv_num[3], test_src_nbr_info->rcv_num[4], test_src_nbr_info->rcv_num[5], test_src_nbr_info->rcv_num[6], test_src_nbr_info->rcv_num[7], test_src_nbr_info->rcv_num[8], test_src_nbr_info->rcv_num[9], test_src_nbr_info->rcv_num[10], test_src_nbr_info->rcv_num[11], test_src_nbr_info->total_num, test_src_nbr_info->tf_cnt, test_src_nbr_info->nr_cnt);
			test_src_nbr_info = test_src_nbr_info->next;
		}
		test_info = test_info->next;
	}
	printk("-----------------------------------------------------------------------\n");

}


struct relay_info *relay_info_create(unsigned char round, unsigned char type, unsigned char addr[], unsigned char clout, unsigned char rate){
	struct relay_info *info;
	info = kmalloc(sizeof(struct relay_info), GFP_ATOMIC);
	info->round = round;
	info->type = type;
	memcpy(info->addr, addr, ETH_ALEN);
	info->clout = clout;
	info->rate = rate;
	info->offset = 0;
	info->next = NULL;
	info->prev = NULL;
	info->head = NULL;

	return info;
}

void relay_info_list_purge(struct relay_info_list *list){
	struct relay_info *info;

	if(list == NULL) return;

	info = list->next;

	while(info != NULL){
		relay_info_free(info);
		info = list->next;
	}
}

void relay_info_free(struct relay_info *info){
	if(info == NULL) return;

	else if(info->head == NULL){
	}

	else{
		struct relay_info *next;
		struct relay_info *prev;
		prev = info->prev;
		next = info->next;

		if(prev == NULL){
			if(next == NULL){
				info->head->next = NULL;
			}
			else{
				info->head->next = next;
				next->prev = NULL;
			}
		}
		else{
			if(next == NULL){
				prev->next = NULL;
			}
			else{
				prev->next = next;
				next->prev = prev;
			}
		}
		info->head->qlen--;
	}
	kfree(info);
}

void relay_info_insert(struct relay_info *newinfo, struct relay_info_list *list){
	struct relay_info *info;

	newinfo->head = list;

	if((info = list->next) == NULL){
		list->next = newinfo;
	}

	else{
		while(info->next != NULL){
			info = info->next;
		}

		info->next = newinfo;
		newinfo->prev = info;
	}
	list->qlen++;
}

void selected_relay_info_print(struct selected_relay_info *test_info){
	printk(KERN_INFO "Round: %d (Source  Clout: %d Rate: %d) (Relay1 Addr %x:%x:%x:%x:%x:%x Clout: %d Rate: %d) (SRP Addr %x:%x:%x:%x:%x:%x Clout: %d Rate: %d)\n", test_info->round, test_info->clout1, test_info->rate1, test_info->addr2[0], test_info->addr2[1], test_info->addr2[2], test_info->addr2[3], test_info->addr2[4], test_info->addr2[5], test_info->clout2, test_info->rate2, test_info->addr3[0], test_info->addr3[1], test_info->addr3[2], test_info->addr3[3], test_info->addr3[4], test_info->addr3[5], test_info->clout3, test_info->rate3);
}

void relay_info_list_print(struct relay_info_list *list){
	struct relay_info * test_info = list->next;
	while(test_info != NULL){
	printk(KERN_INFO "Round: %d Type: %d Addr %x:%x:%x:%x:%x:%x Clout: %d Rate: %d Offset: %d\n", test_info->round, test_info->type, test_info->addr[0], test_info->addr[1], test_info->addr[2], test_info->addr[3], test_info->addr[4], test_info->addr[5], test_info->clout, test_info->rate, test_info->offset);
	test_info = test_info->next;
	}
}

void vimor_info_list_init(struct vimor_info_list *list){
	list->next = NULL;
	list->qlen = 0;
}

struct vimor_info *vimor_info_create(unsigned char addr[], unsigned char rate, unsigned char hop, unsigned int dist, unsigned char p_addr[]){
	struct vimor_info *info;
	info = kmalloc(sizeof(struct vimor_info), GFP_ATOMIC);
	memcpy(info->addr, addr, ETH_ALEN);
	info->rate = rate;
	info->hop = hop;
	info->dist = dist;
	memcpy(info->parent_addr, p_addr, ETH_ALEN);
	vimor_info_list_init(&(info->child_list));
	
	info->next = NULL;
	info->prev = NULL;
	info->head = NULL;

	return info;
}

void vimor_info_list_purge(struct vimor_info_list *list){
	struct vimor_info *info;

	if(list == NULL) return;

	info = list->next;

	while(info != NULL){
		vimor_info_free(info);
		info = list->next;
	}
}

void vimor_info_free(struct vimor_info *info){
	if(info == NULL) return;

	else if(info->head == NULL){
	}

	else{
		struct vimor_info *next;
		struct vimor_info *prev;
		prev = info->prev;
		next = info->next;

		if(prev == NULL){
			if(next == NULL){
				info->head->next = NULL;
			}
			else{
				info->head->next = next;
				next->prev = NULL;
			}
		}
		else{
			if(next == NULL){
				prev->next = NULL;
			}
			else{
				prev->next = next;
				next->prev = prev;
			}
		}
		info->head->qlen--;
	}
	vimor_info_list_purge(&(info->child_list));
	kfree(info);
}

void vimor_info_insert(struct vimor_info *newinfo, struct vimor_info_list *list){
	struct vimor_info *info;

	newinfo->head = list;

	if((info = list->next) == NULL){
		list->next = newinfo;
	}

	else{
		while(info->next != NULL){
			info = info->next;
		}

		info->next = newinfo;
		newinfo->prev = info;
	}
	list->qlen++;
}

struct vimor_info *vimor_info_find_addr(struct vimor_info_list *list, unsigned char addr[]){
	struct vimor_info *info;

	if(list == NULL) return NULL;

	if((info = list->next) == NULL) return NULL;

	while(info != NULL){
		if(!memcmp(info->addr, addr, ETH_ALEN)) return info;
		info = info->next;
	}

	return NULL;
}

unsigned int rsc_adjust(struct relay_info_list * r_list, struct dst_info_list * d_list, struct tr_info_list * t_list, unsigned char round){
	struct dst_info * d_info = d_list->next;
	struct dst_info_list temp_dst_list;
	struct dst_info_list dst_list_by_src;
	unsigned char max_src_clout = 0;
	unsigned char max_relay_clout = 0;
	struct relay_info * r_info = r_list->next;
	unsigned int airtime = 0;
	unsigned int pkt_len = 1400;

	dst_info_list_init(&temp_dst_list);
	dst_info_list_init(&dst_list_by_src);

	if (round == 1)
	{
		for(r_info = r_list->next; r_info != NULL; r_info = r_info->next){
			if (r_info->type != 2)
				airtime += cal_tx_time(r_info->rate, r_info->clout, pkt_len);
			else{   
				struct relay_info * pair_info;
				for(pair_info = r_list->next; pair_info != NULL; pair_info = pair_info->next){
					if (pair_info->round == r_info->round && pair_info->type == 1)
						break;
				}	

				if (pair_info != NULL){	
					airtime -= cal_tx_time (pair_info->rate, pair_info->clout, pkt_len);	
					airtime += cal_tx_time(r_info->rate, r_info->clout, pkt_len) >  cal_tx_time(pair_info->rate, pair_info->clout, pkt_len) ? cal_tx_time(r_info->rate, r_info->clout, pkt_len) :  cal_tx_time(pair_info->rate, pair_info->clout, pkt_len); 
				}
			}	
		}
		
		return airtime;
	}

	for (d_info = d_list->next; d_info != NULL; d_info = d_info->next){
		d_info->pr_dof = 10;
		dst_info_insert(dst_info_create(d_info->addr, 10, d_info->round), &temp_dst_list);
		dst_info_insert(dst_info_create(d_info->addr, 10, d_info->round), &dst_list_by_src);
	}

	printk(KERN_INFO "Start Adjustment\n");	
	
	while(r_info != NULL){
		if (r_info->type == 0 && r_info->clout > max_src_clout)
			max_src_clout = r_info->clout;
		if (r_info->type != 0 && r_info->clout > max_relay_clout)
			max_relay_clout = r_info->clout;
		
		if (r_info->round == round && r_info->type == 0){
			struct relay_info *prev_info = r_list->next;
			struct relay_info *next_info = r_info->prev;

			while (prev_info != NULL){
				if (r_info->rate < prev_info->rate || prev_info->type != 0){
					if (prev_info == r_list->next){
						r_list->next = r_info;
						r_info->prev = NULL;
					}
					else{
						struct relay_info * prev_prev_info = prev_info->prev; 
						prev_prev_info->next = r_info;
						r_info->prev = prev_prev_info;
					}
					
					if (next_info != NULL){
						next_info->next = r_info->next;
						if (r_info->next != NULL)
							r_info->next->prev = next_info;
					}
						
					r_info->next = prev_info;
					prev_info->prev = r_info;

					break;	
				}
				prev_info = prev_info->next; 
			}
		}
		if (r_info->round == round && r_info->type != 0){
			struct relay_info *prev_info = r_list->next;
			struct relay_info *next_info = r_info->prev;

			while (prev_info != NULL){
				if (r_info->rate < prev_info->rate && prev_info->type !=0){
					if (prev_info == r_list->next){
						r_list->next = r_info;
						r_info->prev = NULL;
					}
					else{
						struct relay_info * prev_prev_info = prev_info->prev; 
						prev_prev_info->next = r_info;
						r_info->prev = prev_prev_info;
					}
					
					if (next_info != NULL){
						next_info->next = r_info->next;
						if (r_info->next != NULL)
							r_info->next->prev = next_info;
					}
						
					r_info->next = prev_info;
					prev_info->prev = r_info;

					break;	
				}
				prev_info = prev_info->next; 
			}
		}
	
		r_info = r_info->next;		
	}	
	// end of source txrsc re-order
	//printk(KERN_INFO "After re-ordering\n");
	//relay_info_list_print(r_list);


	//Source node intra-node adjustment
	
		
	// 1) Find the dof list contributed by the source node

	for (r_info = r_list->next; r_info != NULL; r_info = r_info->next){
		struct tr_info * info;
		
		if (r_info->type == 0){
			for (info = t_list->next; info != NULL; info = info->next){
				struct dst_info * d_info = dst_info_find_addr(&dst_list_by_src, info->addr);
				if (d_info->round > 0){
					unsigned char dof = 0;	

					if (d_info->pr_dof == 0)
						continue;

					dof = n_to_k(r_info->clout, info->total_num, info->rcv_num[r_info->rate]);
					d_info->pr_dof = d_info->pr_dof > dof ? d_info->pr_dof - dof : 0;
				}
			}
		}
	}

	//printk("Served by source\n");
	//dst_info_list_print(&dst_list_by_src);
/*	
	for (d_info = dst_list_by_src.next; d_info != NULL; d_info = d_info->next){
		if (d_info->pr_dof == 10)
			dst_info_free(d_info);
	}
*/


	// 2) Find the most efficient resource assignment such that satisfies the dof obtained by 1)
/*
	for (r_info = r_list->next; r_info != NULL; r_info = r_info->next){
		struct relay_info * info;
		for (info = r_list->next; info != NULL; info = info->next){
			if (info->type == 0 && info->clout > max_src_clout)
				max_src_clout = info->clout;
		}
	}		
*/
	for (r_info = r_list->next; r_info != NULL; r_info = r_info->next){
		struct tr_info * info;
		unsigned char max_clout = 0;
		
		if (r_info->type == 0){
			for (info = t_list->next; info != NULL; info = info->next){
				struct dst_info * temp_d_info = dst_info_find_addr(&temp_dst_list, info->addr);
				struct dst_info * src_d_info = dst_info_find_addr(&dst_list_by_src, info->addr);
				unsigned char i = 0;
				unsigned char tmp_clout = 0;
				
				//if (src_d_info->pr_dof == 10)
				//	continue;
					
				//printk("Temp dof: %d Src dof: %d\n", temp_d_info->pr_dof, src_d_info->pr_dof);
					
				if (temp_d_info->pr_dof <= src_d_info->pr_dof || temp_d_info->round == 0 )
				{
							continue;
				}
				for (i=0; i <= max_src_clout; i++){
				//for (i=0; i <= max_relay_clout; i++){
					unsigned char dof = n_to_k(i, info->total_num, info->rcv_num[r_info->rate]);
					//printk("i: %d dof: %d rcv: %d\n ", i, dof, info->rcv_num[r_info->rate]);
					if (temp_d_info->pr_dof - src_d_info->pr_dof >= dof){
							tmp_clout = i;	
					}
					else
						break;
					
					if (dof == temp_d_info->pr_dof - src_d_info->pr_dof)
						break;
				}
				//printk("%d packets are needed to serve\n", tmp_clout);
				if (tmp_clout > max_clout)
					max_clout = tmp_clout;
			}

			if (r_info->clout != max_clout)
				printk("Adjusted from %d to %d\n", r_info->clout, max_clout);
			
			r_info->clout = max_clout;
			
			if (r_info->clout == 0){
				relay_info_free(r_info);
				continue;
			}		
			// Find the adjusted clout 
			for (info = t_list->next; info != NULL; info = info->next){
				struct dst_info * temp_d_info = dst_info_find_addr(&temp_dst_list, info->addr);
				struct dst_info * new_d_info = dst_info_find_addr(d_list, info->addr);
				unsigned char dof = 0;	

				if (temp_d_info->pr_dof == 0 || new_d_info->pr_dof == 0)
					continue;

			 	dof = n_to_k(r_info->clout, info->total_num, info->rcv_num[r_info->rate]);
				temp_d_info->pr_dof = temp_d_info->pr_dof > dof ? temp_d_info->pr_dof - dof : 0;
				new_d_info->pr_dof = new_d_info->pr_dof > dof ? new_d_info->pr_dof - dof : 0;
				if (new_d_info->pr_dof == 0)
					new_d_info->round = r_info->round;
			}
		}
			else{
			struct tr_info * hop1 = tr_info_find_addr(t_list, r_info->addr);
			for (info = (hop1->nbr_list).next; info != NULL; info = info->next){
				struct dst_info * temp_d_info = dst_info_find_addr(&temp_dst_list, info->addr);
				unsigned char i = 0;
				unsigned char tmp_clout = 0;

				if (temp_d_info->pr_dof == 0 || temp_d_info->round == 0 )
					continue;

				for (i=0; i <= max_relay_clout; i++){
					unsigned char dof = n_to_k(i, info->total_num, info->rcv_num[r_info->rate]);
					if (temp_d_info->pr_dof >= dof){
						tmp_clout = i;	
					}
					else
						break;
					if (dof == temp_d_info->pr_dof)
						break;
				}
				if (tmp_clout > max_clout)
					max_clout = tmp_clout;
			}	
			
			if (r_info->clout != max_clout)
				printk("Adjusted from %d to %d\n", r_info->clout, max_clout);
			
			r_info->clout = max_clout;
			if (r_info->clout == 0){
				relay_info_free(r_info);
				continue;
			}		
				// Find the adjusted clout 
			for (info = (hop1->nbr_list).next; info != NULL; info = info->next){
				struct dst_info * temp_d_info = dst_info_find_addr(&temp_dst_list, info->addr);
				struct dst_info * new_d_info = dst_info_find_addr(d_list, info->addr);
				unsigned char dof = 0;	

				if (temp_d_info->pr_dof == 0 || new_d_info->pr_dof == 0)
					continue;

				dof = n_to_k(r_info->clout, info->total_num, info->rcv_num[r_info->rate]);
				temp_d_info->pr_dof = temp_d_info->pr_dof > dof ? temp_d_info->pr_dof - dof : 0;
				new_d_info->pr_dof = new_d_info->pr_dof > dof ? new_d_info->pr_dof - dof : 0;
				if (new_d_info->pr_dof == 0)
					new_d_info->round = r_info->round;
			}
		}

		//printk("Dest info while adjustment\n");
		//dst_info_list_print(&temp_dst_list);
	}

	printk(KERN_INFO "After adjustment\n");
	relay_info_list_print(r_list);
	dst_info_list_print(d_list);

	for(r_info = r_list->next; r_info != NULL; r_info = r_info->next){
		if (r_info->type != 2)
			airtime += cal_tx_time(r_info->rate, r_info->clout, pkt_len);
		else{   
			struct relay_info * pair_info;
			for(pair_info = r_list->next; pair_info != NULL; pair_info = pair_info->next){
				if (pair_info->round == r_info->round && pair_info->type == 1)
					break;
			}	

			if (pair_info != NULL){	
				airtime -= cal_tx_time (pair_info->rate, pair_info->clout, pkt_len);	
				airtime += cal_tx_time(r_info->rate, r_info->clout, pkt_len) >  cal_tx_time(pair_info->rate, pair_info->clout, pkt_len) ? cal_tx_time(r_info->rate, r_info->clout, pkt_len) :  cal_tx_time(pair_info->rate, pair_info->clout, pkt_len); 
			}
		}	
	}

	dst_info_list_purge(&temp_dst_list);
	dst_info_list_purge(&dst_list_by_src);
	
	return airtime;	
}

void assign_offset(struct relay_info_list * r_list, struct tr_info_list * t_list){
	struct relay_info * r_info;
	unsigned int offset = 0;

	// 1) re-ordering in the order of addr
		
	// while(r_info != NULL){
	for (r_info = r_list->next; r_info != NULL; r_info=r_info->next){
		struct relay_info *prev_info = r_list->next;
		
		if (r_info->type == 0)
			continue;

		while (prev_info != r_info->prev){
			if (!memcmp(r_info->addr, prev_info->addr, ETH_ALEN)){
				while (!memcmp(prev_info->addr, prev_info->next->addr, ETH_ALEN)){
					prev_info = prev_info->next;	
				} 
				
				if (prev_info != r_info){
					r_info->prev->next = r_info->next;
					if (r_info->next != NULL)
						r_info->next->prev = r_info->prev;
					
					prev_info->next->prev = r_info;
					r_info->next = prev_info->next; 
	
					prev_info->next = r_info;
					r_info->prev = prev_info;
				}			
				
				break;
			}
			
			prev_info = prev_info->next;
		}
	}

	r_info = r_list->next;
	
	while(r_info != NULL){	
		if (r_info->offset > 0){
			r_info = r_info->next;
			continue;
		}		

		if (r_info->type == 0){
			r_info->offset = 0;		
			offset += calc_total_time(r_list, r_info->addr); 
		}
	
		if (r_info->type != 0){
			struct tr_info *tr = tr_info_find_addr (t_list, r_info->addr);
			struct relay_info * tmp_srp = NULL;
			struct relay_info *srp_relay = NULL;
			unsigned int time_relay = calc_total_time(r_list, r_info->addr);
			unsigned int time_srp = 0;
			unsigned int min_time_gap = 10000;

			for (tmp_srp = r_list->next; tmp_srp != NULL; tmp_srp = tmp_srp->next){
				if (tr_info_find_addr(&(tr->srp_list), tmp_srp->addr) != NULL){
					if (tmp_srp->round == r_info->round){
						srp_relay = tmp_srp;
						break;
					}
					else{
						unsigned int time_tmp = calc_total_time(r_list, tmp_srp->addr);
						unsigned int time_gap = time_relay > time_tmp ? time_relay-time_tmp : time_tmp - time_relay;
						if (time_gap < min_time_gap){
							min_time_gap = time_gap;
							time_srp = time_tmp;
							srp_relay = tmp_srp;
						}
					}	
				}
			}

			if (srp_relay == NULL){
				struct relay_info *info;

				for (info = r_list->next; info != NULL; info = info->next){
					if (!memcmp(r_info->addr, info->addr, ETH_ALEN)){
						info->offset = offset;
					}
				}
			
				offset += time_relay;
			}
			else{
				struct relay_info *info;
				unsigned int time_max = time_srp > time_relay? time_srp : time_relay;
				
				for (info = r_list->next; info != NULL; info = info->next){
					if (!memcmp(r_info->addr, info->addr, ETH_ALEN) || !memcmp(info->addr, srp_relay->addr, ETH_ALEN)){
						info->offset = offset;
					}
				}	
				offset += time_max; 
			}
		}	
		
		r_info = r_info->next;
	}
	
	printk(KERN_INFO "After Assignment\n");
	relay_info_list_print(r_list);	
}

unsigned int calc_total_time(struct relay_info_list * list, unsigned char addr[]){
	unsigned int time = 0;
	unsigned int pkt_len = 1400;
	struct relay_info * info;
	
	for (info = list->next; info != NULL; info = info->next){
		if (!memcmp(info->addr, addr, ETH_ALEN))
			time += cal_tx_time(info->rate, info->clout, pkt_len);
	} 

	return time;	
}

void vimor_relay(struct tr_info_list * list, struct relay_info_list * relay_list, struct dst_info_list * dst_list, unsigned char type){

	unsigned char src_addr[6] = {0xa, 0xa, 0xa, 0xa, 0xa, 0xa};
	struct vimor_info_list vimor_list;
	struct vimor_info * vimor;
	struct tr_info * info;
	unsigned char max_round = 20;
	unsigned char round = 0;
	unsigned char src_rate = 12;
	unsigned int bitrate = 1000000; // in bps
	unsigned int pkt_size = 1316;
	unsigned int fps = 30;
	unsigned int gop = 16;
	unsigned int t_slot = 0; // in msec
	unsigned int c1_max = 0;
	unsigned int gop_pkt = 0;	

	vimor_info_list_init(&vimor_list);
	dst_info_list_init(dst_list);
	relay_info_list_init(relay_list);
	

	gop_pkt = bitrate*gop/fps/pkt_size/8 + 1;
	t_slot = gop * 10 * 1000 / (gop_pkt * fps);  	

	printk("ViMOR Alg Start Type: %d T_slot: %d\n", type, t_slot); 	
	
	for(info = list->next; info != NULL; info = info->next){
		struct tr_info *nbr_info;
		struct tr_info_list * nbr_info_list = &(info->nbr_list);
			
		if(dst_info_find_addr(dst_list, info->addr)==NULL)
		{
			unsigned char pr_dof = tr_get_data_k();
			dst_info_insert(dst_info_create(info->addr, pr_dof, 0), dst_list);
		}
		
		if (vimor_info_find_addr(&vimor_list, info->addr)==NULL){
			unsigned int ett = find_ett(info);
			unsigned char rate = find_mcs(info);
			vimor_info_insert(vimor_info_create(info->addr, rate, 0, ett, src_addr), &vimor_list);
		}	
	
		for(nbr_info = nbr_info_list->next; nbr_info != NULL; nbr_info = nbr_info->next){
			if(dst_info_find_addr(dst_list, nbr_info->addr) == NULL){
				unsigned char pr_dof = tr_get_data_k();
				dst_info_insert(dst_info_create(nbr_info->addr, pr_dof, 0), dst_list);
			}
			if (vimor_info_find_addr(&vimor_list, nbr_info->addr)==NULL && tr_info_find_addr(list, nbr_info->addr)==NULL){
				unsigned char temp_addr[6] = {0};
				vimor_info_insert(vimor_info_create(nbr_info->addr, 0, 0,  0, temp_addr), &vimor_list);
			}	
		}
	}

	vimor_info_list_print(&vimor_list);
	//Get DataRate Use pr_dof for indicating Served node

	for(vimor = vimor_list.next; vimor != NULL; vimor = vimor->next){
		if (vimor->dist == 0){
			unsigned int ett = 1000000;
			struct vimor_info * tmp;
			struct vimor_info * parent = NULL; 
			for (tmp = vimor_list.next; tmp != NULL; tmp = tmp->next){
				if (tmp->dist > 0){
					struct tr_info * hop1 = tr_info_find_addr(list, tmp->addr);
					struct tr_info * hop2;
					if (hop1 == NULL)
						continue;
					hop2 = tr_info_find_addr(&(hop1->nbr_list), vimor->addr);
					if (hop2 != NULL){
						unsigned int temp_ett = tmp->dist + find_ett(hop2);
						unsigned char temp_rate = find_mcs(hop2);
						if (temp_ett < ett){
							vimor->dist = temp_ett;
							vimor->rate = temp_rate;
							vimor->hop = 2;
							memcpy(vimor->parent_addr, tmp->addr, ETH_ALEN);
							ett = temp_ett;
							parent = tmp;
						}
					}	
				}		
			}
			if (parent == NULL){
				printk("No parent is assigned\n");
			}
			else{
				vimor_info_insert(vimor_info_create(vimor->addr, vimor->rate, 2, vimor->dist, parent->addr), &(parent->child_list));
				parent->hop = 1;
			}
		}	
	}	
	//vimor_info_list_print(&vimor_list);	

	while(!vimor_all_zero(&vimor_list) && round < max_round){
		struct vimor_info * best_info = NULL;
		unsigned char best_addr[6] = {0};
		unsigned char p_addr[6] = {0};
		unsigned int min_ett = 1000000;
		unsigned char best_rate = 0;
		
	
		for(vimor = vimor_list.next; vimor != NULL; vimor = vimor->next){
			if (vimor->hop ==0){
				unsigned int ett = vimor->dist;
				struct vimor_info * parent;
				if ( ett < min_ett ){
						memcpy(best_addr, vimor->addr, ETH_ALEN);
						memcpy(p_addr, vimor->parent_addr, ETH_ALEN);
						best_rate = vimor->rate;
						min_ett = ett;
				}
				for (parent = vimor_list.next; parent != NULL; parent = parent->next){
					if (parent->hop == 1){
						struct tr_info * hop1 = tr_info_find_addr(list, parent->addr);
					 	struct tr_info * hop2;
						if (hop1 == NULL)
							continue;
						hop2 = tr_info_find_addr(&(hop1->nbr_list), vimor->addr);
						if (hop2 != NULL){
							unsigned int temp_ett = parent->dist + find_ett(hop2);
							unsigned char temp_rate = find_mcs(hop2);
							if (temp_ett < min_ett){
								memcpy(best_addr, vimor->addr, ETH_ALEN);
								memcpy(p_addr, parent->addr, ETH_ALEN);
								best_rate = temp_rate;
								min_ett = temp_ett;
							}
						}	
					}		
				}	
			}	
		}
			
		best_info = vimor_info_find_addr(&vimor_list, best_addr);

		if (best_info != NULL){	
			if (!memcmp(src_addr, p_addr, ETH_ALEN))
				best_info->hop = 1;		
			else{
				struct vimor_info * parent = vimor_info_find_addr(&vimor_list, p_addr);
				best_info->dist = min_ett;
				best_info->rate = best_rate;
				memcpy(best_info->parent_addr, p_addr, ETH_ALEN);
				best_info->hop = 2;
				
				if (parent != NULL){
					vimor_info_insert(vimor_info_create(best_info->addr, best_info->rate, 2, best_info->dist, parent->addr), &(parent->child_list));
					parent->hop = 1;
				}
			}
		}
		else{
			printk("Null best info\n");
			break;	
		}
		round++;
		//printk("Round %d\n", round);
		//vimor_info_list_print(&vimor_list);	
	}


	for(vimor = vimor_list.next; vimor != NULL; vimor = vimor->next){
		if (vimor->hop == 1){
			if (vimor->rate < src_rate)
				src_rate = vimor->rate;
			
			if ((vimor->child_list).qlen > 0){
				struct vimor_info * info;
				unsigned char relay_rate = 12;				

				for (info = (vimor->child_list).next; info != NULL; info = info->next){
					if (info->rate < relay_rate)
						relay_rate = info->rate;	
				}

				relay_info_insert(relay_info_create(0, 1, vimor->addr, 10, relay_rate), relay_list);
			}
		}	
	}

	relay_info_insert(relay_info_create(0, 0, src_addr, 10, src_rate), relay_list);
	vimor_info_list_print(&vimor_list);	
	
	if (type == 0) // ec
	{
		unsigned int t1 = cal_tx_time(src_rate, 1, 1316);
		unsigned int t2_tot = 0;
		unsigned int c1 = 0;
		unsigned int c2 = 0;
		unsigned long min_err = 100000000;
		unsigned int c1_opt = 0;
		unsigned int c2_opt = 0;
		struct relay_info * relay;
	
		for (relay = relay_list->next; relay != NULL; relay = relay->next){
			if (relay->type == 1)	
				t2_tot += cal_tx_time(relay->rate, 1, 1316); 			
		}
		c1_max =  (t_slot*1000 - 10*t2_tot)/t1;
		if (c1_max < 10){
			c1 = t_slot*1000 / t1;
			c2 = 0;
			
			for (relay = relay_list->next; relay != NULL; relay = relay->next){
				if (relay->type == 0)
					relay->offset = c1;
				else
					relay->offset = 0;
			}
		}	
		else{
			for (c1 = 10; c1 <= c1_max; c1++){
				unsigned long temp_err = 0;
				c2 = (t_slot*1000 - c1*t1) / t2_tot;
				
				for (relay = relay_list->next; relay != NULL; relay = relay->next){
					if (relay->type == 0)
						relay->offset = c1;
					else
						relay->offset = c2;	
				}
			
				temp_err = calc_error_prob(list, relay_list, &vimor_list);
				
				if (temp_err < min_err){
					c1_opt = c1;
					c2_opt = c2;
					min_err = temp_err;
				}
			}
			for (relay = relay_list->next; relay != NULL; relay = relay->next){
				if (relay->type == 0)
					relay->offset = c1_opt;
				else
					relay->offset = c2_opt;
			}
		}
	} 
	else if (type == 1){ //ET
		unsigned int t1 = cal_tx_time(src_rate, 1, 1316);
		unsigned int t2_max = 0;
		unsigned int c1 = 0;
		unsigned int c2 = 0;
		unsigned long min_err = 100000000;
		unsigned int c1_opt = 0;
		unsigned int n_relay = 0;
		struct relay_info * relay;
	
		for (relay = relay_list->next; relay != NULL; relay = relay->next){
			if (relay->type == 1){	
				unsigned int t2 = cal_tx_time(relay->rate, 1, 1316); 
				if (t2 > t2_max){
					t2_max = t2;
				}
				n_relay++;			
			}
		}
		
		
		c1_max = (t_slot * 1000 - 10*n_relay*t2_max)/t1;
		
		 if (c1_max < 10){
			c1 = t_slot*1000 / t1;
			c2 = 0;
			
			for (relay = relay_list->next; relay != NULL; relay = relay->next){
				if (relay->type == 0)
					relay->offset = c1;
				else
					relay->offset = 0;
			}
		}	
		else{
			unsigned int i = 0;
			unsigned int * c2_opt = NULL;
			
			if (n_relay > 0)
				c2_opt = kmalloc(sizeof(unsigned int)*n_relay, GFP_ATOMIC);	

			for (c1 = 10; c1 <= c1_max; c1++){
				unsigned long temp_err = 0;
				
				for (relay = relay_list->next; relay != NULL; relay = relay->next){
					if (relay->type == 0)
						relay->offset = c1;
					else{
						unsigned int t2 = cal_tx_time(relay->rate, 1, 1316);
						c2 = (t_slot*1000 - c1*t1) / n_relay / t2;
						relay->offset = c2;	
					}
				}
			
				temp_err = calc_error_prob(list, relay_list, &vimor_list);
				
				if (temp_err < min_err){
					i = 0;
					c1_opt = c1;
					
					for (relay = relay_list->next; relay != NULL; relay = relay->next){
						if (relay->type == 1){
							c2_opt[i] = relay->offset;
							i++;
						}
					}

					min_err = temp_err;
				}
			}
		
			i = 0;
			for (relay = relay_list->next; relay != NULL; relay = relay->next){
				if (relay->type == 0)
					relay->offset = c1_opt;
				else{
					relay->offset = c2_opt[i];
					i++;
				}
			}
			
			kfree(c2_opt);
		}		
	}
	//error_avg = calc_error_prob(list, relay_list, &vimor_list, 10, 10, src_rate);		

	vimor_info_list_purge(&vimor_list);	
}
//VIMOR

unsigned long calc_error_prob (struct tr_info_list * t_list, struct relay_info_list * r_list, struct vimor_info_list * v_list){
	unsigned long ret = 0;
	unsigned long scale = 1000000000; // multiplied by 10000
	struct vimor_info * v_info;
	struct relay_info * src_info = NULL;


	for (src_info = r_list->next; src_info != NULL; src_info = src_info->next){
		if (src_info->type == 0)
			break;
	}

	if (src_info == NULL)
		return 0;

	for (v_info = v_list->next; v_info != NULL; v_info = v_info->next){
		unsigned long e_d = scale;
		struct relay_info * relay;
	
		if (v_info->hop == 1){	
			for (relay = r_list->next; relay != NULL; relay = relay->next){
				if (relay->type == 0){
					struct tr_info * hop1 = tr_info_find_addr(t_list, v_info->addr);

					if (hop1 == NULL){
						printk("Never occur\n");
						continue;
					}

					else{
						unsigned int tot1 = 0;			
						unsigned int err1 = 0;
						unsigned int i = 0;	
						unsigned long hop1_err = scale;

						tot1 = (unsigned int)hop1->total_num;
						err1 = tot1 - (unsigned int)hop1->rcv_num[src_info->rate];

						for (i=0; i < src_info->offset; i++){
							hop1_err = hop1_err * (unsigned long)err1 / (unsigned long)tot1;		
						}

						e_d = e_d * hop1_err / scale; 
					}				
				}
			}
		}

		else{	
			for (relay = r_list->next; relay != NULL; relay = relay->next){
				if (relay->type == 0){
					struct tr_info * hop1 = tr_info_find_addr(t_list, v_info->addr);

					if (hop1 == NULL)
						continue;

					else{
						unsigned int tot1 = 0;			
						unsigned int err1 = 0;
						unsigned int i = 0;	
						unsigned long hop1_err = scale;

						tot1 = (unsigned int)hop1->total_num;
						err1 = tot1 - (unsigned int)hop1->rcv_num[src_info->rate];

						for (i=0; i < src_info->offset; i++){
							hop1_err = hop1_err * (unsigned long)err1 / (unsigned long)tot1;		
						}

						e_d = e_d * hop1_err / scale; 
					}				
				}

				else{
					struct tr_info * hop1 = tr_info_find_addr(t_list, relay->addr);
					struct tr_info * hop2;

					if(hop1 == NULL){
						printk("Never occur!!\n");
						continue;	
					}			

					hop2 = tr_info_find_addr(&(hop1->nbr_list), v_info->addr); 

					if (hop2 != NULL){
						unsigned int tot1 = 0;			
						unsigned int tot2 = 0;			
						unsigned int err1 = 0;
						unsigned int err2 = 0;
						unsigned int i = 0;	
						unsigned long hop1_err = scale;
						unsigned long hop2_err = scale;		

						tot1 = (unsigned int)hop1->total_num;
						err1 = tot1 - (unsigned int)hop1->rcv_num[src_info->rate];

						tot2 = (unsigned int)hop2->total_num;
						err2 = tot2 - (unsigned int)hop2->rcv_num[relay->rate];

						for (i=0; i < src_info->offset; i++){
							hop1_err = hop1_err * (unsigned long)err1 / (unsigned long)tot1;		
						}

						for (i=0; i < relay->offset; i++){
							hop2_err = hop2_err * (unsigned long)err2 / (unsigned long)tot2;		
						}	

						e_d = e_d * (scale - (scale - hop1_err)*(scale - hop2_err)/scale) / scale ;		
					}	// hop2 is not NULL
				} //else
			} // for relay
		} // vimor type == 2 
		//printk("Node %x:%x:%x:%x:%x:%x E_d: %ld\n", v_info->addr[0], v_info->addr[1], v_info->addr[2], v_info->addr[3], v_info->addr[4], v_info->addr[5], e_d);
		
		ret += e_d;
	} // for vimor
//	ret = ret / v_list->qlen;	
	
	printk("Average Error Prob with (c1:%d): %ld\n", src_info->offset, ret);
	
	return ret;
}
