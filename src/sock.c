/* supports socket and epoll
 */

#include "sock.h"

/* using EPOLL implementation for linux platform.
 * and SELECT for other platform, like macos.
 */
#if defined(__linux__)
#define HAVE_EPOLL
#else
#define HAVE_SELECT
#endif

/* provide time ticks
 */
sck_tick (*get_tick_us)(void) = NULL;

sck_tick get_tick_us_posix(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  sck_tick tick = ((uint64_t)ts.tv_sec) * 1000000 + ts.tv_nsec / 1000;
  // dprint("tick: %llu", tick);
  return tick;
}

static int sck_tick_init(void) {
  if (!get_tick_us) {
    get_tick_us = get_tick_us_posix;
  }
  return 0;
}

/* socket core
 */
static int sck_set_nonblock(int fd) {
  int flags;

  if ((flags = fcntl(fd, F_GETFL)) == -1) {
    return -1;
  }

  flags |= O_NONBLOCK;

  if (fcntl(fd, F_SETFL, flags) == -1) {
    return -1;
  }

  return 0;
}

static int sck_set_sndbuf(int fd, size_t size) {
  int rc;
  rc = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
  if (rc < 0) {
    return -1;
  }
  return 0;
}

static int sck_set_rcvbuf(int fd, size_t size) {
  int rc;
  rc = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
  if (rc < 0) {
    return -1;
  }
  return 0;
}

static int sck_set_timeo(int fd, int opt, uint64_t ms) {
  int rc;
  struct timeval tv;

  tv.tv_sec = ms / 1000;
  tv.tv_usec = (ms % 1000) * 1000;
  rc = setsockopt(fd, SOL_SOCKET, opt, &tv, sizeof(tv));
  if (rc < 0) {
    return -1;
  }
  return 0;
}

static int sck_set_sndtimeo(int fd, long long ms) {
  return sck_set_timeo(fd, SO_SNDTIMEO, ms);
}

static int sck_set_rcvtimeo(int fd, long long ms) {
  return sck_set_timeo(fd, SO_RCVTIMEO, ms);
}

#if defined(__APPLE__)
static int sck_set_nosigpipe(int fd) {
  int rc;
  int yes = 1;
  rc = setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &yes, sizeof(yes));
  if (rc < 0) {
    return -1;
  }
  return 0;
}
#endif

int _API sck_unix_socket() {
  int fd = 0;
  int rc = 0;
  int yes = 1;

  rc = socket(AF_LOCAL, SOCK_STREAM, 0);
  if (rc < 0) {
    goto exit;
  }
  fd = rc;

  rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if (rc < 0) {
    goto exit;
  }

  rc = sck_set_nonblock(fd);
  if (rc < 0) {
    goto exit;
  }

  return fd;

exit:
  warn("socket");
  if (fd > 0) {
    close(fd);
  }
  return rc;
}

int sck_unix_listen(int sfd, const char* path, mode_t perm, int backlog) {
  int rc = 0;
  struct sockaddr_un sa;

  memset(&sa, 0, sizeof(sa));
  sa.sun_family = AF_LOCAL;
  strncpy(sa.sun_path, path, sizeof(sa.sun_path) - 1);
  rc = bind(sfd, (struct sockaddr*)&sa, sizeof(sa));
  if (rc < 0) {
    return rc;
  }

  rc = listen(sfd, backlog);
  if (rc < 0) {
    return rc;
  }

  if (perm) {
    chmod(sa.sun_path, perm);
  }
  return 0;
}

int sck_unix_connect(int fd, const char* path) {
  int rc = 0;
  struct sockaddr_un sa;

  sa.sun_family = AF_LOCAL;
  strncpy(sa.sun_path, path, sizeof(sa.sun_path) - 1);
  rc = connect(fd, (struct sockaddr*)&sa, sizeof(sa));
  if (rc < 0) {
    return -1;
  }
  return 0;
}

