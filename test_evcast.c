#include<linux/module.h>
#include<linux/kernel.h>

#include "tl_rx.h"

int init_module(void)
{
	unsigned char i=0;
	unsigned char total_n = 20;
	unsigned char n_hop1 = 5;
	unsigned char max_hop2 = 5;
	unsigned char batt_src = 0;
	unsigned char b_v = 0;

	struct tr_info_list total_list;
	struct tr_info_list src_list;
	struct relay_info_list relay;
	struct dst_info_list dst;
	struct tr_info * info;

	tr_info_list_init(&total_list);
	tr_info_list_init(&src_list);
	relay_info_list_init(&relay);
	dst_info_list_init(&dst);

	get_random_bytes(&b_v, 1);
	batt_src = set_batt(b_v%2, (b_v%100)+1);
	
	printk(KERN_INFO "test_module() called\n");
	
	for (i=0; i < total_n; i++){
		unsigned char addr[6] = {0};
		unsigned char batt = 0;
		char rssi = 0;
		unsigned int n_rcv[NUM_MCS]={0};
		unsigned int num = 100;
		struct net_device * dev = NULL;
		unsigned char capa = 0;
		unsigned char charge = 0;
		int m = 0;
		unsigned char v = 0;
		

		get_random_bytes(&v, 1);
		rssi = -1*(v%100+1);
		get_random_bytes(&addr, 6);
		capa = (v%100)+1;
		charge = v%2;
		batt = set_batt(charge, capa);

		for (m=NUM_MCS-1; m >= 0; m--){
			unsigned int temp_rcv = 0;
			get_random_bytes(&temp_rcv, 4);
			temp_rcv %= (num - 80);
			temp_rcv++;		
	
			if (m == NUM_MCS-1)
				n_rcv[m] = temp_rcv;
				
			else{
				while(temp_rcv < n_rcv[m+1]){
					get_random_bytes(&temp_rcv, 4);
					temp_rcv %= (num - 20);
					temp_rcv++;		
				}	
				n_rcv[m] = temp_rcv;	
			}
		}
		 	
		tr_info_insert(tr_info_create(addr, dev, num, n_rcv, rssi, batt), &total_list);
	}

	info = total_list.next;

	for (i=0; i < n_hop1; i++){
		unsigned char n_hop2 = 0;
		unsigned char temp = 0;
		struct tr_info * hop1;
		struct tr_info_list *nbr_2hop_list;
		unsigned char j=0;

		hop1 = 	tr_info_create(info->addr, info->dev, info->total_num, info->rcv_num, info->rssi, info->batt);
		tr_info_insert(hop1, &src_list);
		tr_info_list_init(&(hop1->nbr_list));
		nbr_2hop_list = &(hop1->nbr_list);

		get_random_bytes(&temp, 1);
		n_hop2 = (temp % max_hop2)+1;			

		for (j = 0; j < n_hop2; j++){
			unsigned char k = 0;
			unsigned char move = 0;
			struct tr_info * hop2;
		
			get_random_bytes(&temp, 1);
			move = temp % total_n;
			hop2 = total_list.next;
			
			for (k=0; k < move; k++){
				hop2 = hop2->next;			
			}
			
			if (!memcmp(hop2->addr, hop1->addr, ETH_ALEN) || tr_info_find_addr(nbr_2hop_list, hop2->addr)!=NULL ){
				printk(KERN_INFO "same addr\n");
			}
			else{
			char rssi = 0;
			unsigned int n_rcv[NUM_MCS]={0};
			int m = 0;
			unsigned char v = 0;

			get_random_bytes(&v, 1);
			rssi = -1*(v%100+1);
			
	
			for (m=NUM_MCS-1; m >= 0; m--){
				unsigned int temp_rcv = 0;
				get_random_bytes(&temp_rcv, 4);
				temp_rcv %= (hop2->total_num - 80);
				temp_rcv++;		
				
				if (m == NUM_MCS-1)
					n_rcv[m] = temp_rcv;
		
				else{
					while(temp_rcv < n_rcv[m+1]){
						get_random_bytes(&temp_rcv, 4);
						temp_rcv %= (hop2->total_num - 20);
						temp_rcv++;		
					}	
					n_rcv[m] = temp_rcv;	
				}
			}

			tr_info_insert(tr_info_create(hop2->addr, hop2->dev, hop2->total_num, n_rcv, rssi, hop2->batt), nbr_2hop_list);
			}
		}
		
		info = info->next;
	}

	printk(KERN_INFO "Print lists\n");		
	tr_info_list_print(&total_list);
	tr_info_list_print(&src_list);

		
	printk(KERN_INFO "Start alg\n");
	vimor_relay(&src_list, &relay, &dst, 1);	
	
	printk(KERN_INFO "\nAlgorithm Result\n");
	relay_info_list_print(&relay);	
	dst_info_list_print(&dst);	
	relay_info_list_purge(&relay);	
	dst_info_list_purge(&dst);	
	
	tr_info_list_purge(&total_list);
	tr_info_list_purge(&src_list);

		
	return 0;
}

void cleanup_module(void){
	printk(KERN_INFO "cleanup_module() called\n");
}
