#ifndef ML605_API_H
#define ML605_API_H

#define ML605_MAGIC 'M'     /**< Magic number for use in IOCTLs */
#define ML605_MAX_CMD 4     /**< Total number of IOCTLs */

// RD means raw data
#define RD_CMD_QUERY_TX_BUF   _IOR(ML605_MAGIC, 1, int)
#define RD_CMD_QUERY_RX_BUF   _IOR(ML605_MAGIC, 2, int)
#define RD_CMD_GET_COUNTER    _IOR(ML605_MAGIC, 3, int)
#define RD_CMD_SET_RF_CMD     _IOW(ML605_MAGIC, 4, int)

// PCIE SFP start flag
#define SFP_TX_START 0
#define SFP_RX_START 1

int ML605Open(void);
int ML605Close(int fd);
int ML605Send(int fd, const void *buf, unsigned int len);
int ML605Recv(int fd, void *buf, unsigned int len);
int ML605QueryTxBuf(int fd);
int ML605QueryRxBuf(int fd);
int ML605StartEthernet(int fd, int flag);
int ML605GetHwCounterMs(int fd);
int ML605GetHwCounters(int fd, int *ptr_counter_ms, int *ptr_counter_50mhz);
int ML605SetRfCmd(int fd, int rf_cmd);

#endif    // ML605_API_H
