#include "bdd-for-c.h"
#include "realm.h"

spec("turf.realm.linux.fallback") {
  int rc;
#if 0
    it ("test basic realm") {
        struct rlm_t *r = rlm_new();
        check(r); 
        
        rc = rlm_chroot(r, "test/workdir/runtime/busybox");
        check(rc == 0);

        rc = rlm_binary(r, "/home/admin/lab/turf/test/workdir/runtime/busybox/bin/busybox");
        check(rc == 0);
        
        const char *av[] = {
            "busybox",
            "ls",
            "/",
            0, 
        };
        
        rc = rlm_arg(r, 3, av);
        check(rc == 0);

        const char *ev[] = {
            "PATH=/usr/bin:/usr/sbin:/bin:/sbin",
            "TERM=xterm",
            0,
        }; 
        rc = rlm_env(r, ev);
        check(rc == 0);
       
        r->cfg.flags.terminal = 1;
        rc = rlm_run(r);
        check(rc == 0);
       
        rc = rlm_wait(r);
        check(rc == 0);
    }
#endif
}
