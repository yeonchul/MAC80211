#include<linux/module.h>
#include<linux/kernel.h>

#include "online_table.h"
#include "tl_rx.h"

int init_module(void)
{
	//unsigned char i=0;
	unsigned char mcs = 0;
	unsigned int seq = 0;
	
	init_table();

	for (mcs = 0; mcs < NUM_MCS; mcs++){
		unsigned int id = 0;
		get_random_bytes(&id, 4);
		
		for (seq = 0; seq < 100; seq++){
			unsigned char succ = 0;
			get_random_bytes(&succ, 1);
			succ = succ%100+1;
			
			if (succ > 20){
				int rssi = 0;
				unsigned char v = 0;
				bool start_nc = false;
				unsigned int nc_n = 0;			
				
				get_random_bytes(&v, 1);
					
				rssi = -1*(v%30+50);
				update_table(seq, id, mcs, rssi, start_nc, nc_n);
			}
		}
	}
	
	for (mcs = 0; mcs < NUM_MCS; mcs++){
		filling_blank(mcs);
	}

	print_pdr_table();
	print_res_table();
	monotonicity();
	printk("------- After Monotonicity-------\n");
	print_res_table();
	
	return 0;
}	


void cleanup_module(void){
	printk(KERN_INFO "cleanup_module() called\n");
}
