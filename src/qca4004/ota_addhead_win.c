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

typedef struct t_ota_bin_s
{ 
    DL_U32 bin_offset;
    char model[16]; /* 型号 */
    char hw_version[16]; /* 硬件版本 */
    char sw_version[16]; /* 软件版本 */
    DL_U32 utc_time;
    DL_U8 md5[16]; /* MD5值 */
    DL_U32 boardata_start;       /*boardata 起始位置*/
    DL_U32 boardata_len;                /*boardata 长度*/
    DL_U8 fix_bin_md5[16];       /*除去boardata之后文件的md5值*/
    DL_32 clear_cfg_flag; /* 清空配置标识 1--清空； 0--保持 */
    DL_32 force_update_flag; /* 强制升级标识，不检查配置 */
    DL_U32 bin_len; /* 升级文件长度 */
    DL_U32 cfg_len;
    DL_U8 cfg_md5[16];
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


static FILE *rfd;
static FILE *wfd;
static FILE *fpConfig = NULL;
static MD5Init_Tpye   MD5_Init;
static MD5Update_Tpye MD5_Update;
static MD5Final_Tpye  MD5_Final;




/*  写0x00 */
int file_erase(FILE *fd, int addr, int len)
{
    char buf[16];
    int buflen = len;
	int addr_offset = 0;
    memset(buf,0x00,16);
    if((addr_offset=fseek(fd,addr,SEEK_SET)) < 0)
    {
        printf(" file erase error,addr=%d\n",addr_offset);
        return -1;
    }
    while(buflen>16)
    {
        if(fwrite(buf,1,16,fd) != 16)
        {
            printf("write ox00 error....\n");
            return -1;
        }
        buflen -=16;
    }
    if(fwrite(buf,1,buflen,fd) != buflen)
    {
        printf("write ox00 error...\n");
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
    

    ota_head->bin_offset = atoi(argv[1]);
    if(ota_head->bin_offset < sizeof(T_OTA_BIN_S))
    {
        printf("the offset is not long enough..\n");
        return -1;
    }

    if(strlen(argv[2])>=16)
    {
        printf("the model length is more than 16\n");
        return -1;
    }
    if(strlen(argv[3])>=16)
    {
        printf("the hw_version length is more than 16\n");
        return -1;
    }
    if(strlen(argv[4])>=16)
    {
        printf("the sw_version length is more than 16\n");
        return -1;
    }
    strcpy(ota_head->model,argv[2]);
    strcpy(ota_head->hw_version,argv[3]);
    strcpy(ota_head->sw_version,argv[4]);
    ota_head->utc_time=atoi(argv[5]);
    
    
    if(stat(argv[6], &stat_info) < 0)
    {
        printf("get the input file info error\n");
        return -2;
    }
    ota_head->bin_len = stat_info.st_size + ota_head->bin_offset;
    printf("new file size:[0x%08x]\n",ota_head->bin_len);
    
    if((rfd = fopen(argv[6],"rb")) == NULL)
    {
        printf("open the input file error\n");
        return -2;
    }
    
//  strncpy(filename,argv[7],31);
    ota_head->boardata_start = atoi(argv[8]);
    ota_head->boardata_len = atoi(argv[9]);
    
    if((wfd =fopen(argv[7],"wb+")) == NULL)
    {
        printf("open the output file error\n");
        return -2;
    }

    
    if(file_erase(wfd,0,ota_head->bin_offset)<0)
    {
        return -1;
    }


    if(fseek(wfd, ota_head->bin_offset, SEEK_SET) < 0)
    {
        printf("file lseek error\n");
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
    
    MD5_Init(&ctx_handle_bin);
    while((size = fread(buf,1,ONE_RDWR_SIZE,rfd))> 0)
    {
        if(fwrite(buf,1,size,wfd)<0)
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
    memcpy(ota_head->md5, ctx_handle_bin.digest, 16);
    return 0;
}

int md5sum_except_boarddata(T_OTA_BIN_S *ota_head)
{
    MD5_CTX ctx_handle_fix_bin;
    DL_U32 totalSize = 0;
    char buf[ONE_RDWR_SIZE] = {0};
    int size = 0;
    
    if(fseek(wfd, ota_head->bin_offset,SEEK_SET) < 0)
    {
        printf("file lseek error\n");
        return -1;
    }
    
    MD5_Init(&ctx_handle_fix_bin);
    while(totalSize + ONE_RDWR_SIZE < ota_head->boardata_start )
    {
        size = fread(buf, 1, ONE_RDWR_SIZE, wfd);
        if(size < 0)
        {
            printf("read file error...\n");
            return -1;
        }
        if(size == 0)
        {
            break;
        }
        MD5_Update(&ctx_handle_fix_bin,buf,size);
        totalSize += size;
    }
    if(ota_head->boardata_start > totalSize)
    {
        size = fread(buf, 1, ota_head->boardata_start - totalSize, wfd);
        if(size < 0)
        {
            printf("read file error...\n");
            return -1;
        }
        MD5_Update(&ctx_handle_fix_bin,buf,size);
        totalSize += size;
    }

    if(fseek(wfd, ota_head->boardata_len,SEEK_CUR) < 0)
    {
        printf("file lseek error\n");
        return -1;
    }

    while((size = fread(buf, 1, ONE_RDWR_SIZE, wfd))> 0)
    {
        MD5_Update(&ctx_handle_fix_bin,buf,size); 
    }
    if(size < 0)
    {
        printf("read file error...\n");
        return -1;
    }
    MD5_Final(&ctx_handle_fix_bin);
    memcpy(ota_head->fix_bin_md5, ctx_handle_fix_bin.digest, 16);
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
    
    if(fseek(wfd, 0, SEEK_END) < 0)
    {
        printf("file lseek to end error\n");
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
        if(fwrite(cBuf, 1, uiBufLen + 1, wfd) < 0)
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
    if(fseek(wfd,0,SEEK_SET) < 0)
    {
        printf("file lseek to 0 error\n");
        return -1;
    }
    
    if(fwrite(ota_head, sizeof(T_OTA_BIN_S), 1, wfd)<0)
    {
        printf("write ot_bin_head error...\n");
        return -3;
    }
    
    fclose(rfd);
    fclose(wfd);
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

    if(md5sum_except_boarddata(&ota_bin_head) < 0)
    {
        return -3;
    }

    if(add_cfg_to_end(&ota_bin_head) < 0)
    {
        return -4;
    }

    if(add_head_final(&ota_bin_head) < 0)
    {
        return -5;
    }
    
    printf("add ota_file head successful...\n");
    return 0;
}




