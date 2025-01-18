#pragma once

#include <pthread.h>

namespace meha
{

class RWLock {
public:
    RWLock() {
        pthread_rwlock_init(&m_rwlock, nullptr);
    }

    ~RWLock() {
        pthread_rwlock_destroy(&m_rwlock);
    }

    void ReadLock() {
        pthread_rwlock_rdlock(&m_rwlock);
    }

    void WriteLock() {
        pthread_rwlock_wrlock(&m_rwlock);
    }

    void Unlock() {
        pthread_rwlock_unlock(&m_rwlock);
   }
private:
    pthread_rwlock_t m_rwlock;
};

}