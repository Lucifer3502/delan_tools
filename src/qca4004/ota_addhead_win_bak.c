#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <io.h>
#include <Windows.h>
#include <windows.h>

//#include "openssl/md5.h"



typedef unsigned int DL_U32;
typedef unsigned char DL_U8;


typedef struct t_ota_bin_s
{ 
	DL_U32 bin_offset;
	char model[16]; /* 型号 */
	char hw_version[16]; /* 硬件版本 */
	char sw_version[16]; /* 软件版本 */
	DL_U32 utc_time;
	DL_U8 md5[16]; /* MD5值 */
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

#define ONE_RDWR_SIZE (1024U)



int file_erase(FILE *fd,int addr,int len)
{
	char buf[16];
	int buflen=len;

	memset(buf,0x00,16);

	if(fseek(fd,addr,SEEK_SET) < 0)
	{
		printf(" file seek error\n");
		return -1;
	}
	while(buflen>16)
	{
		if(fwrite(buf,1,16,fd) != 16)
		{
			printf("_write ox00 error.1...\n");
			return -1;
		}
		buflen -=16;
	}
	if(fwrite(buf,1,16,fd) != 1)
	{
		printf("_write ox00 error..2.\n");
		return -1;
	}
	return 0;
}

int main(int argc,char *argv[])
{
	T_OTA_BIN_S ota_bin_head;
	FILE *rfd=NULL;
	FILE *wfd=NULL;
	char filename[32] = {0};
	char defaultname[] = "output.bin";
	int size;
	char buf[ONE_RDWR_SIZE] = {0};
	MD5_CTX ctx_handle;	
	MD5Init_Tpye   MD5Init;
	MD5Update_Tpye MD5Update;  
	MD5Final_Tpye  MD5Final;
	HINSTANCE hDLL = LoadLibrary(L"Cryptdll.dll");

	int ret;
	if (hDLL == NULL)
	{
		return -1;
	}
	MD5Init = (MD5Init_Tpye)GetProcAddress(hDLL, "MD5Init");
	MD5Update = (MD5Update_Tpye)GetProcAddress(hDLL, "MD5Update");
	MD5Final = (MD5Final_Tpye)GetProcAddress(hDLL, "MD5Final");
	if (MD5Init == NULL || MD5Update == NULL || MD5Final == NULL)
	{
		FreeLibrary(hDLL);
		return -1;
	}
	memset(&ota_bin_head,0,sizeof(T_OTA_BIN_S));
	
	if(argc < 7)
	{
		printf("error,please input 7 args:offset,model,hw,sw,time,filename\n");
		return -1;
	}
	

	ota_bin_head.bin_offset = atoi(argv[1]);
	if(ota_bin_head.bin_offset < sizeof(T_OTA_BIN_S))
	{
		printf("the offset is not long enough..\n");
		return -1;
	}
//	printf("bin_offset:%d,struct size:%d\n",ota_bin_head.bin_offset,sizeof(ota_bin_head));
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
	strcpy(ota_bin_head.model,argv[2]);
	strcpy(ota_bin_head.hw_version,argv[3]);
	strcpy(ota_bin_head.sw_version,argv[4]);
	ota_bin_head.utc_time=atoi(argv[5]);
	
	printf("debug test 1\n");
	MD5Init(&ctx_handle);
	
	if((rfd=fopen(argv[6],"rb")) == NULL)
	{
		printf("open the input file error\n");
		return -2;
	}
	
	if(argc == 8)
	{
		memcpy(filename,argv[7],31);
	}
	else 
	{
		strcpy(filename,defaultname);
	}
	
	if((wfd =fopen(filename,"wb+")) == NULL)
	{
		fclose(rfd);
		printf("open the output file error\n");
		return -2;
	}

	
//	if(file_erase(wfd,0,ota_bin_head.bin_offset)<0)
//	{
//		printf(" file erase error\n");
//		return -1;
//	}
	
	if((ret=fseek(wfd,ota_bin_head.bin_offset,SEEK_SET)) < 0)
	{
		printf(" file seek error,%d\n",ret);
		perror("error:");
		return -1;
	}
	printf("debug test 2\n");
	while((size = fread(buf,1,ONE_RDWR_SIZE,rfd))> 0)
	{
		if(fwrite(buf,1,size,wfd)<0)
		{
			printf("_write the file error...\n");
			return -1;
		}
		MD5Update(&ctx_handle,buf,size);
		memset (buf,0,ONE_RDWR_SIZE);
		printf("debug test 5,%d\n",size);
	}
	if(size < 0)
	{
		printf("_read file error...\n");
		printf("debug test 4\n");
		return -1;
	}
	printf("debug test 3,%d\n",size);
	MD5Final(&ctx_handle);
	memcpy(ota_bin_head.md5,ctx_handle.digest,16);
	
	if(fseek(wfd,0,SEEK_SET) < 0)
	{
		printf("file _lseek to 0 error\n");
		perror("error:");
		return -1;
	}
	
	if(fwrite(&ota_bin_head,sizeof(ota_bin_head),1,wfd)<0)
	{
		printf("_write ot_bin_head error...\n");
		return -3;
	}
	
	fclose(rfd);
	fclose(wfd);
	printf("success...\n");
	return 0;
}
