
/*런타임이 작을때는 좋게 결과가 나오는데 
런타임을 1000000으로 했을때는 오류
생산아이템과 소비 아이템의 수가 맞지 않음
내가 볼때 생산을 하고 종료가 되어서 그런것 같기도하고
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

#define N 8
#define MAX 10240
#define BUFSIZE 4
#define RUNTIME 1000    /* 출력량을 제한하기 위한 실행시간 (마이크로초) */
#define RED "\e[0;31m"
#define RESET "\e[0m"

int buffer[BUFSIZE];
int in = 0;
int out = 0;
_Atomic int counter = 0; // 아토믹 변수로 변경
int next_item = 0;

int task_log[MAX][2];
int produced = 0;
int consumed = 0;

bool alive = true;

#define UNLOCKED 0
#define LOCKED 1

volatile int lock = UNLOCKED;

void spin_lock(volatile int *lock) {
    while (__atomic_test_and_set(lock, __ATOMIC_ACQUIRE)) {}
}

void spin_unlock(volatile int *lock) {
    __atomic_clear(lock, __ATOMIC_RELEASE);
}

void *producer(void *arg)
{
    int i = *(int *)arg;
    int item;
    
    while (alive) {
        spin_lock(&lock); // 스핀락 획득
        
        // 버퍼가 가득 찼는지 확인
        if (counter == BUFSIZE) {
            spin_unlock(&lock); // 스핀락 해제
            continue;
        }
        
        // 새로운 아이템을 생산하여 버퍼에 넣고 관련 변수를 갱신
        item = next_item++;
        buffer[in] = item;
        in = (in + 1) % BUFSIZE;
        
        // 카운터 증가
        counter++;
        
        if (task_log[item][0] == -1) {
            task_log[item][0] = i;
            produced++;
        }
        else {
            printf("<P%d,%d>....ERROR: 아이템 %d 중복생산\n", i, item, item);
            spin_unlock(&lock); // 스핀락 해제
            continue;
        }
        
        printf("<P%d,%d>\n", i, item);
        
        // 스핀락 해제
        spin_unlock(&lock);
    }
    
    pthread_exit(NULL);
}

void *consumer(void *arg)
{
    int i = *(int *)arg;
    int item;
    
    while (alive || counter > 0 || next_item != produced) {
        spin_lock(&lock); // 스핀락 획득
        
        // 버퍼가 비어있는지 확인하고, 모든 생산이 끝났는지 확인
        if (counter == 0 && next_item == produced) {
            spin_unlock(&lock); // 스핀락 해제
            continue;
        }
        
        // 버퍼가 비어있는지 확인
        if (counter == 0) {
            spin_unlock(&lock); // 스핀락 해제
            continue;
        }
        
        // 버퍼에서 아이템을 꺼내고 관련 변수를 갱신
        item = buffer[out];
        out = (out + 1) % BUFSIZE;
        
        // 카운터 감소
        counter--;
        
        if (task_log[item][0] == -1) {
            printf(RED"<C%d,%d>"RESET"....ERROR: 아이템 %d 미생산\n", i, item, item);
            spin_unlock(&lock); // 스핀락 해제
            continue;
        }
        else if (task_log[item][1] == -1) {
            task_log[item][1] = i;
            consumed++;
        }
        else {
            printf(RED"<C%d,%d>"RESET"....ERROR: 아이템 %d 중복소비\n", i, item, item);
            spin_unlock(&lock); // 스핀락 해제
            continue;
        }
        
        printf(RED"<C%d,%d>"RESET"\n", i, item);
        
        // 스핀락 해제
        spin_unlock(&lock);
    }
    
    pthread_exit(NULL);
}

int main(void)
{
    pthread_t tid[N];
    int i, id[N];

    /*
     * 생산자와 소비자를 기록하기 위한 logs 배열을 초기화한다.
     */
    for (i = 0; i < MAX; ++i)
        task_log[i][0] = task_log[i][1] = -1;
    /*
     * N/2 개의 소비자 스레드를 생성한다.
     */
    for (i = 0; i < N/2; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, consumer, id+i);
    }
    /*
     * N/2 개의 생산자 스레드를 생성한다.
     */
    for (i = N/2; i < N; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, producer, id+i);
    }
    /*
     * 스레드가 출력하는 동안 RUNTIME 마이크로초 쉰다.
     * 이 시간으로 스레드의 출력량을 조절한다.
     */
    usleep(RUNTIME);
    /*
     * 스레드가 자연스럽게 무한 루프를 빠져나올 수 있게 한다.
     */
    alive = false;
    /*
     * 자식 스레드가 종료될 때까지 기다린다.
     */
    for (i = 0; i < N; ++i)
        pthread_join(tid[i], NULL);
    /*
     * 생산된 아이템을 건너뛰지 않고 소비했는지 검증한다.
     */
    for (i = 0; i < consumed; ++i)
        if (task_log[i][1] == -1) {
            printf("....ERROR: 아이템 %d 미소비\n", i);
            return 1;
        }
    /*
     * 생산된 아이템의 개수와 소비된 아이템의 개수를 출력한다.
     */
    if (next_item == produced) {
        printf("Total %d items were produced.\n", produced);
        printf("Total %d items were consumed.\n", consumed);
    }
    else {
        printf("....ERROR: 생산량 불일치\n");
        return 1;
    }
    /*
     * 메인함수를 종료한다.
     
     */
    return 0;
}                   
