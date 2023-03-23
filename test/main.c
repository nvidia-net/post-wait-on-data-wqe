#include <device_memory_functions.h>
#include <post_wod_wqe.h>
#include <infiniband/mlx5dv.h>
#include <infiniband/verbs.h>
#include <inttypes.h>
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int init_qp(struct ibv_context *ctx, struct ibv_pd *pd, struct ibv_cq *cq, uint8_t port, struct ibv_qp **qp, struct ibv_qp_ex **qpex, struct mlx5dv_qp_ex **mqpex)
{

    struct ibv_qp_init_attr_ex attr_ex;
    struct mlx5dv_qp_init_attr attr_dv;
    struct ibv_qp_attr qp_attr_to_init;
    struct ibv_qp_attr qp_attr_to_rtr;
    struct ibv_qp_attr qp_attr_to_rts;

    memset(&attr_ex, 0, sizeof(attr_ex));
    memset(&attr_dv, 0, sizeof(attr_dv));
    memset(&qp_attr_to_init, 0, sizeof(qp_attr_to_init));
    memset(&qp_attr_to_rtr, 0, sizeof(qp_attr_to_rtr));
    memset(&qp_attr_to_rts, 0, sizeof(qp_attr_to_rts));

    attr_ex.qp_type = IBV_QPT_DRIVER;
    attr_ex.send_cq = cq;
    attr_ex.recv_cq = cq;
    attr_ex.pd = pd;
    attr_ex.cap.max_send_wr = 2000;
    attr_ex.cap.max_send_sge = 1;
    attr_ex.comp_mask |= IBV_QP_INIT_ATTR_SEND_OPS_FLAGS | IBV_QP_INIT_ATTR_PD;
    attr_ex.send_ops_flags = 0x3ff;

    attr_dv.comp_mask |= MLX5DV_QP_INIT_ATTR_MASK_DC |
                         MLX5DV_QP_INIT_ATTR_MASK_QP_CREATE_FLAGS |
                         MLX5DV_QP_INIT_ATTR_MASK_SEND_OPS_FLAGS;
    attr_dv.dc_init_attr.dc_type = MLX5DV_DCTYPE_DCI;
    attr_dv.create_flags |= MLX5DV_QP_CREATE_DISABLE_SCATTER_TO_CQE;

    attr_dv.send_ops_flags = MLX5DV_QP_EX_WITH_RAW_WQE;
    qp_attr_to_init.qp_state = IBV_QPS_INIT;
    qp_attr_to_init.pkey_index = 0;
    qp_attr_to_init.port_num = port;
    qp_attr_to_init.qp_access_flags =
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ |
        IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_ATOMIC;

    qp_attr_to_rtr.qp_state = IBV_QPS_RTR;
    qp_attr_to_rtr.path_mtu = IBV_MTU_4096;
    qp_attr_to_rtr.min_rnr_timer = 20;
    qp_attr_to_rtr.ah_attr.port_num = port;
    qp_attr_to_rtr.ah_attr.is_global = 0;

    qp_attr_to_rts.qp_state = IBV_QPS_RTS;
    qp_attr_to_rts.timeout = 10; // todo - what value?
    qp_attr_to_rts.retry_cnt = 7;
    qp_attr_to_rts.rnr_retry = 7;
    qp_attr_to_rts.sq_psn = 0x123;
    qp_attr_to_rts.max_rd_atomic = 1;

    // create DCIs
    *qp = mlx5dv_create_qp(ctx, &attr_ex, &attr_dv);
    if (!*qp)
    {
        perror("Failed to create QP");
        return 1;
    }

    *qpex = ibv_qp_to_qp_ex(*qp);
    if (!*qpex)
    {
        perror("Failed turn ibv_qp to ibv_qp_ex");
        return 1;
    }
    *mqpex = mlx5dv_qp_ex_from_ibv_qp_ex(*qpex);
    if (!*mqpex)
    {
        perror("Failed turn ibv_qp_ex to mlx5dv_qp_ex");
        return 1;
    }

    if (ibv_modify_qp(*qp, &qp_attr_to_init, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT) != 0)
    {
        perror("Failed to modify init qp");
        return 1;
    }

    if (ibv_modify_qp(*qp, &qp_attr_to_rtr, IBV_QP_STATE | IBV_QP_PATH_MTU | IBV_QP_AV) != 0)
    {
        perror("Failed to modify qp to rtr");
        return 1;
    }

    if (ibv_modify_qp(*qp, &qp_attr_to_rts, IBV_QP_STATE | IBV_QP_TIMEOUT | IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER) != 0)
    {
        perror("Failed to modify qp to rts");
        return 1;
    }
    return 0;
}

int poll_cq(struct ibv_cq *cq, uint64_t *wr_id)
{
    struct ibv_wc wc;
    int ret = ibv_poll_cq(cq, 1, &wc);
    if (ret < 0)
    {
        perror("ibv_poll_cq() failed");
        return -1;
    }
    if ((ret == 1) && (wc.status != IBV_WC_SUCCESS))
    {
        fprintf(stderr, "bad work completion status: %s, wr_id %zu\n", ibv_wc_status_str(wc.status), wc.wr_id);
        return -1;
    }
    *wr_id = wc.wr_id;
    return ret;
}

int print_usage(const char *prog)
{
    printf("\n%s [-d device_name] [-i DM_INIT_VALUE]\n", prog);
    printf("    DM_INIT_VALUE       >=0\n\n");
    return 0;
}

