/* ***********************************************
MYID	: Chen Fan
LANG	: G++
PROG	: RDMA_CHANNEL_H
************************************************ */

#ifndef RDMA_CHANNEL_H
#define RDMA_CHANNEL_H

#include "rdma_buffer.h"
#include "rdma_message.h"

enum Channel_status
{
    NONE,
    IDLE,
    LOCK
};

class RDMA_Endpoint;
class RDMA_Buffer;

class RDMA_Channel
{
public:
    friend class TCP_Sock_Pre;
    friend class RDMA_Session;
    friend class RDMA_Endpoint;

    RDMA_Channel(RDMA_Endpoint* endpoint, ibv_pd* pd, ibv_qp* qp);
    ~RDMA_Channel();

    void send_message(Message_type msgt, uint64_t addr = 0);

private:
    void ParseMessage(Message_type& rm, void* buffer);
    void write(uint32_t imm_data, size_t size, uint64_t wr_id = 0);
    void send(uint32_t imm_data, size_t size, uint64_t wr_id = 0);

    RDMA_Endpoint* endpoint_;
    ibv_pd* pd_;
    ibv_qp* qp_;

    RDMA_Buffer* incoming_;
    RDMA_Buffer* outgoing_;

    Remote_MR remote_mr_;

    Channel_status local_status_;
    Channel_status remote_status_;
};

#endif // !RDMA_CHANNEL_H