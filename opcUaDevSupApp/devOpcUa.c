/*************************************************************************\
* Copyright (c) 2016 Helmholtz-Zentrum Berlin
*     fuer Materialien und Energie GmbH (HZB), Berlin, Germany.
*
*   This program is free software: you can redistribute it and/or modify
*   it under the terms of the GNU Lesser General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   This program is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with this program.  If not, see <http://www.gnu.org/licenses/>.
\*************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errlog.h>

// #EPICS LIBS
#include "dbAccess.h"
#include "dbEvent.h"
#include "dbScan.h"
#include "epicsExport.h"
#include <epicsTypes.h>
#include <initHooks.h>
#include "devSup.h"
#include "recSup.h"
#include "recGbl.h"
#include "aiRecord.h"
#include "aaiRecord.h"
#include "aoRecord.h"
#include "aaoRecord.h"
#include "biRecord.h"
#include "boRecord.h"
#include "longinRecord.h"
#include "longoutRecord.h"
#include "stringinRecord.h"
#include "stringoutRecord.h"
#include "mbbiRecord.h"
#include "mbboRecord.h"
#include "mbbiDirectRecord.h"
#include "mbboDirectRecord.h"
#include "asTrapWrite.h"
#include "alarm.h"
#include "asDbLib.h"
#include "cvtTable.h"
#include "menuFtype.h"
#include "menuAlarmSevr.h"
#include "menuAlarmStat.h"
#include "menuConvert.h"
//#define GEN_SIZE_OFFSET
#include "waveformRecord.h"
//#undef  GEN_SIZE_OFFSET

#include <devOpcUa.h>
#include <drvOpcUa.h>

#ifdef _WIN32
__inline int debug_level(dbCommon *prec) {
#else
inline int debug_level(dbCommon *prec) {
#endif
        return prec->tpro;
}

#define DEBUG_LEVEL debug_level((dbCommon*)prec)

static  long         read(dbCommon *prec);
static  long         write(dbCommon *prec);
static  void         outRecordCallback(CALLBACK *pcallback);
static  long         get_ioint_info(int cmd, dbCommon *prec, IOSCANPVT * ppvt);

//extern int OpcUaInitItem(char *OpcUaName, dbCommon* pRecord, OPCUA_ItemINFO** uaItem);
//extern void checkOpcUaVariables(void);

/*+**************************************************************************
 *		DSET functions
 **************************************************************************-*/
long init (int after);

static long init_longout  (struct longoutRecord* plongout);
static long write_longout (struct longoutRecord* plongout);

static long init_longin (struct longinRecord* pmbbid);
static long read_longin (struct longinRecord* pmbbid);
static long init_mbbiDirect (struct mbbiDirectRecord* pmbbid);
static long read_mbbiDirect (struct mbbiDirectRecord* pmbbid);
static long init_mbboDirect (struct mbboDirectRecord* pmbbid);
static long write_mbboDirect (struct mbboDirectRecord* pmbbid);
static long init_mbbi (struct mbbiRecord* pmbbid);
static long read_mbbi (struct mbbiRecord* pmbbid);
static long init_mbbo (struct mbboRecord* pmbbod);
static long write_mbbo (struct mbboRecord* pmbbod);
static long init_bi  (struct biRecord* pbi);
static long read_bi (struct biRecord* pbi);
static long init_bo  (struct boRecord* pbo);
static long write_bo (struct boRecord* pbo);
static long init_ai (struct aiRecord* pai);
static long read_ai (struct aiRecord* pai);
static long init_ao  (struct aoRecord* pao);
static long write_ao (struct aoRecord* pao);
static long init_stringin (struct stringinRecord* pstringin);
static long read_stringin (struct stringinRecord* pstringin);
static long init_stringout  (struct stringoutRecord* pstringout);
static long write_stringout (struct stringoutRecord* pstringout);

typedef struct {
   long number;
   DEVSUPFUN report;
   DEVSUPFUN init;
   DEVSUPFUN init_record;
   DEVSUPFUN get_ioint_info;
   DEVSUPFUN write_record;
} OpcUaDSET;

