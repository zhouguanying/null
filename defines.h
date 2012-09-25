#ifndef DEFINES_H
#define DEFINES_H

#define __PACKED__ __attribute__ ((packed))

struct tv_params {
	struct timeval      curr;
	struct timeval      last;
	u32                 delta;
        unsigned long long  sum;
        unsigned long long  total;
};

static inline u32 get_timeval_stats(struct tv_params *tv)
{
        if (tv == NULL)
                return 0;

        return (u32) (tv->sum / tv->total);
}

static inline u32 update_timeval_stats(struct tv_params *tv)
{
        if (tv == NULL)
                return 0;

        /* Update stats */
        tv->sum += tv->delta;
        tv->total++;
        
        return 0;
}

static inline u32 update_timeval(struct tv_params *tv)
{
	if (tv == NULL)
		return 0;

	tv->last.tv_usec = tv->curr.tv_usec;
	gettimeofday(&tv->curr, NULL);

	if (tv->curr.tv_usec > tv->last.tv_usec)
		tv->delta = tv->curr.tv_usec - tv->last.tv_usec;
	else if (tv->curr.tv_usec < tv->last.tv_usec)
		tv->delta = (1000000 - tv->last.tv_usec) + tv->curr.tv_usec; 
	else
		tv->delta = 0;


	return tv->delta;
}

static inline struct tv_params * create_timeval(void)
{
	struct tv_params *tv = malloc(sizeof(*tv));
	if (tv == NULL)
		return NULL;
	memset(tv, 0, sizeof(*tv));
	return tv;
}

static inline u32 init_timeval(struct tv_params *tv)
{
	gettimeofday(&tv->curr, NULL);
	return tv->curr.tv_usec;
}

static inline void dbg_timeval(char *type, size_t len, u32 rate)
{
	printf("%s(%s): b=%d u=%d\n", __func__, type, len, rate);
}

#endif /* DEFINES_H */
