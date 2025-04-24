#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "./FPGA_Driver.c"

#define MAX_SIZE 10000


#define NUM_BEAT_BASE    0x0000000000 


#define SIGNAL_BASE      0x0000000004 

#define RR_BASE          0x0008000000 

#define SYMBOL_BASE      0x0008800000 

#define CT_BASE          0x0008880000

#define STATE_BASE       0x000A000000

#define NORMAL_BASE      0x000A000004


#define ABNORMAL_BASE    0x000A000008

int main() {
    float signal_f[MAX_SIZE * 100], rr_f[MAX_SIZE];
    uint32_t symbol[MAX_SIZE];
    float CT[MAX_SIZE * 100];
    int num_beat, record;
    uint32_t signal[MAX_SIZE * 100];
    uint32_t rr[MAX_SIZE];
    
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
        fscanf(f_signal, "%f", &signal_f[i]);
        signal[i] = (uint32_t)(signal_f[i] * 65536.0f);
    }
    for (int i = 0; i < num_beat; i++) {
        fscanf(f_rr, "%f", &rr_f[i]);
        rr[i] = (uint32_t)(rr_f[i] * 65536.0f);
        fscanf(f_symbol, "%u", &symbol[i]);
    }

    printf("\n== In ra 10 giá trị đầu tiên của rr (gốc float) ==\n");
    for (int i = 0; i < 10 && i < num_beat; i++) {
        printf("rr[%d] = %f (int fixed = %u)\n", i, rr_f[i], rr[i]);
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
    uint32_t* reg_state    = (uint32_t*)(membase + STATE_BASE);
    uint32_t* reg_normal   = (uint32_t*)(membase + NORMAL_BASE);
    uint32_t* reg_abnormal = (uint32_t*)(membase + ABNORMAL_BASE);

    // Ghi dữ liệu vào FPGA
    *reg_numbeat = num_beat;
    dma_write(NUM_BEAT_BASE, 1);

    memcpy(reg_signal, signal, sizeof(uint32_t) * num_beat * 100);
    dma_write(SIGNAL_BASE, num_beat * 100);

    memcpy(reg_rr, rr, sizeof(uint32_t) * num_beat);
    dma_write(RR_BASE, num_beat);

    if (*reg_state == 11) {
        memcpy(reg_symbol, symbol, sizeof(uint32_t) * num_beat);
        
        dma_write(SYMBOL_BASE, num_beat);
    }

    // Bắt đầu chạy IP
    printf("Bắt đầu xử lý FPGA...\n");
    while (*reg_state == 0);
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
        fprintf(f_out, "%u\n", CT[i]);  // nếu muốn ghi lại thành float thì chia CT[i] / 65536.0f
    }
    fclose(f_out);
    printf("Đã ghi %d CT vào %s\n", num_beat * 100, file_output);

    printf("Normal   = %u\n", *reg_normal & 0xFFF);
    printf("Abnormal = %u\n", *reg_abnormal & 0xFFF);

    return 0;
}
