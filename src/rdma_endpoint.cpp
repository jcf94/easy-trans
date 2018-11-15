/* ***********************************************
MYID	: Chen Fan
LANG	: G++
PROG	: RDMA_ENDPOINT_CPP
************************************************ */

#include <netdb.h>
#include "rdma_util.h"

#include "rdma_message.h"
#include "rdma_endpoint.h"
#include "rdma_buffer.h"
#include "rdma_channel.h"

RDMA_Endpoint::RDMA_Endpoint(ibv_pd* pd, ibv_cq* cq, ibv_context* context, int ib_port, int cq_size)
    : pd_(pd), ib_port_(ib_port), connected_(false)
{
    // create the Queue Pair
    struct ibv_qp_init_attr qp_init_attr;
    memset(&qp_init_attr, 0, sizeof(qp_init_attr));

    qp_init_attr.qp_type    = IBV_QPT_RC;
    // qp_init_attr.sq_sig_all = 1;
    qp_init_attr.send_cq    = cq;
    qp_init_attr.recv_cq    = cq;
    qp_init_attr.cap.max_send_wr  = cq_size;
    qp_init_attr.cap.max_recv_wr  = cq_size;
    qp_init_attr.cap.max_send_sge = 1;
    qp_init_attr.cap.max_recv_sge = 1;
    
    qp_ = ibv_create_qp(pd_, &qp_init_attr);
    if (!qp_)
    {
        log_error("failed to create QP");
        return;
    }
    
    log_info(make_string("QP was created, QP number=0x%x", qp_->qp_num));

    modify_qp_to_init();

    // Local address
    struct ibv_port_attr attr;
    if (ibv_query_port(context, ib_port_, &attr))
    {
        log_error(make_string("ibv_query_port on port %u failed", ib_port_));
        return;
    }

    self_.lid = attr.lid;
    self_.qpn = qp_->qp_num;
    self_.psn = static_cast<uint32_t>(New64()) & 0xffffff;

    // int SIZE = 32;
    // // Create Message Buffer ......
    // incoming_mbuffer_ = new RDMA_Buffer(this, SIZE);
    // outgoing_mbuffer_ = new RDMA_Buffer(this, SIZE);
    // // ACK Buffer ......
    // incoming_abuffer_ = new RDMA_Buffer(this, SIZE);
    // outgoing_abuffer_ = new RDMA_Buffer(this, SIZE);
    channel_ = new RDMA_Channel(this, pd_, qp_);

    log_info("RDMA_Endpoint Created");
}

RDMA_Endpoint::~RDMA_Endpoint()
{
    delete channel_;

    if (ibv_destroy_qp(qp_))
    {
        log_error("Failed to destroy QP");
    }

    log_info("RDMA_Endpoint Deleted");
}

cm_con_data_t RDMA_Endpoint::get_local_con_data()
{
    cm_con_data_t local_con_data;
    // exchange using TCP sockets info required to connect QPs
    local_con_data.maddr = htonll((uintptr_t)channel_->incoming()->buffer());
    local_con_data.mrkey = htonl(channel_->incoming()->mr()->rkey);
    local_con_data.qpn = htonl(self_.qpn);
    local_con_data.lid = htonl(self_.lid);
    local_con_data.psn = htonl(self_.psn);

    log_info(make_string("Local QP number  = 0x%x", self_.qpn));
    log_info(make_string("Local LID        = 0x%x", self_.lid));
    log_info(make_string("Local PSN        = 0x%x", self_.psn));

    return local_con_data;
}

void RDMA_Endpoint::connect(cm_con_data_t remote_con_data)
{
    if (connected_)
    {
        log_warning("Channel Already Connected.");
    } else
    {
        channel_->remote_mr_.remote_addr = ntohll(remote_con_data.maddr);
        channel_->remote_mr_.rkey = ntohl(remote_con_data.mrkey);
        remote_.qpn = ntohl(remote_con_data.qpn);
        remote_.lid = ntohl(remote_con_data.lid);
        remote_.psn = ntohl(remote_con_data.psn);

        /* save the remote side attributes, we will need it for the post SR */

        //fprintf(stdout, "Remote address   = 0x%"PRIx64"\n", remote_con_data.addr);
        //fprintf(stdout, "Remote rkey      = 0x%x\n", remote_con_data.rkey);
        log_info(make_string("Remote QP number = 0x%x", remote_.qpn));
        log_info(make_string("Remote LID       = 0x%x", remote_.lid));
        log_info(make_string("Remote PSN       = 0x%x", remote_.psn));

        modify_qp_to_rtr();
        modify_qp_to_rts();

        connected_ = true;
    }
}

