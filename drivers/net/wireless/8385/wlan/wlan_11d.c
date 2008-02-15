/*
 * 	File: wlan_11d.c
 *
 * (c) Copyright © 2003-2006, Marvell International Ltd. 
 *
 * This software file (the "File") is distributed by Marvell International 
 * Ltd. under the terms of the GNU General Public License Version 2, June 1991 
 * (the "License").  You may use, redistribute and/or modify this File in 
 * accordance with the terms and conditions of the License, a copy of which 
 * is available along with the File in the license.txt file or by writing to 
 * the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 
 * 02111-1307 or on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 *
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE 
 * ARE EXPRESSLY DISCLAIMED.  The License provides additional details about 
 * this warranty disclaimer.
 *
 */

#include	"include.h"

#define TX_PWR_DEFAULT	10

/* Following 2 structure defines the supported channels */
CHANNEL_FREQ_POWER	channel_freq_power_UN_BG[] = {
	{1, 2412, TX_PWR_DEFAULT}, 
	{2, 2417, TX_PWR_DEFAULT}, 
	{3, 2422, TX_PWR_DEFAULT}, 
	{4, 2427, TX_PWR_DEFAULT}, 
	{5, 2432, TX_PWR_DEFAULT}, 
	{6, 2437, TX_PWR_DEFAULT}, 
	{7, 2442, TX_PWR_DEFAULT}, 
	{8, 2447, TX_PWR_DEFAULT},
	{9, 2452, TX_PWR_DEFAULT}, 
	{10, 2457, TX_PWR_DEFAULT},
	{11, 2462, TX_PWR_DEFAULT}, 
	{12, 2467, TX_PWR_DEFAULT},
	{13, 2472, TX_PWR_DEFAULT}, 
	{14, 2484, TX_PWR_DEFAULT} 
};

CHANNEL_FREQ_POWER	channel_freq_power_UN_AJ[] = {
	{8,   5040, TX_PWR_DEFAULT},
	{12,  5060, TX_PWR_DEFAULT}, 
	{16,  5080, TX_PWR_DEFAULT},
	{34,  5170, TX_PWR_DEFAULT}, 
	{38,  5190, TX_PWR_DEFAULT}, 
	{42,  5210, TX_PWR_DEFAULT}, 
	{46,  5230, TX_PWR_DEFAULT}, 
	{36,  5180, TX_PWR_DEFAULT}, 
	{40,  5200, TX_PWR_DEFAULT}, 
	{44,  5220, TX_PWR_DEFAULT}, 
	{48,  5240, TX_PWR_DEFAULT}, 
	{52,  5260, TX_PWR_DEFAULT}, 
	{56,  5280, TX_PWR_DEFAULT}, 
	{60,  5300, TX_PWR_DEFAULT}, 
	{64,  5320, TX_PWR_DEFAULT}, 
	{100, 5500, TX_PWR_DEFAULT}, 
	{104, 5520, TX_PWR_DEFAULT}, 
	{108, 5540, TX_PWR_DEFAULT}, 
	{112, 5560, TX_PWR_DEFAULT}, 
	{116, 5580, TX_PWR_DEFAULT}, 
	{120, 5600, TX_PWR_DEFAULT}, 
	{124, 5620, TX_PWR_DEFAULT}, 
	{128, 5640, TX_PWR_DEFAULT}, 
	{132, 5660, TX_PWR_DEFAULT}, 
	{136, 5680, TX_PWR_DEFAULT}, 
	{140, 5700, TX_PWR_DEFAULT},
	{149, 5745, TX_PWR_DEFAULT}, 
	{153, 5765, TX_PWR_DEFAULT}, 
	{157, 5785, TX_PWR_DEFAULT},
	{161, 5805, TX_PWR_DEFAULT}, 
	{165, 5825, TX_PWR_DEFAULT},
/*	{240, 4920, TX_PWR_DEFAULT}, 
	{244, 4940, TX_PWR_DEFAULT}, 
	{248, 4960, TX_PWR_DEFAULT}, 
	{252, 4980, TX_PWR_DEFAULT}, 
channels for 11J JP 10M channel gap */
};

