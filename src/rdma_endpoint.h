/* ***********************************************
MYID	: Chen Fan
LANG	: G++
PROG	: RDMA_ENDPOINT_H
************************************************ */

#ifndef RDMA_ENDPOINT_H
#define RDMA_ENDPOINT_H

#include <map>

#include "rdma_channel.h"

class RDMA_Session;

class RDMA_Endpoint
{
public:
    friend class RDMA_Buffer;
    friend class RDMA_Channel;
    friend class RDMA_Session;

    RDMA_Endpoint(RDMA_Session* session, int ib_port);
    ~RDMA_Endpoint();


    struct cm_con_data_t get_local_con_data();
    void connect(struct cm_con_data_t remote_con_data);
    void recv();
    void send_message(Message_type msgt);
    void read_data(RDMA_Buffer* buffer, Remote_info msg);

    std::map<uint64_t, uint64_t> map_table;
    uint64_t find_in_table(uint64_t key);

private:
    int modify_qp_to_init();
    int modify_qp_to_rtr();
    int modify_qp_to_rts();

    bool connected_;
    int ib_port_;

    RDMA_Session* session_;
    // Queue Pair
    ibv_qp* qp_;
    RDMA_Address self_, remote_;
    // Message channel
    RDMA_Channel* channel_;
};

#endif // !RDMA_ENDPOINT_H