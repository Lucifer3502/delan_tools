1. 根据编译环境选择是否打开define LINUX_OS 这个宏定义。如果选择在win下编译，则注释掉这个宏。

2. linux编译cmd： gcc esp32_ota_head.c -o esp32_ota_head -l crypto

3.  打包头脚本示例： ./esp32_ota_head $MODULE $HW_VER $SW_VER $UTIME $USER_OFFSET $USER_BIN $OTA_BIN $PATCH_NONE_FILE
参数列表分别表示：型号，硬件版本，软件版本，打包时间，程序偏移量，程序文件名称，生成的新文件名，配置文件名。