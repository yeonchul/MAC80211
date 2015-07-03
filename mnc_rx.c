// Modified_130524_GJLEE_YCSHIN

#include <linux/skbuff.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include "mnc_tx.h"
#include "mnc_rx.h"
#include "tl_rx.h"
#include "gf.h"

#define ETIMEOUT 125 // 500msec
#define DTIMEOUT 125 // 500msec
#define ETHERLEN 14

//static unsigned int out_of_order = 0;

void skb_decoding_sys(struct sk_buff *skb){
	int mh_pos = skb_mac_header(skb) - skb->data;
	int i;
	char *mh = skb_mac_header(skb);

	for(i = 0; i < ETHERLEN; i++){
		*(mh + ETHERLEN - 1 + 3 - i) = *(mh + ETHERLEN - 1 - i);
	}

	mh_pos += 3;

	skb_set_mac_header(skb, mh_pos);

	skb_pull(skb, 3);
}

void decoding_try(struct sk_buff *skb)
{
	static struct mnc_queue_head list;
	u16 ethertype;
	static int i = 0;
	unsigned char eid;
	unsigned char kp;
	unsigned int k;
	unsigned char m;
	int mh_pos;
	struct mnc_queue *mncq;
	unsigned long cjiffies;
	static unsigned char last_did[3] = {0, 0, 0};
	unsigned char j;
	struct net_device *rdev = skb->dev;

	mh_pos = skb_mac_header(skb) - skb->head;
	ethertype = (skb->head[mh_pos+12]<<8) | skb->head[mh_pos+13];

	if((ethertype != 0x0810) && (ethertype != 0x0811)){
		netif_receive_skb(skb);
		return;
	}
	else if(ethertype == 0x0811){
//		printk(KERN_INFO "Decode packet! ethertype: %x\n", ethertype);
		//dev_kfree_skb(skb);
		skb_linearize(skb);
		if(tr_get_src()) tl_receive_skb_src(skb);
		else tl_receive_skb_dst(skb);
		return;
	}
	else{
//		printk("Decoding?\n");
//		netif_receive_skb(skb);
		
		skb->head[mh_pos+12]=0x08;
		skb->head[mh_pos+13]=0x00;
		skb->protocol = 0x0008;

		skb_linearize(skb);

		if(i == 0){
			printk(KERN_INFO "Initialize MNC\n");
			mnc_queue_head_init(&list);
			i = 1;
		}

		cjiffies = jiffies;
		eid = skb->data[1];
		kp = skb->data[0];
		k = kp >> 2;
		m = (skb->data[2] >> 2);

		mnc_queue_head_check_etime(&list, cjiffies);

		//	printk(KERN_INFO "last_did[0] = %x, last_did[1] = %x, last_did[2] = %x\n", last_did[0], last_did[1], last_did[2]);

		for(j = 0; j < 3; j++){
			if(eid == last_did[j]){
				//			printk(KERN_INFO "Drop skb, eid = %x, last_did[%d] = %x\n", eid, j, last_did[j]);
				kfree_skb(skb);
				return;
			}
		}

		mncq = mnc_queue_head_find_eid(&list, eid);

		if(mncq == NULL){
			mncq = mnc_queue_create(eid, cjiffies, kp, &list);
			mnc_queue_insert(&list, mncq);
		}

		if(kp != mncq->kp){
			printk(KERN_INFO "wrong kp\n");
			kfree_skb(skb);
			return;
		}

		if(m != 0x3f){ // original packet
			struct sk_buff *skb_temp = skb_copy(skb, GFP_ATOMIC);
			skb_decoding_sys(skb);
			netif_receive_skb(skb);
			skb = skb_temp;
		}

		else{ // coding packet
			mncq->sys = false;
		}

		skb_queue_tail(&(mncq->skbs), skb);

		// printk(KERN_INFO "eid = %x, kp = %x, m = %x, cjiffies = %lx, skb = %lx\n", eid, kp, m, cjiffies, skb);

		if(skb_queue_len(&(mncq->skbs)) == k){
			if(mncq->sys){
				// printk(KERN_INFO "All member of queue is systematic code, free mncq, eid = %x\n", eid);
				mnc_queue_free(mncq);
			}

			else{
				struct sk_buff_head newskbs;
				skb_queue_head_init(&newskbs);

				// printk(KERN_INFO "Decoding start, eid = %x\n", eid);
				if(skbs_decoding(&(mncq->skbs), &newskbs, kp)){
					// printk(KERN_INFO "Decoding success! eid: %x, kp: %x, last_did[0] = %x, last_did[1] = %x, last_did[2] = %x\n", eid, kp, last_did[0], last_did[1], last_did[2]);
					if((tr_get_data_n() > 0) && (!tr_get_src())){
						mnc_encoding_tx(&newskbs, rdev, eid);
					}
					while(!skb_queue_empty(&newskbs)){
						netif_receive_skb(skb_dequeue(&newskbs));
					}

					// printk(KERN_INFO "Free the queue, eid = %x\n", eid);
					mnc_queue_free(mncq);
					//printk(KERN_INFO "Out-of-order: %d\n", out_of_order);
					// printk(KERN_INFO "Free success!\n");
				}
				else{
					// printk(KERN_INFO "Decoding fail! eid: %x\n", eid);
					return;
				}
			}
			last_did[2] = last_did[1];
			last_did[1] = last_did[0];
			last_did[0] = eid;

		}
		//	printk(KERN_INFO "\n");
	}

}


