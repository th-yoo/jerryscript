/* Copyright 2014-2015 Samsung Electronics Co., Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/** \addtogroup mem Memory allocation
 * @{
 *
 * \addtogroup poolman Memory pool manager
 * @{
 */

/**
 * Memory pool manager implementation
 */

#define JERRY_MEM_POOL_INTERNAL

#include "jrt.h"
#include "jrt-libc-includes.h"
#include "mem-allocator.h"
#include "mem-heap.h"
#include "mem-pool.h"
#include "mem-poolman.h"

/*
 * Valgrind-related options and headers
 */
#ifdef JERRY_VALGRIND
# include "memcheck.h"

# define VALGRIND_NOACCESS_SPACE(p, s)  (void)VALGRIND_MAKE_MEM_NOACCESS((p), (s))
# define VALGRIND_UNDEFINED_SPACE(p, s) (void)VALGRIND_MAKE_MEM_UNDEFINED((p), (s))
# define VALGRIND_DEFINED_SPACE(p, s)   (void)VALGRIND_MAKE_MEM_DEFINED((p), (s))
#else /* JERRY_VALGRIND */
# define VALGRIND_NOACCESS_SPACE(p, s)
# define VALGRIND_UNDEFINED_SPACE(p, s)
# define VALGRIND_DEFINED_SPACE(p, s)
#endif /* JERRY_VALGRIND */

/**
 * Lists of pools
 */
mem_pool_state_t *mem_pools;

/**
 * Number of free chunks
 */
#if 0
size_t mem_free_chunks_number;
#endif

/**
 * List of free pool chunks
 */
mem_pool_chunk_t *mem_free_chunk_p;

static void mem_check_pools (void);

#ifdef MEM_STATS
/**
 * Pools' memory usage statistics
 */
mem_pools_stats_t mem_pools_stats;

static void mem_pools_stat_init (void);
static void mem_pools_stat_alloc_pool (void);
static void mem_pools_stat_free_pool (void);
static void mem_pools_stat_alloc_chunk (void);
static void mem_pools_stat_free_chunk (void);

#  define MEM_POOLS_STAT_INIT() mem_pools_stat_init ()
#  define MEM_POOLS_STAT_ALLOC_POOL() mem_pools_stat_alloc_pool ()
#  define MEM_POOLS_STAT_FREE_POOL() mem_pools_stat_free_pool ()
#  define MEM_POOLS_STAT_ALLOC_CHUNK() mem_pools_stat_alloc_chunk ()
#  define MEM_POOLS_STAT_FREE_CHUNK() mem_pools_stat_free_chunk ()
#else /* !MEM_STATS */
#  define MEM_POOLS_STAT_INIT()
#  define MEM_POOLS_STAT_ALLOC_POOL()
#  define MEM_POOLS_STAT_FREE_POOL()
#  define MEM_POOLS_STAT_ALLOC_CHUNK()
#  define MEM_POOLS_STAT_FREE_CHUNK()
#endif /* !MEM_STATS */

/**
 * Initialize pool manager
 */
void
mem_pools_init (void)
{
#if 0
  mem_free_chunks_number = 0;
#endif
  mem_free_chunk_p = NULL;

  MEM_POOLS_STAT_INIT ();
} /* mem_pools_init */

/**
 * Finalize pool manager
 */
void
mem_pools_finalize (void)
{
  mem_pools_remove_empty_pools ();

#if 0
  JERRY_ASSERT (mem_free_chunks_number == 0);
#endif
} /* mem_pools_finalize */

