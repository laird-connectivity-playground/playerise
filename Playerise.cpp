/******************************************************************************/
/*
** Playerise.cpp : Defines the entry point for the console application.
**
** Copyright (C) 2017, Laird Connectivity
*/
/******************************************************************************/

/******************************************************************************/
/* Include Files*/
/******************************************************************************/

#if defined(_WIN32)
#include <conio.h>
#else
#include <ncurses.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

/******************************************************************************/
/* Local Defines*/
/******************************************************************************/

#define READ_LINESIZE                          (1024*8)
#define MAX_SCR_LINE_LEN                        1024

/******************************************************************************/
/* Local Macros*/
/******************************************************************************/
#if defined(__linux__)
#define StdSTRCMP(_a_,_b_)              strcmp((_a_),(_b_))
#define StdSTRCMPI(_a_,_b_)             strcasecmp((_a_),(_b_))
#define _StdSTRCMPI(_a_,_b_)            strcasecmp((_a_),(_b_))
#else
#define StdSTRCMP(_a_,_b_)              strcmp((_a_),(_b_))
#define StdSTRCMPI(_a_,_b_)             strcmpi((_a_),(_b_))
#define _StdSTRCMPI(_a_,_b_)            _strcmpi((_a_),(_b_))
#endif


/******************************************************************************/
/* Local enums */
/******************************************************************************/

/******************************************************************************/
/* Local Forward Class,Struct & Union Declarations*/
/******************************************************************************/

/******************************************************************************/
/* Local Class,Struct,Union Typedefs*/
/******************************************************************************/

/******************************************************************************/
/* External Variable Declarations*/
/******************************************************************************/

/******************************************************************************/
/* Global/Static Variable Declarations*/
/******************************************************************************/

char gbaLineBuf[READ_LINESIZE+2];
char *cSEND = "SEND -1 [vPort] \"[vCmd]\" [vPort] \"[vResp]\"";

    /*
    0 == call PlayeriseTheFile() 
    1 == call PlayeriseTheFileAuto00() command line "auto00"
    2 == call PlayeriseTheFileAsData() command line "datafile"
    */
int  gmMode = 0;
char gbaStringPlayerFilename[128]={0};
bool gfAddATpRun=false;
bool gfUseATpFWRH=false;
bool gfAddTITLE=true;
char gbaPortId[32];
int nATpFWR_MaxStrLen = 68; /* must be <=(MAX_SCR_LINE_LEN-16) */
int  gnTimeoutMs=5000;
  

/******************************************************************************/
/******************************************************************************/
/* Local Functions or Private Members*/
/******************************************************************************/
/******************************************************************************/

/*=============================================================================*/
/*=============================================================================*/
bool PlayeriseIsWhitespace(char ch)
{
    return ((ch<=' ')||(ch>126)) ? true : false;
}
/*=============================================================================*/
/*=============================================================================*/
void PlayeriseTrimWhitespaceTrailing(char *pLine, char *pLineToNull)
{
    pLineToNull--;
    while(pLine <= pLineToNull)
    {
        if( PlayeriseIsWhitespace(*pLineToNull) )
        {
            *pLineToNull=0;
            pLineToNull--;
        }
        else
        {
            return;
        }
    }
}
/*=============================================================================*/
/*=============================================================================*/
char *PlayeriseTrimWhitespaceLeading(char *pLine)
{
    while( PlayeriseIsWhitespace(*pLine) )
    {
        pLine++;
    }
    return pLine;
}

/*=============================================================================*/
/*=============================================================================*/
char *PlayeriseTrimLine(char *pLine)
{
    char *pRetVal=pLine;
    int nLineLen = strlen(pLine);
    PlayeriseTrimWhitespaceTrailing(pLine,&pLine[nLineLen]);
#if 1

#else
    nLineLen = strlen(pLine);
    if(nLineLen)
    {
        pRetVal = PlayeriseTrimWhitespaceLeading(pLine);
    }
    else
    {
        pRetVal = pLine;
    }
#endif
    return pRetVal;
}

