#include "bdd-for-c.h"

/**
 * warm container fork().
 *
 * turf start a container and let warm.
 * turd sends a signal for fork().
 * warm process main thread calls fork() for a new container.
 * turf_fork() hooks the fork() api,
 *    clone a new process to be the warm container's brother.
 *    turfd register the new container.
 * the new container stops, tigger the SIGCHILD to turfd.
 */

int test_fork() {
  return 0;
}

spec("turf.warm_fork") {
  // test cli
}