int sck_unix_accept(int sfd) {
  int fd = -1;
  int rc = 0;
  struct sockaddr_un sa;
  socklen_t salen = sizeof(sa);

  int i;
  for (i = 0; i < 10; i++) {  // retry
    fd = accept(sfd, (struct sockaddr*)&sa, &salen);
    if (fd == -1) {
      if (errno == EINTR)
        continue;
      else {
        break;
      }
    }
    break;
  }

  if (fd < 0) {
    warn("accept");
    return fd;
  }

  rc = sck_set_nonblock(fd);
  if (rc < 0) {
    goto error;
  }

  return fd;

error:
  if (fd > 0) {
    close(fd);
  }
  return rc;
}

#if defined(HAVE_EPOLL)
/* for linux supports EPOLL
 */
#include <sys/epoll.h>
struct sck_poll_state {
  int epfd;                    // epoll fd
  struct epoll_event* events;  // hold events
};

static int sck_poll_new(struct sck_loop* loop) {
  int rc = 0;
  struct sck_poll_state* poll =
      (struct sck_poll_state*)calloc(1, sizeof(struct sck_poll_state));
  if (!poll) {
    set_errno(ENOMEM);
    rc = -1;
    goto error;
  }
  poll->epfd = epoll_create(1024);  // size is ignored but must >0.
  if (poll->epfd < 0) {
    rc = -1;
    goto error;
  }

  poll->events =
      (struct epoll_event*)calloc(loop->setsize, sizeof(struct epoll_event));
  if (!poll->events) {
    set_errno(ENOMEM);
    rc = -1;
    goto error;
  }

  loop->poll_state = poll;
  return 0;

error:
  if (poll) {
    if (poll->epfd) {
      close(poll->epfd);
    }
    free(poll);
  }
  return rc;
}

static int sck_poll_add(struct sck_loop* loop, int fd, int mask) {
  struct sck_poll_state* poll = loop->poll_state;
  struct epoll_event ee = {0};

  int op = loop->events[fd].mask == 0 ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;

  ee.events = 0;
  mask |= loop->events[fd].mask;
  if (mask & SCK_READ) {
    ee.events |= EPOLLIN;
  }
  if (mask & SCK_WRITE) {
    ee.events |= EPOLLOUT;
  }
  ee.data.fd = fd;

  if (epoll_ctl(poll->epfd, op, fd, &ee) == -1) {
    return -1;
  }
  return 0;
}

static int sck_poll_del(struct sck_loop* loop, int fd, int delmask) {
  int rc = 0;
  struct epoll_event ee = {0};
  struct sck_poll_state* poll = loop->poll_state;
  int mask = loop->events[fd].mask & (~delmask);

  ee.events = 0;
  if (mask & SCK_READ) {
    ee.events |= EPOLLIN;
  }
  if (mask & SCK_WRITE) {
    ee.events |= EPOLLOUT;
  }
  ee.data.fd = fd;
  if (mask != 0) {
    rc = epoll_ctl(poll->epfd, EPOLL_CTL_MOD, fd, &ee);
  } else {
    rc = epoll_ctl(poll->epfd, EPOLL_CTL_DEL, fd, &ee);
  }
  return rc;
}

static int sck_poll(struct sck_loop* loop, struct timeval* tv) {
  struct sck_poll_state* poll = loop->poll_state;
  int rc, numevents = 0;

  rc = epoll_wait(poll->epfd,
                  poll->events,
                  loop->setsize,
                  tv ? (tv->tv_sec * 1000 + tv->tv_usec / 1000) : -1);
  if (rc > 0) {
    int i;

    numevents = rc;
    for (i = 0; i < numevents; i++) {
      int mask = 0;
      struct epoll_event* e = &poll->events[i];

      if (e->events & EPOLLIN) mask |= SCK_READ;
      if (e->events & EPOLLOUT) mask |= SCK_WRITE;
      if (e->events & EPOLLERR) mask |= SCK_READ | SCK_WRITE;
      if (e->events & EPOLLHUP) mask |= SCK_READ | SCK_WRITE;

      loop->fired[i].fd = e->data.fd;
      loop->fired[i].mask = mask;
    }
  }
  return numevents;
}
#elif defined(HAVE_SELECT)
#include <sys/select.h>
/* for platform supports SELECT
 */
