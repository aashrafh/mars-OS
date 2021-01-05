/**
 * @file scheduler.c
 * @author Ahmed Ashraf (ahmed.ashraf.cmp@gmail.com)
 * @brief Keeps track of the processes and their states and it decides which process will run and for how long.
 * @version 0.1
 * @date 2020-12-30
 */

#define DEBUG
#define RQSZ 1000

#include "headers.h"
#include "process_generator.h"
#include "priority_queue.h"


struct msgbuff
{
        long mtype;
        char* mtext;
        process_t proc;
};

key_t msgqid;
int *shmRaddr;
struct Queue *rq;
PCB *running;
int procGenFinished;

// Remaining time for current quantum
int currQ, nproc, schedulerType, quantum;

// Functions declaration
void ReadMSGQ(short wait);
void ReadProcess(int signum);
void CreateEntry(process_t entry);
void HPFSheduler();
void SRTNSheduler();
void RRSheduler(int q);
char *myItoa(int number);
void ProcFinished(int signum);

int main(int argc, char *argv[])
{
        signal(SIGMSGQ, ReadProcess);
        signal(SIGPF, ProcFinished);

        initClk();

        if (argc < 4) {
                perror("\n\nScheduler: Not enough argument\n");
                exit(EXIT_FAILURE);
        }

        schedulerType = atoi(argv[1]);
        nproc = atoi(argv[2]);
        quantum = atoi(argv[3]);

        msgqid = msgget(MSGQKEY, 0644);
        if (msgqid == -1) {
                perror("\n\nScheduler: Failed to get the message queue\n");
                exit(EXIT_FAILURE);
        }


        // Create shared memory for remaining time of the running process
        key_t shmR_id = shmget(PRSHKEY, 4, IPC_CREAT | 0644);
        if (shmR_id == -1) {
                perror("\n\nScheduler: Failed to get the shared memory\n");
                exit(EXIT_FAILURE);
        }

        // attach to shared memory of the running process
        shmRaddr = (int *)shmat(shmR_id, (void *)0, 0);
        if ((long)shmRaddr == -1) {
                perror("\n\nError in attaching the shm in scheduler!\n");
                exit(EXIT_FAILURE);
        }


        union Semun semun;

        
        int semSchedGen = semget(SEM_SCHED_GEN_KEY, 1, 0666 | IPC_CREAT);
        int semSchedProc = semget(SEM_SCHED_PROC_KEY, 1, 0666 | IPC_CREAT);
        
        if (semSchedProc == -1 || semSchedGen == -1){
                perror("Error in create sem");
                exit(-1);
        }

        semun.val = 0; /* initial value of the semaphore, Binary semaphore */
        if (semctl(semSchedGen, 0, SETVAL, semun) == -1){
                perror("Error in semctl");
                exit(-1);
        }

        if (semctl(semSchedProc, 0, SETVAL, semun) == -1){
                perror("Error in semctl");
                exit(-1);
        }

        rq = CreateQueue(RQSZ);
        running = NULL;

        int currTime = -1;
        int clk;
        printf("nproc is %d\n", nproc);
        while (nproc) {
                clk =  getClk();
                printf("clk is %d\n", clk);
                if (clk == currTime) continue;
                fflush(stdout);
                printf("lol xD");
                currTime = getClk();
                if (!procGenFinished) down(semSchedGen);
                
                if (running != NULL) down(semSchedProc);
                
                ReadMSGQ(0);
                switch (schedulerType){
                case 0:
                SRTNSheduler();
                break;

                case 1:
                RRSheduler(quantum);
                break;

                default:
                HPFSheduler();
                break;
                }
                
        }

        // upon termination release the clock resources.
        destroyClk(false);
}

/**
 * @brief Reads the message queue and push the new processes
 * to the ready queue
 * @param wait 1 if it has to wait 0 otherwise
 */
void ReadMSGQ(short wait)
{
        while (1) {
                int recVal;
                struct msgbuff msg;

                // Try to recieve the new process
                recVal = msgrcv(msgqid, &msg, sizeof(msg.proc) + sizeof(msg.mtext), 0, wait ? !IPC_NOWAIT : IPC_NOWAIT); 
                if (recVal == -1) {
                        // If there is no process recieved then break
                        break;
                }

                // If successfuly recieved the new process add it to the ready queue
                CreateEntry(msg.proc);
        }
}

/**
 * @brief Signal handler for the SIGUSR to handle arriving of a new process
 *
 * @param signum SIGUSR flag
 */
void ReadProcess(int signum)
{        
        procGenFinished = 1;
}

/**
 * @brief Create a PCB object and insert it in the ready queue
 * 
 * @param entry process object
 */
