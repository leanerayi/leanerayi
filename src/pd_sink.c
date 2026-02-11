#include "pd_sink.h"

#include <stddef.h>
#include <string.h>

#define PD_VOLTAGE_STEP_MV 50U
#define PD_CURRENT_STEP_MA 10U

static uint16_t pdo_fixed_mv(uint32_t pdo)
{
    return (uint16_t)(((pdo >> 10) & 0x3FFU) * PD_VOLTAGE_STEP_MV);
}

static uint16_t pdo_fixed_max_current_ma(uint32_t pdo)
{
    return (uint16_t)((pdo & 0x3FFU) * PD_CURRENT_STEP_MA);
}

static void set_state(pd_sink_ctx_t *ctx, pd_sink_state_t next)
{
    pd_sink_state_t prev = ctx->state;
    if (prev == next) {
        return;
    }

    ctx->state = next;
    if (ctx->port.on_state_changed != NULL) {
        ctx->port.on_state_changed(prev, next);
    }
}

static bool pick_request_from_caps(const pd_sink_policy_t *policy,
                                   const pd_source_caps_t *caps,
                                   pd_request_t *out)
{
    uint8_t i;
    uint16_t best_delta_mv = 0xFFFFU;
    bool found = false;

    if ((policy == NULL) || (caps == NULL) || (out == NULL)) {
        return false;
    }

    memset(out, 0, sizeof(*out));

    for (i = 0; i < caps->object_count && i < PD_MAX_DATA_OBJECTS; ++i) {
        uint32_t pdo = caps->objects[i];
        uint16_t mv = pdo_fixed_mv(pdo);
        uint16_t src_ma = pdo_fixed_max_current_ma(pdo);

        if (mv == 0U || src_ma == 0U) {
            continue;
        }

        if (mv > policy->preferred_mv) {
            continue;
        }

        if (src_ma < policy->preferred_ma) {
            continue;
        }

        uint16_t delta = (uint16_t)(policy->preferred_mv - mv);
        if (!found || delta < best_delta_mv) {
            best_delta_mv = delta;
            out->object_position = (uint8_t)(i + 1U);
            out->operating_current_ma = policy->preferred_ma;
            out->max_operating_current_ma = policy->preferred_ma;
            out->capability_mismatch = false;
            found = true;
        }
    }

    if (!found) {
        for (i = 0; i < caps->object_count && i < PD_MAX_DATA_OBJECTS; ++i) {
            uint32_t pdo = caps->objects[i];
            uint16_t mv = pdo_fixed_mv(pdo);
            uint16_t src_ma = pdo_fixed_max_current_ma(pdo);
            if ((mv == policy->fallback_mv) && (src_ma >= policy->fallback_ma)) {
                out->object_position = (uint8_t)(i + 1U);
                out->operating_current_ma = policy->fallback_ma;
                out->max_operating_current_ma = policy->fallback_ma;
                out->capability_mismatch = false;
                return true;
            }
        }
    }

    return found;
}

