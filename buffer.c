#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>
/*런타임이 작을때는 어쩔때는 좋게 결과가 나오는데 
런타임을 1000000으로 했을때는 오류가 무조건 발생함
미소비 되었다고 뜸 그리고 출력했을때 생산자가 2개씩 뜸
*/
#define N 8
#define MAX 10240
#define BUFSIZE 4
#define RUNTIME 100000    /* 출력량을 제한하기 위한 실행시간 (마이크로초) */
#define RED "\e[0;31m"
#define RESET "\e[0m"

int buffer[BUFSIZE];
_Atomic int in = 0;
_Atomic int out = 0;
_Atomic int counter = 0;
_Atomic int next_item = 0;

int task_log[MAX][2]; // 생산된 아이템과 소비된 아이템의 로그를 기록하는 배열
int produced = 0; // 생산된 아이템의 수
int consumed = 0; // 소비된 아이템의 수

bool alive = true; // 스레드가 살아 있는지를 나타내는 변수

// 락 프리 스핀락 구현
_Atomic int lock = 0;

// 스핀락 획득 함수
void spinlock_acquire(_Atomic int *lock) {
    int expected = 0;
    while (!atomic_compare_exchange_weak(lock, &expected, 1)) {
        expected = 0;
    }
}

// 스핀락 해제 함수
void spinlock_release(_Atomic int *lock) {
    *lock = 0;
}

// 생산자 스레드 함수
void *producer(void *arg) {
    int i = *(int *)arg;
    int item;
    
    while (alive) {
        // 아이템을 생성할 때마다 next_item 값을 읽어와서 사용
        item = atomic_fetch_add(&next_item, 1);
        
        // 스핀락 획득
        spinlock_acquire(&lock);
        
        if (counter < BUFSIZE) {
            buffer[in] = item;
            in = (in + 1) % BUFSIZE;
            counter++;
            // task_log 배열의 인덱스 계산 수정
            if (task_log[item % MAX][0] == -1) {
                task_log[item % MAX][0] = i;
                produced++;
            } else {
                printf("<P%d,%d>....ERROR: 아이템 %d 중복생산\n", i, item, item);
            }
            printf("<P%d,%d>\n", i, item);
        } else {
            // 버퍼가 가득 찬 경우, 스핀락 해제 후 잠시 대기
            spinlock_release(&lock);
            usleep(100);
            continue;
        }
        /*
         * 생산한 아이템을 출력한다.
         */
        printf("<P%d,%d>\n", i, item);
        // 스핀락 해제
        spinlock_release(&lock);
    }
    pthread_exit(NULL);
}

// 소비자 스레드 함수
void *consumer(void *arg) {
    int i = *(int *)arg;
    int item;
    
    while (alive) {
        // 스핀락 획득
        spinlock_acquire(&lock);
        
        if (counter > 0) {
            item = buffer[out];
            out = (out + 1) % BUFSIZE;
            counter--;
            if (task_log[item % MAX][0] == -1) {
                printf(RED"<C%d,%d>"RESET"....ERROR: 아이템 %d 미생산\n", i, item, item);
            } else {
                if (task_log[item % MAX][1] == -1) {
                    task_log[item % MAX][1] = i;
                    consumed++;
                } else {
                    printf(RED"<C%d,%d>"RESET"....ERROR: 아이템 %d 중복소비\n", i, item, item);
                    continue;
                }
            }
            printf(RED"<C%d,%d>"RESET"\n", i, item);
        }
        
        // 스핀락 해제
        spinlock_release(&lock);
    }
    pthread_exit(NULL);
}

int main(void) {
    pthread_t tid[N];
    int i, id[N];

    // task_log 배열 초기화
    for (i = 0; i < MAX; ++i)
        task_log[i][0] = task_log[i][1] = -1;

    // 소비자 스레드 생성
    for (i = 0; i < N/2; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, consumer, id+i);
    }

    // 생산자 스레드 생성
    for (i = N/2; i < N; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, producer, id+i);
    }

    usleep(RUNTIME);
    alive = false;

    // 스레드 종료 대기
    for (i = 0; i < N; ++i)
        pthread_join(tid[i], NULL);

    // 미소비 확인
    for (i = 0; i < consumed; ++i)
        if (task_log[i][1] == -1) {
            printf("....ERROR: 아이템 %d 미소비\n", i);
            return 1;
        }

    // 생산된 아이템과 소비된 아이템의 수 출력
    if (next_item == produced) {
        printf("Total %d items were produced.\n", produced);
        printf("Total %d items were consumed.\n", consumed);
    } else {
        printf("....ERROR: 생산량 불일치\n");
        return 1;
    }

    return 0;
}