static region_code_mapping_t region_code_mapping[] =
{
	{ "US ", 0x10 }, /* US FCC	*/
	{ "CA ", 0x10 }, /* IC Canada	*/ 
	{ "SG ", 0x10 }, /* Singapore	*/
	{ "EU ", 0x30 }, /* ETSI	*/
	{ "AU ", 0x30 }, /* Australia	*/
	{ "KR ", 0x30 }, /* Republic Of Korea */
	{ "ES ", 0x31 }, /* Spain	*/
	{ "FR ", 0x32 }, /* France	*/
	{ "JP ", 0x40 }, /* Japan	*/
};

u8 wlan_region_2_code( char *region )
/* Convert Region string to code integer*/
{
	int i;
	int size = sizeof(region_code_mapping)/sizeof(region_code_mapping_t);  

	for ( i=0; region[i] && i< COUNTRY_CODE_LEN; i++ )
			region[i] = toupper( region[i] );

	for (i=0; i<size; i++){
		if ( !memcmp(region, region_code_mapping[i].region, 
							COUNTRY_CODE_LEN) )
			return (region_code_mapping[i].code);
	}

	/* default is US*/
	return (region_code_mapping[0].code);
}

char *wlan_code_2_region( u8 code )
/*Convert interger code to region string*/
{
	int i;
	int size =sizeof(region_code_mapping)/ sizeof(region_code_mapping_t); 
	for ( i=0; i< size; i++ ) {
		if ( region_code_mapping[i].code == code )
			return (region_code_mapping[i].region);
	}
	/* default is US*/
	return (region_code_mapping[0].region);
}


u32 chan_2_freq ( u16 chan, u8 band )
{
	CHANNEL_FREQ_POWER 	*cf;
	u8			cnt;
	int 			i;

	ENTER();

#ifdef MULTI_BANDS
	if ( band & BAND_A ) {
		cf =channel_freq_power_UN_AJ; 
		cnt = sizeof(channel_freq_power_UN_AJ)/sizeof(CHANNEL_FREQ_POWER);
	}
	else
#endif
	{
		cf = channel_freq_power_UN_BG;
		cnt = sizeof(channel_freq_power_UN_BG)/sizeof(CHANNEL_FREQ_POWER);
	}

	for ( i=0; i< cnt; i++ ) {
		if ( chan == cf[i].Channel ) {
			LEAVE();
			return cf[i].Freq;
		}
	}

	LEAVE();
	return 0;
}

int wlan_generate_domain_info_11d( 
		parsed_region_chan_11d_t * parsed_region_chan, 
		wlan_802_11d_domain_reg_t *domaininfo)
