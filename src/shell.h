#ifndef _TURF_SHELL_H_
#define _TURF_SHELL_H_

#include "misc.h"

// environ
const char _API* env_home(void);

// turf path
int _API tfd_set_workdir(const char* workdir);
const char _API* tfd_path();
const char _API* tfd_path_sandbox();
const char _API* tfd_path_runtime();
const char _API* tfd_path_overlay();
const char _API* tfd_path_sock();
const char _API* tfd_path_libturf();

// shell operations
int _API shl_access2(const char* parent, const char* dir);
int _API shl_access3(const char* parent, const char* sbx, const char* dir);
bool _API shl_exist2(const char* parent, const char* dir);
bool _API shl_notexist2(const char* parent, const char* dir);
bool _API shl_exist3(const char* parent, const char* sbx, const char* dir);
bool _API shl_notexist3(const char* parent, const char* sbx, const char* dir);
bool _API shl_exist4(const char* parent,
                     const char* sbx,
                     const char* dir,
                     const char* file);
int _API shl_mkdir(const char* dir);
int _API shl_mkdir2(const char* parent, const char* dir);
int _API shl_mkdir3(const char* parent, const char* sbx, const char* dir);
int _API shl_path2(char* buf, size_t size, const char* parent, const char* dir);
int _API shl_path3(char* buf,
                   size_t size,
                   const char* parent,
                   const char* sbx,
                   const char* dir);
int _API shl_path4(char* buf,
                   size_t size,
                   const char* parent,
                   const char* sbx,
                   const char* dir,
                   const char* file);
int _API shl_cp(const char* src, const char* dest, const char* name);
int _API shl_rmdir2(const char* parent, const char* dir);

#endif  // _TURF_SHELL_H_