OpcUaDSET devlongoutOpcUa =    {5, NULL, init, init_longout, get_ioint_info, write_longout  };
epicsExportAddress(dset,devlongoutOpcUa);

OpcUaDSET devlonginOpcUa =     {5, NULL, init, init_longin, get_ioint_info, read_longin	 };
epicsExportAddress(dset,devlonginOpcUa);

OpcUaDSET devmbbiDirectOpcUa = {5, NULL, init, init_mbbiDirect, get_ioint_info, read_mbbiDirect};
epicsExportAddress(dset,devmbbiDirectOpcUa);

OpcUaDSET devmbboDirectOpcUa = {5, NULL, init, init_mbboDirect, get_ioint_info, write_mbboDirect};
epicsExportAddress(dset,devmbboDirectOpcUa);

OpcUaDSET devmbbiOpcUa = {5, NULL, init, init_mbbi, get_ioint_info, read_mbbi};
epicsExportAddress(dset,devmbbiOpcUa);

OpcUaDSET devmbboOpcUa = {5, NULL, init, init_mbbo, get_ioint_info, write_mbbo};
epicsExportAddress(dset,devmbboOpcUa);

OpcUaDSET devbiOpcUa = {5, NULL, init, init_bi, get_ioint_info, read_bi};
epicsExportAddress(dset,devbiOpcUa);

OpcUaDSET devboOpcUa = {5, NULL, init, init_bo, get_ioint_info, write_bo};
epicsExportAddress(dset,devboOpcUa);

OpcUaDSET devstringinOpcUa = {5, NULL, init, init_stringin, get_ioint_info, read_stringin};
epicsExportAddress(dset,devstringinOpcUa);

OpcUaDSET devstringoutOpcUa = {5, NULL, init, init_stringout, get_ioint_info, write_stringout};
epicsExportAddress(dset,devstringoutOpcUa);

struct aidset { // analog input dset
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
        DEVSUPFUN	init_record; 	    //returns: (-1,0)=>(failure,success)
	DEVSUPFUN	get_ioint_info;
        DEVSUPFUN	read_ai;    	    // 2 => success, don't convert)
                        // if convert then raw value stored in rval
	DEVSUPFUN	special_linconv;
} devaiOpcUa =         {6, NULL, init, init_ai, get_ioint_info, read_ai, NULL };
epicsExportAddress(dset,devaiOpcUa);

struct aodset { // analog input dset
	long		number;
	DEVSUPFUN	dev_report;
	DEVSUPFUN	init;
        DEVSUPFUN	init_record; 	    //returns: 2=> success, no convert
	DEVSUPFUN	get_ioint_info;
        DEVSUPFUN	write_ao;   	    //(0)=>(success )
	DEVSUPFUN	special_linconv;
} devaoOpcUa =         {6, NULL, init, init_ao, get_ioint_info, write_ao, NULL };
epicsExportAddress(dset,devaoOpcUa);

static long init_waveformRecord();
static long read_wf();
struct {
    long number;
    DEVSUPFUN report;
    DEVSUPFUN init;
    DEVSUPFUN init_Record;
    DEVSUPFUN get_ioint_info;
    DEVSUPFUN read;
    DEVSUPFUN special_linconv;
} devwaveformOpcUa = {
    6,
    NULL,
    NULL,
    init_waveformRecord,
    get_ioint_info,
    read_wf,
    NULL
};
epicsExportAddress(dset,devwaveformOpcUa);

/***************************************************************************
 *		Defines and Locals
 **************************************************************************-*/

static void opcuaMonitorControl (initHookState state)
{
    switch (state) {
    case initHookAfterFinishDevSup:
        OpcUaSetupMonitors();
        break;
    default:
        break;
    }
}

long init (int after)
{
    static int done = 0;

    if (!done) {
        done = 1;
        return (initHookRegister(opcuaMonitorControl));
    }
    return 0;
}

