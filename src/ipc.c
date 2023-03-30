
#define _GNU_SOURCE
#include "ipc.h"
#include <string.h>  // strdup()

/*
 * DON'T use MSG macros outside the ipc.c source.
 * DON'T use begin with '_' inner macro directly.
 */

// define a msg scope, MSG_ENC for encoding
#define _MSG(buff, nsize)                                                      \
  char *_cur = buff, *_start = buff;                                           \
  size_t _left = nsize;                                                        \
  int _ec = 0;                                                                 \
  _start = _start;                                                             \
  for (int _i = 1; _i; --_i)

#define MSG_ENC(buff, nsize)                                                   \
  char* _enc;                                                                  \
  _MSG(buff, nsize)

// MSG_DEC for decoding
#define MSG_DEC(buff, nsize)                                                   \
  char* _dec;                                                                  \
  _MSG(buff, nsize)

#define _VA_ENC() _enc = _enc
#define _VA_DEC() _dec = _dec

// break if errors, make error in log.
#define _ERROR_BREAK(x)                                                        \
  _ec = -1;                                                                    \
  set_errno(x);                                                                \
  error("IPC: codec error %d in %s:%d", errno, __func__, __LINE__);            \
  dprint("_start(%p), _cur=(%p), _left=(%ld)", _start, _cur, _left);           \
  break;

// check size if not enough for rw
#define _CHECK_SIZE(x)                                                         \
  if (_left < (x)) {                                                           \
    _ERROR_BREAK(EMSGSIZE);                                                    \
  }

// put (x) bytes with value (v)
#define _PUTx(x, v)                                                            \
  _CHECK_SIZE((x));                                                            \
  memcpy(_cur, &v, (x));                                                       \
  _left -= (x);                                                                \
  _cur += (x);                                                                 \
  _enc = _enc;

// get (x) bytes to value (v)
#define _GETx(x, v)                                                            \
  _VA_DEC();                                                                   \
  _CHECK_SIZE((x));                                                            \
  memcpy((char*)&v, _cur, (x));                                                \
  _left -= (x);                                                                \
  _cur += (x);

// put a msg hdr
#define PUT_HDR(t)                                                             \
  _VA_ENC();                                                                   \
  struct tipc_hdr* _hdr = (struct tipc_hdr*)_cur;                              \
  _cur += sizeof(struct tipc_hdr);                                             \
  _left -= sizeof(struct tipc_hdr);                                            \
  _hdr->magic = TIPC_HDR_MAGIC;                                                \
  _hdr->type = (t);                                                            \
  _hdr->padding = 0;                                                           \
  _hdr->length = 0;                                                            \
  _hdr->checksum = 0;

// calc crc32 value for mas, include the header but skip magic and checksum.
#define _MSG_CRC() crc32(&_hdr->type, (_hdr->length - 12))

// update msg header (length and checksum)
#define CALC_HDR()                                                             \
  _VA_ENC();                                                                   \
  _hdr->length = (_cur - _start);                                              \
  _hdr->checksum = _MSG_CRC();

// check msg intergrity
#define CHECK_MSG()                                                            \
  _VA_DEC();                                                                   \
  struct tipc_hdr* _hdr = (struct tipc_hdr*)_cur;                              \
  _cur += sizeof(struct tipc_hdr);                                             \
  _left -= sizeof(struct tipc_hdr);                                            \
  if (_hdr->checksum != _MSG_CRC()) {                                          \
    _ERROR_BREAK(EBADMSG);                                                     \
  }

// check msg header
#define CHECK_HDR(t)                                                           \
  _VA_DEC();                                                                   \
  struct tipc_hdr* _hdr = (struct tipc_hdr*)_cur;                              \
  _cur += sizeof(struct tipc_hdr);                                             \
  _left -= sizeof(struct tipc_hdr);                                            \
  if (_hdr->magic != TIPC_HDR_MAGIC) {                                         \
    _ERROR_BREAK(EBADMSG);                                                     \
  }                                                                            \
  if (_hdr->type != (t)) {                                                     \
    _ERROR_BREAK(ENOMSG);                                                      \
  }

