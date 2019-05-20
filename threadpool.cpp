#include "threadpool.h"
/**
 *  创建线程池
 *
 *  @int thread_count    派生线程数
 *  @int queue_size      任务队列长度
 *  @int flags           保留参数，未使用
 */
threadpool_t *threadpool_create(int thread_count, int queue_size, int flags)
{
	threadpool_t *pool;
	int i;
	//(void) flags;
	do
	{
		if (thread_count <= 0 || thread_count > MAX_THREADS || queue_size <= 0 || queue_size > MAX_QUEUE) {
			return NULL;
		}
		/* 申请内存创建内存池对象 */
		if ((pool = (threadpool_t *)malloc(sizeof(threadpool_t))) == NULL)
		{
			break;
		}

		/* Initialize */
		pool->thread_count = 0;
		pool->queue_size = queue_size;
		pool->head = pool->tail = pool->count = 0;
		pool->shutdown = pool->started = 0;

		/* Allocate thread and task queue */
		/* 申请线程数组和任务队列所需的内存 */
		pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * thread_count);
		pool->queue = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_size);

		/* Initialize mutex and conditional variable first */
		/* 初始化互斥锁和条件变量 */
		if ((pthread_mutex_init(&(pool->lock), NULL) != 0) ||
			(pthread_cond_init(&(pool->notify), NULL) != 0) ||
			(pool->threads == NULL) ||
			(pool->queue == NULL))
		{
			break;
		}

		/* Start worker threads */
		/* 创建指定数量的线程开始运行 */
		for (i = 0; i < thread_count; i++) {
			if (pthread_create(&(pool->threads[i]), NULL, threadpool_thread, (void*)pool) != 0)
			{
				threadpool_destroy(pool, 0);
				return NULL;
			}
			pool->thread_count++;
			pool->started++;
		}

		return pool;
	} while (false);

	if (pool != NULL)
	{
		threadpool_free(pool);
	}
	return NULL;
}
/**
 *  添加需要执行的任务
 */
int threadpool_add(threadpool_t *pool, void(*function)(void *), void *argument, int flags)
{
	//printf("add to thread pool !\n");
	int err = 0;
	int next;
	//(void) flags;
	if (pool == NULL || function == NULL)
	{
		return THREADPOOL_INVALID;
	}
	if (pthread_mutex_lock(&(pool->lock)) != 0)
	{
		return THREADPOOL_LOCK_FAILURE;
	}
	next = (pool->tail + 1) % pool->queue_size;
	do
	{
		/* Are we full ? */
		if (pool->count == pool->queue_size) {
			err = THREADPOOL_QUEUE_FULL;
			break;
		}
		/* Are we shutting down ? */
		if (pool->shutdown) {
			err = THREADPOOL_SHUTDOWN;
			break;
		}
		/* Add task to queue */
		pool->queue[pool->tail].function = function;
		pool->queue[pool->tail].argument = argument;
		pool->tail = next;
		pool->count += 1;

		/* pthread_cond_broadcast */
		if (pthread_cond_signal(&(pool->notify)) != 0) {
			err = THREADPOOL_LOCK_FAILURE;
			break;
		}
	} while (false);

	if (pthread_mutex_unlock(&pool->lock) != 0) {
		err = THREADPOOL_LOCK_FAILURE;
	}

	return err;
}
/**
 *  销毁存在的线程池
 */
int threadpool_destroy(threadpool_t *pool, int flags)
{
	printf("Thread pool destroy !\n");
	int i, err = 0;

	if (pool == NULL)
	{
		return THREADPOOL_INVALID;
	}

	if (pthread_mutex_lock(&(pool->lock)) != 0)
	{
		return THREADPOOL_LOCK_FAILURE;
	}

	do
	{
		/* Already shutting down */
		if (pool->shutdown) {
			err = THREADPOOL_SHUTDOWN;
			break;
		}

		pool->shutdown = (flags & THREADPOOL_GRACEFUL) ?
			graceful_shutdown : immediate_shutdown;

		/* Wake up all worker threads */
		if ((pthread_cond_broadcast(&(pool->notify)) != 0) ||
			(pthread_mutex_unlock(&(pool->lock)) != 0)) {
			err = THREADPOOL_LOCK_FAILURE;
			break;
		}

		/* Join all worker thread */
		for (i = 0; i < pool->thread_count; ++i)
		{
			if (pthread_join(pool->threads[i], NULL) != 0)
			{
				err = THREADPOOL_THREAD_FAILURE;
			}
		}
	} while (false);

	/* Only if everything went well do we deallocate the pool */
	if (!err)
	{
		threadpool_free(pool);
	}
	return err;
}
/**
 *  释放线程池所申请的内存资源。
 */
int threadpool_free(threadpool_t *pool)
{
	if (pool == NULL || pool->started > 0)
	{
		return -1;
	}

	/* Did we manage to allocate ? */
	if (pool->threads)
	{
		free(pool->threads);
		free(pool->queue);

		/* Because we allocate pool->threads after initializing the
		   mutex and condition variable, we're sure they're
		   initialized. Let's lock the mutex just in case. */
		pthread_mutex_lock(&(pool->lock));
		pthread_mutex_destroy(&(pool->lock));
		pthread_cond_destroy(&(pool->notify));
	}
	free(pool);
	return 0;
}

/**
 *  线程池每个线程所执行的函数
 */
static void *threadpool_thread(void *threadpool)
{
	threadpool_t *pool = (threadpool_t *)threadpool;
	threadpool_task_t task;

	for (;;)
	{
		/* 取得互斥锁资源 */
		pthread_mutex_lock(&(pool->lock));

		/* 用 while 是为了在唤醒时重新检查条件 */
		while ((pool->count == 0) && (!pool->shutdown))
		{
			/* 任务队列为空，且线程池没有关闭时阻塞在这里 */
			pthread_cond_wait(&(pool->notify), &(pool->lock));
		}
		/* 关闭的处理 */
		if ((pool->shutdown == immediate_shutdown) ||
			((pool->shutdown == graceful_shutdown) &&
			(pool->count == 0)))
		{
			break;
		}

		/* Grab our task */
		/* 取得任务队列的第一个任务 */
		task.function = pool->queue[pool->head].function;
		task.argument = pool->queue[pool->head].argument;
		/* 更新 head 和 count */
		pool->head = (pool->head + 1) % pool->queue_size;
		pool->count -= 1;


		/* 释放互斥锁 */
		pthread_mutex_unlock(&(pool->lock));

		/* 开始运行任务 */
		(*(task.function))(task.argument);
	}
	/* 线程将结束，更新运行线程数 */
	--pool->started;

	pthread_mutex_unlock(&(pool->lock));
	pthread_exit(NULL);
	return(NULL);
}