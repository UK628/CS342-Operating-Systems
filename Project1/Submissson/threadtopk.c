#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

//constants
#define MAX_WORD_SIZE 64

// A struct to hold the frequency of word 
typedef struct {
    char word[MAX_WORD_SIZE];
    int count;
} wordFreq;

// A struct to hold the frequency table
typedef struct {
    wordFreq* freqArr;
    int arrSize;
} freqTable;

//struct to pass arguments to threads
typedef struct {
    int startIndex;
    char* fileName;
} threadArgs;


//Global Variables
char* sharedMemName;
wordFreq* shrPtr; //pointer to the shared memory object
char** inputFiles; // names of the input files

int n; // num of input files
int k; // num of words to find     
char outFileName[MAX_WORD_SIZE] ;

// A function to process a file & return a frequency table
freqTable processFile(char* name) {

    // Open the file to read
    FILE *file = fopen(name, "r"); // read only

    // Check if the file is exist, and readable
    if (file == NULL) {
        perror("Error: cannot open the file!");
        exit(EXIT_FAILURE);
    }

    freqTable freqTable;
    freqTable.freqArr = NULL;
    freqTable.arrSize = 0;

    char word[MAX_WORD_SIZE];
    int wordCount = 0;

    // Read each word from the file
    while (fscanf(file, "%s", word) != EOF) {


        //convert all lowercase letters to uppercase
        for (int i = 0; i < strlen(word); i++) {
            word[i] = toupper(word[i]);
        }

        // Check if the word already exists in the frequency table
        bool alreadyExist = false;
        for (int i = 0; i < freqTable.arrSize; i++) {

            //the word is already exist
            if (strcmp(word, freqTable.freqArr[i].word) == 0) {
                freqTable.freqArr[i].count++;
                alreadyExist = true;
                break;
            }
        }

        // If the word doesn't exist in the frequency table, add it
        if (!alreadyExist) {
            
            //allocate memory
            if (freqTable.freqArr == NULL) {
                //first allocation
                freqTable.freqArr = (wordFreq*) malloc(1 * sizeof(wordFreq));
            } else {
                //reallocation
                freqTable.freqArr = (wordFreq*) realloc(freqTable.freqArr, (freqTable.arrSize + 1) * sizeof(wordFreq)); 

                if (freqTable.freqArr == NULL) {
                    perror("Error: cannot reallocate memory for word frequency table!");
                    exit(EXIT_FAILURE);
                }

            }

            //add the new word to the end of the string
            strcpy(freqTable.freqArr[freqTable.arrSize].word, word); 
            freqTable.freqArr[freqTable.arrSize].count = 1;
            freqTable.arrSize++;
            wordCount++;
        }

    }
    freqTable.arrSize = wordCount;

    // Close the file
    fclose(file);

    // Sort the frequency table based on word count && select the top K word
    sortFreqTable(freqTable.freqArr, freqTable.arrSize);

    //free the unnecessary memory
    if(k < freqTable.arrSize) {

        freqTable.freqArr = (wordFreq*) realloc(freqTable.freqArr, k * sizeof(wordFreq)); 
        freqTable.arrSize = k;

        if (freqTable.freqArr == NULL) {
            perror("Error: cannot reallocate memory for word frequency table!");
            exit(EXIT_FAILURE);
        }
    }

    return freqTable;
}

void saveShared(freqTable table,  int startIndex) {

    //copy each element to the shared memory
    for(int i = 0; i < table.arrSize; i++) {
        shrPtr[startIndex + i] = table.freqArr[i];  
    }

}

void sortFreqTable(wordFreq* freqArr, int arrSize) {

    for (int i = 0; i < arrSize && i < k; i++) {
        for (int j = i+1; j < arrSize; j++) {
            if (freqArr[i].count < freqArr[j].count || ( (freqArr[i].count == freqArr[j].count) && strcmp(freqArr[i].word,freqArr[j].word) > 0 )) {
                wordFreq temp = freqArr[i];
                freqArr[i] = freqArr[j];
                freqArr[j] = temp;
            }
        }
    }
}

