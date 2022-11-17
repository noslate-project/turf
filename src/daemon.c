/* daemon support
 *   with client, mainloop
 */
#include "daemon.h"
#include "cli.h"
#include "realm.h"
#include "shell.h"
#include "sock.h"
#include "turf.h"

#define DAEMON_DEFAULT_BACKLOG (1024)

static const char* sck_path = NULL;  // turf used unix socket path
static struct sck_loop* loop;        // socket loop
static int tfd_fd;                   // daemon listen socket fd
static struct tf_cli cli;            // hold the cli cfg

/* signal handler
 */
void handle_child_exit(int sig) {
  int status;
  struct rusage ru;

  while (1) {
    pid_t p = wait4(-1, &status, WNOHANG, &ru);
    if (p < 0) {
      if (errno == EINTR) {  // interrupted
        continue;
      }

      // break on everything
      break;
    } else if (p == 0) {
      // no more child exit
      break;
    } else {
      // normal procedure
      warn("SIGCHLD: pid=%d, status=%x", p, status);
      rusage_dump(&ru);
      tf_child_exit(p, &ru, status);
    }
  }
}

void handle_graceful_shutdown(int sig) {
  warn("SIGTERM");
  tf_exit_all();
}

int register_sighandler() {
  // capture the child exit event.
  signal(SIGCHLD, handle_child_exit);

  // capture the terminate signal and do graceful shutdown.
  signal(SIGTERM, handle_graceful_shutdown);

  // client may disconnect anytime, ignore the PIPE signal.
  signal(SIGPIPE, SIG_IGN);

  return 0;
}

/* turf daemon events
 */
static void daemon_on_write(struct sck_loop* loop,
                            int fd,
                            void* data,
                            int mask) {
  struct msg_hdr* hdr = (struct msg_hdr*)data;
  int rc = sck_write(fd, (char*)hdr, sizeof(*hdr));
  if (rc != sizeof(*hdr)) {
    warn("write failed (%d)", rc);
  }
  dprint("daemon_write(%d, %d)", fd, rc);
  rc = sck_delete_event(loop, fd, SCK_WRITE);
  if (rc) {
    error("delete event");
  }
  free(hdr);
}

static void daemon_on_read(struct sck_loop* loop,
                           int fd,
                           void* data,
                           int mask) {
  int rc = 0;
  struct msg_hdr hdr;
  rc = sck_read(fd, (char*)&hdr, sizeof(hdr));
  if (rc <= 0) {
    warn("Client disconnected(%d)", fd);
    goto exit;
  } else if (rc != sizeof(hdr)) {
    dprint("msg size mismatch (%d != %d)", (int)sizeof(hdr), rc);
    goto exit;
  }

  if (hdr.msg_type != T_MSG_CLI_REQ) {
    dprint("bad req (%d) from (%d)", hdr.msg_type, fd);
    goto exit;
  }

  if (hdr.msg_size < 1) {
    dprint("msg size error(%d), from (%d)", hdr.msg_size, fd);
    goto exit;
  }
  if (hdr.msg_size > 1024) {
    dprint("msg size too large (%d), from (%d)", hdr.msg_size, fd);
    goto exit;
  }

  int size = hdr.msg_size;
  char* msg = (char*)calloc(1, size);
  rc = sck_read(fd, msg, size);
  if (rc != size) {
    dprint("read failed, size (%d) rc (%d)", rc, size);
    goto exit;
  }

  // TBD: size 30 hard coded
  int argc = 0;
  char** argv = calloc(30, sizeof(char*));

  int i;
  argv[argc++] = "turf";
  argv[argc++] = msg;
  for (i = 0; i < size - 1; i++) {
    if (msg[i] == 0) {
      argv[argc++] = &msg[i + 1];
    }
  }

#if 0
  for (i = 0; i < argc; i++) {
    dprint("%s", argv[i]);
  }
#endif

  tf_cli cli = {0};
  rc = cli_parse_remote(&cli, argc, argv);
  if (rc == 0) {
    rc = tf_action(&cli);
  } else {
    error("remote command parse failed %d", rc);
  }

  // free resources.
  cli_free(&cli);
  if (argv) {
    free(argv);
  }
  if (msg) {
    free(msg);
  }

  if (rc != 0) {
    rc = -errno;
  }

  {
    struct msg_hdr* hdr = (struct msg_hdr*)calloc(1, sizeof(struct msg_hdr));
    hdr->hdr_magic = MSG_HDR_MAGIC;
    hdr->msg_type = T_MSG_CLI_RSP;
    hdr->msg_code = rc;
    hdr->msg_size = 0;
    sck_create_event(loop, fd, SCK_WRITE, daemon_on_write, hdr);
    // hdr will be freed in daemon_on_write().
  }

  // good return
  return;

exit:
  // close fd when error
  if (fd > 0) {
    sck_delete_event(loop, fd, SCK_RW);
    close(fd);
    dprint("fd %d closed", fd);
  }
}