/*=============================================================================*/
/*=============================================================================*/
void PlayeriseSendCmd(FILE *fpOut,char *pPortId,char *pLine)
{
    int ch;
    int nLineLen = strlen(pLine);

    fprintf(fpOut,"\nSENDCMD %s \"",pPortId);
    while(nLineLen--)
    {
        ch = *pLine++;
        switch(ch)
        {
        case '"':
            fprintf(fpOut,"\\22");
            break;
        case 10:
            fprintf(fpOut,"\\n");
            break;
        case 13:
            fprintf(fpOut,"\\r");
            break;
        case 9:
            fprintf(fpOut,"\\t");
            break;
        //case '\\':
        //    fprintf(fpOut,"\\\\");
        //    break;
        default:
            fprintf(fpOut,"%c",ch);
        }
    }
    fprintf(fpOut,"\\r\"");
}

/*=============================================================================*/
/*=============================================================================*/
void PlayeriseWaitRespEx(FILE *fpOut,char *pPortId,int timeoutMs,char *pLine,bool fAddCR)
{
    int ch;
    int nLineLen = strlen(pLine);

    fprintf(fpOut,"\nWAITRESPEX %d %s \"\\n",timeoutMs,pPortId);
    while(nLineLen--)
    {
        ch = *pLine++;
        switch(ch)
        {
        case '"':
            fprintf(fpOut,"\\22");
            break;
        case 10:
            fprintf(fpOut,"\\n");
            break;
        case 13:
            fprintf(fpOut,"\\r");
            break;
        default:
            if( (ch<' ')||(ch>=0x7F) )
            {
                fprintf(fpOut,"\\%02X",ch);
            }
            else
            {
                fprintf(fpOut,"%c",ch);
            }
        }
    }
    if(fAddCR)
    {
        fprintf(fpOut,"\\r\"");
    }
    else
    {
        fprintf(fpOut,"\"");
    }
}

/*=============================================================================*/
/*=============================================================================*/
void  PlayeriseWriteHeaderLines(FILE *fpOut, char *pFilename, char *pPortId)
{
    fprintf(fpOut,"\n//*******************************************************************************");
    fprintf(fpOut,"\nTITLE \"<<<NewSub>>>\"");
    fprintf(fpOut,"\nSET vFilename \"%s\"",pFilename);
    fprintf(fpOut,"\n\n//===============================================================================");
    fprintf(fpOut,"\nTITLE \"**** [vFileName] ****\"");
    fprintf(fpOut,"\n\nSET vPort \"%s\"",pPortId);
    fprintf(fpOut,"\n#include \"BrkResetAt.sbr\"\n");
}

/*=============================================================================*/
/*=============================================================================*/
int PlayeriseTheFile(FILE *fpIn, FILE *fpOut, char *pFilename)
{
    char *pLineTrimmed;
    int nLineLen;
    char *pLine = gbaLineBuf;
    int nState=0;

    PlayeriseWriteHeaderLines(fpOut,pFilename,0);

    while(fgets(pLine,READ_LINESIZE,fpIn) != NULL)
    {
        pLineTrimmed = PlayeriseTrimLine(pLine);
        nLineLen = strlen(pLineTrimmed);
        if(nLineLen)
        {
            switch(nState)
            {
            case 0:
#if 1
                if( strstr(pLineTrimmed,"at+run")!=NULL)
                {
                    nState=1;
                    PlayeriseSendCmd(fpOut,gbaPortId,pLineTrimmed);
                }
                else 
#endif
                if( (pLineTrimmed[0]=='{') && (pLineTrimmed[1]=='{') )
                {
                    nState=1;
                    pLineTrimmed = &pLineTrimmed[2];
                    PlayeriseSendCmd(fpOut,gbaPortId,pLineTrimmed);
                }
                else if( ! isdigit(pLineTrimmed[0]) )
                {
                    PlayeriseSendCmd(fpOut,gbaPortId,pLineTrimmed);
                }
                else
                {
                    PlayeriseWaitRespEx(fpOut,0,gnTimeoutMs,pLineTrimmed,true);
                    fprintf(fpOut,"\n");
                }
                break;

            case 1:
#if 1
                if( (pLineTrimmed[0]=='0') && (pLineTrimmed[1]=='0') )
                {
                    nState=0;
                    PlayeriseWaitRespEx(fpOut,0,gnTimeoutMs,pLineTrimmed,true);
                    fprintf(fpOut,"\n");
                }
                else 
#endif
                if( (pLineTrimmed[0]=='}') && (pLineTrimmed[1]=='}') )
                {
                    nState=0;
                    pLineTrimmed = &pLineTrimmed[2];
                    PlayeriseWaitRespEx(fpOut,0,gnTimeoutMs,pLineTrimmed,true);
                    fprintf(fpOut,"\n");
                }
                else
                {
                    PlayeriseWaitRespEx(fpOut,0,gnTimeoutMs,pLineTrimmed,false);
                }
                break;
            }
        }
    }
    return 0;
}

