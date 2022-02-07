#include "os2021_thread_api.h"
#include "json-c/json.h"

struct itimerval Signaltimer;
ucontext_t dispatch_context;
ucontext_t timer_context;
int numofthread=0;
int runtime=0;
int tq=0;
char convertp[4]= {'0','L','M','H'};


typedef struct uthread
{
    ucontext_t context;
    char *name;
    int bp,cp;
    int cancel,qt,wt;
    int tid;
    int becancelled;
    enum state;

    //default = -1
    int event;
    int waitingfor;
    struct uthread *next;
} uthread;

uthread *runningthread=NULL;
uthread *readyq=NULL,*waitingq=NULL,*terminateq=NULL;

int push(uthread **new,uthread **head)
{
    //printf("1");
    if(*head == NULL)
    {
        //printf("2");
        *head=*new;
        //printf("%d\n",head->num);
        return 0;
    }
    else
    {
        uthread *temp=*head;
        uthread *pre=temp;
        while(temp!=NULL)
        {
            if((*new)->cp > temp->cp)
            {
                if(temp==*head)
                {
                    (*new)->next=temp;
                    *head=*new;
                    return 2;
                }
                else
                {
                    pre->next=*new;
                    (*new)->next=temp;
                    return 3;
                }
            }
            else
            {
                pre=temp;
                temp=temp->next;
            }
        }

        pre->next=*new;
        (*new)->next=NULL;
        return 4;

    }
    return 0;
}

uthread* pop(uthread **head)
{
    uthread *temp=*head;
    if(temp==NULL)  return NULL;

    *head=(*head)->next;
    temp->next=NULL;
    return temp;
}

void parse()
{
    struct json_object *all;
    struct json_object *thread;
    struct json_object *name;
    struct json_object *priority;
    struct json_object *function;
    struct json_object *cancel;
    int n;

    all=json_object_from_file("init_threads.json");
    json_object_object_get_ex(all,"Threads",&all);
    n=json_object_array_length(all);

    for(int i=0; i<n; i++)
    {
        thread=json_object_array_get_idx(all,i);
        json_object_object_get_ex(thread,"name",&name);
        json_object_object_get_ex(thread,"entry function", &function);
        json_object_object_get_ex(thread,"priority",&priority);
        json_object_object_get_ex(thread,"cancel mode",&cancel);

        if(OS2021_ThreadCreate(json_object_get_string(name),json_object_get_string(function),json_object_get_string(priority),json_object_get_int(cancel))==-1)
            printf("Invalid argument in thread %d\n",i);
        else
        {
            /*
            uthread *r=readyq;
            while(r!=NULL){
                printf("%s\n",r->name);
                r=r->next;
            }
            */
        }
    }
}

int OS2021_ThreadCreate(char *job_name, char *p_function, char *priority, int cancel_mode)
{
    uthread *t=malloc(sizeof(uthread));
    t->cancel=cancel_mode;
    if(priority[0]=='H') t->cp=t->bp=3;
    if(priority[0]=='M') t->cp=t->bp=2;
    if(priority[0]=='L') t->cp=t->bp=1;
    t->name=job_name;
    t->qt=0;
    t->wt=0;
    t->waitingfor=-1;
    t->event=-1;
    t->tid=numofthread;
    numofthread++;
    // printf("%s\n",t->name);

    if(strcmp(p_function,"Function1")==0)   CreateContext(&(t->context),NULL,&Function1);
    else if(strcmp(p_function,"Function2")==0)   CreateContext(&(t->context),NULL,&Function2);
    else if(strcmp(p_function,"Function3")==0)   CreateContext(&(t->context),NULL,&Function3);
    else if(strcmp(p_function,"Function4")==0)   CreateContext(&(t->context),NULL,&Function4);
    else if(strcmp(p_function,"Function5")==0)   CreateContext(&(t->context),NULL,&Function5);
    else if(strcmp(p_function,"ResourceReclaim")==0) CreateContext(&(t->context),NULL,&ResourceReclaim);
    else
    {
        free(t);
        return -1;
    }

    push(&t,&readyq);
    return t->tid;
}

