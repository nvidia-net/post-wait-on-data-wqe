#ifndef POST_WOD_WQE_H
#define POST_WOD_WQE_H

#include <infiniband/mlx5dv.h>
#include <infiniband/verbs.h>
#include <inttypes.h>


enum wod_action_on_fail
{
    WOD_RETRY = 0x0, // retry until condition is met
    WOD_SEND_ERROR_CQE = 0x1,
    WOD_SQ_DRAIN = 0x2
};

enum wod_fence_mode
{
    WOD_NO_FENCE = 0x0,
    WOD_INITIATOR_SMALL_FENCE = 0x1,
    WOD_FENCE = 0x2,
    WOD_STRONG_ORDERING = 0x3,
    WOD_FENCE_AND_INITIATOR_SMALL_FENCE = 0x4
};

// Completion and event mode
enum wod_ce_mode
{
    WOD_CQE_ON_CQE_ERROR = 0x0,
    WOD_CQE_ON_FIRST_CQE_ERROR = 0x1,
    WOD_CQE_ALWAYS = 0x2,
    WOD_CQE_AND_EQE = 0x3
};

// compare (data & data_mask) to (lkey.data & data_mask)
// according to the required operation.
enum wod_operation
{
    WOD_ALWAYS_TRUE = 0x0,   // The condition will always met (no matter what is the value of memory)
    WOD_EQUAL = 0x1,         // Equal
    WOD_BIGGER = 0x2,        // Unsigned bigger
    WOD_SMALLER = 0x3,       // Unsigned smaller
    WOD_CYCLIC_BIGGER = 0x4, // Signed bigger ("Cyclic" means "Signed" at PRM)
    WOD_CYCLIC_SMALLER = 0x5 // Signed smaller ("Cyclic" means "Signed" at PRM)
};

int post_wod_wqe(  struct ibv_qp *qp,
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
                    int send_doorbell);

#endif