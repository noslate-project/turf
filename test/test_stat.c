#include "bdd-for-c.h"
#include "stat.h"

/* proc filesystem only support linux platfrom
 */
spec("turf.stat") {
#if defined(__linux__)
  it("pid_stat") {
    tf_stat stat = {0};

    // do a little math
    system("echo \"scale=1000; 4*a(1)\" | bc -l -q");

    // get pid_stat
    pid_t pid = getpid();
    int rc = pid_stat(pid, &stat);
    check(rc == 0);
    dump_stat(&stat);
    check(stat.rchar > 0);
    check(stat.wchar > 0);
    check(stat.syscr > 0);
    check(stat.syscw > 0);
    check(stat.cutime > 0);
    check(stat.rss > 0);
    check(stat.vsize > 0);
  }

#else
#warning "test_stat is disabled due to platform."
#endif
}
