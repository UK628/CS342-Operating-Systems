#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>
#include <stdbool.h>
#include <math.h>

extern int opterr;
extern int optopt;
extern char* optarg;
extern int optind;

#define DEFAULT_NUM_PROCESSORS 2
#define DEFAULT_SCHEDULING_ALGORITHM "RR"
#define DEFAULT_TIME_QUANTUM 20
#define DEFAULT_QUEUE_SELECTION_METHOD "RM"
#define DEFAULT_INPUT_FILE "none"
#define DEFAULT_OUTPUT_FILE "none"
#define DEFAULT_QUEUE_APPROACH 'M'
#define DEFAULT_MODE 1

pthread_mutex_t size_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t finished_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;
int threadCount = 0;
struct timeval simulationStart;
struct ReadyQueue* readyQueues;
struct ReadyQueue finishedItems;

bool isRandomly;
bool willInputGenerated;

struct Args {
    int num_processors;
    char scheduling_algorithm[4];
    int time_quantum;
    char queue_approach;
    char queue_selection_method[3];
    char input_file[64];
    char output_file[64];
    int mode;
    int T;
    int T1;
    int T2;
    int L;
    int L1;
    int L2;
    int PC;
};

struct Args args = {
    .num_processors = DEFAULT_NUM_PROCESSORS,
    .scheduling_algorithm = DEFAULT_SCHEDULING_ALGORITHM,
    .time_quantum = DEFAULT_TIME_QUANTUM,
    .queue_selection_method = DEFAULT_QUEUE_SELECTION_METHOD,
    .queue_approach = DEFAULT_QUEUE_APPROACH,
    .input_file = DEFAULT_INPUT_FILE,
    .output_file = DEFAULT_OUTPUT_FILE,
    .mode = DEFAULT_MODE
};

void print_usage() {
    printf("Usage: mps [-n N] [-a SAP QS] [-s ALG Q] [-i INFILE] [-o OUTFILE] [-r T T1 T2 L L1 L2] [-m OUTMODE]\n");
}

void generate_txt_file(int T, int T1, int T2, int L, int L1, int L2, int PC) {
    // Set up random number generator
    srand(time(NULL));

    // Open file for writing
    FILE *file = fopen("randomGenerated.txt", "w");
    if (!file) {
        printf("Error opening file for writing!");
        return;
    }

    // Generate process interarrival times and burst lengths
    for (int i = 0; i < PC; i++) {
        // Generate interarrival time
        int interarrival_time = 0;
        while (interarrival_time < T1 || interarrival_time > T2) {
            double x = -log(1 - ((double)rand() / RAND_MAX)) / (1.0 / T);
            interarrival_time = round(x);
        }

        // Generate burst length
        int burst_length = 0;
        while (burst_length < L1 || burst_length > L2) {
            double x = -log(1 - ((double)rand() / RAND_MAX)) / (1.0 / L);
            burst_length = round(x);
        }

        // Write to file
        if (i == PC - 1) {
            fprintf(file, "PL %d", burst_length);
        } else {
            fprintf(file, "PL %d\nIAT %d\n", burst_length, interarrival_time);
        }
    }   

    // Close file
    fclose(file);
}


