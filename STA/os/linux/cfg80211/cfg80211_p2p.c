/****************************************************************************
 * Ralink Tech Inc.
 * Taiwan, R.O.C.
 *
 * (c) Copyright 2013, Ralink Technology, Inc.
 *
 * All rights reserved. Ralink's source code is an unpublished work and the
 * use of a copyright notice does not imply otherwise. This source code
 * contains confidential trade secret material of Ralink Tech. Any attemp
 * or participation in deciphering, decoding, reverse engineering or in any
 * way altering the source code is stricitly prohibited, unless the prior
 * written consent of Ralink Technology, Inc. is obtained.
 ***************************************************************************/

/****************************************************************************
 
	Abstract:

	All related CFG80211 P2P function body.

	History:

***************************************************************************/

#define RTMP_MODULE_OS

#ifdef RT_CFG80211_SUPPORT

#include "rt_config.h"

#if defined (HE_BD_CFG80211_SUPPORT) && defined (BD_KERNEL_VER)
#undef  LINUX_VERSION_CODE
#define LINUX_VERSION_CODE KERNEL_VERSION(2,6,39)
#endif /* HE_BD_CFG80211_SUPPORT && BD_KERNEL_VER */


UCHAR CFG_WPS_OUI[4] = {0x00, 0x50, 0xf2, 0x04};
UCHAR CFG_P2POUIBYTE[4] = {0x50, 0x6f, 0x9a, 0x9}; /* spec. 1.14 OUI */

BUILD_TIMER_FUNCTION(CFG80211RemainOnChannelTimeout);

static 
VOID CFG80211_RemainOnChannelInit(RTMP_ADAPTER *pAd)
{
	if (pAd->cfg80211_ctrl.Cfg80211RocTimerInit == FALSE)
	{
		CFG80211DBG(RT_DEBUG_TRACE, ("CFG80211_ROC : INIT Cfg80211RocTimer\n"));
		RTMPInitTimer(pAd, &pAd->cfg80211_ctrl.Cfg80211RocTimer, 
			GET_TIMER_FUNCTION(CFG80211RemainOnChannelTimeout), pAd, FALSE);
		pAd->cfg80211_ctrl.Cfg80211RocTimerInit = TRUE;
	}
}

VOID CFG80211RemainOnChannelTimeout(
	PVOID SystemSpecific1, PVOID FunctionContext,
	PVOID SystemSpecific2, PVOID SystemSpecific3)
{
	RTMP_ADAPTER *pAd = (RTMP_ADAPTER *) FunctionContext;
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;
	
	DBGPRINT(RT_DEBUG_INFO, ("CFG80211_ROC: RemainOnChannelTimeout\n"));
	
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
#define RESTORE_COM_CH_TIME 100
	APCLI_STRUCT *pApCliEntry = &pAd->ApCfg.ApCliTab[MAIN_MBSSID];

	if (pApCliEntry->Valid && 
	     	RTMP_CFG80211_VIF_P2P_CLI_ON(pAd) && 
            	(pAd->LatchRfRegs.Channel != pApCliEntry->MlmeAux.Channel))
	{
		/* Extend the ROC_TIME for Common Channel When P2P_CLI on */
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_ROC: ROC_Timeout APCLI_ON Channel: %d\n", 
								pApCliEntry->MlmeAux.Channel));

        	AsicSwitchChannel(pAd, pApCliEntry->MlmeAux.Channel, FALSE);
        	AsicLockChannel(pAd, pApCliEntry->MlmeAux.Channel);

		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_NULL: P2P_CLI PWR_ACTIVE ROC_END\n"));
		CFG80211_P2pClientSendNullFrame(pAd, PWR_ACTIVE);
		
#ifdef CONFIG_STA_SUPPORT
		if (INFRA_ON(pAd))
		{
			DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_NULL: CONCURRENT STA PWR_ACTIVE ROC_END\n"));
			RTMPSendNullFrame(pAd, pAd->CommonCfg.TxRate, 
								(OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED) ? TRUE:FALSE),
								pAd->CommonCfg.bAPSDForcePowerSave ? PWR_SAVE : pAd->StaCfg.Psm);			
		}
