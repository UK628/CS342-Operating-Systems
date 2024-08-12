#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "rm.h"
#include <stdbool.h>

// global variables

int DA;  // indicates if deadlocks will be avoided or not
int N;   // number of processes
int M;   // number of resource types
int ExistingRes[MAXR]; // Existing resources vector
pthread_mutex_t mutex;
pthread_cond_t condVar = PTHREAD_COND_INITIALIZER;

pthread_t* threadIds;

int** MAX;    // Maximum resources matrix
int** ALLO;   // Allocation matrix
int** NEED;   // Need matrix
int** REQUEST;  // Request matrix
int* AVAIL;   // Available resources vector

// end of global variables

// Function to check if the system is in a safe state
int isSafe(int* temp_avail,int** temp_alloc,int** temp_need) {

    //Step 1: Initiliaze work and finish vectors
    int* work = malloc(M * sizeof(int));
    bool* finish = malloc(N * sizeof(bool));

    // Initialize work vector with available resources
    for (int i = 0; i < M; ++i) {
        work[i] = temp_avail[i];
    }

    // Initialize finish vector
    for (int i = 0; i < N; ++i) {
        finish[i] = false;
    }

    // Step 2: Find an i such that both a) Finish[i] = false b) Needi <= Work
    bool found;
    do {
        found = false;
        for (int i = 0; i < N; ++i) {
            if (!finish[i]) {
                int j;
                for (j = 0; j < M; ++j) { 
                    if (temp_need[i][j] > work[j]) {
                        break;
                    }
                }
                if (j == M) {
                    // We found a process that can be finished
                    found = true; 
                    //step 3
                    for (int k = 0; k < M; ++k) {
                        work[k] += temp_alloc[i][k];
                    }
                    finish[i] = true;
                }
            }
        }
    } while (found); //if you reach to step 3 go to step 2

    // Step 4: Check if all processes have been finished
    for (int i = 0; i < N; ++i) {
        if (!finish[i]) {
            return 0;  // System is not in a safe state
        }
    }

    return 1;  // System is in a safe state
}

int findIndex(pthread_t id) {

    for(int i = 0; i < N; i++) {
        if(pthread_equal(threadIds[i], id)) {
            //printf("Found THREAD ID!! : %d \n", i);
            //fflush(stdout);
            return i;
        }
    } 

    printf("ERROR: Could not find the thread ID! \n");
    fflush(stdout);
    return -1;
}

int rm_thread_started(int tid)
{
    if (tid < 0 || tid >= N)
        return -1;  // Invalid thread ID

    pthread_mutex_lock(&mutex);  // Acquire the mutex lock for thread-safety

    // Set the current thread ID
    threadIds[tid] = pthread_self();

    pthread_mutex_unlock(&mutex);  // Release the mutex lock

    return 0;  // Thread started successfully
}


int rm_thread_ended()
{
    pthread_mutex_lock(&mutex);  // Acquire the lock to ensure thread safety

    int ret = 0;

    // Release the allocated resources for the current thread
    int threadIndex = findIndex(pthread_self());
    for (int i = 0; i < M; ++i) {
        AVAIL[i] += ALLO[threadIndex][i];  // Release allocated resources
        ALLO[threadIndex][i] = 0;          // Reset allocation for the thread
    }

    // Reset the MAX, NEED, and REQUEST matrices for the current thread
    for (int i = 0; i < M; ++i) {
        MAX[threadIndex][i] = 0;
        NEED[threadIndex][i] = 0;
        REQUEST[threadIndex][i] = 0;
    }

    threadIds[threadIndex] = 0;

    pthread_mutex_unlock(&mutex);  // Release the lock

    return ret;
}


int rm_claim (int claim[])
{
    pthread_mutex_lock(&mutex);  // Acquire the mutex lock for thread-safety

    // Check if the claimed resources exceed the existing resources
    for (int i = 0; i < M; ++i) {

        if (claim[i] > ExistingRes[i]) {
            pthread_mutex_unlock(&mutex);
            return -1;  // Error: Claimed instances exceed existing instances
        }
    }

    // Populate the MaxDemand matrix with the claim information
    int curr = findIndex(pthread_self());
    for (int i = 0; i < M; ++i) {
        MAX[curr][i] = claim[i];
        NEED[curr][i] = claim[i];
    }

    pthread_mutex_unlock(&mutex);  // Release the mutex lock
    return 0;  // Claim successful
}


