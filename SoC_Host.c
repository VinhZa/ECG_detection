#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h> // for usleep
#include "./FPGA_Driver.c"

#define MAX_SIZE 10000


#define NUM_BEAT_BASE    0x0000000004
#define START_BASE       0x0000000008
#define SIGNAL_BASE      0x0000000020 
#define RR_BASE          0x0008000000 
#define SYMBOL_BASE      0x0008800000 
#define STATE_BASE       0x000A000004
#define DONE_BASE        0x000A000048

#define SIGNALS_PER_BEAT 100

int main() {
    uint32_t rr[MAX_SIZE], symbol[MAX_SIZE];
    int32_t signal[MAX_SIZE * SIGNALS_PER_BEAT];
    int num_beat, record;

    // Nhập dữ liệu
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

    // Tên file
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

    // Đọc dữ liệu
    for (int i = 0; i < num_beat * 100; i++) {
        float temp_signal;
        fscanf(f_signal, "%f", &temp_signal);
        signal[i] = (int32_t)(temp_signal * 65536);  
    }

    for (int i = 0; i < num_beat; i++) {
        uint32_t temp_rr;
        fscanf(f_rr, "%d", &temp_rr);
        rr[i] = (uint32_t)(temp_rr *256);
        fscanf(f_symbol, "%d", &symbol[i]);
    }

    fclose(f_signal);
    fclose(f_rr);
    fclose(f_symbol);

    unsigned char* membase;
    if (fpga_open() == 0)
        exit(1);

    fpga.dma_ctrl = CGRA_info.dma_mmap;
    membase = (unsigned char*)CGRA_info.ddr_mmap;

    uint32_t* reg_signal   = (uint32_t*)(membase + SIGNAL_BASE);
    uint32_t* reg_rr       = (uint32_t*)(membase + RR_BASE);
    uint32_t* reg_symbol   = (uint32_t*)(membase + SYMBOL_BASE);
    uint32_t* reg_numbeat  = (uint32_t*)(membase + NUM_BEAT_BASE);
    uint32_t* state        = (uint32_t*)(membase + STATE_BASE);
    uint32_t* reg_start     = (uint32_t*)(membase + START_BASE);
 
    // Ghi 100 giá trị đầu tiên vào DDR
    for (int i = 0; i < 100; i++) {
        reg_signal[i] = signal[i];
    }
    
    for (int i = 0; i < num_beat; i++) {
        reg_rr[i] = rr[i];
        reg_symbol[i] = symbol[i];
    }

        
    uintptr_t offset = 0;
    int32_t M = 1;
    uint32_t rr_mod[MAX_SIZE * 2];
    int rr_mod_len = 0;
    
    // Chèn 0 sau mỗi 3 phần tử và dời phần tử thứ 4 về sau
    for (int i = 0; i < num_beat; i++) {
        if ((i % 4) != 3) {
            rr_mod[rr_mod_len++] = rr[i];
        } else {
            rr_mod[rr_mod_len++] = 0;       // Chèn 0 vào vị trí mới
            rr_mod[rr_mod_len++] = rr[i];   // Dời phần tử thứ 4 về sau
        }
    }
    
    reg_start[0] = 0;
    // Ghi num_beat
    reg_numbeat[0] = num_beat;
    dma_write(NUM_BEAT_BASE, 1);
    dma_write(START_BASE, 1);
    
    dma_write(SIGNAL_BASE, 100);
    dma_write(RR_BASE, 1);  
    
for (int N = 1; N <= num_beat; N++) {
    printf("=== Bắt đầu truyền cụm %d ===\n", N);
    
    offset = N*4;
    
    if ((offset & 0xF) == 0xC) {
    offset += 4;
    M ++;
    }
    // Đợi trạng thái == 1
    do {
        dma_read(STATE_BASE, 1);
        printf("[DMA READ] Địa chỉ: 0x%08X | Giá trị đọc được: %d\n", STATE_BASE, *state);
    } while (*state != 1);

    // Ghi RR cụm N
    reg_rr[N] = 0;
    reg_rr[M] = rr_mod[M];
    printf("[GHI RR] reg_rr[%d] (addr = 0x%08lX) = %u\n", M, (uintptr_t)&reg_rr[M] - (uintptr_t)membase, rr_mod[M]);
    dma_write(RR_BASE + offset, 1);

    // Ghi SIGNAL cụm N
    for (int i = 0; i < 100; i++) {
        reg_signal[N * 100 + i] = signal[N * 100 + i];
    }
    printf("[GHI SIGNAL] reg_signal[%d~%d] (addr = 0x%08lX ~ 0x%08lX)\n", 
        N * 100, N * 100 + 99,
        (uintptr_t)&reg_signal[N * 100] - (uintptr_t)membase,
        (uintptr_t)&reg_signal[N * 100 + 99] - (uintptr_t)membase);

    // Đợi trạng thái == 2 hoặc 3
    do {
        dma_read(STATE_BASE, 1);
        printf("[DMA READ] Địa chỉ: 0x%08X | Giá trị đọc được: %d\n", STATE_BASE, *state);
    } while (*state != 2 && *state != 3);

    // DMA write signal block
    dma_write(SIGNAL_BASE + N * SIGNALS_PER_BEAT * 4, 100);
    printf("=== Hoàn tất cụm %d ===\n\n", N);
    M++;
}


printf("endhere\n");

        
    return 0;
}
