
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
#define NAME_LEN   128
#define VALUE_LEN  128



typedef unsigned int DL_U32;
typedef int DL_32;
typedef unsigned char DL_U8;

typedef struct
{
    char model[16];
	char hw_version[16];
	char sw_version[16]; /* 软件版本 */
	DL_U32 utc_time;

	DL_U32 user_bin_offset;   //bin 起始位置
	DL_U32 user_bin_len;
	DL_U8 user_md5[16]; /* MD5值 */

    DL_U32 cfg_len;
    DL_U8 cfg_md5[16];
    
    DL_U32 reserve[8];
}DL_OTA_BIN_S;

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
LOCAL FILE *g_fp_config = NULL;


LOCAL int file_erase(FILE *fd,int addr,int len, DL_U8 ch)
{
	char buf[16];
	int buflen=len;
	memset(buf,ch,16);
	if(fseek(fd,addr,SEEK_SET) < 0)
	{
		printf("file lseek error...\n");
		return -1;
	}
	while(buflen>16)
	{
		if(fwrite(buf,1,16,fd) != 16)
		{
			printf("write 0x%02x error....\n", ch);
			return -1;
		}
		buflen -=16;
	}
	if(fwrite(buf,1,buflen,fd) != buflen)
	{
		printf("write 0x%02x error...\n", ch);
		return -1;
	}
	return 0;
}

LOCAL int analyze_input_args(DL_OTA_BIN_S *ota_head, int argc, char *argv[])
{
    struct stat stat_info;
    
    if(argc < 9)
    {
        printf("error,less than 9 args...\r\n");
        return -1;
    }

    if(strlen(argv[1])>=16)
	{
		printf("the model length is more than 16\n");
		return -1;
	}
	if(strlen(argv[2])>=16)
	{
		printf("the hw_version length is more than 16\n");
		return -1;
	}
	if(strlen(argv[3])>=16)
	{
		printf("the sw_version length is more than 16\n");
		return -1;
	}
    strcpy(ota_head->model,argv[1]);
	strcpy(ota_head->hw_version,argv[2]);
	strcpy(ota_head->sw_version,argv[3]);
    ota_head->utc_time=atoi(argv[4]);

    ota_head->user_bin_offset = atoi(argv[5]);
	if(ota_head->user_bin_offset < sizeof(DL_OTA_BIN_S))
	{
		printf("the user1 offset is not long enough..\n");
		return -1;
	}

    //user1_bin info
    if(stat(argv[6], &stat_info) < 0)
    {
        printf("get the user1 file info error\n");
        return -2;
    }
    ota_head->user_bin_len = stat_info.st_size;

    if((g_fp_old_bin = fopen(argv[6], "rb")) == NULL)
    {
        printf("Open bin file failed.\r\n");
        return -1;
    }

    if((g_fp_new_bin = fopen(argv[7], "wb")) == NULL)
    {
        printf("Open bin file failed.\r\n");
        return -1;
    }

    if(file_erase(g_fp_new_bin, 0 ,ota_head->user_bin_offset, 0)<0)
	{
		return -1;
	}

    if((g_fp_config= fopen(argv[8],"r")) == NULL)
    {
        printf("Open config file failed.\r\n");
        return -2;
    }
    return 0;
}

