#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H
#define USERPROG

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/interrupt.h"
#include "threads/synch.h" /** project2-System Call */
#ifdef VM
#include "vm/vm.h"
#endif

#define NICE_DEFAULT 0
#define RECENT_CPU_DEFAULT 0
#define LOAD_AVG_DEFAULT 0

/** project2-System Call */
#define FDT_PAGES 3						   // File Descriptor Table을 저장할 페이지 수 (multi-oom 테스트용)
#define FDCOUNT_LIMIT FDT_PAGES * (1 << 9) // 최대 파일 디스크립터 개수 (한 페이지 4KB / 포인터 8바이트 = 512개)

/* ---- 스레드 상태(thread status) ---- */
enum thread_status
{
	THREAD_RUNNING, /* CPU에서 실행 중인 상태 */
	THREAD_READY,	/* 실행 준비 완료 상태 (레디 큐에 있음) */
	THREAD_BLOCKED, /* 이벤트를 기다리는 상태 (세마포어, 락 등) */
	THREAD_DYING	/* 종료 중인 상태 */
};

/* 스레드 식별자 타입 */
typedef int tid_t;
#define TID_ERROR ((tid_t) - 1) /* 실패 시 반환할 tid 값 */

/* ---- 스레드 우선순위 ---- */
#define PRI_MIN 0	   /* 최저 우선순위 */
#define PRI_DEFAULT 31 /* 기본 우선순위 */
#define PRI_MAX 63	   /* 최고 우선순위 */

/* ---- 스레드 구조체 ----
 * 각 스레드는 고유한 커널 스택 페이지(4KB)를 갖고 있으며,
 * struct thread는 그 페이지의 맨 아래에 저장된다.
 * 나머지 공간은 스택으로 사용된다.
 */
struct thread
{
	/* 기본 정보 */
	tid_t tid;				   /* 스레드 ID */
	enum thread_status status; /* 스레드 상태 */
	char name[16];			   /* 스레드 이름 (디버깅용) */
	int priority;			   /* 현재 우선순위 */

	/* 스케줄링 및 동기화용 */
	struct list_elem elem; /* ready_list 또는 wait_list에서 사용되는 노드 */

	/* Alarm Clock 기능 (프로젝트 1) */
	int64_t ticks_awake;				   // 깨워야 하는 시각
	int priority_original;				   // 원래 우선순위 (priority donation 복구용)
	struct list lst_donation;			   // 나에게 우선순위를 기부한 스레드 목록
	struct list_elem lst_donation_elem;	   // 기부자 리스트에서 쓰일 노드
	struct lock *lock_donated_for_waiting; // 내가 기다리는 락 (donation 적용 대상)

	/* MLFQS 관련 */
	int nice;				   // nice 값
	int recent_cpu;			   // recent_cpu 값
	struct list_elem all_elem; // all_list에 들어갈 노드

#ifdef USERPROG
	/* 유저 프로세스 관련 (프로젝트 2) */
	uint64_t *pml4;			/* 페이지 테이블 포인터 (4레벨) */
	int exit_status;		/* 종료 코드 */
	int fd_idx;				/* 파일 디스크립터 인덱스 */
	struct file **fdt;		/* 파일 디스크립터 테이블 */
	struct file *runn_file; /* 현재 실행 중인 실행 파일 (write 금지용) */

	struct intr_frame parent_if; // 부모 프로세스의 intr_frame 복제본
	struct list child_list;		 // 자식 프로세스 리스트
	struct list_elem child_elem; // 부모의 child_list에서 사용되는 노드

	/* 프로세스 동기화 세마포어 */
	struct semaphore fork_sema; // fork 완료 대기용
	struct semaphore exit_sema; // 자식 프로세스 종료 대기용
	struct semaphore wait_sema; // 부모가 wait()할 때 사용
#endif

#ifdef VM
	/* 프로젝트 3: 가상 메모리 관련 */
	struct supplemental_page_table spt;
#endif

	/* 커널 내부용 */
	struct intr_frame tf; /* 스위칭 시 저장할 레지스터 값 */
	unsigned magic;		  /* 스택 오버플로우 감지용 매직 넘버 */
};

/* 스케줄러 정책 (기본: RR, 옵션: MLFQS) */
extern bool thread_mlfqs;

/* ---- 스레드 초기화 및 실행 ---- */
void thread_init(void);	 // 스레드 서브시스템 초기화
void thread_start(void); // 스케줄러 시작 및 idle 스레드 실행

/* ---- 타이머 및 통계 ---- */
void thread_tick(void);		   // 매 tick마다 호출 (스케줄링/통계 업데이트)
void thread_print_stats(void); // 스케줄링 통계 출력

/* ---- 스레드 생성 및 제어 ---- */
typedef void thread_func(void *aux);
tid_t thread_create(const char *name, int priority, thread_func *, void *); // 새로운 스레드 생성

void thread_block(void);			  // 현재 스레드를 BLOCKED 상태로 전환
void thread_unblock(struct thread *); // BLOCKED → READY 상태로 전환

struct thread *thread_current(void); // 현재 실행 중인 스레드 반환
tid_t thread_tid(void);				 // 현재 스레드의 tid 반환
const char *thread_name(void);		 // 현재 스레드 이름 반환

void thread_exit(void) NO_RETURN; // 현재 스레드 종료
void thread_yield(void);		  // CPU 양보 (READY 큐로 이동)

/* ---- Alarm Clock (프로젝트 1) ---- */
void thread_sleep(int64_t ticks);									 // 현재 스레드를 지정한 tick까지 재움
bool sort_thread_ticks(struct list_elem *a, struct list_elem *b);	 // wakeup tick 기준 정렬 비교
void thread_awake(int64_t ticks);									 // 깨어날 시간이 된 스레드 깨우기
bool sort_thread_priority(struct list_elem *a, struct list_elem *b); // 우선순위 기준 정렬
void thread_swap_prior(void);										 // 실행 중 스레드와 ready_list의 우선순위 비교 후 교체

/* ---- 우선순위 (프로젝트 1) ---- */
int thread_get_priority(void);				// 현재 스레드의 우선순위 반환
void thread_set_priority(int new_priority); // 현재 스레드 우선순위 변경

/* ---- MLFQS 스케줄러 (프로젝트 1) ---- */
int thread_get_nice(void);		 // 현재 스레드의 nice 값 반환
void thread_set_nice(int);		 // 현재 스레드의 nice 값 설정
int thread_get_recent_cpu(void); // 현재 스레드의 recent_cpu 반환
int thread_get_load_avg(void);	 // 시스템 load_avg 반환

/* ---- 컨텍스트 스위칭 ---- */
void do_iret(struct intr_frame *tf); // 유저 모드로 복귀 (iretq 실행)

/* ---- MLFQS 계산 함수 ---- */
void mlfqs_priority(struct thread *t);	 // priority 계산
void mlfqs_recent_cpu(struct thread *t); // recent_cpu 갱신
void mlfqs_load_avg(void);				 // load_avg 갱신
void mlfqs_update_recent_cpu(void);		 // 전체 스레드 recent_cpu 갱신
void mlfqs_update_priority(void);		 // 전체 스레드 priority 갱신

#endif /* threads/thread.h */
