#ifndef PD_SINK_H
#define PD_SINK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PD_MAX_DATA_OBJECTS 7U

/**
 * @brief Sink 侧策略引擎状态。
 *
 * 这里聚焦最小可用流程（MVP）：
 * WaitForAttach -> WaitForCaps -> WaitAccept -> WaitPS_RDY -> Ready。
 */
typedef enum {
    PD_SINK_STATE_DISABLED = 0,
    PD_SINK_STATE_WAIT_FOR_ATTACH,
    PD_SINK_STATE_WAIT_FOR_CAPS,
    PD_SINK_STATE_EVALUATE_CAPS,
    PD_SINK_STATE_WAIT_ACCEPT,
    PD_SINK_STATE_WAIT_PS_RDY,
    PD_SINK_STATE_READY,
    PD_SINK_STATE_SOFT_RESET,
    PD_SINK_STATE_HARD_RESET
} pd_sink_state_t;

/**
 * @brief 状态机输入事件。
 *
 * 事件通常来自三类入口：
 * 1) Type-C attach/detach 检测
 * 2) PD 报文接收（Source_Cap/Accept/PS_RDY 等）
 * 3) 定时器超时
 */
typedef enum {
    PD_EVT_NONE = 0,
    PD_EVT_ATTACH,
    PD_EVT_DETACH,
    PD_EVT_RX_SOURCE_CAPS,
    PD_EVT_RX_ACCEPT,
    PD_EVT_RX_REJECT,
    PD_EVT_RX_PS_RDY,
    PD_EVT_RX_SOFT_RESET,
    PD_EVT_TIMEOUT_SENDER_RESPONSE,
    PD_EVT_TIMEOUT_SINK_WAIT_CAP,
    PD_EVT_HARD_RESET_REQUEST
} pd_event_t;

/**
 * @brief Source_Capabilities 报文的简化表示。
 */
typedef struct {
    uint8_t object_count;
    uint32_t objects[PD_MAX_DATA_OBJECTS];
} pd_source_caps_t;

/**
 * @brief Sink Request（RDO）的抽象字段。
 *
 * object_position 对应被请求的 PDO 索引（1-based）。
 */
typedef struct {
    uint8_t object_position;
    uint16_t operating_current_ma;
    uint16_t max_operating_current_ma;
    bool capability_mismatch;
} pd_request_t;

/**
 * @brief Sink 选档策略。
 *
 * 优先使用 preferred 档位；若不可用可回退到 fallback 档位。
 */
typedef struct {
    uint16_t preferred_mv;
    uint16_t preferred_ma;
    uint16_t fallback_mv;
    uint16_t fallback_ma;
} pd_sink_policy_t;

/**
 * @brief 平台适配接口。
 *
 * 协议状态机不直接访问 UCPD 寄存器，所有硬件相关能力通过回调注入：
 * - 报文发送
 * - 定时器管理
 * - 状态变化日志
 */
typedef struct {
    void (*send_request)(const pd_request_t *request);
    void (*send_control_accept)(void);
    void (*send_control_soft_reset)(void);
    void (*send_control_hard_reset)(void);
    uint32_t (*get_time_ms)(void);
    void (*arm_timer)(pd_event_t timeout_event, uint32_t timeout_ms);
    void (*cancel_timer)(pd_event_t timeout_event);
    void (*on_state_changed)(pd_sink_state_t from, pd_sink_state_t to);
} pd_sink_port_if_t;

/**
 * @brief Sink 协议上下文。
 */
typedef struct {
    pd_sink_state_t state;
    pd_sink_policy_t policy;
    pd_sink_port_if_t port;
    pd_source_caps_t last_caps;
    pd_request_t active_request;
} pd_sink_ctx_t;

void pd_sink_init(pd_sink_ctx_t *ctx,
                  const pd_sink_policy_t *policy,
                  const pd_sink_port_if_t *port_if);

/**
 * @brief 向 Sink 状态机注入事件。
 *
 * @param event_payload
 * - PD_EVT_RX_SOURCE_CAPS: 传入 const pd_source_caps_t*
 * - 其他事件: 传 NULL
 */
void pd_sink_on_event(pd_sink_ctx_t *ctx,
                      pd_event_t event,
                      const void *event_payload);

#ifdef __cplusplus
}
#endif

#endif // PD_SINK_H
