#include <pthread.h>
#include<queue>
#include<functional>
using callback = void*(*)(void*);
//typedef void* callback(void *);
//using WriteCallback = std::function<void(std::string)>;

struct Task{
    callback function;          //任务执行函数地址
    void* arg;                  //参数
    Task(){
        function = nullptr;
        arg = nullptr;
    }
    Task(callback f, void* arg){
        function = f;
        this->arg = arg;
    }
};
// 任务队列
class TaskQueue{
    private:
    pthread_mutex_t m_mutex;
    std::queue<Task> m_queue;
    public:
    TaskQueue();
    ~TaskQueue();

    void addTask(Task& task);
    void addTask(callback func, void* arg);

    Task takeTask();

    inline int taskNumber(){return m_queue.size();}

};
class ThreadPool{
public:
    ThreadPool(int min, int max);
    ~ThreadPool();

    //添加任务
    void addTask(Task t);
    //获取忙线程个数
    int getBusyNumber();
    //获取活着线程个数
    int getAliveNumber();

private:
    static void* worker(void* arg);
    static void* manager(void* arg);
    void threadExit();
private:
    pthread_mutex_t m_lock;
    pthread_cond_t m_notEmpty;
    pthread_t* m_threadIDs;
    pthread_t m_managerID;
    TaskQueue* m_taskQueue;

    int m_minNum, m_maxNum, m_busyNum, m_aliveNum, m_exitNum;
    bool m_shutdown = false;

};
//define