#endif /*CONFIG_STA_SUPPORT*/
		RTMPSetTimer(&pCfg80211_ctrl->Cfg80211RocTimer, RESTORE_COM_CH_TIME);
	}
	else if (INFRA_ON(pAd) &&
	   	     (
			 (pAd->StaCfg.wdev.bw == HT_BW_40) && (pAd->CommonCfg.BBPCurrentBW == HT_BW_20)  /*&&  
			 (pAd->LatchRfRegs.Channel != pAd->StaCfg.wdev.CentralChannel) && (pAd->StaCfg.wdev.CentralChannel != 0)*/)
			 || ((pAd->StaCfg.wdev.bw == HT_BW_20) && (pAd->LatchRfRegs.Channel != pAd->StaCfg.wdev.channel) && (pAd->StaCfg.wdev.channel != 0))
			 )
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_ROC: ROC_Timeout INFRA_ON Channel: %d\n", 
									pAd->CommonCfg.Channel));

	if (pAd->StaCfg.wdev.bw == HT_BW_40)
	{
		bbp_set_bw(pAd, BW_40);
		AsicSwitchChannel(pAd, pAd->StaCfg.wdev.CentralChannel, FALSE);
	       AsicLockChannel(pAd, pAd->StaCfg.wdev.CentralChannel);
		pAd->CommonCfg.Channel = pAd->StaCfg.wdev.channel;
		pAd->CommonCfg.CentralChannel = pAd->StaCfg.wdev.CentralChannel;
		OS_WAIT(10);

	CmdACQueue_Control(pAd, 2, 0, 15);
//
	}
	else
	{
		bbp_set_bw(pAd, BW_20);
		AsicSwitchChannel(pAd, pAd->StaCfg.wdev.channel, FALSE);
	       AsicLockChannel(pAd, pAd->StaCfg.wdev.channel);
		OS_WAIT(10);

		CmdACQueue_Control(pAd, 2, 0, 15);

		pAd->CommonCfg.Channel = pAd->StaCfg.wdev.channel;
		pAd->CommonCfg.CentralChannel = pAd->StaCfg.wdev.channel;			
	}
#ifdef CONFIG_STA_SUPPORT
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_NULL: INFRA_ON PWR_ACTIVE ROC_END\n"));
		RTMPSendNullFrame(pAd, pAd->CommonCfg.TxRate, 
							(OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED) ? TRUE:FALSE),
							pAd->CommonCfg.bAPSDForcePowerSave ? PWR_SAVE : pAd->StaCfg.Psm);
#endif /*CONFIG_STA_SUPPORT*/		
		RTMPSetTimer(&pCfg80211_ctrl->Cfg80211RocTimer, RESTORE_COM_CH_TIME);		    	 
	}
	else
#endif /*RT_CFG80211_P2P_CONCURRENT_DEVICE */		
	{
/* CFG TODO: move to cfg802_util */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
		PWIRELESS_DEV pwdev = NULL;
		pwdev = pCfg80211_ctrl->Cfg80211ChanInfo.pWdev;
		cfg80211_remain_on_channel_expired(pwdev, pCfg80211_ctrl->Cfg80211ChanInfo.cookie,
            		pCfg80211_ctrl->Cfg80211ChanInfo.chan, GFP_ATOMIC);
#else		
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
        	cfg80211_remain_on_channel_expired( CFG80211_GetEventDevice(pAd),	
        		pCfg80211_ctrl->Cfg80211ChanInfo.cookie, pCfg80211_ctrl->Cfg80211ChanInfo.chan, 
        		pCfg80211_ctrl->Cfg80211ChanInfo.ChanType, GFP_ATOMIC);
#endif /* LINUX_VERSION_CODE 2.6.34 */
#endif /* LINUX_VERSION_CODE 3.8.0 */

		pCfg80211_ctrl->Cfg80211RocTimerRunning = FALSE;
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_ROC: RemainOnChannelTimeout -- FINISH\n"));
	}
		
}

