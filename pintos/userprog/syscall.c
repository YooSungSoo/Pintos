#include "userprog/syscall.h"  // 이 파일의 함수 원형 선언 포함
#include <stdio.h>			   // printf() 사용 (exit 시 메시지 출력용)
#include <syscall-nr.h>		   // 시스템 콜 번호 정의 (예: SYS_HALT, SYS_EXIT 등)
#include "threads/interrupt.h" // intr_frame 구조체 정의
#include "threads/thread.h"	   // thread_current(), thread_exit(), tid_t 등
#include "threads/loader.h"	   // load(), ELF 로더 관련 함수
#include "userprog/gdt.h"	   // GDT(Global Descriptor Table) 관련
#include "threads/flags.h"	   // EFLAGS 레지스터 플래그 상수 (FLAG_IF 등)
#include "intrinsic.h"		   // write_msr(), read_msr() 등 저수준 함수 제공

#include "threads/vaddr.h"	 // is_kernel_vaddr(), pg_round_down(), 가상주소 유틸리티
#include "threads/palloc.h"	 // palloc_get_page(), PAL_ZERO (페이지 할당)
#include "filesys/filesys.h" // filesys_create(), filesys_remove(), filesys_open()
#include "filesys/file.h"	 // file_read(), file_write(), file_seek(), file_length(), file_close()
#include "devices/input.h"	 // input_getc() → stdin 입력 처리
#include <string.h>			 // strlen(), memcpy(), memset()

#include "threads/mmu.h"	  // pml4_get_page() (주소 매핑 확인용)
#include "userprog/process.h" // process_exec(), process_wait(), process_fork() 등

// syscall_entry() : 어셈블리 수준에서 SYSCALL 진입 시 실행되는 함수
// syscall_handler(): 커널 진입 후 시스템 콜 번호를 해석하고 처리하는 핸들러
void syscall_entry(void);
void syscall_handler(struct intr_frame *);

// 파일 시스템 동시 접근 방지를 위한 전역 락
struct lock filesys_lock;

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081			/* 세그먼트 셀렉터 저장 MSR */
#define MSR_LSTAR 0xc0000082		/* SYSCALL → 진입할 커널 함수 주소 저장 */
#define MSR_SYSCALL_MASK 0xc0000084 /* SYSCALL 시 EFLAGS에서 꺼야 할 플래그 */

void syscall_init(void)
{
	// 유저 세그먼트/커널 세그먼트 지정
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48 |
							((uint64_t)SEL_KCSEG) << 32);

	// SYSCALL 명령어 진입 시점 → syscall_entry() 함수로 점프
	write_msr(MSR_LSTAR, (uint64_t)syscall_entry);

	// 커널 진입 직후 EFLAGS에서 꺼야 할 플래그 지정
	write_msr(MSR_SYSCALL_MASK,
			  FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	// 파일 시스템 동기화를 위한 락 초기화
	lock_init(&filesys_lock);
}

/** project2-System Call */
void check_address(void *addr)
{
	// 잘못된 주소 접근일 경우 프로세스를 강제 종료(exit(-1))
	//조건 1 : NULL 포인터 → 잘못된 접근
	//조건 2 : 커널 가상 메모리 주소 → 커널 보호
	//조건 3 : 현재 프로세스의 page table(pml4) 에 매핑되지 않은 주소 
	if (is_kernel_vaddr(addr) || addr == NULL ||
		pml4_get_page(thread_current()->pml4, addr) == NULL)
		exit(-1);
}

/* The main system call interface */
void syscall_handler(struct intr_frame *f UNUSED)
{
	// 시스템 콜 번호는 유저가 rax 레지스터에 넣고 SYSCALL 호출
	int sys_number = f->R.rax;

	// 인자 순서: rdi, rsi, rdx, r10, r8, r9
	switch (sys_number)
	{
	case SYS_HALT:
		halt();
		break;
	case SYS_EXIT:
		exit(f->R.rdi); // 첫 번째 인자 → 종료 상태
		break;
	case SYS_FORK:
		f->R.rax = fork(f->R.rdi); // 반환값을 rax에 저장
		break;
	case SYS_EXEC:
		f->R.rax = exec(f->R.rdi);
		break;
	case SYS_WAIT:
		f->R.rax = process_wait(f->R.rdi);
		break;
	case SYS_CREATE:
		f->R.rax = create(f->R.rdi, f->R.rsi);
		break;
	case SYS_REMOVE:
		f->R.rax = remove(f->R.rdi);
		break;
	case SYS_OPEN:
		f->R.rax = open(f->R.rdi);
		break;
	case SYS_FILESIZE:
		f->R.rax = filesize(f->R.rdi);
		break;
	case SYS_READ:
		f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_WRITE:
		f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
		break;
	case SYS_SEEK:
		seek(f->R.rdi, f->R.rsi);
		break;
	case SYS_TELL:
		f->R.rax = tell(f->R.rdi);
		break;
	case SYS_CLOSE:
		close(f->R.rdi);
		break;
	default:
		exit(-1); // 알 수 없는 syscall → 프로세스 종료
	}
}