int process_args(int argc, char *argv[], unsigned long *dm_init_value, char** device_name)
{
    int c;
    char* stopstring;
    
    while ((c = getopt(argc, argv, ":i:d:")) != -1)
    {
        switch (c)
        {
        case 'd':
			*device_name = strdup(optarg);
			break;
        case 'i':
            *dm_init_value = strtoul(optarg, &stopstring, 10 /*BASE*/);
            if (*dm_init_value < 0)
                goto error;
            break;
        default:
        error:
            print_usage(argv[0]);
            return 1;
        }
    }
    return 0;
}

int open_device(struct ibv_context **ctx, char* ib_devname) {
    int num, i;
    struct ibv_device	*ib_dev = NULL;
    struct ibv_device **dev_list = ibv_get_device_list(&num);
    if(dev_list == NULL){
        perror("ibv_get_device_list failed!");
        return 1;
    }
    for (i = 0; i < num; ++i){
        if (!strcmp(ibv_get_device_name(dev_list[i]), ib_devname)){
            ib_dev = dev_list[i];
            break;
        }
    }
    if (!ib_dev) {
        fprintf(stderr, "IB device %s not found\n", ib_devname);
        return 1;
    }
    struct mlx5dv_context_attr attr = {.flags = MLX5DV_CONTEXT_FLAGS_DEVX};
    *ctx = mlx5dv_open_device(dev_list[i], &attr);
    if (*ctx == NULL) {
        printf("Could not open a device!\n");
        return 1;
    }
    printf("%s opened successfully!\n", ibv_get_device_name(dev_list[i]));
    ibv_free_device_list(dev_list);
    return 0;
}

int main(int argc, char *argv[])
{
    struct ibv_context *ctx = NULL;
    struct ibv_pd *pd = NULL;
    struct ibv_cq *cq = NULL;
    struct ibv_dm *dm = NULL;
    struct ibv_mr *dm_mr;
    int ib_port = 1, i;
    struct ibv_qp *qp;
    struct ibv_qp_ex *qpex;
    struct mlx5dv_qp_ex *mqpex;
    uint64_t value, mask;
    int op = 1;
    int ret = 0;
    uint64_t wr_id;
    char* device_name = NULL;

    if (process_args(argc, argv, &value, &device_name) != 0)
        return 1;

    if (open_device(&ctx, device_name) != 0)
        return 1;
    pd = ibv_alloc_pd(ctx);
    if (!pd)
    {
        perror("ibv_alloc_pd failed");
        return 1;
    }
    cq = ibv_create_cq(ctx, 64, NULL, NULL, 0);
    if (!cq)
    {
        perror("ibv_create_cq failed");
        return 1;
    }
    if (init_qp(ctx, pd, cq, ib_port, &qp, &qpex, &mqpex) != 0)
        return 1;

    value = 0;
    if (alloc_dm(   ctx, 
                    &dm, 
                    sizeof(value), 
                    &value, 
                    pd, 
                    &dm_mr,
                    IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_ATOMIC | IBV_ACCESS_ZERO_BASED,
                    NULL /*bar_addr*/, 
                    0 /*bar_op*/) != 0)
        return 1;

    while (op != 0)
    {
        uint64_t curr_value;
        if (ibv_memcpy_from_dm(&curr_value, dm, 0 /*offset*/, sizeof(curr_value)) != 0)
        {
            perror("ibv_memcpy_from_dm failed");
        }

        printf("\n> Operations:\n");
        printf("    > For setting DM value in Decimal press:   \"1 <decimal_num>\"\n");
        printf("    > For posting wait-on-data WQE press:      \"2 <decimal_value> <hex_mask>\"\n");
        printf("    > For finishing the program press:         \"0\"\n");
        printf("Current DM value: %" PRIu64 "\n\n", be64toh(curr_value));

        scanf("%d", &op);

        switch (op)
        {
        case 0:
            break;
        case 1:
            scanf("%" SCNu64, &value);
            break;
        case 2:
            scanf("%" SCNu64, &value);
            scanf("%" SCNx64, &mask);
            break;
        }
        if (op == 1)
        {
            value = be64toh(value);
            if (ibv_memcpy_to_dm(dm, 0 /*offset*/, &value, sizeof(value)) != 0)
            {
                perror("ibv_memcpy_to_dm failed");
                return 1;
            }
            value = htobe64(value);
        }
        if (op == 2)
        {
            if (post_wod_wqe(   qp, 
                                qpex, 
                                mqpex, 
                                value, 
                                dm_mr->lkey,
                                (uintptr_t)dm_mr->addr,
                                value,
                                WOD_CQE_ALWAYS,
                                1 /*verbosity_level*/,
                                mask,
                                WOD_RETRY, 
                                WOD_NO_FENCE, 
                                WOD_EQUAL, 
                                1 /*send_doorbell*/) != 0)
                return 1;
        }

        usleep(1000 * 500);
        ret = poll_cq(cq, &wr_id);
        if (ret == 1)
        {
            printf(">> Got wait-on-data CQE on value %" PRIu64 "\n", wr_id);
        }
        else if (ret != 0)
        {
            printf("============ Error!!! ============\n");
        }
    }

    if (dealloc_dm(dm, dm_mr) != 0)
        return 1;
    if (ibv_destroy_qp(qp) != 0)
    {
        perror("ibv_destroy_qp failed");
        return 1;
    }
    if (ibv_destroy_cq(cq) != 0)
    {
        perror("ibv_destroy_cq failed");
        return 1;
    }
    if (ibv_dealloc_pd(pd) != 0)
    {
        perror("ibv_dealloc_pd failed");
        return 1;
    }
    if (ibv_close_device(ctx) != 0)
    {
        perror("ibv_close_device failed");
        return 1;
    }
    return 0;
}