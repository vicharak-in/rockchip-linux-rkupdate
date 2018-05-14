#include "RKAndroidDevice.h"

CHAR CRKAndroidDevice::FindIDBlock(char pos,char &IDBlockPos)
{
	BYTE bData[SECTOR_SIZE*4];
	int iRet = ERR_SUCCESS;
	int i = FindValidBlocks(pos,1);
	if ( i<0 )
		return -1;
	for( ;i<IDBLOCK_TOP;i=FindValidBlocks(i+1,1) )
	{
		if ( i<0 )
		{
			break;
		}
		memset(bData,0,SECTOR_SIZE*4);
		iRet = m_pComm->RKU_ReadSector( i*m_flashInfo.uiSectorPerBlock, 4, bData);
		
		if(ERR_SUCCESS!=iRet)
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:FindIDBlock-->RKU_ReadSector %x failed,RetCode(%d)"),i*m_flashInfo.uiSectorPerBlock,iRet);
			}
			return -1;//数据读取失败
		}
		RKANDROID_IDB_SEC0 *pSec0;
		pSec0 = (RKANDROID_IDB_SEC0 *)bData;
		P_RC4((BYTE *)pSec0,SECTOR_SIZE);
 //       if (bData[514]==0x69)//0x69='i'
		if (pSec0->dwTag==0x0FF0AA55)
		{
			//增加判断tag
			RKANDROID_IDB_SEC1 *pSec;
			pSec = (RKANDROID_IDB_SEC1 *)(bData+SECTOR_SIZE);
			if (pSec->uiChipTag==0x38324B52)
			{
				IDBlockPos = i;
				return 0;//找到idb
			}
			else
			{
				continue;//tag不对
			}
        }
		
	}
	return -1;
}
char CRKAndroidDevice::FindAllIDB()
{
	char i,iIndex,iStart=0;
	CHAR iRet;
	m_oldIDBCounts = 0;
	for( i=0; i<5; i++)
	{
		iRet = FindIDBlock( iStart,iIndex );
		if ( iRet<0 )
		{
			return m_oldIDBCounts;
		}
		
		m_idBlockOffset[i] = iIndex;
		m_oldIDBCounts++;
		iStart = iIndex+1;
	}

	return m_oldIDBCounts;
}
bool CRKAndroidDevice::ReserveIDBlock(char iBlockIndex,char iIdblockPos)
{
	char i;
	CHAR iRet;
	for(i=iIdblockPos; i<IDB_BLOCKS; i++)
	{
		iRet = iBlockIndex = FindValidBlocks(iBlockIndex,m_flashInfo.usPhyBlokcPerIDB);
		if( iRet<0 )
		{
			return false;
		}
		m_idBlockOffset[i] = iBlockIndex;
		iBlockIndex += m_flashInfo.usPhyBlokcPerIDB;
	}	
	return true;
}
bool CRKAndroidDevice::CalcIDBCount()
{
	bool bRet;
	UINT uiIdSectorNum;//ID BLOCK基本区

	bRet = GetLoaderSize();
	if (!bRet)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:CalcIDBCount-->GetLoaderSize failed"));
		}
		return false;
	}
	bRet = GetLoaderDataSize();
	if (!bRet)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:CalcIDBCount-->GetLoaderDataSize failed"));
		}
		return false;
	}
	uiIdSectorNum = 4 + m_usFlashDataSec + m_usFlashBootSec;
	
	m_flashInfo.uiSecNumPerIDB = uiIdSectorNum;
	m_flashInfo.usPhyBlokcPerIDB = CALC_UNIT(uiIdSectorNum,m_flashInfo.usValidSecPerBlock);
	return true;
}
bool CRKAndroidDevice::OffsetIDBlock(char pos)
{
	int iBlockIndex,i;
	for ( i=0;i<m_flashInfo.usPhyBlokcPerIDB;i++)
	{
		m_flashInfo.blockState[m_idBlockOffset[pos]+i]=1;//标记上坏块
	}
	iBlockIndex = m_idBlockOffset[pos]+m_flashInfo.usPhyBlokcPerIDB;
	for(i=pos; i<5; i++)
	{
		iBlockIndex = FindValidBlocks(iBlockIndex, m_flashInfo.usPhyBlokcPerIDB);
		if ( iBlockIndex<0 )
		{
			return false;
		}
		m_idBlockOffset[i] = iBlockIndex;
		iBlockIndex += m_flashInfo.usPhyBlokcPerIDB;
	}
	
	return true;
}
bool CRKAndroidDevice::GetLoaderSize()
{
	if (!m_pImage)
	{
		return false;
	}
	CHAR index;
	bool bRet;
	tchar loaderName[]=_T("FlashBoot");
	index = m_pImage->m_bootObject->GetIndexByName(ENTRYLOADER,loaderName);
	if (index==-1)
	{
		return false;
	}
	DWORD dwDelay;
	bRet = m_pImage->m_bootObject->GetEntryProperty(ENTRYLOADER,index,m_dwLoaderSize,dwDelay);
	if (bRet)
	{
		m_usFlashBootSec = PAGEALIGN(BYTE2SECTOR(m_dwLoaderSize))*4;
	}
	return bRet;
}
bool CRKAndroidDevice::GetLoaderDataSize()
{
	if (!m_pImage)
	{
		return false;
	}
	CHAR index;
	bool bRet;
	tchar loaderName[]=_T("FlashData");
	index = m_pImage->m_bootObject->GetIndexByName(ENTRYLOADER,loaderName);
	if (index==-1)
	{
		return false;
	}
	DWORD dwDelay;
	bRet = m_pImage->m_bootObject->GetEntryProperty(ENTRYLOADER,index,m_dwLoaderDataSize,dwDelay);
	if (bRet)
	{
		m_usFlashDataSec = PAGEALIGN(BYTE2SECTOR(m_dwLoaderDataSize))*4;
	}
	return bRet;
}

CRKAndroidDevice::CRKAndroidDevice(STRUCT_RKDEVICE_DESC &device):CRKDevice(device)
{
	m_oldSec0 = NULL;
	m_oldSec1 = NULL;
	m_oldSec2 = NULL;
	m_oldSec3 = NULL;
	m_dwLoaderSize = 0;
	m_dwLoaderDataSize = 0;
	m_oldIDBCounts = 0;
	m_usFlashBootSec = 0;
	m_usFlashDataSec = 0;
	m_dwBackupOffset = 0xFFFFFFFF;
	m_paramBuffer = NULL;
	m_pCallback = NULL;
	m_pProcessCallback = NULL;
}
CRKAndroidDevice::~CRKAndroidDevice()
{
	if (m_oldSec0)
	{
		delete m_oldSec0;
		m_oldSec0 = NULL;
	}
	if (m_oldSec1)
	{
		delete m_oldSec1;
		m_oldSec1 = NULL;
	}
	if (m_oldSec2)
	{
		delete m_oldSec2;
		m_oldSec2 = NULL;
	}
	if (m_oldSec3)
	{
		delete m_oldSec3;
		m_oldSec3 = NULL;
	}
	if (m_paramBuffer)
	{
		delete []m_paramBuffer;
		m_paramBuffer = NULL;
	}
}
bool CRKAndroidDevice::GetOldSectorData()
{
	BYTE bData[SECTOR_SIZE*4];

	if (m_oldIDBCounts<=0)
	{
		return false;
	}

	if (!GetWriteBackData(m_oldIDBCounts,bData))
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:GetOldSectorData-->GetWriteBackData failed"));
		}
		return false;//数据读取失败
	}
	PBYTE pSec;
	if (!m_oldSec0)
	{
		m_oldSec0 = new RKANDROID_IDB_SEC0;
	}
	pSec = (PBYTE)(m_oldSec0);
	memset(pSec,0,SECTOR_SIZE);
	memcpy(pSec,bData,SECTOR_SIZE);
	P_RC4(pSec,SECTOR_SIZE);
	
	if (!m_oldSec1)
	{
		m_oldSec1 = new RKANDROID_IDB_SEC1;
	}
	pSec = (PBYTE)(m_oldSec1);
	memset(pSec,0,SECTOR_SIZE);
	memcpy(pSec,bData+512,SECTOR_SIZE);
	
	if (!m_oldSec2)
	{
		m_oldSec2 = new RKANDROID_IDB_SEC2;
	}
	pSec = (PBYTE)(m_oldSec2);
	memset(pSec,0,SECTOR_SIZE);
	memcpy(pSec,bData+512*2,SECTOR_SIZE);
	P_RC4(pSec,SECTOR_SIZE);

	if (!m_oldSec3)
	{
		m_oldSec3 = new RKANDROID_IDB_SEC3;
	}
	pSec = (PBYTE)(m_oldSec3);
	memset(pSec,0,SECTOR_SIZE);
	memcpy(pSec,bData+512*3,SECTOR_SIZE);
	P_RC4(pSec,SECTOR_SIZE);

	return true;

}
bool CRKAndroidDevice::MakeSector0(PBYTE pSector)
{
	PRKANDROID_IDB_SEC0 pSec0;
	memset(pSector,0,SECTOR_SIZE);
	pSec0 = (PRKANDROID_IDB_SEC0)pSector;
	
	pSec0->dwTag = 0x0FF0AA55;
	if (m_pImage->m_bootObject->Rc4DisableFlag)
	{
		pSec0->uiRc4Flag = 1;
	}
	pSec0->usBootCode1Offset = 0x4;
	pSec0->usBootCode2Offset = 0x4;
	pSec0->usBootDataSize = m_usFlashDataSec;
	pSec0->usBootCodeSize = m_usFlashDataSec + m_usFlashBootSec;
	
//	pSec0->usCrc = CRC_16(pSector,SECTOR_SIZE-2);
	return true;
}