void
mem_pools_remove_empty_pools (void)
{
  /*
   * At first pass collect pointers to those of free chunks that are first at their pools
   * to separate list and change layout of the chunks to the following:
   */

  const uint16_t hint_magic_num_value = 0x7e89;

  constexpr uint32_t number_of_pool_lists = 8;
  mem_pool_chunk_t *first_chunk_of_a_pool_list_p[number_of_pool_lists];
  for (uint32_t i = 0; i < number_of_pool_lists; i++)
  {
    first_chunk_of_a_pool_list_p[i] = NULL;
  }

  uint32_t number_of_pools_with_free_first_chunk = 0;

  for (mem_pool_chunk_t *free_chunk_iter_p = mem_free_chunk_p, *prev_free_chunk_p = NULL, *next_free_chunk_p;
       free_chunk_iter_p != NULL;
       free_chunk_iter_p = next_free_chunk_p)
  {
    mem_pool_state_t *pool_p = (mem_pool_state_t *) mem_heap_get_chunked_block_start (free_chunk_iter_p);

    VALGRIND_DEFINED_SPACE (free_chunk_iter_p, MEM_POOL_CHUNK_SIZE);

    next_free_chunk_p = free_chunk_iter_p->u.free.next_p;
                          
    if ((mem_pool_chunk_t *) MEM_POOL_SPACE_START (pool_p) == free_chunk_iter_p)
    {
      /*
       * The chunk is first at its pool
       *
       * Remove the chunk from common list of free chunks
       */
      if (prev_free_chunk_p == NULL)
      {
        JERRY_ASSERT (mem_free_chunk_p == free_chunk_iter_p);

        mem_free_chunk_p = next_free_chunk_p;
      }
      else
      {
        prev_free_chunk_p->u.free.next_p = next_free_chunk_p;
      }

      number_of_pools_with_free_first_chunk++;

      uint8_t id = number_of_pools_with_free_first_chunk % number_of_pool_lists;

      MEM_CP_SET_POINTER (free_chunk_iter_p->u.u_pool_gc.stage1.next_first_cp, first_chunk_of_a_pool_list_p[id]);
      free_chunk_iter_p->u.u_pool_gc.stage1.free_list_cp = MEM_CP_NULL;
      free_chunk_iter_p->u.u_pool_gc.stage1.free_chunks_num = 1; /* 1 (first chunk) */
      free_chunk_iter_p->u.u_pool_gc.stage1.hint_magic_num = hint_magic_num_value;
      free_chunk_iter_p->u.u_pool_gc.stage1.id = id;

      first_chunk_of_a_pool_list_p[id] = free_chunk_iter_p;
    }
    else
    {
      prev_free_chunk_p = free_chunk_iter_p;
    }
  }

  if (number_of_pools_with_free_first_chunk == 0)
  {
    /* there are no empty pools */

    return;
  }

  /*
   * At second pass we check for all rest free chunks whether their pool's first chunk could be free,
   * and so currently in temporary list of first chunks of their pools.
   *
   * For each of the chunk contained in pool, try to find the first chunk through the list,
   * to check that the first chunk is really free, and so, was acquired during first stage of
   * empty pools collection.
   *
   * If the corresponding first chunk is really free, increment counter of free chunks in it, and move the checked
   * chunk to temporary local list of corresponding pool's free chunks.
   */
  for (mem_pool_chunk_t *free_chunk_iter_p = mem_free_chunk_p, *prev_free_chunk_p = NULL, *next_free_chunk_p;
       free_chunk_iter_p != NULL;
       free_chunk_iter_p = next_free_chunk_p)
  {
    mem_pool_state_t *pool_p = (mem_pool_state_t *) mem_heap_get_chunked_block_start (free_chunk_iter_p);

    next_free_chunk_p = free_chunk_iter_p->u.free.next_p;

    mem_pool_chunk_t *first_chunk_of_pool_p = (mem_pool_chunk_t *) MEM_POOL_SPACE_START (pool_p);

    bool is_chunk_moved_to_local_list = false;

    if (first_chunk_of_pool_p->u.u_pool_gc.stage1.hint_magic_num == hint_magic_num_value)
    {
      /*
       * Maybe, the first chunk is free.
       *
       * If it is so, it is included in the list of pool's first free chunks.
       */
      uint8_t id_to_search_in = first_chunk_of_pool_p->u.u_pool_gc.stage1.id;

      if (id_to_search_in < number_of_pool_lists)
      {
        for (mem_pool_chunk_t *first_pool_chunks_iter_p = first_chunk_of_a_pool_list_p[id_to_search_in];
             first_pool_chunks_iter_p != NULL;
             first_pool_chunks_iter_p = MEM_CP_GET_POINTER (mem_pool_chunk_t,
                                                            first_pool_chunks_iter_p->u.u_pool_gc.stage1.next_first_cp))
        {
          if (first_pool_chunks_iter_p == first_chunk_of_pool_p)
          {
            /*
             * The first chunk is actually free.
             *
             * So, incrementing free chunks counter in it.
             */
            first_chunk_of_pool_p->u.u_pool_gc.stage1.free_chunks_num++;

            /*
             * It is possible that the corresponding pool is empty
             *
             * Moving current chunk from common list of free chunks to temporary list, local to the pool
             */
            if (prev_free_chunk_p == NULL)
            {
              JERRY_ASSERT (mem_free_chunk_p == free_chunk_iter_p);

              mem_free_chunk_p = next_free_chunk_p;
            }
            else
            {
              prev_free_chunk_p->u.free.next_p = next_free_chunk_p;
            }

            free_chunk_iter_p->u.free.next_p = MEM_CP_GET_POINTER (mem_pool_chunk_t,
                                                                   first_chunk_of_pool_p->u.u_pool_gc.stage1.free_list_cp);
            MEM_CP_SET_NON_NULL_POINTER (first_chunk_of_pool_p->u.u_pool_gc.stage1.free_list_cp, free_chunk_iter_p);

            is_chunk_moved_to_local_list = true;

            break;
          }
        }
      }
    }

    if (!is_chunk_moved_to_local_list)
    {
      prev_free_chunk_p = free_chunk_iter_p;
    }
  }

  /*
   * At third pass we check each pool with first chunk free for counter number of free chunks in the pool.
   *
   * If the number is equal to number of chunks in the pool - the the pool is freed,
   * otherwise - free chunks of the pool are returned to common list of free chunks.
   */
  for (uint32_t list_id = 0; list_id < number_of_pool_lists; list_id++)
  {
    for (mem_pool_chunk_t *first_pool_chunks_iter_p = first_chunk_of_a_pool_list_p[list_id], *next_p;
         first_pool_chunks_iter_p != NULL;
         first_pool_chunks_iter_p = next_p)
    {
      next_p = MEM_CP_GET_POINTER (mem_pool_chunk_t,
                                   first_pool_chunks_iter_p->u.u_pool_gc.stage1.next_first_cp);

      if (first_pool_chunks_iter_p->u.u_pool_gc.stage1.free_chunks_num == MEM_POOL_CHUNKS_NUMBER)
      {
        mem_heap_free_block (first_pool_chunks_iter_p);

#if 0
        mem_free_chunks_number -= MEM_POOL_CHUNKS_NUMBER;
#endif
      }
      else
      {
        mem_pool_chunk_t *first_chunk_of_pool_p = (mem_pool_chunk_t *) first_pool_chunks_iter_p;

        mem_pool_chunk_t *non_first_free_chunk_of_the_pool_p = MEM_CP_GET_POINTER (mem_pool_chunk_t,
                                                                                   first_pool_chunks_iter_p->u.u_pool_gc.stage1.free_list_cp);
        first_chunk_of_pool_p->u.free.next_p = non_first_free_chunk_of_the_pool_p;

        for (mem_pool_chunk_t *pool_chunks_iter_p = first_chunk_of_pool_p;
             ;
             pool_chunks_iter_p = pool_chunks_iter_p->u.free.next_p)
        {
          JERRY_ASSERT (pool_chunks_iter_p != NULL);

          if (pool_chunks_iter_p->u.free.next_p == NULL)
          {
            pool_chunks_iter_p->u.free.next_p = mem_free_chunk_p;
            mem_free_chunk_p = first_chunk_of_pool_p;

            break;
          }
        }
      }
    }
  }

  /*
   * Valgrind-mode specific pass
   *
   * Set all free chunks inaccessible
   */
  for (mem_pool_chunk_t *free_chunk_iter_p = mem_free_chunk_p, *next_free_chunk_p;
       free_chunk_iter_p != NULL;
       free_chunk_iter_p = next_free_chunk_p)
  {
    next_free_chunk_p = free_chunk_iter_p->u.free.next_p;

    VALGRIND_NOACCESS_SPACE (free_chunk_iter_p, MEM_POOL_CHUNK_SIZE);
  }
}

