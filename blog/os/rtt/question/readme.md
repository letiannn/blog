# 开启CmBacktrace和ulog fileback模式会出现崩溃打印错误
[讨论解决](https://github.com/RT-Thread/rt-thread/issues/6327)
[问题原因](https://github.com/armink-rtt-pkgs/CmBacktrace/pull/14/files)
1. 添加关日志锁可修复问题

```c
rt_cm_backtrace_exception_hook

rt_cm_backtrace_assert_hook

取消函数内的rt_interrupt_enter和rt_interrupt_leave
```