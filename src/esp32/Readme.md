1. ���ݱ��뻷��ѡ���Ƿ��define LINUX_OS ����궨�塣���ѡ����win�±��룬��ע�͵�����ꡣ

2. linux����cmd�� gcc esp32_ota_head.c -o esp32_ota_head -l crypto

3.  ���ͷ�ű�ʾ���� ./esp32_ota_head $MODULE $HW_VER $SW_VER $UTIME $USER_OFFSET $USER_BIN $OTA_BIN $PATCH_NONE_FILE
�����б�ֱ��ʾ���ͺţ�Ӳ���汾������汾�����ʱ�䣬����ƫ�����������ļ����ƣ����ɵ����ļ����������ļ�����