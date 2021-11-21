#include <pthread.h>
#include "Thread.h"

/*****************************************************************************/
Thread::Thread()
{
    status_ = CREATED;
}


/*****************************************************************************/
Thread::~Thread()
{
}


/*****************************************************************************/
int 
Thread::create()
{
    return pthread_create(&threadId_, 0, &Thread::entryPoint, this);
}


/*****************************************************************************/
const Thread::Status 
Thread::status() const
{
    return status_;
}


/*****************************************************************************/
void* 
Thread::entryPoint(
        void* thread)
{
    Thread* t = reinterpret_cast<Thread*>(thread);
    t->run();
    t->status_ = DONE;
    return 0;
}


/*****************************************************************************/
int 
Thread::run()
{
    status_ = RUNNING;
    setup();
    execute();
    return 0;
}
