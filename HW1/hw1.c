/*
* Author: Robert Taylor
* Homework: Concurrency 1

*REFERENCE: Intel https://software.intel.com/en-us/articles/intel-digital-random-number-generator-drng-software-implementation-guide
*/

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include "mt19937ar.c"

#define MAX 32

#define CPUID(EAX, EBX, ECX, EDX)\
__asm__ __volatile__("cpuid;" :\
"=a"(EAX), "=b"(EBX), "=c"(ECX), "=d"(EDX)\
 : "a"(EAX)\
);

#define _rdrand_generate(x) ({ unsigned char err; asm volatile("rdrand %0; setc %1":"=r"(*x), "=qm"(err)); err;})

struct boofer {
	int value;
	int wait;
};

int count = 0;
int cwait = 0;

unsigned int eax,ebx,ecx,edx;

void cpuid(void){
    CPUID(eax,ebx,ecx,edx);
}

typedef struct rd_struct {
	unsigned int num;
} uint32_t;


struct boofer buff = { 0 , 0 };

pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;  //protect the resource
pthread_cond_t sig_consumer = PTHREAD_COND_INITIALIZER;
pthread_cond_t sig_producer = PTHREAD_COND_INITIALIZER;


int rdrand32_step (uint32_t *rand) {

    unsigned char ok;
    asm volatile ("rdrand %0; setc %1"
        : "=r" (*rand), "=qm" (ok));

    return (int) ok;
}

//This function is responsible for consuming number and signalling the producer.

void *consumer(void *dummy) {
	while (1) {
		//int timer = 0;
		pthread_mutex_lock(&mu);
		pthread_cond_signal(&sig_producer);
		pthread_cond_wait(&sig_consumer, &mu);
	    eax = 0x01;
	    cpuid();

	    //If the left shifted 30th bit is set, supports rdrand
	   /* if(ecx & 1<<30) {
	        //_rdrand_generate(&timer);
	        timer = rdrand32_step(timer);
	        timer = (abs(timer) % 8) + 2;
	    } else {
	        init_genrand(rand());
			timer = (int)genrand_int32();
			timer = (abs(timer) % 8) + 2;
			buff.wait = timer;
	    }*/
		
		printf("Consumer sleeping for: %d seconds\n", cwait);
		sleep(cwait);
		/* Consume (print) the number. */
		printf("Consumer : Value: %d\n", buff.value);
		pthread_mutex_unlock(&mu);
		if (count == MAX) {
			break;
		}
	}
}


//This function is responsible for incrementing and signalling the consumer.
void *producer(void *dummy) {
	while (1) {
		int timer = 0;
		int value = 0;
		pthread_mutex_lock(&mu);
		count = count + 1;
		eax = 0x01;
		uint32_t random;
	    cpuid();
	    //If the left shifted 30th bit is set, supports rdrand
	    if(ecx & 1<<30) {
	    	printf("Using rdrand\n");
	    	//timer for producer
	        rdrand32_step(&random);
			timer = (abs(random.num) % 5) + 3;
			buff.wait = timer;
			//timer for consumer
			rdrand32_step(&random);
			cwait = (abs(random.num) % 8) + 2;
			//random number
			rdrand32_step(&random);
			value = (abs(random.num) % 100);
			buff.value = value;
	    } else {
	        init_genrand(rand());
	        //timer for producer
			timer = (int)genrand_int32();
			timer = (abs(timer) % 5) + 3;
			//timer for consumer
			cwait = (int)genrand_int32();
			cwait = (abs(cwait) % 8) + 2;
			//random number
			value = (int)genrand_int32();
			value = (abs(value) % 100);
			buff.value = value;
			buff.wait = timer;
	    }
    	printf("Producer : Value: %d\t Producer will sleep for: %d seconds\n", buff.value, buff.wait);
    	
	    pthread_cond_signal(&sig_consumer);

	    if (count != MAX) {
      		pthread_cond_wait(&sig_producer, &mu);
      	}

	    pthread_mutex_unlock(&mu);
	    printf("Producer sleeping for: %d seconds\n", buff.wait);
		sleep(buff.wait);
	    if (count == MAX) {
	    	break;
	    }
  	}
}

int main() {
	int rc, i;
	pthread_t t[2];
	srand(time(NULL)); // randomize seed
	//srand((unsigned) time(&timet));

	buff.value = 0;

	/* Create consumer & producer threads. */
	if ((rc= pthread_create(&t[0], NULL, consumer, NULL)))
		printf("Error creating the consumer thread..\n");
	if ((rc= pthread_create(&t[1], NULL, producer, NULL)))
		printf("Error creating the producer thread..\n");

	/* Wait for consumer/producer to exit. */
	for (i= 0; i < 2; i ++)
		pthread_join(t[i], NULL);

	printf("Done..\n");
}