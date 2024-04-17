#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>

// 9x9 스도쿠 배열
int sudoku[9][9] = {{6,3,9,8,4,1,2,7,5},{7,2,4,9,5,3,1,6,8},{1,8,5,7,2,6,3,9,4},{2,5,6,1,3,7,4,8,9},{4,9,1,5,8,2,6,3,7},{8,7,3,4,6,9,5,2,1},{5,4,2,3,9,8,7,1,6},{3,1,8,6,7,5,9,4,2},{9,6,7,2,1,4,8,5,3}};

// 행, 열, 그리드(3x3 부분 격자)의 검증 결과를 저장하는 배열
bool valid[3][9];

// 행을 검증하는 함수
void *check_rows(void *arg)
{
    int row, num, found;
    for (row = 0; row < 9; row++) {
        bool nums[10] = {false}; 
        valid[0][row] = true; // 기본적으로 행은 true로 설정
        for (num = 0; num < 9; num++) {
            found = sudoku[row][num];
            if (found < 1 || found > 9 || nums[found]) {
                valid[0][row] = false; // 오류 발생 시 해당 행만 false로 설정
                break;
            }
            nums[found] = true;
        }
        
    }
    return NULL;
}

// 열을 검증하는 함수
void *check_columns(void *arg)
{
    int col, num, found;
    for (col = 0; col < 9; col++) {
        bool nums[10] = {false};
        valid[1][col] = true; // 기본적으로 열은 true로 설정	
        for (num = 0; num < 9; num++) {
            found = sudoku[num][col];
            if (found < 1 || found > 9 || nums[found]) {
                valid[1][col] = false; // 오류 발생 시 해당 열만 false로 설정
                break;
            }
            nums[found] = true;
        }
    }
    return NULL; // 모든 검증이 완료된 후 스레드 종료
}

// 부분 그리드(3x3 부분 격자)를 검증하는 함수
void *check_subgrid(void *arg)
{
    int start_row = (*(int *)arg / 3) * 3;
    int start_col = (*(int *)arg % 3) * 3;
    int row, col, found;
    bool nums[10] = {false};

    for (row = start_row; row < start_row + 3; row++) {
        for (col = start_col; col < start_col + 3; col++) {
            found = sudoku[row][col];
            if (nums[found] || found < 1 || found > 9) {
                valid[2][*(int *)arg] = false;
                return NULL;
            }
            nums[found] = true;
        }
    }
    valid[2][*(int *)arg] = true;
    return NULL;
}

// 스도쿠를 검증하는 함수
void check_sudoku(void)
{
    int i, j;
    pthread_t threads[11];  
    int grid_index[9];      

    // 스도쿠 출력
    for (i = 0; i < 9; ++i) {
        for (j = 0; j < 9; ++j)
            printf("%2d", sudoku[i][j]);
        printf("\n");
    }
    printf("---\n");

    // 행, 열, 부분 그리드를 검증하는 스레드 생성
    pthread_create(&threads[0], NULL, check_rows, NULL);
    pthread_create(&threads[1], NULL, check_columns, NULL);
    for (i = 0; i < 9; i++) {
        grid_index[i] = i;
        pthread_create(&threads[2 + i], NULL, check_subgrid, (void *)&grid_index[i]);
    }

    // 모든 스레드 종료를 기다림
    for (i = 0; i < 11; i++) {
        pthread_join(threads[i], NULL);
    }

    // 검증 결과 출력
    printf("ROWS: ");
    for (i = 0; i < 9; ++i)
        printf(valid[0][i] ? "(%d,YES)" : "(%d,NO)", i);
    printf("\n");

    printf("COLS: ");
    for (i = 0; i < 9; ++i)
        printf(valid[1][i] ? "(%d,YES)" : "(%d,NO)", i);
    printf("\n");

    printf("GRID: ");
    for (i = 0; i < 9; ++i)
        printf(valid[2][i] ? "(%d,YES)" : "(%d,NO)", i);
    printf("\n---\n");
}

// 스레드 동작 여부를 나타내는 변수
bool alive = true;