/* Set a given time on specific channel to listen action Frame */
BOOLEAN CFG80211DRV_OpsRemainOnChannel(VOID *pAdOrg, VOID *pData, UINT32 duration)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_CHAN *pChanInfo;
	BOOLEAN Cancelled;
	PCFG80211_CTRL pCfg80211_ctrl = &pAd->cfg80211_ctrl;
	UCHAR lock_channel;

	pChanInfo = (CMD_RTPRIV_IOCTL_80211_CHAN *) pData;

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
        PWIRELESS_DEV pwdev = NULL;
        pwdev = pChanInfo->pWdev;
#endif /* LINUX_VERSION_CODE: 3.6.0 */
	
	CFG80211DBG(RT_DEBUG_INFO, ("%s\n", __FUNCTION__));
	
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
	APCLI_STRUCT *pApCliEntry = &pAd->ApCfg.ApCliTab[MAIN_MBSSID];
	/* Will be Exit the ApCli Connected Channel so send Null frame on current */
	if (pApCliEntry->Valid && RTMP_CFG80211_VIF_P2P_CLI_ON(pAd) &&
        (pApCliEntry->MlmeAux.Channel != pChanInfo->ChanId) &&
        (pApCliEntry->MlmeAux.Channel == pAd->LatchRfRegs.Channel))	
	{
        DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_NULL: APCLI PWR_SAVE ROC_START\n"));
        CFG80211_P2pClientSendNullFrame(pAd, PWR_SAVE);
	}

	if ( INFRA_ON(pAd) &&
	    ((pAd->CommonCfg.Channel != pChanInfo->ChanId) || (pAd->CommonCfg.CentralChannel != pChanInfo->ChanId)) &&
        ((pAd->CommonCfg.Channel == pAd->LatchRfRegs.Channel) || (pAd->CommonCfg.CentralChannel== pAd->LatchRfRegs.Channel)))	
	{
	CmdACQueue_Control(pAd, 0, 0, 15);
	OS_WAIT(20);

    	DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_NULL: STA PWR_SAVE ROC_START\n"));
		RTMPSendNullFrame(pAd, pAd->CommonCfg.TxRate, 
						  (OPSTATUS_TEST_FLAG(pAd, fOP_STATUS_WMM_INUSED) ? TRUE:FALSE),
						  PWR_SAVE);				
	}	
#endif /*RT_CFG80211_P2P_CONCURRENT_DEVICE */

	/* Channel Switch Case:
	 * 1. P2P_FIND:    [SOCIAL_CH]->[COM_CH]->[ROC_CH]--N_TUs->[ROC_TIMEOUT]
	 *                 Set COM_CH to ROC_CH for merge COM_CH & ROC_CH dwell section.
     	 *	 
	 * 2. OFF_CH_WAIT: [ROC_CH]--200ms-->[ROC_TIMEOUT]->[COM_CH]
	 *                 Most in GO case.
	 * 
	 */
	//lock_channel = CFG80211_getCenCh(pAd, pChanInfo->ChanId);
	lock_channel = pChanInfo->ChanId;
	if (pAd->LatchRfRegs.Channel != lock_channel
	|| (INFRA_ON(pAd) && (pAd->StaCfg.wdev.bw == BW_40))
) 
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_PKT: ROC CHANNEL_LOCK %d\n", pChanInfo->ChanId));
		//AsicSetChannel(pAd, lock_channel, BW_20, EXTCHA_NONE, FALSE);
		bbp_set_bw(pAd, BW_20);

		AsicSwitchChannel(pAd, lock_channel, FALSE);
		AsicLockChannel(pAd, lock_channel);
	}
	else
	{
		DBGPRINT(RT_DEBUG_INFO, ("80211> ComCH == ROC_CH \n"));
	}
	
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,6,0))
        cfg80211_ready_on_channel(pwdev,  pChanInfo->cookie, pChanInfo->chan, duration, GFP_ATOMIC);	