void RDMA_Endpoint::close()
{
    if (connected_)
    {
        RDMA_Message::send_message_to_channel(channel_, RDMA_MESSAGE_CLOSE);
        RDMA_Message::send_message_to_channel(channel_, RDMA_MESSAGE_TERMINATE);
        connected_ = false;
    } else
    {
        log_warning("Endpoint Already Closed");
    }
}

void RDMA_Endpoint::send_data(void* addr, int size)
{
    RDMA_Buffer* new_buffer = new RDMA_Buffer(this, pd_, size, addr);
    channel_->request_read(new_buffer);
}

int RDMA_Endpoint::modify_qp_to_init()
{
    struct ibv_qp_attr attr;
    int flags;
    int rc;

    // do the following QP transition: RESET -> INIT
    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_INIT;
    attr.port_num = ib_port_;
    attr.pkey_index = 0;
    attr.qp_access_flags = IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ;

    flags = IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS;

    rc = ibv_modify_qp(qp_, &attr, flags);
    if (rc) {
        fprintf(stderr, "failed to modify QP state to INIT\n");
        return rc;
    }

    return 0;
}

int RDMA_Endpoint::modify_qp_to_rtr()
{
    struct ibv_qp_attr attr;
    int flags;
    int rc;

    // do the following QP transition: INIT -> RTR
    memset(&attr, 0, sizeof(attr));

    attr.qp_state = IBV_QPS_RTR;
    attr.path_mtu = IBV_MTU_4096;
    attr.dest_qp_num = remote_.qpn;
    attr.rq_psn = remote_.psn;
    attr.max_dest_rd_atomic = 1;
    attr.min_rnr_timer = 0x12;
    attr.ah_attr.is_global = 0;
    attr.ah_attr.dlid = remote_.lid;
    attr.ah_attr.sl = 0;
    attr.ah_attr.src_path_bits = 0;
    attr.ah_attr.port_num = ib_port_;

    flags = IBV_QP_STATE | IBV_QP_AV | IBV_QP_PATH_MTU | IBV_QP_DEST_QPN | 
        IBV_QP_RQ_PSN | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER;

    rc = ibv_modify_qp(qp_, &attr, flags);
    if (rc) {
        fprintf(stderr, "failed to modify QP state to RTR\n");
        return rc;
    }

    return 0;
}

int RDMA_Endpoint::modify_qp_to_rts()
{
    struct ibv_qp_attr attr;
    int flags;
    int rc;

    // do the following QP transition: RTR -> RTS
    memset(&attr, 0, sizeof(attr));

    attr.qp_state = IBV_QPS_RTS;
    attr.timeout = 0x14;
    attr.retry_cnt = 7;
    attr.rnr_retry = 7; // infinite
    attr.sq_psn = self_.psn;
    attr.max_rd_atomic = 1;

    flags = IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | 
        IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC;

    rc = ibv_modify_qp(qp_, &attr, flags);
    if (rc) {
        fprintf(stderr, "failed to modify QP state to RTS\n");
        return rc;
    }

    return 0;
}

uint64_t RDMA_Endpoint::find_in_table(uint64_t key, bool erase)
{
    std::lock_guard<std::mutex> lock(map_lock_);
    auto temp = map_table_.find(key);
    if (temp == map_table_.end())
    {
        log_error("Request key not found in map_table");
        return 0;
    } else
    {
        //log_warning(make_string("Find %p %p", temp->first, temp->second));
        uint64_t res = temp->second;
        // Erase is important, if two RDMA_Buffer has same address
        // the table find result will be a out of date value
        if (erase) map_table_.erase(temp);
        return res;
    }
}

void RDMA_Endpoint::insert_to_table(uint64_t key, uint64_t value)
{
    std::lock_guard<std::mutex> lock(map_lock_);
    //log_warning(make_string("Insert %p %p", key, value));
    map_table_.insert(std::make_pair(key, value));
}

void RDMA_Endpoint::data_send_success(int size)
{
    total_send_data_ += size;
}

void RDMA_Endpoint::data_recv_success(int size)
{
    total_recv_data_ += size;
}