long init_common (dbCommon *prec, struct link* plnk, int recType, void *val, int inpType, void *inpVal)
{
    OPCUA_ItemINFO* uaItem;

    if(plnk->type != INST_IO) {
        long status;
        if (inpType) status = S_dev_badOutType; else status = S_dev_badInpType;
        recGblRecordError(status, prec, "devOpcUa (init_record) Bad INP/OUT link type (must be INST_IO)");
        return status;
    }                                                                                               

    uaItem =  (OPCUA_ItemINFO *) calloc(1,sizeof(OPCUA_ItemINFO));
    if (!uaItem) {
        long status = S_db_noMemory;
        recGblRecordError(status, prec, "devOpcUa (init_record) Out of memory, calloc() failed");
        return status;
    }

    if(strlen(plnk->value.instio.string) < ITEMPATHLEN) {
        strcpy(uaItem->ItemPath,plnk->value.instio.string);
        addOPCUA_Item(uaItem);
    }
    else {
        long status = S_db_badField;
        recGblRecordError(status, prec, "devOpcUa (init_record) INP/OUT field too long");
        return status;
    }

    prec->dpvt = uaItem;
    uaItem->recDataType=recType;
    uaItem->pRecVal = val;
    uaItem->prec = prec;
    uaItem->debug = prec->tpro;
    uaItem->flagLock = epicsMutexMustCreate();
    if(uaItem->debug >= 2)
        errlogPrintf("init_common %s\t PACT= %i, recVal=%p\n", prec->name, prec->pact, uaItem->pRecVal);
    // get OPC item type in init -> after

    if(inpType) { // is OUT record
        uaItem->inpDataType = inpType;
        uaItem->pInpVal = inpVal;
        callbackSetCallback(outRecordCallback, &(uaItem->callback));
        callbackSetUser(prec, &(uaItem->callback));
    }
    else {
        scanIoInit(&(uaItem->ioscanpvt));
    }
    return 0;
}

/***************************************************************************
    	    	    	    	Longin Support
 **************************************************************************-*/
long init_longin (struct longinRecord* prec)
{
    return init_common((dbCommon*)prec,&(prec->inp),epicsInt32T,(void*)&(prec->val),0,NULL);
}

long read_longin (struct longinRecord* prec)
{
    char buf[256];
    OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*)prec->dpvt;
    int flagSuppressWrite = uaItem->flagSuppressWrite;
    int udf   = prec->udf;
    int ret;
    
    epicsMutexLock(uaItem->flagLock);
    ret = read((dbCommon*)prec);
    if (!ret) {
        prec->val = (uaItem->varVal).Int32;
        if(DEBUG_LEVEL >= 2) errlogPrintf("read_longin     %s %s %d\n",prec->name,getTime(buf),prec->val);
        if(DEBUG_LEVEL >= 3) errlogPrintf("\tflagSuppressWrite %d->%d, UDF %d->%d \n",flagSuppressWrite,uaItem->flagSuppressWrite,udf,prec->udf);
    }
    epicsMutexUnlock(uaItem->flagLock);
    return ret;
}

/***************************************************************************
                                Longout Support
 ***************************************************************************/
long init_longout( struct longoutRecord* prec)
{
    return init_common((dbCommon*)prec,&(prec->out),epicsInt32T,(void*)&(prec->val),epicsInt32T,(void*)&(prec->val));
}

long write_longout (struct longoutRecord* prec)
{
    char buf[256];
    if(DEBUG_LEVEL >= 2) errlogPrintf("write_longout   %s %s RVAL:%d\n",prec->name,getTime(buf),prec->val);
    return write((dbCommon*)prec);
}

/*+**************************************************************************
                                MbbiDirect Support
 **************************************************************************-*/
long init_mbbiDirect (struct mbbiDirectRecord* prec)
{
    prec->mask <<= prec->shft;
    return init_common((dbCommon*)prec,&(prec->inp),epicsUInt32T,(void*)&(prec->rval),0,NULL);
}

