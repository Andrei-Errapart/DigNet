#ifndef THREAD_H
#define THREAD_H

#include <pthread.h>

/**
	A simple wrapper class around PThread. Implement setup and execute to do your own stuff and run create() to start the thread.
*/
class Thread {
    enum Status {
        CREATED,    ///< Thread is created but not yet running
        RUNNING,    ///< Thread is running
        DONE        ///< Thread has finished running
    };
public:
    Thread();
    /**  creates and runs the thread */
    int 
    create();

    virtual 
    ~Thread();

    /** Shows the \c Status of the thread */
    const Status 
    status() const;

protected:
    /**  Runs the thread, calls \c setup() and \c execute() */
    int 
    run();

    /** Creates the thread and runs it 
     * @param thread pointer to \c this, calls \c thread->run() after creating the thread
    */
    static void* 
    entryPoint(
        void* thread);

    /** Thread setup goes here. Gets run before execute() */
    virtual void 
    setup() = 0;

    /** User thread mainloop should go in here */
    virtual void 
    execute() = 0;

private:
    /** Thread ID */
    pthread_t threadId_;

    /** Thread status */
    Status status_;
};

#endif