/*generate domaininfo from parsed_region_chan*/
{
	u8	NoOfSubband =0;
	
	u8	NoOfChan = parsed_region_chan->NoOfChan;
	u8	NoOfParsedChan =0;

	u8	firstChan=0, nextChan=0, maxPwr=0;

	int 	i, flag =0;

	
	ENTER();

	memcpy( domaininfo->CountryCode, parsed_region_chan->CountryCode, 
				COUNTRY_CODE_LEN);
	

	PRINTK2("11D:NoOfChan=%d\n",NoOfChan ); 
	HEXDUMP( "11D:parsed_region_chan:", (char *)parsed_region_chan, 
			sizeof(parsed_region_chan_11d_t) );

	for ( i=0; i<NoOfChan; i++ ) {
		if ( !flag ) {
			flag = 1;
			nextChan = firstChan = parsed_region_chan->chanPwr[i].chan;
			maxPwr =  parsed_region_chan->chanPwr[i].pwr;
			NoOfParsedChan = 1;
			continue;
		}

		if ( parsed_region_chan->chanPwr[i].chan == nextChan+1 && 
			parsed_region_chan->chanPwr[i].pwr == maxPwr ) {
			nextChan++;
			NoOfParsedChan++;
		}
		else {
			domaininfo->Subband[NoOfSubband].FirstChan = firstChan;
			domaininfo->Subband[NoOfSubband].NoOfChan = NoOfParsedChan;
			domaininfo->Subband[NoOfSubband].MaxTxPwr = maxPwr;
			NoOfSubband++;
			nextChan = firstChan = parsed_region_chan->chanPwr[i].chan;
			maxPwr =  parsed_region_chan->chanPwr[i].pwr;
		}
	}
	
	if( flag ) {
		domaininfo->Subband[NoOfSubband].FirstChan = firstChan;
		domaininfo->Subband[NoOfSubband].NoOfChan = NoOfParsedChan;
		domaininfo->Subband[NoOfSubband].MaxTxPwr = maxPwr;
		NoOfSubband++;
	}
	domaininfo->NoOfSubband = NoOfSubband;

	PRINTK2("NoOfSubband=%x\n", domaininfo->NoOfSubband);
	HEXDUMP( "11D:domaininfo:", (char *)domaininfo, 
		  	COUNTRY_CODE_LEN+ 1 + 
			sizeof(IEEEtypes_SubbandSet_t)*NoOfSubband );
	LEAVE();
	return 0;
}

void  wlan_generate_parsed_region_chan_11d(
		REGION_CHANNEL *region_chan,
		parsed_region_chan_11d_t *parsed_region_chan )
/*generate parsed_region_chan from Domain Info learned from AP/IBSS*/
{
	int i;
	CHANNEL_FREQ_POWER *cfp;
 
	ENTER();

	if( region_chan ==NULL ) {
		PRINTK2("11D: region_chan is NULL\n");
		return;
	}

	cfp = region_chan->CFP;
	if( cfp ==NULL ) {
		PRINTK2("11D: cfp equal NULL \n");
		return;
	}

	parsed_region_chan->band = region_chan->Band;
	parsed_region_chan->region = region_chan->Region;
	memcpy( parsed_region_chan->CountryCode, 
			wlan_code_2_region(region_chan->Region), 
			COUNTRY_CODE_LEN);

	PRINTK2("11D: region[0x%x] band[%d]\n", parsed_region_chan->region,
					   	parsed_region_chan->band );
	
	for( i=0; i< region_chan->NrCFP; i++, cfp++ ) {
		parsed_region_chan->chanPwr[i].chan = cfp->Channel; 
		parsed_region_chan->chanPwr[i].pwr = cfp->MaxTxPower;
		PRINTK("11D: Chan[%d] Pwr[%d]\n", parsed_region_chan->chanPwr[i].chan,
						  parsed_region_chan->chanPwr[i].pwr );
	}
	parsed_region_chan->NoOfChan = region_chan->NrCFP;	
	
	PRINTK2("11D: NoOfChan[%d]\n", parsed_region_chan->NoOfChan);

	LEAVE();
	return;
}

static u8 wlan_get_chan_11d( u8 band, int firstChan, int NoOfChan )
/*find the NoOfChan-th chan after the firstChan*/
{
	int i;
	CHANNEL_FREQ_POWER *cfp;
	int	cfp_no;

	ENTER();

/*get chan list table*/
#ifdef MULTI_BANDS
		if (band & (BAND_B | BAND_G)) 
#endif
		{
			cfp =channel_freq_power_UN_BG; 
			cfp_no = sizeof(channel_freq_power_UN_BG)/
				sizeof(CHANNEL_FREQ_POWER);
		}
#ifdef MULTI_BANDS
		else {
			if (band & BAND_A) {
				cfp =channel_freq_power_UN_AJ; 
				cfp_no = sizeof(channel_freq_power_UN_AJ)/
					sizeof(CHANNEL_FREQ_POWER);
			}
			else {
				PRINTK2("Wrong Band[%d\n", band );
				return 0;
			}
		}
#endif
		
/*If firstChan is in the chan list */
	for ( i=0; i<cfp_no; i++ ) {
		if( (cfp+i)->Channel == firstChan ) {
			PRINTK2("firstChan found\n");
			break;
		}
	}	

/*if beyond the boundary*/
	if ( i<cfp_no ) {
		if( i+NoOfChan < cfp_no )
			return (cfp+i+NoOfChan)->Channel;
		else
			return (cfp+cfp_no-1)->Channel;
	}

	LEAVE();
	return 0;
}

