#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <openssl/md5.h>


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



#define ONE_RDWR_SIZE (1024U)
#define NAME_LEN   128
#define VALUE_LEN  128



static int user1_rfd;
static int user2_rfd;
static int wfd;
FILE *fpConfig = NULL;



/*  写0x00 */
int file_erase(int fd,int addr,int len, DL_U8 ch)
{
	char buf[16];
	int buflen=len;
	memset(buf,ch,16);
	int addr_offset;
	if((addr_offset=lseek(fd,addr,SEEK_SET)) < 0)
	{
		printf(" file erase error,addr=%d\n",addr_offset);
		return -1;
	}
	while(buflen>16)
	{
		if(write(fd,buf,16) != 16)
		{
			printf("write 0x%02x error....\n", ch);
			return -1;
		}
		buflen -=16;
	}
	if(write(fd,buf,buflen) != buflen)
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
	if((user1_rfd=open(argv[6],O_RDONLY)) < 0)
	{
		printf("open the user1 file error\n");
		return -2;
	}

    if((user2_rfd=open(argv[8],O_RDONLY)) < 0)
	{
		printf("open the user2 file error\n");
		return -2;
	}
	
	if((wfd = open(argv[9], O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO)) < 0)
	{
		printf("open the output file error\n");
		return -2;
	}

	
	if(file_erase(wfd, 0 ,ota_head->user1_bin_offset, 0)<0)
	{
		return -1;
	}
    if(file_erase(wfd, ota_head->user1_bin_offset, ota_head->user2_bin_offset - ota_head->user1_bin_offset, 0xff)<0)
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

    if(lseek(wfd, ota_head->user1_bin_offset, SEEK_SET) != ota_head->user1_bin_offset)
	{
		printf("file lseek error\n");
		return -1;
	}
    
    MD5_Init(&ctx_handle_bin);
	while((size = read(user1_rfd, buf, ONE_RDWR_SIZE))> 0)
	{
		if(write(wfd,buf,size)<0)
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
	MD5_Final(ota_head->user1_md5,&ctx_handle_bin);

    
    if(lseek(wfd, ota_head->user2_bin_offset, SEEK_SET) != ota_head->user2_bin_offset)
	{
		printf("file lseek error\n");
		return -1;
	}
    MD5_Init(&ctx_handle_bin);
	while((size = read(user2_rfd, buf, ONE_RDWR_SIZE))> 0)
	{
		if(write(wfd,buf,size)<0)
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
	MD5_Final(ota_head->user2_md5,&ctx_handle_bin);

    
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
    
    if(lseek(wfd, 0, SEEK_END) != (ota_head->user2_bin_offset + ota_head->user2_bin_len))
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
        if(write(wfd, cBuf, uiBufLen + 1) < 0)
	    {
		    printf("write config info error...\n");
		    return -3;
	    }
        MD5_Update(&ctx_handle_config, cBuf, uiBufLen + 1);
        cfg_len += uiBufLen + 1; 
    }

    MD5_Final(ota_head->cfg_md5, &ctx_handle_config);
    ota_head->cfg_len = cfg_len;
    return 0;
}

int add_head_final(T_OTA_BIN_S *ota_head)
{
    if(lseek(wfd,0,SEEK_SET) < 0)
	{
		printf("file lseek to 0 error\n");
		return -1;
	}
	
	if(write(wfd, ota_head,sizeof(T_OTA_BIN_S))<0)
	{
		printf("write ot_bin_head error...\n");
		return -3;
	}
    
	close(user1_rfd);
    close(user2_rfd);
	close(wfd);
	fclose(fpConfig);
    
    return 0;
}



int main(int argc,char *argv[])
{
	T_OTA_BIN_S ota_bin_head;
		
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




