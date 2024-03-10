#include<stdio.h>
#include<stdlib.h>
#include<pthread.h>
#include<semaphore.h>
#include "hw2_output.h"

//////////////////////////////////////
/* 2 NxM matrix sum --> A + B = J   */
/* 2 MxK matrix sum --> C + D = L   */
/* J(NxM) * L(MxK) = R(NxK)         */
/* total N+M+N threads              */                                      
//////////////////////////////////////

typedef struct{
    int row_index;
    int col_number;
    int** result
} sum_thread_args;

typedef struct{
    int row_index;
} mult_thread_args;

int N,M,K;

int **A, **B, **J; /*(NxM)*/
int **C, **D, **L; /*(MxK)*/
int **R; /*(NxK)*/

//Semaphores
sem_t *J_row_ready; /*(N)*/
sem_t **L_col_ready; /*(MxK)*/

//Threads
pthread_t *thread;
sum_thread_args *sum_args;
mult_thread_args *mult_args;

pthread_mutex_t mutex;

//inputs and malloc J,L,R
void takeInput();
void printInputs();
void printResults();
//malloc and init sem_t's
void initSemaphores();
void initThreads();
void joinThreads();


void matrixSum(void *tmp_args);
void matrixMultiplication(void *tmp_args);

// free all mallocs
void freeMems();

void takeInputs()
{
    //inputs for matrix A(NxM)
    scanf("%d %d",&N,&M);
    A=malloc(sizeof(int*)*N);
    for(int i=0;i<N;i++)
    {
        A[i]=malloc(sizeof(int)*M);
        for(int j=0;j<M;j++)
        {
            scanf("%d",&A[i][j]);
        }
    }

    //inputs for matrix B(NxM)
    scanf("%d %d",&N,&M);
    B=malloc(sizeof(int*)*N);
    for(int i=0;i<N;i++)
    {
        B[i]=malloc(sizeof(int)*M);
        for(int j=0;j<M;j++)
        {
            scanf("%d",&B[i][j]);
        }
    }

    //inputs for matrix C(MxK)
    scanf("%d %d",&M,&K);
    C=malloc(sizeof(int*)*M);
    for(int i=0;i<M;i++)
    {
        C[i]=malloc(sizeof(int)*K);
        for(int j=0;j<K;j++)
        {
            scanf("%d",&C[i][j]);
        }
    }

    //inputs for matrix D(MxK)
    scanf("%d %d",&M,&K);
    D=malloc(sizeof(int*)*M);
    for(int i=0;i<M;i++)
    {
        D[i]=malloc(sizeof(int)*K);
        for(int j=0;j<K;j++)
        {
            scanf("%d",&D[i][j]);
        }
    }

    J=malloc(sizeof(int*)*N);
    for(int i=0;i<N;i++)
    {
        J[i]=malloc(sizeof(int)*M);
        for(int j=0;j<M;j++)
        {
            J[i][j]=0;
        }
    }

    L=malloc(sizeof(int*)*M);
    for(int i=0;i<M;i++)
    {
        L[i]=malloc(sizeof(int)*K);
        for(int j=0;j<K;j++)
        {
            L[i][j]=0;
        }
    }

    R=malloc(sizeof(int*)*N);
    for(int i=0;i<N;i++)
    {
        R[i]=malloc(sizeof(int)*K);
        for(int j=0;j<K;j++)
        {
            R[i][j]=0;
        }
    }
}
void printInputs()
{
    printf("A (%dx%d)\n",N,M);
    for(int i=0;i<N;i++)
    {
        for(int j=0;j<M;j++)
        {
            printf("%d ",A[i][j]);
        }
        printf("\n");
    }

    printf("B (%dx%d)\n",N,M);
    for(int i=0;i<N;i++)
    {
        for(int j=0;j<M;j++)
        {
            printf("%d ",B[i][j]);
        }
        printf("\n");
    }

    printf("C (%dx%d)\n",M,K);
    for(int i=0;i<M;i++)
    {
        for(int j=0;j<K;j++)
        {
            printf("%d ",C[i][j]);
        }
        printf("\n");
    }

    printf("D (%dx%d)\n",M,K);
    for(int i=0;i<M;i++)
    {
        for(int j=0;j<K;j++)
        {
            printf("%d ",D[i][j]);
        }
        printf("\n");
    }
}
void printResults()
{
    // printf("A + B = J\n");
    // for(int i=0;i<N;i++)
    // {
    //     for(int j=0;j<M;j++)
    //     {
    //         printf("%d ",A[i][j]);
    //     }
    //     printf("\t");
    //     for(int j=0;j<M;j++)
    //     {
    //         printf("%d ",B[i][j]);
    //     }
    //     printf("\t");
    //     for(int j=0;j<M;j++)
    //     {
    //         printf("%d ",J[i][j]);
    //     }
    //     printf("\n");
    // }

    // printf("C + D = L\n");
    // for(int i=0;i<M;i++)
    // {
    //     for(int j=0;j<K;j++)
    //     {
    //         printf("%d ",C[i][j]);
    //     }
    //     printf("\t");
    //     for(int j=0;j<K;j++)
    //     {
    //         printf("%d ",D[i][j]);
    //     }
    //     printf("\t");
    //     for(int j=0;j<K;j++)
    //     {
    //         printf("%d ",L[i][j]);
    //     }
    //     printf("\n");
    // }

    // printf("Sum 1\n");
    // for(int i=0;i<N;i++)
    // {
    //     for(int j=0;j<M;j++)
    //     {
    //         printf("%d ",J[i][j]);
    //     }
    //     printf("\n");
    // }

    // printf("Sum2\n");
    // for(int i=0;i<M;i++)
    // {
    //     for(int j=0;j<K;j++)
    //     {
    //         printf("%d ",L[i][j]);
    //     }
    //     printf("\n");
    // }

    // printf("Mult\n");
    for(int i=0;i<N;i++)
    {
        for(int j=0;j<K;j++)
        {
            printf("%d ",R[i][j]);
        }
        printf("\n");
    }
}