void CRKAndroidDevice::MakeSector1(PBYTE pSector)
{
	PRKANDROID_IDB_SEC1 pSec1;
	memset(pSector,0,SECTOR_SIZE);
	pSec1 = (PRKANDROID_IDB_SEC1)pSector;
	USHORT usSysReserved;
	if ((m_idBlockOffset[4]+1)%12==0)
	{
		usSysReserved=m_idBlockOffset[4]+13;
	}
	else
	{
		usSysReserved =((m_idBlockOffset[4]+1)/12+1)*12;
	}
	if (usSysReserved>IDBLOCK_TOP)
	{
		usSysReserved = IDBLOCK_TOP;
	}
	pSec1->usSysReservedBlock = usSysReserved;


	pSec1->usDisk0Size = 0;
	pSec1->usDisk1Size = 0;
	pSec1->usDisk2Size = 0;
	pSec1->usDisk3Size = 0;
	pSec1->uiChipTag = 0x38324B52;
	pSec1->uiMachineId = 0;
	pSec1->usLoaderYear = UshortToBCD(((STRUCT_RKTIME)m_pImage->m_bootObject->ReleaseTime).usYear);
	pSec1->usLoaderDate = ByteToBCD(((STRUCT_RKTIME)m_pImage->m_bootObject->ReleaseTime).ucMonth);
	pSec1->usLoaderDate = (pSec1->usLoaderDate<<8)|ByteToBCD(((STRUCT_RKTIME)m_pImage->m_bootObject->ReleaseTime).ucDay);
	pSec1->usLoaderVer =  m_pImage->m_bootObject->Version;
	if (m_oldSec1)
	{
		pSec1->usLastLoaderVer = m_oldSec1->usLoaderVer;
		pSec1->usReadWriteTimes = m_oldSec1->usReadWriteTimes+1;
	}
	else
	{
		pSec1->usLastLoaderVer = 0;
		pSec1->usReadWriteTimes = 1;
	}
	pSec1->uiFlashSize = m_flashInfo.uiFlashSize*2*1024;
	pSec1->usBlockSize = m_flashInfo.usBlockSize*2;
	pSec1->bPageSize = m_flashInfo.uiPageSize*2;
	pSec1->bECCBits = m_flashInfo.bECCBits;
	pSec1->bAccessTime = m_flashInfo.bAccessTime;

	pSec1->usFlashInfoLen = 0;
	pSec1->usFlashInfoOffset = 0;
	

	pSec1->usIdBlock0 = m_idBlockOffset[0];
	pSec1->usIdBlock1 = m_idBlockOffset[1];
	pSec1->usIdBlock2 = m_idBlockOffset[2];
	pSec1->usIdBlock3 = m_idBlockOffset[3];
	pSec1->usIdBlock4 = m_idBlockOffset[4];
}
bool CRKAndroidDevice::MakeSector2(PBYTE pSector)
{
	PRKANDROID_IDB_SEC2 pSec2;
	pSec2 = (PRKANDROID_IDB_SEC2)pSector;

	pSec2->usInfoSize = 0;
	memset(pSec2->bChipInfo,0,CHIPINFO_LEN);
	
	if (m_oldSec2)
	{
		memcpy(pSec2->reserved,m_oldSec2->reserved,RKANDROID_SEC2_RESERVED_LEN);
		pSec2->usSec3CustomDataOffset = m_oldSec2->usSec3CustomDataOffset;
		pSec2->usSec3CustomDataSize = m_oldSec2->usSec3CustomDataSize;
	}
	else
	{
		memset(pSec2->reserved,0,RKANDROID_SEC2_RESERVED_LEN);
		pSec2->usSec3CustomDataOffset = m_usWriteBackCustomDataOffset;
		pSec2->usSec3CustomDataSize = m_usWriteBackCustomDataSize;
	}
	
	strcpy(pSec2->szVcTag,"VC");
	strcpy(pSec2->szCrcTag,"CRC");
	return true;
}
bool CRKAndroidDevice::MakeSector3(PBYTE pSector)
{
	PRKANDROID_IDB_SEC3 pSec3;
	memset(pSector,0,SECTOR_SIZE);
	pSec3 = (PRKANDROID_IDB_SEC3)pSector;

	if (m_oldSec3)
	{
		memcpy(pSector,(PBYTE)m_oldSec3,SECTOR_SIZE);
	}
	else
	{
		if (m_backupBuffer)
		{
			memcpy(pSector,(PBYTE)m_backupBuffer,SECTOR_SIZE);
		}
	}


	if (m_uid)
	{
		if ((m_oldSec3)||(m_backupBuffer))
		{
			if (!CheckUid(pSec3->uidSize,pSec3->uid))
			{
				pSec3->uidSize = RKDEVICE_UID_LEN;
				memcpy(pSec3->uid,m_uid,RKDEVICE_UID_LEN);
			}
		}
		else
		{
			pSec3->uidSize = RKDEVICE_UID_LEN;
			memcpy(pSec3->uid,m_uid,RKDEVICE_UID_LEN);
		}
	}

	return true; 
}
int CRKAndroidDevice::MakeIDBlockData(PBYTE lpIDBlock)
{
	if (m_pLog)
	{
		m_pLog->Record(_T("INFO:MakeIDBlockData in"));
	}
 	RKANDROID_IDB_SEC0 sector0Info;
	RKANDROID_IDB_SEC1 sector1Info;
	RKANDROID_IDB_SEC2 sector2Info;
	RKANDROID_IDB_SEC3 sector3Info;

	if (!m_pImage)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:MakeIDBlockData-->Image is invalid"));
		}
		return -1;
	}
	CHAR index;
	tchar loaderCodeName[]=_T("FlashBoot");
	index = m_pImage->m_bootObject->GetIndexByName(ENTRYLOADER,loaderCodeName);
	if (index==-1)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:MakeIDBlockData-->Get LoaderCode Entry failed"));
		}
		return -2;
	}
	PBYTE loaderCodeBuffer;
	loaderCodeBuffer = new BYTE[m_dwLoaderSize];
	memset(loaderCodeBuffer,0,m_dwLoaderSize);
	if ( !m_pImage->m_bootObject->GetEntryData(ENTRYLOADER,index,loaderCodeBuffer) )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:MakeIDBlockData-->Get LoaderCode Data failed"));
		}
		delete []loaderCodeBuffer;
		return -3;
	}

	tchar loaderDataName[]=_T("FlashData");
	index = m_pImage->m_bootObject->GetIndexByName(ENTRYLOADER,loaderDataName);
	if (index==-1)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:MakeIDBlockData-->Get LoaderData Entry failed"));
		}
		delete []loaderCodeBuffer;
		return -4;
	}
	PBYTE loaderDataBuffer;
	loaderDataBuffer = new BYTE[m_dwLoaderDataSize];
	memset(loaderDataBuffer,0,m_dwLoaderDataSize);
	if ( !m_pImage->m_bootObject->GetEntryData(ENTRYLOADER,index,loaderDataBuffer) )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:MakeIDBlockData-->Get LoaderData Data failed"));
		}
		delete []loaderDataBuffer;
		delete []loaderCodeBuffer;
		return -5;
	}
		
	////////////// 生成数据 ////////////////////////////////////////////
	UINT i;
	MakeSector0((PBYTE)&sector0Info);
	MakeSector1((PBYTE)&sector1Info);
	if (!MakeSector2((PBYTE)&sector2Info))
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:MakeIDBlockData-->MakeSector2 failed"));
		}
		return -6;
	}
	if (!MakeSector3((PBYTE)&sector3Info))
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:MakeIDBlockData-->MakeSector3 failed"));
		}
		return -7;
	}
	sector2Info.usSec0Crc = CRC_16((PBYTE)&sector0Info,SECTOR_SIZE);
	sector2Info.usSec1Crc = CRC_16((PBYTE)&sector1Info,SECTOR_SIZE);
	sector2Info.usSec3Crc = CRC_16((PBYTE)&sector3Info,SECTOR_SIZE);

	memcpy(lpIDBlock, &sector0Info, SECTOR_SIZE);
	memcpy(lpIDBlock+SECTOR_SIZE, &sector1Info, SECTOR_SIZE);
//	memcpy(lpIDBlock+SECTOR_SIZE*2, &sector2Info, SECTOR_SIZE);
	memcpy(lpIDBlock+SECTOR_SIZE*3, &sector3Info, SECTOR_SIZE);

	if (sector0Info.uiRc4Flag)
	{//close rc4 encryption
		for (i=0;i<m_dwLoaderDataSize/SECTOR_SIZE;i++)
		{
			P_RC4(loaderDataBuffer+SECTOR_SIZE*i,SECTOR_SIZE);
		}
		for (i=0;i<m_dwLoaderSize/SECTOR_SIZE;i++)
		{
			P_RC4(loaderCodeBuffer+SECTOR_SIZE*i,SECTOR_SIZE);
		}
	}
	
	memcpy(lpIDBlock+SECTOR_SIZE*4, loaderDataBuffer, m_dwLoaderDataSize);
	memcpy(lpIDBlock+SECTOR_SIZE*(4+m_usFlashDataSec), loaderCodeBuffer, m_dwLoaderSize);
	
	sector2Info.uiBootCodeCrc = CRC_32((PBYTE)(lpIDBlock+SECTOR_SIZE*4),sector0Info.usBootCodeSize*SECTOR_SIZE);
	memcpy(lpIDBlock+SECTOR_SIZE*2, &sector2Info, SECTOR_SIZE);

    for(i=0; i<4; i++)
	{
        if(i == 1)
		{
            continue;
        }
        else
		{
			P_RC4(lpIDBlock+SECTOR_SIZE*i, SECTOR_SIZE);
        }
    }
	
	delete []loaderDataBuffer;
	delete []loaderCodeBuffer;
	if (m_pLog)
	{
		m_pLog->Record(_T("INFO:MakeIDBlockData out"));
	}
	return 0;
}

bool CRKAndroidDevice::MakeSpareData(PBYTE lpIDBlock,DWORD dwSectorNum,PBYTE lpSpareBuffer)
{
	int i = 0;
	BYTE bchOutBuf[512+3+13];
	BYTE bchInBuf[512+3];

	for (i=0; i<dwSectorNum; i++)
	{
		memcpy(bchInBuf, lpIDBlock+512*i, 512);
		bchInBuf[514] = ((i==0) ? 'i' : 0xff);
		bchInBuf[512] = 0xff;
		bchInBuf[513] = 0xff;
		//对bchInBuf进行BCH编码（生成13个字节的编码），生成的bchOutBuf(528 Bytes)
		//由bchInBuf(515 Bytes)与BCH编码(13 Bytes)组成
		bch_encode(bchInBuf, bchOutBuf);
		memcpy(lpSpareBuffer+i*16+3, bchOutBuf+515, 13);
	}
    lpSpareBuffer[2] = 'i';
	return true;
}

