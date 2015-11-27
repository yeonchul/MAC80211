#include "online_table.h"
#include "tl_rx.h"
/*
X_RSSI 50
  7 #define SCALE 1000 // a time SCALE, e.g., 0.1 = 100 (/1000)
  8 #define ALPHA 9 // 0.9
  9 #define THRE_HIGH 950  // IF SCALE changes, it should be also changed
 10 #define THRE_LOW 50    // It is the same 
 11 #define NUM_SAMPLE 20
 12 #define RSSI_TRANSITION 4
*/
#define YCSHIN 0

#undef YDEBUG
#ifdef YCSHIN
	#define YDEBUG(fmt, args...) printk(KERN_INFO " " fmt, ## args)
#else
#	define YDEBUG(fmt, args...)
#endif

void init_table(){
	unsigned char i = 0;
	for (i = 0; i < NUM_MCS; i++){
		unsigned int j = 0;
		m_info[i].l_exist = false;
		m_info[i].m_exist = false;
		m_info[i].h_exist = false;
		m_info[i].l_idx = 0;
		m_info[i].h_idx = 0;
		for (j = 0; j < NUM_RSSI; j++){
			pdr_table[i][j] = 0;
		}
	}	
	init = false;	
}
void update_table(unsigned int seq, unsigned int id, unsigned char mcs, int rssi, bool start_nc, unsigned int nc_n){
	static int rssi_cur = 0;
	static unsigned char mcs_cur = 0;
	static unsigned int first_seq = 0;
	static unsigned int id_cur = 0;
	static unsigned int rcv_cur = 0;
	static unsigned int tot_cur = 0;	

	if (init == false){
		rssi_cur = rssi;
		mcs_cur = mcs;
		first_seq = 0;
		id_cur = id;
		rcv_cur = 1;
		tot_cur = seq;
		init = true;	
		
		return;
	}	
	if (start_nc == true){
		if (id != id_cur || mcs != mcs_cur){
					record_sample(mcs_cur, rssi_cur, rcv_cur, nc_n);
					filling_blank(mcs_cur);
					monotonicity();
			
			mcs_cur = mcs;
			rssi_cur = rssi;
			first_seq = 0;
			rcv_cur = 1;
			id_cur = id;
		}
		else{
			rssi_cur += rssi;
			rcv_cur++;
			tot_cur = nc_n;
		}		

		return;	
	}
	if (mcs != mcs_cur || id != id_cur ){
		if (mcs != mcs_cur){
			printk("New mcs is comming %d\n", (unsigned int)mcs);
			tot_cur = tr_get_tf_k() - first_seq; 
		}
		else {
			printk("New id comming %d\n", id);
		}

		if (tot_cur > MIN_SAMPLE)
			record_sample(mcs_cur, rssi_cur, rcv_cur, tot_cur);

		mcs_cur = mcs;
		rssi_cur = rssi;
		first_seq = 0;
		rcv_cur = 1;
		tot_cur = seq;
		id_cur = id;

		return;
	}
	else{
		if (seq - first_seq >= NUM_SAMPLE){
				if (tot_cur > MIN_SAMPLE)
					record_sample(mcs_cur, rssi_cur, rcv_cur, NUM_SAMPLE);
				
				rssi_cur = rssi;
				tot_cur = seq - first_seq - NUM_SAMPLE + 1;
				rcv_cur = 1;
				first_seq = seq - tot_cur + 1;
		}	
		else{
			rssi_cur += rssi;	
			rcv_cur++;
			tot_cur = seq - first_seq + 1;
		}
	}
}
void record_sample(unsigned char mcs, int rssi_sum, unsigned int rcv, unsigned int tot){
	int rssi = (2*rssi_sum)/rcv - rssi_sum/rcv; //round half up
	unsigned int pdr = rcv*SCALE/tot;
	unsigned int rssi_idx = 0;
	unsigned int pdr_cur = 0;
	//unsigned int final_rssi = 0;

	if (rssi < RSSI_MIN || (rssi - RSSI_MIN) >= NUM_RSSI){
		printk("Too high or too low RSSI: %d (dB)\n", rssi);
		return;
	}
	else{
		rssi_idx = rssi - RSSI_MIN;
	}

	pdr_cur = pdr_table[mcs][rssi_idx];
	
	if (pdr_cur == 0){
		pdr_cur = pdr;
	}
	else{
		pdr_cur = ((10-ALPHA)*pdr_cur + ALPHA*pdr)/10;
	}
	/*
	for (unsigned int i = 0; i < rssi_idx; i++){
		if (pdr_table[mcs][i] > pdr_cur && pdr_table[mcs][i] <= SCALE){
			pdr_cur = pdr_table[mcs][i];
			break;
		}
	} 
	*/
	pdr_table[mcs][rssi_idx] = pdr_cur;
}
void print_table(unsigned int table[][NUM_RSSI]){
	unsigned int i = 0;
	for (i = 0; i < NUM_MCS; i++){
		unsigned int j=0;
		printk("MCS %d Rcv: (", i);	
		for (j=0; j < NUM_RSSI; j++){
			printk("%d ", table[i][j]);
		}
		printk(")\n");
	}
}
unsigned int get_pdr(unsigned char mcs, int rssi){
	unsigned int ret = 0;
	if (rssi < RSSI_MIN){
		ret = 0;
	}
	else if ((rssi - RSSI_MIN) >= NUM_RSSI){
		ret = THRE_HIGH*100/SCALE;
	} 
	else{
		unsigned int rssi_idx = rssi - RSSI_MIN;
		ret = (pdr_table[mcs][rssi_idx]*100)/SCALE;
	}
	
	return ret;
}
void filling_blank(unsigned char mcs){
	unsigned int low_value = SCALE+1;
	unsigned int high_value = 0;
	unsigned int j = 0;

	for (j=0; j < NUM_RSSI; j++){
		unsigned int pdr = pdr_table[mcs][j];
		bool mark = (pdr > 0);
	
		if (mark == true){
			if (pdr >= THRE_HIGH){
				unsigned int k;
				res_table[mcs][j] = THRE_HIGH;
				
				for (k = j+1; k < NUM_RSSI; k++){
					res_table[mcs][k] = THRE_HIGH;
				}
				high_value = pdr;
				m_info[mcs].h_idx = j;
				m_info[mcs].h_exist = true;
				
				if (low_value == SCALE+1)
					m_info[mcs].l_idx = j;
				
				break;
			}
			else if (pdr < THRE_LOW){
				res_table[mcs][j] = 0;
				m_info[mcs].l_exist = true;
			}
			else
				res_table[mcs][j] = pdr;
			
			if (low_value == SCALE+1 && pdr <= low_value){
				m_info[mcs].l_idx = j;
				low_value = pdr;
			}

			if (pdr > THRE_LOW && pdr < THRE_HIGH){
				m_info[mcs].m_exist = true;
			}
		
			if (pdr > high_value){
				//printk("high pdr value: %d\n", pdr);
				if (high_value != 0){
					unsigned int k;
					//printk("h_idx: %d\n", m_info[mcs].h_idx);
					for (k = m_info[mcs].h_idx + 1; k < j; k++){
						res_table[mcs][k] = high_value + ((k-m_info[mcs].h_idx)*(pdr - high_value))/(j-m_info[mcs].h_idx);
						//printk("k: %d value: %d\n", k, res_table[mcs][k]);
					}
				}
				high_value = pdr;
				m_info[mcs].h_idx = j;
			}
			else if (pdr <= high_value){
				unsigned int k;	
				//printk("low pdr value: %d\n", pdr);
				//printk("h_idx: %d\n", m_info[mcs].h_idx);
				for (k = m_info[mcs].h_idx + 1; k <= j; k++){
					res_table[mcs][k] = high_value;
					//printk("k: %d value: %d\n", k, res_table[mcs][k]);
				}
				m_info[mcs].h_idx = j;
			}
		}
	}

	
	YDEBUG("Filling MCS: %d High: %d Low: %d\n ", mcs, m_info[mcs].h_idx, m_info[mcs].l_idx);	
	/*
		if (mark == true && pdr < THRE_LOW){
			m_info[mcs].l_exist = true;
		}
		if (mark == true && pdr <= low_value){
			m_info[mcs].l_idx = j;
			low_value = pdr;
		}
		if (mark == true && pdr > THRE_HIGH && pdr < THRE_HIGH){
			m_info[mcs].m_exist = true;
		}
		if (mark == true && pdr > high_value){
			if (high_value != 0){
				unsigned int k;
				for (k = m_info[mcs].h_idx + 1; k < j; k++){
					res_table[mcs][k] = high_value + ((k-m_info[mcs].h_idx)*(pdr - high_value))*SCALE/(j-m_info[mcs].h_idx);
				}
			}
			high_value = pdr;
			m_info[mcs].h_idx = j;
		}
		

		if (mark == true && pdr >= THRE_HIGH){
			unsigned int k;
			for (k = j+1; k < NUM_RSSI; k++){
				res_table[mcs][k] = THRE_HIGH;
			}
			high_value = pdr;
			m_info[mcs].h_idx = j;
			m_info[mcs].h_exist = true;
			break;
		}
	}*/

}
void monotonicity(void){
	unsigned char ref_mcs = NUM_MCS;
	unsigned char i = 0;
	
	for (i=0; i < NUM_MCS; i++){
		if (m_info[i].h_exist && m_info[i].l_exist){
				ref_mcs = i;
				break;
		}
		if ((m_info[i].h_exist && m_info[i].m_exist) && i < ref_mcs){
			ref_mcs = i;
		}
	}
	if (ref_mcs == NUM_MCS){
		unsigned int id_mcs = NUM_RSSI;
		for (i=0; i < NUM_MCS; i++){
			
			unsigned int temp_id = m_info[i].h_idx > (unsigned int)i ? m_info[i].h_idx - (unsigned int)i : 0;

			if (temp_id < id_mcs){
				ref_mcs = i;
				id_mcs = temp_id;
			}	
		}
	}
	
	if (m_info[ref_mcs].l_exist == false){
		unsigned int high_value = res_table[ref_mcs][m_info[ref_mcs].h_idx];
		unsigned int low_value = res_table[ref_mcs][m_info[ref_mcs].l_idx];
		unsigned int slope = 0;
		unsigned int k = 0;
		unsigned int low_limit = 0;
		unsigned int low_id = m_info[ref_mcs].l_idx;
		unsigned int high_id = m_info[ref_mcs].h_idx;

		if (m_info[ref_mcs].m_exist == false){
			slope = THRE_HIGH / RSSI_TRANSITION;
		}
		else{
			if (high_value == low_value)
				slope = THRE_HIGH / RSSI_TRANSITION;
			else
			//slope = (high_value - low_value)*SCALE / (m_info[ref_mcs].h_idx - m_info[ref_mcs].l_idx);
				slope = (high_value - low_value) / (m_info[ref_mcs].h_idx - m_info[ref_mcs].l_idx);
		}
		
		low_limit = m_info[ref_mcs].h_idx > RSSI_TRANSITION ? m_info[ref_mcs].h_idx - RSSI_TRANSITION : 0;
	
		YDEBUG("REF MCS: %d High: %d Low: %d Slope: %d Low Limit: %d\n ", ref_mcs, m_info[ref_mcs].h_idx, m_info[ref_mcs].l_idx, slope, low_limit);	
			
		if (m_info[ref_mcs].l_idx > low_limit){
			for (k = m_info[ref_mcs].l_idx; k >= low_limit; k--){
				int temp_value = high_value - (m_info[ref_mcs].h_idx - k)*slope;
				//res_table[ref_mcs][k] = high_value - (m_info[ref_mcs].h_idx-k)*slope;
				low_id = k;
				if (temp_value > THRE_LOW)
					res_table[ref_mcs][k] = (unsigned int)temp_value;
				else{ 
					res_table[ref_mcs][k] = 0;
					break;
				}
			}
		}
		if (res_table[ref_mcs][low_id] > 0){
			low_id--;
		}
		for (k=m_info[ref_mcs].h_idx; k < NUM_RSSI; k++){
				int temp_value = high_value + (k - m_info[ref_mcs].h_idx)*slope;
				
				if (temp_value < THRE_HIGH){
						res_table[ref_mcs][k] = (unsigned int)temp_value;
						high_id = k;
				}		
				else{
					unsigned int l = 0;
					high_id = k;
					for (l = k; l < NUM_RSSI; l++){
						res_table[ref_mcs][l] = THRE_HIGH;
					}
					break;
				}
		}
		m_info[ref_mcs].l_idx = low_id;
		m_info[ref_mcs].h_idx = high_id;
		YDEBUG("REF MCS: %d High: (%d, %d) Low: (%d, %d)\n", ref_mcs, m_info[ref_mcs].h_idx, high_value, m_info[ref_mcs].l_idx, low_value);	
	}

#if 1
	for (i = 0; i < NUM_MCS; i++){
		bool low = m_info[i].l_exist;
		bool middle = m_info[i].m_exist;
		bool high = m_info[i].h_exist;
		unsigned int slope = 0;
		unsigned int k = 0;
	
		YDEBUG("MCS: %d High: %d Low: %d (%d %d %d)\n", i, m_info[i].h_idx, m_info[i].l_idx, low, middle, high);	

		if (i == ref_mcs)
				continue;
		if (high && low)
				continue;
		else if (high && middle)
				continue;
		else if (high && !middle){
				if(m_info[i].h_idx > m_info[ref_mcs].h_idx + i - ref_mcs ){
						m_info[i].h_idx = m_info[ref_mcs].h_idx + i - ref_mcs;
				}
				if (m_info[i].l_idx > m_info[ref_mcs].l_idx + i -ref_mcs){
						m_info[i].l_idx = m_info[ref_mcs].l_idx + i -ref_mcs;
				}

				if (m_info[i].h_idx == m_info[i].l_idx)
					slope = THRE_HIGH / RSSI_TRANSITION;
				else
					slope = THRE_HIGH / (m_info[i].h_idx - m_info[i].l_idx);
				
				printk("MCS: %d High: %d Low: %d (%d %d %d) Slope: %d\n", (unsigned int)i, m_info[i].h_idx, m_info[i].l_idx, low, middle, high, slope);
				
				for (k = m_info[i].h_idx; k < 50; k++){
						res_table[i][k] = THRE_HIGH;
				}
				for (k = 0; k <= m_info[i].l_idx; k++){
						res_table[i][k] = 0;
				}

				for (k = m_info[i].l_idx+1; k < m_info[i].h_idx; k++){
						res_table[i][k] = THRE_HIGH - (m_info[i].h_idx-k)*slope;
						if (res_table[i][k] < THRE_LOW){
								res_table[i][k] = 0;
						}
				}
		}
		else if (!high && !low){
				unsigned int high_value = res_table[i][m_info[i].h_idx];
				unsigned int low_value = res_table[i][m_info[i].l_idx];
				unsigned int k;

				if (m_info[i].l_idx == m_info[i].h_idx){
						if (m_info[i].h_idx == 0)
								continue;

						YDEBUG("Single sample\n");

						for (k = 0; k < m_info[i].h_idx; k++){
								int temp_value =  high_value > (m_info[i].h_idx-k)*(THRE_HIGH /RSSI_TRANSITION) ? high_value - (m_info[i].h_idx-k)*(THRE_HIGH /RSSI_TRANSITION) : 0;
								res_table[i][k] = temp_value;
								if (res_table[i][k] < THRE_LOW){
										res_table[i][k] = 0;
								}
						}

						for (k = m_info[i].h_idx; k < 50; k++){
								int temp_value =  high_value + (k - m_info[i].h_idx)*(THRE_HIGH /RSSI_TRANSITION) > 0 ? high_value + (k - m_info[i].h_idx)*(THRE_HIGH /RSSI_TRANSITION) : 0;
								res_table[i][k] = temp_value;
								if (res_table[i][k] >= THRE_HIGH){
										res_table[i][k] = THRE_HIGH;
								}
						}
				}

				else{
					unsigned int slope = (high_value - low_value) / (m_info[i].h_idx - m_info[i].l_idx);
						for (k = 0; k < m_info[i].l_idx; k++){
								if ( k + RSSI_TRANSITION < m_info[i].l_idx )
										res_table[i][k] = 0;
								else{
										int temp_value =  low_value > (m_info[i].l_idx - k)*slope ? low_value - (m_info[i].l_idx - k)*slope : 0;
										res_table[i][k] = temp_value;
										if (res_table[i][k] < THRE_LOW){
												res_table[i][k] = 0;
										}
								}
						}

						for (k = m_info[i].h_idx+1; k < 50; k++){
								res_table[i][k] = high_value + (k - m_info[i].h_idx)*slope;
								if (res_table[i][k] >= THRE_HIGH){
										res_table[i][k] = THRE_HIGH;
								}
						}
				}
		}
	}
#endif
}
void print_pdr_table(void){
	printk("-------------------Measured Table----------------------\n");
	print_table(pdr_table);
	printk("\n");
}
void print_res_table(void){
	printk("-------------------Result Table----------------------\n");
	print_table(res_table);
	printk("\n");
}