extern CHANNEL_FREQ_POWER *wlan_get_region_cfp_table( u8 region, 
			u8 band, int * cfp_no);

BOOLEAN wlan_region_chan_supported_11d( u8 region, u8 band, u8 chan )
/*check if chan is supported in the region*/
{
	CHANNEL_FREQ_POWER *cfp;
	int cfp_no;
	int idx;

	ENTER();

	if ( (cfp= wlan_get_region_cfp_table(region, band, &cfp_no)) == NULL ) {
		return FALSE;
	}

	for ( idx=0; idx < cfp_no; idx++ ) {
		if( chan == (cfp+idx)->Channel ) {
			/* If Mrvl Chip Supported? */
			if( (cfp+idx)->Unsupported ) {
				return FALSE;
			}
			else { 
				return TRUE;
			}
		}
	}

	/*chan is not in the region table*/
	LEAVE();
	return FALSE;
}

int wlan_parse_domain_info_11d(
		IEEEtypes_CountryInfoFullSet_t *CountryInfo, u8 band,
		parsed_region_chan_11d_t * parsed_region_chan ) 
/*generate parsed_region_chan from CountryInfo*/
/*Validation Rules:
  1. Valid Region Code
  2. First Chan increment
  3. Channel range no overlap
  4. Channel is valid?
  5. Channel is supported by Region?
  6. Others
 */
{
	int 	NoOfSubband, NoOfChan;
	int 	lastChan, firstChan, curChan;
	u8	region;

	u8 	idx =0;		/*chan index in parsed_region_chan*/
	
	int 	j, i;

	ENTER();

	HEXDUMP("CountryInfo:", (char *)CountryInfo, 30 );	

	if( (*(CountryInfo->CountryCode))==0 ||  (CountryInfo->Len <= COUNTRY_CODE_LEN ) ){
	/* No region Info or Wrong region info: treat as No 11D info*/
		LEAVE();
		return WLAN_STATUS_SUCCESS;
	}

	/*Step1: check region_code*/
	parsed_region_chan->region = region =
		wlan_region_2_code(CountryInfo->CountryCode);

	PRINTK2("regioncode=%x\n",(int)parsed_region_chan->region);
	HEXDUMP("CountryCode:", (char *)CountryInfo->CountryCode, COUNTRY_CODE_LEN);	

	parsed_region_chan->band = band;

	memcpy( parsed_region_chan->CountryCode, CountryInfo->CountryCode, 
			COUNTRY_CODE_LEN);

	NoOfSubband = (CountryInfo->Len - COUNTRY_CODE_LEN)/ 
					sizeof(IEEEtypes_SubbandSet_t);

	
	for ( j=0, lastChan=0; j<NoOfSubband; j++ ) {

		/*Step2&3. Check First Chan Num increment and no overlap*/
		if ( CountryInfo->Subband[j].FirstChan <= lastChan ) {
			PRINTK("11D: Chan[%d>%d] Overlap\n", 
				CountryInfo->Subband[j].FirstChan, lastChan );
			continue;
/*Skip the overlaped triplets
			LEAVE();
			return WLAN_STATUS_FAILURE;
*/
		}
		
		firstChan = CountryInfo->Subband[j].FirstChan;
		NoOfChan = CountryInfo->Subband[j].NoOfChan;

		/*Copy to parsed_region_chan*/
		for ( i = 0; idx<MAX_NO_OF_CHAN && i< NoOfChan; i++ ) {
		/*step4: channel is supported?*/

			curChan = wlan_get_chan_11d( band, firstChan, i);
			PRINTK2("firstChan[%d], i[%d] band[%d] curChan[%d]\n", 
					firstChan, i, band, curChan);

			/* Chan is not found in UN table*/
			if( curChan == 0 || curChan == lastChan ) {
				break;
			}

			lastChan = curChan;

			/*step5: Check if curChan is supported by mrvl in region*/
			if ( wlan_region_chan_supported_11d( region, band, curChan) ) { 
				parsed_region_chan->chanPwr[idx].chan = curChan;
				parsed_region_chan->chanPwr[idx].pwr = 
						CountryInfo->Subband[j].MaxTxPwr;
				idx++;
			}
			else {
				/*not supported and ignore the chan*/
				PRINTK("11D:i[%d] chan[%d] unsupported in region[%x] band[%d]\n", 
					i, curChan, region, band );
			}
		}

		/*Step6: Add other checking if any*/

	}

	parsed_region_chan->NoOfChan = idx;

	PRINTK2("NoOfChan=%x\n", parsed_region_chan->NoOfChan );
	HEXDUMP( "11D:parsed_region_chan:", (char *)parsed_region_chan, 
			2 + COUNTRY_CODE_LEN +
			sizeof(parsed_region_chan_11d_t)*idx );

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}