int parse_args(int argc, char* argv[], struct Args* args) {
    int opt;
    while ((opt = getopt(argc, argv, "n:a:s:i:o:r:m:")) != -1) {
        switch (opt) {
            case 'n':
                // Check if optarg is a number
                for (int i = 0; optarg[i] != '\0'; i++) {
                    if (!isdigit(optarg[i])) {
                        fprintf(stderr, "Invalid argument for option -n: %s is not a number\n", optarg);
                        exit(EXIT_FAILURE);
                    }
                }

                // Convert optarg to integer and assign to args->num_processors
                args->num_processors = atoi(optarg);
                break;
            case 'a':
                if (strlen(optarg) == 1 || strlen(optarg) == 3) {
                    char queue_approach = optarg[0];
                    char* queue_selection_method = argv[optind++];
                    if (strlen(queue_selection_method) == 2 && (queue_approach == 'M' || queue_approach == 'S') && ((queue_selection_method[0] == 'N'  && queue_selection_method[1] == 'A') || ((queue_selection_method[0] == 'L' || queue_selection_method[0] == 'R') && (queue_selection_method[1] == 'M') ))) {
                        args->queue_approach = queue_approach;
                        args->queue_selection_method[0] = queue_selection_method[0];
                        args->queue_selection_method[1] = queue_selection_method[1];
                        args->queue_selection_method[2] = '\0';
                    } else {
                        fprintf(stderr, "Error: Invalid argument for -a option\n");
                        return -1;
                    }
                } else {
                    fprintf(stderr, "Error: Invalid argument for -a option\n");
                    return -1;
                }
                break;
            case 's':
                if (strlen(optarg) >= 2 && strlen(optarg) <= 4) {
                    strcpy(args->scheduling_algorithm, optarg);
                    if (strcmp(args->scheduling_algorithm, "FCFS") == 0 || strcmp(args->scheduling_algorithm, "SJF") == 0) {
                        args->time_quantum = 0;
                    } else if (strcmp(args->scheduling_algorithm, "RR") == 0) {
                        args->time_quantum = atoi(argv[optind++]);
                    } else {
                        fprintf(stderr, "Error: Invalid argument for -s option\n args->scheduling_algorithm: %s, check: %d", args->scheduling_algorithm, strcmp(args->scheduling_algorithm, "FCFS") == 0 );
                        return -1;
                    }
                } else {
                    fprintf(stderr, "Error: Invalid argument for -s option\n, strlen(optarg): %d", (int)(strlen(optarg)));
                    return -1;
                }
                break;
            case 'i':
                willInputGenerated = true;
                strncpy(args->input_file, optarg, 64);
                break;
            case 'o':
                strncpy(args->output_file, optarg, 64);
                break;
            case 'r':
                if (optind + 5 > argc) {
                    fprintf(stderr, "Error: Not enough arguments for -r option\n");
                    return -1;
                } else {
                    willInputGenerated = true;
                    isRandomly = true;
                    args->T = atoi(argv[--optind]);
                    args->T1 = atoi(argv[++optind]);
                    args->T2 = atoi(argv[++optind]);
                    args->L = atoi(argv[++optind]);
                    args->L1 = atoi(argv[++optind]);
                    args->L2 = atoi(argv[++optind]);
                    args->PC = atoi(argv[++optind]);
                    generate_txt_file(args->T,args->T1,args->T2,args->L,args->L1,args->L2,args->PC);
                }
                break;
            case 'm':
                args->mode = atoi(optarg);
                if (args->mode < 1 || args->mode > 3) {
                    fprintf(stderr, "Invalid argument for option -m: %s is not a valid mode\n", optarg);
                    exit(EXIT_FAILURE);
                }
                break;
            case '?':
                if (optopt == 'n' || optopt == 'a' || optopt == 's' || optopt == 'i' || optopt == 'o' || optopt == 'r' || optopt == 'm') {
                    fprintf(stderr, "Error: Option -%c requires an argument\n", optopt);
                } else {
                    fprintf(stderr, "Error: Unknown option -%c\n", optopt);
                }
                return -1;
        }
    }
    return 1;
}

struct PCB {
    int pid; // process id
    int cpu; //cpuid
    int burst_length; // length of the cpu burst
    int arrival_time; // arrival time of the process
    int remaining_time; // remaining required cpu time of the burst
    int finish_time; // finish time of the process
    int turnaround_time; // turnaround time of the process
};

struct Node {
    struct PCB pcb;
    struct Node* next;
};

struct ReadyQueue {
    struct Node* head;
    struct Node* tail;
    int size;
    char algorithm[4];
    int time_quantum; // for Round Robin algorithm
    pthread_mutex_t lock; // mutex lock for synchronization
};

void init_ready_queue(struct ReadyQueue* rq, char* algorithm, int time_quantum) {
    rq->head = NULL;
    rq->tail = NULL;
    rq->size = 0;
    strcpy(rq->algorithm, algorithm);
    rq->time_quantum = time_quantum;
    pthread_mutex_init(&rq->lock, NULL);
}

