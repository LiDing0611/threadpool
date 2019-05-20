#define THREADPOOL_THREAD_NUM 32
#define QUEUE_SIZE  256

#include <pthread.h>
#include "threadpool.h"
#include <iostream>
using namespace std;

int tasks = 0, done = 0;


void run_thread(void *arg) {
	cout<<"This is a pthread"<<endl;
}

int main(int argc, char **argv)
{


	threadpool_t *threadpool = threadpool_create(THREADPOOL_THREAD_NUM, QUEUE_SIZE, 0);
	if(threadpool !=NULL){
		cout << "Threadpool started with " << THREADPOOL_THREAD_NUM << " and queue size of " << QUEUE_SIZE<<endl;
	}


	while (threadpool_add(threadpool, &run_thread, NULL, 0) == 0) {
		
	}

	
	threadpool_destroy(threadpool, 0);

	cout<< "done" << endl;

	return 0;
}

