# STM32G0x1 USB PD Sink 最小可用方案（MVP）

## 目标
先跑通 Sink 主流程：
1. Attach
2. 接收 `Source_Capabilities`
3. 发送 `Request`
4. 接收 `Accept`
5. 接收 `PS_RDY`
6. 进入 Ready

## 代码结构
- `include/pd_sink.h`：对外接口、状态、事件、端口回调
- `src/pd_sink.c`：Sink 策略状态机（不直接访问 UCPD 寄存器）


## 分层定位（先 Sink）
- `pd_sink`：Policy Engine（PE），负责状态机与策略决策。
- `pd_protocol`：Protocol Layer（PRL）基础能力，负责报文头/RDO 打包和 MessageID 维护。
- `phy_ucpd`（待接入）：UCPD 寄存器、中断、收发队列。

## 集成建议
1. UCPD ISR 中仅做收发搬运与事件入队。
2. 主循环调用 `pd_sink_on_event` 推进状态机。
3. 通过 `pd_sink_port_if_t` 对接平台层：
   - 发送 PD 控制/数据报文
   - 统一定时器
   - 状态变化日志

## 下一步（建议顺序）
1. 在平台层实现 Request 报文打包（RDO）与控制报文发送。
2. 增加 MessageID 跟踪与 GoodCRC 逻辑（Protocol Layer）。
3. 补齐 Hard Reset 恢复路径与重试策略。
4. 加入错误注入测试（超时、Reject、重复包）。