#else
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34))
	cfg80211_ready_on_channel(CFG80211_GetEventDevice(pAd), pChanInfo->cookie, 
				  pChanInfo->chan, pChanInfo->ChanType, duration, GFP_ATOMIC);
#endif /* LINUX_VERSION_CODE: 2.6.34 */
#endif /* LINUX_VERSION_CODE: 3.6.0 */

	NdisCopyMemory(&pCfg80211_ctrl->Cfg80211ChanInfo, pChanInfo, sizeof(CMD_RTPRIV_IOCTL_80211_CHAN));

	CFG80211_RemainOnChannelInit(pAd);
	
	if (pCfg80211_ctrl->Cfg80211RocTimerRunning == TRUE)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_ROC : CANCEL Cfg80211RocTimer\n"));
		RTMPCancelTimer(&pCfg80211_ctrl->Cfg80211RocTimer, &Cancelled);
		pCfg80211_ctrl->Cfg80211RocTimerRunning = FALSE;
	}

	RTMPSetTimer(&pCfg80211_ctrl->Cfg80211RocTimer, duration + 20);
	pCfg80211_ctrl->Cfg80211RocTimerRunning = TRUE;

	return TRUE;	
}

BOOLEAN CFG80211DRV_OpsCancelRemainOnChannel(VOID *pAdOrg, UINT32 cookie)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	BOOLEAN Cancelled;
	CFG80211DBG(RT_DEBUG_TRACE, ("%s\n", __FUNCTION__));

	if (pAd->cfg80211_ctrl.Cfg80211RocTimerRunning == TRUE)
	{
		DBGPRINT(RT_DEBUG_TRACE, ("CFG_ROC : CANCEL Cfg80211RocTimer\n"));
		RTMPCancelTimer(&pAd->cfg80211_ctrl.Cfg80211RocTimer, &Cancelled);
		CmdACQueue_Control(pAd, 2, 0, 15);
		pAd->cfg80211_ctrl.Cfg80211RocTimerRunning = FALSE;
	}
}

INT CFG80211_setPowerMgmt(VOID *pAdCB, UINT Enable)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;

	DBGPRINT(RT_DEBUG_TRACE, ("@@@ %s: %d\n", __FUNCTION__, Enable));

#ifdef RT_CFG80211_P2P_SUPPORT		
	pAd->cfg80211_ctrl.bP2pCliPmEnable = Enable;
#endif /* RT_CFG80211_P2P_SUPPORT */

	return 0;	
}

#if defined(RT_CFG80211_P2P_CONCURRENT_DEVICE) || defined(CFG80211_MULTI_STA)
VOID CFG80211_P2pClientSendNullFrame(VOID *pAdCB, INT PwrMgmt)
{
    PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
    MAC_TABLE_ENTRY *pEntry;

    pEntry = MacTableLookup(pAd, pAd->ApCfg.ApCliTab[MAIN_MBSSID].MlmeAux.Bssid);
    if (pEntry == NULL)
    {
            DBGPRINT(RT_DEBUG_TRACE, ("CFG80211_ROC: Can't Find In Table: %02x:%02x:%02x:%02x:%02x:%02x\n",
                                               PRINT_MAC(pAd->ApCfg.ApCliTab[MAIN_MBSSID].MlmeAux.Bssid)));
    }
    else
    {
            ApCliRTMPSendNullFrame(pAd,
                                   RATE_6,
                                   (CLIENT_STATUS_TEST_FLAG(pEntry, fCLIENT_STATUS_WMM_CAPABLE)) ? TRUE:FALSE,
                                   pEntry, PwrMgmt);
            OS_WAIT(20);
    }
}