void freeMems()
{

    //Free matrix A
    for(int i=0;i<N;i++) free(A[i]);
    free(A);

    //Free matrix B
    for(int i=0;i<N;i++) free(B[i]);
    free(B);

    //Free matrix C
    for(int i=0;i<M;i++) free(C[i]);
    free(C);

    //Free matrix D
    for(int i=0;i<M;i++) free(D[i]);
    free(D);

    //Free matrix J - First sum
    for(int i=0;i<N;i++) free(J[i]);
    free(J);

    //Free matrix L - Second sum
    for(int i=0;i<M;i++) free(L[i]);
    free(L);

    //Free matrix R - Result
    for(int i=0;i<N;i++) free(R[i]);
    free(R);

    // Free semophore J
    for(int i=0;i<N;i++) sem_destroy(&J_row_ready[i]);
    free(J_row_ready);

    // Free semophore L
    for(int i=0;i<M;i++)
    {
        for(int j=0;j<K;j++) sem_destroy(&L_col_ready[i][j]);
        free(L_col_ready[i]);    
    }
    free(L_col_ready);

    //Free threads
    free(thread);

    //Free sum_args
    free(sum_args);

    //Free mult_args
    free(mult_args);
}
void matrixSum(void *tmp_args)
{
    sum_thread_args *args=tmp_args;
    int row_index=args->row_index;
    int col_number=args->col_number;

    int** result=args->result;
    for(int i=0;i<col_number;i++)
    {
        if(result==J) /* J = first of multiplication */
        {
            result[row_index][i]=A[row_index][i]+B[row_index][i];
            hw2_write_output(0,row_index+1,i+1,result[row_index][i]);

            if (i==(M-1)) /*end of row, last column*/
            {
                if(sem_post(&J_row_ready[row_index]))
                {
                    perror("matrixSum sem_post error J");
                }
            }
        }
        else{ /* L = second of multiplication */

            result[row_index][i]=C[row_index][i]+D[row_index][i];
            hw2_write_output(1,row_index+1,i+1,result[row_index][i]);

            for(int j=0;j<N;j++) /* signal to all rows */
            {
                if(sem_post(&L_col_ready[row_index][i]))
                {
                    perror("matrixSum sem_post error L");
                }
            } 
        }
    }
    pthread_exit(NULL);
}

void matrixMultiplication(void *tmp_args)
{
    mult_thread_args *args=tmp_args;
    int row_index=args->row_index;
    sem_wait(&J_row_ready[row_index]);
    for(int i=0;i<K;i++) /*for column*/
    {
        int total=0;
        for(int j=0;j<M;j++) /*col size of result*/
        {
            sem_wait(&L_col_ready[j][i]); 
            total+=J[row_index][j]*L[j][i];
        }
        R[row_index][i] = total;
        hw2_write_output(2,row_index+1,i+1,total);
    }
    pthread_exit(NULL);
}

void initSemaphores()
{
    J_row_ready=malloc(sizeof(sem_t)*N);
    for(int i=0;i<N;i++)
    {
        if(sem_init(&J_row_ready[i],0,0))
        {
            perror("initSemaphore J_ready error");
        }
    }

    L_col_ready=malloc(sizeof(sem_t*)*M);
    for(int i=0;i<M;i++)
    {
        L_col_ready[i]=malloc(sizeof(sem_t)*K);
        for(int j=0;j<K;j++)
        {
            if(sem_init(&L_col_ready[i][j],0,0))
            {
                perror("initSemaphore L_ready error");
            }
        }
        
    }
}

void initThreads()
{
    thread=malloc(sizeof(pthread_t)*(N+M+N));
    sum_args=malloc(sizeof(sum_thread_args)*(N+M));
    mult_args=malloc(sizeof(mult_thread_args)*(N));
    
    for(int i=0;i<N;i++) /* first sum threads */
    {
        sum_args[i].row_index=i;
        sum_args[i].col_number=M;
        sum_args[i].result=J;
        if(pthread_create(&thread[i], NULL, (void*)matrixSum, &sum_args[i]))
        {
            perror("initThreads first sum");
        }
    }

    for(int i=N;i<N+M;i++) /* second sum threads */
    {
        sum_args[i].row_index=i-N;
        sum_args[i].col_number=K;
        sum_args[i].result=L;
        if(pthread_create(&thread[i], NULL, (void*)matrixSum, &sum_args[i]))
        {
            perror("initThreads second sum");
        }
    }

    for(int i=0;i<N;i++) /* multiplication threads */
    {
        mult_args[i].row_index=i;
        if(pthread_create(&thread[N+M+i], NULL, (void*)matrixMultiplication, &mult_args[i]))
        {
            perror("initThreads multiplication error");
        }
    }
}

void joinThreads()
{
    for(int i=0;i<2*N+M;i++)
    {
        pthread_join(thread[i],NULL);
        pthread_detach(thread[i]);
    }
}

int main()
{

    hw2_init_output();
    
    takeInputs();
    
    initSemaphores();
    
    initThreads();
    
    joinThreads();

    printResults();

    freeMems();
    return 0;
}