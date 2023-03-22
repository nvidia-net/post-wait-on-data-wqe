#include <post_wod_wqe.h>
#include <stdio.h>
#include <stdlib.h>

#define WOD_PTR_OFFSET(_ptr, _offset) ((void *)((ptrdiff_t)(_ptr) + (size_t)(_offset)))
// size of a single Data Segment in the WQE = 16 Bytes (read PRM)
#define WOD_DS_SIZE (16)
// Number of Data segments (read PRM): 1 (Ctrl-Seg) + 2 (Wait-on-Data-Seg)
#define WOD_N_DS (3)
// Total size of wait-on-data WQE
#define WOD_TOTAL_SIZE (WOD_DS_SIZE * WOD_N_DS)
// MLX5_OPCODE_WAIT - 0xF (PRM)
#define WOD_OPCODE_WAIT (0xF)
// wait on data opmode - 0x1 (PRM)
#define WOD_WAIT_ON_DATA_OPMODE (0x1)

typedef struct wod_wait_on_data_seg
{
    __be32 op; /* 4 bits op + 1 inv */
    __be32 lkey;
    __be64 va_fail;   // 3 bits action on fail + 61 bits of va into lkey
    __be64 data;      // value to wait
    __be64 data_mask; // value to wait
} wod_wait_on_data_seg_t;

static int wod_action_on_fail_to_str(enum wod_action_on_fail action_on_fail, char *str)
{
    switch (action_on_fail)
    {
    case WOD_RETRY:
        strcpy(str, "WOD_RETRY");
        break;
    case WOD_SEND_ERROR_CQE:
        strcpy(str, "WOD_SEND_ERROR_CQE");
        break;
    case WOD_SQ_DRAIN:
        strcpy(str, "WOD_SQ_DRAIN");
        break;
    default:
        return 1;
    }
    return 0;
}

static int wod_fence_mode_to_str(enum wod_fence_mode fence_mode, char *str)
{
    switch (fence_mode)
    {
    case WOD_NO_FENCE:
        strcpy(str, "WOD_NO_FENCE");
        break;
    case WOD_INITIATOR_SMALL_FENCE:
        strcpy(str, "WOD_INITIATOR_SMALL_FENCE");
        break;
    case WOD_FENCE:
        strcpy(str, "WOD_FENCE");
        break;
    case WOD_STRONG_ORDERING:
        strcpy(str, "WOD_STRONG_ORDERING");
        break;
    case WOD_FENCE_AND_INITIATOR_SMALL_FENCE:
        strcpy(str, "WOD_FENCE_AND_INITIATOR_SMALL_FENCE");
        break;
    default:
        return 1;
    }
    return 0;
}

static int wod_ce_mode_to_str(enum wod_ce_mode ce_mode, char *str)
{
    switch (ce_mode)
    {
    case WOD_CQE_ON_CQE_ERROR:
        strcpy(str, "WOD_CQE_ON_CQE_ERROR");
        break;
    case WOD_CQE_ON_FIRST_CQE_ERROR:
        strcpy(str, "WOD_CQE_ON_FIRST_CQE_ERROR");
        break;
    case WOD_CQE_ALWAYS:
        strcpy(str, "WOD_CQE_ALWAYS");
        break;
    case WOD_CQE_AND_EQE:
        strcpy(str, "WOD_CQE_AND_EQE");
        break;
    default:
        return 1;
    }
    return 0;
}

static int wod_operation_to_str(enum wod_operation operation, char *str)
{
    switch (operation)
    {
    case WOD_ALWAYS_TRUE:
        strcpy(str, "WOD_ALWAYS_TRUE");
        break;
    case WOD_EQUAL:
        strcpy(str, "WOD_EQUAL");
        break;
    case WOD_BIGGER:
        strcpy(str, "WOD_BIGGER");
        break;
    case WOD_SMALLER:
        strcpy(str, "WOD_SMALLER");
        break;
    case WOD_CYCLIC_BIGGER:
        strcpy(str, "WOD_CYCLIC_BIGGER");
        break;
    case WOD_CYCLIC_SMALLER:
        strcpy(str, "WOD_CYCLIC_SMALLER");
        break;
    default:
        return 1;
    }
    return 0;
}

int post_wod_wqe(   struct ibv_qp *qp,
                    struct ibv_qp_ex *qpex,
                    struct mlx5dv_qp_ex *mqpex,
                    uint64_t value,
                    uint32_t lkey,
                    uintptr_t addr,
                    uint64_t wr_id,
                    enum wod_ce_mode ce_mode,
                    int verbosity_level,
                    uint64_t data_mask,
                    enum wod_action_on_fail action_on_fail,
                    enum wod_fence_mode fence_mode,
                    enum wod_operation operation,
                    int send_doorbell)
{
    if (send_doorbell)
    {
        ibv_wr_start(qpex);
    }
    qpex->wr_id = wr_id;
    char wqe_desc[WOD_TOTAL_SIZE] = {0};
    struct mlx5_wqe_ctrl_seg *ctrl;
    wod_wait_on_data_seg_t *wseg;
    ctrl = (void *)wqe_desc;
    uint8_t fm_ce_se = (fence_mode << 5) | (ce_mode << 2);
    mlx5dv_set_ctrl_seg(ctrl, 0x0, WOD_OPCODE_WAIT, WOD_WAIT_ON_DATA_OPMODE, qp->qp_num, fm_ce_se, WOD_N_DS, 0x0, 0x0);
    wseg = WOD_PTR_OFFSET(ctrl, WOD_DS_SIZE);
    wseg->op = htobe32(operation);
    wseg->lkey = htobe32(lkey);
    if (addr & 0x7)
    {
        fprintf(stderr, "Error! address should be aligned to 8.\n");
        return 1;
    }
    wseg->va_fail = htobe64((addr) | (action_on_fail));
    wseg->data = htobe64(value);
    wseg->data_mask = htobe64(data_mask);
    mlx5dv_wr_raw_wqe(mqpex, wqe_desc);
    if (send_doorbell)
    {
        if (ibv_wr_complete(qpex))
        {
            perror("Failure during wr complete");
            return 1;
        }
    }
    if (verbosity_level)
    {
        char str[265] = {0};
        printf("> Post wait-on-data wqe:\n");
        printf("    > value (64-bit):            0x%" PRIx64 " (%" PRIu64 ")\n", value, value);
        printf("    > data_mask (64-bit):        0x%" PRIx64 " (%" PRIu64 ")\n", data_mask, data_mask);
        printf("    > wr_id (64-bit):            0x%" PRIx64 " (%" PRIu64 ")\n", wr_id, wr_id);
        wod_ce_mode_to_str(ce_mode, str);
        printf("    > Completion and event mode: %s (%d)\n", str, ce_mode);
        wod_fence_mode_to_str(fence_mode, str);
        printf("    > fence_mode:                %s (%d)\n", str, fence_mode);
        wod_action_on_fail_to_str(action_on_fail, str);
        printf("    > action_on_fail:            %s (%d)\n", str, action_on_fail);
        wod_operation_to_str(operation, str);
        printf("    > operation:                 %s (%d)\n", str, operation);
        printf("    > send_doorbell:             %d\n", send_doorbell);
    }
    return 0;
}