void mnc_queue_head_check_etime(struct mnc_queue_head *list, unsigned long cjiffies){
	struct mnc_queue *mncq;
	int i = 1;

	if(list == NULL) return;

	mncq = list->next;

	while(mncq != NULL){
		struct mnc_queue *next = mncq->next;
//		printk(KERN_INFO "%dth queue: eid = %x, ejiffies = %lx\n", i, mncq->eid, mncq->ejiffies);
		if((cjiffies - mncq->ejiffies) > ETIMEOUT){
//			printk(KERN_INFO "ETIMEOUT in %dth queue: eid = %x, cjiffies - ejiffies = %lx\n", i, mncq->eid, cjiffies - mncq->ejiffies);
			mnc_queue_free(mncq);
		}
		mncq = next;
		i++;
	}
}

void mnc_queue_head_init(struct mnc_queue_head *list){
	list->next = NULL;
	list->qlen = 0;
}

struct mnc_queue *mnc_queue_create(unsigned char eid, unsigned long ejiffies, unsigned char kp, struct mnc_queue_head *list){
	struct mnc_queue *mncq;
	mncq = kmalloc(sizeof(struct mnc_queue), GFP_ATOMIC);
	mncq->eid = eid;
	mncq->ejiffies = ejiffies;
	mncq->kp = kp;
	mncq->sys = true;
	skb_queue_head_init(&(mncq->skbs));
	mncq->next = NULL;
	mncq->prev = NULL;
	mncq->list = list;
	return mncq;
}