void printProcessInfo(int op, int pid, int cpu, int burst_length, int arrival_time, int remaining_time, int finish_time, int turnaround_time, int timeSlice) {

    if(strcmp(args.output_file, "none") == 0) {
        //to the screen
        if (op== 0) {
            printf("Process added to queue:\n");
        } 
        else if (op==1) {
            printf("Burst picked for CPU:\n");
        }
        else if(op == 2)
        {
            printf("Burst has finished: \n");

        } else if(op == 3) {
            printf("The time slice expired for a burst: \n");
        }
        
        printf("Process ID: %d\n", pid);
        printf("CPU ID: %d\n", cpu);
        printf("Burst length: %d\n", burst_length);
        printf("Arrival time: %d\n", arrival_time);
        printf("Remaining time: %d\n", remaining_time);
        if(op == 2) {
            printf("Finish time: %d\n", finish_time);
            printf("Turnaround time: %d\n", turnaround_time);
        }

        if(op == 3) {
            printf("The time slice expired: %d\n", timeSlice);
        }

        printf("\n");
    } else {
        //to the file 
        pthread_mutex_lock(&file_mutex);
        FILE* file = fopen(args.output_file, "a");
        
        if (op== 0) {
            fprintf(file,"Process added to queue: \n");
        } 
        else if (op==1) {
            fprintf(file, "Burst picked for CPU:\n");
        }
        else if(op == 2)
        {
            fprintf(file, "Burst has finished: \n");

        } else if(op == 3) {
            fprintf(file,"The time slice expired for a burst: \n");
        }
        
        fprintf(file,"Process ID: %d\n", pid);

        if(op != 0) {
            fprintf(file,"CPU ID: %d\n", cpu);
        }
        
        fprintf(file,"Burst length: %d\n", burst_length);
        fprintf(file,"Arrival time: %d\n", arrival_time);
        fprintf(file,"Remaining time: %d\n", remaining_time);
        if(op == 2) {
            fprintf(file,"Finish time: %d\n", finish_time);
            fprintf(file,"Turnaround time: %d\n", turnaround_time);
        }

        if(op == 3) {
            fprintf(file,"The time slice expired: %d\n", timeSlice);
        }

        fprintf(file, "\n");

        fclose(file);
        pthread_mutex_unlock(&file_mutex);
    }
}


void enqueue(struct ReadyQueue* rq, struct PCB pcb) {
    struct Node* node = (struct Node*)malloc(sizeof(struct Node));
    node->pcb = pcb;
    node->next = NULL;
    pthread_mutex_lock(&rq->lock);
    if (strcmp(rq->algorithm, "SJF") == 0) {
        struct Node* prev = NULL;
        struct Node* curr = rq->head;
        while (curr != NULL && pcb.burst_length > curr->pcb.burst_length) {
            prev = curr;
            curr = curr->next;
        }
        if (prev == NULL) {
            node->next = rq->head;
            rq->head = node;
        } else {
            node->next = curr;
            prev->next = node;
        }
        if (node->next == NULL) {
            rq->tail = node;
        }
    } else {
        if (rq->tail == NULL) {
            rq->head = node;
            rq->tail = node;
        } else {
            rq->tail->next = node;
            rq->tail = node;
        }
    }
    rq->size++;

    if (args.mode == 3 && pcb.pid != -2 && pcb.pid != -1 && rq->time_quantum != -5)   // OUTPUT MODE 3 (not finished items -5)
    {
       printProcessInfo(0,pcb.pid,pcb.cpu,pcb.burst_length,pcb.arrival_time,pcb.remaining_time,pcb.finish_time,pcb.turnaround_time,0);
    }
    
    pthread_mutex_unlock(&rq->lock);
}

struct PCB dequeue(struct ReadyQueue* rq) {
    struct PCB pcb;
    pthread_mutex_lock(&rq->lock);
    if (rq->head == NULL) {
        pcb.pid = -1; // error condition
        pthread_mutex_unlock(&rq->lock);
        return pcb;
    }
    struct Node* node = rq->head;
    pcb = node->pcb;
    rq->head = node->next;
    if (rq->head == NULL) {
        rq->tail = NULL;
    }
    rq->size--;
    free(node);
    pthread_mutex_unlock(&rq->lock);