struct sck_poll_state {
  fd_set rfds, wfds;
  /* We need to have a copy of the fd sets as it's not safe to reuse
   * FD sets after select(). */
  fd_set _rfds, _wfds;
} aeApiState;

static int sck_poll_new(struct sck_loop* loop) {
  struct sck_poll_state* state = calloc(1, sizeof(struct sck_poll_state));
  if (!state) {
    set_errno(ENOMEM);
    return -1;
  }

  FD_ZERO(&state->rfds);
  FD_ZERO(&state->wfds);
  loop->poll_state = state;

  return 0;
}

static int sck_poll_add(struct sck_loop* loop, int fd, int mask) {
  struct sck_poll_state* state = loop->poll_state;

  if (mask & SCK_READ) {
    FD_SET(fd, &state->rfds);
  }
  if (mask & SCK_WRITE) {
    FD_SET(fd, &state->wfds);
  }
  return 0;
}

static int sck_poll_del(struct sck_loop* loop, int fd, int mask) {
  struct sck_poll_state* state = loop->poll_state;

  if (mask & SCK_READ) {
    FD_CLR(fd, &state->rfds);
  }
  if (mask & SCK_WRITE) {
    FD_CLR(fd, &state->wfds);
  }
  return 0;
}

static int sck_poll(struct sck_loop* loop, struct timeval* tvp) {
  struct sck_poll_state* state = loop->poll_state;
  int retval, j, numevents = 0;

  memcpy(&state->_rfds, &state->rfds, sizeof(fd_set));
  memcpy(&state->_wfds, &state->wfds, sizeof(fd_set));

  retval = select(loop->maxfd + 1, &state->_rfds, &state->_wfds, NULL, tvp);
  if (retval > 0) {
    for (j = 0; j <= loop->maxfd; j++) {
      int mask = 0;
      struct sck_evfile* fe = &loop->events[j];

      if (fe->mask == 0) {
        continue;
      }
      if (fe->mask & SCK_READ && FD_ISSET(j, &state->_rfds)) {
        mask |= SCK_READ;
      }
      if (fe->mask & SCK_WRITE && FD_ISSET(j, &state->_wfds)) {
        mask |= SCK_WRITE;
      }
      loop->fired[numevents].fd = j;
      loop->fired[numevents].mask = mask;
      numevents++;
    }
  }
  return numevents;
}
#endif

static int64_t sck_calc_pop_time(struct sck_loop* loop) {
  struct sck_evtimer* te = NULL;
  struct sck_evtimer* earliest = NULL;

  if (TAILQ_EMPTY(&loop->timer_head)) {
    return -1;
  }

  TAILQ_FOREACH(te, &loop->timer_head, in_list) {
    if (!earliest || (te->when < earliest->when)) {
      earliest = te;
    }
  }

  sck_tick now = get_tick_us();
  if (now >= earliest->when) {
    return 0;
  }

  return (earliest->when - now) / 1000;
}

static int sck_process_timers(struct sck_loop* loop) {
  int processed = 0;
  struct sck_evtimer *te = NULL, *te_save = NULL;

  sck_timer_id cur_id = loop->timer_id_next - 1;
  sck_tick now = get_tick_us();

  TAILQ_FOREACH_SAFE(te, &loop->timer_head, in_list, te_save) {
    // process deleted timer
    if (te->id == SCK_ID_DELETED) {
      if (te->refcnt) {  // in reference
        dprint("refcnt %d", te->refcnt);
        continue;
      }

      // free the timer
      TAILQ_REMOVE(&loop->timer_head, te, in_list);
      free(te);
      dprint("free %ld", te->id);
      continue;
    }

    // skip iteration timers, what we created in timer_proc()
    if (te->id > cur_id) {
      continue;
    }

    // now we pop expired timer
    if (te->when <= now) {
      sck_timer_id id = te->id;

      te->refcnt++;
      int rc = te->timer_proc(loop, id, te->userdata);
      te->refcnt--;

      processed++;

      /* perid timer based on the return code(in ms in future).
       */
      now = get_tick_us();
      if (rc >= 0) {
        te->when = now + (rc * 1000);
      } else {
        te->id = SCK_ID_DELETED;
      }
    }
  }

  return processed;
}