/*=============================================================================*/
/*=============================================================================*/
int PlayeriseTheFileAuto00(FILE *fpIn, FILE *fpOut)
{
    char *pLineTrimmed;
    int nLineLen;
    char *pLine = gbaLineBuf;
    int nState=0;

    while(fgets(pLine,READ_LINESIZE,fpIn) != NULL)
    {
        pLineTrimmed = PlayeriseTrimLine(pLine);
        nLineLen = strlen(pLineTrimmed);
        if(nLineLen)
        {
            PlayeriseSendCmd(fpOut,gbaPortId,pLineTrimmed);
            PlayeriseWaitRespEx(fpOut,gbaPortId,gnTimeoutMs,"00\r",false);
            fprintf(fpOut,"\n");
        }
    }
    return 0;
}

//===========================================================================
//===========================================================================
int HexitAndAdd(unsigned char nByte, char *pDst)
{
    sprintf(pDst,"%02X",nByte);
    pDst[2]=0;
    return 2;
}

//===========================================================================
//===========================================================================
int EscapeAndAdd(unsigned char nByte, char *pDst)
{
    int nRetVal=1;
    if( (nByte >= ' ') 
        && ( nByte < 0x7F) 
        && (nByte != '\\') 
        && (nByte != '"')
        && (nByte != '[')
        && (nByte != ']')
        )
    {
        *pDst=nByte;
    }
    else
    {
        /* else we need to escape the character */
#if 1
        nRetVal=5;
        sprintf(pDst,"\\5C%02X",nByte);
#else
        switch(nByte)
        {
        case '\r':
            nRetVal=4;
            memcpy(pDst,"\\5Cr",nRetVal);
            break;
        case '\n':
            nRetVal=4;
            memcpy(pDst,"\\5Cn",nRetVal);
            nRetVal=4;
            break;
        case '\t':
            nRetVal=4;
            memcpy(pDst,"\\5Ct",nRetVal);
            nRetVal=4;
            break;
        case '\\':
            nRetVal=6;
            memcpy(pDst,"\\5C\\5C",nRetVal);
            break;
        case '[':
        case ']':
        default:
            nRetVal=5;
            sprintf(pDst,"\\5C%02X",nByte);
            break;
        }
#endif
    }
    pDst[nRetVal]=0;
    return nRetVal;
}

/*=============================================================================*/
/*=============================================================================*/
int LoadFromBinFile(FILE *fpIn,char *pLine, int nMaxLine, bool fSendHex )
{
    unsigned char nByte;
    int nCount;

    nCount=0;
    while( nCount < nMaxLine )
    {
        if( fread(&nByte,1,1,fpIn) <= 0 )
        {
             break;
        }
        if(fSendHex)
        {
            nCount+=HexitAndAdd(nByte,&pLine[nCount]);
        }
        else
        {
            nCount+=EscapeAndAdd(nByte,&pLine[nCount]);
        }
    }
    return nCount;
}