// skip msg header
#define SKIP_HDR(t)                                                            \
  struct tipc_hdr* _hdr = (struct tipc_hdr*)_cur;                              \
  _cur += sizeof(struct tipc_hdr);                                             \
  _left -= sizeof(struct tipc_hdr);

// get msg header
#define MSG_HDR() _hdr

// 1 bytes OP
#define PUT_1(v) _PUTx(1, v)
#define GET_1(v) _GETx(1, v)

// 2 bytes OP
#define PUT_2(v) _PUTx(2, v)
#define GET_2(v) _GETx(2, v)

// 4 bytes OP
#define PUT_4(v) _PUTx(4, v)
#define GET_4(v) _GETx(4, v)

// 8 bytes OP
#define PUT_8(v) _PUTx(8, v)
#define GET_8(v) _GETx(8, v)

// put string to buffer, terminated with '\0'
#define PUT_STR(x)                                                             \
  {                                                                            \
    _VA_ENC();                                                                 \
    int _has = (x != NULL);                                                    \
    PUT_1(_has);                                                               \
    if (_has) {                                                                \
      size_t _l = strlen(x) + 1;                                               \
      _CHECK_SIZE((_l));                                                       \
      strcpy(_cur, x);                                                         \
      _cur += _l;                                                              \
      _left -= _l;                                                             \
    }                                                                          \
  }

// get string from buffer, terminated with '\0'
#define GET_STR(x)                                                             \
  {                                                                            \
    int _has = 0;                                                              \
    GET_1(_has);                                                               \
    if (_has) {                                                                \
      _VA_DEC();                                                               \
      size_t _l = strlen(_cur) + 1;                                            \
      _CHECK_SIZE((_l));                                                       \
      x = strdup(_cur);                                                        \
      _cur += _l;                                                              \
      _left -= _l;                                                             \
    }                                                                          \
  }

// get MSG_ENC/DEC error code, 0(OK), -1(errno for reason)
#define MSG_EC() _ec

#define MSG_LEN() (_ec < 0) ? _ec : (_cur - _start)

/*
 * BEGIN of the IPC messages
 */
const char* tipc_msg_type(int type) {
  switch (type) {
    case TIPC_MSG_SEED_READY:
      return "seed_ready";
      break;

    case TIPC_MSG_FORK_REQ:
      return "fork_req";
      break;

    case TIPC_MSG_FORK_RSP:
      return "fork_rsp";
      break;
  }

  return "unknown";
}

#define HDR_LEN (sizeof(struct tipc_hdr))

int _API tipc_read(int fd, TIPC_DecodeCB* cb, void* data) {
  char msg[4096];
  struct tipc_hdr* hdr = (struct tipc_hdr *)msg;
  int rc;
  rc = read(fd, msg, HDR_LEN);
  if (rc < 0) {
    return rc;
  }
  info("%s rc=%d", __func__, rc);

  if (hdr->magic != TIPC_HDR_MAGIC) {
    set_errno(EBADMSG);
    error("IPC: codec error %d in %s:%d", errno, __func__, __LINE__);
    return -1;
  }

  rc = read(fd, msg + HDR_LEN, hdr->length - HDR_LEN);
  if (rc < 0) {
    return rc;
  }

  return tipc_decode(msg, hdr->length, cb, data);
}

int _API tipc_decode(char* buff, size_t nsize, TIPC_DecodeCB* cb, void* data) {
  struct tipc_hdr* msg_hdr;

  if (!cb) {
    set_errno(EINVAL);
    return -1;
  }

  MSG_DEC(buff, nsize) {
    CHECK_MSG();

    msg_hdr = MSG_HDR();
  }

  int rc = MSG_EC();
  if (rc < 0) {
    return rc;
  }

  return (*cb)(msg_hdr->type, buff, nsize, data);
}

// seed process informs turfd, he's ready for seed.
int _API tipc_enc_seed_ready(char* buff, size_t nsize) {
  MSG_ENC(buff, nsize) {
    PUT_HDR(TIPC_MSG_SEED_READY);

    CALC_HDR();
  }

  return MSG_LEN();
}

