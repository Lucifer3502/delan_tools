#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#define BUFSIZE    1024
#define NAME_LEN   128
#define VALUE_LEN  128
#define CFG_HEAD   "CFG_HEAD"
#define CFG_TAIL   "CFG_TAIL"
#define OK_VALUE   "OK"

typedef unsigned int uint32;


int GetNameValue(FILE *fpConfig, FILE *fpBinFile, const uint32 uiConfigOffset)
{
    char cBuf[BUFSIZE];
    uint32 uiBufLen = 0;
    uint32 uiNameLen = 0;
    uint32 uiValueLen = 0;
    char *pEqualSign = NULL;

    if(0 != fseek(fpBinFile, uiConfigOffset, SEEK_SET))
    {
        return -1;
    }

    fwrite(CFG_HEAD, sizeof(char), sizeof(CFG_HEAD), fpBinFile);
    fwrite(OK_VALUE, sizeof(char), sizeof(OK_VALUE), fpBinFile);
    
    while(fgets(cBuf, BUFSIZE, fpConfig))
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
        fwrite(cBuf, sizeof(char), uiBufLen + 1, fpBinFile);
        
    }
    
    fwrite(CFG_TAIL, sizeof(char), sizeof(CFG_TAIL), fpBinFile);
    fwrite(OK_VALUE, sizeof(char), sizeof(OK_VALUE), fpBinFile);
    
    return 0;
}

int main(int argc, char *argv[])
{
    char cValue[VALUE_LEN] = {0};
    FILE *fpConfig = NULL;
    FILE *fpBinFile = NULL;
    uint32 uiConfigOffset;
    int iRet = 0;
    
    if(argc < 4)
    {
        return -1;
    }
    
    /*config file name*/
    if((fpConfig = fopen(argv[1],"r")) == NULL)
    {
        return -2;
    }
    
    /*config file name*/
    if((fpBinFile = fopen(argv[2],"rb+")) == NULL)
    {
        fclose(fpConfig);
        return -2;
    }

    uiConfigOffset = atoi(argv[3]);
    
    iRet = GetNameValue(fpConfig, fpBinFile, uiConfigOffset);
    if(iRet)
    {
        printf("GetNameValue failed. iRet = [%d]\n",iRet);
        iRet = -3;
    }
    fclose(fpConfig);
    fclose(fpBinFile);
    
    return iRet;
}

