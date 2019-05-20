#define QUEUE_SIZE  256
#include <stdlib.h>
#include <pthread.h>
#include "threadpool.h"
#include <iostream>
#include <stdlib.h>
using namespace std;


void run_thread(void *arg) {
	cout<<"This is a pthread"<<endl;
}

int main(int argc, char **argv)
{


	if(argc != 2){
		cout<<"Usage: ./threadpool [-t thread_numbers]";
		exit(1);
	}
	int THREADPOOL_THREAD_NUM = atoi(argv[2]);
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