int CRKAndroidDevice::WriteIDBlock(PBYTE lpIDBlock,DWORD dwSectorNum,bool bErase)
{
	if (m_pLog)
	{
		m_pLog->Record(_T("INFO:WriteIDBlock in"));
	}
	STRUCT_END_WRITE_SECTOR end_write_sector_data;
	BYTE writeBuf[8*SECTOR_SIZE];
	UINT uiOffset,uiTotal,uiWriteByte,uiCrc;
	int iRet,i,nTryCount=3;
	uiTotal = dwSectorNum*SECTOR_SIZE;
	uiCrc = CRC_32(lpIDBlock,uiTotal);
	end_write_sector_data.uiSize = uiTotal;
	end_write_sector_data.uiCrc = uiCrc;
	for(i=0;i<5;i++)
		end_write_sector_data.uiBlock[i] = m_idBlockOffset[i];
	while (nTryCount>0)
	{
		uiOffset = 0;
		uiTotal = dwSectorNum * SECTOR_SIZE;
		while (uiTotal>0)
		{
			if (uiTotal>=2048)
				uiWriteByte = 2048;
			else
				uiWriteByte = uiTotal;

			memcpy(writeBuf+8,lpIDBlock+uiOffset,uiWriteByte);
			iRet = m_pComm->RKU_WriteSector(uiOffset,uiWriteByte,writeBuf);
			if (iRet!=ERR_SUCCESS)
			{
				if (m_pLog)
					m_pLog->Record(_T("ERROR:WriteIDBlock-->RKU_WriteSector failed!"));
				return -1;
			}
			uiOffset += uiWriteByte;
			uiTotal -= uiWriteByte;
		}
		iRet = m_pComm->RKU_EndWriteSector((BYTE*)&end_write_sector_data);
		if (iRet==ERR_SUCCESS)
			break;
		nTryCount--;
	}
	if (nTryCount<=0)
		return -2;
	if (m_pLog)
	{
		m_pLog->Record(_T("INFO:WriteIDBlock out"));
	}
	return 0;
}

int CRKAndroidDevice::
	PrepareIDB()
{
	int i;
	generate_gf();
	gen_poly();
	string strInfo="";
	char szTmp[32];
	bool bFirstCS=false;
	for(i=0; i<8; i++)
	{
		if( m_flashInfo.bFlashCS&(1<<i) )
		{
			if (i==0)
			{
				bFirstCS = true;
			}
			if (m_pLog)
			{
				m_pLog->Record(_T("INFO:CS(%d)\t\t(%dMB)\t\t(%s)"),i+1,m_flashInfo.uiFlashSize,m_flashInfo.szManufacturerName);
			}
		}
	}
	if (!bFirstCS)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:PrepareIDB-->No Found 1st Flash CS"));
		}
		return -1;
	}

	if ( !BuildBlockStateMap(0) )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:PrepareIDB-->BuildBlockStateMap failed"));
		}
		return -2;
	}
	
	FindAllIDB();

	if (m_pLog)
	{
		m_pLog->Record(_T("ERROR:PrepareIDB-->IDblock count=%d."),m_oldIDBCounts);
	}

		
	
	memset(m_backupBuffer,0,SECTOR_SIZE);

	if (m_oldIDBCounts>0)
	{
		if (m_pLog)
		{
			strInfo = "";
			for (i=0;i<m_oldIDBCounts;i++)
			{
				sprintf(szTmp,"%d ",m_idBlockOffset[i]);
				strInfo += szTmp;
			}
			m_pLog->Record(_T("ERROR:PrepareIDB-->IDblock offset=%s."),strInfo.c_str());
		}
		BYTE buffer[4*SECTOR_SIZE];
		PRKANDROID_IDB_SEC3 pSec;
		PRKANDROID_IDB_SEC2 pSec2;
		pSec2 = (PRKANDROID_IDB_SEC2)(buffer+2*SECTOR_SIZE);
		pSec = (PRKANDROID_IDB_SEC3)(buffer+3*SECTOR_SIZE);

		if (!GetWriteBackData(m_oldIDBCounts,buffer))
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:PrepareIDB-->GetWriteBackData failed"));
			}
			return -3;
		}
		P_RC4((PBYTE)pSec2,SECTOR_SIZE);
		P_RC4((PBYTE)pSec,SECTOR_SIZE);
		IsExistSector3Crc(pSec2);
		
		m_usWriteBackCrc = CRC_16((PBYTE)pSec,SECTOR_SIZE);
		if (m_bExistSector3Crc)
		{
			m_usWriteBackCustomDataOffset = pSec2->usSec3CustomDataOffset;
			m_usWriteBackCustomDataSize = pSec2->usSec3CustomDataSize;
			if (m_usSector3Crc!=m_usWriteBackCrc)
			{
				if (m_pLog)
				{
					m_pLog->Record(_T("ERROR:PrepareIDB-->Check sector3 crc failed"));
				}
			}
		}
		memcpy(m_backupBuffer,pSec,SECTOR_SIZE);
	}
	else
	{
		FindBackupBuffer();
	}

	if (m_oldIDBCounts>0)
	{
		if ( !GetOldSectorData() )
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:PrepareIDB-->GetOldSectorData failed"));
			}
			return -4;
		}
	}
	if ( !CalcIDBCount() )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:PrepareIDB-->CalcIDBCount failed"));
		}
		return -5;
	}
	if ( !ReserveIDBlock() )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:PrepareIDB-->ReserveIDBlock failed"));
		}
		return -6;
	}
	if (m_pLog)
	{
		strInfo = "";
		for (i=0;i<5;i++)
		{
			sprintf(szTmp,"%d ",m_idBlockOffset[i]);
			strInfo += szTmp;
		}
		m_pLog->Record(_T("ERROR:PrepareIDB-->New IDblock offset=%s."),strInfo.c_str());
	}

	return 0;
}

