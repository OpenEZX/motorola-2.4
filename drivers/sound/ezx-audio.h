/*
 *  linux/drivers/sound/ezx-audio.h
 *
 *
 *  Description:  audio interface for ezx
 *
 *
 *  Copyright:	BJDC motorola.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *
 *  History:
 *  zhou qiong         June,2002               Created
 *  LiYong             Sep 23,2003             Port from EZX
 *  Jin Lihong(w20076) Jan 02,2004,Libdd66088  (1) Port from UDC e680 kernel of jem vob.
 *                                             (2) Move all audio driver DEBUG macro definition to ezx-audio.h
 *                                                 header file,and redefine DEBUG to EZX_OSS_DEBUG/EZX_OSS_AUDIO_DEBUG
 *                                             (3) reorganize file header
 *  Jin Lihong(w20076) Jan 13,2004,LIBdd68327  Make the e680 louder speaker work.
 *  Jin Lihong(w20076) Feb.23,2004,LIBdd79747  add e680 audio path switch and gain setting funcs
 *  Jin Lihong(w20076) Mar.15,2004,LIBdd86574  mixer bug fix
 *  Jin Lihong(w20076) Apr.13,2004,LIBdd96876  close dsp protection,and add 3d control interface for app
 *
 */

#ifndef EZX_AUDIO_H
#define EZX_AUDIO_H


#define NO_HEADSET	0
#define MONO_HEADSET	1
#define STEREO_HEADSET	2

#define OUT_ADJUST_GAIN		2
#define IN_ADJUST_GAIN		12

#define ASSP_CLK                    (1<<20)
#define ASSP_PINS                   (7<<17)


#define PHONE_CODEC_DEFAULT_RATE    8000
#define PHONE_CODEC_16K_RATE        16000


#define STEREO_CODEC_8K_RATE        8000
#define STEREO_CODEC_11K_RATE       11025
#define STEREO_CODEC_12K_RATE       12000
#define STEREO_CODEC_16K_RATE       16000
#define STEREO_CODEC_22K_RATE       22050
#define STEREO_CODEC_24K_RATE       24000
#define STEREO_CODEC_32K_RATE       32000
#define STEREO_CODEC_44K_RATE       44100
#define STEREO_CODEC_48K_RATE       48000


typedef struct {
	int offset;			/* current buffer position */
	char *data; 	           	/* actual buffer */
	pxa_dma_desc *dma_desc;		/* pointer to the starting desc */
	int master;             	/* owner for buffer allocation, contain size whn true */
}audio_buf_t;

typedef struct {
	char *name;			/* stream identifier */
	audio_buf_t *buffers;   	/* pointer to audio buffer array */
	u_int usr_frag;			/* user fragment index */
	u_int dma_frag;			/* DMA fragment index */
	u_int fragsize;			/* fragment size */
	u_int nbfrags;			/* number of fragments */
	u_int dma_ch;			/* DMA channel number */
	dma_addr_t dma_desc_phys;	/* phys addr of descriptor ring */
	u_int descs_per_frag;		/* nbr descriptors per fragment */
	int bytecount;			/* nbr of processed bytes */
	int getptrCount;		/* value of bytecount last time anyone asked via GETxPTR*/
	int fragcount;			/* nbr of fragment transitions */
	struct semaphore sem;		/* account for fragment usage */
	wait_queue_head_t frag_wq;	/* for poll(), etc. */
	wait_queue_head_t stop_wq;	/* for users of DCSR_STOPIRQEN */
	u_long dcmd;			/* DMA descriptor dcmd field */
	volatile u32 *drcmr;		/* the DMA request channel to use */
	u_long dev_addr;		/* device physical address for DMA */
	int mapped:1;			/* mmap()'ed buffers */
	int output:1;			/* 0 for input, 1 for output */
}audio_stream_t;
	
typedef struct {
	audio_stream_t *output_stream;
	audio_stream_t *input_stream;
	int dev_dsp;			/* audio device handle */
	int rd_ref:1;           	/* open reference for recording */
	int wr_ref:1;         		/* open reference for playback */
	int (*hw_init)(void);
	void (*hw_shutdown)(void);
	int (*client_ioctl)(struct inode *, struct file *, uint, ulong);
	struct pm_dev *pm_dev;
	struct semaphore sem;		/* prevent races in attach/release */
}audio_state_t;


extern int cotulla_audio_attach(struct inode *inode, struct file *file,	audio_state_t *state);

/* for headset and mic interrupt routine*/
extern void headjack_change_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs);
extern void mic_change_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs);
extern int mixer_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

extern void set_pcap_telephone_codec(int port);
extern void set_pcap_output_path(void);
extern void set_pcap_input_path(void);


extern void bluetoothaudio_open(void);
extern void bluetoothaudio_close(void);


#endif

