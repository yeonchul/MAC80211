////Modified_130619_GJLEE

#include "tl_rx.h"
#define TIME_LIMIT 10000


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
	unsigned long result = (unsigned long) n * (unsigned long) rcv * (unsigned long) rcv * 80 * 1000000;
	result /= total;
	result /= total;
	result /= 100;
	result /= 1000000;
	printk("n_to_k result = %ld, n = %d, total = %d, rcv = %d\n", result, n, total, rcv);
	return (unsigned char) result;
}

unsigned char k_to_n(unsigned char k, unsigned int total, unsigned int rcv){
	unsigned long result = (unsigned long) k * (unsigned long) total * (unsigned long) total * 100 * 1000000;
	result /= rcv;
	result /= rcv;
	result /= 80;
	if((result % 1000000) != 0) result += 1000000;
	result /= 1000000;
	printk("k_to_n result = %ld, k = %d, total = %d, rcv = %d\n", result, k, total, rcv);
	return (unsigned char) (result < 255 ? result : 255);
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


unsigned char find_best_mcs(struct tr_info * info){
	unsigned char i = 0;
	unsigned char temp_value = 0;
	unsigned char ret = 0;
		
	for(i=0; i<NUM_MCS; i++){
		unsigned int time = cal_tx_time(i, 1, 1316);
		unsigned int rcv = info->rcv_num[i];

		unsigned int value = rcv*65536/time;
		if (value > temp_value)
		{
			temp_value = value;
			ret = i;
		}
	}
	return ret;
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

void evcast_relay(struct tr_info_list *list, unsigned char src_addr[], unsigned char src_batt){
	struct dst_info_list dst_list;
	struct relay_info_list relay_list;
	unsigned int max_time_us = 10000;	
	unsigned int used_time_us = 0;
	unsigned int round = 0;
	unsigned int pkt_len = 1400;
	unsigned char omega = 2;

	//struct tr_info *test_info;
	struct tr_info *info;
	struct dst_info *d_info;

	dst_info_list_init(&dst_list);
	relay_info_list_init(&relay_list);
	
	printk("Start EV-CAST Algorithm\n");
	
	for(info = list->next; info != NULL; info = info->next){
		struct tr_info *nbr_info;
		struct tr_info_list * nbr_info_list = &(info->nbr_list);
		
		struct tr_info * info2;
			
		if(dst_info_find_addr(&dst_list, info->addr)==NULL)
		{
			unsigned char pr_dof = tr_get_data_k();
			dst_info_insert(dst_info_create(info->addr, pr_dof), &dst_list);
		}	
		for(nbr_info = nbr_info_list->next; nbr_info != NULL; nbr_info = nbr_info->next){
			if(dst_info_find_addr(&dst_list, nbr_info->addr) == NULL){
				unsigned char pr_dof = tr_get_data_k();
				dst_info_insert(dst_info_create(nbr_info->addr, pr_dof), &dst_list);
			}
		}
		
		for (info2 = list->next; info2!=NULL; info2 = info2->next){
			bool is_srp = false;
			if (info == info2)
				continue;
			if (tr_info_find_addr(nbr_info_list,info2->addr)!=NULL)
				continue;
 			if (tr_info_find_addr(&(info2->nbr_list), info->addr)!=NULL)
				continue;
			
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
				tr_info_insert(tr_info_create(info2->addr, info2->dev, info2->total_num, info2->rcv_num, info2->rssi, info2->batt), &(info->srp_list));
			}
		}
	}
	
	printk("Initialization of Destination List Complete, Num of Dest: %d\n", dst_list.qlen);

	while(!dst_all_zero(&dst_list) && (used_time_us < max_time_us)){
		struct relay_info relay;
		//struct dst_info* dst;
		struct tr_info * hop1;
	
		relay.utility = 0;
		relay.round= 0;	
		memset(relay.addr1, 0, ETH_ALEN);
		relay.rate1 = 0;
		relay.clout1= 0;
		memset(relay.addr2, 0, ETH_ALEN);
		relay.rate2 = 0;
		relay.clout2 = 0;
		memset(relay.addr3, 0, ETH_ALEN);
		relay.rate3 = 0;
		relay.clout3 = 0;
		relay.next = NULL;
		relay.prev = NULL;
		relay.head = NULL;
		
		round++;
		if (round > 10){
			printk("Too high round\n");
			return;
		}
		
		printk("===== Round %d =====\n", round);
		
		// 1-hop only
		
		for(hop1 = list->next; hop1 != NULL; hop1 = hop1->next){
			struct dst_info_list temp_dst_list;
			unsigned char pr_dof = get_pr_dof(&dst_list, hop1->addr);
			struct tr_info * hop2;
			struct tr_info * srp;
			unsigned char user = 0;
			unsigned int utility = 0;
			unsigned int total_cost = 0;
			unsigned char rate1 = 0;
			unsigned char clout1 = 0;
			
			dst_info_list_init(&temp_dst_list);	

			if (pr_dof > 0){
					struct tr_info * other1;
					struct relay_info * r_info;
					unsigned char remain_c = 0;	
					unsigned int time_cost = 0;
					unsigned char charge = 0;
					unsigned char capacity = 0;
					
					rate1 = find_best_mcs(hop1); 				
					clout1 = k_to_n(pr_dof, hop1->total_num, hop1->rcv_num[rate1]);
					remain_c = clout1;
					user++;
					dst_info_insert(dst_info_create(hop1->addr, 0), &temp_dst_list);	
					
					for(other1 = list->next; other1 != NULL; other1 = other1->next){
						if (hop1 != other1){
							unsigned char pr_dof2 = get_pr_dof(&dst_list, other1->addr);
							if (pr_dof2 == 0)
								continue;
							
							if (n_to_k(clout1, other1->total_num, other1->rcv_num[rate1]) > pr_dof2){
								dst_info_insert(dst_info_create(other1->addr, 0), &temp_dst_list);
								user++;
							}
						}	
					}
					for (r_info = relay_list.next; r_info != NULL; r_info = r_info->next){
						if (r_info->clout1 > 0 && rate1 <= r_info->rate1){
							if (r_info->clout1 >= remain_c){
								time_cost += cal_tx_time(rate1, remain_c, pkt_len)-cal_tx_time(r_info->rate1, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time_cost += cal_tx_time(rate1, r_info->clout1, pkt_len)-cal_tx_time(r_info->rate1, r_info->clout1, pkt_len);
								remain_c = remain_c - r_info->clout1;
							}
						}
					}

					time_cost+=cal_tx_time(rate1, remain_c, pkt_len);
					charge = get_charge(src_batt);
					capacity = get_capa(src_batt);			
					
					if (charge == 1){
						total_cost = time_cost*100/(omega*capacity); 
					}
					else{
						total_cost = time_cost*100/(capacity); 
					}

					utility = user*10000/total_cost;

					if (utility >= relay.utility){
						relay.utility = utility;
						relay.round = round;
						relay.rate1 = rate1;
						relay.clout1 = clout1;
						relay.rate2 = 0;
						relay.clout2 = 0;
						relay.rate3 = 0;
						relay.clout3 = 0; 
					}
			} // if the 1hop node is unserved
		
			for(hop2 = (hop1->nbr_list).next; hop2 != NULL; hop2 = hop2->next){
				unsigned char pr_dof2 = get_pr_dof(&dst_list, hop2->addr);

				if (pr_dof2 > 0 &&  dst_info_find_addr(&temp_dst_list, hop2->addr)==NULL){
					struct tr_info * other2;
					unsigned char rate2 = find_best_mcs(hop2); 				
					unsigned char clout2 = k_to_n(pr_dof2, hop2->total_num, hop2->rcv_num[rate2]);
					unsigned char remain_c = clout2;	
					unsigned int time_cost2 = 0;
					unsigned char charge = 0;
					unsigned char capacity = 0;
					unsigned char user2 = 0;
					struct relay_info * r_info;
					
					user2++;	
					dst_info_insert(dst_info_create(hop2->addr, 0), &temp_dst_list);	
					
					for(other2 = (hop1->nbr_list).next; other2 != NULL; other2 = other2->next){
						if (hop2 != other2){
							unsigned char pr_dof_other2 = get_pr_dof(&dst_list, other2->addr);
							if (pr_dof2 == 0)
								continue;
							
							if (n_to_k(clout2, other2->total_num, other2->rcv_num[rate2]) > pr_dof_other2){
								if (dst_info_find_addr(&temp_dst_list, other2->addr)==NULL){
									dst_info_insert(dst_info_create(other2->addr, 0), &temp_dst_list);	
									user2++;	
								}
							}
						}	
					}
					for (r_info = relay_list.next; r_info != NULL; r_info = r_info->next){
						if (!memcmp(r_info->addr2, hop1->addr, ETH_ALEN) && r_info->clout2 > 0 && rate2 <= r_info->rate2){
							if (r_info->clout2 >= remain_c){
								time_cost2 += cal_tx_time(rate2, remain_c, pkt_len)-cal_tx_time(r_info->rate2, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time_cost2 += cal_tx_time(rate2, r_info->clout2, pkt_len)-cal_tx_time(r_info->rate2, r_info->clout2, pkt_len);
								remain_c = remain_c - r_info->clout2;
							}
						}
						else if (!memcmp(r_info->addr3, hop1->addr, ETH_ALEN) && r_info->clout3 > 0 && rate2 <= r_info->rate3){
							if (r_info->clout3 >= remain_c){
								time_cost2 += cal_tx_time(rate2, remain_c, pkt_len)-cal_tx_time(r_info->rate3, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time_cost2 += cal_tx_time(rate2, r_info->clout3, pkt_len)-cal_tx_time(r_info->rate3, r_info->clout3, pkt_len);
								remain_c = remain_c - r_info->clout3;
							}
						}
					}

					time_cost2+=cal_tx_time(rate2, remain_c, pkt_len);
					charge = get_charge(hop2->batt);
					capacity = get_capa(hop2->batt);			
					
					if (charge == 1){
						total_cost += time_cost2*100 / (omega*capacity);
					}
					else{
						total_cost += time_cost2*100 / capacity;	
					}
					utility = (user+user2)*10000 / total_cost;					

					if (utility >= relay.utility){
						relay.utility = utility;
						relay.round = round;
						relay.rate1 = rate1;
						relay.clout1 = clout1;
						memcpy(relay.addr2, hop2->addr, ETH_ALEN);  
						relay.rate2 = rate2;
						relay.clout2 = clout2;
						relay.rate3 = 0;
						relay.clout3 = 0; 
					}		
				}//pr_dof	
			}//for 2hop nodes of a chosen 1hop node
			
			
			total_cost = 0;
			
			for (srp = (hop1->srp_list).next; srp != NULL; srp = srp->next){
				struct dst_info_list temp_dst_list2;
				unsigned char pr_dof_srp = get_pr_dof(&dst_list, srp->addr);
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
				struct relay_info * r_info;
				 
				if (pr_dof_srp > 0 || pr_dof > 0){
					unsigned char rate11 = 0;
					unsigned char rate12 = 0;
					unsigned char clout11 = 0;
					unsigned char clout12 = 0;
					unsigned char remain_c = 0;
					unsigned char charge = 0;
					unsigned char capacity = 0;
					struct tr_info * other1;
					
					rate11 = find_best_mcs(hop1); 				
					rate12 = find_best_mcs(srp);
					rate1 = rate11 <= rate12 ? rate11 : rate12;
					  				
					clout11 = k_to_n(pr_dof, hop1->total_num, hop1->rcv_num[rate1]);
					clout12 = k_to_n(pr_dof_srp, srp->total_num, srp->rcv_num[rate1]);
					clout1 = clout11 >= clout12 ? clout11 : clout12;
					
					remain_c = clout1;
					
					//user1 value is initialized as 0 since for-statement below does not make an excpetion for the target nodes (1-hop or srp)
					for(other1 = list->next; other1 != NULL; other1 = other1->next){
							unsigned char pr_dof2 = get_pr_dof(&dst_list, other1->addr);
							if (pr_dof2 == 0)
								continue;
							
							if (n_to_k(clout1, other1->total_num, other1->rcv_num[rate1]) > pr_dof2){
								dst_info_insert(dst_info_create(other1->addr, 0), &temp_dst_list);
								user1++;
							}
					}

					for (r_info = relay_list.next; r_info != NULL; r_info = r_info->next){
						if (r_info->clout1 > 0 && rate1 <= r_info->rate1){
							if (r_info->clout1 >= remain_c){
								time_cost += cal_tx_time(rate1, remain_c, pkt_len)-cal_tx_time(r_info->rate1, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time_cost += cal_tx_time(rate1, r_info->clout1, pkt_len)-cal_tx_time(r_info->rate1, r_info->clout1, pkt_len);
								remain_c = remain_c - r_info->clout1;
							}
						}
					}

					time_cost+=cal_tx_time(rate1, remain_c, pkt_len);
					charge = get_charge(src_batt);
					capacity = get_capa(src_batt);			

					if (charge == 1){
						total_cost = time_cost*100/(omega*capacity); 
					}
					else{
						total_cost = time_cost*100/(capacity); 
					}
				}//if either 1-hop or the chosen srp is unserved
		
				for(hop2 = (hop1->nbr_list).next; hop2 != NULL; hop2 = hop2->next){
					unsigned char pr_dof2 = 0;  
					unsigned char charge = 0;
					unsigned char capacity = 0;
					unsigned char remain_c = 0;	
					unsigned char dof_by_src = 0;
					struct tr_info * info_from_src;
					struct tr_info * other2;
					
					info_from_src =	tr_info_find_addr(list, hop2->addr);
					if (list != NULL)
						dof_by_src = n_to_k(clout1, info_from_src->total_num, info_from_src->rcv_num[rate1]);
					else
						dof_by_src = 0;	
					
					pr_dof2 = get_pr_dof(&dst_list, hop2->addr) > dof_by_src ? get_pr_dof(&dst_list, hop2->addr) - dof_by_src : 0;

					// if the pr_dof of that node is 0 or 
					if (!(pr_dof2 > 0 &&  dst_info_find_addr(&temp_dst_list, hop2->addr)==NULL))
						continue;
				
					user2 = 1;	
					rate2 = find_best_mcs(hop2); 				
					clout2 = k_to_n(pr_dof2, hop2->total_num, hop2->rcv_num[rate2]);
					remain_c = clout2;	

					dst_info_insert(dst_info_create(hop2->addr, 0), &temp_dst_list);	
					
					for(other2 = (hop1->nbr_list).next; other2 != NULL; other2 = other2->next){
						if (hop2 != other2){
							unsigned char pr_dof_other2 = get_pr_dof(&dst_list, other2->addr);
							info_from_src =	tr_info_find_addr(list, other2->addr);
							if (list != NULL)
								dof_by_src = n_to_k(clout1, info_from_src->total_num, info_from_src->rcv_num[rate1]);	
							else
								dof_by_src = 0;

							pr_dof_other2 = pr_dof_other2 > dof_by_src ? pr_dof_other2 - dof_by_src : 0;							
	
							if (pr_dof_other2 == 0)
								continue;
							
							if (n_to_k(clout2, other2->total_num, other2->rcv_num[rate2]) > pr_dof_other2){
								if (dst_info_find_addr(&temp_dst_list, other2->addr)==NULL){
									dst_info_insert(dst_info_create(other2->addr, 0), &temp_dst_list);	
									user2++;	
								}
							}
						}	
					}
					for (r_info = relay_list.next; r_info != NULL; r_info = r_info->next){
						if (!memcmp(r_info->addr2, hop1->addr, ETH_ALEN) && r_info->clout2 > 0 && rate2 <= r_info->rate2){
							if (r_info->clout2 >= remain_c){
								time2 += cal_tx_time(rate2, remain_c, pkt_len)-cal_tx_time(r_info->rate2, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time2 += cal_tx_time(rate2, r_info->clout2, pkt_len)-cal_tx_time(r_info->rate2, r_info->clout2, pkt_len);
								remain_c = remain_c - r_info->clout2;
							}
						}
						else if (!memcmp(r_info->addr3, hop1->addr, ETH_ALEN) && r_info->clout3 > 0 && rate2 <= r_info->rate3){
							if (r_info->clout3 >= remain_c){
								time2 += cal_tx_time(rate2, remain_c, pkt_len)-cal_tx_time(r_info->rate3, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time2 += cal_tx_time(rate2, r_info->clout3, pkt_len)-cal_tx_time(r_info->rate3, r_info->clout3, pkt_len);
								remain_c = remain_c - r_info->clout3;
							}
						}
					}
					
					time2+=cal_tx_time(rate2, remain_c, pkt_len);
					charge = get_charge(hop1->batt);
					capacity = get_capa(hop1->batt);			
					
					if (charge == 1){
						total_cost += time2*100 / (omega*capacity);
					}
					else{
						total_cost += time2*100 / capacity;	
					}
				// the chosen 2-hop node of the target 1-hop node
				
				for(hop2 = (srp->nbr_list).next; hop2 != NULL; hop2 = hop2->next){
					struct tr_info * other2;
					unsigned char remain_c = 0;
					unsigned char pr_dof2 = 0;	
					unsigned char dof_by_src = 0;
					struct tr_info * info_from_src;

					
					info_from_src =	tr_info_find_addr(list, hop2->addr);
					if (list != NULL)
						dof_by_src = n_to_k(clout1, info_from_src->total_num, info_from_src->rcv_num[rate1]);
					else
						dof_by_src = 0;	
					
					pr_dof2 = get_pr_dof(&dst_list, hop2->addr) > dof_by_src ? get_pr_dof(&dst_list, hop2->addr) - dof_by_src : 0;

					// if the pr_dof of that node is 0 or dst_info is found
					if (!(pr_dof2 > 0 &&  dst_info_find_addr(&temp_dst_list, hop2->addr)==NULL))
						continue;
					
					user3 = 1;
					rate3 = find_best_mcs(hop2); 				
					clout3 = k_to_n(pr_dof2, hop2->total_num, hop2->rcv_num[rate3]);
					remain_c = clout3;	

					dst_info_insert(dst_info_create(hop2->addr, 0), &temp_dst_list);	
					
					for(other2 = (srp->nbr_list).next; other2 != NULL; other2 = other2->next){
						if (hop2 != other2){
							unsigned char pr_dof_other2 = get_pr_dof(&dst_list, other2->addr);
							info_from_src =	tr_info_find_addr(list, other2->addr);
							if (list != NULL)
								dof_by_src = n_to_k(clout1, info_from_src->total_num, info_from_src->rcv_num[rate1]);	
							else
								dof_by_src = 0;

							pr_dof_other2 = pr_dof_other2 > dof_by_src ? pr_dof_other2 - dof_by_src : 0;							
	
							if (pr_dof_other2 == 0)
								continue;
							
							if (n_to_k(clout2, other2->total_num, other2->rcv_num[rate3]) > pr_dof_other2){
								if (dst_info_find_addr(&temp_dst_list, other2->addr)==NULL){
									dst_info_insert(dst_info_create(other2->addr, 0), &temp_dst_list);	
									user3++;	
								}
							}
						}	
					}
					for (r_info = relay_list.next; r_info != NULL; r_info = r_info->next){
						if (!memcmp(r_info->addr2, hop1->addr, ETH_ALEN) && r_info->clout2 > 0 && rate3 <= r_info->rate2){
							if (r_info->clout2 >= remain_c){
								time3 += cal_tx_time(rate3, remain_c, pkt_len)-cal_tx_time(r_info->rate2, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time3 += cal_tx_time(rate3, r_info->clout2, pkt_len)-cal_tx_time(r_info->rate2, r_info->clout2, pkt_len);
								remain_c = remain_c - r_info->clout2;
							}
						}
						else if (!memcmp(r_info->addr3, hop1->addr, ETH_ALEN) && r_info->clout3 > 0 && rate3 <= r_info->rate3){
							if (r_info->clout3 >= remain_c){
								time3 += cal_tx_time(rate3, remain_c, pkt_len)-cal_tx_time(r_info->rate3, remain_c, pkt_len);
								remain_c = 0;
							}
							else{
								time3 += cal_tx_time(rate3, r_info->clout3, pkt_len)-cal_tx_time(r_info->rate3, r_info->clout3, pkt_len);
								remain_c = remain_c - r_info->clout3;
							}
						}
					}

					time3 += cal_tx_time(rate3, remain_c, pkt_len);
					charge = get_charge(srp->batt);
					capacity = get_capa(srp->batt);			

					if (charge == 1){
						total_cost += time3*100 / (omega*capacity);
					}
					else{
						total_cost += time3*100 / capacity;	
					}

					utility = (user1+user2+user3)*10000 / total_cost;					
						if (utility >= relay.utility){
							relay.utility = utility;
							relay.round = round;
							relay.rate1 = rate1;
							relay.clout1 = clout1;
							memcpy(relay.addr2, hop2->addr, ETH_ALEN);  
							relay.rate2 = rate2;
							relay.clout2 = clout2;
							memcpy(relay.addr3, srp->addr, ETH_ALEN);  
							relay.rate3 = rate3;
							relay.clout3 = clout3; 
						}	
					}// the chosen 2-hop node by the srp of the target node	
				}// the chosen  chosen 2-hop node of the target 1-hop node 
			}// with the selected srp
		}//hop1 node
		printk("Best relay selection for round %d\n", round);	
	}//end while all zero
}

void tl_select_relay(struct tr_info_list *list){
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
	if(MNPRELAY) mnp_relay(list);
	else min_max_relay(list);
}

void dst_info_list_init(struct dst_info_list *list){
	list->next = NULL;
	list->qlen = 0;
}

struct dst_info *dst_info_create(unsigned char addr[], unsigned char pr_dof){
	struct dst_info *info;
	info = kmalloc(sizeof(struct dst_info), GFP_ATOMIC);
	memcpy(info->addr, addr, ETH_ALEN);
	info->pr_dof = pr_dof;
	info->round = 0;
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

void tr_info_list_print(struct tr_info_list *list){
	struct tr_info *test_info = list->next;
	printk("-------------------------src_nbr_list status---------------------------\n");
	while(test_info != NULL){
		struct tr_info *test_src_nbr_info = (&(test_info->nbr_list))->next;
		printk(KERN_INFO "1hop %x:%x:%x:%x:%x:%x, rssi %d, batt %x, (%d %d %d %d %d %d %d %d %d %d %d %d)/%d, %d, %d\n", test_info->addr[0], test_info->addr[1], test_info->addr[2], test_info->addr[3], test_info->addr[4], test_info->addr[5], test_info->rssi, test_info->batt, test_info->rcv_num[0], test_info->rcv_num[1], test_info->rcv_num[2], test_info->rcv_num[3], test_info->rcv_num[4], test_info->rcv_num[5], test_info->rcv_num[6], test_info->rcv_num[7],  test_info->rcv_num[8], test_info->rcv_num[9], test_info->rcv_num[10], test_info->rcv_num[11], test_info->total_num, test_info->tf_cnt, test_info->nr_cnt);
		//printk(KERN_INFO "1-hop addr: %x:%x:%x:%x:%x:%x, total_num = %d, rcv_num = %d, tf_cnt = %d, nr_cnt = %d\n", test_info->addr[0], test_info->addr[1], test_info->addr[2], test_info->addr[3], test_info->addr[4], test_info->addr[5], test_info->total_num, test_info->rcv_num, test_info->tf_cnt, test_info->nr_cnt);
		while(test_src_nbr_info != NULL){
			printk(KERN_INFO "---> %x:%x:%x:%x:%x:%x, rssi %d, batt %x, (%d %d %d %d %d %d %d %d %d %d %d %d)/%d, %d, %d\n", test_src_nbr_info->addr[0], test_src_nbr_info->addr[1], test_src_nbr_info->addr[2], test_src_nbr_info->addr[3], test_src_nbr_info->addr[4], test_src_nbr_info->addr[5], test_src_nbr_info->rssi, test_src_nbr_info->batt, test_src_nbr_info->rcv_num[0], test_src_nbr_info->rcv_num[1], test_src_nbr_info->rcv_num[2], test_src_nbr_info->rcv_num[3], test_src_nbr_info->rcv_num[4], test_src_nbr_info->rcv_num[5], test_src_nbr_info->rcv_num[6], test_src_nbr_info->rcv_num[7], test_src_nbr_info->rcv_num[8], test_src_nbr_info->rcv_num[9], test_src_nbr_info->rcv_num[10], test_src_nbr_info->rcv_num[11], test_src_nbr_info->total_num, test_src_nbr_info->tf_cnt, test_src_nbr_info->nr_cnt);
			test_src_nbr_info = test_src_nbr_info->next;
		}
		test_info = test_info->next;
	}
	printk("-----------------------------------------------------------------------\n");

}


struct relay_info *relay_info_create(void){
	struct relay_info *info;
	info = kmalloc(sizeof(struct relay_info), GFP_ATOMIC);
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
