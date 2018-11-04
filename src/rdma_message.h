/* ***********************************************
MYID   : Chen Fan
LANG   : G++
PROG   : RDMA_MESSAGE_H
************************************************ */

#ifndef RDMA_MESSAGE_H
#define RDMA_MESSAGE_H

#include "rdma_util.h"

enum Message_type
{
    RDMA_MESSAGE_ACK,
    RDMA_MESSAGE_BUFFER_UNLOCK,
    RDMA_MESSAGE_WRITE_REQUEST,
    RDMA_MESSAGE_READ_REQUEST,
    RDMA_MESSAGE_CLOSE,
    RDMA_MESSAGE_TERMINATE
};

struct Buffer_MR
{
    uint64_t remote_addr;
    uint32_t rkey;
};

struct Message_Content
{
    Buffer_MR buffer_mr;
    uint64_t buffer_size;

    // |remote_addr|rkey|buffer_size|
    // |     8B    | 4B |     8B    |
};

static const size_t kRemoteAddrStartIndex = 0;
static const size_t kRemoteAddrEndIndex = kRemoteAddrStartIndex + sizeof(Message_Content::buffer_mr.remote_addr);
static const size_t kRkeyStartIndex = kRemoteAddrEndIndex;
static const size_t kRkeyEndIndex = kRkeyStartIndex + sizeof(Message_Content::buffer_mr.rkey);
static const size_t kBufferSizeStartIndex = kRkeyEndIndex;
static const size_t kBufferSizeEndIndex = kBufferSizeStartIndex + sizeof(Message_Content::buffer_size);

static const size_t kMessageTotalBytes = kBufferSizeEndIndex;

namespace RDMA_Message {

std::string get_message(Message_type msgt);

void fill_message_content(char* target, void* addr, uint64_t size, ibv_mr* mr);
Message_Content parse_message_content(char* content);

};

#endif // !RDMA_MESSAGE_H