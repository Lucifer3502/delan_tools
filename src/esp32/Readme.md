1. 根据编译环境选择是否打开define LINUX_OS 这个宏定义。如果选择在win下编译，则注释掉这个宏。

2. linux编译cmd： gcc ota_addhead_linux.c -lcrypto -o plug_ota_head 。

3.  打包头脚本示例： ./plug_ota_head DL3043A_SMCD  1.0.0  128  DL3043A_SMCD_OTA_V1.0.0_OLD.bin   DL3043A_SMCD_OTA_V1.0.0_NEW.bin
第一个参数表示硬件型号，第二个参数表示软件版本，第三个表示程序偏移量，第四个是打包源文件，第五个是打包完成的文件。