#include "threadPool.h"
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <unistd.h>
using namespace std;
TaskQueue::TaskQueue(){
    pthread_mutex_init(&m_mutex, NULL);
}
TaskQueue::~TaskQueue(){
    pthread_mutex_destroy(&m_mutex);
}
void TaskQueue::addTask(Task& t){
    pthread_mutex_lock(&m_mutex);
    m_queue.push(t);
    pthread_mutex_unlock(&m_mutex);
}
void TaskQueue::addTask(callback func, void* arg){
    pthread_mutex_lock(&m_mutex);
    Task t;
    t.function = func, t.arg = arg;
    m_queue.push(t);
    pthread_mutex_unlock(&m_mutex);
}
Task TaskQueue::takeTask(){
    Task t;
    //对队列加锁，取出队头任务返回
    pthread_mutex_lock(&m_mutex);
    if(m_queue.size() > 0){
        t = m_queue.front();
        m_queue.pop();
    }
    pthread_mutex_unlock(&m_mutex);
    return t;
}
//Importance Part:

ThreadPool::ThreadPool(int min, int max){
    m_taskQueue = new TaskQueue();
    do{
        m_minNum = min;
        m_maxNum = max;
        m_busyNum = 0;
        m_aliveNum = min;

        m_threadIDs = new pthread_t[max];
        if(m_threadIDs == nullptr){
            cout << "malloc thread fail!"<<endl;
            break;
        }
        memset(m_threadIDs, 0, sizeof(pthread_t) * max);
        pthread_mutex_init(&m_lock, NULL);
        pthread_cond_init(&m_notEmpty, NULL);

        //create threads
        for(int i=0; i<min; ++i){
            pthread_create(&m_threadIDs[i], NULL, worker, this);
        }
        pthread_create(&m_managerID, NULL, manager, this);
    }while(0);
}
ThreadPool::~ThreadPool(){
    m_shutdown = true;
    // destroy manager thread
    pthread_join(m_managerID, NULL);
    // 唤醒所有消费者线程 ???
    for(int i = 0; i< m_aliveNum; ++i){
        pthread_cond_signal(&m_notEmpty);
    }
    if(m_taskQueue) delete m_taskQueue;
    if(m_threadIDs) delete[] m_threadIDs;
    pthread_mutex_destroy(&m_lock);
    pthread_cond_destroy(&m_notEmpty);
}
void ThreadPool::addTask(Task t){
    if(m_shutdown) return;
    m_taskQueue->addTask(t);
    pthread_cond_signal(&m_notEmpty);
}
int ThreadPool::getAliveNumber(){
    int threadNum = 0;
    pthread_mutex_lock(&m_lock);
    threadNum = m_aliveNum;
    pthread_mutex_unlock(&m_lock);
    return threadNum;
}
int ThreadPool::getBusyNumber(){
    int busyNum = 0;
    pthread_mutex_lock(&m_lock);
    busyNum = m_busyNum;
    pthread_mutex_unlock(&m_lock);
    return busyNum;
}
void* ThreadPool::worker(void* arg){
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while(true){
        //访问任务队列加锁
        pthread_mutex_lock(&pool->m_lock);
        //任务列表为空则阻塞线程
        while(pool->m_taskQueue->taskNumber() == 0 && !pool->m_shutdown){
            cout << "thread" << to_string(pthread_self()) << "waiting.."<<endl;
            //阻塞
            pthread_cond_wait(&pool->m_notEmpty, &pool->m_lock);
            //阻塞结束后判断是否要销毁
            if(pool->m_exitNum > 0){
                pool->m_aliveNum--;
                pthread_mutex_lock(&pool->m_lock);
                pool->threadExit();
            }
        }
    
        // 判断线程池是否被关闭了 ???
        if (pool->m_shutdown)
        {
            pthread_mutex_unlock(&pool->m_lock);
            pool->threadExit();
        }
        //从任务队列取出任务，busy+1，线程池解锁，然后执行任务
        Task t = pool->m_taskQueue->takeTask();
        pool->m_busyNum++;
        pthread_mutex_unlock(&pool->m_lock);
        cout<<"thread "<<to_string(pthread_self()) << " start working.." << endl;
        t.function(t.arg);
        t.arg = nullptr; //???

        //任务处理结束，busy-1
        cout<<"thread "<<to_string(pthread_self()) << " end working.." << endl;
        pthread_mutex_lock(&pool->m_lock);
        pool->m_busyNum--;
        pthread_mutex_unlock(&pool->m_lock);

    }
    return nullptr;
}
// 管理者线程任务函数
void* ThreadPool::manager(void* arg){
    ThreadPool* pool = static_cast<ThreadPool*>(arg);
    while(!pool->m_shutdown){
        sleep(5);
        
        pthread_mutex_lock(&pool->m_lock);
        int queSize = pool->m_taskQueue->taskNumber();
        int liveCount = pool->m_aliveNum;
        int busyCount = pool->m_busyNum;
        pthread_mutex_unlock(&pool->m_lock);

        
        printf("evrey 5 sec adjust pthread nums live:%d,busy:%d\n",liveCount,busyCount);
        continue;
        //线程变化个数
        const int NUMBER = 2;
        //任务队列中任务个数大于存活线程数且存活数小于线程池最大线程设定，创建NUMBER个线程
        if(queSize > liveCount && liveCount < pool->m_maxNum){
            pthread_mutex_lock(&pool->m_lock);
            int num = 0;
            //找到未创建的线程句柄创建后，添加道存活列表里
            for(int i=0; i < pool->m_maxNum && num < NUMBER &&
                pool->m_aliveNum < pool->m_maxNum; i++){
                    if(pool->m_threadIDs[i] == 0){
                        pthread_create(&pool->m_threadIDs[i], NULL, worker, pool);
                        num++;
                        pool->m_aliveNum++;
                    }
                }
            pthread_mutex_unlock(&pool->m_lock);
        }
        //同理销毁多余线程:有一半以上未干活线程时，销毁NUMBER个
        if(busyCount * 2 < liveCount && liveCount > pool->m_minNum){
            pthread_mutex_lock(&pool->m_lock);
            pool->m_exitNum = NUMBER;
            pthread_mutex_unlock(&pool->m_lock);
            for (int i = 0; i < NUMBER; ++i)
            {
                pthread_cond_signal(&pool->m_notEmpty);
            }
        }
    }
    return nullptr;
}

void ThreadPool::threadExit(){
    pthread_t tid = pthread_self();
    for(int i = 0; i < m_maxNum; ++i){
        if(m_threadIDs[i] == tid){
            cout << "threadExit() function: thread " 
                << to_string(pthread_self()) << " exiting..." << endl;
            m_threadIDs[i] = 0;
            break;
        }
    }
    pthread_exit(NULL);
}

int main2(int argc, char* argv[] ){
    /* ThreadPool* myPool = new ThreadPool(2, 10);
    Task t;
    int arg = 10;
    t.function = [](void* arg){
        int count = *static_cast<int*>(arg);
        std::cout << "task count is: "<< count <<std::endl;
        for(int i=count;i>=0;i--){
            count--;
        }
        return (void*)1;
    };
    while(true){
        sleep(3);
        t.arg = &arg;
        arg *= 10;
        myPool->addTask(t);
    }
    std::cout << "threadPool for epoll connect"<<std::endl; */
    return -1;
}