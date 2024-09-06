#include <stdio.h>
#include <stdlib.h>

#define MAX_INST 1024
#define READY 0
#define RUNNING 1
#define DONE 2
#define TRUE 1
#define FALSE 0

/******************************************************************************* 
 * Filename    : edf.c 
 * Author      : Joshua Meharg
 * Date        : 9 March 2023
 * Description : Earliest Deadline First Scheduling Algorithm 
 * Pledge      : I pledge my honor that I have abided by the Stevens Honor System
 ******************************************************************************/ 

//GLOBAL VARIABLES
int total_waited = 0;
int total_procs = 0;
int total_created = 0;

struct proc_instance {
    double id;
    int CPU;
    int period;
    int state;
    int time_created;
    int time_started;
    int time_waited;
    int exec_left;
    int deadline;
    int time_finished;

};

struct process {
    int id;
    int CPU;
    int period;
    int instances; //amount of instances of process
    struct proc_instance* insts[MAX_INST]; 
};


typedef struct Node {
    struct proc_instance* proc;
    struct Node *prev;
} Node;

typedef struct Queue {
    int size;
    Node* front;
    Node* back;
    
} Queue;

Queue *createQueue(){

    Queue* q = (Queue*)malloc(sizeof(Queue));
    q->size = 0;
    q->front = NULL;
    q->back = NULL;

    return q;

}

void addQ(Queue* q, Node* proc){
    proc->prev = NULL;

    //queue is empty
    if (q->size == 0){
        q->front = proc;
        q->back = proc;

        q->size++;
        return;
    }

    //queue is not empty
    q->back->prev = proc;
    q->back = proc;
    q->size++;

    return;

}

Node* removeFQ(Queue* q){
    Node* node;
    node = q->front;
    q->front = (q->front)->prev;
    q->size--;
    return node;
}

void cleanQueue(Queue* q){
    Node* curr;
    Node* next;
    curr = q->front;
    while(curr != NULL){
        curr = removeFQ(q);
        next = curr->prev;
        //total_waited += curr->proc->time_waited;
        //free the process and node objects
        free(curr->proc);
        free(curr);
        curr = next;
    }
    free(q);
}

Node* createNode(struct proc_instance* proc){
    Node* node = (Node*)malloc(sizeof(Node));
    node->proc = proc;
    node->prev = NULL;
    return node;
}


Node* copyNode(Node* node){
    struct proc_instance* p = (struct proc_instance*)malloc(sizeof(struct proc_instance));
    p->id = node->proc->id;
    p->CPU = node->proc->CPU;
    p->period = node->proc->period;
    p->state = node->proc->state;
    p->time_created = node->proc->time_created;
    p->time_started = node->proc->time_started;
    p->time_waited = node->proc->time_waited;
    p->exec_left = node->proc->exec_left;
    p->deadline = node->proc->deadline;
    p->time_finished = node->proc->time_finished;
    return createNode(p);
}




Node* getNext(Queue* q){
    //returns the oldest process with the earliest deadline and is in the READY state

          Node* curr = q->front;
          Node* next;
          int min;
          //initialize min to be node with a state that is not DONE
            while(curr !=NULL){
                if (curr->proc->state != DONE){
                    next = curr;
                    min = curr->proc->deadline;
                    break;
                }
                curr = curr->prev;
            }

            if (curr == NULL){
                return NULL;
            }
            

            while(curr != NULL){
                //gets based off the earliest deadline
                if (curr->proc->state == DONE){
                    curr = curr->prev;
                    continue;
                }
                if (min > curr->proc->deadline){
                    next = curr;
                    min = curr->proc->deadline;
                    curr = curr->prev;
                    continue;
                }
                //if deadlines are the same the oldest processes gets priority
                if (min == curr->proc->deadline){
                    if (curr->proc->time_created < next->proc->time_created){
                        next = curr;
                    }
                }

                curr = curr->prev;
            }

            return next;
        }


    void printQueue(Queue* queue, int cl_time){
        printf("%d: processes:", cl_time);
            Node* curr = queue->front;
            while (curr != NULL){
                if (curr->proc->state != DONE){
                    printf(" %d (%d ms)", (int)curr->proc->id, curr->proc->exec_left);
                }
                curr = curr->prev;
            }

            printf("\n");
    }