    return pcb;
}

int is_empty(struct ReadyQueue* rq) {
    pthread_mutex_lock(&rq->lock);
    int size = rq->size;
    pthread_mutex_unlock(&rq->lock);
    return size == 0;
}

int get_size(struct ReadyQueue* rq) {
    pthread_mutex_lock(&rq->lock);
    int size = rq->size;
    pthread_mutex_unlock(&rq->lock);
    return size;
}

void print_ready_queue(struct ReadyQueue* rq) {
    printf("Ready Queue: ");
    struct Node* node = rq->head;
    while(node != NULL) {
        printf("%d ", node->pcb.pid);
        node = node->next;
    }
    printf("\n");
}

void print_output(struct ReadyQueue* rq) {
    struct Node* node = rq->head;
    int total_turnaround_time = 0;
    int num_processes = 0;
    printf("pid\tcpu\tburstlength\tarrival\tfinish\twaitingtime\tturnaround\n");
    while (node != NULL) {
        struct PCB pcb = node->pcb;
        int turnaround_time = pcb.turnaround_time;
        int waiting_time = turnaround_time - pcb.burst_length;
        total_turnaround_time += turnaround_time;
        num_processes++;
        printf("%d\t%d\t%d\t\t%d\t%d\t%d\t\t%d\n", pcb.pid, pcb.cpu, pcb.burst_length, pcb.arrival_time, pcb.finish_time, waiting_time, turnaround_time);
        node = node->next;
    }
    if (num_processes > 0) {
        int avg_turnaround_time = total_turnaround_time / num_processes;
        printf("average turnaround time: %d ms\n", avg_turnaround_time);
    }
}

void print_output_to_file(struct ReadyQueue* rq, char* filename) {
    FILE* file;
    if(args.mode == 1) {
        file = fopen(filename, "w");
    } else {
        file = fopen(filename, "a");
    }
   
    if (file == NULL) {
        printf("Error opening file %s\n", filename);
        return;
    }
    struct Node* node = rq->head;
    int total_turnaround_time = 0;
    int num_processes = 0;
    fprintf(file, "%-4s  %-3s  %-11s  %-7s  %-7s  %-13s  %-10s\n", "pid", "cpu", "burstlength", "arrival", "finish", "waitingtime", "turnaround");
    while (node != NULL) {
        struct PCB pcb = node->pcb;
        int turnaround_time = pcb.turnaround_time;
        int waiting_time = turnaround_time - pcb.burst_length;
        total_turnaround_time += turnaround_time;
        num_processes++;
        fprintf(file, "%-4d  %-3d  %-11d  %-7d  %-7d  %-13d  %-10d\n", pcb.pid, pcb.cpu, pcb.burst_length, pcb.arrival_time, pcb.finish_time, waiting_time, turnaround_time);
        node = node->next;
    }
    if (num_processes > 0) {
        int avg_turnaround_time = total_turnaround_time / num_processes;
        fprintf(file, "average turnaround time: %d ms\n", avg_turnaround_time);
    }
    fclose(file);
}

