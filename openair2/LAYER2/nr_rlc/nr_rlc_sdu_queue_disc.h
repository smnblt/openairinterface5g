/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */

#ifndef _NR_RLC_SDU_QUEUE_DISC_H_
#define _NR_RLC_SDU_QUEUE_DISC_H_

#define PI2_AQM_ALPHA_BETA_GRANULARITY 6

#include <stdint.h>
#include <stdbool.h>

struct nr_rlc_entity_s;

typedef struct nr_rlc_sdu_queue_disc_s 
{
  /* functions provided by the AQM module */
  /* at enqueue pass the rlc entity to access sdu queu stats and the packet*/
  bool (*enqueue)(void *queue_disc_ctx, struct nr_rlc_entity_s *entity, char *buffer, int size, int sdu_id);
  
  /* at dequeue*/
  //TO DO
  //void (*dequeue)(struct nr_rlc_entity_s *entity, char *buffer, int size, int sdu_id);
  //void (*do_act)
  int type;
  void *queue_disc_ctx;
} nr_rlc_sdu_queue_disc_t;

typedef struct step_queue_disc_ctx_s 
{    
    uint32_t latency_threshold_us;

} step_queue_disc_ctx_t;

typedef struct pi2_queue_disc_ctx_s 
{    
    /* https://github.com/L4STeam/linux/blob/testing/net/sched/sch_dualpi2.c */
    uint32_t target_us;	/* Target queueing delay in useconds */
    uint32_t tupdate_us;/* Timer frequency in useconds */
	//uint32_t prob;	/* Base PI probability */
    int prob;	/* Base PI probability */
	uint32_t alpha;	/* Gain factor for the integral rate response */
	uint32_t beta;	/* Gain factor for the proportional response */
    uint32_t last_qdelay_ns;

} pi2_queue_disc_ctx_t;

void nr_rlc_initialize_sdu_queue_disc(struct nr_rlc_entity_s *nr_rlc, int flag);
void nr_rlc_delete_sdu_queue_disc_ctx(struct nr_rlc_entity_s *nr_rlc);

bool transparent_queue_disc_enqueue(void *queue_disc_ctx,
                                    struct nr_rlc_entity_s *entity,
                                    char *buffer,
                                    int size,
                                    int sdu_id);

bool step_drop_queue_disc_enqueue(void *queue_disc_ctx,
                                  struct nr_rlc_entity_s *entity,
                                  char *buffer,
                                  int size,
                                  int sdu_id);

bool step_mark_queue_disc_enqueue(void *queue_disc_ctx,
                                  struct nr_rlc_entity_s *entity,
                                  char *buffer,
                                  int size,
                                  int sdu_id);

bool pi2_queue_disc_enqueue(void *queue_disc_ctx,
                                  struct nr_rlc_entity_s *entity,
                                  char *buffer,
                                  int size,
                                  int sdu_id);

#endif