// 스도쿠를 무작위로 섞는 함수
void *shuffle_sudoku(void *arg)
{
    int tmp;
    int grid;
    int row1, row2;
    int col1, col2;
    
    srand(time(NULL));
    while (alive) {
        grid = rand() % 9;
        row1 = row2 = (grid/3)*3;
        col1 = col2 = (grid%3)*3;
        row1 += rand() % 3; col1 += rand() % 3;
        row2 += rand() % 3; col2 += rand() % 3;
        tmp = sudoku[row1][col1];
        sudoku[row1][col1] = sudoku[row2][col2];
        sudoku[row2][col2] = tmp;
    }
    pthread_exit(NULL);
}

// 메인 함수
int main(void)
{
    int i, tmp;
    pthread_t tid;
    struct timespec req;
    
    // 기본 테스트 출력
    printf("********** BASIC TEST **********\n");
    
    // 스도쿠 검증
    check_sudoku();
    // 검증 오류가 있는지 확인 후 출력
    for (i = 0; i < 9; ++i)
        if (valid[0][i] == false || valid[1][i] == false || valid[2][i] == false) {
            printf("ERROR: 스도쿠 검증오류!\n");
            return 1;
        }

    // 스도쿠 일부를 변경하여 검증 오류 유발
    tmp = sudoku[0][0]; sudoku[0][0] = sudoku[1][1]; sudoku[1][1] = tmp;
    tmp = sudoku[5][3]; sudoku[5][3] = sudoku[4][5]; sudoku[4][5] = tmp;
    tmp = sudoku[7][7]; sudoku[7][7] = sudoku[8][8]; sudoku[8][8] = tmp;
    // 변경 후 스도쿠 재검증
    check_sudoku();
    // 검증 오류가 있는지 확인 후 출력
    for (i = 0; i < 9; ++i)
        if ((i == 2 || i == 3 || i == 6) && valid[0][i] == false) {
            printf("ERROR: 행 검증오류!\n");
            return 1;
        }
        else if ((i != 2 && i != 3 && i != 6) && valid[0][i] == true) {
            printf("ERROR: 행 검증오류!\n");
            return 1;
        }
    for (i = 0; i < 9; ++i)
        if ((i == 2 || i == 4 || i == 6) && valid[1][i] == false) {
            printf("ERROR: 열 검증오류!\n");
            return 1;
        }
        else if ((i != 2 && i != 4 && i != 6) && valid[1][i] == true) {
            printf("ERROR: 열 검증오류!\n");
            return 1;
        }
    for (i = 0; i < 9; ++i)
        if (valid[2][i] == false) {
            printf("ERROR: 부분격자 검증오류!\n");
            return 1;
        }

    // 무작위 테스트 출력
    printf("********** RANDOM TEST **********\n");

    // 스도쿠를 무작위로 섞는 스레드 생성
    tmp = sudoku[0][0]; sudoku[0][0] = sudoku[1][1]; sudoku[1][1] = tmp;
    tmp = sudoku[5][3]; sudoku[5][3] = sudoku[4][5]; sudoku[4][5] = tmp;
    tmp = sudoku[7][7]; sudoku[7][7] = sudoku[8][8]; sudoku[8][8] = tmp;
    if (pthread_create(&tid, NULL, shuffle_sudoku, NULL) != 0) {
        fprintf(stderr, "pthread_create error: shuffle_sudoku\n");
        return -1;
    }

    // 무작위 테스트를 위해 대기 후 검증 실행
    req.tv_sec = 0;
    req.tv_nsec = 1000;
    for (i = 0; i < 5; ++i) {
        check_sudoku();
        nanosleep(&req, NULL);
    }

    // 무작위 테스트 종료
    alive = 0;
    pthread_join(tid, NULL);

    // 스도쿠 재검증 후 빙고 결과 출력
    check_sudoku();
    for (i = 0; i < 9; ++i)
        if (valid[0][i] == true)
            printf("빙고! %d번 행이 맞았습니다.\n", i);
    for (i = 0; i < 9; ++i)
        if (valid[1][i] == true)
            printf("빙고! %d번 열이 맞았습니다.\n", i);
    for (i = 0; i < 9; ++i)
        if (valid[2][i] == false) {
            printf("ERROR: 부분격자 검증오류!\n");
            return 1;
        }
    
    return 0;
}
