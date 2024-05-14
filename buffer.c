
/*
런타임이 작을때는 좋게 결과가 나오는데 
런타임을 1000000으로 했을때는 오류
생산아이템과 소비 아이템의 수가 맞지 않음
내가 볼때 생산을 하고 종료가 되어서 그런것 같기도하고


과제3은 pthread의 spinlock을 사용해서 해결하는 과제가 아닙니다. CAE 명령어를 사용해서 spinlock을 구현하고 (proj3.pdf 참조), 
이를 두 문제에 응용하는 과제입니다. CAE 명령어를 이해하라고 과제를 준 것이기 때문에 pthread의 spinklock을 사용하면 결과물로 인정하지 않습니다.
이게 뭔소린지;;
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

int buffer[BUFSIZE];         // 아이템을 담을 버퍼
int in = 0;                   // 다음에 아이템을 삽입할 위치
int out = 0;                  // 다음에 아이템을 꺼낼 위치
_Atomic int counter = 0;      // 현재 버퍼에 있는 아이템의 수 (원자적 연산으로 보호)
int next_item = 0;            // 다음에 생산될 아이템의 번호

int task_log[MAX][2];         // 생산된 아이템과 소비된 아이템을 기록하기 위한 배열
int produced = 0;             // 생산된 아이템의 수
int consumed = 0;             // 소비된 아이템의 수

bool alive = true;            // 프로그램이 실행 중인지 여부를 나타내는 플래그

#define UNLOCKED 0
#define LOCKED 1

volatile int lock = UNLOCKED; // 스핀락을 위한 변수

// 스핀락 획득 함수
void spin_lock(volatile int *lock) {
    while (__atomic_test_and_set(lock, __ATOMIC_ACQUIRE)) {}
}

// 스핀락 해제 함수
void spin_unlock(volatile int *lock) {
    __atomic_clear(lock, __ATOMIC_RELEASE);
}

// 생산자 스레드 함수
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
        
        // 아이템을 기록하고 생산된 아이템 수 증가
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

// 소비자 스레드 함수
void *consumer(void *arg)
{
    int i = *(int *)arg;
    int item;
    
    // 버퍼가 비어있지 않거나 모든 생산이 완료될 때까지 실행
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
        
        // 아이템을 기록하고 소비된 아이템 수 증가
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
    pthread_t tid[N];   // 스레드 식별자 배열
    int i, id[N];       // 스레드 식별자와 인덱스를 저장하는 변수

    // 생산된 아이템과 소비된 아이템을 기록하는 배열 초기화
    for (i = 0; i < MAX; ++i)
        task_log[i][0] = task_log[i][1] = -1;
    
    // N/2 개의 소비자 스레드 생성
    for (i = 0; i < N/2; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, consumer, id+i);
    }
    
    // N/2 개의 생산자 스레드 생성
    for (i = N/2; i < N; ++i) {
        id[i] = i;
        pthread_create(tid+i, NULL, producer, id+i);
    }
    
    // 일정 시간 동안 스레드가 출력하도록 지연
    usleep(RUNTIME);
    
    // 프로그램 종료를 위해 alive 플래그 변경
    alive = false;
    
    // 모든 스레드가 종료될 때까지 대기
    for (i = 0; i < N; ++i)
        pthread_join(tid[i], NULL);
    
    // 모든 아이템이 소비되었는지 검증
    for (i = 0; i < consumed; ++i)
        if (task_log[i][1] == -1) {
            printf("....ERROR: 아이템 %d 미소비\n", i);
            return 1;
        }
    
    // 생산된 아이템의 수와 소비된 아이템의 수 출력
    if (next_item == produced) {
        printf("Total %d items were produced.\n", produced);
        printf("Total %d items were consumed.\n", consumed);
    }
    else {
        printf("....ERROR: 생산량 불일치\n");
        return 1;
    }
    
    return 0;
}

