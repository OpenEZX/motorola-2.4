/*
 * linux/drivers/sound/ezx-audio.h, audio interface for ezx
 *
 * Copyright (C) 2002 Motorola Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define STEREO_OUTPUT	0	/* for stereo headset */
#define A2_OUTPUT	1		/* for loudspeaker  */
#define A1_OUTPUT	2		/* for speakphone  */
#define A4_OUTPUT	3		/* for carkit  */
#define HEADJACK_OUTPUT 4	/* for mono headjack */
	
#define EXTMIC_INPUT	0	/* for carkit  */
#define A5_INPUT	1		/* for mic  */
#define A3_INPUT	2		/* for headset mic  */

#define PHONE_DEVICE	0x1
#define DSP_DEVICE	0x2
#define DSP16_DEVICE	0x4
#define AUDIO_DEVICE	0x8	

#define NO_HEADSET	0
#define MONO_HEADSET	1
#define STEREO_HEADSET	2

#define OUT_ADJUST_GAIN		2
#define IN_ADJUST_GAIN		12

typedef struct {
	int offset;			/* current buffer position */
	char *data; 	           	/* actual buffer */
	pxa_dma_desc *dma_desc;	/* pointer to the starting desc */
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
	int rd_ref:1;           /* open reference for recording */
	int wr_ref:1;           /* open reference for playback */
	int (*hw_init)(void);
	void (*hw_shutdown)(void);
	int (*client_ioctl)(struct inode *, struct file *, uint, ulong);
	struct pm_dev *pm_dev;
	struct semaphore sem;		/* prevent races in attach/release */
}audio_state_t;

/* codec_output_path=0, AR,AL enable(stereo output); codec_output_path=1, A2 enalbe(loudspeaker);
 codec_output_path=2, A1 enable(earpiece); codec_output_path=3, A4 enable(carkit audio out);
 codec_output_path=4, AR enable(mono output) */
/* codec_output_gain=0x8, 0x7 is 0db (-21db - 21db) */
extern int codec_output_gain, codec_output_path;
/* codec_input_path=0, external mic; codec_input_path =1, A5 enable(headset mic); 
codec_input_path=2, A3 enalbe(carkit audio in)  */
/* codec_input_gain=0x0 is 0db (0db-31db) */
extern int codec_input_gain, codec_input_path;
extern int output_gain_matrix[][8];

extern int audioonflag; 
extern int micinflag;

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
extern void headset_change_interrupt_routine(int ch, void *dev_id, struct pt_regs *regs);