void sort_ready_queue(struct ReadyQueue* rq) {
    pthread_mutex_lock(&rq->lock);
    if (rq->size < 2) {
        // The ready queue is already sorted (or empty)
        pthread_mutex_unlock(&rq->lock);
        return;
    }
    // Create an array of PCB pointers and populate it with the PCBs in the ready queue
    struct PCB** pcb_arr = (struct PCB**)malloc(rq->size * sizeof(struct PCB*));
    int i = 0;
    struct Node* curr = rq->head;
    while (curr != NULL) {
        pcb_arr[i] = &curr->pcb;
        curr = curr->next;
        i++;
    }
    // Sort the array of PCB pointers based on the PCBs' PIDs using insertion sort
    for (int j = 1; j < rq->size; j++) {
        struct PCB* key = pcb_arr[j];
        int k = j - 1;
        while (k >= 0 && pcb_arr[k]->pid > key->pid) {
            pcb_arr[k + 1] = pcb_arr[k];
            k--;
        }
        pcb_arr[k + 1] = key;
    }
    // Reconstruct the ready queue using the sorted array of PCB pointers
    rq->head = NULL;
    rq->tail = NULL;
    for (int j = 0; j < rq->size; j++) {
        struct PCB* pcb = pcb_arr[j];
        struct Node* node = (struct Node*)malloc(sizeof(struct Node));
        node->pcb = *pcb;
        node->next = NULL;
        if (rq->tail == NULL) {
            rq->head = node;
            rq->tail = node;
        } else {
            rq->tail->next = node;
            rq->tail = node;
        }
    }
    free(pcb_arr);
    pthread_mutex_unlock(&rq->lock);
}


int get_timestamp() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    int time_diff_in_ms = ((current_time.tv_sec - simulationStart.tv_sec) * 1000 +
                           (current_time.tv_usec - simulationStart.tv_usec) / 1000);
    return time_diff_in_ms;
}

struct threadParam {
    struct ReadyQueue* rq;
    int cpu_id;
};

void outmode_2(int time, int cpu, int pid, int burstlen, int remainingTime) {
    if(strcmp(args.output_file, "none") == 0) {
        //to the screen
        printf("time= %d, cpu = %d, pid = %d, burslen = %d, remainingtime = %d \n", time, cpu, pid, burstlen, remainingTime);
    } else {
        //to the file 
        pthread_mutex_lock(&file_mutex);
        FILE* file = fopen(args.output_file, "a");
        fprintf(file, "time= %d, cpu = %d, pid = %d, burslen = %d, remainingtime = %d \n", time, cpu, pid, burstlen, remainingTime);
        fclose(file);
        pthread_mutex_unlock(&file_mutex);
    }
}