static int wlan_channel_known_11d ( u8 chan,  
				parsed_region_chan_11d_t *parsed_region_chan)
/*Check if chan txpwr is learned from AP/IBSS*/
{
	chan_power_11d_t 	*chanPwr = parsed_region_chan->chanPwr;
	u8			NoOfChan = parsed_region_chan->NoOfChan;
	int 			i =0;

	ENTER();
	HEXDUMP( "11D:parsed_region_chan:", (char *)chanPwr, 
			sizeof(chan_power_11d_t)*NoOfChan );

	for( i=0; i<NoOfChan; i++ ) {
		if ( chan == chanPwr[i].chan ) {
			PRINTK("11D: Found Chan:%d\n", chan );
			LEAVE();
			return 1;
		}
	}

	PRINTK("11D: Not Find Chan:%d\n", chan );
	LEAVE();
	return 0;
}

u8 wlan_get_scan_type_11d(  u8 chan,  
			parsed_region_chan_11d_t *parsed_region_chan)
/* calsulate the scan typefor channels
   1. Active scan for all chan in parsed_region_chan(learned form AP/IBSS)
   2. Passive scan for all other channels
*/
{
        u8  	scan_type = HostCmd_SCAN_TYPE_PASSIVE;

	ENTER();

	if ( wlan_channel_known_11d( chan, parsed_region_chan) ) {
		PRINTK("11D: Found and do Active Scan\n");
		scan_type = HostCmd_SCAN_TYPE_ACTIVE;
	}
	else {
		PRINTK("11D: Not Find and do Passive Scan\n");
	}

	LEAVE();
	return scan_type;

}

state_11d_t  wlan_get_state_11d( wlan_private *priv )
/* Get if 11D is enabled?*/
{
	wlan_adapter	*Adapter = priv->adapter;
	wlan_802_11d_state_t	*state = &Adapter->State11D;
	return (state->Enable11D);
}

void wlan_init_11d( wlan_private *priv)
{
	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11d_state_t		*state = &Adapter->State11D;

	state->Enable11D = DISABLE_11D;
		
	memset( &(priv->adapter->parsed_region_chan), 0,  
			sizeof(parsed_region_chan_11d_t) );

	return;
}
	
int wlan_enable_11d( wlan_private * priv, state_11d_t flag )
/* Enable/Disable 11D via set SNMP OID to FW*/
{
	wlan_adapter			*Adapter = priv->adapter;
	wlan_802_11d_state_t		*state = &Adapter->State11D;
	int ret;
	state_11d_t enable = flag;
	
	ENTER();

	state->Enable11D = flag;

	/* send cmd to FW to enable/disable 11D fucntion in FW */
	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11_SNMP_MIB,
		0, HostCmd_OPTION_USE_INT, OID_802_11D_ENABLE, 
		HostCmd_PENDING_ON_SET_OID, &enable );

	if (ret) {
		PRINTK2("11D: Fail to enable 11D \n");  
	}

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}