int CRKAndroidDevice::DownloadIDBlock()
{
	DWORD dwSectorNum;
	dwSectorNum = m_flashInfo.uiSecNumPerIDB;
	
	PBYTE pIDBData=NULL;
	pIDBData = new BYTE[dwSectorNum*SECTOR_SIZE];
	if (!pIDBData)
		return -1;

	int iRet=0;
	memset(pIDBData,0,dwSectorNum*SECTOR_SIZE);
	
	iRet = MakeIDBlockData(pIDBData);
	if ( iRet!=0 )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:DownloadIDBlock-->MakeIDBlockData failed,RetCode(%d)"),iRet);
		}
		return -2;
	}

	iRet = WriteIDBlock(pIDBData,dwSectorNum,false);
	delete []pIDBData;
	if (iRet==0)
	{
		return 0;
	}
	else
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:DownloadIDBlock-->WriteIDBlock failed,RetCode(%d)"),iRet);
		}
		BufferWriteBack();
		return -3;
	}

}
int CRKAndroidDevice::DownloadImage()
{
	long long dwFwOffset;
	bool  bRet;
	dwFwOffset = m_pImage->FWOffset;
	STRUCT_RKIMAGE_HDR rkImageHead;
	int iHeadSize;
	iHeadSize = sizeof(STRUCT_RKIMAGE_HDR);
	char szPrompt[100];
	if (m_pProcessCallback)
		m_pProcessCallback(0.1,5);
	bRet = m_pImage->GetData(dwFwOffset,iHeadSize,(PBYTE)&rkImageHead);
	if ( !bRet )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:DownloadImage-->GetData failed"));
		}
		return -1;
	}
	if ( rkImageHead.item_count<=0 )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:DownloadImage-->No Found item"));
		}
		return -2;
	}
	
	m_dwBackupOffset = 0xFFFFFFFF;
	int i;
	bool bFound=false,bFoundSystem=false,bFoundUserData=false;
	int iParamPos=-1;
	long long uiTotalSize=0;
	long long ulItemSize;
	for ( i=0;i<rkImageHead.item_count;i++ )
	{
		if ( rkImageHead.item[i].flash_offset!=0xFFFFFFFF )
		{
			if ( strcmp(rkImageHead.item[i].name,PARTNAME_PARAMETER)==0)
			{
				bFound = true;
				iParamPos = i;
			}
			else
			{
				if (strcmp(rkImageHead.item[i].name,PARTNAME_SYSTEM)==0)
				{
					bFoundSystem = true;
				}
				if (strcmp(rkImageHead.item[i].name,PARTNAME_USERDATA)==0)
				{
					bFoundUserData = true;
				}
				if (strcmp(rkImageHead.item[i].name,PARTNAME_BACKUP)==0)
				{
					m_dwBackupOffset = rkImageHead.item[i].flash_offset;
				}
				if (rkImageHead.item[i].file[55]=='H')
				{
					ulItemSize = *((DWORD *)(&rkImageHead.item[i].file[56]));
					ulItemSize <<= 32;
					ulItemSize += rkImageHead.item[i].size;
				}
				else
					ulItemSize = rkImageHead.item[i].size;
				uiTotalSize += ulItemSize;
			}
			
		}
	}
	
	if ( !bFound )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:DownloadImage-->No Found Parameter file"));
		}
		return -3;
	}

	if (!MakeParamFileBuffer(rkImageHead.item[iParamPos]))
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:DownloadImage-->MakeParamFileBuffer failed"));
		}
		return -12;
	}

	if (!CheckParamPartSize(rkImageHead,iParamPos))
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:DownloadImage-->CheckParamPartSize failed"));
		}
		return -13;
	}
	uiTotalSize += (8*m_uiParamFileSize);
	
	m_uiLBATimes = 1;
	if (m_pProcessCallback)
		m_pProcessCallback(0.4,60);
	long long uiCurrentByte=0;
	for ( i=0;i<rkImageHead.item_count;i++ )
	{
		if (m_pProcessCallback)
			m_pProcessCallback((double)uiCurrentByte/(double)uiTotalSize,0);
		if ( rkImageHead.item[i].flash_offset==0xFFFFFFFF )
		{
			continue;
		}
		if (i==iParamPos)
		{
			if (m_pCallback)
			{
				sprintf(szPrompt,"%s writing...",rkImageHead.item[i].name);
				m_pCallback(szPrompt);
			}
			bRet = RKA_Param_Download(rkImageHead.item[i],uiCurrentByte,uiTotalSize);
			if ( !bRet )
			{
				if (m_pLog)
				{
					m_pLog->Record(_T(" ERROR:DownloadImage-->RKA_Param_Download failed"));
				}
//				if(m_pCallback)
//				{
//					sprintf(szPrompt,"%s writing... failed",rkImageHead.item[i].name);
//					m_pCallback(szPrompt);
//				}
				return -4;
			}
		}
		else
		{
			if (rkImageHead.item[i].file[55]=='H')
			{
				ulItemSize = *((DWORD *)(&rkImageHead.item[i].file[56]));
				ulItemSize <<= 32;
				ulItemSize += rkImageHead.item[i].size;
			}
			else
				ulItemSize = rkImageHead.item[i].size;
			
			if (ulItemSize>0)
			{
				if (m_pCallback)
				{
					sprintf(szPrompt,"%s writing...",rkImageHead.item[i].name);
					m_pCallback(szPrompt);
				}
				bRet = RKA_File_Download(rkImageHead.item[i],uiCurrentByte,uiTotalSize);
				if ( !bRet )
				{
					if (m_pLog)
					{
						m_pLog->Record(_T("ERROR:DownloadImage-->RKA_File_Download failed(%s)"),rkImageHead.item[i].name);
					}
					return -5;
				}
			}
		}
	}
	m_pComm->RKU_ReopenLBAHandle();
	if (m_pProcessCallback)
		m_pProcessCallback(1,0);
	if (m_pProcessCallback)
		m_pProcessCallback(0.4,60);
	uiCurrentByte = 0;
	for ( i=0;i<rkImageHead.item_count;i++ )
	{
		if (m_pProcessCallback)
			m_pProcessCallback((double)uiCurrentByte/(double)uiTotalSize,0);
		if ( rkImageHead.item[i].flash_offset==0xFFFFFFFF )
		{
			continue;
		}
		if (i==iParamPos)
		{ 
			if (m_pCallback)
			{
				sprintf(szPrompt,"%s checking...",rkImageHead.item[i].name);
				m_pCallback(szPrompt);
			}
			bRet = RKA_Param_Check(rkImageHead.item[i],uiCurrentByte,uiTotalSize);
			if ( !bRet )
			{
				if (m_pLog)
				{
					m_pLog->Record(_T("ERROR:DownloadImage-->RKA_Param_Check failed"));
				}
				return -6;
			}
		}
		else
		{
			if (rkImageHead.item[i].file[55]=='H')
			{
				ulItemSize = *((DWORD *)(&rkImageHead.item[i].file[56]));
				ulItemSize <<= 32;
				ulItemSize += rkImageHead.item[i].size;
			}
			else
				ulItemSize = rkImageHead.item[i].size;
			if (ulItemSize>0)
			{
				if (m_pCallback)
				{
					sprintf(szPrompt,"%s checking...",rkImageHead.item[i].name);
					m_pCallback(szPrompt);
				}
				bRet = RKA_File_Check(rkImageHead.item[i],uiCurrentByte,uiTotalSize);
				if ( !bRet )
				{
					if (m_pLog)
					{
						m_pLog->Record(_T("ERROR:DownloadImage-->RKA_File_Check failed(%s)"),rkImageHead.item[i].name);
					}
					return -7;
				}
			}
			
		}
	}
	if (m_pProcessCallback)
		m_pProcessCallback(1,0);
	return 0;
}
bool CRKAndroidDevice::write_partition_upgrade_flag(DWORD dwOffset,BYTE *pMd5,UINT uiFlag)
{
	BYTE flagSector[SECTOR_SIZE];
	int iRet;
	memset(flagSector,0,SECTOR_SIZE);
	memcpy(flagSector,pMd5,32);
	memcpy(flagSector+32,(BYTE *)(&uiFlag),4);
	iRet = m_pComm->RKU_WriteLBA(dwOffset,1,flagSector);
	if (iRet!=ERR_SUCCESS)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:write_partition_upgrade_flag-->RKU_WriteLBA failed,err=%d",iRet);
		}
		return false;
	}
	return true;
}
bool CRKAndroidDevice::read_partition_upgrade_flag(DWORD dwOffset,BYTE *pMd5,UINT *uiFlag)
{
	if (m_pLog)
	{
		m_pLog->Record("INFO:read_partition_upgrade_flag in");
	}	
	BYTE flagSector[SECTOR_SIZE];
	int iRet;
	iRet = m_pComm->RKU_ReadLBA(dwOffset,1,flagSector);
	if (iRet!=ERR_SUCCESS)
	{
		if (m_pLog)
		{
			m_pLog->Record("ERROR:read_partition_upgrade_flag-->RKU_ReadLBA failed,err=%d",iRet);
		}
		return false;
	}
	memcpy(pMd5,flagSector,32);
	(*uiFlag) = *((UINT *)(flagSector+32));
	if (m_pLog)
	{
		m_pLog->Record("INFO:read_partition_upgrade_flag out,flag=0x%x",*uiFlag);
	}
	return true;
}

