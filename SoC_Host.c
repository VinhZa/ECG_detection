#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "./FPGA_Driver.c"

#define MAX_SIZE 10000



#define START_BASE       0x0000000000
#define NUM_BEAT_BASE    0x0000000004 
#define SIGNAL_BASE      0x0000000008 
#define RR_BASE          0x0008000000 
#define SYMBOL_BASE      0x0008800000 
#define STATE_BASE       0x000A000000
#define NORMAL_BASE      0x000A000040
#define ABNORMAL_BASE    0x000A000044
#define DONE_BASE        0x000A000048

void write_output(uint32_t reg_signal[100]) {
    FILE *fileOut = fopen("Output_Signal.txt", "w");
    
    if (!fileOut) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }
    
    for (int i = 0; i < 100; i++) {
        fprintf(fileOut, "%d ", reg_signal[i]);
        fprintf(fileOut, "\n");
    }
    
    fclose(fileOut);
    printf("Output written to Output_ROM.txt\n");
}

int main() {
    uint32_t rr[MAX_SIZE], symbol[MAX_SIZE];
    int32_t signal[MAX_SIZE * 100];
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

    for (int i = 0; i < num_beat*100; i++) {
        float temp_signal;
        fscanf(f_signal, "%f", &temp_signal);
        signal[i] = (int32_t)(temp_signal * 65536);  
    }
    
    printf("100 giá trị đầu tiên của signal:\n");
    for (int i = 0; i < 100 && i < num_beat * 100; i++) {
        printf("signal[%d] = %d\n", i, signal[i]); 
    }
    
    for (int i = 0; i < num_beat; i++) {
        fscanf(f_rr, "%f", &rr[i]);
        fscanf(f_symbol, "%d", &symbol[i]);
    }

    fclose(f_signal);
    fclose(f_rr);
    fclose(f_symbol);

    // Mở kết nối FPGA
    if (fpga_open() == 0) return -1;
    fpga.dma_ctrl = CGRA_info.dma_mmap;
    unsigned char* membase = (unsigned char*)CGRA_info.ddr_mmap;

    uint32_t* reg_start    = (uint32_t*)(membase + START_BASE);
    uint32_t* reg_numbeat  = (uint32_t*)(membase + NUM_BEAT_BASE);
    uint32_t* reg_signal   = (uint32_t*)(membase + SIGNAL_BASE);
    uint32_t* reg_rr       = (uint32_t*)(membase + RR_BASE);
    uint32_t* reg_symbol   = (uint32_t*)(membase + SYMBOL_BASE);
    uint32_t* state        = (uint32_t*)(membase + STATE_BASE);
    uint32_t* normal        = (uint32_t*)(membase + NORMAL_BASE);

    // Ghi dữ liệu vào FPGA
    reg_start = 1;
    dma_write(START_BASE, 1);
    printf("%u\n", *state);

    reg_numbeat = num_beat;
    dma_write(NUM_BEAT_BASE, 1);
    
    
    for (int i = 0; i < 100; i++) {
        reg_signal[i] = signal[i];
    }
    dma_write(SIGNAL_BASE, 100);
    
    dma_read(SIGNAL_BASE, 100);
    write_output(reg_signal);    

    
    for (int i = 0; i < num_beat; i++) {
        reg_rr[i] = rr[i];
        reg_symbol[i] = symbol[i];
    }

    printf("da toi duoc day3");
    dma_write(RR_BASE, num_beat);
    dma_write(SYMBOL_BASE, num_beat);
}
