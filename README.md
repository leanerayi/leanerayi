# leanerayi USB PD Sink scaffold

一个面向 STM32G0x1（UCPD）从零实现 USB PD Sink 的最小骨架。

## 当前包含
- 事件驱动的 Sink 状态机骨架
- 可替换的平台回调接口（发送、定时器、日志）
- 基础 PDO 选择策略（优先电压/电流 + 回退档位）

## 快速检查
```bash
gcc -std=c11 -Wall -Wextra -Werror -Iinclude -c src/pd_sink.c -o /tmp/pd_sink.o
```

> 该仓库暂未包含 STM32 外设寄存器驱动；建议在 `pd_sink_port_if_t` 的回调里接入 UCPD 平台层。