void mergeTables(wordFreq* freqTables) {
    //display
    //display(freqTables, n * k);

    // Create a frequency table to store the merged results
    freqTable mergedTable;
    mergedTable.freqArr = (wordFreq*) malloc( n * k * sizeof(wordFreq)); // allocate enough space for all the words
    mergedTable.arrSize = 0;

    // Loop through all the words in the frequency table
    for (int i = 0; i < n * k; i++) {

        // Find if the word is already in the merged table
        int idx = -1;
        for (int l = 0; l < mergedTable.arrSize; l++) {
            if (strcmp(mergedTable.freqArr[l].word, freqTables[i].word) == 0) {
                idx = l;
                break;
            }
        }

        // If the word is not in the merged table, add it
        if (idx == -1) {
            idx = mergedTable.arrSize;
            mergedTable.freqArr[idx] = freqTables[i];
            mergedTable.arrSize++;
        }

        // Otherwise, add the frequency to the existing word count
        else {
            mergedTable.freqArr[idx].count += freqTables[i].count;
        }
    }

    // Sort the merged frequency table by word count
    sortFreqTable(mergedTable.freqArr, mergedTable.arrSize);
    //displayFreqTable(mergedTable);

    // Write the top K words to the output file
    FILE* outFile = fopen(outFileName, "w");
    if (outFile == NULL) {
        perror("fopen");
        exit(1);
    }

    for (int i = 0; i < k && i < mergedTable.arrSize; i++) {
        if (mergedTable.freqArr[i].count != 0 ) {
            fprintf(outFile, "%s %d\n", mergedTable.freqArr[i].word, mergedTable.freqArr[i].count);
        }
    }

    // Free the memory
    free(mergedTable.freqArr);
    fclose(outFile);
}

void display(wordFreq* freqArr, int arrSize) {
    // Print the frequency table
    printf("\n");
    printf("%-15s %s\n", "Word", "Count");
    printf("=====================\n");
    for (int i = 0; i < arrSize; i++) {
        printf("%-15s %d\n", freqArr[i].word,freqArr[i].count);
    }
}

void displayFreqTable(freqTable table) {
    // Print the frequency table
    printf("\n");
    printf("%-15s %s\n", "Word", "Count");
    printf("=====================\n");
    for (int i = 0; i < table.arrSize; i++) {
        printf("%-15s %d\n", table.freqArr[i].word, table.freqArr[i].count);
    }
}

//thread function
void *processFileThread(void *arg) {

    threadArgs *args = (threadArgs *)arg;
    char *fileName = args->fileName;
    int startIndex = args->startIndex;

    freqTable table = processFile(fileName);
    //displayFreqTable(table);
    saveShared(table, startIndex);

    pthread_exit(NULL);
}


int main(int argc, char** argv) {
    
    // Check if the correct number of arguments are provided
    if (argc < 5) {
        printf("Usage: proctopk <K> <outfile> <N> <infile1> .... <infileN>\n");
        return 0;
    }

    // Parse command line arguments
    k = atoi(argv[1]);
    strcpy(outFileName, argv[2]);
    n = atoi(argv[3]);

    //allocate memory
    inputFiles = (char**) malloc (n * sizeof(char *));
    if (inputFiles == NULL) {
        perror("Error: cannot allocate memory for input file names!");
        exit(EXIT_FAILURE);
    }

    //copy the file names
    for (int i = 0; i < n; i++) {
        inputFiles[i] = argv[i+4];
    }

    // create the global memomry for threads
    shrPtr = (wordFreq*) malloc(n * k * sizeof(wordFreq));


    //create threads
    pthread_t threads[n];

    for (int i = 0; i < n; i++) {
        int startIndex = i * k;
        threadArgs *args = malloc(sizeof(threadArgs));
        args->fileName = inputFiles[i];
        args->startIndex = startIndex;

        if (pthread_create(&threads[i], NULL, processFileThread, (void *)args) != 0) {
            printf("Error creating thread\n");
            return 1;
        }
    }

    //wait for threads
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }

    // merge the frequency tables from the shared memory
    mergeTables(shrPtr);

    return 0;
}