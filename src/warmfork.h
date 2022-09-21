#ifndef _TURF_WORMFORK_H_
#define _TURF_WORMFORK_H_

// Now we only support linux.

#include <unistd.h>  // read() ...
#include "ipc.h"     // ipc
#include "misc.h"
#include "realm.h"     // rlm_t
#include "turf_phd.h"  // turf place holder defines

struct tf_seed {
  struct tipc_ep endpoint;  // socket endpoint
};

// hold a phd slot
struct tf_phd_slot {
  void** pfn;  // func pointer
  char* name;  // func name
};

// export API
int _API load_turfphd(void);

#endif  // _TURF_WORMFORK_H_
