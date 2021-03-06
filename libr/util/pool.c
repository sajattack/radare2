/* radare - LGPL - Copyright 2010-2011 pancake<nopcode.org> */

#include <r_util.h>
#include <stdlib.h>

#define ALLOC_POOL_SIZE 1024
#define ALLOC_POOL_COUNT 128

// TODO: add api to serialize/deserialize memory pools from/to disk
// This can be useful when the application is swapping (userland swapping?)
// Do user-swapping takes sense?
// This API have several bugs that wasnt considered at the moment of developing it
// * pointers are not aligned, so they may result in crashes in arm/mips
// * extra padding to avoid memory overruns is not considerd either
// * pool have some hardcoded limits which shouldnt be happening
// FTR: this was inspired by the GLibSlices API, and its not used in r2 at all

R_API RMemoryPool *r_mem_pool_deinit(RMemoryPool *pool) {
	int i;
	for (i = 0; i < pool->npool; i++) {
		free (pool->nodes[i]);
	}
	free (pool->nodes);
	pool->nodes = NULL;
	return pool;
}

R_API RMemoryPool *r_mem_pool_new(int nodesize, int poolsize, int poolcount) {
	RMemoryPool *mp = R_NEW (RMemoryPool);
	if (mp) {
		if (poolsize < 1) {
			poolsize = ALLOC_POOL_SIZE;
		}
		if (poolcount < 1) {
			poolcount = ALLOC_POOL_COUNT;
		}
		// TODO: assert nodesize?;
		mp->poolsize = poolsize;
		mp->poolcount = poolcount;
		mp->nodesize = nodesize;
		mp->npool = -1;
		mp->ncount = mp->poolsize;	// force init
		mp->nodes = (ut8 **) calloc (sizeof (void *), mp->poolcount);
		if (!mp->nodes) {
			R_FREE (mp);
			return NULL;
		}
	}
	return mp;
}

R_API RMemoryPool *r_mem_pool_free(RMemoryPool *pool) {
	r_mem_pool_deinit (pool);
	free (pool);
	return NULL;
}

R_API void *r_mem_pool_alloc(RMemoryPool *pool) {
	if (!pool) {
		return NULL;
	}
	if (pool->ncount >= pool->poolsize) {
		if (++pool->npool >= pool->poolcount) {
			eprintf ("FAIL: Cannot allocate more memory in the pool\n");
			return NULL;
		}
		pool->nodes[pool->npool] = malloc (pool->nodesize * pool->poolsize);
		if (!pool->nodes[pool->npool]) {
			return NULL;
		}
		pool->ncount = 0;
	}
	// TODO: fix warning
	return (void *) (&(pool->nodes[pool->npool][pool->ncount++]));
}

// TODO: not implemented
R_API int r_mem_pool_dealloc(RMemoryPool *pool, void *p) {
	return false;
}

/* poolfactory */
/* TODO: must tune */
static RPoolFactory single_pf = {
	0
};

R_API RPoolFactory *r_poolfactory_instance() {
	return &single_pf;
}

R_API void r_poolfactory_init(int limit) {
	if (limit < 1) {
		return;
	}
	single_pf.limit = limit + 1;
	free (single_pf.pools);
	single_pf.pools = calloc (limit, sizeof (RMemoryPool *));
	if (!single_pf.pools) {
		return;
	}
}

R_API RPoolFactory *r_poolfactory_new(int limit) {
	if (limit > 0) {
		int size = sizeof (RMemoryPool *) * limit;
		RPoolFactory *pf = R_NEW0 (RPoolFactory);
		if (!pf) {
			return NULL;
		}
		pf->limit = limit + 1;
		pf->pools = calloc (1, size);
		if (!pf->pools) {
			r_poolfactory_free (pf);
			return NULL;
		}
		return pf;
	}
	return NULL;
}

R_API void *r_poolfactory_alloc(RPoolFactory *pf, int nodesize) {
	if (nodesize > pf->limit) {
		return NULL;
	}
	if (!pf->pools[nodesize]) {
		pf->pools[nodesize] = r_mem_pool_new (nodesize,
			ALLOC_POOL_SIZE, ALLOC_POOL_COUNT);
	}
	return r_mem_pool_alloc (pf->pools[nodesize]);
}

// TODO: not implemented
R_API int r_poolfactory_dealloc(RPoolFactory *pool, void *p) {
	return false;
}
// TODO: add support for ranged limits, from-to
R_API void r_poolfactory_stats(RPoolFactory *pf) {
	int i = 0;
	eprintf ("RPoolFactory stats:\n");
	eprintf ("  limits: %d\n", pf->limit);
	for (i = 0; i < pf->limit; i++) {
		if (pf->pools[i]) {
			eprintf ("  size: %d\t npool: %d\t count: %d\n",
				pf->pools[i]->nodesize,
				pf->pools[i]->npool,
				pf->pools[i]->ncount);
		}
	}
}

R_API void r_poolfactory_free(RPoolFactory *pf) {
	free (pf->pools);
	free (pf);
}