int CRKAndroidDevice::UpgradePartition()
{
	long long dwFwOffset;
	bool  bRet,bSameFw=false;
	BYTE localMd5[32];
	BYTE *fwMd5,*fwSignMd5;
	UINT uiFlag;
	DWORD dwFlagSector=0;
	dwFwOffset = m_pImage->FWOffset;
	STRUCT_RKIMAGE_HDR rkImageHead;
	vector<int> vecUpgradePartition;
	vecUpgradePartition.clear();
	char szPrompt[100];
	int iHeadSize;
	iHeadSize = sizeof(STRUCT_RKIMAGE_HDR);
	if (m_pProcessCallback)
		m_pProcessCallback(0.1,5);

	bRet = m_pImage->GetData(dwFwOffset,iHeadSize,(PBYTE)&rkImageHead);
	if ( !bRet )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:UpgradePartition-->GetData failed"));
		}
		return -1;
	}
	if ( rkImageHead.item_count<=0 )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:UpgradePartition-->No Found item"));
		}
		return -2;
	}
	
	m_dwBackupOffset = 0xFFFFFFFF;
	int i;
	vector<int>::iterator iter;
	bool bFound=false,bFoundSystem=false,bFoundUserData=false;
	int iParamPos=-1;
	long long uiTotalSize=0;
	long long ulItemSize;
	for ( i=0;i<rkImageHead.item_count;i++ )
	{
		if ( rkImageHead.item[i].flash_offset!=0xFFFFFFFF )
		{
			for (iter=vecUpgradePartition.begin();iter!=vecUpgradePartition.end();iter++)
			{
				if (rkImageHead.item[*iter].flash_offset>rkImageHead.item[i].flash_offset)
				{
					iter = vecUpgradePartition.insert(iter,i);
					break;
				}
			}
			if (iter==vecUpgradePartition.end())
			{
				vecUpgradePartition.push_back(i);
			}
			if ( strcmp(rkImageHead.item[i].name,PARTNAME_PARAMETER)==0)
			{
				bFound = true;
				iParamPos = i;
			}
			else
			{
				if (strcmp(rkImageHead.item[i].name,PARTNAME_SYSTEM)==0)
				{
					bFoundSystem = true;
				}
				if (strcmp(rkImageHead.item[i].name,PARTNAME_MISC)==0)
				{
					dwFlagSector = rkImageHead.item[i].flash_offset + rkImageHead.item[i].part_size -4;
				}
				if (strcmp(rkImageHead.item[i].name,PARTNAME_USERDATA)==0)
				{
					bFoundUserData = true;
				}
				if (strcmp(rkImageHead.item[i].name,PARTNAME_BACKUP)==0)
				{
					m_dwBackupOffset = rkImageHead.item[i].flash_offset;
				}
				if (rkImageHead.item[i].file[55]=='H')
				{
					ulItemSize = *((DWORD *)(&rkImageHead.item[i].file[56]));
					ulItemSize <<= 32;
					ulItemSize += rkImageHead.item[i].size;
				}
				else
					ulItemSize = rkImageHead.item[i].size;
				uiTotalSize += ulItemSize;
			}
			
		}
	}
	
	if ( !bFound )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:UpgradePartition-->No Found Parameter file"));
		}
		return -3;
	}

	if (!MakeParamFileBuffer(rkImageHead.item[iParamPos]))
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:UpgradePartition-->MakeParamFileBuffer failed"));
		}
		return -12;
	}

	if (!CheckParamPartSize(rkImageHead,iParamPos))
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:UpgradePartition-->CheckParamPartSize failed"));
		}
		return -13;
	}
	uiTotalSize += (8*m_uiParamFileSize);//加上参数文件需要的大小
	m_uiLBATimes = 1;
	m_pImage->GetMd5Data(fwMd5,fwSignMd5);
	if (dwFlagSector!=0)
	{
		if (read_partition_upgrade_flag(dwFlagSector,localMd5,&uiFlag))
		{
			if (memcmp(localMd5,fwMd5,32)==0)
				bSameFw = true;
		}
	}
	if (m_pProcessCallback)
		m_pProcessCallback(0.8,90);
	long long uiCurrentByte=0;
	for ( i=0;i<vecUpgradePartition.size();i++ )
	{
		if (m_pProcessCallback)
			m_pProcessCallback((double)uiCurrentByte/(double)uiTotalSize,0);
		if (vecUpgradePartition[i]==iParamPos)
		{
			if ((bSameFw)&&(uiFlag>=rkImageHead.item[vecUpgradePartition[i]].flash_offset))
			{
				uiCurrentByte += (8*m_uiParamFileSize);
				continue;
			}
			if (m_pCallback)
			{
				sprintf(szPrompt,"%s writing...\n",rkImageHead.item[vecUpgradePartition[i]].name);
				m_pCallback(szPrompt);
			}
			bRet = RKA_Param_Download(rkImageHead.item[vecUpgradePartition[i]],uiCurrentByte,uiTotalSize);
			if ( !bRet )
			{
				if (m_pLog)
				{
					m_pLog->Record(_T(" ERROR:UpgradePartition-->RKA_Param_Download failed"));
				}
				return -4;
			}
			m_pComm->RKU_ReopenLBAHandle();
			if (m_pCallback)
			{
				sprintf(szPrompt,"%s checking...\n",rkImageHead.item[vecUpgradePartition[i]].name);
				m_pCallback(szPrompt);
			}
			uiCurrentByte -= (8*m_uiParamFileSize);
			bRet = RKA_Param_Check(rkImageHead.item[vecUpgradePartition[i]],uiCurrentByte,uiTotalSize);
			if ( !bRet )
			{
				if (m_pLog)
				{
					m_pLog->Record(_T("ERROR:UpgradePartition-->RKA_Param_Check failed"));
				}
				return -6;
			}
		}
		else
		{
			
			if (rkImageHead.item[vecUpgradePartition[i]].file[55]=='H')
			{
				ulItemSize = *((DWORD *)(&rkImageHead.item[vecUpgradePartition[i]].file[56]));
				ulItemSize <<= 32;
				ulItemSize += rkImageHead.item[vecUpgradePartition[i]].size;
			}
			else
				ulItemSize = rkImageHead.item[vecUpgradePartition[i]].size;
			if ((bSameFw)&&(uiFlag>=rkImageHead.item[vecUpgradePartition[i]].flash_offset))
			{
				uiCurrentByte += ulItemSize;
				continue;
			}
			
			if (ulItemSize>0)
			{
				if (m_pCallback)
				{
					sprintf(szPrompt,"%s writing...\n",rkImageHead.item[vecUpgradePartition[i]].name);
					m_pCallback(szPrompt);
				}
				bRet = RKA_File_Download(rkImageHead.item[vecUpgradePartition[i]],uiCurrentByte,uiTotalSize);
				if ( !bRet )
				{
					if (m_pLog)
					{
						m_pLog->Record(_T("ERROR:UpgradePartition-->RKA_File_Download failed(%s)"),rkImageHead.item[vecUpgradePartition[i]].name);
					}
					return -5;
				}
				m_pComm->RKU_ReopenLBAHandle();
				if (m_pCallback)
				{
					sprintf(szPrompt,"%s checking...\n",rkImageHead.item[vecUpgradePartition[i]].name);
					m_pCallback(szPrompt);
				}
				uiCurrentByte -= ulItemSize;
				bRet = RKA_File_Check(rkImageHead.item[vecUpgradePartition[i]],uiCurrentByte,uiTotalSize);
				if ( !bRet )
				{
					if (m_pLog)
					{
						m_pLog->Record(_T("ERROR:UpgradePartition-->RKA_File_Check failed(%s)"),rkImageHead.item[vecUpgradePartition[i]].name);
					}
					return -7;
				}
			}
			else
				continue;
		}
		if (dwFlagSector!=0)
		{
			write_partition_upgrade_flag(dwFlagSector,fwMd5,rkImageHead.item[vecUpgradePartition[i]].flash_offset);
		}
	}
	if (m_pProcessCallback)
		m_pProcessCallback(1,0);
	return 0;
}
int CRKAndroidDevice::EraseIDB()
{
	DWORD dwEraseCounts;
	if ( m_oldIDBCounts>0 )
	{
		dwEraseCounts = m_oldSec1->usSysReservedBlock;
	}
	else
	{
		dwEraseCounts = IDBLOCK_TOP;
	}
	if (m_bEmmc)
	{
		if(EraseEmmcBlock(0,0,dwEraseCounts)!=ERR_SUCCESS)
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:EraseIDB-->EraseEmmcBlock failed"));
			}
			return -1;
		}
	}
	else
	{
		if ( !EraseMutilBlock(0,0,dwEraseCounts,false) )
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:EraseIDB-->EraseMutilBlock failed"));
			}
			return -1;
		}
	}
	
	return 0;
}
int CRKAndroidDevice::EraseAllBlocks()
{
	int i;
	UINT uiBlockCount;
	int iRet=ERR_SUCCESS,iErasePos=0,iEraseBlockNum=0,iEraseTimes=0,iCSIndex=0;
	BYTE bCSCount=0;
	for (i=0;i<8;i++)
	{
		if ( m_flashInfo.bFlashCS & (1<<i) )
		{
			bCSCount++;
		}
	}

	for (i=0;i<8;i++)
	{
		if ( m_flashInfo.bFlashCS & (1<<i) )
		{
			uiBlockCount = m_flashInfo.uiBlockNum;
			iErasePos=0;iEraseTimes=0;
			while (uiBlockCount>0)
			{
				iEraseBlockNum = (uiBlockCount<MAX_ERASE_BLOCKS)?uiBlockCount:MAX_ERASE_BLOCKS;
				if (m_bEmmc)
				{
					iRet = EraseEmmcBlock(i,iErasePos,iEraseBlockNum);
					if (iRet!=ERR_SUCCESS)
					{
						if (m_pLog)
						{
							m_pLog->Record(_T("ERROR:EraseAllBlocks-->EraseEmmcBlock failed,RetCode(%d)"),iRet);
						}
						return -1;
					}
				}
				else
				{
					iRet = m_pComm->RKU_EraseBlock(i,iErasePos,iEraseBlockNum,ERASE_FORCE);
					if ((iRet!=ERR_SUCCESS)&&(iRet!=ERR_FOUND_BAD_BLOCK))
					{
						if (m_pLog)
						{
							m_pLog->Record(_T("ERROR:EraseAllBlocks-->RKU_EraseBlock failed,RetCode(%d)"),iRet);
						}
						return -1;
					}
				}
				
				iErasePos += iEraseBlockNum;
				uiBlockCount -= iEraseBlockNum;
				iEraseTimes++;
			}
			iCSIndex++;
		}
	}
	
	return 0;
}



bool CRKAndroidDevice::BufferWriteBack()
{
	FindAllIDB();
	if (m_oldIDBCounts>0)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:BufferWriteBack-->Found IDB"));
		}
		return true;
	}
	if (m_usWriteBackCrc==0)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("INFO:BufferWriteBack-->Crc is zero"));
		}
		return true;
	}
	BYTE pWriteBackBuffer[2*SECTOR_SIZE];
	
	char *pszTag = (char *)pWriteBackBuffer;
	USHORT *pValue = (USHORT *)(pWriteBackBuffer+4);

	memset(pWriteBackBuffer,0,2*SECTOR_SIZE);

	
	strcpy(pszTag,"CRC");
	*pValue = m_usWriteBackCrc;
	pValue++;
	*pValue = m_usWriteBackCustomDataOffset;
	pValue++;
	*pValue = m_usWriteBackCustomDataSize;
	memcpy(pWriteBackBuffer+SECTOR_SIZE,m_backupBuffer,SECTOR_SIZE);

	STRUCT_END_WRITE_SECTOR end_write_sector_data;
	BYTE writeBuf[8*SECTOR_SIZE];
	UINT uiOffset,uiTotal,uiWriteByte,uiCrc;
	int iRet,i,nTryCount=3;
	uiTotal = 2*SECTOR_SIZE;
	uiCrc = CRC_32(pWriteBackBuffer,uiTotal);
	end_write_sector_data.uiSize = uiTotal;
	end_write_sector_data.uiCrc = uiCrc;
	for(i=WBBUFFER_BOTTOM;i<WBBUFFER_TOP;i++)
		end_write_sector_data.uiBlock[i] = i;
	while (nTryCount>0)
	{
		uiOffset = 0;
		uiTotal = 2 * SECTOR_SIZE;
		while (uiTotal>0)
		{
			if (uiTotal>=2048)
				uiWriteByte = 2048;
			else
				uiWriteByte = uiTotal;

			memcpy(writeBuf+8,pWriteBackBuffer+uiOffset,uiWriteByte);
			iRet = m_pComm->RKU_WriteSector(uiOffset,uiWriteByte,writeBuf);
			if (iRet!=ERR_SUCCESS)
			{
				if (m_pLog)
					m_pLog->Record(_T("ERROR:BufferWriteBack-->RKU_WriteSector failed!"));
				return false;
			}
			uiOffset += uiWriteByte;
			uiTotal -= uiWriteByte;
		}
		iRet = m_pComm->RKU_EndWriteSector((BYTE*)&end_write_sector_data);
		if (iRet==ERR_SUCCESS)
			break;
		nTryCount--;
	}
	if (nTryCount<=0)
		return false;

	return true;
}
bool CRKAndroidDevice::FindBackupBuffer()
{
	int i,iRet;
	bool bRet;
	BYTE buffer[2*SECTOR_SIZE];
	for (i=WBBUFFER_BOTTOM;i<WBBUFFER_TOP;i++)
	{
		memset(buffer,0,2*SECTOR_SIZE);
		iRet = m_pComm->RKU_ReadSector(i*m_flashInfo.uiSectorPerBlock,2,buffer);
		if (iRet!=ERR_SUCCESS)
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:FindBackupBuffer-->RKU_ReadSector failed,RetCode(%d)"),iRet);
			}
			continue;
		}
		else
		{
			PSTRUCT_RKANDROID_WBBUFFER pWriteBack;
			pWriteBack = (PSTRUCT_RKANDROID_WBBUFFER)buffer;
			char *pszCrcTag = (char *)buffer;
			if (pWriteBack->dwTag==0x38324B52)
			{
				bRet = CheckCrc16(buffer,SECTOR_SIZE-2,pWriteBack->usCrc);
				if (!bRet)
				{
					if (m_pLog)
					{
						m_pLog->Record(_T("ERROR:FindBackupBuffer-->Check Crc Failed"));
					}
//					continue;
				}
				PRKANDROID_IDB_SEC3 pSec = (PRKANDROID_IDB_SEC3)m_backupBuffer;
				pSec->usSNSize = pWriteBack->usSnSize;
				memcpy(pSec->sn,pWriteBack->btSnData,RKDEVICE_SN_LEN);
				memset(pSec->reserved,0,RKANDROID_SEC3_RESERVED_LEN);
				memcpy(pSec->reserved+6,pWriteBack->btReserve,RKANDROID_SEC3_RESERVED_LEN-6);
				pSec->imeiSize = pWriteBack->btImeiSize;
				memcpy(pSec->imei,pWriteBack->btImeiData,RKDEVICE_IMEI_LEN);
				pSec->uidSize = pWriteBack->btUidSize;
				memcpy(pSec->uid,pWriteBack->btUidData,RKDEVICE_UID_LEN);
				pSec->blueToothSize = pWriteBack->btBlueToothSize;
				memcpy(pSec->blueToothAddr,pWriteBack->btBlueToothData,RKDEVICE_BT_LEN);
				pSec->macSize = pWriteBack->btMacSize;
				memcpy(pSec->macAddr,pWriteBack->btMacData,RKDEVICE_MAC_LEN);
				m_usWriteBackCrc = CRC_16(m_backupBuffer,SECTOR_SIZE);
			}
			else if (strcmp(pszCrcTag,"CRC")==0)
			{
				m_usWriteBackCrc = *((USHORT *)(buffer+4));
				m_usWriteBackCustomDataOffset = *((USHORT *)(buffer+6));
				m_usWriteBackCustomDataSize = *((USHORT *)(buffer+8));
				bRet = CheckCrc16(buffer+SECTOR_SIZE,SECTOR_SIZE,m_usWriteBackCrc);
				if (!bRet)
				{
					if (m_pLog)
					{
						m_pLog->Record(_T("ERROR:FindBackupBuffer-->Check Crc Failed"));
					}
					m_usWriteBackCrc = CRC_16(buffer+SECTOR_SIZE+SPARE_SIZE,SECTOR_SIZE);
//					continue;
				}
				memcpy(m_backupBuffer,buffer+SECTOR_SIZE,SECTOR_SIZE);
			}
			else
			{
				if (m_pLog)
				{
					m_pLog->Record(_T("INFO:FindBackupBuffer-->No Found Tag"));
				}
				continue;
			}
			
			break;
		}
	}
	if ( i<WBBUFFER_TOP )
	{
		return true;
	}
	else
		return false;
}