bool remove(const char *file)
{
	check_address(file); // 포인터 유효성 검사

	lock_acquire(&filesys_lock);		 // 동기화 시작
	bool success = filesys_remove(file); // 파일 삭제
	lock_release(&filesys_lock);		 // 동기화 해제

	return success;
}

bool create(const char *file, unsigned initial_size)
{
	check_address(file);

	lock_acquire(&filesys_lock);
	bool success = filesys_create(file, initial_size);
	lock_release(&filesys_lock);

	return success;
}

void exit(int status)
{
	struct thread *t = thread_current();
	t->exit_status = status;				   // 부모에게 반환할 종료 코드 저장
	printf("%s: exit(%d)\n", t->name, status); // 로그 출력
	thread_exit();							   // 스레드 종료
}

void halt(void)
{
	power_off(); // Pintos 자체 종료
}

int open(const char *file)
{
	check_address(file);

	lock_acquire(&filesys_lock);
	struct file *newfile = filesys_open(file);

	if (newfile == NULL) // 파일 없음
		goto err;

	int fd = process_add_file(newfile); // 현재 프로세스 FDT에 등록
	if (fd == -1)
		file_close(newfile); // 실패 시 닫기

	lock_release(&filesys_lock);
	return fd;

err:
	lock_release(&filesys_lock);
	return -1;
}

int filesize(int fd)
{
	struct file *file = process_get_file(fd);

	if (file == NULL)
		return -1;

	return file_length(file);
}

int read(int fd, void *buffer, unsigned length)
{
	check_address(buffer); // 유저 포인터 검증

	if (fd == 0)
	{ // stdin → 키보드 입력
		unsigned char *buf = buffer;
		for (unsigned i = 0; i < length; i++)
		{
			buf[i] = input_getc();
		}
		return length;
	}
	// 그 외의 경우
	if (fd < 3) // stdout, stderr를 읽으려고 할 경우 & fd가 음수일 경우
		return -1;

	struct file *file = process_get_file(fd);
	off_t bytes = -1;

	if (file == NULL) // 파일이 비어있을 경우
		return -1;

	lock_acquire(&filesys_lock);
	bytes = file_read(file, buffer, length);
	lock_release(&filesys_lock);

	return bytes;
}

int write(int fd, const void *buffer, unsigned length)
{
	check_address(buffer);

	off_t bytes = -1;

	if (fd <= 0) // stdin에 쓰려고 할 경우 & fd 음수일 경우
		return -1;

	if (fd < 3)
	{ // 1(stdout) * 2(stderr) -> console로 출력
		putbuf(buffer, length);
		return length;
	}

	struct file *file = process_get_file(fd);

	if (file == NULL)
		return -1;

	lock_acquire(&filesys_lock);
	bytes = file_write(file, buffer, length);
	lock_release(&filesys_lock);

	return bytes;
}

void seek(int fd, unsigned position)
{

	struct file *file = process_get_file(fd);

	if (file == NULL || (file >= STDIN && file <= STDERR))
		return;

	file_seek(file, position);
}

int tell(int fd)
{
	struct file *file = process_get_file(fd);

	if (file == NULL || (file >= STDIN && file <= STDERR))
		return -1;

	return file_tell(file);
}

void close(int fd)
{
	struct file *file = process_get_file(fd);

	if (fd < 3 || file == NULL)
		return;

	process_close_file(fd);

	file_close(file);
}

int exec(const char *cmd_line)
{
	check_address(cmd_line);

	off_t size = strlen(cmd_line) + 1;
	char *cmd_copy = palloc_get_page(PAL_ZERO);

	if (cmd_copy == NULL)
		return -1;

	memcpy(cmd_copy, cmd_line, size);

	if (process_exec(cmd_copy) == -1)
		return -1;

	return 0; // process_exec 성공시 리턴 값 없음 (do_iret)
}

int wait(tid_t tid)
{
	return process_wait(tid);
}

tid_t fork(const char *thread_name)
{
	check_address(thread_name);

	return process_fork(thread_name, NULL);
}
