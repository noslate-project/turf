#ifndef _TURF_IPC_H_
#define _TURF_IPC_H_

#include <errno.h>
#include "crc32.h"
#include "misc.h"
#include "realm.h"

// represent an turf-ipc endpoint
struct tipc_ep {
  int fd;  // socket fd
};

enum tipc_msg_type {
  TIPC_MSG_SEED_READY,
  TIPC_MSG_FORK_REQ,
  TIPC_MSG_FORK_RSP,
  TIPC_MSG_MAX,
};

#pragma pack(push)
#pragma pack(1)

// msg header
struct tipc_hdr {
#define TIPC_HDR_MAGIC (0xffffffffffffffffULL)
  uint64_t magic;     // TIPC_HDR_MAGIC
  uint32_t checksum;  // crc32, without magic and checksum
  uint8_t type;       // TIPC_MSG_TYPE
  uint8_t padding;    // must be ZERO
  uint16_t length;    // whole msg length, including header
};                    // 16 bytes

#pragma pack(pop)

const char* tipc_msg_type(int type);

// callback for TIPC msg decoder
typedef int(TIPC_DecodeCB)(int type, char* buff, size_t nsize, void* data);
int _API tipc_read(int fd, TIPC_DecodeCB* cb, void* data);
int _API tipc_decode(char* buff, size_t nsize, TIPC_DecodeCB* cb, void* data);

// TIPC_MSG_SEED_READY
int _API tipc_enc_seed_ready(char* buff, size_t nsize);
int _API tipc_dec_seed_ready(char* buff, size_t nsize);

// TIPC_MSG_FORK_REQ
int _API tipc_enc_fork_req(char* buff, size_t nsize, struct rlm_t* realm);
int _API tipc_dec_fork_req(struct rlm_t* realm, char* buff, size_t nsize);

// TIPC_MSG_FORK_RSP
int _API tipc_enc_fork_rsp(char* buff, size_t nsize, struct rlm_t* realm);
int _API tipc_dec_fork_rsp(struct rlm_t* realm, char* buff, size_t nsize);

#endif  // _TURF_IPC_H_