bool CRKAndroidDevice::RKA_File_Download(STRUCT_RKIMAGE_ITEM &entry,long long &currentByte,long long totalByte)
{
	UINT uiLBATransferSize=(LBA_TRANSFER_SIZE)*m_uiLBATimes;
	UINT uiLBASector = uiLBATransferSize/SECTOR_SIZE;
	int iRet;
	bool bRet;
	UINT uiBufferSize=uiLBATransferSize;
	long long uifileBufferSize;
	long long ulEntryStartOffset;
	DWORD dwFWOffset;
	dwFWOffset = m_pImage->FWOffset;
	if (entry.file[50]=='H')
	{
		ulEntryStartOffset = *((DWORD *)(&entry.file[51]));
		ulEntryStartOffset <<= 32;
		ulEntryStartOffset += entry.offset;
		ulEntryStartOffset += m_pImage->FWOffset;
	}
	else
	{
		ulEntryStartOffset = m_pImage->FWOffset;
		ulEntryStartOffset += entry.offset;
	}
	if (entry.file[55]=='H')
	{
		uifileBufferSize = *((DWORD *)(&entry.file[56]));
		uifileBufferSize <<= 32;
		uifileBufferSize += entry.size;
	}
	else
		uifileBufferSize = entry.size;
	if (m_pLog)
	{
		m_pLog->Record(_T(" INFO:Start to download %s,offset=0x%x,size=%llu"),entry.name,entry.flash_offset,uifileBufferSize);
	}

	BYTE byRWMethod=RWMETHOD_IMAGE;
	
	if (entry.flash_offset>m_dwBackupOffset)
	{
		byRWMethod = RWMETHOD_LBA;
	}
	
	PBYTE pBuffer=NULL;
	pBuffer = new BYTE[uiBufferSize];
	if (!pBuffer)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:RKA_File_Download-->New memory failed"));
		}
		return false;
	}

	bool bUser=false;
//	if (strcmp(entry.name,PARTNAME_MISC)==0)
//	{
//		currentByte += uifileBufferSize;
//		return true;
//	}
	if (strcmp(entry.name,PARTNAME_USER)==0)
	{
		bUser = true;
	}
	
	UINT uiBegin,uiLen,uiWriteByte;
	long long uiEntryOffset;
	uiBegin = entry.flash_offset;
	uiLen = 0;uiWriteByte = 0;uiEntryOffset=0;
	while ( uifileBufferSize>0 )
	{
		memset(pBuffer,0,uiBufferSize);
		if ( uifileBufferSize<uiBufferSize )
		{
			uiWriteByte = uifileBufferSize;
			uiLen = ( (uiWriteByte%SECTOR_SIZE==0) ? (uiWriteByte/SECTOR_SIZE) : (uiWriteByte/SECTOR_SIZE+1) );
		}
		else
		{
			uiWriteByte = uiBufferSize;
			uiLen = uiLBASector;
		}
		bRet = m_pImage->GetData(dwFWOffset+entry.offset+uiEntryOffset,uiWriteByte,pBuffer);
		if ( !bRet )
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:RKA_File_Download-->GetFileData failed"));
			}
			delete []pBuffer;
			pBuffer = NULL;
			return false;
		}
		if (bUser)
		{
			if ((pBuffer[0]==0xEB)&&(pBuffer[1]==0x58)&&(pBuffer[2]==0x90))
			{//fat user image
				iRet = m_pComm->RKU_TestDeviceReady((DWORD *)&m_uiUserSectors,NULL,TU_GETUSERSECTOR_SUBCODE);
				if (iRet!=ERR_SUCCESS)
				{
					if (m_pLog)
					{
						m_pLog->Record(_T("ERROR:RKA_File_Download-->Get user sectors failed,RetCode(%d)"),iRet);
					}

					delete []pBuffer;
					pBuffer = NULL;
					return false;
				}
				if ((m_uiUserSectors==0)||(m_uiUserSectors==(DWORD)-1))
				{
					if (m_pLog)
					{
						m_pLog->Record(_T("ERROR:RKA_File_Download-->User size is wrong,value=0x%x"),m_uiUserSectors);
					}

					delete []pBuffer;
					pBuffer = NULL;
					return false;
				}
				if (m_uiUserSectors<=uiBegin)
				{
					if (m_pLog)
					{
						m_pLog->Record(_T("ERROR:RKA_File_Download-->Available total is smaller than user offset"));
					}

					delete []pBuffer;
					pBuffer = NULL;
					return false;
				}
				m_uiUserSectors -= uiBegin;
				
				PBYTE pDbr,pCopyDbr;
				pDbr = pBuffer;
				pCopyDbr = pBuffer + SECTOR_SIZE*6;
				if (*(UINT *)(pDbr+32)<m_uiUserSectors)
				{
					if (m_pLog)
					{
						m_pLog->Record(_T("ERROR:RKA_File_Download-->Original size is smaller than current user size"));
					}

					delete []pBuffer;
					pBuffer = NULL;
					return false;
				}

				(*(UINT *)(pDbr+32)) = m_uiUserSectors;
				(*(UINT *)(pCopyDbr+32)) = m_uiUserSectors;
			}
				
			bUser = false;
		}
		
		iRet = m_pComm->RKU_WriteLBA(uiBegin,uiLen,pBuffer,byRWMethod);
		if( iRet!=ERR_SUCCESS )
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:RKA_File_Download-->RKU_WriteLBA failed,Written(%d),RetCode(%d)"),uiEntryOffset,iRet);
			}
			
			delete []pBuffer;
			pBuffer = NULL;
			return false;
		}
		uifileBufferSize -= uiWriteByte;
		uiEntryOffset += uiWriteByte;
		uiBegin += uiLen;
		currentByte += uiWriteByte;
		
	}
	delete []pBuffer;
	pBuffer = NULL;
	return true;
}

bool CRKAndroidDevice::RKA_File_Check(STRUCT_RKIMAGE_ITEM &entry,long long &currentByte,long long totalByte)
{
	UINT uiLBATransferSize=(LBA_TRANSFER_SIZE)*m_uiLBATimes;
	UINT uiLBASector = uiLBATransferSize/SECTOR_SIZE;
	int iRet;
	bool bRet;
	UINT uiBufferSize=uiLBATransferSize;
	long long uifileBufferSize;
	long long ulEntryStartOffset;
	DWORD dwFWOffset;
	dwFWOffset = m_pImage->FWOffset;
	if (entry.file[50]=='H')
	{
		ulEntryStartOffset = *((DWORD *)(&entry.file[51]));
		ulEntryStartOffset <<= 32;
		ulEntryStartOffset += entry.offset;
		ulEntryStartOffset += m_pImage->FWOffset;
	}
	else
	{
		ulEntryStartOffset = m_pImage->FWOffset;
		ulEntryStartOffset += entry.offset;
	}
	if (entry.file[55]=='H')
	{
		uifileBufferSize = *((DWORD *)(&entry.file[56]));
		uifileBufferSize <<= 32;
		uifileBufferSize += entry.size;
	}
	else
		uifileBufferSize = entry.size;

	BYTE byRWMethod=RWMETHOD_IMAGE;
	if (entry.flash_offset>m_dwBackupOffset)
	{
		byRWMethod = RWMETHOD_LBA;
	}

	PBYTE pBufferFromFile=NULL;
	pBufferFromFile = new BYTE[uiBufferSize];
	if (!pBufferFromFile)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:RKA_File_Check-->New memory failed"));
		}
		return false;
	}
	PBYTE pBufferFromFlash=NULL;
	pBufferFromFlash = new BYTE[uiBufferSize];
	if (!pBufferFromFlash)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:RKA_File_Check-->New memory failed"));
		}
		delete []pBufferFromFile;
		return false;
	}
	
	bool bUser=false;