void OS2021_ThreadCancel(char *job_name)
{
    uthread *temp=readyq;
    uthread *pre=temp;
    uthread *c=NULL;
    uthread *prec=NULL;
    int q=0;
    //cancel itself
    if(strcmp(runningthread->name,job_name)==0)
    {
        printf("runnungthread wants to kill itself\n");
        push(&runningthread,&terminateq);
        setcontext(&dispatch_context);
        return;
    }

    //find in readyq
    while(temp!=NULL)
    {
        if(strcmp(temp->name,job_name)==0)
        {
            c=temp;
            prec=pre;
            q=1;
        }

        pre=temp;
        temp=temp->next;
    }

    //find in waitingq
    temp=waitingq;
    pre=temp;
    while(temp!=NULL)
    {
        if(strcmp(temp->name,job_name)==0)
        {
            c=temp;
            prec=pre;
            q=2;
        }
        pre=temp;
        temp=temp->next;

    }

    if(c==NULL) return;

    if(c->cancel==0)
    {
        printf("%s kills thread %s\n",runningthread->name,c->name);
        if(q==1)
        {
            if(c==readyq)
            {
                readyq=readyq->next;
                push(&c,&terminateq);
            }
            else
            {
                prec->next=c->next;
                push(&c,&terminateq);
            }

        }
        else
        {
            if(c==waitingq)
            {
                waitingq=waitingq->next;
                push(&c,&terminateq);
            }
            else
            {
                prec->next=c->next;
                push(&c,&terminateq);
            }
        }
    }
    else
    {
        c->becancelled=1;
        //printf("%s wants to cancel thread %s\n\n\n\n\n",runningthread->name,c->name);
    }

}

void OS2021_ThreadWaitEvent(int event_id)
{
    runningthread->event=event_id;
    printf("%s wants to wait for event %d\n",runningthread->name,runningthread->event);
    if(runningthread->cp < 3)
    {
        runningthread->cp++;
        printf("The priority of thread %s is changed from %c to %c\n",runningthread->name,convertp[(runningthread->cp-1)],convertp[runningthread->cp]);
    }
    push(&runningthread,&waitingq);
    swapcontext(&(runningthread->context),&dispatch_context);
}

void OS2021_ThreadSetEvent(int event_id)
{
    uthread *temp=waitingq;
    uthread *pre=temp;
    uthread *tempt;

    while(temp!=NULL)
    {
        if(temp->event == event_id)
        {
            tempt=temp;
            tempt->event=-1;

            if(tempt==waitingq)
            {
                waitingq=waitingq->next;
                tempt->next=NULL;
                push(&tempt,&readyq);
                printf("%s changes the status of %s to READY\n",runningthread->name,tempt->name);
            }
            else
            {
                pre->next=tempt->next;
                tempt->next=NULL;
                push(&tempt,&readyq);
                printf("%s changes the status of %s to READY\n",runningthread->name,tempt->name);
            }

        }
        temp=temp->next;
    }
}

void OS2021_ThreadWaitTime(int msec)
{
    uthread *temp=runningthread;
    if(temp->cp < 3)
    {
        temp->cp++;
        printf("The priority of thread %s is changed from %c to %c\n",temp->name,convertp[(temp->cp-1)],convertp[temp->cp]);
    }
    temp->waitingfor=msec;
    push(&temp,&waitingq);
    //printf("wtfunction push %s to waitingq\n",temp->name);
    swapcontext(&(temp->context),&dispatch_context);
}

void OS2021_DeallocateThreadResource()
{
    uthread *temp=terminateq;
    while(temp!=NULL)
    {
        terminateq=terminateq->next;
        printf("The memory space by %s has been released\n",temp->name);
        free(temp);
        temp=terminateq;
    }
}

void OS2021_TestCancel()
{
    if(runningthread->becancelled==1)
    {
        push(&runningthread,&terminateq);
        setcontext(&dispatch_context);
    }
}

void CreateContext(ucontext_t *context, ucontext_t *next_context, void *func)
{
    getcontext(context);
    context->uc_stack.ss_sp = malloc(STACK_SIZE);
    context->uc_stack.ss_size = STACK_SIZE;
    context->uc_stack.ss_flags = 0;
    context->uc_link = next_context;
    makecontext(context,(void (*)(void))func,0);
    //printf("context create success\n");
}

void ResetTimer()
{
    Signaltimer.it_value.tv_sec = 0;
    Signaltimer.it_value.tv_usec = 10000;
    if(setitimer(ITIMER_REAL, &Signaltimer, NULL) < 0)
    {
        printf("ERROR SETTING TIME SIGALRM!\n");
    }
}

