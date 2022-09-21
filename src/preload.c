/* hook glibc by using LD_PRELOAD
 */

#define _GNU_SOURCE
#include <fcntl.h>
#ifdef __linux__
#include <link.h>
#include <warmfork.h>
#endif

static int (*real_main)(int, char**, char**);
static void* libc_handle = NULL;
static pid_t (*real_fork)(void);

static int my_proc_stat() {
  char buf[1024];
  char* array[64] = {0};
  int fd = open("/proc/self/stat", O_RDONLY | O_CLOEXEC);
  read(fd, buf, 1024);
  char* p;
  p = strtok(buf, " ");
  int i = 0;
  while (p) {
    if (i > 63) {
      break;
    }
    array[i++] = p;
    p = strtok(NULL, " ");
  }
  close(fd);

  struct {
    const char* name;
    int idx;
  } cl[] = {
      {"pid", 0},         {"cmd", 1},       {"state", 2},
      {"ppid", 3},        {"pgid", 4},      {"sid", 5},
      {"flags", 8},       {"min_flt", 9},   {"cmin_flt", 10},
      {"maj_flt", 11},    {"cmaj_flt", 12}, {"utime", 13},
      {"stime", 14},      {"cutime", 15},   {"cstime", 16},
      {"priority", 17},   {"nice", 18},     {"num_threads", 19},
      {"start_time", 21}, {"vsize", 22},    {"rss", 23},
      {"rsslimit", 24},
  };

  for (i = 0; i < ARRAY_SIZE(cl); i++) {
    printf("%s = %s\n", cl[i].name, array[cl[i].idx]);
  }

  return 0;
}

pid_t _API fork(void) {
  pid_t pid;
  printf("hook fork()\n");
  pid = real_fork();
  printf("fork() = %d\n", pid);
  return pid;
}

#if 0
static int
callback(struct dl_phdr_info *info, size_t size, void *data)
{
   const char *type;
   int p_type;

   printf("Name: \"%s\" (%d segments), base(%p)\n", info->dlpi_name,
              info->dlpi_phnum, info->dlpi_addr);

   for (int j = 0; j < info->dlpi_phnum; j++) {
       p_type = info->dlpi_phdr[j].p_type;
       type =  (p_type == PT_LOAD) ? "PT_LOAD" :
               (p_type == PT_DYNAMIC) ? "PT_DYNAMIC" :
               (p_type == PT_INTERP) ? "PT_INTERP" :
               (p_type == PT_NOTE) ? "PT_NOTE" :
               (p_type == PT_INTERP) ? "PT_INTERP" :
               (p_type == PT_PHDR) ? "PT_PHDR" :
               (p_type == PT_TLS) ? "PT_TLS" :
               (p_type == PT_GNU_EH_FRAME) ? "PT_GNU_EH_FRAME" :
               (p_type == PT_GNU_STACK) ? "PT_GNU_STACK" :
               (p_type == PT_GNU_RELRO) ? "PT_GNU_RELRO" : NULL;

       printf("    %2d: [%14p; memsz:%7jx] flags: %#jx; type: %d", j,
               (void *) (info->dlpi_addr + info->dlpi_phdr[j].p_vaddr),
               (uintmax_t) info->dlpi_phdr[j].p_memsz,
               (uintmax_t) info->dlpi_phdr[j].p_flags,
               info->dlpi_phdr[j].p_type);
       if (type != NULL)
           printf("%s\n", type);
       else
           printf("[other (%#x)]\n", p_type);
   }

   return 0;
}

void dummy_turf_fork_wait(int *rc, int *argc, char ***argv)
{
    printf("called from %s in libturf.so\n", __func__);
    *rc = 0;
    *argc = 0;
    *argv = 0;
}