int getGCD(int x, int y){
    if(y == 0){
        return x;
    }

    return getGCD(y, x%y);
}






void initializeProcs(struct process** procs, int length, int cl_time, Queue* q){
            for(int i=0; i<length; i++){
                struct proc_instance* p = (struct proc_instance*)malloc(sizeof(struct proc_instance));
                p->state = READY; 
                p->id = (double)i + 1.1; //p.id is <process number type>.<the instance of the process number type>
                p->time_created = i;
                p->deadline = procs[i]->period;
                p->CPU = procs[i]->CPU;
                p->period = procs[i]->period;
                p->time_waited = 0;
                p->exec_left = p->CPU;
                p->time_started = -1;
                procs[i]->instances = 1;
                procs[i]->insts[0] = p;
                total_procs++;
                addQ(q, createNode(p));
            }


        }

    void executeRunning(Queue* q){
        Node* curr = q->front;
        while(curr != NULL){
            if (curr->proc->state == RUNNING){
                curr->proc->exec_left--;
                return;
            }
            curr = curr->prev;
        }
    }

    void setRunProc(Node* p, int cl_time){
            p->proc->state = RUNNING;
            if (p->proc->time_started == -1){
                p->proc->time_started = cl_time;
            }
            printf("%d: process %d starts\n", cl_time, (int)p->proc->id);
        
        }

    void updateProcs(Queue* q, int cl_time, int nprocs){
            Node* curr;
            Node* finished = NULL;
            curr = q->front;

            //create an array of queues to hold processes that missed deadlines
            Queue* deadlines[nprocs];
                for (int i=0; i<nprocs; i++){
                    deadlines[i] = createQueue();
                }

            while(curr != NULL){

                if(curr->proc->state == DONE){
                    curr = curr->prev;
                    continue;
                }

                

                //if deadline has passed add to deadline array to print later in sorted format
                if ((curr->proc->deadline <= cl_time) & (curr->proc->exec_left > 0)){
                    addQ(deadlines[(int)curr->proc->id-1], copyNode(curr));
                    //printf("%d: process %d missed deadline (%d ms left)\n", cl_time, (int)curr->proc->id, curr->proc->exec_left);
                    curr->proc->deadline = cl_time + (curr->proc->period - cl_time%curr->proc->period);
                }   

                //if process is running 
                if (curr->proc->state == RUNNING){

                    //check to see its now finished
                    if (curr->proc->exec_left == 0){
                        curr->proc->state = DONE;
                        curr->proc->time_finished = cl_time;
                        //total_waited+= curr->proc->time_waited;
                        finished = curr;
                        //get the next earliest process and set it to the running state
                        curr = curr->prev;
                        continue;
                    }
                }

               
                

                //if process is still waiting waiting time is incremented
                if (curr->proc->state == READY){
                    curr->proc->time_waited++;
                    total_waited++;
                    
                }
                curr = curr->prev;
                
            }

            if (finished != NULL){
                printf("%d: process %d ends\n", cl_time, (int)finished->proc->id);
            }

            //print all deadlines from deadline queue
            for (int i=0; i<nprocs; i++){
                Node* curr = deadlines[i]->front;
                while(curr != NULL){
                    printf("%d: process %d missed deadline (%d ms left)\n", cl_time, i+1, curr->proc->exec_left);
                    curr = curr->prev;
                }
                //clean up queue
                cleanQueue(deadlines[i]);
            }
            
            

        }

    int createProcs(struct process** procs, int nprocs, int cl_time, Queue* queue){
            int created = 0;
            for (int i=0; i<nprocs; i++){
                
                //checks to see if one period cycle has passed leading to a new process being created
                if (cl_time%procs[i]->period == 0){
                    struct proc_instance* p = (struct proc_instance*)malloc(sizeof(struct proc_instance));
                    procs[i]->instances++;
                    p->id = (double)i+1; //p.id is <process number type>.<the instance of the process number type>
                    p->CPU = procs[i]->CPU;
                    p->period = procs[i]->period;
                    p->state = READY; 
                    p->time_created = cl_time;
                    p->deadline = cl_time + (p->period - cl_time%p->period);
                    p->time_waited = 0;
                    p->exec_left = p->CPU;
                    p->time_started = -1;
                    procs[i]->insts[procs[i]->instances-1] = p;
                    total_procs++;
                    addQ(queue, createNode(p));
                    created++;
                }
            }

            return created;
        }



    Node* getRunning(Queue* q){
            Node* curr = q->front;
            while(curr != NULL){
                if (curr->proc->state == RUNNING){
                    return curr;
                }
                curr = curr->prev;
            }
            return NULL;
        }