int wlan_set_domain_info_11d( wlan_private * priv )
/* Set DOMAIN INFO to FW*/
{
	int ret; 

	if( !wlan_get_state_11d( priv) ) {
		PRINTK2("11D: dnld domain Info with 11d disabled\n");  
		return WLAN_STATUS_SUCCESS;
	}

 	ret = PrepareAndSendCommand(priv, HostCmd_CMD_802_11D_DOMAIN_INFO,
			HostCmd_ACT_GEN_SET, HostCmd_OPTION_USE_INT,
			0, HostCmd_PENDING_ON_NONE, NULL);
	if (ret) {
		PRINTK2("11D: Fail to dnld domain Info\n");  
	}

	LEAVE();
	return ret;
}

int wlan_set_universaltable(wlan_private *priv, u8 band)
/* Setup scan channels when disconnected */
{
	wlan_adapter	*Adapter = priv->adapter;
	int		size = sizeof(CHANNEL_FREQ_POWER);
	int		i = 0;

	ENTER();
	
	memset(Adapter->universal_channel, 0, sizeof(Adapter->universal_channel));

#ifdef MULTI_BANDS
	if (band & (BAND_B | BAND_G)) 
#endif
	{
		Adapter->universal_channel[i].NrCFP =
			sizeof(channel_freq_power_UN_BG) / size;
		PRINTK2("11D: BG-band NrCFP=%d\n",
				Adapter->universal_channel[i].NrCFP );

		Adapter->universal_channel[i].CFP = 
						channel_freq_power_UN_BG;
		Adapter->universal_channel[i].Valid	= TRUE;
		Adapter->universal_channel[i].Region	= UNIVERSAL_REGION_CODE;
#ifdef MULTI_BANDS
		Adapter->universal_channel[i].Band	= 
			(band & BAND_G) ? BAND_G : BAND_B;
#else
		Adapter->universal_channel[i].Band		= band;
#endif
		i++;
	}

#ifdef MULTI_BANDS
	if (band & BAND_A) {
		Adapter->universal_channel[i].NrCFP =
			sizeof(channel_freq_power_UN_AJ) / size;
		PRINTK2("11D: AJ-band NrCFP=%d\n",
				Adapter->universal_channel[i].NrCFP );
		Adapter->universal_channel[i].CFP = 
						channel_freq_power_UN_AJ;

		Adapter->universal_channel[i].Valid	= TRUE;
		Adapter->universal_channel[i].Region	= UNIVERSAL_REGION_CODE;
		Adapter->universal_channel[i].Band	= BAND_A;
		i++;
	}
#endif
		
	LEAVE();
	return 0;
}


int wlan_cmd_802_11d_domain_info(wlan_private * priv,
			   HostCmd_DS_COMMAND * cmd, u16 cmdno, u16 CmdOption)