/**
 * Long path for mem_pools_alloc
 */
static void __attr_noinline___
mem_pools_alloc_longpath (void)
{
  mem_check_pools ();

  JERRY_ASSERT (mem_free_chunk_p == NULL);
#if 0
  JERRY_ASSERT (mem_free_chunks_number == 0);
#endif

  JERRY_ASSERT (MEM_POOL_SIZE <= mem_heap_get_chunked_block_data_size ());
  JERRY_ASSERT (MEM_POOL_CHUNKS_NUMBER >= 1);

  mem_pool_state_t *pool_state_p = (mem_pool_state_t*) mem_heap_alloc_chunked_block (MEM_HEAP_ALLOC_LONG_TERM);
  JERRY_ASSERT (pool_state_p != NULL);

  if (mem_free_chunk_p != NULL)
  {
    /* some chunks could be freed due to GC called by heap allocator */
    mem_heap_free_block (pool_state_p);

    return;
  }

  mem_pool_chunk_t *first_pool_free_chunk_p = mem_pool_init (pool_state_p, MEM_POOL_SIZE);

  // pool_state_p->prev_pool_cp = MEM_CP_NULL;
  // MEM_CP_SET_POINTER (pool_state_p->next_pool_cp, mem_pools);

  // if (mem_pools != NULL)
  // {
  //   MEM_CP_SET_NON_NULL_POINTER (mem_pools->prev_pool_cp, pool_state_p);
  // }

  // mem_pools = pool_state_p;

#if 0
  mem_free_chunks_number += MEM_POOL_CHUNKS_NUMBER;
#endif
  mem_free_chunk_p = first_pool_free_chunk_p;

  MEM_POOLS_STAT_ALLOC_POOL ();

  mem_check_pools ();
} /* mem_pools_alloc_longpath */

