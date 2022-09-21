#ifndef _TURF_PHD_H_
#define _TURF_PHD_H_

/* TURF-PHD is a function place holder for userspace programs.
 *
 * "place holder" is a stub function pointer, if it attached to a turf envrion
 * the function pointer will be replaced by a real turf implementation.
 * otherwise, only a condition judgment will be performed, and return "-1" in
 * return code directly.
 *
 * the replacement will be done before main() is invoked.
 *
 * how to use it?
 *
 * int any_your_function(...) {
 *     int rc;          // output return code
 *     int out1;        // output value
 *     TURF_PHD("name", &rc, &out1);
 *     if (rc == 0) {   // zero means success called
 *          do_your_logic(out1);
 *     }
 *     // otherwise
 *     do_other_locgic();
 * }
 */

#define _TPHD_NOTE_NAME "turfphd"
#define _TPHD_NOTE_TYPE 11
#define _TPHD_SEC_NAME ".turfphd"

// Turf Place Holder
#ifdef __linux__
#define TURF_PHD(name, rc, args...)                                            \
  do {                                                                         \
    _TPHD_DEF(_t_phd_##name);                                                  \
    _TPHD_CODE(name, _t_phd_##name);                                           \
    if (_t_phd_##name) {                                                       \
      _t_phd_##name(rc, ##args);                                               \
    } else {                                                                   \
      *rc = -1;                                                                \
    }                                                                          \
  } while (0)
#else
// TBD: support macOS
#define TURF_PHD(name, rc, args...)                                            \
  do {                                                                         \
    *rc = -1;                                                                  \
  } while (0)
#endif

/* .note section {
 *    4byte len(namesz),
 *    4byte len(descsz),
 *    4byte type,
 *    sz name ... \0,
 *    sz desc ... \0,
 * }
 * turf_phd desc {
 *    8byte func_pc,
 *    sz func_name ... \0,
 * }
 */

struct tphd_desc {
  void* pc;      // pointer address
  char name[0];  // function name
};

#define _TPHD_STR(x) #x
#define _TPHD_TYPE(x) _TPHD_STR(x)
#define _TPHD_DEF(x)                                                           \
  static void __attribute__((used)) (*x)(int*, ...) asm(#x) = NULL;

#define _TPHD_ASM_CODE(name, ptr)                                              \
  "     .pushsection \"" _TPHD_SEC_NAME "\", \"a\", \"note\" \n"               \
  "200: .balign 4 \n"                                                          \
  "     .4byte 202f-201f, 204f-203f, " _TPHD_TYPE(                             \
      _TPHD_NOTE_TYPE) " \n"                                                   \
                       "201: .asciz \"" _TPHD_NOTE_NAME "\" \n"                \
                       "202: .balign 4 \n"                                     \
                       "203: .8byte " _TPHD_STR(                               \
                           ptr) "\n"                                           \
                                "     .asciz \"" _TPHD_STR(                    \
                                    name) "\" \n"                              \
                                          "204: .balign 4 \n"                  \
                                          "     .popsection\n"

#define _TPHD_CODE(...) __asm__ __volatile__(_TPHD_ASM_CODE(__VA_ARGS__))

#endif  // _TURF_PHD_H_
