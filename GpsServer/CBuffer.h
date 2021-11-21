#ifndef CBuffer_h_
#define	CBuffer_h_
/*
vim: ts=2
vim: shiftwidth=2
*/

#include <stdint.h>		// uint8_t
#include <stdbool.h>	// bool

/**
 * Lock-free circular buffer.
 */
template <class E, int size>
class CBuffer {
public:
    /** Initialize buffer to empty state. */
    CBuffer()
    : push_index_(0), pop_index_(0)
    {
    }

    /** Is buffer empty? */
    bool IsEmpty() const
    {
        return push_index_ == pop_index_;
    }
    /** Is buffer full? */
    bool IsFull() const
    {
        return ((push_index_ + 1) % size) == pop_index_;
    }

    /** Push element \c e into buffer.
        * \return true on success, false on failure.
        */
    bool Push(	const E	e)
    {
        const uint8_t	next_push_index = (push_index_ + 1) % size;
        if (next_push_index == pop_index_) {
            return false;
        } else {
            buffer_[push_index_] = e;
            push_index_ = next_push_index;
            return true;
        }
    }

    /** Pop an element, note that it doesn't check for failure!
        * \return Popped element.
        */
    E Pop()
    {
        const E	r  = buffer_[pop_index_];
        pop_index_ = (pop_index_ + 1) % size;;
        return r;
    }

    /** Pop an element, check for failure.
        * \return Popped element.
        */
    bool Pop(	E&	e)
    {
        if (pop_index_ == push_index_) {
            return false;
        } else {
            e = buffer_[pop_index_];
            pop_index_ = (pop_index_ + 1) % size;;
            return true;
        }
    }
private:
    /** Circular buffer. */
    E	buffer_[size];
    /** Push index (heading). */
    volatile uint8_t	push_index_;
    /** Pop index (trailing). */
    volatile uint8_t	pop_index_;
}; // class CBuffer

#endif /* CBuffer_h_ */