static void daemon_on_accept(struct sck_loop* loop,
                             int fd,
                             void* clientdata,
                             int mask) {
  int client_fd = sck_unix_accept(fd);
  if (client_fd < 0) {
    die("accept");
  }
  dprint("accepted");

  int rc;
  rc = sck_create_event(loop, client_fd, SCK_READ, daemon_on_read, NULL);
  assert(!rc);
}

int daemon_start() {
  int rc;
  chdir(tfd_path());

  unlink(sck_path);
  tfd_fd = sck_unix_socket();
  if (!tfd_fd) {
    die("daemon socket");
  }
  rc = sck_unix_listen(tfd_fd, sck_path, 0660, 1024);
  if (rc < 0) {
    die("listen");
  }

  sck_create_event(loop, tfd_fd, SCK_READ, daemon_on_accept, NULL);
  return 0;
}

/* daemon actions
 */
static int daemon_health_check(struct sck_loop* loop,
                               sck_timer_id id,
                               void* clientData) {
  // sck_tick tick = get_tick_us();

  tf_health_check();

  // return next tick in ms, or AE_NOMORE terminate the timer.
  return 1000;  // AE_NOMORE;
}

#if defined(__linux__)
// callback on SIGCHLD
static void signalfd_on_read(struct sck_loop* loop,
                             int fd,
                             void* clientData,
                             int mask) {
  struct signalfd_siginfo siginfo;
  int rc;
  rc = sck_read(fd, (char*)&siginfo, sizeof(siginfo));
  if (rc <= 0) {
    warn("signalfd return %d", rc);
  }
  info("sig: %d, pid: %d", siginfo.ssi_signo, siginfo.ssi_pid);
  handle_child_exit(0);
}

// receive SIGCHLD through signalfd
static void daemon_signalfd() {
  int rc;
  int sigfd;

  sigset_t mask;
  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  rc = sigprocmask(SIG_BLOCK, &mask, NULL);
  if (rc < 0) {
    die("mask SIGCHLD failed.");
  }
  sigfd = signalfd(-1, &mask, 0);

  if (sigfd <= 0) {
    die("signal fd");
  }

  rc = sck_create_event(loop, sigfd, SCK_READ, signalfd_on_read, NULL);
  if (rc) {
    error("register signalfd failed.%d", rc);
  }
}
#endif

static int daemon_action(tf_cli* cli) {
  // create health_check timer
  sck_create_timer(loop, 1000, daemon_health_check, NULL);

#if defined(__linux__)
  // macos dosen't support.
  daemon_signalfd();
#endif

  // bring up the daemon
  daemon_start();

  // foreground
  if (!cli->has.foreground) {
    daemon(1, 0);
  }

  return 0;
}

/* client actions
 */
static int client_retry_timer(struct sck_loop* loop,
                              sck_timer_id id,
                              void* clientData);