//	if (strcmp(entry.name,PARTNAME_MISC)==0)
//	{
//		currentByte += uifileBufferSize;
//		return true;
//	}
	if (strcmp(entry.name,PARTNAME_USER)==0)
	{
		bUser = true;
		if ((entry.name[PART_NAME-2]=='N')&&(entry.name[PART_NAME-1]=='C'))
		{//no check user
			currentByte += uifileBufferSize;
			return true;
		}
	}
	
	UINT uiBegin,uiLen,uiWriteByte;
	long long uiEntryOffset;
	uiBegin = entry.flash_offset;
	uiLen = 0;uiWriteByte = 0;uiEntryOffset=0;
	while ( uifileBufferSize>0 )
	{
		if ( uifileBufferSize<uiBufferSize )
		{
			uiWriteByte = uifileBufferSize;
			uiLen = ( (uiWriteByte%SECTOR_SIZE==0) ? (uiWriteByte/SECTOR_SIZE) : (uiWriteByte/SECTOR_SIZE+1) );
		}
		else
		{
			uiWriteByte = uiBufferSize;
			uiLen = uiLBASector;
		}

	
		memset(pBufferFromFile,0,uiBufferSize);
		memset(pBufferFromFlash,0,uiBufferSize);
		
		iRet = m_pComm->RKU_ReadLBA(uiBegin,uiLen,pBufferFromFlash,byRWMethod);
		if( iRet!=ERR_SUCCESS )
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:RKA_File_Check-->RKU_ReadLBA failed,Read(%d),RetCode(%d)"),uiEntryOffset,iRet);
			}
			delete []pBufferFromFile;
			delete []pBufferFromFlash;
			return false;
		}
		bRet = m_pImage->GetData(dwFWOffset+entry.offset+uiEntryOffset,uiWriteByte,pBufferFromFile);
		if ( !bRet )
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:RKA_File_Check-->GetFileData failed"));
			}
			delete []pBufferFromFile;
			delete []pBufferFromFlash;
			return false;
		}
		if (bUser)
		{
			if ((pBufferFromFile[0]==0xEB)&&(pBufferFromFile[1]==0x58)&&(pBufferFromFile[2]==0x90))
			{//fat user image		
				PBYTE pDbr,pCopyDbr;
				pDbr = pBufferFromFile;
				pCopyDbr = pBufferFromFile + SECTOR_SIZE*6;

				(*(UINT *)(pDbr+32)) = m_uiUserSectors;
				(*(UINT *)(pCopyDbr+32)) = m_uiUserSectors;
			}

			bUser = false;
		}
	
		if ( memcmp(pBufferFromFile,pBufferFromFlash,uiWriteByte)!=0 )
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:RKA_File_Check-->Memcmp failed,Read(%d)"),uiEntryOffset);
				tchar szDateTime[100];
				tstring strFile;
				time_t	now;
				struct tm timeNow;
				time(&now);
				localtime_r(&now,&timeNow);
				_stprintf(szDateTime,_T("%02d-%02d-%02d"),timeNow.tm_hour,timeNow.tm_min,timeNow.tm_sec);
		
				strFile = szDateTime;
				strFile += _T("/tmp/file.bin");
				m_pLog->SaveBuffer( strFile,pBufferFromFile,uiWriteByte );
				
				strFile = szDateTime;
				strFile += _T("/tmp/flash.bin");
				m_pLog->SaveBuffer( strFile,pBufferFromFlash,uiWriteByte );
			}
			delete []pBufferFromFile;
			delete []pBufferFromFlash;
			return false;
		}
//		if (uiBegin == entry.flash_offset)
//		{
//			tstring strFile;
//			strFile = "/tmp/";
//			strFile += entry.name;
//			strFile += ".img";
//			m_pLog->SaveBuffer( strFile,pBufferFromFlash,uiWriteByte );
//			m_pLog->Record("%s=%x %x %x %x",entry.name,pBufferFromFlash[0],pBufferFromFlash[1],pBufferFromFlash[2],pBufferFromFlash[3]);
//		}
		
		currentByte += uiWriteByte;
		uiEntryOffset += uiWriteByte;
		uifileBufferSize -= uiWriteByte;
		uiBegin += uiLen;
		
	}

	delete []pBufferFromFile;
	delete []pBufferFromFlash;
	return true;
}

bool CRKAndroidDevice::RKA_Param_Download(STRUCT_RKIMAGE_ITEM &entry,long long &currentByte,long long totalByte)
{//写5份参数文件
	UINT uiLBATransferSize=(LBA_TRANSFER_SIZE)*m_uiLBATimes;
	UINT uiLBASector = uiLBATransferSize/SECTOR_SIZE;
	int  iRet,i;
	BYTE byRWMethod=RWMETHOD_IMAGE;
	if (entry.flash_offset>m_dwBackupOffset)
	{
		byRWMethod = RWMETHOD_LBA;
	}
	
	UINT uiTransfer;
	UINT uiStepSec=entry.part_size/8;
//	if (m_pLog)
//	{
//		m_pLog->Record(_T("INFO:RKA_Param_Download-->step=%d"),uiStepSec);
//	}
	
	UINT uiLen,uiWriteByte,uiFileSize;
	UINT uiBegin;
	for ( i=0;i<8;i++ )
	{
		uiFileSize = m_uiParamFileSize;
		uiBegin = entry.flash_offset+uiStepSec*i;
		uiLen = 0;
		uiWriteByte = 0;
		uiTransfer = 0;
//		if (m_pLog)
//		{
//			m_pLog->Record(_T("INFO:RKA_Param_Download-->no %d,offset=%d"),i+1,uiBegin);
//		}
		while (uiFileSize>0)
		{
			if ( uiFileSize<uiLBATransferSize )
			{
				uiWriteByte = uiFileSize;
				uiLen = ( (uiWriteByte%512==0) ? (uiWriteByte/512) : (uiWriteByte/512+1) );
			}
			else
			{
				uiWriteByte = uiLBATransferSize;
				uiLen = uiLBASector;
			}
			iRet = m_pComm->RKU_WriteLBA(uiBegin,uiLen,m_paramBuffer+uiTransfer,byRWMethod);//每次都要写32扇区,按page对齐
			if( iRet!=ERR_SUCCESS )
			{
				if (m_pLog)
				{
					m_pLog->Record(_T("ERROR:RKA_Param_Download-->RKU_WriteLBA failed,Written(%d),RetCode(%d)"),uiTransfer,iRet);
				}
				
				return false;
			}
			
			uiTransfer += uiWriteByte;
			currentByte += uiWriteByte;
			uiBegin += uiLen;
			uiFileSize -= uiWriteByte;
			
		}
	}
	
	return true;
}
bool CRKAndroidDevice::RKA_Param_Check(STRUCT_RKIMAGE_ITEM &entry,long long &currentByte,long long totalByte)
{
	UINT uiLBATransferSize=(LBA_TRANSFER_SIZE)*m_uiLBATimes;
	UINT uiLBASector = uiLBATransferSize/SECTOR_SIZE;
	int iRet,i;
	UINT uiReadBufferSize=uiLBATransferSize;
	BYTE byRWMethod=RWMETHOD_IMAGE;
	if (entry.flash_offset>m_dwBackupOffset)
	{
		byRWMethod = RWMETHOD_LBA;
	}

	PBYTE pRead=NULL;
	pRead = new BYTE[uiReadBufferSize];
	if (!pRead)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:RKA_Param_Check-->New ReadBuffer failed"));
		}
		return false;
	}
	
	UINT uiTransfer;
	UINT uiStepSec=entry.part_size/8;
	
	UINT uiLen,uiWriteByte,uiFileSize;
	UINT uiBegin;
	for ( i=0;i<8;i++ )
	{
		uiFileSize = m_uiParamFileSize;
		uiBegin = entry.flash_offset+uiStepSec*i;
		uiLen = 0;
		uiWriteByte = 0;
		uiTransfer = 0;
//		if (m_pLog)
//		{
//			m_pLog->Record(_T("INFO:RKA_Param_Check-->no %d,offset=%d"),i+1,uiBegin);
//		}
		while (uiFileSize>0)
		{
			memset(pRead,0,uiReadBufferSize);
			if ( uiFileSize<uiLBATransferSize )
			{
				uiWriteByte = uiFileSize;
				uiLen = ( (uiWriteByte%512==0) ? (uiWriteByte/512) : (uiWriteByte/512+1) );
			}
			else
			{
				uiWriteByte = uiLBATransferSize;
				uiLen = uiLBASector;
			}
			iRet = m_pComm->RKU_ReadLBA(uiBegin,uiLen,pRead,byRWMethod);
			if( iRet!=ERR_SUCCESS )
			{
				if (m_pLog)
				{
					m_pLog->Record(_T("ERROR:RKA_Param_Check-->RKU_ReadLBA failed,Read(%d),RetCode(%d)"),uiTransfer,iRet);
				}
				delete []pRead;
				return false;
			}
			if ( memcmp(pRead,m_paramBuffer+uiTransfer,uiWriteByte)!=0 )
			{
				if (m_pLog)
				{
					m_pLog->Record(_T("ERROR:RKA_Param_Check-->Memcmp failed,Read(%d)"),uiTransfer);
					tchar szDateTime[100];
					tstring strFile;
					time_t	now;
					struct tm timeNow;
					time(&now);
					localtime_r(&now,&timeNow);
					_stprintf(szDateTime,_T("%02d-%02d-%02d"),timeNow.tm_hour+1,timeNow.tm_min+1,timeNow.tm_sec+1);
					
					strFile = szDateTime;
					strFile += _T("/tmp/file.bin");
					m_pLog->SaveBuffer( strFile,m_paramBuffer+uiTransfer,uiWriteByte );
					
					strFile = szDateTime;
					strFile += _T("/tmp/flash.bin");
					m_pLog->SaveBuffer( strFile,pRead,uiWriteByte );
				}
				
				delete []pRead;
				return false;
				
			}
//			if (m_pLog)
//			{
//				string strSrc,strDst;
//				if (uiWriteByte>16)
//				{
//					m_pLog->PrintBuffer(strSrc,pRead,16);
//					m_pLog->PrintBuffer(strDst,m_paramBuffer+uiTransfer,16);
//				}
//				else
//				{
//					m_pLog->PrintBuffer(strSrc,pRead,uiWriteByte);
//					m_pLog->PrintBuffer(strDst,m_paramBuffer+uiTransfer,uiWriteByte);
//				}
//				m_pLog->Record("Read:%s",strSrc.c_str());
//				m_pLog->Record("Compare:%s",strDst.c_str());
//			}
			uiTransfer += uiWriteByte;
			currentByte += uiWriteByte;
			uiBegin += uiLen;
			uiFileSize -= uiWriteByte;
			
		}
	}
	
	delete []pRead;
	return true;
}