int rm_init(int p_count, int r_count, int r_exist[],  int avoid)
{
    // Check if the values are valid
    if (p_count <= 0 || r_count <= 0 || r_exist == NULL)
        return -1;

    int i,j;
    int ret = 0;
    
    DA = avoid;
    N = p_count;
    M = r_count;
    // initialize (create) resources
    AVAIL = (int*)malloc(M * sizeof(int));
    MAX = (int**)malloc(N * sizeof(int*));
    ALLO = (int**)malloc(N * sizeof(int*));
    NEED = (int**)malloc(N * sizeof(int*));
    REQUEST = (int**)malloc(N * sizeof(int*));
    threadIds = (pthread_t*) malloc(N * sizeof(pthread_t));
    

    for (i = 0; i < M; ++i) {
        ExistingRes[i] = r_exist[i];
        AVAIL[i] = r_exist[i];
    }
    // resources initialized (created)

    // Initialize matrices
    for (i = 0; i < N; ++i) {
        MAX[i] = (int*)malloc(M * sizeof(int));
        ALLO[i] = (int*)malloc(M * sizeof(int));
        NEED[i] = (int*)malloc(M * sizeof(int));
        REQUEST[i] = (int*)malloc(M * sizeof(int));

        for (j = 0; j < M; ++j) {
            MAX[i][j] = 0;
            ALLO[i][j] = 0;
            NEED[i][j] = 0;
            REQUEST[i][j] = 0;
        }
    }
    //....
    // Initialize mutex lock
    pthread_mutex_init(&mutex, NULL);

    return  (ret);
}


int rm_request (int request[])
{
    pthread_mutex_lock(&mutex);  // Acquire the mutex lock for thread-safety
    int curr_thread = findIndex(pthread_self());

    // fill request matrix
    for (int i = 0; i < M; ++i) {
        REQUEST[curr_thread][i] = request[i];
    }

    // Check if the requested resources exceed the existing resources
    for (int i = 0; i < M; ++i) {
        if (request[i] > ExistingRes[i]) {
            pthread_mutex_unlock(&mutex);
            return -1;  // Error: Requested instances exceed existing instances
        }
    }


    // Check if deadlock avoidance is enabled
    if (DA) {
        // Step 1: Check if request_i <= need_i
        bool exceedMaxClaim = false;
        for (int i = 0; i < M; ++i) {
            if (request[i] > NEED[curr_thread][i]) {
                exceedMaxClaim = true;
                break;
            }
        }

        if (exceedMaxClaim) {
            pthread_mutex_unlock(&mutex);
            return -1;  // Error: Requested instances exceed maximum claim
        }

        // Step 2: Check if request_i <= available
        // Check if the requested resources are available
        for (int i = 0; i < M; ++i) {
            while (request[i] > AVAIL[i]) {
                // Block the thread using a condition variable until the resources are available
                pthread_cond_wait(&condVar, &mutex);
                i = 0; // The previously checked available resources before the ith index may not be availbe now since we might sleep and release the lock, hence recheck from the beginning.
            }
        }

        // Step 3: Pretend to allocate the requested resources
        int* temp_avail = malloc(M * sizeof(int));
        int** temp_alloc = malloc(N * sizeof(int*));
        int** temp_need = malloc(N * sizeof(int*));
        
        // Copy the current state to temporary variables
        for (int i = 0; i < M; ++i) {
            temp_avail[i] = AVAIL[i];
        }

        for (int i = 0; i < N; ++i) {
            temp_alloc[i] = malloc(M * sizeof(int));
            temp_need[i] = malloc(M * sizeof(int));
            for (int j = 0; j < M; ++j) {
                temp_alloc[i][j] = ALLO[i][j];
                temp_need[i][j] = NEED[i][j];
            }
        }

        // New state calculation
        for (int i = 0; i < M; ++i) {
            temp_avail[i] -= request[i];
            temp_alloc[curr_thread][i] += request[i];
            temp_need[curr_thread][i] -= request[i];
        }

        // Run safety check algorithm on the new state
        while(!isSafe(temp_avail, temp_alloc, temp_need)) {
            pthread_cond_wait(&condVar, &mutex);

            //before checking if the new state is safe, we need to update our temp variables
            // Copy the current state to temporary variables
            for (int i = 0; i < M; ++i) {
                temp_avail[i] = AVAIL[i];
            }

            for (int i = 0; i < N; ++i) {
                for (int j = 0; j < M; ++j) {
                    temp_alloc[i][j] = ALLO[i][j];
                    temp_need[i][j] = NEED[i][j];
                }
            }

            // New state calculation
            for (int i = 0; i < M; ++i) {
                temp_avail[i] -= request[i];
                temp_alloc[curr_thread][i] += request[i];
                temp_need[curr_thread][i] -= request[i];
            }
        }

        // The requested resources are allocated to P_i. P_i can proceed.
        for (int i = 0; i < M; ++i) {
            AVAIL[i] = temp_avail[i];
            ALLO[curr_thread][i] = temp_alloc[curr_thread][i];
            NEED[curr_thread][i] = temp_need[curr_thread][i];
        }


         
        // Free temporary variables
        free(temp_avail);
        for (int i = 0; i < N; ++i) {
            free(temp_alloc[i]);
            free(temp_need[i]);
        }
        free(temp_alloc);
        free(temp_need);
    } else {
        // Check if the requested resources are available
        for (int i = 0; i < M; ++i) {
            while (request[i] > AVAIL[i]) {
                // Block the thread using a condition variable until the resources are available
                pthread_cond_wait(&condVar, &mutex);
                i = 0; // The previously checked available resources before the ith index may not be availbe now since we might sleep and release the lock, hence recheck from the beginning.
            }
        }

        // Allocate the requested resources
        for (int i = 0; i < M; ++i) {
            AVAIL[i] -= request[i];
            ALLO[curr_thread][i] += request[i];
            NEED[curr_thread][i] -= request[i];
        }

    }
    // empty request matrix
    for (int i = 0; i < M; ++i) {
        REQUEST[curr_thread][i] = 0;
    }

    pthread_mutex_unlock(&mutex);  // Release the mutex lock
    return 0;  // Resources allocated successfully
}