VOID CFG80211DRV_P2pClientKeyAdd(VOID *pAdOrg, VOID *pData)
{

	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_KEY *pKeyInfo;
	
    	DBGPRINT(RT_DEBUG_TRACE, ("CFG Debug: CFG80211DRV_P2pClientKeyAdd\n"));
    	pKeyInfo = (CMD_RTPRIV_IOCTL_80211_KEY *)pData;
	
	if (pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP40 || pKeyInfo->KeyType == RT_CMD_80211_KEY_WEP104)
		;
	else
	{	
		INT 	BssIdx;
		PAPCLI_STRUCT pApCliEntry;
		MAC_TABLE_ENTRY	*pMacEntry=(MAC_TABLE_ENTRY *)NULL;
		STA_TR_ENTRY *tr_entry;

		BssIdx = pAd->ApCfg.BssidNum + MAX_MESH_NUM + MAIN_MBSSID;
		pApCliEntry = &pAd->ApCfg.ApCliTab[MAIN_MBSSID];
		struct wifi_dev *p2p_wdev = NULL;
		p2p_wdev = &(pApCliEntry->wdev);

		pMacEntry = &pAd->MacTab.Content[pApCliEntry->MacTabWCID]; 
		tr_entry = &pAd->MacTab.tr_entry[pMacEntry->wcid];
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,37))
        	if (pKeyInfo->bPairwise == FALSE )
#else
        	if (pKeyInfo->KeyId > 0)