long read_mbbiDirect (struct mbbiDirectRecord* prec)
{
    char buf[256];
    OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*)prec->dpvt;
    long ret;
    int flagSuppressWrite = uaItem->flagSuppressWrite;
    int udf   = prec->udf;

    ret = read((dbCommon*)prec);
    epicsMutexLock(uaItem->flagLock);
    if (!ret) {
        prec->rval = (uaItem->varVal).UInt32 & prec->mask;
        if(DEBUG_LEVEL >= 2) errlogPrintf("read_mbbiDirect %s %s VAL:%d RVAL:%d\n",prec->name,getTime(buf),prec->val,prec->rval);
        if(DEBUG_LEVEL >= 3) errlogPrintf("\tflagSuppressWrite %d->%d, UDF %d->%d \n",flagSuppressWrite,uaItem->flagSuppressWrite,udf,prec->udf);
    }
    epicsMutexUnlock(uaItem->flagLock);
    return ret;
}

/***************************************************************************
                                mbboDirect Support
 ***************************************************************************/
long init_mbboDirect( struct mbboDirectRecord* prec)
{
    return init_common((dbCommon*)prec,&(prec->out),epicsUInt32T,(void*)&(prec->rval),epicsUInt32T,(void*)&(prec->val));
}

long write_mbboDirect (struct mbboDirectRecord* prec)
{
    char buf[256];
    prec->rval = prec->rval & prec->mask;
    if(DEBUG_LEVEL >= 2) errlogPrintf("write_mbboDirect %s %s RVAL:%d\n",prec->name,getTime(buf),prec->rval);
    return write((dbCommon*)prec);
}
/*+**************************************************************************
                                Mbbi Support
 **************************************************************************-*/
long init_mbbi (struct mbbiRecord* prec)
{
    prec->mask <<= prec->shft;
    return init_common((dbCommon*)prec,&(prec->inp),epicsUInt32T,(void*)&(prec->rval),0,NULL);
}

long read_mbbi (struct mbbiRecord* prec)
{
    char buf[256];
    OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*)prec->dpvt;
    long ret;
    int flagSuppressWrite = uaItem->flagSuppressWrite;
    int udf   = prec->udf;

    epicsMutexLock(uaItem->flagLock);
    ret = read((dbCommon*)prec);
    if (!ret) {
        prec->rval = (uaItem->varVal).UInt32 & prec->mask;
        if(DEBUG_LEVEL >= 2) errlogPrintf("read_mbbi %s %s VAL:%d RVAL:%d\n",prec->name,getTime(buf),prec->val,prec->rval);
        if(DEBUG_LEVEL >= 3) errlogPrintf("\tflagSuppressWrite %d->%d, UDF %d->%d \n",flagSuppressWrite,uaItem->flagSuppressWrite,udf,prec->udf);
    }
    epicsMutexUnlock(uaItem->flagLock);
    return ret;
}

/***************************************************************************
                                mbbo Support
 ***************************************************************************/
long init_mbbo( struct mbboRecord* prec)
{
    prec->mask <<= prec->shft;
    return init_common((dbCommon*)prec,&(prec->out),epicsUInt32T,(void*)&(prec->rval),epicsUInt32T,(void*)&(prec->rval));
}

long write_mbbo (struct mbboRecord* prec)
{
    char buf[256];
    prec->rval = prec->rval & prec->mask;
    if(DEBUG_LEVEL >= 2) errlogPrintf("write_mbbo      %s %s RVAL:%d\n",prec->name,getTime(buf),prec->rval);
    return write((dbCommon*)prec);
}

/*+**************************************************************************
                                Bi Support
 **************************************************************************-*/
long init_bi (struct biRecord* prec)
{
    return init_common((dbCommon*)prec,&(prec->inp),epicsUInt32T,(void*)&(prec->rval),0,NULL);
}

long read_bi (struct biRecord* prec)
{
    char buf[256];
    OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*)prec->dpvt;
    int flagSuppressWrite = uaItem->flagSuppressWrite;
    int udf   = prec->udf;
    long ret = 0;
	
    epicsMutexLock(uaItem->flagLock);
    ret = read((dbCommon*)prec);
    if (!ret) {
        prec->rval = (uaItem->varVal).UInt32;
        if(DEBUG_LEVEL >= 2) errlogPrintf("read_bi         %s %s RVAL:%d\n",prec->name,getTime(buf),prec->rval);
        if(DEBUG_LEVEL >= 3) errlogPrintf("\tflagSuppressWrite %d->%d, UDF %d->%d \n",flagSuppressWrite,uaItem->flagSuppressWrite,udf,prec->udf);
    }
    epicsMutexUnlock(uaItem->flagLock);
    return ret;
}


