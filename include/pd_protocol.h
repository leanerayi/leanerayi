#ifndef PD_PROTOCOL_H
#define PD_PROTOCOL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** USB PD 数据对象最大个数（SOP） */
#define PD_PROTOCOL_MAX_DATA_OBJECTS 7U

/**
 * @brief PD 报文头的抽象字段。
 */
typedef struct {
    uint8_t message_type;
    uint8_t port_data_role;
    uint8_t spec_revision;
    uint8_t port_power_role;
    uint8_t message_id;
    uint8_t num_data_objects;
    bool extended;
} pd_msg_header_t;

/**
 * @brief PRL 层上下文。
 *
 * 当前只维护 MessageID 与可选的 GoodCRC 自动应答开关，
 * 后续可继续扩展 SOP'/SOP'' 与重传计数。
 */
typedef struct {
    uint8_t tx_message_id;
    uint8_t rx_message_id;
    bool auto_goodcrc;
} pd_protocol_ctx_t;

/**
 * @brief Sink Request 抽象参数（用于打包 RDO）。
 */
typedef struct {
    uint8_t object_position;
    uint16_t operating_current_ma;
    uint16_t max_operating_current_ma;
    bool capability_mismatch;
    bool usb_communications_capable;
    bool no_usb_suspend;
} pd_request_data_t;

void pd_protocol_init(pd_protocol_ctx_t *ctx);

uint16_t pd_protocol_pack_header(const pd_msg_header_t *header);

uint32_t pd_protocol_pack_fixed_rdo(const pd_request_data_t *request);

uint8_t pd_protocol_next_tx_message_id(pd_protocol_ctx_t *ctx);

void pd_protocol_note_rx_message_id(pd_protocol_ctx_t *ctx, uint8_t message_id);

#ifdef __cplusplus
}
#endif

#endif // PD_PROTOCOL_H