void CreateEntry(process_t proc)
{
        PCB *entry = (PCB *)malloc(sizeof(PCB));
        entry->id = proc.id;
        entry->arrivalTime = proc.arrivalTime;
        entry->runTime = proc.runTime;
        entry->priority = proc.priority;
        entry->state = READY;
        
        switch (schedulerType)
        {
        case 0:
        entry->priority = entry->remainingTime;
        InsertValue(rq, entry);
        // if(running) SRTNSheduler();  // Should be called again to check that the current runnning proc is the SRTN
        break;
        case 1:
        Enqueue(rq, entry);
        break;
        case 2:
        InsertValue(rq, entry);
        break;
        default:
        break;
        }
        printf("\n\nScheduler: process %d arrived at %d\n", entry->id, getClk());
}

/**
 * @brief Schedule the processes using Non-preemptive Highest Priority First 
 * 
 */
void HPFSheduler()
{
        if (running == NULL) {
                running = ExtractMin(rq);

                #ifdef DEBUG
                printf("process %d started running at %d.\n", running->id, getClk());
                #endif

                // Start a new process. (Fork it and give it its parameters.)
                *shmRaddr = running->remainingTime;

                int pid;
                if ((pid = fork()) == 0) {
                        int rt = execl("build/process.out", "process.out", myItoa(running->id),
                                myItoa(running->arrivalTime), myItoa(running->runTime), myItoa(running->priority), NULL);

                        if (rt == -1) {
                                perror("\n\nScheduler: couldn't run scheduler.out\n");
                                exit(EXIT_FAILURE);
                        }
                }
                else {
                        running->pid = pid;
                }
        }
}

/**
 * @brief Schedule the processes using Shortest Remaining time Next
 * 
 */
void SRTNSheduler()
{
  if(running)
  {
          PCB* nextProc = Minimum(rq);
          if(running->remainingTime > nextProc->remainingTime)  // Context Switching
          {
                  printf("\n\nScheduler: process %d has blocked at time %d", running->id, getClk());
                  running->state = BLOCKED;
                  InsertValue(rq, running);
                  kill(running->pid, SIGSLP);
          }
          else return;
  }
  else if(running == NULL || running->state == BLOCKED){
          running = ExtractMin(rq); 
          if(running == READY){

                #ifdef DEBUG
                printf("process %d started running at %d.\n", running->id, getClk());
                #endif

                *shmRaddr = running ->remainingTime;
                if(fork() == 0){
                        int rt = execl("build/process.out", "process.out", myItoa(running->id),
                        myItoa(running->arrivalTime), myItoa(running->runTime), myItoa(running->priority), NULL);
                        if (rt == -1)
                        {
                                perror("\n\nScheduler: couldn't run scheduler.out\n");
                                exit(EXIT_FAILURE);
                        }
                }
          }
          else if (running->state == BLOCKED)
          {
                  kill(running->pid, SIGSLP);
          }
  }
}

/**
 * @brief Schedule the processes using Round Robin
 * 
 */

void RRSheduler(int quantum)
{
  if (running)
  {
    currQ--;
    if (currQ == 0)
    {
      Enqueue(rq, running);
      kill(running->pid, SIGSLP);
      running->state = BLOCKED;
    }
    else
      return;
  }

  currQ = quantum;
  running = Dequeue(rq);
  if (running->state == READY)
  { // Start a new process. (Fork it and give it its parameters.)
    *shmRaddr = running ->remainingTime;
    if (fork() == 0)
    {
      int rt = execl("build/process.out", "process.out", myItoa(running->id),
                     myItoa(running->arrivalTime), myItoa(running->runTime), myItoa(running->priority), NULL);
      if (rt == -1)
      {
        perror("scheduler: couldn't run scheduler.out\n");
        exit(EXIT_FAILURE);
      }
    }
  }
  else if (running->state == BLOCKED)
  {
    kill(running->pid, SIGSLP);
  }
  
}

/**
 * @brief this a handler of SIGPF signal of this process to handle
 * when a process is finished.
 * 
 * @param signum SIGPF
 */
void ProcFinished(int signum)
{
        #ifdef DEBUG
        printf("\n\nScheduler: process %d finished running at %d.\n", running->id, getClk());
        #endif

        free(running);
        running = NULL;
        nproc--;


        //rebind the signal handler
        signal(SIGPF, ProcFinished);
}

/**
 * @brief convert an integer to a null terminated string.
 * 
 * @param number the number to convert.
 * @return char* a pointer to the converted string.
 */
char *myItoa(int number)
{
  uint8_t ch;
  int numberSize = 0;
  char *numberStr = NULL;
  if (number != 0)
  {
    while ((ch = number % 10))
    {
      number /= 10;

      numberSize++;
      numberStr = (char *)realloc(numberStr, numberSize);
      numberStr[numberSize - 1] = ch + '0';
    }
  }
  else
  {
    numberSize++;
    numberStr = (char *)realloc(numberStr, numberSize);
    numberStr[numberSize - 1] = '0';
  }

  //null terminate the string
  numberSize++;
  numberStr = (char *)realloc(numberStr, numberSize);
  numberStr[numberSize - 1] = '\0';

  return numberStr;
}