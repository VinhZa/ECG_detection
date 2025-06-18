#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/mman.h>
#include "./FPGA_Driver.c"

#define MAX_SIZE 10000

#define RESET_BASE       0x0000000004
#define NUM_BEAT_BASE    0x0000000080
#define SIGNAL_BASE      0x0000000084 
#define RR_BASE          0x0008000000 
#define SYMBOL_BASE      0x0008800000 
#define STATE_BASE       0x000A000004

#define SIGNALS_PER_BEAT 100
#define MAPPING_SIZE     0x01000000  // 16MB là ví dụ

// Biến toàn cục để cleanup
unsigned char* membase = NULL;
int fpga_fd = -1;
uint32_t* reg_reset = NULL;

void cleanup() {
    printf("\n[INFO] Đang dọn dẹp...\n");

    if (reg_reset) {
        reg_reset[0] = 0;
        dma_write(RESET_BASE, 1);
        printf("[CLEANUP] Đã reset FPGA.\n");
    }

    if (membase) {
        munmap(membase, MAPPING_SIZE);
        membase = NULL;
        printf("[CLEANUP] Đã unmap FPGA memory.\n");
    }

    if (fpga_fd != -1) {
        close(fpga_fd);
        fpga_fd = -1;
        printf("[CLEANUP] Đã đóng thiết bị FPGA.\n");
    }
}

void signal_handler(int sig) {
    printf("\n[INFO] Nhận tín hiệu %d (Ctrl+C?), thoát an toàn...\n", sig);
    cleanup();
    exit(1);
}

int main() {
    // Đăng ký xử lý tín hiệu
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    atexit(cleanup);

    uint32_t rr[MAX_SIZE], symbol[MAX_SIZE];
    int32_t signal[MAX_SIZE * SIGNALS_PER_BEAT];
    int num_beat, record;

    printf("Nhập num_beat: ");
    scanf("%d", &num_beat);
    if (num_beat <= 0 || num_beat > MAX_SIZE) {
        printf("num_beat không hợp lệ\n");
        return -1;
    }

    printf("Nhập record number (100–200): ");
    scanf("%d", &record);
    if (record < 100 || record > 200) {
        printf("Record phải trong khoảng 100–200\n");
        return -1;
    }

    char file_signal[100], file_rr[100], file_symbol[100];
    sprintf(file_signal, "data_ListSignal_%d.txt", record);
    sprintf(file_rr, "data_ListRRinternal_%d.txt", record);
    sprintf(file_symbol, "data_symbol_%d.txt", record);

    FILE *f_signal = fopen(file_signal, "r");
    FILE *f_rr = fopen(file_rr, "r");
    FILE *f_symbol = fopen(file_symbol, "r");

    if (!f_signal || !f_rr || !f_symbol) {
        perror("Lỗi mở file input");
        return -1;
    }

    for (int i = 0; i < num_beat * 100; i++) {
        float temp_signal;
        fscanf(f_signal, "%f", &temp_signal);
        signal[i] = (int32_t)(temp_signal * 65536);  
    }

    for (int i = 0; i < num_beat; i++) {
        uint32_t temp_rr;
        fscanf(f_rr, "%d", &temp_rr);
        rr[i] = (uint32_t)(temp_rr * 256);
        fscanf(f_symbol, "%d", &symbol[i]);
    }

    fclose(f_signal);
    fclose(f_rr);
    fclose(f_symbol);

    // Mở thiết bị và ánh xạ FPGA
    fpga_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fpga_fd < 0) {
        perror("Không mở được /dev/mem");
        return -1;
    }

    membase = mmap(NULL, MAPPING_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fpga_fd, 0x00000000);
    if (membase == MAP_FAILED) {
        perror("mmap thất bại");
        return -1;
    }

    reg_reset    = (uint32_t*)(membase + RESET_BASE);
    uint32_t* reg_signal   = (uint32_t*)(membase + SIGNAL_BASE);
    uint32_t* reg_rr       = (uint32_t*)(membase + RR_BASE);
    uint32_t* reg_symbol   = (uint32_t*)(membase + SYMBOL_BASE);
    uint32_t* reg_numbeat  = (uint32_t*)(membase + NUM_BEAT_BASE);
    uint32_t* state        = (uint32_t*)(membase + STATE_BASE);

    // Ghi dữ liệu vào FPGA
    for (int i = 0; i < num_beat * 100; i++) {
        reg_signal[i] = signal[i];
    }

    for (int i = 0; i < num_beat; i++) {
        reg_rr[i] = rr[i];
        reg_symbol[i] = symbol[i];
    }

    reg_reset[0] = 0;
    dma_write(RESET_BASE, 1);

    reg_numbeat[0] = num_beat;
    dma_write(NUM_BEAT_BASE, 1);
    dma_write(SIGNAL_BASE, 100);
    dma_write(RR_BASE, 1);

    int N = 1;
    while (N <= num_beat) {
        while (*state != 1) {
            dma_read(STATE_BASE, 1);
        }

        reg_rr[N] = rr[N];
        dma_write(RR_BASE + N * 4, 1);

        while (*state != 2 && *state != 3) {
            dma_read(STATE_BASE, 1);
        }

        dma_write(SIGNAL_BASE + N * 100 * 4, 100);
        N++;
    }

    printf("Hoàn tất cập nhật và tối ưu mẫu\n");

    while (*state != 5) {
        dma_read(STATE_BASE, 1);
    }

    dma_write(RR_BASE, num_beat);
    dma_write(SYMBOL_BASE, num_beat);
    printf("Hoàn tất nạp RR và symbol\n");

    return 0;
}
