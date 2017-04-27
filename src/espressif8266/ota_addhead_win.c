#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <io.h>
#include <Windows.h>
#include <windows.h>

#define ONE_RDWR_SIZE (1024U)
#define NAME_LEN   128
#define VALUE_LEN  128


typedef unsigned int DL_U32;
typedef int DL_32;
typedef unsigned char DL_U8;
//#define FIX_BIN_LEN    0x1DFB


typedef struct t_ota_bin_s
{
    char model[16];
	char hw_version[16];
	char sw_version[16]; /* 软件版本 */
	DL_U32 utc_time;

	DL_U32 user1_bin_offset;   //bin 起始位置
	DL_U32 user1_bin_len;
	DL_U8 user1_md5[16]; /* MD5值 */
	
	DL_U32 user2_bin_offset;
	DL_U32 user2_bin_len;
	DL_U8 user2_md5[16];

    DL_U32 cfg_len;
    DL_U8 cfg_md5[16];
    
    DL_U32 reserve[8];
}T_OTA_BIN_S;

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


static MD5Init_Tpye   MD5_Init;
static MD5Update_Tpye MD5_Update;
static MD5Final_Tpye  MD5_Final;



static FILE *g_user1_fd = NULL;
static FILE *g_user2_fd = NULL;
static FILE *g_wfd = NULL;
FILE *fpConfig = NULL;



/*  写0x00 */
int file_erase(FILE *fd,int addr,int len, DL_U8 ch)
{
	char buf[16];
	int buflen=len;
	memset(buf,ch,16);
	if(fseek(fd,addr,SEEK_SET) < 0)
	{
		printf("file lseek error,addr=%d\n");
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


int check_input_args(T_OTA_BIN_S *ota_head, int argc, char *argv[])
{
    struct stat stat_info;
    
    if(argc < 11)
	{
		printf("error,the args less than 11...\n");
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

    ota_head->user1_bin_offset = atoi(argv[5]);
	if(ota_head->user1_bin_offset < sizeof(T_OTA_BIN_S))
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
    ota_head->user1_bin_len = stat_info.st_size;


    ota_head->user2_bin_offset = atoi(argv[7]);
	if(ota_head->user2_bin_offset < (ota_head->user1_bin_offset + ota_head->user1_bin_len))
	{
		printf("the user2 offset is not long enough..\n");
		return -1;
	}
    if(stat(argv[8], &stat_info) < 0)
    {
        printf("get the user2 file info error\n");
        return -2;
    }
    ota_head->user2_bin_len = stat_info.st_size;


    //use1.bin
	if((g_user1_fd = fopen(argv[6],"rb")) == NULL)
	{
		printf("open the user1 file error\n");
		return -2;
	}

    if((g_user2_fd = fopen(argv[8],"rb"))  == NULL)
	{
		printf("open the user2 file error\n");
		return -2;
	}
	
	if((g_wfd = fopen(argv[9],"wb"))  == NULL)
	{
		printf("open the output file error\n");
		return -2;
	}

	
	if(file_erase(g_wfd, 0 ,ota_head->user1_bin_offset, 0)<0)
	{
		return -1;
	}
    if(file_erase(g_wfd, ota_head->user1_bin_offset, ota_head->user2_bin_offset - ota_head->user1_bin_offset, 0xff)<0)
	{
		return -1;
	}

    if((fpConfig = fopen(argv[10],"r")) == NULL)
    {
        return -2;
    }
    

    return 0;
}

int copy_file_and_md5sum(T_OTA_BIN_S *ota_head)
{
    char buf[ONE_RDWR_SIZE] = {0};
    MD5_CTX ctx_handle_bin;
    int size = 0;

    if(fseek(g_wfd, ota_head->user1_bin_offset, SEEK_SET) < 0)
	{
		printf("file lseek error\n");
		return -1;
	}
    
    MD5_Init(&ctx_handle_bin);
	while((size = fread(buf, 1, ONE_RDWR_SIZE, g_user1_fd))> 0)
	{
		if(fwrite(buf, 1, size, g_wfd)<0)
		{
			printf("write the file error...\n");
			return -1;
		}
  		MD5_Update(&ctx_handle_bin,buf,size); 
	}
	if(size < 0)
	{
		printf("read file error...\n");
		return -1;
	}
	MD5_Final(&ctx_handle_bin);
    memcpy(ota_head->user1_md5, ctx_handle_bin.digest, 16);
    
    if(fseek(g_wfd, ota_head->user2_bin_offset, SEEK_SET) < 0 )
	{
		printf("file lseek error\n");
		return -1;
	}
    MD5_Init(&ctx_handle_bin);
	while((size = fread(buf, 1, ONE_RDWR_SIZE, g_user2_fd))> 0)
	{
		if(fwrite(buf,1, size, g_wfd)<0)
		{
			printf("write the file error...\n");
			return -1;
		}
  		MD5_Update(&ctx_handle_bin,buf,size); 
	}
	if(size < 0)
	{
		printf("read file error...\n");
		return -1;
	}
    MD5_Final(&ctx_handle_bin);
    memcpy(ota_head->user2_md5, ctx_handle_bin.digest, 16);
    
    return 0;
}



int add_cfg_to_end(T_OTA_BIN_S *ota_head)
{
    char cBuf[ONE_RDWR_SIZE] = {0};
    int cfg_len = 0;
    DL_U32 uiBufLen = 0;
    DL_U32 uiNameLen = 0;
    DL_U32 uiValueLen = 0;
    char *pEqualSign = NULL;
    MD5_CTX ctx_handle_config;
    
    if(fseek(g_wfd, 0, SEEK_END) < 0)
    {
        printf("file fseek to end error\n");
        return -1;
    }

    MD5_Init(&ctx_handle_config);
    while(fgets(cBuf, ONE_RDWR_SIZE, fpConfig))
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
        if(fwrite(cBuf, 1, uiBufLen + 1, g_wfd) < 0)
	    {
		    printf("write config info error...\n");
		    return -3;
	    }
        MD5_Update(&ctx_handle_config, cBuf, uiBufLen + 1);
        cfg_len += uiBufLen + 1; 
    }
    MD5_Final(&ctx_handle_config);
    memcpy(ota_head->cfg_md5, ctx_handle_config.digest, 16);
    ota_head->cfg_len = cfg_len;
    return 0;
}

int add_head_final(T_OTA_BIN_S *ota_head)
{
    if(fseek(g_wfd,0,SEEK_SET) < 0)
	{
		printf("file lseek to 0 error\n");
		return -1;
	}
	
	if(fwrite(ota_head,sizeof(T_OTA_BIN_S), 1, g_wfd)<0)
	{
		printf("write ot_bin_head error...\n");
		return -3;
	}
    
	fclose(g_user1_fd);
    fclose(g_user2_fd);
	fclose(g_wfd);
	fclose(fpConfig);
    
    return 0;
}



int main(int argc,char *argv[])
{
	T_OTA_BIN_S ota_bin_head;
	
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
    
    memset(&ota_bin_head,0,sizeof(T_OTA_BIN_S));
    
	if(check_input_args(&ota_bin_head, argc, argv) < 0)
    {
        return -1;
    }   
	
	if(copy_file_and_md5sum(&ota_bin_head) < 0)
    {
        return -2;
    }

    if(add_cfg_to_end(&ota_bin_head) < 0)
    {
        return -4;
    }

    if(add_head_final(&ota_bin_head) < 0)
    {
        return -5;
    }
	
	printf("Add OTA_FILE HEAD Successful...\n");
	return 0;
}




