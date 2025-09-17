#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#define STDIN 0
#define STDOUT 1
#define STDERR 2

/* ---- 시스템 콜 초기화 ---- */
void syscall_init(void);
// 시스템 콜 인터럽트 핸들러 등록 및 관련 자료구조 초기화

/** project2-System Call */
#include <stdbool.h>
#include "threads/thread.h"    /* tid_t 정의 */
#include "threads/interrupt.h" /* struct intr_frame 정의 */
#include "userprog/process.h"

/* ---- 유저 메모리 접근 검증 ---- */
void check_address(void *addr);
// 전달된 유저 주소(addr)가 유효한지 검사
// 커널 주소(NULL, 커널 영역, 매핑 안 된 페이지)일 경우 프로세스 종료(exit(-1))

/* ---- 시스템 콜 구현부 ---- */
void halt(void);
// Pintos OS 종료 (power_off)

void exit(int status);
// 현재 프로세스 종료, 상태(status)를 부모 프로세스에 반환
// 종료 메시지 출력 (프로젝트 요구사항)

bool create(const char *file, unsigned initial_size);
// 새로운 파일 생성
// 성공 시 true, 실패 시 false

bool remove(const char *file);
// 파일 삭제
// 성공 시 true, 실패 시 false

int open(const char *file);
// 파일 열기, FDT(File Descriptor Table)에 등록 후 fd 반환
// 실패 시 -1 반환

int filesize(int fd);
// fd에 해당하는 파일 크기(byte 단위) 반환
// 실패 시 -1 반환

int read(int fd, void *buffer, unsigned length);
// 파일 또는 표준입력(stdin)으로부터 length 만큼 데이터를 읽어 buffer에 저장
// 성공 시 읽은 바이트 수 반환, 실패 시 -1 반환

int write(int fd, const void *buffer, unsigned length);
// 파일 또는 표준출력(stdout)으로 length 만큼 데이터를 출력
// 성공 시 출력한 바이트 수 반환, 실패 시 -1 반환

void seek(int fd, unsigned position);
// 파일의 현재 위치(offset)를 position으로 이동

int tell(int fd);
// 파일의 현재 위치(offset) 반환
// 실패 시 -1 반환

void close(int fd);
// fd에 해당하는 파일 닫기, FDT에서 제거

tid_t fork(const char *thread_name);
int exec(const char *cmd_line);
int wait(tid_t tid);

#endif /* userprog/syscall.h */