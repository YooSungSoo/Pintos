#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* ---- 프로세스 생성 및 실행 ---- */
tid_t process_create_initd(const char *file_name);
// initd 프로세스를 생성하여 실행
// file_name 실행 파일을 로드하고 새로운 스레드 생성
// 실패 시 TID_ERROR 반환

tid_t process_fork(const char *name, struct intr_frame *if_);
// 현재 프로세스를 복제(fork)
// 자식은 부모의 메모리 공간(pml4, fdt 등)을 그대로 복제하여 실행
// 성공 시 자식 tid 반환, 실패 시 TID_ERROR

int process_exec(void *f_name);
// 현재 프로세스를 새 실행 파일(f_name)로 교체 (exec 시스템 콜)
// 성공 시 반환하지 않음(do_iret으로 새 프로그램 실행), 실패 시 -1 반환

int process_wait(tid_t tid);
// 자식 프로세스 tid의 종료를 기다림
// 종료 시 exit_status 반환, 잘못된 tid거나 이미 wait한 경우 -1 반환

void process_exit(void);
// 현재 프로세스 종료 처리 (열린 파일 닫기, FDT 해제, 부모 세마포어 signal 등)

void process_activate(struct thread *next);
// 새 스레드(next)의 주소 공간(pml4) 활성화 및 TSS 갱신
// 스케줄링 시 호출됨

/** ---- Project2: Command Line Parsing ---- */
void argument_stack(char **argv, int argc, struct intr_frame *if_);
// 프로그램 실행 시 전달된 인자들을 유저 스택에 올려주는 함수
// argc, argv 형태로 정렬 후 intr_frame 레지스터에 설정

/** ---- Project2: System Call 관련 ---- */
struct thread *get_child_process(int pid);
// 현재 프로세스의 자식 리스트(child_list)에서 pid와 일치하는 자식 thread 반환
// 없으면 NULL 반환

int process_add_file(struct file *f);
// 현재 프로세스의 FDT(File Descriptor Table)에 파일 f 추가
// fd 번호 반환, 실패 시 -1

struct file *process_get_file(int fd);
// 현재 프로세스의 FDT에서 fd 번호에 해당하는 파일 구조체 반환
// 없으면 NULL

int process_close_file(int fd);
// 현재 프로세스의 FDT에서 fd 슬롯을 닫음(NULL로 설정)
// 성공 시 0, 실패 시 -1 반환

#endif /* userprog/process.h */