void* simulate(void* param) {
    struct threadParam* thread_param = (struct threadParam*) param;
    struct ReadyQueue* rq = thread_param->rq;
    int cpu_id = thread_param->cpu_id;

    while(1) {                
        while(is_empty(rq)) {
            usleep(1 * 1000);
        }

        if( (args.queue_approach == 'M') && (strcmp(args.queue_selection_method, "LM") == 0) ){
            pthread_mutex_lock(&count_mutex);
            threadCount++;
            if(threadCount == 1) {
                pthread_mutex_lock(&size_mutex);
            }
            pthread_mutex_unlock(&count_mutex);
        }
        
        pthread_mutex_lock(&rq->lock);
        if(rq->head != NULL && rq->head->pcb.pid == -2) {
            pthread_mutex_unlock(&rq->lock);
            if(get_size(rq) == 1) {
                pthread_exit(0);
            } 
        } else {
            pthread_mutex_unlock(&rq->lock);
        }

        if(strcmp(rq->algorithm, "RR") == 0) {
            struct PCB pcb = dequeue(rq);
            if(pcb.pid == -1 || pcb.pid == -2) {
                if(pcb.pid == -2) {
                    enqueue(rq, pcb);
                }
                if( (args.queue_approach == 'M') && (strcmp(args.queue_selection_method, "LM") == 0) ) {
                    pthread_mutex_lock(&count_mutex);
                    threadCount--;
                    if(threadCount == 0) {
                        pthread_mutex_unlock(&size_mutex);
                    }
                    pthread_mutex_unlock(&count_mutex);
                }
                continue;
            }
            pcb.cpu = cpu_id;
            if (args.mode == 3 && pcb.pid != -2 && pcb.pid != -1)  // OUTPUT MODE 3
            {
                printProcessInfo(1,pcb.pid,pcb.cpu,pcb.burst_length,pcb.arrival_time,pcb.remaining_time,pcb.finish_time,pcb.turnaround_time, 0);
            }
            if(pcb.remaining_time <= rq->time_quantum) {
                if(args.mode == 2) {
                    outmode_2(get_timestamp(), pcb.cpu, pcb.pid, pcb.burst_length, pcb.remaining_time);
                }
                usleep(pcb.remaining_time * 1000);
                pcb.finish_time = get_timestamp();
                pcb.remaining_time = 0;
                pcb.turnaround_time = pcb.finish_time - pcb.arrival_time;
                if (args.mode == 3)  // OUTPUT MODE 3
                {
                    printProcessInfo(2,pcb.pid,pcb.cpu,pcb.burst_length,pcb.arrival_time,pcb.remaining_time,pcb.finish_time,pcb.turnaround_time, 0);
                }
                enqueue(&finishedItems, pcb);
            } else {
                if(args.mode == 2) {
                    outmode_2(get_timestamp(), pcb.cpu, pcb.pid, pcb.burst_length, pcb.remaining_time);
                }
                usleep(rq->time_quantum * 1000);
                pcb.remaining_time = pcb.remaining_time - rq->time_quantum;
                if (args.mode == 3)  // OUTPUT MODE 3
                {
                    printProcessInfo(3,pcb.pid,pcb.cpu,pcb.burst_length,pcb.arrival_time,pcb.remaining_time, pcb.finish_time,pcb.turnaround_time, get_timestamp());
                }
                enqueue(rq, pcb);
            }
        } else {
            struct PCB pcb = dequeue(rq);
            if(pcb.pid == -1 || pcb.pid == -2) {
                if(pcb.pid == -2) {
                    enqueue(rq, pcb);
                }
                if( (args.queue_approach == 'M') && (strcmp(args.queue_selection_method, "LM") == 0) ) {
                    pthread_mutex_lock(&count_mutex);
                    threadCount--;
                    if(threadCount == 0) {
                        pthread_mutex_unlock(&size_mutex);
                    }
                    pthread_mutex_unlock(&count_mutex);
                }
                continue;
            }
            pcb.cpu = cpu_id;
            if (args.mode == 3 && pcb.pid != -2 && pcb.pid != -1)  // OUTPUT MODE 3 
            {
                printProcessInfo(1,pcb.pid,pcb.cpu,pcb.burst_length,pcb.arrival_time,pcb.remaining_time,pcb.finish_time,pcb.turnaround_time, 0);
            }
            if(args.mode == 2) {
                outmode_2(get_timestamp(), pcb.cpu, pcb.pid, pcb.burst_length, pcb.remaining_time);
            }
            usleep(pcb.burst_length * 1000);
            pcb.finish_time = get_timestamp();
            pcb.remaining_time = 0;
            pcb.turnaround_time = pcb.finish_time - pcb.arrival_time;
            if (args.mode == 3)  // OUTPUT MODE 3
            {
                printProcessInfo(2,pcb.pid,pcb.cpu,pcb.burst_length,pcb.arrival_time,pcb.remaining_time,pcb.finish_time,pcb.turnaround_time, 0);
            }
            enqueue(&finishedItems, pcb);
        }

        if( (args.queue_approach == 'M') && (strcmp(args.queue_selection_method, "LM") == 0) ) {
            pthread_mutex_lock(&count_mutex);
            threadCount--;
            if(threadCount == 0) {
                pthread_mutex_unlock(&size_mutex);
            }
            pthread_mutex_unlock(&count_mutex);
        }
    }
}