#endif		
		{
			
			if (pApCliEntry->wdev.WepStatus == Ndis802_11Encryption3Enabled)
			{
				printk("APCLI: Set AES Security Set. [%d] (GROUP) %d\n", BssIdx, pKeyInfo->KeyLen);
				NdisZeroMemory(&pApCliEntry->SharedKey[pKeyInfo->KeyId], sizeof(CIPHER_KEY));  
				pApCliEntry->SharedKey[pKeyInfo->KeyId].KeyLen = LEN_TK;
				NdisMoveMemory(pApCliEntry->SharedKey[pKeyInfo->KeyId].Key, pKeyInfo->KeyBuf, pKeyInfo->KeyLen);
				
				pApCliEntry->SharedKey[pKeyInfo->KeyId].CipherAlg = CIPHER_AES;

				AsicAddSharedKeyEntry(pAd, BssIdx, pKeyInfo->KeyId, 
						      &pApCliEntry->SharedKey[pKeyInfo->KeyId]);

					
				RTMPAddWcidAttributeEntry(pAd, BssIdx, pKeyInfo->KeyId, 
							  pApCliEntry->SharedKey[pKeyInfo->KeyId].CipherAlg, 
							  NULL);				

				if (INFRA_ON(pAd))	
				{
					if((pAd->StaCfg.wdev.bw != p2p_wdev->bw) && ((pAd->StaCfg.wdev.channel == p2p_wdev->channel)))
					{
						pAd->Mlme.bStartScc = TRUE;
						AsicSwitchChannel(pAd, pAd->StaCfg.wdev.CentralChannel, FALSE);
						AsicLockChannel(pAd, pAd->StaCfg.wdev.CentralChannel);	
						bbp_set_bw(pAd, BW_40);
					}
				}

				

#ifdef MT_MAC
				if (pAd->chipCap.hif_type == HIF_MT)
	        			CmdProcAddRemoveKey(pAd, 0, BSS0, pKeyInfo->KeyId, APCLI_MCAST_WCID, 
						    SHAREDKEYTABLE, &pApCliEntry->SharedKey[pKeyInfo->KeyId], 
						    BROADCAST_ADDR);
#endif /* MT_MAC*/										  
				if (pMacEntry->AuthMode >= Ndis802_11AuthModeWPA)
				{
					/* set 802.1x port control */
					tr_entry->PortSecured = WPA_802_1X_PORT_SECURED;
					pMacEntry->PrivacyFilter = Ndis802_11PrivFilterAcceptAll;
				}
			}
		}	
		else
		{	
			if(pMacEntry)
			{
				printk("APCLI: Set AES Security Set. [%d] (PAIRWISE) %d\n", BssIdx, pKeyInfo->KeyLen);
				NdisZeroMemory(&pMacEntry->PairwiseKey, sizeof(CIPHER_KEY));  
				pMacEntry->PairwiseKey.KeyLen = LEN_TK;
				
				NdisCopyMemory(&pMacEntry->PTK[OFFSET_OF_PTK_TK], pKeyInfo->KeyBuf, OFFSET_OF_PTK_TK);
				NdisMoveMemory(pMacEntry->PairwiseKey.Key, &pMacEntry->PTK[OFFSET_OF_PTK_TK], pKeyInfo->KeyLen);
				
				pMacEntry->PairwiseKey.CipherAlg = CIPHER_AES;
				
				AsicAddPairwiseKeyEntry(pAd, (UCHAR)pMacEntry->Aid, &pMacEntry->PairwiseKey);
				RTMPSetWcidSecurityInfo(pAd, BssIdx, 0, pMacEntry->PairwiseKey.CipherAlg, pMacEntry->Aid, PAIRWISEKEYTABLE);

#ifdef MT_MAC
    			if (pAd->chipCap.hif_type == HIF_MT)
            			CmdProcAddRemoveKey(pAd, 0, pMacEntry->func_tb_idx, 0, pMacEntry->wcid, 
						    PAIRWISEKEYTABLE, &pMacEntry->PairwiseKey, 
						    pMacEntry->Addr);
#endif /* MT_MAC*/

#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE

			// p2p cli key done case!!
			//check if Infra STA is port secured
			if (INFRA_ON(pAd))
			{
				if (!(pAd->StaCfg.wpa_supplicant_info.WpaSupplicantUP & WPA_SUPPLICANT_ENABLE_WPS)) //WPS not under goining!
				{
					struct wifi_dev *wdev = &pAd->StaCfg.wdev;
					BOOLEAN bStart = FALSE;
					PAPCLI_STRUCT pApCliEntry = pApCliEntry = &pAd->ApCfg.ApCliTab[MAIN_MBSSID];
					struct wifi_dev *p2p_wdev = NULL;			
					p2p_wdev = &pApCliEntry->wdev;
					/*check the security setting  OPEN ==> just start; security ==> check key done then start*/
					switch (wdev->AuthMode)
					{
						case Ndis802_11AuthModeAutoSwitch:
							//fall through
						case Ndis802_11AuthModeShared:
						case Ndis802_11AuthModeWPAPSK:
						case Ndis802_11AuthModeWPA2PSK:
						case Ndis802_11AuthModeWPA1PSKWPA2PSK:
						case Ndis802_11AuthModeWPA:
						case Ndis802_11AuthModeWPA2:
						case Ndis802_11AuthModeWPA1WPA2:
							if (pAd->StaCfg.wdev.PortSecured ==WPA_802_1X_PORT_SECURED)
							{
								bStart = TRUE;
							}
							break;
						default:
							bStart = TRUE;
							break;
				       }
				
					if (bStart == TRUE)
					{
						if ((wdev->bw != p2p_wdev->bw) && ((wdev->channel == p2p_wdev->channel)))
						{
							pAd->Mlme.bStartScc = TRUE;				
						}				
					}
				}
			}
#endif /*RT_CFG80211_P2P_CONCURRENT_DEVICE */


			}
			else	
			{
				printk("APCLI: Set AES Security Set. (PAIRWISE) But pMacEntry NULL\n");
			}			
		}		
	}
}