int main(const int argc, char* argv[]){
    int nprocs;
    int MaxTime;
    if (argc != 1){
        printf("Usage: %s\n", argv[0]);
        return -1;
    }

        //////////////////////INPUT VALIDATION

        printf("Enter the number of processes to schedule: ");
        // fflush(stdout); //this line is to ensure message is printed before reading
        // read(0, hold, 2);
        scanf("%d", &nprocs);

        if (nprocs <= 0){
            printf("Error: number of processes has to be over 0\n");
            return -1;
        }


        /////////////////////IMPORTANT CONTAINERS
        struct process* procs[nprocs]; // array to hold process number types
        
        Queue* queue = createQueue();
        
        /////////////////////



        //////////////////////INPUT HANDLING
        //get all the amount of processes and information

        for (int i=0; i<nprocs; i++){
            struct process* proc = (struct process*)malloc(sizeof(struct process));
            printf("Enter the CPU time of process %d: ", i+1);
            scanf("%d", &proc->CPU);

            printf("Enter the period of process %d: ", i+1);
            scanf("%d", &proc->period);

            proc->id = i+1;

            procs[i] = proc;

            
        }

        //////////////////////MAXTIME CALCULATION
        // (LCM of all periods)
        int gcd = procs[0]->period;
        int lcm = procs[0]->period;
        for(int i=1; i<nprocs; i++){
            gcd = getGCD(procs[i]->period, lcm);
            lcm = (lcm*procs[i]->period)/gcd;
        }
        MaxTime = lcm;

                



        //////////////////////DRIVER LOOP
     
        initializeProcs(procs, nprocs, 0, queue);
        printQueue(queue, 0);
        setRunProc(queue->front, 0);
        Node* running=NULL;
        Node* next=NULL;
        int created;
        

        for (int i=1; i<MaxTime; i++){
            //executes running process for 1 ms
            executeRunning(queue);

            //update procs:
            //increments waiting times
            //sees if running proc is finished
            //finds all processes who missed deadline and recalculates new one
            updateProcs(queue, i, nprocs);

            //creates new procs based on their period times and the current clock value
            created = createProcs(procs, nprocs, i, queue);

            //if a process is created print
             if (created > 0){
                printQueue(queue, i);
            }

            //get current running process
            running = getRunning(queue);

            //get process with earliest deadline
            next = getNext(queue);

            //No processes with READY or RUNNING state in queue
            if (next == NULL){
                continue;
            }

            //Previously running process ended
            if (running == NULL){
                setRunProc(next, i);
                continue;
            }

            //if current running process does not have the earliest deadline it is preempted
            if (running->proc->id != next->proc->id){
                running->proc->state = READY;
                printf("%d: process %d preempted!\n", i, (int)running->proc->id);
                setRunProc(next, i);
            }

            
           


        }

        //increment for all processes still waiting at MaxTime
        Node* waiting = queue->front;
        while(waiting != NULL){
            if (waiting->proc->state == READY){
                total_waited++;
            }
            waiting = waiting->prev;
        }

        //At Max Time see if the running process finsihed
        running = getRunning(queue);
        //If a process is still running at the maxtime
        if (running != NULL){
            running->proc->exec_left--;
            if (running->proc->exec_left == 0){
                printf("%d: process %d ends\n", MaxTime, (int)running->proc->id);
            }
        }
        

        //cleanup and get totals handling
        cleanQueue(queue);
        for(int i=0; i<nprocs; i++){
            
            free(procs[i]);
        }
        
       printf("%d: Max Time reached\n", MaxTime);
       printf("Sum of all waiting times: %d\n", total_waited);
       printf("Number of processes created: %d\n", total_procs);
       printf("Average Waiting Time: %.2lf\n", (double)total_waited/total_procs);

    

    return 0;
}