
/*런타임이 작을때는 어쩔때는 좋게 결과가 나오는데 
런타임을 1000000으로 했을때는 오류가 무조건 발생함
미소비 되었다고 버퍼문제인가? 로그배열 문제인가?
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <pthread.h>
#include <stdatomic.h>

#define N 8                         // 스레드 수
#define MAX 10240                   // 작업 로그 배열 크기
#define BUFSIZE 4                   // 버퍼 크기
#define RUNTIME 1000                // 실행 시간 제한 (마이크로초)
#define RED "\e[0;31m"              // 빨간색 출력용 ANSI 코드
#define RESET "\e[0m"               // ANSI 코드 리셋

int buffer[BUFSIZE];                // 버퍼
int in = 0;                          // 생산자가 아이템을 넣을 위치
int out = 0;                         // 소비자가 아이템을 꺼낼 위치
_Atomic int counter = 0;             // 버퍼 안의 현재 아이템 수
_Atomic int next_item = 0;           // 다음에 생산될 아이템 번호

int task_log[MAX][2];               // 작업 로그 배열 (생산자, 소비자)
_Atomic int produced = 0;            // 생산된 아이템 수
_Atomic int consumed = 0;            // 소비된 아이템 수

bool alive = true;                   // 쓰레드 실행 여부를 제어하기 위한 변수

bool lock = false;                   // 스핀락 변수

// 스핀락 획득 함수
void acquire_lock(bool *lock) {
    while (__atomic_test_and_set(lock, __ATOMIC_ACQUIRE));
}

// 스핀락 해제 함수
void release_lock(bool *lock) {
    __atomic_clear(lock, __ATOMIC_RELEASE);
}

// 생산자 스레드 함수
void *producer(void *arg) {
    int i = *(int *)arg;  // 스레드 ID
    int item;             // 생산된 아이템

    while (alive) {
        acquire_lock(&lock);  // 락 획득

        item = __atomic_fetch_add(&next_item, 1, __ATOMIC_RELAXED);  // 다음 아이템 번호 가져오기

        if (counter < BUFSIZE) {  // 버퍼에 여유 공간이 있는 경우
            buffer[in] = item;    // 버퍼에 아이템 추가
            in = (in + 1) % BUFSIZE;  // 다음 생산자가 넣을 위치 갱신
            __atomic_fetch_add(&counter, 1, __ATOMIC_RELAXED);  // 버퍼 안의 아이템 수 증가

            // 작업 로그 갱신
            if (task_log[item][0] == -1) {
                task_log[item][0] = i;
                __atomic_fetch_add(&produced, 1, __ATOMIC_RELAXED);
            } else {
                printf("<P%d,%d>....ERROR: 아이템 %d 중복생산\n", i, item, item);
                release_lock(&lock);  // 락 해제
                continue;
            }
            printf("<P%d,%d>\n", i, item);  // 생산된 아이템 로그 출력
        }

        release_lock(&lock);  // 락 해제
    }
    pthread_exit(NULL);
}

// 소비자 스레드 함수
void *consumer(void *arg) {
    int i = *(int *)arg;  // 스레드 ID
    int item;             // 소비된 아이템

    while (alive) {
        acquire_lock(&lock);  // 락 획득

        if (counter > 0) {  // 버퍼에 아이템이 있는 경우
            item = buffer[out];    // 버퍼에서 아이템 가져오기
            out = (out + 1) % BUFSIZE;  // 다음 소비자가 꺼낼 위치 갱신
            __atomic_fetch_sub(&counter, 1, __ATOMIC_RELAXED);  // 버퍼 안의 아이템 수 감소

            // 작업 로그 갱신
            if (task_log[item][0] == -1) {
                printf(RED"<C%d,%d>"RESET"....ERROR: 아이템 %d 미생산\n", i, item, item);
                release_lock(&lock);  // 락 해제
                continue;
            } else if (task_log[item][1] == -1) {
                task_log[item][1] = i;
                __atomic_fetch_add(&consumed, 1, __ATOMIC_RELAXED);
            } else {
                printf(RED"<C%d,%d>"RESET"....ERROR: 아이템 %d 중복소비\n", i, item, item);
                release_lock(&lock);  // 락 해제
                continue;
            }
            printf(RED"<C%d,%d>"RESET"\n", i, item);  // 소비된 아이템 로그 출력
        }

        release_lock(&lock);  // 락 해제
    }
    pthread_exit(NULL);
}

int main(void) {
    pthread_t tid[N];  // 스레드 ID 배열
    int i, id[N];

    // 작업 로그 배열 초기화
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

    usleep(RUNTIME);  // 실행 시간 제한

    alive = false;  // 쓰레드 실행 종료 요청

    // 모든 스레드 종료 대기
    for (i = 0; i < N; ++i)
        pthread_join(tid[i], NULL);

    // 모든 아이템이 소비되었는지 확인
    for (i = 0; i < MAX; ++i)
        if (task_log[i][1] == -1 && task_log[i][0] != -1) {
            printf("....ERROR: 아이템 %d 미소비\n", i);
            return 1;
        }

    // 생산된 아이템 수와 소비된 아이템 수 일치 여부 확인
    if (produced == consumed) {
        printf("Total %d items were produced.\n", produced);
        printf("Total %d items were consumed.\n", consumed);
    } else {
        printf("....ERROR: 생산량 불일치\n");
        return 1;
    }

    return 0;
}