/***************************************************************************
                                bo Support
 ***************************************************************************/
long init_bo( struct boRecord* prec)
{
    prec->mask=1;
    return init_common((dbCommon*)prec,&(prec->out),epicsUInt32T,(void*)&(prec->rval),epicsUInt32T,(void*)&(prec->rval));
}

long write_bo (struct boRecord* prec)
{
    char buf[256];
    if(DEBUG_LEVEL >= 2) errlogPrintf("write_bo        %s %s RVAL:%d\n",prec->name,getTime(buf),prec->rval);
    return write((dbCommon*)prec);
}

/*+**************************************************************************
                                ao Support
 **************************************************************************-*/
long init_ao (struct aoRecord* prec)
{
    long ret;
    if(prec->linr == menuConvertNO_CONVERSION)
        ret = init_common((dbCommon*)prec,&(prec->out),epicsFloat64T,(void*)&(prec->oval),epicsFloat64T,(void*)&(prec->val));
    else
        ret = init_common((dbCommon*)prec,&(prec->out),epicsInt32T,(void*)&(prec->rval),epicsFloat64T,(void*)&(prec->val));
    if(DEBUG_LEVEL >= 2) {
        OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*)prec->dpvt;
        errlogPrintf("init_ao %s\t VAL %f RVAL %d OPCVal %f\n",prec->name,prec->val,prec->rval,(uaItem->varVal).Double);
    }
    return ret;
}

long write_ao (struct aoRecord* prec)
{
    char buf[256];
    if(DEBUG_LEVEL >= 2) {
        OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*)prec->dpvt;
        errlogPrintf("write_ao %s %s VAL %f RVAL %d OPCVal %f\n",prec->name,getTime(buf),prec->val,prec->rval,(uaItem->varVal).Double);
    }
    return write((dbCommon*)prec);
}
/***************************************************************************
                                ai Support
 **************************************************************************
  In case of LINR == NO_CONVERSION: read() set the VAL field direct and perform
  NO conversion. This is to avoid loss of data for double values from the OPC

  In other cases for LINR the RVAL field is set + record performs the conversion
  In case of double values from the OPC there may be a loss off data caused by the
  integer conversion!
*/
long init_ai (struct aiRecord* prec)
{
    if(prec->linr == menuConvertNO_CONVERSION)
        return init_common((dbCommon*)prec,&(prec->inp),epicsFloat64T,(void*)&(prec->val),0,NULL);
    else
        return init_common((dbCommon*)prec,&(prec->inp),epicsInt32T,(void*)&(prec->rval),0,NULL);
}

long read_ai (struct aiRecord* prec)
{
    char buf[256];
    double newVal;
    long ret;
    OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*) prec->dpvt;
    int flagSuppressWrite = uaItem->flagSuppressWrite;
    int udf   = prec->udf;

    epicsMutexLock(uaItem->flagLock);
    ret = read((dbCommon*)prec);
    if (!ret) {
        if(prec->linr == menuConvertNO_CONVERSION) {
            newVal = (uaItem->varVal).Double;
            prec->udf = FALSE;	// aiRecord process doesn't set udf field in case of no convert!
            if( (prec->smoo > 0) && (! prec->init) ) {
                prec->val = newVal * (1 - prec->smoo) + prec->val * prec->smoo;
            }
            else {
                prec->val = newVal;
            }
            if(DEBUG_LEVEL>= 2) errlogPrintf("read_ai         %s %s\n\tbuf:%f VAL:%f\n", prec->name,getTime(buf),newVal,prec->val);
            if(DEBUG_LEVEL >= 3) errlogPrintf("\tflagSuppressWrite %d->%d, UDF %d->%d  ret: 2 NO_CONVERSION\n",flagSuppressWrite,uaItem->flagSuppressWrite,udf,prec->udf);
            ret = 2;
        }
        else {
            prec->rval = (uaItem->varVal).Int32;
            if(DEBUG_LEVEL >= 2) errlogPrintf("read_ai         %s %s\n\tbuf:%f RVAL:%d\n", prec->name,getTime(buf),(uaItem->varVal).Double,prec->rval);
            if(DEBUG_LEVEL >= 3) errlogPrintf("\tflagSuppressWrite %d->%d, UDF %d->%d ret: 0 LINR=%d\n",flagSuppressWrite,uaItem->flagSuppressWrite,udf,prec->udf,prec->linr);
        }
    }
    epicsMutexUnlock(uaItem->flagLock);
    return ret;
}

