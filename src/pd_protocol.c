#include "pd_protocol.h"

#include <stddef.h>
#include <string.h>

#define PD_RDO_CURRENT_STEP_MA 10U

void pd_protocol_init(pd_protocol_ctx_t *ctx)
{
    if (ctx == NULL) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->auto_goodcrc = true;
}

uint16_t pd_protocol_pack_header(const pd_msg_header_t *header)
{
    uint16_t packed = 0U;

    if (header == NULL) {
        return 0U;
    }

    packed |= (uint16_t)(header->message_type & 0x1FU);
    packed |= (uint16_t)((header->port_data_role & 0x01U) << 5);
    packed |= (uint16_t)((header->spec_revision & 0x03U) << 6);
    packed |= (uint16_t)((header->port_power_role & 0x01U) << 8);
    packed |= (uint16_t)((header->message_id & 0x07U) << 9);
    packed |= (uint16_t)((header->num_data_objects & 0x07U) << 12);
    packed |= (uint16_t)((header->extended ? 1U : 0U) << 15);

    return packed;
}

uint32_t pd_protocol_pack_fixed_rdo(const pd_request_data_t *request)
{
    uint32_t operating_units;
    uint32_t max_units;
    uint32_t packed = 0U;

    if (request == NULL || request->object_position == 0U || request->object_position > 7U) {
        return 0U;
    }

    operating_units = (uint32_t)request->operating_current_ma / PD_RDO_CURRENT_STEP_MA;
    max_units = (uint32_t)request->max_operating_current_ma / PD_RDO_CURRENT_STEP_MA;

    packed |= ((uint32_t)request->object_position & 0x07U) << 28;
    packed |= (request->capability_mismatch ? 1UL : 0UL) << 26;
    packed |= (request->usb_communications_capable ? 1UL : 0UL) << 25;
    packed |= (request->no_usb_suspend ? 1UL : 0UL) << 24;
    packed |= (max_units & 0x3FFUL) << 10;
    packed |= (operating_units & 0x3FFUL);

    return packed;
}

uint8_t pd_protocol_next_tx_message_id(pd_protocol_ctx_t *ctx)
{
    uint8_t next;

    if (ctx == NULL) {
        return 0U;
    }

    next = ctx->tx_message_id;
    ctx->tx_message_id = (uint8_t)((ctx->tx_message_id + 1U) & 0x07U);

    return next;
}

void pd_protocol_note_rx_message_id(pd_protocol_ctx_t *ctx, uint8_t message_id)
{
    if (ctx == NULL) {
        return;
    }

    ctx->rx_message_id = (uint8_t)(message_id & 0x07U);
}
