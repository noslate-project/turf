#include "bdd-for-c.h"
#include "shell.h"

spec("turf.shell") {
  it("environ.home") {
    const char* home = env_home();
    check(home);

    const char* home2 = env_home();
    check(home == home2);

    const char* home3 = getenv("HOME");
    check(strcmp(home, home3) == 0);
  }

  it("turfd.path") {
    const char *p, *p2;
    p = tfd_path();
    check(p);
    p2 = tfd_path();
    check(p == p2);

    p = tfd_path_overlay();
    p2 = tfd_path_overlay();
    check(p);
    check(p == p2);

    p = tfd_path_sandbox();
    p2 = tfd_path_sandbox();
    check(p);
    check(p == p2);

    p = tfd_path_runtime();
    p2 = tfd_path_runtime();
    check(p);
    check(p == p2);
  }
}