bool CRKAndroidDevice::MakeParamFileBuffer(STRUCT_RKIMAGE_ITEM &entry)
{
	bool bRet;
	UINT uiFileBufferSize;
	DWORD dwFWOffset;
	
	dwFWOffset = m_pImage->FWOffset;
	uiFileBufferSize = 2*entry.size;
	m_uiParamFileSize = entry.size;
	PBYTE pBuffer=NULL;
	pBuffer = new BYTE[uiFileBufferSize];
	if (!pBuffer)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:MakeParamFileBuffer-->New memory failed"));
		}
		return false;
	}
	memset(pBuffer,0,uiFileBufferSize);
	bRet = m_pImage->GetData(dwFWOffset+entry.offset,entry.size,pBuffer);
	if (!bRet)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:MakeParamFileBuffer-->GetFileData failed"));
		}
		delete[] pBuffer;
		pBuffer = NULL;
		return false;
	}
	//判断是否要修改Paramter文件内容,手机升级需要将paramter文件中的partition部分数据改成以字节为单位进行偏移
	
	UINT uiParamSec;
	if (m_uiParamFileSize%512==0)
	{
		uiParamSec = m_uiParamFileSize/512;
	}
	else
		uiParamSec = (m_uiParamFileSize+512)/512;

	if (m_paramBuffer)
	{
		delete []m_paramBuffer;
		m_paramBuffer = NULL;
	}
	m_paramBuffer = new BYTE[uiParamSec*512];
	if (!m_paramBuffer)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:MakeParamFileBuffer-->new memory failed"));
		}
		delete []pBuffer;
		return false;
	}
	memset(m_paramBuffer,0,uiParamSec*512);
	memcpy(m_paramBuffer,pBuffer,m_uiParamFileSize);
	delete []pBuffer;
	return true;
}
bool CRKAndroidDevice::CheckParamPartSize(STRUCT_RKIMAGE_HDR &rkImageHead,int iParamPos)
{
//	UINT uiParamPartSize;
//	int i;
//	uiParamPartSize = 0xFFFFFFFF;
//	for (i=0;i<rkImageHead.item_count;i++)
//	{
//		if (i!=iParamPos)
//		{
//			if (rkImageHead.item[i].flash_offset<uiParamPartSize)
//			{
//				uiParamPartSize = rkImageHead.item[i].flash_offset;
//			}
//		}
//	}
	if (!GetParameterPartSize(rkImageHead.item[iParamPos]))
	{
		return false;
	}
	if (m_uiParamFileSize>rkImageHead.item[iParamPos].part_size/8*512)//是否满足存放8份
	{
		return false;
	}

	return true;
}
bool CRKAndroidDevice::IsExistSector3Crc(PRKANDROID_IDB_SEC2 pSec)
{
	if (!pSec)
	{
		return false;
	}
	
	if (strcmp(pSec->szCrcTag,"CRC")==0)
	{
		m_bExistSector3Crc = true;
		m_usSector3Crc = pSec->usSec3Crc;
	}
	return true;
}
bool CRKAndroidDevice::ParsePartitionInfo(string &strPartInfo,string &strName,UINT &uiOffset,UINT &uiLen)
{
	string::size_type pos,prevPos;
	string strOffset,strLen;
	int iCount;
	prevPos = pos = 0;
	if (strPartInfo.size()<=0)
	{
		return false;
	}
	pos = strPartInfo.find('@');
	if (pos==string::npos)
	{
		return false;
	}
	strLen = strPartInfo.substr(prevPos,pos-prevPos);
	strLen.erase(0,strLen.find_first_not_of(" "));
	strLen.erase(strLen.find_last_not_of(" ")+1);
	if (strchr(strLen.c_str(),'-'))
	{
		uiLen = 0xFFFFFFFF;
	}
	else
	{
		iCount = sscanf(strLen.c_str(),"0x%x",&uiLen);
		if (iCount!=1)
		{
			return false;
		}
	}
	
	prevPos = pos +1;
	pos = strPartInfo.find('(',prevPos);
	if (pos==string::npos)
	{
		return false;
	}
	strOffset = strPartInfo.substr(prevPos,pos-prevPos);
	strOffset.erase(0,strOffset.find_first_not_of(" "));
	strOffset.erase(strOffset.find_last_not_of(" ")+1);
	iCount = sscanf(strOffset.c_str(),"0x%x",&uiOffset);
	if (iCount!=1)
	{
		return false;
	}
	
	prevPos = pos +1;
	pos = strPartInfo.find(')',prevPos);
	if (pos==string::npos)
	{
		return false;
	}
	strName = strPartInfo.substr(prevPos,pos-prevPos);
	strName.erase(0,strName.find_first_not_of(" "));
	strName.erase(strName.find_last_not_of(" ")+1);
	
	return true;
}
bool CRKAndroidDevice::GetParameterPartSize(STRUCT_RKIMAGE_ITEM &paramItem)
{
	PBYTE pParamBuf=NULL;
	pParamBuf = new BYTE[paramItem.size-12+1];
	if (!pParamBuf)
	{
		return false;
	}
	memset(pParamBuf,0,paramItem.size-12+1);
	bool bRet;
	bRet = m_pImage->GetData(m_pImage->FWOffset+paramItem.offset+8,paramItem.size-12,pParamBuf);
	if (!bRet)
	{
		delete []pParamBuf;
		return false;
	}
	string strParamFile = (char *)pParamBuf;
	stringstream paramStream(strParamFile);
	delete []pParamBuf;

	string strLine,strPartition,strPartInfo,strPartName;
	string::size_type line_size,pos,posColon,posComma;
	UINT uiPartOffset,uiPartSize;
	while (!paramStream.eof())
	{
		getline(paramStream,strLine);
		line_size = strLine.size();
		if (line_size<=0)
		{
			continue;
		}
		if (strLine[line_size-1]=='\r')
		{
			strLine = strLine.substr(0,line_size-1);
		}
		if (strLine.size()<=0)
		{
			continue;
		}
		if (strLine[0]=='#')
		{
			continue;
		}
		pos = strLine.find("mtdparts");
		if (pos==string::npos)
		{
			continue;
		}
		posColon = strLine.find(':',pos);
		if (posColon==string::npos)
		{
			continue;
		}
		strPartition = strLine.substr(posColon+1);
		//提取分区信息
		pos = 0;
		posComma = strPartition.find(',',pos);
		while (posComma!=string::npos)
		{
			strPartInfo = strPartition.substr(pos,posComma-pos);
			bRet = ParsePartitionInfo(strPartInfo,strPartName,uiPartOffset,uiPartSize);
			if (!bRet)
			{
				if (m_pLog)
				{
					m_pLog->Record(_T("ERROR:GetParameterPartSize-->ParsePartitionInfo failed"));
				}
				return false;
			}
			paramItem.part_size = uiPartOffset;
			return true;
		}
	}
	return false;
}

bool CRKAndroidDevice::GetPublicKey(unsigned char *pKey,unsigned int &nKeySize)
{
	int i,j,iRet,nRsaByte;
	bool bRet;
	BYTE bData[SECTOR_SIZE*8];
	PRKANDROID_IDB_SEC0 pSec0=(PRKANDROID_IDB_SEC0)bData;
	PRK_SECURE_HEADER pSecureHdr=(PRK_SECURE_HEADER)(bData+SECTOR_SIZE*4);
	string strOutput;
	bRet = GetFlashInfo();
	if (!bRet)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:GetPublicKey-->GetFlashInfo failed"));
		}
		return false;
	}
	if ( !BuildBlockStateMap(0) )
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:GetPublicKey-->BuildBlockStateMap failed"));
		}
		return false;
	}
	
	FindAllIDB();
//	sleep(1);
//	if (m_pLog)
//	{
//		m_pLog->Record(_T("INFO:GetPublicKey-->IDblock count=%d."),m_oldIDBCounts);
//	}
	if (m_oldIDBCounts<=0)
	{
		if (m_pLog)
		{
			m_pLog->Record(_T("ERROR:GetPublicKey-->IDblock count=%d."),m_oldIDBCounts);
		}
		return false;
	}
	for(i=0;i<m_oldIDBCounts;i++)
	{
		iRet = m_pComm->RKU_ReadSector(m_idBlockOffset[i]*m_flashInfo.uiSectorPerBlock, 8,bData);
//		sleep(1);
		if( iRet!=ERR_SUCCESS )
		{
			if (m_pLog)
			{
				m_pLog->Record(_T("ERROR:GetPublicKey-->RKU_ReadSector failed,RetCode(%d)"),iRet);
			}
			return false;
		}
		
		P_RC4(bData,SECTOR_SIZE);

		if (pSec0->uiRc4Flag==0)
		{
//			if (m_pLog)
//			{
//				m_pLog->PrintBuffer(strOutput,bData+4*512,512,16);
//				m_pLog->Record("INFO:secure header\n%s",strOutput.c_str());
//			}
			for(j=0;j<4;j++)
				P_RC4(bData+SECTOR_SIZE*(j+4),SECTOR_SIZE);
//			if (m_pLog)
//			{
//				m_pLog->PrintBuffer(strOutput,bData+4*512,512,16);
//				m_pLog->Record("INFO:secure header rc4\n%s",strOutput.c_str());
//			}
		}
//		if (m_pLog)
//		{
//			m_pLog->Record("INFO:secure header tag=0x%x",pSecureHdr->uiTag);
//		}
		if (pSecureHdr->uiTag==0x4B415352)
		{
			nRsaByte = pSecureHdr->usRsaBit/8;
			*((USHORT *)pKey) = pSecureHdr->usRsaBit;
			for(j=0;j<nRsaByte;j++)
				*(pKey+j+2) = pSecureHdr->nFactor[nRsaByte-j-1];
			for(j=0;j<nRsaByte;j++)
				*(pKey+j+2+nRsaByte) = pSecureHdr->eFactor[nRsaByte-j-1];
			nKeySize = nRsaByte*2+2;
//			if (m_pLog)
//			{
//				m_pLog->PrintBuffer(strOutput,pKey,nKeySize,16);
//				m_pLog->Record("INFO:Key\n%s",strOutput.c_str());
//			}
			return true;
		}
		
	}
	
	return false;
}