int rm_release (int release[])
{
    pthread_mutex_lock(&mutex);  // Acquire the mutex lock for thread-safety
    int curr_thread = findIndex(pthread_self());

    // Check if the released resources exceed the allocated resources
    for (int i = 0; i < M; ++i) {

        if (release[i] > ALLO[curr_thread][i]) {
            pthread_mutex_unlock(&mutex);
            return -1;  // Error: Trying to release more instances than allocated
        }
    }

    // Release the indicated resource instances
    for (int i = 0; i < M; ++i) {
        AVAIL[i] += release[i];
        ALLO[curr_thread][i] -= release[i];
    }

    // Wake up blocked threads (if any)
    pthread_cond_broadcast(&condVar);
    
    pthread_mutex_unlock(&mutex);  // Release the mutex lock
    return 0;  // Resources released successfully
}


int rm_detection() {
    pthread_mutex_lock(&mutex);

    int* work = malloc(M * sizeof(int));
    int* finish = malloc(N * sizeof(int));

    // Step 1: Initialization
    for (int i = 0; i < M; i++) {
        work[i] = AVAIL[i];
    }

    int flag = 1;
    for (int i = 0; i < N; i++) {
        flag = 1;

        for(int j = 0; j < M; j++) {
            if (REQUEST[i][j] != 0) {
                flag = 0;
                break;
            } 
        } 
        
        finish[i] = flag;
    }

    int deadlockCount = 0;  // Number of deadlocked processes

    // Step 2: Find an index i such that Finish[i] == false and Request_i <= Work
    int found = 1;
    while (found) {
        found = 0;

        for (int i = 0; i < N; i++) {
            if (!finish[i]) {
                int safe = 1;
                for (int j = 0; j < M; j++) {
                    if (REQUEST[i][j] > work[j]) {
                        safe = 0;
                        break;
                    }
                }

                if (safe) {
                    // Step 3: Update Work and Finish
                    for (int j = 0; j < M; j++) {
                        work[j] += ALLO[i][j];
                    }

                    finish[i] = 1;  // True
                    found = 1;
                }
            }
        }
    }

    // Step 4: Check for deadlock
    for (int i = 0; i < N; i++) {
        if (!finish[i]) {
            deadlockCount++;
        }
    }

    free(work);
    free(finish);

    pthread_mutex_unlock(&mutex);

    return deadlockCount;
}


void rm_print_state (char hmsg[])
{
    pthread_mutex_lock(&mutex);

    printf("##########################\n");
    printf("%s\n", hmsg);
    printf("##########################\n");

    printf("Exist:\n");
    printf("%-4s", "");
    for (int i = 0; i < M; ++i)
        printf("R%d ", i);
    printf("\n");
    printf("%-4s", "");
    for (int i = 0; i < M; ++i)
        printf("%-3d" , ExistingRes[i]);
    printf("\n");

    printf("Available:\n");
    printf("%-4s", "");
    for (int i = 0; i < M; ++i)
        printf("R%d ", i);
    printf("\n");
    printf("%-4s", "");
    for (int i = 0; i < M; ++i)
        printf("%-2d ", AVAIL[i]);
    printf("\n");

    printf("Allocation:\n");
    for (int i = 0; i < N; ++i) {
        printf("T%d: ", i);
        for (int j = 0; j < M; ++j)
            printf("%-2d ", ALLO[i][j]);
        printf("\n");
    }

    printf("Request:\n");
    for (int i = 0; i < N; ++i) {
        printf("T%d: ", i);
        for (int j = 0; j < M; ++j)
            printf("%-2d ", REQUEST[i][j]);
        printf("\n");
    }

    printf("MaxDemand:\n");
    for (int i = 0; i < N; ++i) {
        printf("T%d: ", i);
        for (int j = 0; j < M; ++j)
            printf("%-2d ", MAX[i][j]);
        printf("\n");
    }

    printf("Need:\n");
    for (int i = 0; i < N; ++i) {
        printf("T%d: ", i);
        for (int j = 0; j < M; ++j)
            printf("%-2d ", NEED[i][j]);
        printf("\n");
    }

    printf("##########################\n");
    pthread_mutex_unlock(&mutex);  // Release the mutex lock

    return;
}