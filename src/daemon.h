#ifndef _TURF_DAEMON_H_
#define _TURF_DAEMON_H_

#include "misc.h"

// inner msg
#define TFD_MSG_VER 0x01
#define MSG_HDR_MAGIC DEF_MAGIC('T', 'F', 'D', TFD_MSG_VER)

#define T_MSG_CLI_REQ 1
#define T_MSG_CLI_RSP 2

struct msg_hdr {
  uint32_t hdr_magic;  // MSG_HDR_MAGIC with intf version.
  uint8_t msg_type;    // message type
  int8_t msg_code;     // message code
  uint16_t msg_size;   // without header
};

// normally daemon has nothing to export.

#endif  // _TURF_H_