int add_cfg_to_end(DL_OTA_BIN_S *ota_head)
{
    char cBuf[ONE_RDWR_SIZE] = {0};
    int cfg_len = 0;
    DL_U32 uiBufLen = 0;
    DL_U32 uiNameLen = 0;
    DL_U32 uiValueLen = 0;
    char *pEqualSign = NULL;
    MD5_CTX ctx_handle_config;
    
    if(fseek(g_fp_new_bin, 0, SEEK_END) < 0)
    {
        printf("file fseek to end error\n");
        return -1;
    }

    MD5_Init(&ctx_handle_config);
    while(fgets(cBuf, ONE_RDWR_SIZE, g_fp_config))
    {
        if(cBuf[0] == '#' || cBuf[0] == '\n' )
        {
            continue;
        }

        /*去掉回车和换行符*/
        uiBufLen = strlen(cBuf);
        while (cBuf[uiBufLen - 1] == '\n' || cBuf[uiBufLen - 1] == '\r')
        {
            cBuf[uiBufLen - 1] = '\0';
            uiBufLen -= 1;
        }
        
        pEqualSign = strchr(cBuf, '=');
        if(!pEqualSign)
        {
            continue;
        }

        *pEqualSign = '\0';
        uiNameLen = pEqualSign - cBuf;
        uiValueLen = uiBufLen - uiNameLen - 1;
        
        /*the name and value len must less than  NAME_LEN and VALUE_LEN*/
        if((uiValueLen > VALUE_LEN) || (uiNameLen > NAME_LEN))
        {
            return -2;
        }
        
        /* write the name and value with the end sign \0 */
        if(fwrite(cBuf, 1, uiBufLen + 1, g_fp_new_bin) < 0)
	    {
		    printf("write config info error...\n");
		    return -3;
	    }
        MD5_Update(&ctx_handle_config, cBuf, uiBufLen + 1);
        cfg_len += uiBufLen + 1; 
    }

#ifdef LINUX_OS
    MD5_Final(ota_head->cfg_md5, &ctx_handle_config);
#endif

#ifdef WINDOWS_OS
    MD5_Final(&ctx_handle_config);
    memcpy(ota_head->cfg_md5, ctx_handle_config.digest, 16);
#endif

    ota_head->cfg_len = cfg_len;
    return 0;
}


LOCAL int copy_file(DL_OTA_BIN_S *ota_head)
{
    DL_U8 buf[ONE_RDWR_SIZE] = {0};
    DL_U32 size = 0;
    MD5_CTX ctx_bin;

    if(fseek(g_fp_new_bin, ota_head->user_bin_offset, SEEK_SET) < 0)
	{
		printf("file lseek error\n");
		return -1;
	}
    
    MD5_Init(&ctx_bin);
	while((size = fread(buf, 1, ONE_RDWR_SIZE, g_fp_old_bin))> 0)
	{
		if(fwrite(buf, 1, size, g_fp_new_bin)<0)
		{
			printf("write the file error...\n");
			return -1;
		}
  		MD5_Update(&ctx_bin ,buf, size); 
	}
	if(size < 0)
	{
		printf("read file error...\n");
		return -1;
	}
#ifdef LINUX_OS
    MD5_Final(ota_head->user_md5, &ctx_bin);
#endif

#ifdef WINDOWS_OS
    MD5_Final(&ctx_bin);
    memcpy(ota_head->user_md5, ctx_bin.digest, 16);
#endif
    
    
    return 0;
}

int add_head_final(DL_OTA_BIN_S *ota_head)
{
    if(fseek(g_fp_new_bin, 0, SEEK_SET) < 0)
	{
		printf("file lseek to 0 error\n");
		return -1;
	}
	
	if(fwrite(ota_head, sizeof(DL_OTA_BIN_S), 1, g_fp_new_bin) < 0)
	{
		printf("write ot_bin_head error...\n");
		return -3;
	}
    
	fclose(g_fp_old_bin);
    fclose(g_fp_new_bin);
	fclose(g_fp_config);
    
    return 0;
}


int main(int argc, char *argv[])
{
    DL_OTA_BIN_S ota_bin_head;
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
    memset(&ota_bin_head, 0, sizeof(DL_OTA_BIN_S));
    if(analyze_input_args(&ota_bin_head, argc, argv) < 0)
    {
        fflush();
        return -1;
    }

    if(copy_file(&ota_bin_head) < 0)
    {
        return -2;
    }

    if(add_cfg_to_end(&ota_bin_head) < 0)
    {
        return -3;
    }

    if(add_head_final(&ota_bin_head) < 0)
    {
        return -4;
    }
    
    printf("Add OTA_FILE HEAD Successful...\n");
    return 0;
}