/**
 * Allocate a chunk of specified size
 *
 * @return pointer to allocated chunk, if allocation was successful,
 *         or NULL - if not enough memory.
 */
uint8_t* __attr_always_inline___
mem_pools_alloc (void)
{
  do
  {
    if (mem_free_chunk_p != NULL)
    {
      JERRY_ASSERT (mem_free_chunk_p != NULL);

      // JERRY_ASSERT (mem_pools != NULL);

      /**
       * And allocate chunk within it.
       */
#if 0
      mem_free_chunks_number--;
#endif

      MEM_POOLS_STAT_ALLOC_CHUNK ();

      mem_pool_chunk_t *chunk_p = mem_free_chunk_p;

      VALGRIND_DEFINED_SPACE (chunk_p, MEM_POOL_CHUNK_SIZE);

      mem_free_chunk_p = chunk_p->u.free.next_p;

      VALGRIND_UNDEFINED_SPACE (chunk_p, MEM_POOL_CHUNK_SIZE);

      uint8_t *allocated_chunk_p = (uint8_t *) chunk_p;

      mem_check_pools ();

      return allocated_chunk_p;
    }
    else
    {
      mem_pools_alloc_longpath ();

      /* the assertion guarantees that there will be no more than two iterations */
      JERRY_ASSERT (mem_free_chunk_p != NULL);
    }
  } while (true);
} /* mem_pools_alloc */