static int load_turfphd_section()
{
    int self = open("/proc/self/exe", O_RDONLY);

    const ElfW(Ehdr) *ehdr = malloc(sizeof(ElfW(Ehdr)));
    int rc = read(self, ehdr, sizeof(ElfW(Ehdr)));
    assert(rc == sizeof(ElfW(Ehdr)));

    //printf("phdr: shdr(%p), cnt(%d), str(%d)\n", ehdr->e_shoff, ehdr->e_shnum, ehdr->e_shstrndx);

    const ElfW(Shdr) *shdr = malloc(sizeof(ElfW(Shdr)) * ehdr->e_shnum);
    rc = lseek(self, ehdr->e_shoff, SEEK_SET);
    assert (rc == ehdr->e_shoff);
    rc = read(self, shdr, sizeof(ElfW(Shdr))*ehdr->e_shnum); 
    assert (rc == (sizeof(ElfW(Shdr))*ehdr->e_shnum));

    const ElfW(Shdr) *secstr = shdr+ehdr->e_shstrndx;
    //printf("secstr: %p, %d\n", secstr->sh_offset, secstr->sh_size);
   
    char *ss = (char *)malloc(secstr->sh_size); 
    lseek(self, secstr->sh_offset, SEEK_SET);
    rc = read(self, ss, secstr->sh_size);

    const ElfW(Shdr) *s = shdr;
    for (int i=0; i<ehdr->e_shnum; i++) {
        if (strcmp(&ss[s->sh_name], ".turfphd") == 0) {
            //printf("sec: name(%s), type(%d), addr(%p)\n", &ss[s->sh_name], s->sh_type, s->sh_addr);
            
            void *thdr = malloc(s->sh_size);
            const ElfW(Nhdr) *nhdr = thdr, *next_nhdr = nhdr;
            lseek(self, s->sh_offset, SEEK_SET);
            rc = read(self, thdr, s->sh_offset);
            struct thdr {
                void *ptr;
                char *name[0]; 
            } *t;
            t = ((void *)nhdr) + sizeof(ElfW(Nhdr)) + nhdr->n_namesz;
            printf("api: name(%s), at(%p)\n", t->name, t->ptr);
            void **ptr = t->ptr;
            *ptr = dummy_turf_fork_wait;
        }
        s++;
    }

    close(self);
    return 0;
}
#endif

static int hook_main(int argc, char** argv, char** penv) {
  info("before main()");

  // TBD: register USER signal handler
  // dl_iterate_phdr(callback, NULL);
#ifdef __linux__
  load_turfphd();
#endif

  int rc = real_main(argc, argv, penv);
  info("after main()");

  // my_proc_stat();
  return rc;
}

/*
 * The main entry of libturf.so
 */
int _API __libc_start_main(int (*main)(int, char**, char**),
                           int argc,
                           char** ubp_av,
                           void (*init)(void),
                           void (*fini)(void),
                           void (*rtld_fini)(void),
                           void(*stack_end)) {
  printf("init(%p), fini(%p), rtld_fini(%p), stack_end(%p)\n",
         init,
         fini,
         rtld_fini,
         stack_end);
  void* sym;
  /*
   * This hack is unfortunately required by C99 - casting directly from
   * void* to function pointers is left undefined. See POSIX.1-2003, the
   * Rationale for the specification of dlsym(), and dlsym(3). This
   * deliberately violates strict-aliasing rules, but gcc can't tell.
   */
  union {
    int (*fn)(int (*main)(int, char**, char**),
              int argc,
              char** ubp_av,
              void (*init)(void),
              void (*fini)(void),
              void (*rtld_fini)(void),
              void(*stack_end));
    void* symval;
  } real_libc_start_main;

  /*
   * We hold this handle for the duration of the real __libc_start_main()
   * and drop it just before calling the real main().
   */
  libc_handle = dlopen("libc.so.6", RTLD_NOW);

  if (!libc_handle) {
    fatal("can't dlopen() libc");
    /*
     * We dare not use abort() here because it will run atexit()
     * handlers and try to flush stdio.
     */
    _exit(1);
  }
  sym = dlsym(libc_handle, "__libc_start_main");
  if (!sym) {
    fatal("can't find the real __libc_start_main()");
    _exit(1);
  }
  real_libc_start_main.symval = sym;
  real_main = main;

  sym = dlsym(libc_handle, "__libc_fork");
  if (!sym) {
    fatal("can't find the real fork()");
    _exit(1);
  }
  real_fork = (pid_t(*)())sym;
  // printf("%s: %s\n", __func__, (char *)0x7e00000000UL);
  /*
   * Note that we swap fake_main in for main - fake_main knows that it
   * should call real_main after it's done.
   */
  return real_libc_start_main.fn(
      hook_main, argc, ubp_av, init, fini, rtld_fini, stack_end);
}