/***************************************************************************
                                Stringin Support
 **************************************************************************-*/
long init_stringin (struct stringinRecord* prec)
{
    return init_common((dbCommon*)prec,&(prec->inp),epicsOldStringT,(void*)&(prec->val),0,NULL);
}

long read_stringin (struct stringinRecord* prec)
{
    char buf[256];
    OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*)prec->dpvt;
    int flagSuppressWrite = uaItem->flagSuppressWrite;
    int udf   = prec->udf;
    long ret = 0;

    epicsMutexLock(uaItem->flagLock);
    ret = read((dbCommon*)prec);
    if( !ret ) {
        strncpy(prec->val,(uaItem->varVal).cString,40);    // string length: see stringin.h
        prec->udf = FALSE;	// stringinRecord process doesn't set udf field in case of no convert!
    }
    epicsMutexUnlock(uaItem->flagLock);
    if(DEBUG_LEVEL >= 2) errlogPrintf("write_stringin  %s %s VAL:%s\n",prec->name,getTime(buf),prec->val);
    if(DEBUG_LEVEL >= 3) errlogPrintf("\tflagSuppressWrite %d->%d, UDF %d->%d \n",flagSuppressWrite,uaItem->flagSuppressWrite,udf,prec->udf);
    return ret;
}

/***************************************************************************
                                Stringout Support
 ***************************************************************************/
long init_stringout( struct stringoutRecord* prec)
{
    return init_common((dbCommon*)prec,&(prec->out),epicsStringT,(void*)&(prec->val),epicsStringT,(void*)&(prec->val));
}

long write_stringout (struct stringoutRecord* prec)
{
    char buf[256];
    if(DEBUG_LEVEL >= 2) errlogPrintf("write_stringout %s %s VAL:%s\n",prec->name,getTime(buf),prec->val);
    return write((dbCommon*)prec);
}

/***************************************************************************
    	    	    	    	Waveform Support
 **************************************************************************-*/
long init_waveformRecord(struct waveformRecord* prec)
{
    long ret = 0;
    int recType=0;
    OPCUA_ItemINFO* pOpcUa2Epics=NULL;
    prec->dpvt = NULL;
    switch(prec->ftvl) {
        case menuFtypeSTRING: recType = epicsOldStringT; break;
        case menuFtypeCHAR  : recType = epicsInt8T; break;
        case menuFtypeUCHAR : recType = epicsUInt8T; break;
        case menuFtypeSHORT : recType = epicsInt16T; break;
        case menuFtypeUSHORT: recType = epicsUInt16T; break;
        case menuFtypeLONG  : recType = epicsInt32T; break;
        case menuFtypeULONG : recType = epicsUInt32T; break;
        case menuFtypeFLOAT : recType = epicsFloat32T; break;
        case menuFtypeDOUBLE: recType = epicsFloat64T; break;
        case menuFtypeENUM  : recType = epicsEnum16T; break;
    }
    ret = init_common((dbCommon*)prec,&(prec->inp),recType,(void*)prec->bptr,0,NULL);
    pOpcUa2Epics = (OPCUA_ItemINFO*)prec->dpvt;
    if(pOpcUa2Epics != NULL) {
        pOpcUa2Epics->isArray = 1;
        pOpcUa2Epics->arraySize = prec->nelm;
    }
    return  ret;
}