/**
 * Free the chunk
 */
void __attr_always_inline___
mem_pools_free (uint8_t *chunk_p) /**< pointer to the chunk */
{
  mem_check_pools ();

  mem_pool_chunk_t *chunk_to_free_p = (mem_pool_chunk_t *) chunk_p;

  chunk_to_free_p->u.free.next_p = mem_free_chunk_p;
  mem_free_chunk_p = chunk_to_free_p;

  VALGRIND_NOACCESS_SPACE (chunk_to_free_p, MEM_POOL_CHUNK_SIZE);

#if 0
  mem_free_chunks_number++;
#endif

  MEM_POOLS_STAT_FREE_CHUNK ();

  /**
   * If all chunks of the pool are free, free the pool itself.
   */
#if 0
  if (pool_state_p->free_chunks_number == MEM_POOL_CHUNKS_NUMBER)
  {
    if (prev_pool_state_p != NULL)
    {
      prev_pool_state_p->next_pool_cp = pool_state_p->next_pool_cp;

      if (next_pool_state_p != NULL)
      {
        next_pool_state_p->prev_pool_cp = pool_state_p->prev_pool_cp;
      }
    }
    else
    {
      JERRY_ASSERT (mem_pools == pool_state_p);

      mem_pools = MEM_CP_GET_POINTER (mem_pool_state_t, pool_state_p->next_pool_cp);

      if (mem_pools != NULL)
      {
        mem_pools->prev_pool_cp = MEM_CP_NULL;
      }
    }

    mem_free_chunks_number -= MEM_POOL_CHUNKS_NUMBER;

    mem_heap_free_block ((uint8_t*) pool_state_p);

    MEM_POOLS_STAT_FREE_POOL ();
  }
  else if (mem_pools != pool_state_p)
  {
    JERRY_ASSERT (prev_pool_state_p != NULL);

    prev_pool_state_p->next_pool_cp = pool_state_p->next_pool_cp;

    if (next_pool_state_p != NULL)
    {
      next_pool_state_p->prev_pool_cp = pool_state_p->prev_pool_cp;
    }

    pool_state_p->prev_pool_cp = MEM_CP_NULL;
    MEM_CP_SET_NON_NULL_POINTER (pool_state_p->next_pool_cp, mem_pools);

    MEM_CP_SET_NON_NULL_POINTER (mem_pools->prev_pool_cp, pool_state_p);

    mem_pools = pool_state_p;
  }
#endif

  mem_check_pools ();
} /* mem_pools_free */

/**
 * Check correctness of pool allocator state
 */
static void
mem_check_pools (void)
{
#ifndef JERRY_DISABLE_HEAVY_DEBUG
#if 0
  JERRY_ASSERT (mem_pools == NULL || mem_pools->prev_pool_cp == MEM_CP_NULL);

  mem_pool_state_t *last_pool_p = NULL;

  for (mem_pool_state_t *pool_iter_p = mem_pools;
       pool_iter_p != NULL;
       last_pool_p = pool_iter_p, pool_iter_p = MEM_CP_GET_POINTER (mem_pool_state_t, pool_iter_p->next_pool_cp))
  {
    JERRY_ASSERT (pool_iter_p == mem_pools || last_pool_p != NULL);

    met_free_chunks_number += pool_iter_p->free_chunks_number;
  }
  JERRY_ASSERT (mem_pools == NULL || last_pool_p != NULL);
#endif

  size_t met_free_chunks_number = 0;
  for (mem_pool_chunk_t *chunk_iter_p = mem_free_chunk_p, *next_chunk_p;
       chunk_iter_p != NULL;
       chunk_iter_p = next_chunk_p)
  {
    VALGRIND_DEFINED_SPACE (chunk_iter_p, MEM_POOL_CHUNK_SIZE);

    next_chunk_p = chunk_iter_p->u.free.next_p;

    VALGRIND_UNDEFINED_SPACE (chunk_iter_p, MEM_POOL_CHUNK_SIZE);

    met_free_chunks_number++;
  }
  JERRY_ASSERT (met_free_chunks_number == mem_free_chunks_number);

#if 0
  for (mem_pool_state_t *pool_iter_p = last_pool_p;
       pool_iter_p != NULL;
       pool_iter_p = MEM_CP_GET_POINTER (mem_pool_state_t, pool_iter_p->prev_pool_cp))
  {
    JERRY_ASSERT (pool_iter_p->prev_pool_cp != MEM_CP_NULL || pool_iter_p == mem_pools);
  }
#endif
#endif /* !JERRY_DISABLE_HEAVY_DEBUG */
} /* mem_check_pools */