int main(int argc, char* argv[]) {
    
    //parse the arguments
    if (parse_args(argc, argv, &args) == -1) {
        print_usage();
        return EXIT_FAILURE;
    }

    if (!willInputGenerated)
    {
        generate_txt_file(200,10,1000,100,10,500,10);
        isRandomly = true;
    }
    
    //create the queue for the finished items
    init_ready_queue(&finishedItems, "FCFS", -5);

    //create the ready queues according to the approach, single queue or multiple queue. 
    //If multiple queues will exist then create one queue for each processor
    if(args.queue_approach == 'M') {
        readyQueues = (struct ReadyQueue*) malloc(args.num_processors * sizeof(struct ReadyQueue));
        for(int i = 0; i < args.num_processors; i++) {
            init_ready_queue(&readyQueues[i], args.scheduling_algorithm ,args.time_quantum);
        }

    } else {
        readyQueues = (struct ReadyQueue*) malloc(sizeof(struct ReadyQueue));
        init_ready_queue(readyQueues, args.scheduling_algorithm ,args.time_quantum);
    }

    //create processors
    pthread_t threads[args.num_processors];
    struct threadParam thread_params[args.num_processors];
    for (int i = 0; i < args.num_processors; i++) {
        thread_params[i].cpu_id = i + 1;
        if(args.queue_approach == 'M') {
            init_ready_queue(&readyQueues[i], args.scheduling_algorithm ,args.time_quantum);
            thread_params[i].rq = &readyQueues[i];
        } else {
            init_ready_queue(readyQueues, args.scheduling_algorithm ,args.time_quantum);
            thread_params[i].rq = readyQueues;
        }
        pthread_create(&threads[i], NULL, simulate, (void*) &thread_params[i]);
    }

    if(isRandomly) {
        strncpy(args.input_file , "randomGenerated.txt", 64);
    }

    // Open the file to read
    FILE *file = fopen(args.input_file, "r"); // read only
    
    // Check if the file is exist, and readable
    if (file == NULL) {
        perror("Error: cannot open the file!");
        exit(EXIT_FAILURE);
    }

    char word[64];
    int burstLength;
    int IAT;
    int processCount = 0;
    // Read each word from the file
    while(fscanf(file, "%s", word) != EOF) {

        if(strcmp(word, "PL") == 0) {
            //scan the next word which will give the cpu burst length
            fscanf(file, "%d", &burstLength);
            processCount++;

            //create the PCB of the PROCESS            
            struct PCB process = {
                .pid = processCount,
                .cpu = -1,
                .burst_length = burstLength,
                .remaining_time = burstLength,
                .finish_time = -1,
                .turnaround_time = -1
            };

            if(processCount == 1) {
                process.arrival_time =  0;
                gettimeofday(&simulationStart, NULL);
            } else {
                process.arrival_time = get_timestamp();
            }

            if(args.queue_approach == 'M') {
                if(strcmp(args.queue_selection_method, "RM") == 0) {
                    process.cpu = ((processCount - 1) % args.num_processors) + 1;
                    enqueue(&readyQueues[(processCount - 1) % args.num_processors],process);
                } else {    //LM
                    //find the least loaded queue, and put the process to it
                    // acquire the size mutex before reading the queue sizes
                    pthread_mutex_lock(&size_mutex);

                    // find the least loaded queue
                    int smallest_size = readyQueues[0].size;
                    int smallest_id = 0;
                    for (int i = 1; i < args.num_processors; i++) {
                        if (readyQueues[i].size < smallest_size) {
                            smallest_size = readyQueues[i].size;
                            smallest_id = i;
                        }
                    }

                    // release the size mutex
                    pthread_mutex_unlock(&size_mutex);

                    // put the process to the smallest queue
                    process.cpu = smallest_id + 1;
                    enqueue(&readyQueues[smallest_id], process);
                }
            } else {
                //single queue
                enqueue(readyQueues,process);
            }
            
        } else if(strcmp(word, "IAT") == 0)  {
            //scan the next word and it will give IAT and sleep as the scanned amount
            fscanf(file, "%d", &IAT);
            usleep(IAT*1000);
        }
    }

    //put the dummy item        
    struct PCB dummyItem = {
        .pid = -2,
        .burst_length = __INT_MAX__
    };

    if(args.queue_approach == 'S') {
        enqueue(readyQueues, dummyItem);
    } else {
        for(int i = 0; i < args.num_processors; i++) {
            enqueue(&readyQueues[i], dummyItem);
        }
    }

    //wait for threads
    for (int i = 0; i < args.num_processors; i++) {
        pthread_join(threads[i], NULL);
    }

    //default
    //sort finished itemss
    sort_ready_queue(&finishedItems);
    if(strcmp(args.output_file, "none") == 0) {
        print_output(&finishedItems);
    } else {
        pthread_mutex_lock(&file_mutex);
        print_output_to_file(&finishedItems, args.output_file);
        pthread_mutex_unlock(&file_mutex);
    }
}