#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "./FPGA_Driver.c"

#define MAX_SIZE 10000
#define PADDING_BASE     0x01000000

#define NUM_BEAT_BASE    0x0000000000 + PADDING_BASE
#define SIGNAL_BASE      0x0000000004 + PADDING_BASE
#define RR_BASE          0x0000008000 + PADDING_BASE
#define SYMBOL_BASE      0x0000008880 + PADDING_BASE
#define CT_BASE          0x0000008888 + PADDING_BASE

#define START_BASE       0x0000020004
#define DONE_BASE        0x0000020000
#define STATE_BASE       0x0000009008 + PADDING_BASE
#define NORMAL_BASE      0x0000009000 + PADDING_BASE
#define ABNORMAL_BASE    0x0000009004 + PADDING_BASE

int main() {
    uint32_t signal[MAX_SIZE * 100], rr[MAX_SIZE], symbol[MAX_SIZE];
    uint32_t CT[MAX_SIZE * 100];
    int num_beat, record;

    // Nhập thông tin
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

    // Tạo tên file input theo record
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
        fscanf(f_signal, "%x", &signal[i]);
    }
    for (int i = 0; i < num_beat; i++) {
        fscanf(f_rr, "%x", &rr[i]);
        fscanf(f_symbol, "%x", &symbol[i]);
    }

    fclose(f_signal);
    fclose(f_rr);
    fclose(f_symbol);

    // Mở kết nối FPGA
    if (fpga_open() == 0) return -1;
    fpga.dma_ctrl = CGRA_info.dma_mmap;
    unsigned char* membase = (unsigned char*)CGRA_info.ddr_mmap;

    uint32_t* reg_numbeat  = (uint32_t*)(membase + NUM_BEAT_BASE);
    uint32_t* reg_signal   = (uint32_t*)(membase + SIGNAL_BASE);
    uint32_t* reg_rr       = (uint32_t*)(membase + RR_BASE);
    uint32_t* reg_symbol   = (uint32_t*)(membase + SYMBOL_BASE);
    uint32_t* reg_ct       = (uint32_t*)(membase + CT_BASE);
    uint32_t* reg_start    = (uint32_t*)(CGRA_info.pio_32_mmap + START_BASE);
    uint32_t* reg_done     = (uint32_t*)(CGRA_info.pio_32_mmap + DONE_BASE);
    uint32_t* reg_state    = (uint32_t*)(CGRA_info.pio_32_mmap + STATE_BASE);
    uint32_t* reg_normal   = (uint32_t*)(CGRA_info.pio_32_mmap + NORMAL_BASE);
    uint32_t* reg_abnormal = (uint32_t*)(CGRA_info.pio_32_mmap + ABNORMAL_BASE);

    // Ghi dữ liệu vào FPGA
    *reg_numbeat = num_beat;
    dma_write(NUM_BEAT_BASE, 1);

    for (int i = 0; i < num_beat * 100; i++) {
        reg_signal[i] = signal[i];
    }
    dma_write(SIGNAL_BASE, num_beat * 100);

    for (int i = 0; i < num_beat; i++) {
        reg_rr[i] = rr[i];
        reg_symbol[i] = symbol[i];
    }
    dma_write(RR_BASE, num_beat);
    dma_write(SYMBOL_BASE, num_beat);

    // Bắt đầu chạy IP
    printf(" Bắt đầu xử lý FPGA...\n");
    *reg_start = 1;

    while (*reg_done == 0);
    printf("FPGA xử lý xong!\n");

    // Đọc CT
    dma_read(CT_BASE, num_beat * 100);
    for (int i = 0; i < num_beat * 100; i++) {
        CT[i] = reg_ct[i];
    }

    // Ghi CT ra file
    char file_output[100];
    sprintf(file_output, "Output_ROM_%d.txt", record);
    FILE *f_out = fopen(file_output, "w");
    if (!f_out) {
        perror("Lỗi mở file output");
        return -1;
    }

    for (int i = 0; i < num_beat * 100; i++) {
        fprintf(f_out, "%08X\n", CT[i]);
    }
    fclose(f_out);
    printf("Đã ghi %d CT vào %s\n", num_beat * 100, file_output);

    uint32_t normal = *reg_normal & 0xFFF;
    uint32_t abnormal = *reg_abnormal & 0xFFF;

    printf(" Normal   = %u\n", normal);
    printf("Abnormal = %u\n", abnormal);

    return 0;
}