static void client_on_read(struct sck_loop* loop,
                           int fd,
                           void* clientData,
                           int mask) {
  struct msg_hdr hdr;
  int rc = 0;
  tf_cli* cli = (tf_cli*)clientData;

  rc = sck_read(fd, (char*)&hdr, sizeof(hdr));
  if (rc <= 0) {
    dprint("Client disconnected");
  }

  hexbuf((char*)&hdr, sizeof(hdr));
  if (hdr.hdr_magic != MSG_HDR_MAGIC) {
    error("magic error");
  }

  rc = hdr.msg_code;
  info("rc=%d", rc);
  if (cli->cmd == TURF_CLI_STOP) {
    static int retry = 0;

    if (rc == -EAGAIN) {
      dprint("retry stop");
      if (retry < 3) {
        sck_create_timer(loop, 1000, client_retry_timer, clientData);
        retry++;
      } else {
        error("reaches maxium retries.");
        exit(-1);
      }
    } else if (!retry || rc != -ECHILD) {
      error("stop failed");
      exit(rc);
    }
  }

  rc = sck_delete_event(loop, fd, SCK_READ);
  if (rc) {
    error("delete event failed. %d", rc);
  }
  close(fd);
}

static int client_connect(tf_cli* cli) {
  int size = 0;
  int fd = 0;
  int rc = 0;
  char buf[1024];
  struct msg_hdr* hdr = (struct msg_hdr*)buf;
  char* p = buf + sizeof(struct msg_hdr);

  fd = sck_unix_socket();
  if (fd < 0) {
    die("socket");
  }

  rc = sck_unix_connect(fd, sck_path);
  if (rc < 0) {
    die("connect");
  }
  dprint("connected");

  rc = sck_create_event(loop, fd, SCK_READ, client_on_read, cli);
  if (rc) {
    error("register event failed.%d", rc);
  }

  int i;
  for (i = 0; i < cli->argc; i++) {
    char* c = strcpy(p, cli->argv[i]);
    dprint("%s", c);
    p += (strlen(c) + 1);
  }
  size = (p - buf - sizeof(*hdr));
  dprint("msg_size: %d", size);
  if (size < 0) {
    die("overflow");
  }

  hdr->hdr_magic = MSG_HDR_MAGIC;
  hdr->msg_type = T_MSG_CLI_REQ;
  hdr->msg_code = 0;
  hdr->msg_size = size;

  rc = sck_write(fd, (char*)hdr, size + sizeof(*hdr));
  if (rc < 0) {
    dprint("write rc=%d", rc);
    return rc;
  }

  return 0;
}

static int client_action(tf_cli* cli) {
  int rc = 0;
  if (cli->cmd == TURF_CLI_RUN) {
    cli->cmd = TURF_CLI_CREATE;
    rc = tf_action(cli);
    if (rc) return rc;
    cli->cmd = TURF_CLI_RUN;
  }
  return client_connect(cli);
}

static int client_retry_timer(struct sck_loop* loop,
                              sck_timer_id id,
                              void* clientData) {
  dprint("%s", __func__);

  tf_cli* cli = (tf_cli*)clientData;

  client_action(cli);

  // return next tick in ms, or -1 terminate the timer.
  return -1;
}

/* turf entry
 */
int turf_start(tf_cli* cli) {
  int rc;
  if (cli->has.remote) {
    // C/S mode
    if (cli->cmd == TURF_CLI_DAEMON) {
      // daemon
      rc = daemon_action(cli);
    } else {
      // client
      rc = client_action(cli);
    }
  } else {
    // runc mode
    rc = tf_action(cli);
  }
  return rc;
}

/* main entry
 */
int main(int argc, char* argv[]) {
  int rc = 0;

  sck_path = tfd_path_sock();

  loop = sck_default_loop();

  // register signal handler
  register_sighandler();

  // parse user input
  rc = cli_parse(&cli, argc, argv);
  if (rc < 0) {
    error("command line parse failed");
    return 1;
  }

  // go process
  rc = turf_start(&cli);
  if (rc) {
    error("turf failed");
    return rc;
  }

  // main loop
  int more = sck_loop_alive(loop);  // start loop only alive
  while (more) {
    int processed = sck_process_events(loop);
    (void)processed;  // not used
    // dprint("eventloop: processed %d", processed);
    more = sck_loop_alive(loop);
  }

  dprint("loop dead, turf exiting ...");

  return 0;
}