long read_wf(struct waveformRecord *prec)
{
    char buf[256];
    int udf   = prec->udf;
    int ret = 0;
    OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*)prec->dpvt;
    int flagSuppressWrite = uaItem->flagSuppressWrite;
    uaItem->debug = prec->tpro;
    
    epicsMutexLock(uaItem->flagLock);
    ret = read((dbCommon*)prec);
    if(! ret) {
        prec->nord = uaItem->arraySize;
        uaItem->arraySize = prec->nelm;
        prec->udf=FALSE;
    }
    epicsMutexUnlock(uaItem->flagLock);
    if(DEBUG_LEVEL >= 2) errlogPrintf("read_wf         %s %s NELM:%d\n",prec->name,getTime(buf),prec->nelm);
    if(DEBUG_LEVEL >= 3) errlogPrintf("\t  flagSuppressWrite %d -> %d, UDF%d -> %d \n",flagSuppressWrite,uaItem->flagSuppressWrite,udf,prec->udf);
    return ret;
}

/* callback service routine */
static void outRecordCallback(CALLBACK *pcallback) {
    char buf[256];
    dbCommon *prec;
    callbackGetUser(prec, pcallback);
    if(prec) {
        if(DEBUG_LEVEL >= 2) errlogPrintf("outRecordCallback: %s %s\tdbProcess\n", prec->name,getTime(buf));
        dbProcess(prec);
    }
}

static long get_ioint_info(int cmd, dbCommon *prec, IOSCANPVT * ppvt) {
    OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*)prec->dpvt;
    if(!prec || !prec->dpvt)
        return 1;
    *ppvt = uaItem->ioscanpvt;
    if(DEBUG_LEVEL >= 2) errlogPrintf("get_ioint_info %s %s I/O event list - ioscanpvt=%p\n",
                     prec->name, cmd?"removed from":"added to", *ppvt);
    return 0;
}

/* Setup commons for all record types: debug level, flagSuppressWrite, alarms. Don't deal with the value! */
static long read(dbCommon * prec) {
    long ret = 0;
    OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*)prec->dpvt;
    if(!uaItem) {
        errlogPrintf("%s read error uaItem = 0\n", prec->name);
        ret = 1;
    }

    uaItem->debug = prec->tpro;

    if(uaItem->flagSuppressWrite == 1) {      // SCAN=I/O Intr: processed after callback just clear flag.
        uaItem->flagSuppressWrite = 0;
    }

    ret = uaItem->stat;
    if(ret) {
        recGblSetSevr(prec,menuAlarmStatREAD,menuAlarmSevrINVALID);
    }
    else {
        prec->udf=FALSE;
    }
    return ret;
}

static long write(dbCommon *prec) {
    long ret = 0;
    OPCUA_ItemINFO* uaItem = (OPCUA_ItemINFO*)prec->dpvt;
    uaItem->debug = prec->tpro;
    
    if(DEBUG_LEVEL >= 3) errlogPrintf("\twrite()            UDF:%i, flagSuppressWrite=%i\n",prec->udf,uaItem->flagSuppressWrite);

    if(!uaItem) {
        if(DEBUG_LEVEL > 0) errlogPrintf("\twrite %s\t error\n", prec->name);
        ret = -1;
    }
    else {
        epicsMutexLock(uaItem->flagLock);
        if(uaItem->flagSuppressWrite == 1) {
                uaItem->flagSuppressWrite = 0;
                epicsMutexUnlock(uaItem->flagLock);
        }
        else {
            uaItem->flagSuppressWrite = 1;
            epicsMutexUnlock(uaItem->flagLock);
            ret = OpcUaWriteItems(uaItem);
        }
    }
    if(DEBUG_LEVEL >= 3) errlogPrintf("\tOpcUaWriteItems() Done set flagSuppressWrite=%i\n",uaItem->flagSuppressWrite);

    if(ret==0) {
        prec->udf=FALSE;
    }
    else {
        recGblSetSevr(prec,menuAlarmStatWRITE,menuAlarmSevrINVALID);
    }
    return ret;
}
