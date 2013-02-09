/*
 * This file is part of LEM, a Lua Event Machine.
 * Copyright 2012 Emil Renner Berthing
 *
 * LEM is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * LEM is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with LEM.  If not, see <http://www.gnu.org/licenses/>.
 */

static unsigned int pool_jobs;
static unsigned int pool_min;
static unsigned int pool_max;
static unsigned int pool_threads;
static time_t pool_delay;
static pthread_mutex_t pool_mutex;
#if _POSIX_SPIN_LOCKS >= 200112L
static pthread_spinlock_t pool_dlock;
#define pool_done_init()   pthread_spin_init(&pool_dlock, PTHREAD_PROCESS_PRIVATE)
#define pool_done_lock()   pthread_spin_lock(&pool_dlock)
#define pool_done_unlock() pthread_spin_unlock(&pool_dlock)
#else
static pthread_mutex_t pool_dlock;
#define pool_done_init()   pthread_mutex_init(&pool_dlock, NULL)
#define pool_done_lock()   pthread_mutex_lock(&pool_dlock)
#define pool_done_unlock() pthread_mutex_unlock(&pool_dlock)
#endif
static pthread_cond_t pool_cond;
static struct lem_async *pool_head;
static struct lem_async *pool_tail;
static struct lem_async *pool_done;
static struct ev_async pool_watch;

static void *
pool_threadfunc(void *arg)
{
	struct lem_async *a;
	struct timespec ts;
	struct timeval tv;

	(void)arg;

	while (1) {
		gettimeofday(&tv, NULL);
		ts.tv_sec  = tv.tv_sec + pool_delay;
		ts.tv_nsec = 1000*tv.tv_usec;

		pthread_mutex_lock(&pool_mutex);
		while ((a = pool_head) == NULL) {
			if (pool_threads <= pool_min) {
				pthread_cond_wait(&pool_cond, &pool_mutex);
				continue;
			}

			if (pthread_cond_timedwait(&pool_cond, &pool_mutex, &ts)
					&& pool_threads > pool_min)
				goto out;
		}
		pool_head = a->next;
		pthread_mutex_unlock(&pool_mutex);

		lem_debug("Running job %p", a);
		a->work(a);
		lem_debug("Bye %p", a);

		pool_done_lock();
		a->next = pool_done;
		pool_done = a;
		pool_done_unlock();

		ev_async_send(LEM_ &pool_watch);
	}
out:
	pool_threads--;
	pthread_mutex_unlock(&pool_mutex);
	return NULL;
}

static void
pool_cb(EV_P_ struct ev_async *w, int revents)
{
	struct lem_async *a;
	struct lem_async *next;

	(void)revents;

	pool_done_lock();
	a = pool_done;
	pool_done = NULL;
	pool_done_unlock();

	for (; a; a = next) {
		pool_jobs--;
		next = a->next;
		a->reap(a);
	}

	if (pool_jobs == 0)
		ev_async_stop(EV_A_ w);
}

static int
pool_init(void)
{
	int ret;

	/*
	pool_jobs = 0;
	pool_min = 0;
	pool_threads = 0;
	*/
	pool_max = INT_MAX;
	pool_delay = 10;
	/*
	pool_head = NULL;
	pool_tail = NULL;
	pool_done = NULL;
	*/

	ev_async_init(&pool_watch, pool_cb);

	ret = pthread_mutex_init(&pool_mutex, NULL);
	if (ret == 0)
		ret = pool_done_init();
	if (ret) {
		lem_log_error("error initializing lock: %s",
				strerror(ret));
		return -1;
	}

	ret = pthread_cond_init(&pool_cond, NULL);
	if (ret) {
		lem_log_error("error initializing cond: %s",
				strerror(ret));
		return -1;
	}

	return 0;
}

static void
pool_spawnthread(void)
{
	pthread_attr_t attr;
	pthread_t thread;
	int ret;

	ret = pthread_attr_init(&attr);
	if (ret)
		goto error;

	ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	if (ret) {
		pthread_attr_destroy(&attr);
		goto error;
	}

	ret = pthread_create(&thread, &attr, pool_threadfunc, NULL);
	pthread_attr_destroy(&attr);
	if (ret)
		goto error;

	return;
error:
	lem_log_error("error spawning thread: %s", strerror(ret));
	lem_exit(EXIT_FAILURE);
}

void
lem_async_put(struct lem_async *a)
{
	int spawn = 0;

	if (pool_jobs == 0)
		ev_async_start(LEM_ &pool_watch);
	pool_jobs++;

	a->next = NULL;

	pthread_mutex_lock(&pool_mutex);
	if (pool_head == NULL) {
		pool_head = a;
		pool_tail = a;
	} else {
		pool_tail->next = a;
		pool_tail = a;
	}
	if (pool_jobs > pool_threads && pool_threads < pool_max) {
		pool_threads++;
		spawn = 1;
	}
	pthread_mutex_unlock(&pool_mutex);
	pthread_cond_signal(&pool_cond);
	if (spawn)
		pool_spawnthread();
}

void
lem_async_config(int delay, int min, int max)
{
	int spawn;

	pool_delay = (time_t)delay;
	pool_min = min;
	pool_max = max;

	pthread_mutex_lock(&pool_mutex);
	spawn = min - pool_threads;
	if (spawn > 0)
		pool_threads = min;
	pthread_mutex_unlock(&pool_mutex);

	for (; spawn > 0; spawn--)
		pool_spawnthread();
}