void Dispatcher()
{
    runningthread=NULL;
    runningthread=pop(&readyq);
    runningthread->next=NULL;
    //printf("dispatcher poped %s\n",runningthread->name);
    if(runningthread->cp==3) tq=300;
    if(runningthread->cp==2) tq=200;
    if(runningthread->cp==1) tq=100;
    runtime=0;
    ResetTimer();
    //uthread *temp=readyq;
    /*
    printf("%s %d\n",runningthread->name,tq);
    while(temp!=NULL)
    {
        printf("%s ",temp->name);
        temp=temp->next;
    }
    printf("\n");
    */
    setcontext(&(runningthread->context));
}

void report()
{

    int tid=runningthread->tid;
    int bp=runningthread->bp;
    int cp=runningthread->cp;
    int qt=runningthread->qt;
    int wt=runningthread->wt;
    uthread *temp;

    printf("\n");
    printf("**********************************************************************************************************\n");
    printf("*\tTID\tName\t\tState\t\tB_Priority\tC_Priority\tQ_Time\t\tW_time\t *\n");
    printf("*\t%d\t%-10s\tRUNNING\t\t%d\t\t%d\t\t%d\t\t%d\t *\n", tid, runningthread->name, bp, cp, qt, wt);

    temp=readyq;

    while(temp!=NULL)
    {
        tid=temp->tid;
        bp=temp->bp;
        cp=temp->cp;
        qt=temp->qt;
        wt=temp->wt;

        printf("*\t%d\t%-10s\tREADY\t\t%d\t\t%d\t\t%d\t\t%d\t *\n", tid, temp->name, bp, cp, qt, wt);
        temp=temp->next;
    }
    temp=waitingq;
    while(temp!=NULL)
    {
        tid=temp->tid;
        bp=temp->bp;
        cp=temp->cp;
        qt=temp->qt;
        wt=temp->wt;

        printf("*\t%d\t%-10s\tWAITING\t\t%d\t\t%d\t\t%d\t\t%d\t *\n", tid, temp->name, bp, cp, qt, wt);
        temp=temp->next;
    }

    printf("**********************************************************************************************************\n");


    return;

}

void timehandler()
{
    //printf("in handler\nTQ=%d\n",tq);
    tq-=10;
    runtime+=10;

    uthread *temp=readyq;
    while(temp!=NULL)
    {
        temp->qt+=10;
        temp=temp->next;
    }

    uthread *tempx=waitingq;
    uthread *pre=tempx;
    while(tempx!=NULL)
    {
        tempx->wt+=10;
        tempx->waitingfor-=10;
        if(tempx->waitingfor<=0 && tempx->event==-1)
        {
            uthread *tempt=tempx;
            uthread *prep=tempt;
            tempt->waitingfor=0;
            //tempt->wt=0;

            if(tempt==waitingq)
            {
                waitingq=waitingq->next;
                tempt->next=NULL;
                push(&tempt,&readyq);
                // printf("%s wait enough, push to ready\n",tempt->name);
            }
            else
            {
                prep->next=tempt->next;
                tempt->next=NULL;
                push(&tempt,&readyq);
                // printf("%s wait enough, push to ready\n",tempt->name);
            }

        }
        pre=tempx;
        tempx=tempx->next;
    }

    if(tq==0)
    {
        //printf("%s runs out of tq\n",runningthread->name);
        if(runningthread->cp > 1)
        {
            runningthread->cp-=1;
            printf("The priority of thread %s is changed from %c to %c\n",runningthread->name,convertp[(runningthread->cp+1)],convertp[runningthread->cp]);
        }
        push(&runningthread,&readyq);
        swapcontext(&(runningthread->context),&dispatch_context);
    }
    ResetTimer();
    return;
}

void StartSchedulingSimulation()
{
    /*Set Timer*/
    Signaltimer.it_interval.tv_usec = 100;
    Signaltimer.it_interval.tv_sec = 0;
    ResetTimer();
    signal(SIGTSTP,report);
    signal(SIGALRM,timehandler);


    /*Create Context*/
    CreateContext(&dispatch_context, NULL, &Dispatcher);
    OS2021_ThreadCreate("reclaimer","ResourceReclaim","L",1);
    parse();
    setcontext(&dispatch_context);
}
