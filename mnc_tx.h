// Modified_130422_GJLEE_YCSHIN

#ifndef __MWNL_MNC_TX_H
#define __MWNL_MNC_TX_H

#include "ieee80211_i.h"

netdev_tx_t ieee80211_matrix_network_coding(struct sk_buff *skb, struct net_device *dev);

netdev_tx_t mnc_encoding_tx(struct sk_buff_head *skbs, struct net_device *dev, unsigned char id);

//struct sk_buff *make_skb_new(const struct sk_buff_head *skbs, unsigned int coding_num, unsigned char id);

#endif // __MWNL_MNC_TX_H
