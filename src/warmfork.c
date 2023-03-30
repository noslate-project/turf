/*
 * TURF Warmfork
 */

#ifdef __linux__

#define _GNU_SOURCE
#include "warmfork.h"
#include <link.h>    // ElfW(x)
#include <string.h>  // strdup()
#include "sock.h"

static int _stop = 0;

// holds all turf-phds
#define MAX_PHD_SLOTS (32)
static struct tf_phd_slot m_phds[MAX_PHD_SLOTS];
static int m_phd_cnt = 0;
static int m_fd = -1;
static struct rlm_t m_rlm = {0};
static char* _elf_entry = 0;

// read elf offset
static size_t elf_read(int fd, size_t offset, void* buf, size_t size) {
  size_t l;

  dprint("%s(%d, %ld, %p, %ld)", __func__, fd, offset, buf, size);

  l = lseek(fd, offset, SEEK_SET);
  if (l != offset) {
    return -1;
  }

  l = read(fd, buf, size);
  if (l != size) {
    return -1;
  }

  return l;
}

static char* get_start() {
  int mf = open("/proc/self/maps", O_RDONLY);
  char buf[32];
  int l = read(mf, buf, 30);
  close(mf);

  for (int i = 0; i < l; i++) {
    if (buf[i] == '-') {
      buf[i] = 0;
    }
  }
  char* p;
  l = sscanf(buf, "%p", &p);
  return p;
}

// load turfphd section from elf
static int load_turfphd_section() {
  ElfW(Ehdr)* ehdr = NULL;  // elf hdr
  ElfW(Shdr)* shdr = NULL;  // sec hdr
  char* ss = NULL;          // sec str table
  void* tphd = NULL;        // turfphd sec

  int self = open("/proc/self/exe", O_RDONLY);

  // elf header
  size_t size = sizeof(ElfW(Ehdr));
  ehdr = (ElfW(Ehdr)*)malloc(size);
  size_t l = elf_read(self, 0, ehdr, size);
  if (l != size) {
    goto exit;
  }
  // dprint("phdr: shdr(%p), cnt(%d), str(%d)", ehdr->e_shoff, ehdr->e_shnum,
  // ehdr->e_shstrndx);
  if (ehdr->e_type == 0x3) {  // 2:EXEC, 3:DYN
    _elf_entry = get_start();
  }

  // section header
  size = sizeof(ElfW(Shdr)) * ehdr->e_shnum;
  shdr = (ElfW(Shdr)*)malloc(size);
  l = elf_read(self, ehdr->e_shoff, shdr, size);
  if (l != size) {
    goto exit;
  }

  // section name string table
  const ElfW(Shdr)* secstr = shdr + ehdr->e_shstrndx;
  size = secstr->sh_size;
  // dprint("secstr: %p, %d", secstr->sh_offset, size);
  ss = (char*)malloc(size);
  l = elf_read(self, secstr->sh_offset, ss, secstr->sh_size);
  if (l != size) {
    goto exit;
  }

  // walk all section, to find "turfphd"
  const ElfW(Shdr)* s = shdr;
  for (int i = 0; i < ehdr->e_shnum; i++) {
    if (strcmp(&ss[s->sh_name], _TPHD_SEC_NAME) == 0) {
      // dprint("sec: name(%s), type(%d), addr(%p), size(%d)",
      //     &ss[s->sh_name], s->sh_type, s->sh_addr, s->sh_size);

      // read out whole section
      size = s->sh_size;
      tphd = malloc(size);
      l = elf_read(self, s->sh_offset, tphd, s->sh_size);
      if (l != size) {
        goto exit;
      }

      break;
    }
    s++;
  }

  for (size_t i = 0; tphd && i < s->sh_size;) {
    ElfW(Nhdr)* n = (ElfW(Nhdr)*)((char*)tphd + i);
    // printf("note: namesz(%d), descsz(%d), type(%d)\n", n->n_namesz,
    // n->n_descsz, n->n_type);
    int hdr_size = sizeof(ElfW(Nhdr)) + n->n_namesz;
    struct tphd_desc* t = (struct tphd_desc*)((char*)n + hdr_size);
    // printf("api: name(%s), at(%p)\n", t->name, t->pc);

    // save the note slot
    struct tf_phd_slot slot;
    slot.pfn = (void**)t->pc;
    slot.name = strdup(t->name);
    m_phds[m_phd_cnt++] = slot;
    if (m_phd_cnt >= MAX_PHD_SLOTS) {
      break;
    }

    i += MEM_ALIGN(hdr_size + n->n_descsz, 4);
  }

exit:
  if (self > 0) {
    close(self);
  }
  if (tphd) {
    free(tphd);
  }
  if (ss) {
    free(ss);
  }
  if (shdr) {
    free(shdr);
  }
  if (ehdr) {
    free(ehdr);
  }
  return 0;
}

