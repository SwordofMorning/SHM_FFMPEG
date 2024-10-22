#include "thread_test.h"

pthread_mutex_t mutex_param;
pthread_cond_t cond_output, cond_plus;
int param;

void * test_ouput()
{
    while (1)
    {
        pthread_cond_wait(&cond_output, &mutex_param);
        printf("param_output: %d\n", param);
        pthread_mutex_unlock(&mutex_param);
        pthread_cond_broadcast(&cond_plus);
    }
}

void * test_plus()
{
    while (1)
    {
        pthread_cond_wait(&cond_plus, &mutex_param);
        param++;
        pthread_mutex_unlock(&mutex_param);
        pthread_cond_broadcast(&cond_output);
    }
}

int test_main()
{
    /* Init Mutex */
	pthread_mutex_init(&mutex_param, NULL);
	/* Init Cond */
	pthread_cond_init(&cond_output, NULL);
    pthread_cond_init(&cond_plus, NULL);

    param = 0;
    
    /* Create Threads */
    pthread_t plus, output;
    pthread_create(&output, NULL, test_ouput, NULL);
    pthread_detach(output);
    pthread_create(&plus, NULL, test_plus, NULL);
    pthread_detach(plus);

    /* Wait to Start */
    sleep(3);
    pthread_cond_broadcast(&cond_output);
    sleep(3);

    return 0;
}