/*Domain Info command: CMD_802_11D_DOMAIN_INFO*/
{
	HostCmd_DS_802_11D_DOMAIN_INFO *pDomainInfo = &cmd->params.domaininfo;
	MrvlIEtypes_DomainParamSet_t	*domain = &pDomainInfo->Domain;
	wlan_adapter   *Adapter = priv->adapter;
	unsigned int NoOfSubband = Adapter->DomainReg.NoOfSubband;

	ENTER();

	PRINTK2("NoOfSubband=%x\n", NoOfSubband );

	cmd->Command = wlan_cpu_to_le16(cmdno);
	pDomainInfo->Action = wlan_cpu_to_le16(CmdOption);
	if ( CmdOption == HostCmd_ACT_GET ) { 
		cmd->Size = wlan_cpu_to_le16(sizeof( pDomainInfo->Action ) + S_DS_GEN);
		HEXDUMP("11D: 802_11D_DOMAIN_INFO:", (u8 *)cmd, (int)(cmd->Size) );
		LEAVE();
		return 0;
	}
 
	domain->Header.Type = wlan_cpu_to_le16(TLV_TYPE_DOMAIN);  /*0007*/
	memcpy( domain->CountryCode, Adapter->DomainReg.CountryCode, 
					sizeof(domain->CountryCode) );

	domain->Header.Len = 
		wlan_cpu_to_le16(NoOfSubband * sizeof(IEEEtypes_SubbandSet_t) +
		sizeof(domain->CountryCode)); 

	if ( NoOfSubband ) {
		memcpy( domain->Subband, Adapter->DomainReg.Subband, 
			NoOfSubband * sizeof(IEEEtypes_SubbandSet_t) );

		cmd->Size = wlan_cpu_to_le16(sizeof(pDomainInfo->Action)+ 
			domain->Header.Len + 
			sizeof(MrvlIEtypesHeader_t) + S_DS_GEN);
	}
	else {
		cmd->Size = wlan_cpu_to_le16(sizeof(pDomainInfo->Action) + S_DS_GEN);
	}

	HEXDUMP("11D:802_11D_DOMAIN_INFO:", (u8 *)cmd, (int)(cmd->Size) );

 	LEAVE();

	return 0;
}


int wlan_cmd_enable_11d( wlan_private *priv,  struct iwreq *wrq )
/*private cmd: enable/disable 11D*/
{
	int data = *((int *)wrq->u.data.pointer);
	wlan_adapter   *Adapter = priv->adapter;
	
	ENTER();

	if ( wrq->u.data.length == 0 ) {
		return -EINVAL;
	}

	PRINTK2("Enable 11D: %s\n", (data==1)?"Enable":"Disable");
	switch ( data ) {
		case CMD_ENABLED:
			wlan_enable_11d(priv, ENABLE_11D );
			break;
		case CMD_DISABLED:
			wlan_enable_11d( priv, DISABLE_11D );
			break;
		default:			
			break;
	}

	data = (Adapter->State11D.Enable11D==ENABLE_11D)?CMD_ENABLED:CMD_DISABLED;
	copy_to_user(wrq->u.data.pointer, &data, sizeof(int) );
	wrq->u.data.length = 1;

	LEAVE();
	return WLAN_STATUS_SUCCESS;
}


int wlan_ret_802_11d_domain_info(wlan_private * priv, 
				HostCmd_DS_COMMAND * resp)
/*Domain Info response process*/
{
	HostCmd_DS_802_11D_DOMAIN_INFO_RSP
			*domaininfo = &resp->params.domaininforesp;
	MrvlIEtypes_DomainParamSet_t	
			*domain = &domaininfo->Domain;
	u16		Action	= wlan_le16_to_cpu(domaininfo->Action);
	int		ret=0;
	int 		NoOfSubband = 0;

	ENTER();

	HEXDUMP( "11D DOMAIN Info Rsp Data:", (u8 *)resp, 
					(int)wlan_le16_to_cpu(resp->Size) ); 	

	NoOfSubband = ( domain->Header.Len - 3 ) / sizeof(IEEEtypes_SubbandSet_t);
	/* countrycode 3 bytes */	

	PRINTK2("11D Domain Info Resp: NoOfSubband=%d\n", NoOfSubband);

	if (NoOfSubband > MRVDRV_MAX_SUBBAND_802_11D ) {
		PRINTK2("Invalid Numrer of Subband returned!!\n");
		return -1;
	}

	switch (Action) {
	case HostCmd_ACT_SET: /*Proc Set Action */
		break;
	
	case HostCmd_ACT_GET: 
	        break;
	default:
		PRINTK2("Invalid Action:%d\n", domaininfo->Action );
		ret = -1;
		break;
	}

	LEAVE();
	return ret;	
}


