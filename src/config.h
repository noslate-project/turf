#ifndef _TURF_CONFIG_H_
#define _TURF_CONFIG_H_

/* compile cap_sysadmin features
 */
#define USE_SYSADMIN 1

/* compile fallback(without any priviliate) feature
 */
#define USE_FALLBACK 1

/* use syslog for looging,
 * or print to stderr directly.
 */
// #define USE_SYSLOG

/* die() uses abort() generate corefiles,
 * or uses exit()
 */
// #define USE_DIE_ABORT

/* wait4() supports the resource usage,
 * but it is not a standard POSIX api,
 * it may be failed on other linux boxies.
 */
#define USE_WAIT4 1

#endif