int _API tipc_dec_seed_ready(char* buff, size_t nsize) {
  MSG_DEC(buff, nsize) {
    CHECK_HDR(TIPC_MSG_SEED_READY);
  }
  return MSG_LEN();
}

/*
 * the msg used by turfd, send to seed process for clone request.
 */
int _API tipc_enc_fork_req(char* buff, size_t nsize, struct rlm_t* realm) {
  MSG_ENC(buff, nsize) {
    PUT_HDR(TIPC_MSG_FORK_REQ);
    PUT_4(realm->cfg.flags);
    PUT_4(realm->cfg.mode);

    PUT_4(realm->cfg.argc);
    for (int i = 0; i < realm->cfg.argc; i++) {
      PUT_STR(realm->cfg.argv[i]);
    }

    int env_size = sssize(realm->cfg.env);
    PUT_4(env_size);
    for (int i = 0; i < env_size; i++) {
      PUT_STR(realm->cfg.env[i]);
    }

    PUT_STR(realm->cfg.binary);
    PUT_STR(realm->cfg.name);

    PUT_4(realm->cfg.uid);
    PUT_4(realm->cfg.gid);
    dprint("uid, gid: %d, %d", realm->cfg.uid, realm->cfg.gid);

    PUT_STR(realm->cfg.chroot_dir);
    dprint("chroot: %s, _cur(%p)", realm->cfg.chroot_dir, _cur);
    PUT_STR(realm->cfg.fd_stdout);
    dprint("stdout: %s, _cur(%p)", realm->cfg.fd_stdout, _cur);
    PUT_STR(realm->cfg.fd_stderr);
    dprint("stderr: %s, _cur(%p)", realm->cfg.fd_stderr, _cur);

    CALC_HDR();
  }
  return MSG_LEN();
}

int _API tipc_dec_fork_req(struct rlm_t* realm, char* buff, size_t nsize) {
  MSG_DEC(buff, nsize) {
    CHECK_HDR(TIPC_MSG_FORK_REQ);
    GET_4(realm->cfg.flags);
    GET_4(realm->cfg.mode);

    // arguments
    GET_4(realm->cfg.argc);
    if (realm->cfg.argc > 0) {
      char** argv = (char**)calloc(realm->cfg.argc + 1, sizeof(char*));
      for (int i = 0; i < realm->cfg.argc; i++) {
        GET_STR(argv[i]);
      }
      realm->cfg.argv = argv;
    }

    int env_size;
    GET_4(env_size);
    if (env_size > 0) {
      char** env = (char**)calloc(env_size + 1, sizeof(char*));
      for (int i = 0; i < env_size; i++) {
        GET_STR(env[i]);
      }
      realm->cfg.env = env;
    }

    GET_STR(realm->cfg.binary);
    GET_STR(realm->cfg.name);

    GET_4(realm->cfg.uid);
    GET_4(realm->cfg.gid);
    warn("uid, gid: %d, %d", realm->cfg.uid, realm->cfg.gid);

    GET_STR(realm->cfg.chroot_dir);
    dprint("chroot: %s, _cur(%p)", realm->cfg.chroot_dir, _cur);
    GET_STR(realm->cfg.fd_stdout);
    dprint("stdout: %s, _cur(%p)", realm->cfg.fd_stdout, _cur);
    GET_STR(realm->cfg.fd_stderr);
    dprint("stderr: %s, _cur(%p)", realm->cfg.fd_stderr, _cur);
  }
  return MSG_LEN();
}

/*
 * the msg is used by seed process informing turfd fork results.
 */
int _API tipc_enc_fork_rsp(char* buff, size_t nsize, struct rlm_t* realm) {
  MSG_ENC(buff, nsize) {
    PUT_HDR(TIPC_MSG_FORK_RSP);
    PUT_4(realm->st.child_pid);
    PUT_STR(realm->cfg.name);
    CALC_HDR();
  }

  return MSG_LEN();
}

int _API tipc_dec_fork_rsp(struct rlm_t* realm, char* buff, size_t nsize) {
  MSG_DEC(buff, nsize) {
    CHECK_HDR(TIPC_MSG_FORK_RSP);
    GET_4(realm->st.child_pid);
    GET_STR(realm->cfg.name);
  }

  return MSG_LEN();
}