int sck_process_events(struct sck_loop* loop) {
  int i;
  int processed = 0;
  int num_events = 0;

  struct timeval tv = {0};
  int64_t wait_time = sck_calc_pop_time(loop);
  if (wait_time >= 0) {
    tv.tv_sec = wait_time / 1000;
    tv.tv_usec = (wait_time % 1000) * 1000;
  } else {
    tv.tv_sec = 1;
    tv.tv_usec = 0;
  }
  // dprint("wati_time: %ld", wait_time);

  // poll events with timeout
  num_events = sck_poll(loop, &tv);

  // process file events
  for (i = 0; i < num_events; i++) {
    int fd = loop->fired[i].fd;
    int mask = loop->fired[i].mask;
    struct sck_evfile* fe = &loop->events[fd];

    if (fe->mask & mask & SCK_READ) {
      fe->read_proc(loop, fd, fe->userdata, mask);
    }
    if (fe->mask & mask & SCK_WRITE) {
      fe->write_proc(loop, fd, fe->userdata, mask);
    }
    processed++;
  }

  // process all timers
  processed += sck_process_timers(loop);

  return processed;
}

/*
 * begin of API
 */
// create a socket loop, with fixed size.
struct sck_loop _API* sck_loop_create(int setsize) {
  int rc = 0;

  rc = sck_tick_init();
  if (rc < 0) {
    return NULL;
  }

  struct sck_loop* loop = (struct sck_loop*)calloc(1, sizeof(struct sck_loop));
  if (!loop) {
    return NULL;
  }

  loop->setsize = setsize;
  loop->maxfd = -1;
  TAILQ_INIT(&loop->timer_head);

  loop->events = (struct sck_evfile*)calloc(setsize, sizeof(struct sck_evfile));
  if (!loop->events) {
    goto error;
  }

  loop->fired =
      (struct sck_evfired*)calloc(setsize, sizeof(struct sck_evfired));
  if (!loop->fired) {
    goto error;
  }

  rc = sck_poll_new(loop);
  if (rc < 0) {
    goto error;
  }

  return loop;

error:
  if (loop) {
    if (loop->events) {
      free(loop->events);
    }
    if (loop->fired) {
      free(loop->fired);
    }
  }
  return NULL;
}

// return true if loop is not empty.
bool sck_loop_alive(struct sck_loop* loop) {
  int i;
  if (!TAILQ_EMPTY(&loop->timer_head)) {
    return 1;
  }
  for (i = loop->maxfd; i >= 0; i--) {
    if (loop->events[i].mask != 0) {
      return 1;
    }
  }
  return 0;
}

// create fd events
int sck_create_event(struct sck_loop* loop,
                     int fd,
                     int mask,
                     sck_file_callback* proc,
                     void* userdata) {
  dprint("%s(%p, %d, %x, %p)", __func__, loop, fd, mask, proc);
  int rc = 0;
  if (!loop || fd >= loop->setsize) {
    set_errno(EINVAL);
    return -1;
  }

  struct sck_evfile* fe = &loop->events[fd];
  rc = sck_poll_add(loop, fd, mask);
  if (rc < 0) {
    error("poll add failed %d", rc);
    return rc;
  }
  fe->mask |= mask;
  if (mask & SCK_READ) fe->read_proc = proc;
  if (mask & SCK_WRITE) fe->write_proc = proc;
  fe->userdata = userdata;

  // update maxfd
  if (fd > loop->maxfd) {
    loop->maxfd = fd;
  }
  return 0;
}