VOID CFG80211DRV_SetP2pCliAssocIe(VOID *pAdOrg, VOID *pData, UINT ie_len)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	APCLI_STRUCT *apcli_entry;
	hex_dump("P2PCLI=", pData, ie_len);

	apcli_entry = &pAd->ApCfg.ApCliTab[MAIN_MBSSID];
	
	if (ie_len > 0)	
	{
		if (apcli_entry->wpa_supplicant_info.pWpaAssocIe)
		{
			os_free_mem(NULL, apcli_entry->wpa_supplicant_info.pWpaAssocIe);
			apcli_entry->wpa_supplicant_info.pWpaAssocIe = NULL;
		}

		os_alloc_mem(NULL, (UCHAR **)&apcli_entry->wpa_supplicant_info.pWpaAssocIe, ie_len);
		if (apcli_entry->wpa_supplicant_info.pWpaAssocIe)
		{
			apcli_entry->wpa_supplicant_info.WpaAssocIeLen = ie_len;
			NdisMoveMemory(apcli_entry->wpa_supplicant_info.pWpaAssocIe, pData, apcli_entry->wpa_supplicant_info.WpaAssocIeLen);
		}
		else
			apcli_entry->wpa_supplicant_info.WpaAssocIeLen = 0;
	}
	else
	{
		if (apcli_entry->wpa_supplicant_info.pWpaAssocIe)
		{
			os_free_mem(NULL, apcli_entry->wpa_supplicant_info.pWpaAssocIe);
			apcli_entry->wpa_supplicant_info.pWpaAssocIe = NULL;
		}
		apcli_entry->wpa_supplicant_info.WpaAssocIeLen = 0;
	}
}

/* For P2P_CLIENT Connection Setting in AP_CLI SM */
BOOLEAN CFG80211DRV_P2pClientConnect(VOID *pAdOrg, VOID *pData)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdOrg;
	CMD_RTPRIV_IOCTL_80211_CONNECT *pConnInfo;
	UCHAR Connect_SSID[NDIS_802_11_LENGTH_SSID];
	UINT32 Connect_SSIDLen;
	
	APCLI_STRUCT *apcli_entry;
	apcli_entry = &pAd->ApCfg.ApCliTab[MAIN_MBSSID];
	
	POS_COOKIE pObj = (POS_COOKIE) pAd->OS_Cookie;
	pObj->ioctl_if_type = INT_APCLI;
	
	pConnInfo = (CMD_RTPRIV_IOCTL_80211_CONNECT *)pData;
	
	DBGPRINT(RT_DEBUG_TRACE, ("APCLI Connection onGoing.....\n"));

	Connect_SSIDLen = pConnInfo->SsidLen;
	if (Connect_SSIDLen > NDIS_802_11_LENGTH_SSID)
		Connect_SSIDLen = NDIS_802_11_LENGTH_SSID;
	
	memset(&Connect_SSID, 0, sizeof(Connect_SSID));
	memcpy(Connect_SSID, pConnInfo->pSsid, Connect_SSIDLen);

	apcli_entry->wpa_supplicant_info.WpaSupplicantUP = WPA_SUPPLICANT_ENABLE;

	/* Check the connection is WPS or not */
	if (pConnInfo->bWpsConnection) 
	{
		DBGPRINT(RT_DEBUG_TRACE, ("AP_CLI WPS Connection onGoing.....\n"));
		apcli_entry->wpa_supplicant_info.WpaSupplicantUP |= WPA_SUPPLICANT_ENABLE_WPS;
	}		

	/* Set authentication mode */
	if (pConnInfo->WpaVer == 2)
	{
		if (!pConnInfo->FlgIs8021x == TRUE) 
		{
			DBGPRINT(RT_DEBUG_TRACE,("APCLI WPA2PSK\n"));
			Set_ApCli_AuthMode_Proc(pAd, "WPA2PSK");
		}
	}
	else if (pConnInfo->WpaVer == 1)
	{
		if (!pConnInfo->FlgIs8021x) 
		{
			DBGPRINT(RT_DEBUG_TRACE,("APCLI WPAPSK\n"));
			Set_ApCli_AuthMode_Proc(pAd, "WPAPSK");
		}
	}
	else
		Set_ApCli_AuthMode_Proc(pAd, "OPEN");	

	/* Set PTK Encryption Mode */
	if (pConnInfo->PairwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_CCMP) {
		DBGPRINT(RT_DEBUG_TRACE,("AES\n"));
		Set_ApCli_EncrypType_Proc(pAd, "AES");
	}
	else if (pConnInfo->PairwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_TKIP) {
		DBGPRINT(RT_DEBUG_TRACE,("TKIP\n"));
		Set_ApCli_EncrypType_Proc(pAd, "TKIP");
	}
	else if (pConnInfo->PairwiseEncrypType & RT_CMD_80211_CONN_ENCRYPT_WEP)
	{
		DBGPRINT(RT_DEBUG_TRACE,("WEP\n"));
		Set_ApCli_EncrypType_Proc(pAd, "WEP");
	}
	
	
	if (pConnInfo->pBssid != NULL)
	{
		NdisZeroMemory(apcli_entry->CfgApCliBssid, MAC_ADDR_LEN);
		NdisCopyMemory(apcli_entry->CfgApCliBssid, pConnInfo->pBssid, MAC_ADDR_LEN);
	}
	
	OPSTATUS_SET_FLAG(pAd, fOP_AP_STATUS_MEDIA_STATE_CONNECTED);

	pAd->cfg80211_ctrl.FlgCfg80211Connecting = TRUE;
	Set_ApCli_Ssid_Proc(pAd, (RTMP_STRING *)Connect_SSID);
	Set_ApCli_Enable_Proc(pAd, "1");
	CFG80211DBG(RT_DEBUG_OFF, ("80211> APCLI CONNECTING SSID = %s\n", Connect_SSID));

	return TRUE;	
}