// get TURFPHD_FD
static int env_phdfd() {
  char* sz = getenv("TURFPHD_FD");
  if (!sz) {
    warn("TURFPHD_FD is not defined.");
    return -1;
  }

  int fd = atoi(sz);
  if (fd > 2) {
    dprint("TURFPHD_FD=%d", fd);
    return fd;
  }
  return -1;
}

// load turfd cfg
static int load_cfg() {
  m_fd = env_phdfd();
  if (m_fd < 0) {
    error("can't get turfphd fd.");
    return -1;
  }

  // struct sck_loop *loop = sck_default_loop();
  // sck_create_event(loop, m_fd, SCK_READ, on_read, NULL);

  return 0;
}

/*
 * Begin of APIs
 */
void turf_warmfork_exit() {
  _stop = 1;
}

static int twf_inform_seed_ready() {
  char buf[128];

  int len = tipc_enc_seed_ready(buf, 128);
  dprint("tipc msg size: %d", len);

  int l = write(m_fd, buf, len);
  if (l != len) {
    error("send ready failed");
    return -1;
  }

  return 0;
}

static int twf_inform_fork_rsp() {
  char buf[4096];

  int len = tipc_enc_fork_rsp(buf, 4096, &m_rlm);
  dprint("tipc msg size: %d", len);

  int l = write(m_fd, buf, len);
  if (l != len) {
    error("send ready failed");
    return -1;
  }

  return 0;
}

static void dummy_warmfork_wait(int* prc, int* pargc, char*** pargv) {
  static char* av[] = {"test", "hello", "world"};

  printf("called from %s in libturf.so\n", __func__);

  if (!prc) {
    return;
  }

  *prc = 0;

  if (pargc) {
    *pargc = 3;
  }

  if (pargv) {
    *pargv = av;
  }
}

int twf_msg(int type, char* buff, size_t size, void* data) {
  int rc = 0;
  switch (type) {
    case TIPC_MSG_FORK_REQ: {  // fork req
      rc = tipc_dec_fork_req(&m_rlm, buff, size);
      if (rc < 0) {
        return rc;
      }

      pid_t child = rlm_fork(&m_rlm);
      dprint("rlm_fork = %d %s", child, m_rlm.cfg.name);
      if (child) {  // parent
        twf_inform_fork_rsp();
        rlm_free_inner(&m_rlm);
      } else {
        // the rc breaks the loop
        return -999;
      }
    }
  }

  return 0;
}

static void turf_warmfork_wait(int* prc, int* pargc, char*** pargv) {
  // inform turfd we're ready for fork.
  twf_inform_seed_ready();

  // the loop
  while (1) {
    int rc = tipc_read(m_fd, twf_msg, NULL);
    info("%s rc=%d", __func__, rc);

    // break in child process
    if (rc == -999) {
      dprint("in child break");
      break;
    }
  }

  *prc = 0;
  *pargc = m_rlm.cfg.argc;
  *pargv = m_rlm.cfg.argv;
}

static void** reloc_pfn(void* pfn) {
  return (void**)(_elf_entry + (uint64_t)pfn);
}

/*
 * End of APIs
 */

// keep in last of source
static int turfphd_inject() {
  int i;
  for (i = 0; i < m_phd_cnt; i++) {
    if (strcmp("turf_fork_wait", m_phds[i].name) == 0) {
      *reloc_pfn(m_phds[i].pfn) = (void*)turf_warmfork_wait;
      info("turfphd slot %s enabled.", m_phds[i].name);
    } else {
      warn("turfphd slot %s not found.", m_phds[i].name);
    }
  }

  info("turfphd enabled.");
  return 0;
}

// use libturf.so for dummy testing
static int turfphd_dummy_inject() {
  int i;
  for (i = 0; i < m_phd_cnt; i++) {
    if (strcmp("turf_fork_wait", m_phds[i].name) == 0) {
      *reloc_pfn(m_phds[i].pfn) = (void*)dummy_warmfork_wait;
      info("turfphd slot %s enabled.", m_phds[i].name);
    } else {
      warn("turfphd slot %s not found.", m_phds[i].name);
    }
  }

  info("turfphd dummy enabled (for test).");
  return 0;
}

// API for load turf PHD
int _API load_turfphd(void) {
  int rc;

  // load turf phd slots
  load_turfphd_section();

  // load turfd cfg
  rc = load_cfg();
  if (rc < 0) {
    warn("warmfork is not loaded.");

    // load dummy slots
    turfphd_dummy_inject();
    return -1;
  }

  // inject real turf to phd
  turfphd_inject();

  return 0;
}

#endif  // __linux__