int wlan_parse_dnld_countryinfo_11d( wlan_private *priv )
/*parse countryinfo from AP and download country info to FW*/
{
	int ret;
	wlan_adapter * Adapter = priv->adapter;

	ENTER();
	if( wlan_get_state_11d( priv) == ENABLE_11D ) {

		memset( &Adapter->parsed_region_chan, 0, sizeof(parsed_region_chan_11d_t) );

		ret = wlan_parse_domain_info_11d( 
   			&(Adapter->BSSIDList[Adapter->ulAttemptedBSSIDIndex].CountryInfo),
#ifdef MULTI_BANDS
			Adapter->CurBssParams.band,
#else	
			0,
#endif
			&Adapter->parsed_region_chan 
			);
		
		if( ret == WLAN_STATUS_FAILURE ) {
			PRINTK2("11D: Err Parse domain_info from AP..\n");
			LEAVE();
			return ret;
		}


		memset( &Adapter->DomainReg,0, sizeof(wlan_802_11d_domain_reg_t) ); 
		wlan_generate_domain_info_11d( 
			&Adapter->parsed_region_chan,
			&Adapter->DomainReg
			);

		ret = wlan_set_domain_info_11d( priv );

		if ( ret ) {
			PRINTK2("11D: Err set domainInfo to FW\n");
			LEAVE();
			return ret;
		}
	}
	LEAVE();
	return WLAN_STATUS_SUCCESS;
}


int wlan_create_dnld_countryinfo_11d( wlan_private *priv )
/*generate from user specified regioncode and download to FW*/
{
	int ret;
	wlan_adapter * Adapter = priv->adapter;
	REGION_CHANNEL *region_chan;
	int j;

	ENTER();
	PRINTK2("11D:CurBssParams.Band[%d]\n", Adapter->CurBssParams.band);
	
	if( wlan_get_state_11d(priv) == ENABLE_11D ) {
	/* update parsed_region_chan_11; dnld domaininf to FW */

		for (j = 0; j < sizeof(Adapter->region_channel) / 
				sizeof(Adapter->region_channel[0]); j++ ) {
			region_chan = &Adapter->region_channel[j];
			
			PRINTK2("11D:[%d] region_chan->Band[%d]\n",j, region_chan->Band);

			if(!region_chan || !region_chan->Valid || !region_chan->CFP)
				continue;
#ifdef MULTI_BANDS
			switch (region_chan->Band) {
				case BAND_A:
					switch (Adapter->CurBssParams.band) {
						case BAND_A:
							break;
						default:
							continue;
					}
					break;
				case BAND_B:
				case BAND_G:
					switch (Adapter->CurBssParams.band) {
						case BAND_B:
						case BAND_G:
							break;
						default:
							continue;
					}
					break;
				default:
					continue;
			}
#else
			if (region_chan->Band != Adapter->CurBssParams.band )
				continue;
#endif
			break;
		}
	
		if (j >= sizeof(Adapter->region_channel)/
				sizeof(Adapter->region_channel[0]) ) {
			PRINTK2("11D:region_chan not found. Band[%d]\n",
				Adapter->CurBssParams.band);
			LEAVE();
			return WLAN_STATUS_FAILURE;
		}

		memset( &Adapter->parsed_region_chan, 0, sizeof(parsed_region_chan_11d_t) );
		wlan_generate_parsed_region_chan_11d( 
					region_chan,
					&Adapter->parsed_region_chan
					);

		memset( &Adapter->DomainReg,0, sizeof(wlan_802_11d_domain_reg_t) ); 
		wlan_generate_domain_info_11d( 
					&Adapter->parsed_region_chan,
					&Adapter->DomainReg
					);

		ret = wlan_set_domain_info_11d( priv );	

		if ( ret ) {
			PRINTK2("11D: Err set domainInfo to FW\n");
			LEAVE();
			return ret;
		}

	}
	
	LEAVE();
	return WLAN_STATUS_SUCCESS;
}



