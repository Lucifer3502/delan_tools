
/*if compile in linux system, uncommet this macro. otherwise, if compile in windows system, then comment this macro*/
#define LINUX_OS

#ifndef LINUX_OS
#define WINDOWS_OS
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef LINUX_OS
#include <unistd.h>
#include <openssl/md5.h>
#endif

#ifdef WINDOWS_OS
#include <io.h>
#include <Windows.h>
#include <windows.h>
#endif


#ifndef LOCAL
#define LOCAL  static
#endif

#define ONE_RDWR_SIZE (1024U)
#define dl_printf printf


typedef unsigned int DL_U32;
typedef int DL_32;
typedef unsigned char DL_U8;


typedef struct{
    /* 插件型号 */
    DL_U8 plug_model[16];
    /* 软件版本 */
    DL_U8 plug_sw[16]; 
    /*有效程序文件总大小*/ 
    DL_U32 plug_bin_len;
    /*有效程序段起始位置偏移量*/
    DL_U32 plug_bin_offset; 
    /*有效程序段md5校验值*/ 
    DL_U8 plug_bin_md5[16]; 
    /*保留，自定义*/ 
    DL_U8 reserve[24];
}PLUG_OTA_BIN_S;

#ifdef WINDOWS_OS
typedef struct   
{  
    ULONG i[2];  
    ULONG buf[4];  
    unsigned char in[64];  
    unsigned char digest[16];  
} MD5_CTX;

typedef void (CALLBACK* MD5Init_Tpye)(MD5_CTX* context);  
typedef void (CALLBACK* MD5Update_Tpye)(MD5_CTX* context,unsigned char* input,unsigned int inlen);  
typedef void (CALLBACK* MD5Final_Tpye)(MD5_CTX* context);


LOCAL MD5Init_Tpye   MD5_Init;
LOCAL MD5Update_Tpye MD5_Update;
LOCAL MD5Final_Tpye  MD5_Final;

#endif

LOCAL FILE *g_fp_old_bin = NULL;
LOCAL FILE *g_fp_new_bin = NULL;


LOCAL int dl_analyze_input_args(PLUG_OTA_BIN_S *ota_head, int argc, char *argv[])
{
    struct stat stat_info;
    
    if(argc < 6)
    {
        dl_printf("error,less than 6 args...\n");
        return -1;
    }
    //硬件型号
    if(strlen(argv[1]) >= 16)
    {
        dl_printf("the model length is more than 16\r\n");
        return -1;
    }
    //软件版本
    if(strlen(argv[2]) >= 16)
    {
        dl_printf("the sw_version length is more than 16\r\n");
        return -1;
    }

    strcpy(ota_head->plug_model, argv[1]);
    strcpy(ota_head->plug_sw, argv[2]);
    ota_head->plug_bin_offset = atoi(argv[3]);
    if(ota_head->plug_bin_offset < sizeof(PLUG_OTA_BIN_S))
    {
        dl_printf("the offset is not long enough..\n");
        return -1;
    }

    if(stat(argv[4], &stat_info) < 0)
    {
        printf("get the bin file info error\n");
        return -2;
    }
    ota_head->plug_bin_len= stat_info.st_size;
    if((g_fp_old_bin = fopen(argv[4], "rb")) == NULL)
    {
        dl_printf("Open bin file failed.\r\n");
        return -1;
    }

    if((g_fp_new_bin = fopen(argv[5], "wb")) == NULL)
    {
        dl_printf("Open bin file failed.\r\n");
        return -1;
    }

    return 0;
}

LOCAL int dl_copy_file_and_md5sum(PLUG_OTA_BIN_S *ota_head)
{
    DL_U8 buf[ONE_RDWR_SIZE] = {0};
    DL_U32 size = 0;
    MD5_CTX ctx_bin;

    if(0 != fseek(g_fp_new_bin, ota_head->plug_bin_offset, SEEK_SET))
    {
        dl_printf("file fseek error\n");
        return -1;
    }

    MD5_Init(&ctx_bin);
    while((size = fread(buf, sizeof(DL_U8), ONE_RDWR_SIZE, g_fp_old_bin)) > 0)
    {
        fwrite(buf, sizeof(DL_U8), size, g_fp_new_bin);
        MD5_Update(&ctx_bin, buf, size); 
    }
    if(size < 0)
    {
        dl_printf("read file error...\n");
        return -1;
    }
#ifdef LINUX_OS
    MD5_Final(ota_head->plug_bin_md5, &ctx_bin);
#endif

#ifdef WINDOWS_OS
    MD5_Final(&ctx_bin);
    memcpy(ota_head->plug_bin_md5, ctx_bin.digest, 16);
#endif
    if(0 != fseek(g_fp_new_bin, 0, SEEK_SET))
    {
        dl_printf("file fseek error\n");
        return -1;
    }
    
    if(1 != fwrite(ota_head, sizeof(PLUG_OTA_BIN_S), 1, g_fp_new_bin))
    {
        dl_printf("write ota_bin_head error...\n");
        return -1;
    }

    fclose(g_fp_old_bin);
    fclose(g_fp_new_bin);
    
    return 0;
}

int main(int argc, char *argv[])
{
    PLUG_OTA_BIN_S ota_bin_head;
#ifdef WINDOWS_OS
    HINSTANCE hDLL = LoadLibrary(L"Cryptdll.dll");
    if (hDLL == NULL)
	{
		return -1;
	}
    MD5_Init = (MD5Init_Tpye)GetProcAddress(hDLL, "MD5Init");
	MD5_Update = (MD5Update_Tpye)GetProcAddress(hDLL, "MD5Update");
	MD5_Final = (MD5Final_Tpye)GetProcAddress(hDLL, "MD5Final");
    if (MD5_Init == NULL || MD5_Update == NULL || MD5_Final == NULL)
	{
		FreeLibrary(hDLL);
		return -1;
	}
#endif
    memset(&ota_bin_head, 0, sizeof(PLUG_OTA_BIN_S));
    if(dl_analyze_input_args(&ota_bin_head, argc, argv) < 0)
    {
        return -1;
    }

    if(dl_copy_file_and_md5sum(&ota_bin_head) < 0)
    {
        return -2;
    }
    
    dl_printf("Add OTA_FILE HEAD Successful...\n");
    return 0;
}