VOID CFG80211_P2pClientConnectResultInform(
	VOID *pAdCB, UCHAR *pBSSID, UCHAR *pReqIe, UINT32 ReqIeLen, 
	UCHAR *pRspIe, UINT32 RspIeLen, UCHAR FlgIsSuccess)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;

	CFG80211OS_P2pClientConnectResultInform(pAd->ApCfg.ApCliTab[MAIN_MBSSID].wdev.if_dev, pBSSID, 
					pReqIe, ReqIeLen, pRspIe, RspIeLen, FlgIsSuccess);

	pAd->cfg80211_ctrl.FlgCfg80211Connecting = FALSE;
}

VOID CFG80211_LostP2pGoInform(VOID *pAdCB)
{
	PRTMP_ADAPTER pAd = (PRTMP_ADAPTER)pAdCB;
	CFG80211_CB *p80211CB = pAd->pCfg80211_CB;
	PNET_DEV pNetDev = NULL;
	UINT checkDevType = RT_CMD_80211_IFTYPE_P2P_CLIENT;

#ifdef CFG80211_MULTI_STA
	checkDevType = RT_CMD_80211_IFTYPE_STATION;
#endif /* CFG80211_MULTI_STA */

	DBGPRINT(RT_DEBUG_TRACE, ("80211> CFG80211_LostGoInform ==> \n"));

	pAd->cfg80211_ctrl.FlgCfg80211Connecting = FALSE;
	if ((pAd->cfg80211_ctrl.Cfg80211VifDevSet.vifDevList.size > 0) &&        
	    ((pNetDev = RTMP_CFG80211_FindVifEntry_ByType(pAd, checkDevType)) != NULL)
	   )
	{


	        if (pNetDev->ieee80211_ptr->sme_state == CFG80211_SME_CONNECTING)
       	 	{
                   cfg80211_connect_result(pNetDev, NULL, NULL, 0, NULL, 0,
                                                                   WLAN_STATUS_UNSPECIFIED_FAILURE, GFP_KERNEL);
        	}
        	else if (pNetDev->ieee80211_ptr->sme_state == CFG80211_SME_CONNECTED)
        	{
                   cfg80211_disconnected(pNetDev, 0, NULL, 0, GFP_KERNEL);
        	}
	}
	else
		DBGPRINT(RT_DEBUG_ERROR, ("80211> BUG CFG80211_LostGoInform, BUT NetDevice not exist.\n"));
		
	Set_ApCli_Enable_Proc(pAd, "0");	
#ifdef RT_CFG80211_P2P_CONCURRENT_DEVICE
	pAd->Mlme.bStartScc = FALSE;
#endif /*RT_CFG80211_P2P_CONCURRENT_DEVICE */

}
#endif /* RT_CFG80211_P2P_CONCURRENT_DEVICE */
#endif /* RT_CFG80211_SUPPORT */