void pd_sink_init(pd_sink_ctx_t *ctx,
                  const pd_sink_policy_t *policy,
                  const pd_sink_port_if_t *port_if)
{
    if ((ctx == NULL) || (policy == NULL) || (port_if == NULL)) {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->policy = *policy;
    ctx->port = *port_if;
    ctx->state = PD_SINK_STATE_WAIT_FOR_ATTACH;
}

void pd_sink_on_event(pd_sink_ctx_t *ctx,
                      pd_event_t event,
                      const void *event_payload)
{
    if (ctx == NULL) {
        return;
    }

    switch (ctx->state) {
    case PD_SINK_STATE_WAIT_FOR_ATTACH:
        if (event == PD_EVT_ATTACH) {
            set_state(ctx, PD_SINK_STATE_WAIT_FOR_CAPS);
            if (ctx->port.arm_timer != NULL) {
                ctx->port.arm_timer(PD_EVT_TIMEOUT_SINK_WAIT_CAP, 500U);
            }
        }
        break;

    case PD_SINK_STATE_WAIT_FOR_CAPS:
        if (event == PD_EVT_RX_SOURCE_CAPS) {
            const pd_source_caps_t *caps = (const pd_source_caps_t *)event_payload;
            if (caps == NULL) {
                break;
            }

            ctx->last_caps = *caps;
            if (ctx->port.cancel_timer != NULL) {
                ctx->port.cancel_timer(PD_EVT_TIMEOUT_SINK_WAIT_CAP);
            }

            if (pick_request_from_caps(&ctx->policy, &ctx->last_caps, &ctx->active_request)) {
                if (ctx->port.send_request != NULL) {
                    ctx->port.send_request(&ctx->active_request);
                }
                if (ctx->port.arm_timer != NULL) {
                    ctx->port.arm_timer(PD_EVT_TIMEOUT_SENDER_RESPONSE, 30U);
                }
                set_state(ctx, PD_SINK_STATE_WAIT_ACCEPT);
            } else {
                set_state(ctx, PD_SINK_STATE_SOFT_RESET);
                if (ctx->port.send_control_soft_reset != NULL) {
                    ctx->port.send_control_soft_reset();
                }
            }
        } else if (event == PD_EVT_TIMEOUT_SINK_WAIT_CAP) {
            set_state(ctx, PD_SINK_STATE_HARD_RESET);
            if (ctx->port.send_control_hard_reset != NULL) {
                ctx->port.send_control_hard_reset();
            }
        }
        break;

    case PD_SINK_STATE_WAIT_ACCEPT:
        if (event == PD_EVT_RX_ACCEPT) {
            if (ctx->port.cancel_timer != NULL) {
                ctx->port.cancel_timer(PD_EVT_TIMEOUT_SENDER_RESPONSE);
            }
            if (ctx->port.arm_timer != NULL) {
                ctx->port.arm_timer(PD_EVT_TIMEOUT_SENDER_RESPONSE, 500U);
            }
            set_state(ctx, PD_SINK_STATE_WAIT_PS_RDY);
        } else if ((event == PD_EVT_RX_REJECT) || (event == PD_EVT_TIMEOUT_SENDER_RESPONSE)) {
            set_state(ctx, PD_SINK_STATE_WAIT_FOR_CAPS);
            if (ctx->port.arm_timer != NULL) {
                ctx->port.arm_timer(PD_EVT_TIMEOUT_SINK_WAIT_CAP, 500U);
            }
        }
        break;

    case PD_SINK_STATE_WAIT_PS_RDY:
        if (event == PD_EVT_RX_PS_RDY) {
            if (ctx->port.cancel_timer != NULL) {
                ctx->port.cancel_timer(PD_EVT_TIMEOUT_SENDER_RESPONSE);
            }
            set_state(ctx, PD_SINK_STATE_READY);
        } else if (event == PD_EVT_TIMEOUT_SENDER_RESPONSE) {
            set_state(ctx, PD_SINK_STATE_HARD_RESET);
            if (ctx->port.send_control_hard_reset != NULL) {
                ctx->port.send_control_hard_reset();
            }
        }
        break;

    case PD_SINK_STATE_READY:
        if (event == PD_EVT_DETACH) {
            set_state(ctx, PD_SINK_STATE_WAIT_FOR_ATTACH);
        } else if (event == PD_EVT_RX_SOFT_RESET) {
            if (ctx->port.send_control_accept != NULL) {
                ctx->port.send_control_accept();
            }
            set_state(ctx, PD_SINK_STATE_WAIT_FOR_CAPS);
            if (ctx->port.arm_timer != NULL) {
                ctx->port.arm_timer(PD_EVT_TIMEOUT_SINK_WAIT_CAP, 500U);
            }
        } else if (event == PD_EVT_HARD_RESET_REQUEST) {
            set_state(ctx, PD_SINK_STATE_HARD_RESET);
            if (ctx->port.send_control_hard_reset != NULL) {
                ctx->port.send_control_hard_reset();
            }
        }
        break;

    case PD_SINK_STATE_SOFT_RESET:
    case PD_SINK_STATE_HARD_RESET:
        if (event == PD_EVT_DETACH) {
            set_state(ctx, PD_SINK_STATE_WAIT_FOR_ATTACH);
        }
        break;

    default:
        break;
    }
}