#ifdef MEM_STATS
/**
 * Get pools memory usage statistics
 */
void
mem_pools_get_stats (mem_pools_stats_t *out_pools_stats_p) /**< out: pools' stats */
{
  JERRY_ASSERT (out_pools_stats_p != NULL);

  *out_pools_stats_p = mem_pools_stats;
} /* mem_pools_get_stats */

/**
 * Reset peak values in memory usage statistics
 */
void
mem_pools_stats_reset_peak (void)
{
  mem_pools_stats.peak_pools_count = mem_pools_stats.pools_count;
  mem_pools_stats.peak_allocated_chunks = mem_pools_stats.allocated_chunks;
} /* mem_pools_stats_reset_peak */

/**
 * Initalize pools' memory usage statistics account structure
 */
static void
mem_pools_stat_init (void)
{
  memset (&mem_pools_stats, 0, sizeof (mem_pools_stats));
} /* mem_pools_stat_init */

/**
 * Account allocation of a pool
 */
static void
mem_pools_stat_alloc_pool (void)
{
  mem_pools_stats.pools_count++;
#if 0
  mem_pools_stats.free_chunks = mem_free_chunks_number;
#endif

  if (mem_pools_stats.pools_count > mem_pools_stats.peak_pools_count)
  {
    mem_pools_stats.peak_pools_count = mem_pools_stats.pools_count;
  }
  if (mem_pools_stats.pools_count > mem_pools_stats.global_peak_pools_count)
  {
    mem_pools_stats.global_peak_pools_count = mem_pools_stats.pools_count;
  }
} /* mem_pools_stat_alloc_pool */

/**
 * Account freeing of a pool
 */
static void
mem_pools_stat_free_pool (void)
{
  JERRY_ASSERT (mem_pools_stats.pools_count > 0);

  mem_pools_stats.pools_count--;
#if 0
  mem_pools_stats.free_chunks = mem_free_chunks_number;
#endif
} /* mem_pools_stat_free_pool */

/**
 * Account allocation of chunk in a pool
 */
static void
mem_pools_stat_alloc_chunk (void)
{
  JERRY_ASSERT (mem_pools_stats.free_chunks > 0);

  mem_pools_stats.allocated_chunks++;
  mem_pools_stats.free_chunks--;

  if (mem_pools_stats.allocated_chunks > mem_pools_stats.peak_allocated_chunks)
  {
    mem_pools_stats.peak_allocated_chunks = mem_pools_stats.allocated_chunks;
  }
  if (mem_pools_stats.allocated_chunks > mem_pools_stats.global_peak_allocated_chunks)
  {
    mem_pools_stats.global_peak_allocated_chunks = mem_pools_stats.allocated_chunks;
  }
} /* mem_pools_stat_alloc_chunk */

/**
 * Account freeing of chunk in a pool
 */
static void
mem_pools_stat_free_chunk (void)
{
  JERRY_ASSERT (mem_pools_stats.allocated_chunks > 0);

  mem_pools_stats.allocated_chunks--;
  mem_pools_stats.free_chunks++;
} /* mem_pools_stat_free_chunk */
#endif /* MEM_STATS */

/**
 * @}
 */
/**
 * @}
 */
