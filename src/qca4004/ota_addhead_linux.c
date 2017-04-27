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
	DL_U32 bin_offset;
	char model[16]; /* �ͺ� */
	char hw_version[16]; /* Ӳ���汾 */
	char sw_version[16]; /* ����汾 */
	DL_U32 utc_time;
	DL_U8 md5[16]; /* MD5ֵ */
	DL_U32 boardata_start;       /*boardata ��ʼλ��*/
	DL_U32 boardata_len;				/*boardata ����*/
	DL_U8 fix_bin_md5[16];       /*��ȥboardata֮���ļ���md5ֵ*/
    DL_32 clear_cfg_flag; /* ������ñ�ʶ 1--��գ� 0--���� */
    DL_32 force_update_flag; /* ǿ��������ʶ����������� */
    DL_U32 bin_len; /* �����ļ����� */
    DL_U32 cfg_len;
    DL_U8 cfg_md5[16];
}T_OTA_BIN_S;


#define ONE_RDWR_SIZE (1024U)
#define NAME_LEN   128
#define VALUE_LEN  128



static int rfd;
static int wfd;
FILE *fpConfig = NULL;



/*  д0x00 */
int file_erase(int fd,int addr,int len)
{
	char buf[16];
	int buflen=len;
	memset(buf,0x00,16);
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
			printf("write ox00 error....\n");
			return -1;
		}
		buflen -=16;
	}
	if(write(fd,buf,buflen) != buflen)
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
	
	if((rfd=open(argv[6],O_RDONLY)) < 0)
	{
		printf("open the input file error\n");
		return -2;
	}
	
//	strncpy(filename,argv[7],31);
	ota_head->boardata_start = atoi(argv[8]);
	ota_head->boardata_len = atoi(argv[9]);
	
	if((wfd = open(argv[7], O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO)) < 0)
	{
		printf("open the output file error\n");
		return -2;
	}

	
	if(file_erase(wfd,0,ota_head->bin_offset)<0)
	{
		return -1;
	}


	if(lseek(wfd, ota_head->bin_offset, SEEK_SET) != ota_head->bin_offset)
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
	while((size = read(rfd,buf,ONE_RDWR_SIZE))> 0)
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
	MD5_Final(ota_head->md5,&ctx_handle_bin);

    return 0;
}

int md5sum_except_boarddata(T_OTA_BIN_S *ota_head)
{
    MD5_CTX ctx_handle_fix_bin;
    int totalSize = 0;
    char buf[ONE_RDWR_SIZE] = {0};
    int size = 0;
    
    if(lseek(wfd, ota_head->bin_offset,SEEK_SET) != ota_head->bin_offset)
    {
        printf("file lseek error\n");
        return -1;
    }
    
    MD5_Init(&ctx_handle_fix_bin);
    while(totalSize + ONE_RDWR_SIZE < ota_head->boardata_start )
    {
        size = read(wfd,buf,ONE_RDWR_SIZE);
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
        size = read(wfd,buf,ota_head->boardata_start - totalSize);
        if(size < 0)
        {
            printf("read file error...\n");
            return -1;
        }
        MD5_Update(&ctx_handle_fix_bin,buf,size);
        totalSize += size;
    }

    if(lseek(wfd, ota_head->boardata_len,SEEK_CUR) != ota_head->boardata_len + ota_head->bin_offset + ota_head->boardata_start)
    {
        printf("file lseek error\n");
        return -1;
    }

    while((size = read(wfd,buf,ONE_RDWR_SIZE))> 0)
    {
        MD5_Update(&ctx_handle_fix_bin,buf,size); 
    }
    if(size < 0)
    {
        printf("read file error...\n");
        return -1;
    }
    MD5_Final(ota_head->fix_bin_md5,&ctx_handle_fix_bin);

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
    
    if(lseek(wfd, 0, SEEK_END) != ota_head->bin_len)
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

        /*ȥ���س��ͻ��з�*/
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
    
	close(rfd);
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