/*=============================================================================*/
/*=============================================================================*/
int PlayeriseTheFileAsData(FILE *fpIn, FILE *fpOut)
{
    static char baLine[MAX_SCR_LINE_LEN+2];
    int timeoutMs=5000;
    int nLineLen;
    char *pFilename;

    if( strlen(gbaStringPlayerFilename) > 0 )
    {
        pFilename = gbaStringPlayerFilename;
    }
    else
    {
        pFilename = "filename";
    }

    if(gfAddTITLE)
    {
        fprintf(fpOut,"\n//*******************************************************************************");
        fprintf(fpOut,"\nTITLE \"<<<NewSub>>>\"");
        fprintf(fpOut,"\nSET vFilename \"%s.sub\"",pFilename);
        fprintf(fpOut,"\n\n//===============================================================================");
        fprintf(fpOut,"\nTITLE \"**** [vFileName] ****\"");
    }

    fprintf(fpOut,"\n\nSET vPort \"%s\"",gbaPortId);
    fprintf(fpOut,"\n#include \"BrkResetAt.sbr\"\n");
    fprintf(fpOut,"\n\n");

    fprintf(fpOut,"\nSENDCMD %s \"AT+DEL \\22%s\\22 +\\r\"",gbaPortId,pFilename);
    fprintf(fpOut,"\nWAITRESPEX %d %s \"\\n00\\r\"",timeoutMs,gbaPortId);
    fprintf(fpOut,"\nSENDCMD %s \"AT+FOW \\22%s\\22\\r\"",gbaPortId,pFilename);
    fprintf(fpOut,"\nWAITRESPEX %d %s \"\\n00\\r\"",timeoutMs,gbaPortId);
    fprintf(fpOut,"\n\n");

    nLineLen=LoadFromBinFile(fpIn,baLine,nATpFWR_MaxStrLen,gfUseATpFWRH);
    while(nLineLen)
    {
        if(gfUseATpFWRH)
        {
            fprintf(fpOut,"\nSENDCMD %s \"AT+FWRH \\22%s\\22\\r\"",gbaPortId,baLine);
        }
        else
        {
            fprintf(fpOut,"\nSENDCMD %s \"AT+FWR \\22%s\\22\\r\"",gbaPortId,baLine);
        }
        PlayeriseWaitRespEx(fpOut,gbaPortId,gnTimeoutMs,"00\r",false);
        nLineLen=LoadFromBinFile(fpIn,baLine,nATpFWR_MaxStrLen,gfUseATpFWRH);
    }

    fprintf(fpOut,"\n\n");
    fprintf(fpOut,"\nSENDCMD %s \"AT+FCL\\r\"",gbaPortId);
    fprintf(fpOut,"\nWAITRESPEX %d %s \"\\n00\\r\"",timeoutMs,gbaPortId);

    if(gfAddATpRun)
    {
        fprintf(fpOut,"\n\n");
        fprintf(fpOut,"\nSENDCMD %s \"AT+RUN \\22%s\\22\\r\"",gbaPortId,pFilename);
        fprintf(fpOut,"\nWAITRESPEX %d %s \"\\n00\\r\"",timeoutMs,gbaPortId);
    }

    return 0;
}
#undef MAX_SCR_LINE_LEN

/*=============================================================================*/
/*=============================================================================*/
int PlayeriseTheFileAsDataGen(FILE *fpOut)
{
    int i;
    int ch;

    for(i=0;i<512;i++)
    {
        ch = i & 0xFF;
        fwrite(&ch,1,1,fpOut);
    }

    return 0;
}

/******************************************************************************/
/******************************************************************************/
/* Global Functions or Public Members*/
/******************************************************************************/
/******************************************************************************/

/*=============================================================================*/
/*=============================================================================*/
void PrintUsage(void)
{
    printf("\nUsage: Playerise filename outfilename <options>");
    printf("\n  <options> are ... ");
    printf("\n    auto00         Add WAITRESPEX automatically after each line");
    printf("\n    datafile       Download as data file");
    printf("\n    datafilegen    Create a binary file of 512 bytes");
    printf("\n    /F filename    Use this name in the StringPlayer output");
    printf("\n    /P portid      Use 'portid' as a string e in the StringPlayer output");
    printf("\n    /R             Add AT+RUN at the end");
    printf("\n    /H             Use AT+FWRH instead og AT+FWR to download data");
    printf("\n    /T             Add TITLE statements (default)");
    printf("\n    /W timeoutms   Use this timeout in WAITRESPEX statements");
    printf("\n    /t             Suppress TITLE statements");
    printf("\n");
}

