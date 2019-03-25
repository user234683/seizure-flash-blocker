#include <stdio.h>
/* -- Directions --
In the loop or function of interest, issue
    PROFILE(x);
at points of interest. In the first call, x should be 0. 
Use 1 for the next call, etc, until the end of the "trial".

Each repetition of the loop/function is a trial.

Next, in the global scope, #define NUM_TRIALS as the number of trials you
wish to record. #define TIMESTAMPS_PER_TRIAL as the number of PROFILE
statements you used.

Then issue
    PROFILING_DECLARATIONS();
in the global scope somewhere. This will define the array 
that stores the information.

Issue
    PROFILING_INIT();
in the main function. This will get the system clock rate needed to translate
clock cycles to seconds.

At the end of the loop or function, issue
    TRIAL_END(filename)
where filename is the name of the file to save the information to once
NUM_TRIALS is reached.

The saved file will be a list of timestamps in microseconds. There will be an
extra newline between each trial. You can then analyze this with your
interactive language of choice.
*/

#define PROFILING_DECLARATIONS()                                      \
    LARGE_INTEGER PROFILING_RESULTS[NUM_TRIALS*TIMESTAMPS_PER_TRIAL];  \
    LARGE_INTEGER PERF_FREQ;                                           \
    int PROFILING_TRIAL = 0

#define PROFILING_INIT()                       \
    do{                                        \
        QueryPerformanceFrequency(&PERF_FREQ); \
    } while(0)

#define PROFILE(timestamp_n)                                         \
    do {                                                             \
        if(PROFILING_TRIAL < NUM_TRIALS){                            \
            QueryPerformanceCounter(&PROFILING_RESULTS[TIMESTAMPS_PER_TRIAL*PROFILING_TRIAL + timestamp_n]); \
        }                                                            \
    } while(0)

#define TRIAL_END(filename)                                         \
    do {                                                            \
        if(PROFILING_TRIAL == NUM_TRIALS - 1){                      \
            FILE* fp = fopen(filename, "w");                        \
            for(int i = 0; i < NUM_TRIALS; i++){                    \
                for(int j = 0; j < TIMESTAMPS_PER_TRIAL; j++){      \
                    fprintf(fp, "%d\n", PROFILING_RESULTS[TIMESTAMPS_PER_TRIAL*i + j].QuadPart*1000000/PERF_FREQ.QuadPart); \
                }                                                   \
                fprintf(fp, "\n");                                  \
            }                                                       \
            fclose(fp);                                             \
        }                                                           \
        PROFILING_TRIAL++;                                          \
    } while(0)
