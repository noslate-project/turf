#ifndef _TURF_SOCK_H_
#define _TURF_SOCK_H_

#include "misc.h"

#include <poll.h>
#include <time.h>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>

typedef uint64_t sck_tick;      // monotonic ticks, default in microseconds(US)
typedef uint64_t sck_timer_id;  // unified timer id

struct sck_loop;
struct sck_poll_state;

#define SCK_TYPE_UNIX 1
#define SCK_TYPE_TCP 2

#define SCK_READ 1
#define SCK_WRITE 2
#define SCK_RW SCK_READ | SCK_WRITE
#define SCK_ID_DELETED ((sck_timer_id)(-1))

typedef void sck_file_callback(struct sck_loop* loop,
                               int fd,
                               void* userdata,
                               int mask);
typedef int sck_timer_callback(struct sck_loop* loop,
                               sck_timer_id id,
                               void* userdata);

struct sck_evfired {
  int fd;    // which fd has events.
  int mask;  // what kind of event the fd has.
};

struct sck_evfile {
  int mask;
  sck_file_callback* read_proc;
  sck_file_callback* write_proc;
  void* userdata;
};

struct sck_evtimer {
  TAILQ_ENTRY(sck_evtimer) in_list;
  sck_timer_id id;  // birth tick
  sck_tick when;    // pop time
  int refcnt;       // recursive reference count
  sck_timer_callback* timer_proc;
  void* userdata;
};

struct sck_loop {
  int maxfd;
  int setsize;

  struct sck_evfile* events;  // holds registered file events.
  TAILQ_HEAD(sck_evtimer_list, sck_evtimer) timer_head;  // holds all timers
  sck_timer_id timer_id_next;

  struct sck_poll_state* poll_state;  // holds the poll states
  struct sck_evfired* fired;          // holds the poll fired event fds;
};

extern sck_tick (*get_tick_us)(void);

struct sck_loop _API* sck_loop_create(int setsize);
struct sck_loop _API* sck_default_loop();

bool _API sck_loop_alive(struct sck_loop* loop);

int _API sck_create_event(struct sck_loop* loop,
                          int fd,
                          int mask,
                          sck_file_callback* proc,
                          void* userdata);
int _API sck_delete_event(struct sck_loop* loop, int fd, int mask);
sck_timer_id _API sck_create_timer(struct sck_loop* loop,
                                   sck_tick ms,
                                   sck_timer_callback* proc,
                                   void* userdata);
int _API sck_delete_timer(struct sck_loop* loop, sck_timer_id id);
int _API sck_process_events(struct sck_loop* loop);

int _API sck_unix_socket(void);
int _API sck_unix_listen(int sfd, const char* path, mode_t perm, int backlog);
int _API sck_unix_accept(int sfd);
int _API sck_unix_connect(int fd, const char* path);

int _API sck_read(int fd, char* buf, int count);
int _API sck_write(int fd, char* buf, int count);

int _API sck_pair_unix_socket(int sv[2]);
#endif  // _TURF_SOCK_H_