void mnc_queue_free(struct mnc_queue *mncq){
	struct mnc_queue *prev;
	struct mnc_queue *next;
	
	if(mncq == NULL) return;
	
	prev = mncq->prev;
	next = mncq->next;
	
	if(prev == NULL){
		if(next == NULL){
			mncq->list->next = NULL;
		}
		else{
			mncq->list->next = next;
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

	skb_queue_purge(&(mncq->skbs));
	mncq->list->qlen--;
	kfree(mncq);
}

void mnc_queue_insert(struct mnc_queue_head *list, struct mnc_queue *newmncq){
	struct mnc_queue *mncq;
	
	if((mncq = list->next) == NULL){
		list->next = newmncq;
		list->qlen++;
		return;
	}

	while(mncq->next != NULL){
		mncq = mncq->next;
	}

	mncq->next = newmncq;
	newmncq->prev = mncq;
	list->qlen++;
}

struct mnc_queue *mnc_queue_head_find_eid(struct mnc_queue_head *list, unsigned char eid){
	struct mnc_queue *mncq;

	if(list == NULL) return NULL;

	if((mncq = list->next) == NULL) return NULL;

	while(mncq != NULL){
		if(mncq->eid == eid) return mncq;
		mncq = mncq->next;
	}

	return NULL;
}

bool skbs_decoding(struct sk_buff_head *skbs, struct sk_buff_head *newskbs, unsigned char kp){
	unsigned int k = kp >> 2;
	unsigned int p = kp & 0x3;
	unsigned int msize = k*p;
	unsigned int mncheadsize = 3 + p*p*k;
	struct sk_buff *skb[k];
	struct sk_buff *newskb[k];
	bool is_sys[k];
	unsigned char coefficient[msize][msize];
	unsigned char U[msize][msize];
	unsigned char L[msize][msize];
	unsigned int pivot[msize];

	unsigned int i, j, m, n;
	unsigned int mrow, jp, mp;
	unsigned int rank = msize;
	unsigned int d, len;

	// Initialize pivot matrix
	for(i = 0; i < msize; i++){
		pivot[i] = i;
	}

	for(i = 0; i < k; i++){
		skb[i] = newskb[i] = NULL;
		is_sys[i] = false;
	
	}

	// Check is_sys & Fill systematic code
	for(i = 0; i < k; i++){
		struct sk_buff *skb_temp = skb_dequeue(skbs);
		unsigned int mindex = (skb_temp->data[2] >> 2);
		if(mindex != 0x3f){
			if(is_sys[mindex] == true){ // There are two skb to have same m
				kfree_skb(skb_temp);
				for(j = 0; j < k; j++){
					if(skb[j] != NULL){
						skb_queue_tail(skbs, skb[j]);
					}
				}
				return false;
			}
			is_sys[mindex] = true;
			skb[mindex] = skb_temp;
		}
		else{
			skb_queue_tail(skbs, skb_temp);
		}	
	}


	// Fill non-systematic code
	for(i = 0; i < k; i++){
		if(!is_sys[i]){
			skb[i] = skb_dequeue(skbs);
		}
	}

//	for(i = 0; i < k; i++){
//		printk(KERN_INFO "is_sys[%x] = %x, skb[%x] = %lx\n", i, is_sys[i], i, skb[i]);
//	}

/*  debuging tool
	for(i = 0; i < k; i++){
		if(skb[i] != NULL){
			skb_queue_tail(skbs, skb[i]);
		}
	}
	return true;
*/

	// Fill coefficient matrix
	for(i = 0; i < k; i++){
		mrow = i*p;
		if(is_sys[i]){ // Systematic code
			for(m = 0; m < p; m++){
				for(n = 0; n < msize; n++){
					coefficient[mrow+m][n] = ((n==(mrow+m))? 1: 0);
				}
			}
		}
		else{ // Non-systematic code
			for(j = 0; j < msize; j+=p){
				jp = j*p;
				for(m = 0; m < p; m++){
					mp = m*p;
					for(n = 0; n < p; n++){
						coefficient[mrow+m][j+n] = skb[i]->data[3+jp+mp+n];
					}
				}
			}
		}
	}
	
	// Arr = LU
	lu_fac(msize, msize, coefficient, U, L, pivot);

	// Check rank
	for(i = 0; i < msize; i++){
		if(U[i][i] == 0){
			rank--;
			d = pivot[i]/p;
			kfree_skb(skb[d]);
			skb[d] = NULL;
		}
	}

	// If Arr is not full rank, delete dependent matrix
	if(rank != msize){
		for(i = 0; i < k; i++){
			if(skb[i] != NULL){
				skb_queue_tail(skbs, skb[i]);
			}
		}
		return false;
	}

	for(i = 0; i < k; i++){
		newskb[i] = skb_copy(skb[i], GFP_ATOMIC);
	}

	len = newskb[0]->len - (is_sys[0] ? 3 : mncheadsize);

	// Decode skb and fill newskb
	skb_substi(msize, k, p, U, L, pivot, skb, newskb, len, mncheadsize, is_sys);

	for(i = 0; i < k; i++){
		if(!is_sys[i]){
			//printk(KERN_INFO "Decoding success Non-sys : %x\n", i);
			skb_trim(newskb[i], len);
			skb_queue_tail(newskbs, newskb[i]);
			//out_of_order++;
		}
		else{
			kfree_skb(newskb[i]);
		}
	}
	return true;
}

void skb_substi(unsigned int msize, unsigned int k, unsigned int p, unsigned char U[][msize], unsigned char L[][msize], unsigned int pivot[], struct sk_buff *skb[], struct sk_buff *newskb[], unsigned int len, unsigned int mncheadsize, bool is_sys[]){
	unsigned char b[msize][1];
	unsigned char t[msize][1];
	unsigned char y[msize][1];
	unsigned char x[msize][1];

	unsigned int i, j, l, n;
	unsigned int jp;

	for(i = 0; i < len; i += p){

		// Fill B matrix
		for(j = 0; j < k; j++){
			jp = j*p;
			for(l = 0; l < p; l++){
				b[jp + l][0] = t[jp + l][0] = skb[j]->data[(is_sys[j] ? 3 : mncheadsize) + i + l];
			}
		}

		// Multiply P matrix, make PB
		for(j = 0; j < msize; j++){
			b[j][0] = t[pivot[j]][0];
		}

		// Ly = PB
		for(j = 0; j < msize; j++){
			y[j][0] = b[j][0];
			for(l = 0; l < j; l++){
				y[j][0] = gsub(y[j][0], gmul(L[j][l], y[l][0]));
			}
		}

		// Ux = y
		for(j = 0; j < msize; j++){
			n = msize - 1 - j;
			if(U[n][n] == 0) continue;
			x[n][0] = y[n][0];
			for(l = n + 1; l < msize; l++){
				x[n][0] = gsub(x[n][0], gmul(U[n][l], x[l][0]));
			}
			x[n][0] = gdiv(x[n][0], U[n][n]);
		}

		// make original packets from x
		for(j = 0; j < k; j++){
			jp = j*p;
			for(l = 0; l < p; l++){
				newskb[j]->data[i + l] = x[jp + l][0];
			}
		}		
	}
}

/* LU Factorization
 *
 * Arr = LU
 *
 */

void lu_fac(int m, int n, unsigned char Arr[][n], unsigned char U[][n], unsigned char L[][n], unsigned int pivot[]){
	
	// Arr : m x n
	// U   : n x n
	// L   : n x n

	int i, j, k;
	int change;
	int temp_i;
	unsigned char temp_c;

	if(m > n) return; // Only m <= n

	for(i = 0; i < n; i++){
		for(j = 0; j < n; j++){
			U[i][j] = (i < m ? Arr[i][j] : 0);
			L[i][j] = 0;
		}
	}
	
	for(j = 0; j < n; j++){
		L[j][j] = 1;
		for(i = j + 1; i < n; i++){
			if(U[j][j] == 0){
				for(k = j + 1; k < n; k++){
					if(U[k][j] != 0) break;
				}
				change = k;
				if(change == n) continue;

				for(k = 0; k < n; k++){
					temp_c = Arr[j][k];
					Arr[j][k] = Arr[change][k];
					Arr[change][k] = temp_c;
				}

				for(k = j; k < n; k++){
					temp_c = U[j][k];
					U[j][k] = U[change][k];
					U[change][k] = temp_c;
				}

				for(k = 0; k < j; k++){
					temp_c = L[j][k];
					L[j][k] = L[change][k];
					L[change][k] = temp_c;
				}
				temp_i = pivot[change];
				pivot[change] = pivot[j];
				pivot[j] = temp_i;
			}

			L[i][j] = gdiv(U[i][j], U[j][j]);
			if(L[i][j] == 0) continue;
			for(k = j; k < n; k++){
				U[i][k] = gsub(U[i][k], gmul(U[j][k], L[i][j]));
			}
		}
	}
}

void print_matrix(int m, int n, unsigned char M[][n]){
	int i, j;
	for(i = 0; i < m; i++){
		for(j = 0; j < n; j++){
			printk(KERN_INFO "%x\t", M[i][j]);
		}
		printk(KERN_INFO "\n");
	}
}
