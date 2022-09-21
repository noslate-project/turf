#ifndef _TURF_SPEC_H_
#define _TURF_SPEC_H_

extern int src_spec_json;
extern int src_spec_json_len;

static inline const char* get_spec_json() {
  return (char*)&src_spec_json;
}

static inline int get_spec_json_size() {
  return src_spec_json_len;
}

#endif  // _TURF_SPEC_H_
