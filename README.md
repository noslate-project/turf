# Turf

turf 是一个 noslate 的超级轻量进程沙盒, 基于c开发.

turf 使用单进程分发, 内置3种角色
* `turf -D`: 在c/s架构中, 为daemon, 是所有沙盒的父进程
* `turf -H`: 在c/s架构中, 为client, 作为命令行控制器
* `turf create/run`: 直接作为 oci 沙盒使用

* 动态库依赖

```
[turf]$ ldd ./turf
        linux-vdso.so.1 =>  (0x00007ffc11fec000)
        libc.so.6 => /lib64/libc.so.6 (0x00007fdd4b919000)
        /lib64/ld-linux-x86-64.so.2 (0x00007fdd4bcef000)
```

### C/S 结构

类似于 docker 的架构, 其中 turfd 作为所有沙盒的父进程, 并拥有系统特权.
此时 turf 只作为命令行工具, 向 turfd 请求沙盒管理操作.

### Fork/Exec 结构

类似于 podman 的结构, turf 直接作为 oci 沙盒使用, 并成为沙盒的父进程.
这个模式下, 无 daemon 进程, turf直接通过 `/proc` 文件系统进行所有沙盒管理操作.

### 运行模式

#### sysadmin 模式

sysadmin 是 linux 下的权限, 可使用 cgroup, seccomp, overlayfs 等所有功能,
也是 docker/runc 等沙盒运行时使用的方式.

在这个模式下, 每一个沙盒
* 独立 vfs, 根分区隔离 (pivot_root)
* 使用 cgroup 限制 cpu, 内存资源
* 限制系统调用

#### fallback 模式: 脱离 cap_sys_admin 权限运行, 提供有限的沙盒能力

这个模式下,
* 每个程序都有一个自己独立的工作目录, 共享主机文件系统
* 使用 hook 方式进行目录隔离
* 使用 `/proc` 检测沙盒资源的消耗


### 使用 turf

bundle 目录
```
+ bundle\           : 存放用户的 bundle, 可放在任何地方 turf 不管理
  + testfunc\
    + config.json   : oci spec 沙盒配置
    + code.zip      : 用户源码 code.zip
  + testfunc2\
    + config.json   : 沙盒配置
    + code\         : 源代码目录
```

turf工作目录
```
+ runtime\          : 存放 runtime
    + aworker       : aworker runtime
+ sandbox\          : 存放沙盒运行时信息
  + func-1\
    + config.json   : turf 使用的配置文件
    + state         : 沙盒状态文件
+ overlay           : 存放 overlay 的根文件系统
  + func-1\
    + merged\       : 挂载的 rootfs 根分区
+ fallback          : 存放了 fallback 的工作目录
  + func-1\
    + code\         : 用户程序的工作目录, cwd
```

#### 主要方法

* turf create

```
# create a turf bundle dir
mkdir bundle\testfunc

# enter the dir
cd bundle\testfunc

# create a default spec config.json
turf spec

# edit the config.json to match the runtime needs
vim config.json

# create a sandbox from the bundle
turf create func-1

```

* turf list
```
# list all registered sandboxs
turf list
PID xxxx

```


* turf start
```
# start a sandbox
turf start func-1


```

* turf ps

```
# list sandboxs with state infomation
turf ps

```

* turf run

combind "turf create" and "turf start" commands.


* turf delete

delete a sandbox.


* turf agent cmd

给 agent 使用的内置命令


### 使用源代码

libturf.so : 用户注入 sandbox 的进程
libturf.a  : 直接链接
tf_        : 提供沙盒运行的 api

