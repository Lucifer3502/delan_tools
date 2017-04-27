#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define BUFFSIZE (1024U)
#define OFFSET   (40U)
int main(int argc,char *argv[])
{
		int rfd;
		int wfd;
		int n;
		char buf[BUFFSIZE]={0};
		if(argc < 3)
		{
			printf("error,please input the input file and output file\n");
		}
		if((rfd = open(argv[1],O_RDONLY))< 0)
		{
			printf("open input file error\n");
			return -1;
		}
		if((wfd = open(argv[2],O_RDWR|O_CREAT|O_APPEND|O_TRUNC,S_IRWXU|S_IRWXG)) < 0)
		{
			close(rfd);
			printf("open outfile error\n");
			return -1;
		}
		
		if(lseek(rfd,OFFSET,SEEK_SET)!= OFFSET)
		{
			printf("lseek error\n");
			return -1;
		}
		
		while((n = read(rfd,buf,BUFFSIZE))>0)
		{
			if(write(wfd,buf,n)<0)
			{
				printf("write file error\n");
				return -1;
			}
		}
		if(n < 0)
		{
			printf("read file error\n");
			return -1;
		}
		close(wfd);
		close(rfd);
		printf("rm the head success\n");
		
		return 0;
}