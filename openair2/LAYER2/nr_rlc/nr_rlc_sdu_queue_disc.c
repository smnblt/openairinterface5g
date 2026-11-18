/*
 * SPDX-License-Identifier: LicenseRef-CSSL-1.0
 */
#include <stdlib.h>

#include "common/utils/assertions.h"
#include "LOG/log.h"
#include "nr_rlc_entity.h"
#include "nr_rlc_sdu_queue_disc.h"

//static_assert(RAND_MAX == 0x7FFFFFFF, "rand() is expected to return 31 bits");

static uint32_t rand_u32(void)
{
  /* Leaving like this for now. It won't hurt if rand produces 32bit values. Though it might not produce uniformly random values */
  return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}

void transparent_queue_disc_do_periodic(void *queue_disc_ctx, nr_rlc_entity_t *entity)
{
  return;
}

bool transparent_queue_disc_enqueue(void *queue_disc_ctx, nr_rlc_entity_t *entity, char *buffer, int size, int sdu_id)
{
    return true;
}

bool step_drop_queue_disc_enqueue(void *queue_disc_ctx, nr_rlc_entity_t *entity, char *buffer, int size, int sdu_id)
{
  entity->get_stats(entity, &(entity->stats));

  step_queue_disc_ctx_t *ctx = (step_queue_disc_ctx_t *)queue_disc_ctx;

  if (entity->stats.txsdu_wt_us > ctx->latency_threshold_us) {
    return false;
  }

  return true;
}

bool step_mark_queue_disc_enqueue(void *queue_disc_ctx, nr_rlc_entity_t *entity, char *buffer, int size, int sdu_id)
{
  return true;
}

bool pi2_queue_disc_enqueue(void *queue_disc_ctx, nr_rlc_entity_t *entity, char *buffer, int size, int sdu_id)
{
  /*let us call here for now */
  /*TO DO: make it periodic.*/
  pi2_queue_disc_do_periodic(queue_disc_ctx, entity);

  pi2_queue_disc_ctx_t *ctx = (pi2_queue_disc_ctx_t *)queue_disc_ctx;
  bool must_drop = (rand_u32() <= ctx->prob && rand_u32() <= ctx->prob);
  return !must_drop;
}

static uint64_t pi2_queue_disc_scale_delta(uint64_t diff)
{
  return diff >> PI2_AQM_ALPHA_BETA_GRANULARITY;
}

static uint32_t pi2_queue_disc_scale_alpha_beta(uint32_t param)
{
  uint64_t tmp = ((uint64_t)param * PI2_AQM_MAX_PROB >> PI2_AQM_ALPHA_BETA_SCALING);
  return tmp / PI2_AQM_NSEC_PER_SEC;
}

void pi2_queue_disc_do_periodic(void *queue_disc_ctx, nr_rlc_entity_t *entity)
{
  uint32_t new_prob;
  uint64_t qdelay_ns;
  int64_t delta;
  uint64_t time_now = time_average_now();
  pi2_queue_disc_ctx_t *ctx = (pi2_queue_disc_ctx_t *)queue_disc_ctx;
  
  if (time_now - ctx->last_updated_us < ctx->tupdate_us)
  {
    return;
  }
  
  entity->get_stats(entity, &(entity->stats));
  qdelay_ns = entity->stats.txsdu_wt_us * 1000; /* convert to ns */

  /* Alpha and beta take at most 32b, i.e, the delay difference would
   * overflow for queuing delay differences > ~4.2sec.
   */
  delta = ((int64_t)qdelay_ns - ctx->target_us * 1000) * ctx->alpha;
  delta += ((int64_t)qdelay_ns - ctx->last_qdelay_ns) * ctx->beta;

  if (delta > 0) 
  {
    new_prob = pi2_queue_disc_scale_delta(delta) + ctx->prob;
    if (new_prob < ctx->prob)
      new_prob = PI2_AQM_MAX_PROB;
  } 
  else 
  {
    new_prob = ctx->prob - pi2_queue_disc_scale_delta(delta * -1);
    if (new_prob > ctx->prob)
      new_prob = 0;
  }
  ctx->last_qdelay_ns = qdelay_ns;
  ctx->last_updated_us = time_now;

  ctx->prob = new_prob;
}

void nr_rlc_initialize_sdu_queue_disc(nr_rlc_entity_t *nr_rlc, int flag)
{
  nr_rlc->rlc_sdu_queue_disc.type = flag;
  switch (flag) {
    case 1:
      step_queue_disc_ctx_t *ctx = calloc(1, sizeof(*ctx));
      AssertFatal(ctx != 0, "Out of memory allocating queue-disc context\n");

      ctx->latency_threshold_us = 100000;

      nr_rlc->rlc_sdu_queue_disc.queue_disc_ctx = ctx;
      nr_rlc->rlc_sdu_queue_disc.enqueue = step_drop_queue_disc_enqueue;
      nr_rlc->rlc_sdu_queue_disc.do_periodic = transparent_queue_disc_do_periodic;
      break;

    case 2:
      pi2_queue_disc_ctx_t *ctx_pi2 = calloc(1, sizeof(*ctx_pi2));
      AssertFatal(ctx_pi2 != 0, "Out of memory allocating queue-disc context\n");

      ctx_pi2->target_us = 15000; /* Target queueing delay in useconds */
      ctx_pi2->tupdate_us = 16000; /* Timer frequency in useconds */
      ctx_pi2->prob = 0; /* Base PI probability */
      ctx_pi2->alpha = pi2_queue_disc_scale_alpha_beta(41); /* Gain factor for the integral rate response */
      ctx_pi2->beta = pi2_queue_disc_scale_alpha_beta(819); /* Gain factor for the proportional response */
      ctx_pi2->last_qdelay_ns = 0;
      ctx_pi2->last_updated_us = 0;

      nr_rlc->rlc_sdu_queue_disc.queue_disc_ctx = ctx_pi2;
      nr_rlc->rlc_sdu_queue_disc.enqueue = pi2_queue_disc_enqueue;
      nr_rlc->rlc_sdu_queue_disc.do_periodic = pi2_queue_disc_do_periodic;
      break;

    default:
      //if aqm
      nr_rlc->rlc_sdu_queue_disc.queue_disc_ctx = 0;
      nr_rlc->rlc_sdu_queue_disc.enqueue = transparent_queue_disc_enqueue;
      nr_rlc->rlc_sdu_queue_disc.do_periodic = transparent_queue_disc_do_periodic;
      break;
  }
}

void nr_rlc_delete_sdu_queue_disc_ctx(nr_rlc_entity_t *nr_rlc)
{
  if (nr_rlc->rlc_sdu_queue_disc.queue_disc_ctx == NULL)
    return;

  switch (nr_rlc->rlc_sdu_queue_disc.type) {
    case 1:
      free((step_queue_disc_ctx_t *)nr_rlc->rlc_sdu_queue_disc.queue_disc_ctx);
      break;

    default:
      break;
  }

  nr_rlc->rlc_sdu_queue_disc.queue_disc_ctx = NULL;
}