/*=============================================================================*/
/*=============================================================================*/
int main(int argc, char* argv[])
{
    int nRetVal=1;
    int nIndex;
    FILE *fpIn=NULL;
    FILE *fpOut=NULL;

    if(argc >= 3 )
    {
        /*
        argv[1] == input_filename
        argv[2] == output_filename
        */

        /* set defaults */
        strcpy(gbaPortId,"0");

        for(nIndex=3;nIndex<argc;nIndex++)
        {
            if( _StdSTRCMPI(argv[nIndex],"auto00")==0 )
            {
                gmMode = 1;
            }
            else if( _StdSTRCMPI(argv[nIndex],"datafile")==0 )
            {
                gmMode = 2;
            }
            else if( _StdSTRCMPI(argv[nIndex],"datafilegen")==0 )
            {
                gmMode = 3;
            }
            else if( _StdSTRCMPI(argv[nIndex],"/F")==0 )
            {
                nIndex++;
                if(nIndex<argc)
                {
                    strcpy(gbaStringPlayerFilename,argv[nIndex]);
                }
                else
                {
                    gmMode = 0xFFFF;
                }
            }
            else if( _StdSTRCMPI(argv[nIndex],"/P")==0 )
            {
                nIndex++;
                if(nIndex<argc)
                {
                    strcpy(gbaPortId,argv[nIndex]);
                }
                else
                {
                    gmMode = 0xFFFF;
                }
            }
            else if( _StdSTRCMPI(argv[nIndex],"/W")==0 )
            {
                nIndex++;
                if(nIndex<argc)
                {
                    int nWaitMs = atoi(argv[nIndex]);
                    if(nWaitMs < 100 )
                    {
                        gnTimeoutMs = 100;
                    }
                    else if( nWaitMs > 60000 )
                    {
                        gnTimeoutMs = 60000;
                    }
                    else
                    {
                        gnTimeoutMs = nWaitMs;
                    }
                }
                else
                {
                    gmMode = 0xFFFF;
                }
            }
            else if( _StdSTRCMPI(argv[nIndex],"/R")==0 )
            {
                gfAddATpRun = true;
            }
            else if( _StdSTRCMPI(argv[nIndex],"/T")==0 )
            {
                if( argv[nIndex][1] == 'T' )
                {
                    gfAddTITLE = true;
                }
                else
                {
                    gfAddTITLE = false;
                }
            }
            else if( _StdSTRCMPI(argv[nIndex],"/H")==0 )
            {
                gfUseATpFWRH = true;
            }
        }

        switch( gmMode )
        {
        case 0: /* default mode */
            fpIn =fopen(argv[1],"rb");
            fpOut=fopen(argv[2],"wb");
            if(fpIn)
            {
                nRetVal = PlayeriseTheFile(fpIn,fpOut,argv[1]);
            }
            break;
        case 1: /* "auto00" */
            fpIn =fopen(argv[1],"rb");
            fpOut=fopen(argv[2],"wb");
            if(fpIn)
            {
                nRetVal = PlayeriseTheFileAuto00(fpIn,fpOut);
            }
            break;
        case 2: /* "datafile" */
            fpIn =fopen(argv[1],"rb");
            fpOut=fopen(argv[2],"wb");
            if(fpIn)
            {
                nRetVal = PlayeriseTheFileAsData(fpIn,fpOut);
            }
            break;
        case 3: /* "datafilegen" */
            fpOut=fopen(argv[2],"wb");
            if(fpOut)
            {
                nRetVal = PlayeriseTheFileAsDataGen(fpOut);
            }
            break;
        case 0xFFFF: /* args error */
            PrintUsage();
            break;
        }

        if(fpIn)fclose(fpIn);
        if(fpOut)fclose(fpOut);
    }
    else
    {
        PrintUsage();
    }
	return nRetVal;
}

/******************************************************************************/
/* END OF FILE*/
/******************************************************************************/