// delete fd events
int sck_delete_event(struct sck_loop* loop, int fd, int mask) {
  dprint("%s(%p, %d, %x)", __func__, loop, fd, mask);
  int rc = 0;
  if (!loop || fd >= loop->setsize) {
    set_errno(EINVAL);
    return -1;
  }

  struct sck_evfile* fe = &loop->events[fd];
  if (fe->mask == 0) {
    set_errno(ENOENT);
    return -1;
  }

  rc = sck_poll_del(loop, fd, mask);
  if (rc < 0) {
    error("poll del failed %d", rc);
    return rc;
  }
  fe->mask &= (~mask);

  // update maxfd
  if (fd == loop->maxfd && fe->mask == 0) {
    int i;
    for (i = loop->maxfd; i >= 0; i--) {
      if (loop->events[i].mask != 0) {
        break;
      }
    }
    loop->maxfd = i;
  }
  return 0;
}

// create timer
sck_timer_id sck_create_timer(struct sck_loop* loop,
                              sck_tick ms,
                              sck_timer_callback* proc,
                              void* userdata) {
  sck_tick tick = get_tick_us();
  struct sck_evtimer* te =
      (struct sck_evtimer*)calloc(1, sizeof(struct sck_evtimer));
  if (!te) {
    set_errno(ENOMEM);
    return -1;
  }
  te->id = loop->timer_id_next++;
  te->when = tick + (ms * 1000);
  te->timer_proc = proc;
  te->userdata = userdata;
  TAILQ_INSERT_TAIL(&loop->timer_head, te, in_list);
  return te->id;
}

// delete timer
int sck_delete_timer(struct sck_loop* loop, sck_timer_id id) {
  struct sck_evtimer* te = NULL;
  TAILQ_FOREACH(te, &loop->timer_head, in_list) {
    if (te->id == id) {  // flag it to be freed
      te->id = SCK_ID_DELETED;
      return 0;
    }
  }
  set_errno(ENOENT);
  return -1;
}

int _API sck_read(int fd, char* buf, int count) {
  ssize_t nread, totlen = 0;
  while (totlen != count) {
    nread = read(fd, buf, count - totlen);
    if (nread == 0) return totlen;
    if (nread == -1) {
      dprint("sck_read failed: errno(%d)", errno);
      return (totlen > 0) ? totlen : -1;
    }
    totlen += nread;
    buf += nread;
  }
  return totlen;
}

int _API sck_write(int fd, char* buf, int count) {
  ssize_t nwritten, totlen = 0;
  while (totlen != count) {
    nwritten = write(fd, buf, count - totlen);
    if (nwritten == 0) return totlen;
    if (nwritten == -1) return -1;
    totlen += nwritten;
    buf += nwritten;
  }
  return totlen;
}

int _API sck_pair_unix_socket(int sv[2]) {
  int fds[2] = {0};
  int rc = 0;

  rc = socketpair(AF_UNIX, SOCK_STREAM, 0, fds);
  if (rc < 0) {
    goto exit;
  }

  rc = sck_set_nonblock(fds[0]);
  if (rc < 0) {
    goto exit;
  }

#if 0  // client socket keeps block
    rc = sck_set_nonblock(fds[1]);
    if (rc < 0) {
        goto exit;
    }
#endif

  sv[0] = fds[0];
  sv[1] = fds[1];
  return rc;

exit:
  warn("socketpair");
  if (fds[0] > 0) {
    close(fds[0]);
  }
  if (fds[1] > 0) {
    close(fds[1]);
  }
  return rc;
}

struct sck_loop _API* sck_default_loop() {
  static struct sck_loop* loop = NULL;

  if (loop) {
    return loop;
  }

  loop = sck_loop_create(1024);
  return loop;
}
