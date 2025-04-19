rm -rf main data_ListSignal_100.txt data_ListRRinternal_100.txt data_symbol_100.txt Normal.txt Abnormal.txt
gcc SoC_Host.c -I. -o main
./main
