#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/irq.h>

//#include <linux/mutex.h>
//#include <linux/semaphore.h>

#include <asm/irq.h>
#include <asm/system.h>
#include <mach/hardware.h>
#include <asm/leds.h>
#include <asm/mach/irq.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <linux/timed_output.h>

#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <asm/delay.h>
//#include <mach/board.h>
//#include <mach/dmtimer.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <asm/mach/time.h>
#include <mach/io.h>
#include <asm/mach-types.h>
//#include <mach/halo.h> 
#include "../../arch/arm/mach-omap2/mux.h"

#include "Si4709_regs.h"
#include "Si4709_main.h"
#include "Si4709_dev.h"
#include "common.h"

#include <linux/i2c/twl.h>

#if defined(CONFIG_FMRADIO_USE_GPIO_I2C)
#include <plat/i2c-omap-gpio.h>
#endif

enum
{
    eTRUE,
    eFALSE,
}dev_struct_status_t;

/*dev_state*/
/*power_state*/
#define RADIO_ON            1
#define RADIO_POWERDOWN     0
/*seek_state*/
#define RADIO_SEEK_ON       1
#define RADIO_SEEK_OFF      0

#define FREQ_87500_kHz      8750
#define FREQ_76000_kHz      7600

#define RSSI_seek_th_MAX    0x7F
#define RSSI_seek_th_MIN    0x00

#define seek_SNR_th_DISB    0x00
#define seek_SNR_th_MIN     0x01  /*most stops*/
#define seek_SNR_th_MAX     0x0F  /*fewest stops*/

#define seek_FM_ID_th_DISB  0x00
#define seek_FM_ID_th_MAX   0x01  /*most stops*/
#define seek_FM_ID_th_MIN   0x0F  /*fewest stops*/

#define TUNE_RSSI_THRESHOLD		0x02
#define TUNE_SNR_THRESHOLD		0x00
#define TUNE_CNT_THRESHOLD		0x00

#define ADDRESS_GPIO2_VOLDN		0x48002088
#define ADDRESS_GPIO2_VOLUP		0x4800208C

static u8 FM_setVolume;		// Jinokang For FM Volume to 30

typedef struct 
{
    u32 frequency;
    u8 rsssi_val;
}channel_into_t;

typedef struct
{
    u16 band;
    u32 bottom_of_band;
    u16 channel_spacing;
    u32 timeout_RDS;      /****For storing the jiffy value****/
    u32 seek_preset[NUM_SEEK_PRESETS];
    u8 curr_snr;
    u8 curr_rssi_th;
}dev_settings_t;

typedef struct 
{
    /*Any function which 
      - views/modifies the fields of this structure
      - does i2c communication 
        should lock the mutex before doing so.
        Recursive locking should not be done.
        In this file all the exported functions will take care
        of this. The static functions will not deal with the
        mutex*/
    struct mutex lock;
	
    struct i2c_client const *client;

    dev_state_t state;

    dev_settings_t settings;

    channel_into_t rssi_freq[50];

    u16 registers[NUM_OF_REGISTERS];

    /* This field will be checked by all the functions
       exported by this file (except the init function), 
       to validate the the fields of this structure. 
       if eTRUE: the fileds are valid
       if eFALSE: do not trust the values of the fields 
       of this structure*/
    unsigned short valid;

    /*will be true is the client ans state fields are correct*/
    unsigned short valid_client_state;
}Si4709_device_t;


/*extern functions*/
/**********************************************/
/*All the exported functions which view or modify the device
  state/data, do i2c com will have to lock the mutex before 
  doing so*/
/**********************************************/
int Si4709_dev_init(struct i2c_client *);
int Si4709_dev_exit(void);

void Si4709_dev_mutex_init(void); 

int Si4709_dev_suspend(void);
int Si4709_dev_resume(void);

int Si4709_dev_powerup(void);
int Si4709_dev_powerdown(void);

int Si4709_dev_band_set(int);
int Si4709_dev_ch_spacing_set(int);

int Si4709_dev_chan_select(u32);
int Si4709_dev_chan_get(u32*);

int Si4709_dev_seek_up(u32*);
int Si4709_dev_seek_down(u32*);
int Si4709_dev_seek_auto(u32*);

int Si4709_dev_RSSI_seek_th_set(u8);
int Si4709_dev_seek_SNR_th_set(u8);
int Si4709_dev_seek_FM_ID_th_set(u8);
int Si4709_dev_cur_RSSI_get(rssi_snr_t*);
int Si4709_dev_reset_rds_data(void);	// JinoKang 2011.06.03
int Si4709_dev_VOLEXT_ENB(void);
int Si4709_dev_VOLEXT_DISB(void);
int Si4709_dev_volume_set(u8);
int Si4709_dev_volume_get(u8 *);
int Si4709_dev_DSMUTE_ON(void);
int Si4709_dev_DSMUTE_OFF(void);
int Si4709_dev_MUTE_ON(void);
int Si4709_dev_MUTE_OFF(void);
int Si4709_dev_MONO_SET(void);
int Si4709_dev_STEREO_SET(void);
int Si4709_dev_RDS_ENABLE(void);
int Si4709_dev_RDS_DISABLE(void);
int Si4709_dev_RDS_timeout_set(u32);
int Si4709_dev_mute_get(int *isMute);	// JinoKang 2011.06.03
int Si4709_dev_rstate_get(dev_state_t*);
int Si4709_dev_RDS_data_get(radio_data_t*);
int Si4709_dev_device_id(device_id *);          	
int Si4709_dev_chip_id(chip_id *);					
int Si4709_dev_sys_config2(sys_config2 *);  		
int Si4709_dev_power_config(power_config *);		
int Si4709_dev_AFCRL_get(u8*);
int Si4709_dev_DE_set(u8);
#ifdef FMRADIO_TSP_DELAY_CONTROL
int Si4709_dev_tsp_speed_set(unsigned int);
//extern void zinitix_change_scan_delay(int);
#endif

#ifdef FMRADIO_WAIT_TIMEOUT
#define FMRADIO_WAIT_TIME_TUNE	1500
#define FMRADIO_WAIT_TIME_SEEK	8000
extern irqreturn_t Si4709_isr( int irq, void *unused );
#endif
/***********************************************/

#ifdef RDS_INTERRUPT_ON_ALWAYS
#define RDS_BUFFER_LENGTH 50
static u16 *RDS_Block_Data_buffer;
static u8 *RDS_Block_Error_buffer;
static u8 RDS_Buffer_Index_read;  //index number for last read data
static u8 RDS_Buffer_Index_write; //index number for last written data

int Si4709_RDS_flag;
int RDS_Data_Available;
int RDS_Data_Lost;
int RDS_Groups_Available_till_now;
struct workqueue_struct *Si4709_wq;
struct work_struct Si4709_work;
#endif

#if defined(CONFIG_FMRADIO_USE_GPIO_I2C)
static OMAP_GPIO_I2C_CLIENT * Si4709_i2c_client;
#endif

/*static functions*/
/**********************************************/
static void wait(void);

#ifndef RDS_INTERRUPT_ON_ALWAYS	// JinoKang 2011.06.03
static void wait_RDS(void );
#endif

static int powerup(void);
static int powerdown(void);

static int seek(u32*, int);
static int tune_freq(u32);

static void get_cur_chan_freq(u32 *, u16);

static u16 freq_to_channel(u32);
static u32 channel_to_freq(u16);

static int insert_preset(u32,u8,u8*);

static int i2c_read(u8);
static int i2c_write(u8);
/**********************************************/

/*Si4709 device structure*/
static Si4709_device_t Si4709_dev =
{
    .client = NULL,
    .valid = eFALSE,
    .valid_client_state = eFALSE,
};

/*Wait flag*/
/*WAITING or WAIT_OVER or NO_WAIT*/
int Si4709_dev_wait_flag = NO_WAIT;
#ifdef RDS_INTERRUPT_ON_ALWAYS
int Si4709_RDS_flag = NO_WAIT;
#endif

int Si4709_dev_init(struct i2c_client *client)
{
	int ret = 0;

	debug("Si4709_dev_init called");

	mutex_lock(&(Si4709_dev.lock));
	
    Si4709_dev.client = client;

#if defined(CONFIG_FMRADIO_USE_GPIO_I2C)
	Si4709_i2c_client = omap_gpio_i2c_init(OMAP_GPIO_FM_SDA,
						  OMAP_GPIO_FM_SCL,
						  0x10,
						  200);
#endif

	/***reset the device here****/
	
	gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_LOW);	
 		
    mdelay(1);
	gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_HIGH);	

    mdelay(2);	
										
	Si4709_dev.state.power_state = RADIO_POWERDOWN;
	Si4709_dev.state.seek_state = RADIO_SEEK_OFF;

    if( (ret = i2c_read(BOOTCONFIG) ) < 0 )
    {
        debug("i2c_read failed");
    }
    else
    {
	Si4709_dev.valid_client_state = eTRUE;
    }

#if defined(CONFIG_FMRADIO_USE_GPIO_I2C)
	omap_gpio_i2c_deinit(Si4709_i2c_client);
#endif
	
#ifdef RDS_INTERRUPT_ON_ALWAYS 
	/*Creating Circular Buffer*/
	/*Single RDS_Block_Data buffer size is 4x16 bits*/
	RDS_Block_Data_buffer = kzalloc(RDS_BUFFER_LENGTH*8,GFP_KERNEL);  
	if(!RDS_Block_Data_buffer)
	{
		error("Not sufficient memory for creating RDS_Block_Data_buffer");
		ret = -ENOMEM;
		goto EXIT;
	}
	
	/*Single RDS_Block_Error buffer size is 4x8 bits*/	
	RDS_Block_Error_buffer = kzalloc(RDS_BUFFER_LENGTH*4,GFP_KERNEL);  
	if(!RDS_Block_Error_buffer)
	{
		error("Not sufficient memory for creating RDS_Block_Error_buffer");
		ret = -ENOMEM;
		kfree(RDS_Block_Data_buffer);
		goto EXIT;
	}
    
	/*Initialising read and write indices*/
	RDS_Buffer_Index_read= 0;
	RDS_Buffer_Index_write= 0;
	
	/*Creating work-queue*/
	Si4709_wq = create_singlethread_workqueue("Si4709_wq");
	if(!Si4709_wq)
	{
		error("Not sufficient memory for Si4709_wq, work-queue");
		ret = -ENOMEM;
		kfree(RDS_Block_Error_buffer);
		kfree(RDS_Block_Data_buffer);
		goto EXIT;
	}
  
	/*Initialising work_queue*/
	INIT_WORK(&Si4709_work, Si4709_work_func);
	
	RDS_Data_Available = 0;
	RDS_Data_Lost =0;
	RDS_Groups_Available_till_now = 0;
EXIT:	
#endif

	mutex_unlock(&(Si4709_dev.lock));   	 

	debug("Si4709_dev_init call over");
	// JinoKang 2011.06.03	turn off FM Radio in idle
	gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_LOW);

	return ret;
}

int Si4709_dev_exit(void)
{
	int ret = 0;
	 
	debug("Si4709_dev_exit called");

	mutex_lock(&(Si4709_dev.lock));   

	// Temporary blocked by abnormal function call(E-CIM 2657654) - DW Shim. 2010.03.04
	printk(KERN_ERR "Si4709_dev_exit() is called!!");
//    Si4709_dev.client = NULL;	

//    Si4709_dev.valid_client_state = eFALSE;
//    Si4709_dev.valid = eFALSE;

#ifdef RDS_INTERRUPT_ON_ALWAYS 
	if(Si4709_wq)
		destroy_workqueue(Si4709_wq);
	
	if(RDS_Block_Error_buffer)
		kfree(RDS_Block_Error_buffer);
			
	if(RDS_Block_Data_buffer)
		kfree(RDS_Block_Data_buffer);
#endif

	mutex_unlock(&(Si4709_dev.lock)); 

	debug("Si4709_dev_exit call over");

	return ret;
}

void Si4709_dev_mutex_init(void)
{
    mutex_init(&(Si4709_dev.lock));
}

// JinoKang 2011.06.03 for FM Radio Vol 30
#ifdef FMRADIO_VOL_30
extern bool McDrv_Ctrl_fm(struct snd_soc_codec *codec, unsigned int volume);
extern bool wm1811_Set_FM_Radio_Off(struct snd_soc_codec *codec);
extern void wm1811_Set_FM_Mute_Switch_Flag(int isMute);
int Si4709_dev_volume_Fix(void)
{
	int ret = 0;
	u16 sysconfig2 = 0;
	u16 sysconfig3 = 0;
	u8 mvolume = 0;

	debug("Si4709_dev_volume_Fix called");

	mutex_lock(&(Si4709_dev.lock));   

	sysconfig2 = Si4709_dev.registers[SYSCONFIG2];
	sysconfig3 = Si4709_dev.registers[SYSCONFIG3];

	if( Si4709_dev.valid == eFALSE )
	{
		debug("Si4709_dev_volume_set called when DS is invalid");
		ret = -1;   
	}
	else
	{
	
	debug("Si4709_dev_volume_set : volume is %d\n", FM_setVolume);

	
		SYSCONFIG3_BITSET_VOLEXT_DISB(&Si4709_dev.registers[SYSCONFIG3]);
		SYSCONFIG3_BITSET_RESERVED(&Si4709_dev.registers[SYSCONFIG3]);
#if 0
		if(FM_setVolume <= 1)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x02);
		}
		else if(FM_setVolume == 2)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x03);
		}
		else if(FM_setVolume == 3)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x04);
		}
		else if(FM_setVolume == 4)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x06);
		}
		else if(FM_setVolume == 5)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x08);
		}
		else if(FM_setVolume == 6)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x0A);
		}
		else if(FM_setVolume >= 7 && FM_setVolume <= 15)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x0B);
		}
		else if(FM_setVolume == 16)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x0C);
		}
		else if(FM_setVolume == 17)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x0D);
		}
		else if(FM_setVolume == 18)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x0E);
		}
		else if(FM_setVolume >= 19 && FM_setVolume <= 29)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x0F);
		}
		else if(FM_setVolume == 30)
		{
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x0F);
		}
#else
		SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], 0x0F);
#endif
		if( (ret = i2c_write( SYSCONFIG3 )) < 0 )
		{
			debug("Si4709_dev_VOLEXT_DISB i2c_write failed");
			Si4709_dev.registers[SYSCONFIG3] = sysconfig3;
			Si4709_dev.registers[SYSCONFIG2] = sysconfig2;
		}
	}

	mutex_unlock(&(Si4709_dev.lock)); 
	return ret;
}
#endif
int Si4709_dev_powerup(void)
{
	int ret = 0;
	u32 value = 100;
	u16 sysconfig2 = 0;

	debug("Si4709_dev_powerup called");

	mutex_lock(&(Si4709_dev.lock)); 

	if(!(RADIO_ON==Si4709_dev.state.power_state))
	{
		enable_irq(Si4709_IRQ);

#if defined(CONFIG_FMRADIO_USE_GPIO_I2C)
		Si4709_i2c_client = omap_gpio_i2c_init(OMAP_GPIO_FM_SDA,
						  OMAP_GPIO_FM_SCL,
						  0x10,
						  200);
#endif

		debug("Si4709_dev_powerup state start");
		if((ret = powerup()) < 0)
		{
			debug("powerup failed");
		}
		else if( Si4709_dev.valid_client_state == eFALSE )
		{
			debug("Si4709_dev_powerup called when DS (state, client) is invalid");
			ret = -1;	  
		}    
		else
		{
			debug("Si4709_dev_powerup initialize start");
			/*initial settings*/
#if 0
			POWERCFG_BITSET_RDSM_LOW(&Si4709_dev.registers[POWERCFG]);
#else
			POWERCFG_BITSET_RDSM_HIGH(&Si4709_dev.registers[POWERCFG]);
#endif
			//   POWERCFG_BITSET_SKMODE_HIGH(&Si4709_dev.registers[POWERCFG]);
			POWERCFG_BITSET_SKMODE_LOW(&Si4709_dev.registers[POWERCFG]); /*VNVS:18-NOV'09---- wrap at the upper and lower band limit and continue seeking*/
			SYSCONFIG1_BITSET_STCIEN_HIGH(&Si4709_dev.registers[SYSCONFIG1]);
			SYSCONFIG1_BITSET_RDSIEN_LOW(&Si4709_dev.registers[SYSCONFIG1]);
			SYSCONFIG1_BITSET_RDS_HIGH(&Si4709_dev.registers[SYSCONFIG1]);
			SYSCONFIG1_BITSET_DE_50(&Si4709_dev.registers[SYSCONFIG1]); /*VNVS:18-NOV'09--- Setting DE-Time Constant as 50us(Europe,Japan,Australia)*/
			SYSCONFIG1_BITSET_GPIO_STC_RDS_INT(&Si4709_dev.registers[SYSCONFIG1]);
			SYSCONFIG1_BITSET_RESERVED(&Si4709_dev.registers[SYSCONFIG1]);

			//    SYSCONFIG2_BITSET_SEEKTH(&Si4709_dev.registers[SYSCONFIG2],2);
			SYSCONFIG2_BITSET_SEEKTH(&Si4709_dev.registers[SYSCONFIG2], TUNE_RSSI_THRESHOLD); /*VNVS:18-NOV'09---- modified for detecting more stations of good quality*/
			SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2],0x0F);	// JinoKang 2011.06.03
			SYSCONFIG2_BITSET_BAND_87p5_108_MHz(&Si4709_dev.registers[SYSCONFIG2]);
			Si4709_dev.settings.band = BAND_87500_108000_kHz;
			Si4709_dev.settings.bottom_of_band = FREQ_87500_kHz;

			SYSCONFIG2_BITSET_SPACE_100_KHz(&Si4709_dev.registers[SYSCONFIG2]);
			Si4709_dev.settings.channel_spacing = CHAN_SPACING_100_kHz;         				


			//  SYSCONFIG3_BITSET_SKSNR(&Si4709_dev.registers[SYSCONFIG3],3);
			SYSCONFIG3_BITSET_SKSNR(&Si4709_dev.registers[SYSCONFIG3], TUNE_SNR_THRESHOLD);/*VNVS:18-NOV'09---- modified for detecting more stations of good quality*/
			SYSCONFIG3_BITSET_SKCNT(&Si4709_dev.registers[SYSCONFIG3], TUNE_CNT_THRESHOLD);
			SYSCONFIG3_BITSET_VOLEXT_DISB(&Si4709_dev.registers[SYSCONFIG3]);	// JinoKang 2011.06.03

			SYSCONFIG3_BITSET_RESERVED(&Si4709_dev.registers[SYSCONFIG3]);

			Si4709_dev.settings.timeout_RDS = msecs_to_jiffies(value);
			Si4709_dev.settings.curr_snr = TUNE_SNR_THRESHOLD;
			Si4709_dev.settings.curr_rssi_th = TUNE_RSSI_THRESHOLD;

			/*this will write all the above registers*/ 
			if( (ret = i2c_write(SYSCONFIG3)) < 0 )
			{
				debug("Si4709_dev_powerup i2c_write 1 failed");
			}
			else
			{
				debug("==================Si4709_dev_powerup initialize Successful!!!===============");
				Si4709_dev.valid = eTRUE;
#ifdef RDS_INTERRUPT_ON_ALWAYS
				/*Initialising read and write indices*/
				RDS_Buffer_Index_read= 0;
				RDS_Buffer_Index_write= 0;

				RDS_Data_Available = 0;
				RDS_Data_Lost =0;
				RDS_Groups_Available_till_now = 0;
#endif
			}
		}
	}
	else
	{
		debug("Device already Powered-ON");
		sysconfig2 = Si4709_dev.registers[SYSCONFIG2];
		if((ret = i2c_write(SYSCONFIG2)) < 0)
		{
			debug("==================Si4709 Device power error======================");
			debug("==================Si4709 Device init restart=====================");
			if((ret = powerup()) < 0)
			{
				debug("powerup failed");
			}
			else if( Si4709_dev.valid_client_state == eFALSE )
			{
				debug("Si4709_dev_powerup called when DS (state, client) is invalid");
				ret = -1;	  
			}    
			else
			{
				debug("Si4709_dev_powerup initialize start");
				/*initial settings*/
#if 0
				POWERCFG_BITSET_RDSM_LOW(&Si4709_dev.registers[POWERCFG]);
#else
				POWERCFG_BITSET_RDSM_HIGH(&Si4709_dev.registers[POWERCFG]);
#endif
				//   POWERCFG_BITSET_SKMODE_HIGH(&Si4709_dev.registers[POWERCFG]);
				POWERCFG_BITSET_SKMODE_LOW(&Si4709_dev.registers[POWERCFG]); /*VNVS:18-NOV'09---- wrap at the upper and lower band limit and continue seeking*/
				SYSCONFIG1_BITSET_STCIEN_HIGH(&Si4709_dev.registers[SYSCONFIG1]);
				SYSCONFIG1_BITSET_RDSIEN_LOW(&Si4709_dev.registers[SYSCONFIG1]);
				SYSCONFIG1_BITSET_RDS_HIGH(&Si4709_dev.registers[SYSCONFIG1]);
				SYSCONFIG1_BITSET_DE_50(&Si4709_dev.registers[SYSCONFIG1]); /*VNVS:18-NOV'09--- Setting DE-Time Constant as 50us(Europe,Japan,Australia)*/
				SYSCONFIG1_BITSET_GPIO_STC_RDS_INT(&Si4709_dev.registers[SYSCONFIG1]);
				SYSCONFIG1_BITSET_RESERVED(&Si4709_dev.registers[SYSCONFIG1]);

				//    SYSCONFIG2_BITSET_SEEKTH(&Si4709_dev.registers[SYSCONFIG2],2);
				SYSCONFIG2_BITSET_SEEKTH(&Si4709_dev.registers[SYSCONFIG2], TUNE_RSSI_THRESHOLD); /*VNVS:18-NOV'09---- modified for detecting more stations of good quality*/
				SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2],0x0F);	// JinoKang 2011.06.03
				SYSCONFIG2_BITSET_BAND_87p5_108_MHz(&Si4709_dev.registers[SYSCONFIG2]);
				Si4709_dev.settings.band = BAND_87500_108000_kHz;
				Si4709_dev.settings.bottom_of_band = FREQ_87500_kHz;

				SYSCONFIG2_BITSET_SPACE_100_KHz(&Si4709_dev.registers[SYSCONFIG2]);
				Si4709_dev.settings.channel_spacing = CHAN_SPACING_100_kHz;         				


				//  SYSCONFIG3_BITSET_SKSNR(&Si4709_dev.registers[SYSCONFIG3],3);
				SYSCONFIG3_BITSET_SKSNR(&Si4709_dev.registers[SYSCONFIG3], TUNE_SNR_THRESHOLD);/*VNVS:18-NOV'09---- modified for detecting more stations of good quality*/
				SYSCONFIG3_BITSET_SKCNT(&Si4709_dev.registers[SYSCONFIG3], TUNE_CNT_THRESHOLD);
				SYSCONFIG3_BITSET_VOLEXT_DISB(&Si4709_dev.registers[SYSCONFIG3]);	// JinoKang 2011.06.03

				SYSCONFIG3_BITSET_RESERVED(&Si4709_dev.registers[SYSCONFIG3]);

				Si4709_dev.settings.timeout_RDS = msecs_to_jiffies(value);
				Si4709_dev.settings.curr_snr = TUNE_SNR_THRESHOLD;
				Si4709_dev.settings.curr_rssi_th = TUNE_RSSI_THRESHOLD;

				/*this will write all the above registers*/ 
				if( (ret = i2c_write(SYSCONFIG3)) < 0 )
				{
					debug("Si4709_dev_powerup i2c_write 1 failed");
				}
				else
				{
					debug("==================Si4709_dev_powerup initialize Successful!!!===============");
					Si4709_dev.valid = eTRUE;
#ifdef RDS_INTERRUPT_ON_ALWAYS
					/*Initialising read and write indices*/
					RDS_Buffer_Index_read= 0;
					RDS_Buffer_Index_write= 0;

					RDS_Data_Available = 0;
					RDS_Data_Lost =0;
					RDS_Groups_Available_till_now = 0;
#endif
				}
			}
		}
	}

	mutex_unlock(&(Si4709_dev.lock)); 

	return ret;
}

extern struct snd_soc_codec *wm1811_get_snd_soc_codec();

int Si4709_dev_powerdown(void)
{
    int ret = 0;

    msleep(500);	// For avoiding turned off pop noise
    debug("Si4709_dev_powerdown called");

	//FM audio path close when power down, sejong.
    wm1811_Set_FM_Radio_Off(wm1811_get_snd_soc_codec());

    mutex_lock(&(Si4709_dev.lock)); 

    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_powerdown called when DS is invalid");
	       ret = -1;	  
    }
	else
	{
		if ( ( ret = powerdown()) < 0 )
		{
			debug("powerdown failed");
		}
#if defined(CONFIG_FMRADIO_USE_GPIO_I2C)
		omap_gpio_i2c_deinit(Si4709_i2c_client);
		Si4709_i2c_client = NULL;
#endif
		disable_irq(Si4709_IRQ);
	}
    mutex_unlock(&(Si4709_dev.lock)); 

    return ret;
}

int Si4709_dev_suspend(void)
{
    int ret = 0;
	 
    debug("Si4709_dev_suspend called");

    mutex_lock(&(Si4709_dev.lock)); 

    if( Si4709_dev.valid_client_state== eFALSE )
    {
        debug("Si4709_dev_suspend called when DS (state, client) is invalid");
	       ret = -1;	  
    }    
#if 0
    else if( Si4709_dev.state.power_state == RADIO_ON )
    {
        ret = powerdown();
    }
#endif
    
    mutex_unlock(&(Si4709_dev.lock)); 

    debug("Si4709_dev_enable call over");

    return ret;
}

int Si4709_dev_resume(void)
{
    int ret = 0;
	 
//    debug("Si4709_dev_resume called");

    mutex_lock(&(Si4709_dev.lock));   

    if( Si4709_dev.valid_client_state == eFALSE )
    {
        debug("Si4709_dev_resume called when DS (state, client) is invalid");
	       ret = -1;	  
    }    
#if 0
    else  if( Si4709_dev.state.power_state == RADIO_POWERDOWN )
    {
        if( (ret = powerup()) < 0 )
        {
            debug("powerup failed");
        }
    }
#endif
	 
    mutex_unlock(&(Si4709_dev.lock)); 

//    debug("Si4709_dev_disable call over");

    return ret;
}

int Si4709_dev_band_set(int band)
{
    int ret = 0;
    u16 sysconfig2 =0; 
    u16 prev_band = 0;
    u32 prev_bottom_of_band = 0;
   
    debug("Si4709_dev_band_set called");
 
    mutex_lock(&(Si4709_dev.lock));   
     sysconfig2 = Si4709_dev.registers[SYSCONFIG2];     
     prev_band = Si4709_dev.settings.band;
     prev_bottom_of_band = Si4709_dev.settings.bottom_of_band;
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_band_set called when DS is invalid");
	       ret = -1;	  
    }
    else
    {
        switch (band)
        {
            case BAND_87500_108000_kHz:
            	   SYSCONFIG2_BITSET_BAND_87p5_108_MHz(&Si4709_dev.registers[SYSCONFIG2]);
            	   Si4709_dev.settings.band = BAND_87500_108000_kHz;
            	   Si4709_dev.settings.bottom_of_band = FREQ_87500_kHz;
            	   break;

            case BAND_76000_108000_kHz:
                SYSCONFIG2_BITSET_BAND_76_108_MHz(&Si4709_dev.registers[SYSCONFIG2]);
                Si4709_dev.settings.band = BAND_76000_108000_kHz;
                Si4709_dev.settings.bottom_of_band = FREQ_76000_kHz;
                break;

            case BAND_76000_90000_kHz:	
                SYSCONFIG2_BITSET_BAND_76_90_MHz(&Si4709_dev.registers[SYSCONFIG2]);
                Si4709_dev.settings.band = BAND_76000_90000_kHz;
                Si4709_dev.settings.bottom_of_band = FREQ_76000_kHz;
                break;

            default:
            	   ret = -1;
        }

        if(ret == 0)
        {
            if( (ret = i2c_write(SYSCONFIG2)) < 0 )
            {
                debug("Si4709_dev_band_set i2c_write 1 failed");
                Si4709_dev.settings.band = prev_band;
                Si4709_dev.settings.bottom_of_band = prev_bottom_of_band;
                Si4709_dev.registers[SYSCONFIG2] = sysconfig2;
            }
        }
    }
    
    mutex_unlock(&(Si4709_dev.lock)); 
 
    return ret;
}

int Si4709_dev_ch_spacing_set(int ch_spacing)
{
    int ret = 0;
    u16 sysconfig2 = 0; 
    u16 prev_ch_spacing = 0;
    
    debug("Si4709_dev_ch_spacing_set called");										
  
    mutex_lock(&(Si4709_dev.lock));   
     sysconfig2 = Si4709_dev.registers[SYSCONFIG2]; 
     prev_ch_spacing = Si4709_dev.settings.channel_spacing;
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_ch_spacing_set called when DS is invalid");				
        ret = -1;   
    }
    else
    {
        switch (ch_spacing)
        {
            case CHAN_SPACING_200_kHz:
                SYSCONFIG2_BITSET_SPACE_200_KHz(&Si4709_dev.registers[SYSCONFIG2]);
                Si4709_dev.settings.channel_spacing = CHAN_SPACING_200_kHz;
                break;
 
            case CHAN_SPACING_100_kHz:
                SYSCONFIG2_BITSET_SPACE_100_KHz(&Si4709_dev.registers[SYSCONFIG2]);
                Si4709_dev.settings.channel_spacing = CHAN_SPACING_100_kHz;
                break;
 
            case CHAN_SPACING_50_kHz: 
                SYSCONFIG2_BITSET_SPACE_50_KHz(&Si4709_dev.registers[SYSCONFIG2]);
                Si4709_dev.settings.channel_spacing = CHAN_SPACING_50_kHz;
                break;
 
            default:
                ret = -1;
        }
 
        if(ret == 0)
        {
            if( (ret = i2c_write(SYSCONFIG2)) < 0 )
            {
                debug("Si4709_dev_ch_spacing_set i2c_write 1 failed");
                Si4709_dev.settings.channel_spacing = prev_ch_spacing;
                Si4709_dev.registers[SYSCONFIG2] = sysconfig2;
            }
			
        }
    }
    
    mutex_unlock(&(Si4709_dev.lock)); 
  
    return ret;
}

int Si4709_dev_chan_select(u32 frequency)
{
    int ret = 0;
  
    debug("Si4709_dev_chan_select called");
 
    mutex_lock(&(Si4709_dev.lock));   

    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_chan_select called when DS is invalid");
	       ret = -1;	  
    }
    else
    {
        Si4709_dev.state.seek_state = RADIO_SEEK_ON; 
        
        ret = tune_freq(frequency);
        debug("Si4709_dev_chan_select called1");
        Si4709_dev.state.seek_state = RADIO_SEEK_OFF; 
    }

    mutex_unlock(&(Si4709_dev.lock)); 
 
    return ret;
}

int Si4709_dev_chan_get(u32 *frequency)
{
    int ret = 0;

    
    debug("Si4709_dev_chan_get called");
  
    mutex_lock(&(Si4709_dev.lock));   

    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_chan_get called when DS is invalid");
	       ret = -1;	  
    }
    else if( (ret = i2c_read(READCHAN)) < 0 )
    {
        debug("Si4709_dev_chan_get i2c_read failed");
    }
    else
    {
        get_cur_chan_freq(frequency, Si4709_dev.registers[READCHAN]);
        debug("frequency: %u",*frequency);
    }
    
    mutex_unlock(&(Si4709_dev.lock)); 
  
    return ret;
}

int Si4709_dev_seek_up(u32 *frequency)
{
    int ret = 0;
   
    debug("Si4709_dev_seek_up called");
   
    mutex_lock(&(Si4709_dev.lock)); 

    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_seek_up called when DS is invalid");
	       ret = -1;	  
    }
    else
    {
        Si4709_dev.state.seek_state = RADIO_SEEK_ON; 
        
        ret = seek(frequency, 1);

        Si4709_dev.state.seek_state = RADIO_SEEK_OFF; 
    }

    mutex_unlock(&(Si4709_dev.lock)); 
   
    return ret;
}

int Si4709_dev_seek_down(u32 *frequency)
{
    int ret = 0;
   
    debug("Si4709_dev_seek_down called");
   
    mutex_lock(&(Si4709_dev.lock));   

    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_seek_down called when DS is invalid");
	       ret = -1;	  
    }
    else
    {
        Si4709_dev.state.seek_state = RADIO_SEEK_ON;   

        ret = seek(frequency, 0);
 
        Si4709_dev.state.seek_state = RADIO_SEEK_OFF;
    }

    mutex_unlock(&(Si4709_dev.lock)); 
   
    return ret;
}


#if 0
int Si4709_dev_seek_auto(u32 *seek_preset_user)
{
   u8 *rssi_seek;
   int ret = 0;
   int i =0;
   int j = 0;
   channel_into_t temp;

     debug("Si4709_dev_seek_auto called");
     
     if( Si4709_dev.valid == eFALSE )
       {
            debug("Si4709_dev_seek_auto called when DS is invalid");
	          ret = -1;	  
        }
    
    else if( (rssi_seek = (u8 *)kzalloc(sizeof(u8) * NUM_SEEK_PRESETS, GFP_KERNEL)) == NULL )
    {
         debug("Si4709_ioctl: no memory");
         ret = -ENOMEM;
    } 
               
    else
    	  {
    	        
       	      if( (ret = tune_freq(FREQ_87500_kHz)) == 0 )
                   {
                        debug("Si4709_dev_seek_auto tune_freq success");
                        get_cur_chan_freq(&(Si4709_dev.rssi_freq[0].frequency), Si4709_dev.registers[READCHAN]);
                        Si4709_dev_cur_RSSI_get(&(Si4709_dev.rssi_freq[0].rsssi_val));
                       
                    } 
       	      else
       	      	{
       	      	    debug("tunning failed, seek auto failed");
       	      	    ret =-1;
       	      	}
   #if 0    	    
              for(i=0;i<50; i++)
              	{       
                    	    if( (ret = seek(&(Si4709_dev.settings.seek_preset[i]),1)) == 0 )
                    	    	{
                    	    	     get_cur_chan_freq(&(Si4709_dev.rssi_freq[i].frequency), Si4709_dev.registers[READCHAN]);
									Si4709_dev_cur_RSSI_get(&(Si4709_dev.rssi_freq[i].rsssi_val));
									rssi_seek ++;
                           	}
                    	    
                    	    else
                    	      	{
                    	    	     debug("seek failed");
                    	    	    
                    	      	}
              	}
     #endif         
       
    

    /***new method ****/
                for(i=1;i<30; i++)
              	{       
                    	    if( (ret = seek(&(Si4709_dev.settings.seek_preset[i]),1)) == 0 )
                    	    	{
                    	    	     get_cur_chan_freq(&(Si4709_dev.rssi_freq[i].frequency), Si4709_dev.registers[READCHAN]);
                               Si4709_dev_cur_RSSI_get(&(Si4709_dev.rssi_freq[i].rsssi_val));
                           	}
                    	    
                    	    else
                    	      	{
                    	    	     debug("seek failed");
                    	    	    
                    	      	}
              	}
             /***Sort the array of structures on the basis of RSSI value****/
             for(i=0;i<29;i++)
                {
                
                     for(j=i+1;j<30;j++)
                        {
                        
                            if(  Si4709_dev.rssi_freq[j].rsssi_val>Si4709_dev.rssi_freq[i].rsssi_val)
                            	{
                            	    temp=Si4709_dev.rssi_freq[i];
                            	    Si4709_dev.rssi_freq[i]=Si4709_dev.rssi_freq[j];
                            	    Si4709_dev.rssi_freq[j]=temp;
                            	}
                        
                     	 }

               	  }

             /***Store the frequency in Array*****/
             for(i=0;i<19;i++)
             	{
             	     Si4709_dev.settings.seek_preset[i]=Si4709_dev.rssi_freq[i].frequency;
             	}

                
     
    	}
    memcpy(seek_preset_user, Si4709_dev.settings.seek_preset , sizeof(int)*NUM_SEEK_PRESETS);
    kfree(rssi_seek);
    return ret;
}
#endif



int Si4709_dev_RSSI_seek_th_set(u8 seek_th)
{
    int ret = 0;
    u16 sysconfig2 = 0;
       
    debug("Si4709_dev_RSSI_seek_th_set called 0x%x", seek_th);
     
    mutex_lock(&(Si4709_dev.lock));   
    sysconfig2 = Si4709_dev.registers[SYSCONFIG2];
    
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_RSSI_seek_th_set called when DS is invalid");
	       ret = -1;	  
    }
    else
    {
        SYSCONFIG2_BITSET_SEEKTH(&Si4709_dev.registers[SYSCONFIG2], seek_th);
        Si4709_dev.settings.curr_rssi_th=seek_th;
        if( (ret = i2c_write( SYSCONFIG2 )) < 0 )
        {
            debug("Si4709_dev_RSSI_seek_th_set i2c_write 1 failed");
            Si4709_dev.registers[SYSCONFIG2] = sysconfig2;
        }
    }
    
    mutex_unlock(&(Si4709_dev.lock)); 
     
    return ret;
}

int Si4709_dev_seek_SNR_th_set(u8 seek_SNR)
{
    int ret = 0;
    u16 sysconfig3 = 0;
        
    debug("Si4709_dev_seek_SNR_th_set called 0x%x", seek_SNR);
      
    mutex_lock(&(Si4709_dev.lock));   
    
    sysconfig3 = Si4709_dev.registers[SYSCONFIG3]; 
    
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_seek_SNR_th_set called when DS is invalid");
	       ret = -1;	  
    }
    else
    {
        SYSCONFIG3_BITSET_SKSNR(&Si4709_dev.registers[SYSCONFIG3], seek_SNR);
        SYSCONFIG3_BITSET_RESERVED(&Si4709_dev.registers[SYSCONFIG3]);
        Si4709_dev.settings.curr_snr=seek_SNR;

        if( (ret = i2c_write( SYSCONFIG3 )) < 0 )
        {
           debug("Si4709_dev_seek_SNR_th_set i2c_write 1 failed");
           Si4709_dev.registers[SYSCONFIG3] = sysconfig3;
        }
    }
    
    mutex_unlock(&(Si4709_dev.lock)); 
      
    return ret;
}

int Si4709_dev_seek_FM_ID_th_set(u8 seek_FM_ID_th)
{
    int ret = 0;
    u16 sysconfig3 = 0;
        
    debug("Si4709_dev_seek_FM_ID_th_set called 0x%x", seek_FM_ID_th);
       
    mutex_lock(&(Si4709_dev.lock));   
    
    sysconfig3 = Si4709_dev.registers[SYSCONFIG3];
    
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_seek_SNR_th_set called when DS is invalid");
	       ret = -1;	  
    }
    else
    {
        SYSCONFIG3_BITSET_SKCNT(&Si4709_dev.registers[SYSCONFIG3], seek_FM_ID_th);
        SYSCONFIG3_BITSET_RESERVED(&Si4709_dev.registers[SYSCONFIG3]);

        if( (ret = i2c_write( SYSCONFIG3 )) < 0 )
        {
            debug("Si4709_dev_seek_FM_ID_th_set i2c_write 1 failed");
            sysconfig3 = Si4709_dev.registers[SYSCONFIG3];
        }
    }       
        
    mutex_unlock(&(Si4709_dev.lock)); 
       
    return ret;
}

int Si4709_dev_cur_RSSI_get(rssi_snr_t *cur_RSSI)
{
    int ret = 0;
         
    //debug("Si4709_dev_cur_RSSI_get called");
        
    mutex_lock(&(Si4709_dev.lock));   
   
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_cur_RSSI_get called when DS is invalid");
	       ret = -1;	  
    }
    else
    {
        if( (ret = i2c_read(STATUSRSSI)) < 0 )
        {
            debug("Si4709_dev_cur_RSSI_get i2c_read 1 failed");
        }
        else
        {
              
            cur_RSSI->curr_rssi= STATUSRSSI_RSSI_SIGNAL_STRENGTH(Si4709_dev.registers[STATUSRSSI]);
            cur_RSSI->curr_rssi_th=Si4709_dev.settings.curr_rssi_th;
            cur_RSSI->curr_snr=Si4709_dev.settings.curr_snr;
			debug("Si4709_dev_cur_RSSI_get rssi 0x%x, rssi_th 0x%x, snr 0x%x", cur_RSSI->curr_rssi, cur_RSSI->curr_rssi_th, cur_RSSI->curr_snr);
        }
    }     
    mutex_unlock(&(Si4709_dev.lock)); 
        
    return ret;
}											

/*VNVS:START 13-OCT'09---- Functions which reads device-id,chip-id,power configuration, system configuration2 registers */	
int Si4709_dev_device_id(device_id *dev_id)												
{		
	int ret = 0;
         
    debug("Si4709_dev_device_id called");
        
    mutex_lock(&(Si4709_dev.lock));   
   
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_device_id called when DS is invalid");
	       ret = -1;	  
    }
    else
    {
        if( (ret = i2c_read(DEVICE_ID)) < 0 )
        {
            debug("Si4709_dev_device_id i2c_read failed");
        }
        else
        {
              
            dev_id->part_number= DEVICE_ID_PART_NUMBER(Si4709_dev.registers[DEVICE_ID]);
			dev_id->manufact_number =  DEVICE_ID_MANUFACT_NUMBER(Si4709_dev.registers[DEVICE_ID]);
            
        }
    }     
    mutex_unlock(&(Si4709_dev.lock)); 
        
    return ret;
}																					

int Si4709_dev_chip_id(chip_id *chp_id)												
{		
	int ret = 0;
         
    debug("Si4709_dev_chip_id called");
        
    mutex_lock(&(Si4709_dev.lock));   
   
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_chip_id called when DS is invalid");
	       ret = -1;	  
    }
    else
    {
        if( (ret = i2c_read(CHIP_ID)) < 0 )
        {
            debug("Si4709_dev_chip_id i2c_read failed");
        }
        else
        {
              
            chp_id->chip_version= CHIP_ID_CHIP_VERSION(Si4709_dev.registers[CHIP_ID]);
			chp_id->device =  CHIP_ID_DEVICE(Si4709_dev.registers[CHIP_ID]);
            chp_id->firmware_version = CHIP_ID_FIRMWARE_VERSION(Si4709_dev.registers[CHIP_ID]);
        }
    }     
    mutex_unlock(&(Si4709_dev.lock)); 
        
    return ret;
}                                                                                         

int Si4709_dev_sys_config2(sys_config2 *sys_conf2)
{
	int ret = 0;
         
	//debug("Si4709_sys_config2 called");
        
	mutex_lock(&(Si4709_dev.lock));   
   
	if( Si4709_dev.valid == eFALSE )
	{
		debug("Si4709_sys_config2 called when DS is invalid");
		ret = -1;	  
	}
	else
	{
		if((ret = i2c_read(SYSCONFIG2)) < 0)
		{
			debug("Si4709_sys_config2 i2c_read failed");
		}
		else
		{
			sys_conf2->rssi_th =SYS_CONFIG2_RSSI_TH(Si4709_dev.registers[SYSCONFIG2]);
			sys_conf2->fm_band = SYS_CONFIG2_FM_BAND(Si4709_dev.registers[SYSCONFIG2]);
			sys_conf2->fm_chan_spac = SYS_CONFIG2_FM_CHAN_SPAC(Si4709_dev.registers[SYSCONFIG2]);
			sys_conf2->fm_vol = SYS_CONFIG2_FM_VOL(Si4709_dev.registers[SYSCONFIG2]);
			debug("Si4709_sys_config2 rssi_th 0x%x fm_band 0x%x, fm_chan_spac 0x%x, fm_vol 0x%x", 
					sys_conf2->rssi_th, sys_conf2->fm_band, sys_conf2->fm_chan_spac,sys_conf2->fm_vol);
				
		}
	}     
	mutex_unlock(&(Si4709_dev.lock)); 
        
	return ret;
}                                                                                              

int Si4709_dev_sys_config3(sys_config3 *sys_conf3)
{
	int ret = 0;
         
	//debug("Si4709_sys_config3 called");
        
	mutex_lock(&(Si4709_dev.lock));   
   
	if( Si4709_dev.valid == eFALSE )
	{
		debug("Si4709_sys_config3 called when DS is invalid");
		ret = -1;	  
	}
	else
	{
		if((ret = i2c_read(SYSCONFIG3)) < 0)
		{
			debug("Si4709_sys_config3 i2c_read failed");
		}
		else
		{
			sys_conf3->smmute = (Si4709_dev.registers[SYSCONFIG3] & 0xC000) >> 14;
			sys_conf3->smutea = (Si4709_dev.registers[SYSCONFIG3] & 0x3000) >> 12;
			sys_conf3->volext = (Si4709_dev.registers[SYSCONFIG3] & 0x0100) >> 8;
			sys_conf3->sksnr = (Si4709_dev.registers[SYSCONFIG3] & 0x00F0) >> 4;
			sys_conf3->skcnt = (Si4709_dev.registers[SYSCONFIG3] & 0x000F);
			debug("Si4709_sys_config3 smmute 0x%x smutea 0x%x, volext 0x%x, sksnr 0x%x skcnt 0x%x", 
					sys_conf3->smmute, sys_conf3->smutea, sys_conf3->volext, sys_conf3->sksnr, sys_conf3->skcnt);
		}
	}     
	mutex_unlock(&(Si4709_dev.lock)); 
        
	return ret;
}                                                                                              

int Si4709_dev_status_rssi(status_rssi *status)
{
	int ret = 0;
         
	//debug("Si4709_dev_status_rssi called");
        
	mutex_lock(&(Si4709_dev.lock));   
   
	if( Si4709_dev.valid == eFALSE )
	{
		debug("Si4709_dev_status_rssi called when DS is invalid");
		ret = -1;	  
	}
	else
	{
		if((ret = i2c_read(STATUSRSSI)) < 0)
		{
			debug("Si4709_sys_config3 i2c_read failed");
		}
		else
		{
			status->rdsr = (Si4709_dev.registers[STATUSRSSI] & 0x8000) >> 15;
			status->stc = (Si4709_dev.registers[STATUSRSSI] & 0x4000) >> 14;
			status->sfbl = (Si4709_dev.registers[STATUSRSSI] & 0x2000) >> 13;
			status->afcrl = (Si4709_dev.registers[STATUSRSSI] & 0x1000) >> 12;
			status->rdss = (Si4709_dev.registers[STATUSRSSI] & 0x0800) >> 11;
			status->blera = (Si4709_dev.registers[STATUSRSSI] & 0x0600) >> 9; 
			status->st = (Si4709_dev.registers[STATUSRSSI] & 0x0100) >> 8;
			status->rssi = (Si4709_dev.registers[STATUSRSSI] & 0x00FF);
		}
	}     
	mutex_unlock(&(Si4709_dev.lock)); 
        
	return ret;
}                                                                                              

int Si4709_dev_sys_config2_set(sys_config2 *sys_conf2)
{
	int ret = 0;
	u16 register_bak = 0;
         
	//debug("Si4709_dev_sys_config2_set called");
        
	mutex_lock(&(Si4709_dev.lock));   

	register_bak = Si4709_dev.registers[SYSCONFIG2];
    
	if( Si4709_dev.valid == eFALSE )
	{
		debug("Si4709_dev_sys_config2_set called when DS is invalid");
		ret = -1;
	}
	else
	{
		debug(KERN_ERR "Si4709_dev_sys_config2_set() : Register Value = [0x%X], rssi-th = [%X]\n", Si4709_dev.registers[SYSCONFIG2], sys_conf2->rssi_th);
		Si4709_dev.registers[SYSCONFIG2] = (Si4709_dev.registers[SYSCONFIG2] & 0x00FF) | ((sys_conf2->rssi_th) << 8);
		Si4709_dev.registers[SYSCONFIG2] = (Si4709_dev.registers[SYSCONFIG2] & 0xFF3F) | ((sys_conf2->fm_band) << 6);
		Si4709_dev.registers[SYSCONFIG2] = (Si4709_dev.registers[SYSCONFIG2] & 0xFFCF) | ((sys_conf2->fm_chan_spac) << 4);
		Si4709_dev.registers[SYSCONFIG2] = (Si4709_dev.registers[SYSCONFIG2] & 0xFFF0) | (sys_conf2->fm_vol);
		debug(KERN_ERR "Si4709_dev_sys_config2_set() : After Register Value = [0x%X]\n", Si4709_dev.registers[SYSCONFIG2]);
			
		if( (ret = i2c_write( SYSCONFIG2 )) < 0 )
		{
			debug("Si4709_dev_sys_config2_set i2c_write 1 failed");
			Si4709_dev.registers[SYSCONFIG2] = register_bak;
		}
		else
			debug(KERN_ERR "Si4709_dev_sys_config2_set() : Write Sucess!!");	
	}
	
	mutex_unlock(&(Si4709_dev.lock)); 
        
	return ret;
}                                                                                              

int Si4709_dev_sys_config3_set(sys_config3 *sys_conf3)
{
	int ret = 0;
	u16 register_bak = 0;
         
	//debug("Si4709_dev_sys_config3_set called");
        
	mutex_lock(&(Si4709_dev.lock));   

	register_bak = Si4709_dev.registers[SYSCONFIG3];
    
	if( Si4709_dev.valid == eFALSE )
	{
		debug("Si4709_dev_sys_config3_set called when DS is invalid");
		ret = -1;
	}
	else
	{
		debug(KERN_ERR "Si4709_dev_sys_config3_set() : Register Value = [0x%X], sksnrth = [%X]\n skcnt = [%X]\n", 
							Si4709_dev.registers[SYSCONFIG3], sys_conf3->sksnr, sys_conf3->skcnt);
		Si4709_dev.registers[SYSCONFIG3] = (Si4709_dev.registers[SYSCONFIG3] & 0x3FFF) | ((sys_conf3->smmute) << 14);
		Si4709_dev.registers[SYSCONFIG3] = (Si4709_dev.registers[SYSCONFIG3] & 0xCFFF) | ((sys_conf3->smutea) << 12);
		Si4709_dev.registers[SYSCONFIG3] = (Si4709_dev.registers[SYSCONFIG3] & 0xFEFF) | ((sys_conf3->volext) << 8);
		Si4709_dev.registers[SYSCONFIG3] = (Si4709_dev.registers[SYSCONFIG3] & 0xFF0F) | ((sys_conf3->sksnr) << 4);
		Si4709_dev.registers[SYSCONFIG3] = (Si4709_dev.registers[SYSCONFIG3] & 0xFFF0) | (sys_conf3->skcnt);
		debug(KERN_ERR "Si4709_dev_sys_config3_set() : After Register Value = [0x%X]\n", Si4709_dev.registers[SYSCONFIG3]);
			
		if((ret = i2c_write( SYSCONFIG3)) < 0)
		{
			debug("Si4709_dev_sys_config3_set i2c_write 1 failed");
			Si4709_dev.registers[SYSCONFIG3] = register_bak;
		}
	}
	
	mutex_unlock(&(Si4709_dev.lock)); 
        
	return ret;
}                                                                                              

int Si4709_dev_power_config(power_config *pow_conf)											   
{
	int ret =0;
	
	debug("Si4709_dev_power_config called");
	mutex_lock(&(Si4709_dev.lock));   
   
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_power_config called when DS is invalid");
	    ret = -1;	  
    }
    else
    {
        if( (ret = i2c_read(POWERCFG)) < 0 )
        {
            debug("Si4709_dev_power_config i2c_read failed");
        }
        else
        {
              
            pow_conf->dsmute		=POWER_CONFIG_SOFTMUTE_STATUS(Si4709_dev.registers[POWERCFG]);
			pow_conf->dmute 		=POWER_CONFIG_MUTE_STATUS(Si4709_dev.registers[POWERCFG]);
            pow_conf->mono 			=POWER_CONFIG_MONO_STATUS(Si4709_dev.registers[POWERCFG]);
			pow_conf->rds_mode		=POWER_CONFIG_RDS_MODE_STATUS(Si4709_dev.registers[POWERCFG]);
			pow_conf->sk_mode		=POWER_CONFIG_SKMODE_STATUS(Si4709_dev.registers[POWERCFG]);
			pow_conf->seek_up		=POWER_CONFIG_SEEKUP_STATUS(Si4709_dev.registers[POWERCFG]);
			pow_conf->seek			=POWER_CONFIG_SEEK_STATUS(Si4709_dev.registers[POWERCFG]);
			pow_conf->power_disable	=POWER_CONFIG_DISABLE_STATUS(Si4709_dev.registers[POWERCFG]);
			pow_conf->power_enable	=POWER_CONFIG_ENABLE_STATUS(Si4709_dev.registers[POWERCFG]);
        }
    }     
    mutex_unlock(&(Si4709_dev.lock)); 
        
    return ret;
}            					
/*VNVS:END*/

/*VNVS:START 18-NOV'09*/
/*Reading AFCRL Bit*/
int Si4709_dev_AFCRL_get(u8 *afc)
{
    int ret = 0;

    debug("Si4709_dev_AFCRL_get called");
            
    mutex_lock(&(Si4709_dev.lock));   

    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_AFCRL_get called when DS is invalid");
         ret = -1;   
    }
    else
    {
		if( (ret = i2c_read(STATUSRSSI)) < 0 )
        {
            debug("Si4709_dev_AFCRL_get i2c_read failed");
        }
        *afc = STATUSRSSI_AFC_RAIL_STATUS(Si4709_dev.registers[STATUSRSSI]);
    }            
             
    mutex_unlock(&(Si4709_dev.lock)); 
            
    return ret;
}
/*Setting DE-emphasis time constant 50us(Europe,Japan,Australia) or 75us(USA)*/
int Si4709_dev_DE_set(u8 de_tc)
{
    u16 sysconfig1 = 0;
    int ret = 0;
             
    debug("Si4709_dev_DE_set called");
            
    mutex_lock(&(Si4709_dev.lock));   
    
    sysconfig1 = Si4709_dev.registers[SYSCONFIG1];
    
    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_DE_set called when DS is invalid");
         ret = -1;   
    }
    else
    {
     	 switch(de_tc)
		 {		 
			case DE_TIME_CONSTANT_50:
				SYSCONFIG1_BITSET_DE_50(&Si4709_dev.registers[SYSCONFIG1]);
				SYSCONFIG1_BITSET_RESERVED( &Si4709_dev.registers[SYSCONFIG1] );
				break;
			case DE_TIME_CONSTANT_75:
				SYSCONFIG1_BITSET_DE_75(&Si4709_dev.registers[SYSCONFIG1]);
				SYSCONFIG1_BITSET_RESERVED( &Si4709_dev.registers[SYSCONFIG1] );
				break;
			default:
                ret = -1;
		 }
    	 
		 if(0==ret)
			if( (ret = i2c_write(SYSCONFIG1)) < 0 )
			{
				debug("Si4709_dev_DE_set i2c_write failed");
				Si4709_dev.registers[SYSCONFIG1] = sysconfig1;
			}
   	} 	   

    mutex_unlock(&(Si4709_dev.lock)); 

    return ret;
}

/*Resetting the RDS Data Buffer*/
int Si4709_dev_reset_rds_data()
{
    int ret = 0;

    debug_rds("Si4709_dev_reset_rds_data called");
            
    mutex_lock(&(Si4709_dev.lock));   

    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_reset_rds_data called when DS is invalid");
         ret = -1;   
    }
    else
    {
		RDS_Buffer_Index_write = 0;	
  		RDS_Buffer_Index_read = 0;
		RDS_Data_Lost = 0;
		RDS_Data_Available = 0;	
		memset(RDS_Block_Data_buffer,0,RDS_BUFFER_LENGTH*8); 
		memset(RDS_Block_Error_buffer,0,RDS_BUFFER_LENGTH*4);        
    }            
             
    mutex_unlock(&(Si4709_dev.lock)); 
	
    return ret;
}
/*VNVS:END*/
int Si4709_dev_VOLEXT_ENB(void)
{
    int ret = 0;
    u16 sysconfig3 = 0;
          
    debug("Si4709_dev_VOLEXT_ENB called");
         
    mutex_lock(&(Si4709_dev.lock)); 
    
    sysconfig3 = Si4709_dev.registers[SYSCONFIG3];    
     
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_VOLEXT_ENB called when DS is invalid");
        ret = -1;   
    }
    else
    {
        SYSCONFIG3_BITSET_VOLEXT_ENB(&Si4709_dev.registers[SYSCONFIG3]);
        SYSCONFIG3_BITSET_RESERVED(&Si4709_dev.registers[SYSCONFIG3]);

        if( (ret = i2c_write( SYSCONFIG3 )) < 0 )
        {
            debug("Si4709_dev_VOLEXT_ENB i2c_write failed");
            Si4709_dev.registers[SYSCONFIG3] = sysconfig3;
        }
    }
          
    mutex_unlock(&(Si4709_dev.lock)); 
         
    return ret;
}

int Si4709_dev_VOLEXT_DISB(void)
{
    int ret = 0;
    u16 sysconfig3 = 0;
           
    debug("Si4709_dev_VOLEXT_DISB called");
          
    mutex_lock(&(Si4709_dev.lock));  
    
    sysconfig3 = Si4709_dev.registers[SYSCONFIG3];    
    
    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_VOLEXT_DISB called when DS is invalid");
         ret = -1;   
    }
    else
    {
         SYSCONFIG3_BITSET_VOLEXT_DISB(&Si4709_dev.registers[SYSCONFIG3]);
         SYSCONFIG3_BITSET_RESERVED(&Si4709_dev.registers[SYSCONFIG3]);
       
         if( (ret = i2c_write( SYSCONFIG3 )) < 0 )
         {
             debug("Si4709_dev_VOLEXT_DISB i2c_write failed");
             Si4709_dev.registers[SYSCONFIG3] = sysconfig3;
         }
    }
          
    mutex_unlock(&(Si4709_dev.lock)); 
          
    return ret;
}




#ifdef FMRADIO_VOL_30
int Si4709_dev_volume_set(u8 volume)
{
	int ret = 0;
            
	debug("Si4709_dev_volume_set called");
	debug("Si4709_dev_volume_set :: volume is %d , FM_setVolume is %d \n",volume,FM_setVolume);

#if 0
	if (volume == 0 )
	{
		FM_setVolume = volume;
		Si4709_dev_volume_Fix();
		McDrv_Ctrl_fm(wm1811_get_snd_soc_codec(), 0);
	}
	else
	{
		FM_setVolume = volume;
		Si4709_dev_volume_Fix();
		McDrv_Ctrl_fm(wm1811_get_snd_soc_codec(), FM_setVolume);
	}
#else	// fix FM volume. kh76.kim. 11.08.22
	FM_setVolume = volume;
	McDrv_Ctrl_fm(wm1811_get_snd_soc_codec(), FM_setVolume);
#endif

    return ret;
}
#else	// JinoKang 2011.06.03
int Si4709_dev_volume_set(u8 volume)
{
    int ret = 0;
    u16 sysconfig2 = 0;
            
    debug("Si4709_dev_volume_set called volume %d", volume);
           
    mutex_lock(&(Si4709_dev.lock));   
    
    sysconfig2 = Si4709_dev.registers[SYSCONFIG2];       
    
    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_volume_set called when DS is invalid");
         ret = -1;   
    }
    else
    {
         SYSCONFIG2_BITSET_VOLUME(&Si4709_dev.registers[SYSCONFIG2], volume);
         
         if( (ret = i2c_write( SYSCONFIG2 )) < 0 )
         {
             debug("Si4709_dev_volume_set i2c_write failed");
             Si4709_dev.registers[SYSCONFIG2] = sysconfig2;
         }
    }
    
    mutex_unlock(&(Si4709_dev.lock)); 
           
    return ret;
}
#endif	// JinoKang 2011.06.03

int Si4709_dev_volume_get(u8 *volume)
{
    int ret = 0;

    debug("Si4709_dev_volume_get called");
            
    mutex_lock(&(Si4709_dev.lock));   

    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_volume_get called when DS is invalid");
         ret = -1;   
    }
    else
    {
#ifdef FMRADIO_VOL_30
	*volume = FM_setVolume;
#else
	*volume = SYSCONFIG2_GET_VOLUME(Si4709_dev.registers[SYSCONFIG2]);
#endif	// JinoKang 2011.06.03
    }            
             
    mutex_unlock(&(Si4709_dev.lock)); 
            
    return ret;
}

/*
  VNVS:START 19-AUG'10---- Adding DSMUTE ON/OFF feature.The Soft Mute feature is available to attenuate the audio
  outputs and minimize audible noise in very weak signal conditions.
  */
int Si4709_dev_DSMUTE_ON(void)
{
    int ret = 0;
    u16 powercfg = 0;
              
    debug("Si4709_dev_DSMUTE_ON called");
             
    mutex_lock(&(Si4709_dev.lock));
	
    powercfg = Si4709_dev.registers[POWERCFG];
	
    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_DSMUTE_ON called when DS is invalid");
         ret = -1;   
    }
    else
    {
         POWERCFG_BITSET_DSMUTE_LOW(&Si4709_dev.registers[POWERCFG]);
         POWERCFG_BITSET_RESERVED(&Si4709_dev.registers[POWERCFG]);

         if( (ret = i2c_write( POWERCFG )) < 0 )
         {
             error("Si4709_dev_DSMUTE_ON i2c_write failed");
             Si4709_dev.registers[POWERCFG] = powercfg;
         }
    }                   
              
    mutex_unlock(&(Si4709_dev.lock)); 
             
    return ret;
}

int Si4709_dev_DSMUTE_OFF(void)
{
    int ret = 0;
    u16 powercfg = 0;

	return 0;	// JinoKang 2011.06.03	For Always DS MUTE func ON
              
    debug("Si4709_dev_DSMUTE_OFF called");
              
    mutex_lock(&(Si4709_dev.lock));   
    
    powercfg = Si4709_dev.registers[POWERCFG];
    
    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_DSMUTE_OFF called when DS is invalid");
         ret = -1;   
    }
    else
    {
         POWERCFG_BITSET_DSMUTE_HIGH(&Si4709_dev.registers[POWERCFG]);
         POWERCFG_BITSET_RESERVED(&Si4709_dev.registers[POWERCFG]);
 
         if( (ret = i2c_write( POWERCFG )) < 0 )
         {
             error("Si4709_dev_DSMUTE_OFF i2c_write failed");
             Si4709_dev.registers[POWERCFG] = powercfg;
         }
    }                   
               
    mutex_unlock(&(Si4709_dev.lock)); 
              
    return ret;
}
/*VNVS:END*/

int Si4709_dev_MUTE_ON(void)
{
    int ret = 0;
    u16 powercfg = 0;
              
    debug("Si4709_dev_MUTE_ON called");
             
    mutex_lock(&(Si4709_dev.lock));   
    powercfg = Si4709_dev.registers[POWERCFG];
    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_MUTE_ON called when DS is invalid");
         ret = -1;   
    }
    else
    {
 		McDrv_Ctrl_fm(wm1811_get_snd_soc_codec(), 0);

		wm1811_Set_FM_Mute_Switch_Flag(1);

		//FM audio path close when power down, sejong.
		//wm1811_Set_FM_Radio_Off(wm1811_get_snd_soc_codec());

         POWERCFG_BITSET_DMUTE_LOW(&Si4709_dev.registers[POWERCFG]);
         POWERCFG_BITSET_RESERVED(&Si4709_dev.registers[POWERCFG]);

         if( (ret = i2c_write( POWERCFG )) < 0 )
         {
             debug("Si4709_dev_MUTE_ON i2c_write failed");
             Si4709_dev.registers[POWERCFG] = powercfg;
         }
    }                   
              
    mutex_unlock(&(Si4709_dev.lock)); 
             
    return ret;
}
EXPORT_SYMBOL(Si4709_dev_MUTE_ON);

int Si4709_dev_MUTE_OFF(void)
{
    int ret = 0;
    u16 powercfg = 0;
              
    debug("Si4709_dev_MUTE_OFF called");
              
    mutex_lock(&(Si4709_dev.lock));   
    
    powercfg = Si4709_dev.registers[POWERCFG];
    
    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_MUTE_OFF called when DS is invalid");
         ret = -1;   
    }
    else
    {
         POWERCFG_BITSET_DMUTE_HIGH(&Si4709_dev.registers[POWERCFG]);
         POWERCFG_BITSET_RESERVED(&Si4709_dev.registers[POWERCFG]);
 
         if( (ret = i2c_write( POWERCFG )) < 0 )
         {
             debug("Si4709_dev_MUTE_OFF i2c_write failed");
             Si4709_dev.registers[POWERCFG] = powercfg;
         }
		wm1811_Set_FM_Mute_Switch_Flag(0);
 		McDrv_Ctrl_fm(wm1811_get_snd_soc_codec(), FM_setVolume);
    }                   
               
    mutex_unlock(&(Si4709_dev.lock)); 
              
    return ret;
}
EXPORT_SYMBOL(Si4709_dev_MUTE_OFF);

int Si4709_dev_MONO_SET(void)
{
    int ret = 0;
    u16 powercfg = 0;
               
    debug("Si4709_dev_MONO_SET called");
               
    mutex_lock(&(Si4709_dev.lock)); 
    
    powercfg = Si4709_dev.registers[POWERCFG];
    
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_MONO_SET called when DS is invalid");
        ret = -1;   
    }
    else
    {
        POWERCFG_BITSET_MONO_HIGH(&Si4709_dev.registers[POWERCFG]);
        POWERCFG_BITSET_RESERVED(&Si4709_dev.registers[POWERCFG]);
 
        if( (ret = i2c_write( POWERCFG )) < 0 )
        {
            debug("Si4709_dev_MONO_SET i2c_write failed");
            Si4709_dev.registers[POWERCFG] = powercfg;
        }
    }                   
                
    mutex_unlock(&(Si4709_dev.lock)); 
              
    return ret;
}

int Si4709_dev_STEREO_SET(void)
{
    int ret = 0;
    u16 powercfg = 0;
                
    debug("Si4709_dev_STEREO_SET called");
                
    mutex_lock(&(Si4709_dev.lock));   
    
    powercfg = Si4709_dev.registers[POWERCFG];
    
    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_STEREO_SET called when DS is invalid");
        ret = -1;   
    }
    else
    {
        POWERCFG_BITSET_MONO_LOW(&Si4709_dev.registers[POWERCFG]);
        POWERCFG_BITSET_RESERVED(&Si4709_dev.registers[POWERCFG]);
  
        if( (ret = i2c_write( POWERCFG )) < 0 )
        {
            debug("Si4709_dev_STEREO_SET i2c_write failed");
            Si4709_dev.registers[POWERCFG] = powercfg; 
        }
    }                   
                
    mutex_unlock(&(Si4709_dev.lock)); 
               
    return ret;
}

int Si4709_dev_RDS_ENABLE(void)
{
    u16 sysconfig1 = 0;
    int ret = 0;
             
    debug("Si4709_dev_RDS_ENABLE called");
            
    mutex_lock(&(Si4709_dev.lock));   
    
    sysconfig1 = Si4709_dev.registers[SYSCONFIG1];
    
    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_RDS_ENABLE called when DS is invalid");
         ret = -1;   
    }
    else
    {
		SYSCONFIG1_BITSET_RDS_HIGH(&Si4709_dev.registers[SYSCONFIG1]);
#ifdef RDS_INTERRUPT_ON_ALWAYS
		SYSCONFIG1_BITSET_RDSIEN_HIGH(&Si4709_dev.registers[SYSCONFIG1]);   
#endif
		SYSCONFIG1_BITSET_RESERVED( &Si4709_dev.registers[SYSCONFIG1] );	
		if( (ret = i2c_write(SYSCONFIG1)) < 0 )
		{
			debug("Si4709_dev_RDS_ENABLE i2c_write failed");
			Si4709_dev.registers[SYSCONFIG1] = sysconfig1;
		}
#ifdef RDS_INTERRUPT_ON_ALWAYS
		else		
			Si4709_RDS_flag = RDS_WAITING;
#endif
   	} 	   

    mutex_unlock(&(Si4709_dev.lock)); 
   
    return ret;
}



int Si4709_dev_RDS_DISABLE(void)
{
    u16 sysconfig1 = 0;
    int ret = 0;
             
    debug("Si4709_dev_RDS_DISABLE called");
            
    mutex_lock(&(Si4709_dev.lock));   
    
    sysconfig1 = Si4709_dev.registers[SYSCONFIG1];
    
    if( Si4709_dev.valid == eFALSE )
    {
         debug("Si4709_dev_RDS_DISABLE called when DS is invalid");
         ret = -1;   
    }
    else
    {
		SYSCONFIG1_BITSET_RDS_LOW(&Si4709_dev.registers[SYSCONFIG1]);
#ifdef RDS_INTERRUPT_ON_ALWAYS
		SYSCONFIG1_BITSET_RDSIEN_LOW(&Si4709_dev.registers[SYSCONFIG1]); 
#endif
		SYSCONFIG1_BITSET_RESERVED( &Si4709_dev.registers[SYSCONFIG1] );
		if( (ret = i2c_write(SYSCONFIG1)) < 0 )
		{
			debug("Si4709_dev_RDS_DISABLE i2c_write failed");
			Si4709_dev.registers[SYSCONFIG1] = sysconfig1;
		}
#ifdef RDS_INTERRUPT_ON_ALWAYS
		else
			Si4709_RDS_flag = NO_WAIT;
#endif
   	} 	   

    mutex_unlock(&(Si4709_dev.lock)); 

    return ret;
}
     

int Si4709_dev_rstate_get(dev_state_t *dev_state)
{
    int ret = 0;
                
    debug("Si4709_dev_rstate_get called");
                
    mutex_lock(&(Si4709_dev.lock));   

    if( Si4709_dev.valid == eFALSE )
    {
        debug("Si4709_dev_rstate_get called when DS is invalid");
        ret = -1;   
    }
    else
    {
        dev_state->power_state = Si4709_dev.state.power_state;
        dev_state->seek_state = Si4709_dev.state.seek_state;
    }             

    mutex_unlock(&(Si4709_dev.lock)); 
                
    return ret;
}


/*VNVS:START 7-JUNE'10 Function call for work-queue "Si4709_wq"*/
#ifdef RDS_INTERRUPT_ON_ALWAYS  
void Si4709_work_func(struct work_struct *work)
{	
	int i,ret = 0;		
#ifdef RDS_TESTING
	u8 group_type;
#endif
	debug_rds("%s",__func__);
//	mutex_lock(&(Si4709_dev.lock)); 
	
	if( Si4709_dev.valid == eFALSE )
    {
        error("Si4709_dev_RDS_data_get called when DS is invalid");
        ret = -1;   
    }
	else
	{

		if(RDS_Data_Lost > 1)
			debug_rds("No_of_RDS_groups_Lost till now : %d",RDS_Data_Lost);
			
		
		/*RDSR bit and RDS Block data, so reading the RDS registers*/	
		if((ret = i2c_read(RDSD)) < 0)	
			error("Si4709_work_func i2c_read failed");	
		else 
		{
			/*Checking whether RDS Ready bit is set or not, if not set return immediately*/
			if(!(STATUSRSSI_RDS_READY_STATUS(Si4709_dev.registers[STATUSRSSI])))
			{
				error("RDS Ready Bit Not set");
				return;
			}  
	
			debug_rds("RDS Ready bit is set");
			
			debug_rds("No_of_RDS_groups_Available : %d",RDS_Data_Available);
			
			RDS_Data_Available = 0;
		
			debug_rds("RDS_Buffer_Index_write = %d",RDS_Buffer_Index_write);
		
			/*Writing into the Circular Buffer*/
		
			/*Writing into RDS_Block_Data_buffer*/
			i = 0;
			RDS_Block_Data_buffer[i++ + 4*RDS_Buffer_Index_write] = Si4709_dev.registers[RDSA];
			RDS_Block_Data_buffer[i++ + 4*RDS_Buffer_Index_write] = Si4709_dev.registers[RDSB];
			RDS_Block_Data_buffer[i++ + 4*RDS_Buffer_Index_write] = Si4709_dev.registers[RDSC];
			RDS_Block_Data_buffer[i++ + 4*RDS_Buffer_Index_write] = Si4709_dev.registers[RDSD];

			/*Writing into RDS_Block_Error_buffer*/
			i = 0;
			RDS_Block_Error_buffer[i++ + 4*RDS_Buffer_Index_write] = STATUSRSSI_RDS_BLOCK_A_ERRORS(Si4709_dev.registers[STATUSRSSI]);
			RDS_Block_Error_buffer[i++ + 4*RDS_Buffer_Index_write] = READCHAN_BLOCK_B_ERRORS(Si4709_dev.registers[READCHAN]);
			RDS_Block_Error_buffer[i++ + 4*RDS_Buffer_Index_write] = READCHAN_BLOCK_C_ERRORS(Si4709_dev.registers[READCHAN]);
			RDS_Block_Error_buffer[i++ + 4*RDS_Buffer_Index_write] = READCHAN_BLOCK_D_ERRORS(Si4709_dev.registers[READCHAN]); 

#ifdef RDS_TESTING			
			if(RDS_Block_Error_buffer[0 + 4*RDS_Buffer_Index_write] < 3)
			{
				debug_rds("PI Code is %d",RDS_Block_Data_buffer[0 + 4*RDS_Buffer_Index_write]);
			}			
			if(RDS_Block_Error_buffer[1 + 4*RDS_Buffer_Index_write] < 2)
			{
				group_type = RDS_Block_Data_buffer[1 + 4*RDS_Buffer_Index_write] >> 11; 
					
				if (group_type & 0x01)
				{
					debug_rds("PI Code is %d",RDS_Block_Data_buffer[2 + 4*RDS_Buffer_Index_write]);
				}
				if(group_type == GROUP_TYPE_2A || group_type == GROUP_TYPE_2B )
				{
					if(RDS_Block_Error_buffer[2 + 4*RDS_Buffer_Index_write] < 3)
					{
						debug_rds("Update RT with RDSC");								
					}
					else
					{
						debug_rds("RDS_Block_Error_buffer of Block C is greater than 3");			
					}
				}
			}
#endif		
			RDS_Buffer_Index_write++;
		
			if(RDS_Buffer_Index_write >= RDS_BUFFER_LENGTH)
				RDS_Buffer_Index_write = 0;	
			
			debug_rds("RDS_Buffer_Index_write = %d",RDS_Buffer_Index_write);
		}
	}
	
//	mutex_unlock(&(Si4709_dev.lock));
}
#endif
/*VNVS:END*/

int Si4709_dev_RDS_data_get(radio_data_t *data)
{
	int i,ret = 0;
	u16 sysconfig1 = 0;

	debug_rds("Si4709_dev_RDS_data_get called");

	mutex_lock(&(Si4709_dev.lock));  
    
	sysconfig1 = Si4709_dev.registers[SYSCONFIG1];
    
	if( Si4709_dev.valid == eFALSE )
	{
		error("Si4709_dev_RDS_data_get called when DS is invalid");
		ret = -1;
	}
	else
	{
#ifdef RDS_INTERRUPT_ON_ALWAYS

		debug_rds("RDS_Buffer_Index_read = %d",RDS_Buffer_Index_read);
		
		/*If No New RDS Data is available return error*/
		if(RDS_Buffer_Index_read == RDS_Buffer_Index_write)
		{	
			debug_rds("No_New_RDS_Data_is_available");					
			if((ret = i2c_read(READCHAN)) < 0)	
				error("Si4709_dev_RDS_data_get i2c_read 1 failed");
			else
			{
				get_cur_chan_freq(&(data->curr_channel), Si4709_dev.registers[READCHAN]);			
				data->curr_rssi = STATUSRSSI_RSSI_SIGNAL_STRENGTH(Si4709_dev.registers[STATUSRSSI]);
				debug_rds("curr_channel: %u, curr_rssi:%u",data->curr_channel,(u32)data->curr_rssi);
			}
			ret = -1;
		}
		else
		{
			if((ret = i2c_read(READCHAN)) < 0)	
				error("Si4709_dev_RDS_data_get i2c_read 2 failed");
			else
			{
				get_cur_chan_freq(&(data->curr_channel), Si4709_dev.registers[READCHAN]);			
				data->curr_rssi = STATUSRSSI_RSSI_SIGNAL_STRENGTH(Si4709_dev.registers[STATUSRSSI]);
				debug_rds("curr_channel: %u, curr_rssi:%u",data->curr_channel,(u32)data->curr_rssi);
						
				/*Reading from RDS_Block_Data_buffer*/
				i = 0; 
				data->rdsa = RDS_Block_Data_buffer[i++ + 4*RDS_Buffer_Index_read];
				data->rdsb = RDS_Block_Data_buffer[i++ + 4*RDS_Buffer_Index_read];
				data->rdsc = RDS_Block_Data_buffer[i++ + 4*RDS_Buffer_Index_read];
				data->rdsd = RDS_Block_Data_buffer[i++ + 4*RDS_Buffer_Index_read];

				/*Reading from RDS_Block_Error_buffer*/
				i = 0;
				data->blera = RDS_Block_Error_buffer[i++ + 4*RDS_Buffer_Index_read];
				data->blerb = RDS_Block_Error_buffer[i++ + 4*RDS_Buffer_Index_read];
				data->blerc = RDS_Block_Error_buffer[i++ + 4*RDS_Buffer_Index_read];
				data->blerd = RDS_Block_Error_buffer[i++ + 4*RDS_Buffer_Index_read];  

				/*Flushing the read data*/
				memset(&RDS_Block_Data_buffer[0+4*RDS_Buffer_Index_read],0,8); 
				memset(&RDS_Block_Error_buffer[0+4*RDS_Buffer_Index_read],0,4);
			
				RDS_Buffer_Index_read++;
		
				if(RDS_Buffer_Index_read >= RDS_BUFFER_LENGTH)
					RDS_Buffer_Index_read = 0;
			}
		}
				
		debug_rds("RDS_Buffer_Index_read = %d",RDS_Buffer_Index_read);
		
#else 
		SYSCONFIG1_BITSET_RDSIEN_HIGH(&Si4709_dev.registers[SYSCONFIG1]);

		if((ret = i2c_write(SYSCONFIG1)) < 0)
		{
			error("Si4709_dev_RDS_data_get i2c_write 1 failed");
			Si4709_dev.registers[SYSCONFIG1] = sysconfig1;
		}
		else	
		{
			if( (ret=i2c_read(SYSCONFIG1)) < 0)
				error("Si4709_dev_RDS_data_get i2c_read 1 failed");

			debug("sysconfig1: 0x%x",Si4709_dev.registers[SYSCONFIG1] );
			sysconfig1 = Si4709_dev.registers[SYSCONFIG1];

			Si4709_dev_wait_flag = RDS_WAITING;
        
			wait_RDS();

			if((ret=i2c_read(STATUSRSSI)) < 0)
				error("Si4709_dev_RDS_data_get i2c_read 2 failed");

			debug("statusrssi: 0x%x",Si4709_dev.registers[STATUSRSSI] );
			SYSCONFIG1_BITSET_RDSIEN_LOW(&Si4709_dev.registers[SYSCONFIG1]);

			if ((ret = i2c_write(SYSCONFIG1)) < 0)
			{
				error("Si4709_dev_RDS_data_get i2c_write 2 failed");
				Si4709_dev.registers[SYSCONFIG1] = sysconfig1;
			}
            else if(Si4709_dev_wait_flag == WAIT_OVER)
			{
				Si4709_dev_wait_flag = NO_WAIT;

				if((ret = i2c_read(RDSD)) < 0)
					error("Si4709_dev_RDS_data_get i2c_read 3 failed");
				else 
				{
					data->rdsa = Si4709_dev.registers[RDSA];
					data->rdsb = Si4709_dev.registers[RDSB];
					data->rdsc = Si4709_dev.registers[RDSC];
					data->rdsd = Si4709_dev.registers[RDSD];

					get_cur_chan_freq(&(data->curr_channel), Si4709_dev.registers[READCHAN]);
					debug("curr_channel: %u",data->curr_channel);
					data->curr_rssi = STATUSRSSI_RSSI_SIGNAL_STRENGTH(Si4709_dev.registers[STATUSRSSI]);
					debug("curr_rssi:%u",(u32)data->curr_rssi);
					data->blera = STATUSRSSI_RDS_BLOCK_A_ERRORS(Si4709_dev.registers[STATUSRSSI]);
					data->blerb = READCHAN_BLOCK_B_ERRORS(Si4709_dev.registers[READCHAN]);
					data->blerc = READCHAN_BLOCK_C_ERRORS(Si4709_dev.registers[READCHAN]);
					data->blerd = READCHAN_BLOCK_D_ERRORS(Si4709_dev.registers[READCHAN]);
				}
			}
            else
			{
				debug("Si4709_dev_RDS_data_get failure no interrupt or timeout");
				Si4709_dev_wait_flag = NO_WAIT;
				ret = -1;
			}
        } 
#endif
	}

	mutex_unlock(&(Si4709_dev.lock)); 
                
	return ret;
}

int Si4709_dev_RDS_timeout_set(u32 time_out)
{
    int ret = 0; 
    u32 jiffy_count = 0;
    
    debug("Si4709_dev_RDS_timeout_set called");
    /****convert time_out(in milliseconds) into jiffies*****/
    
    jiffy_count = msecs_to_jiffies(time_out);

    debug("jiffy_count%d",jiffy_count);

    mutex_lock(&(Si4709_dev.lock));   
     
    if( Si4709_dev.valid == eFALSE )
    {
        error("Si4709_dev_RDS_timeout_set called when DS is invalid");
        ret = -1;   
    }
    else
    {
        Si4709_dev.settings.timeout_RDS = jiffy_count;
    }

    mutex_unlock(&(Si4709_dev.lock));  

    return ret;
}
    
int Si4709_dev_mute_get(int *isMute)	// JinoKang 2011.06.03
{
	int ret = 0;

	debug("Si4709_dev_mute_get called");
			
	mutex_lock(&(Si4709_dev.lock));   

	if( Si4709_dev.valid == eFALSE )
	{
		 debug("Si4709_dev_mute_get called when DS is invalid");
		 ret = -1;	 
	}
	else
	{
		 *isMute = POWER_CONFIG_MUTE_STATUS(Si4709_dev.registers[POWERCFG]);
	}			 
			 
	mutex_unlock(&(Si4709_dev.lock)); 
			
	return ret;
}

#ifdef FMRADIO_TSP_DELAY_CONTROL
int Si4709_dev_tsp_speed_set(unsigned int tsp_speed)	/* Kyung Hyun Kim 2011.09.30 */
{
	int ret = 0;

	debug("Si4709_dev_tsp_speed_set called");
			
	mutex_lock(&(Si4709_dev.lock));   

	if( Si4709_dev.valid == eFALSE )
	{
		 debug("Si4709_dev_tsp_speed_set called when DS is invalid");
		 ret = -1;	 
	}
	else
	{
		debug("Si4709_dev_tsp_speed_set: TSP speed = %d", tsp_speed);
		//zinitix_change_scan_delay(tsp_speed);
	}			 
			 
	mutex_unlock(&(Si4709_dev.lock)); 
			
	return ret;
}
#endif

/**************************************************************/
static int powerup(void)
{
	int ret=0;
	u16 powercfg = Si4709_dev.registers[POWERCFG];
	int reg;
	 /****Resetting the device****/

	gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_LOW);
	mdelay(10);	
	gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_HIGH);
	mdelay(30);
								
#if 0
    /*Add the i2c driver*/
    if ( (ret = Si4709_i2c_drv_init() < 0) ) 
    {
         debug("Si4709_i2c_drv_init failed");
    }
#endif
	/*Resetting the Si4709_dev.registers[] array*/
	for(reg=0;reg < NUM_OF_REGISTERS;reg++)
		Si4709_dev.registers[reg] = 0;
	
	debug("Resetting the Si4709_dev.registers[] array");

	POWERCFG_BITSET_DMUTE_HIGH( &Si4709_dev.registers[POWERCFG] );
	POWERCFG_BITSET_ENABLE_HIGH( &Si4709_dev.registers[POWERCFG] );
	POWERCFG_BITSET_DISABLE_LOW( &Si4709_dev.registers[POWERCFG] );
	POWERCFG_BITSET_RESERVED( &Si4709_dev.registers[POWERCFG] );
 
	if( (ret = i2c_write(POWERCFG)) < 0 )
	{
		error("powerup->i2c_write 1 failed");
		Si4709_dev.registers[POWERCFG] = powercfg;
	}
	else
	{
		/*Si4709/09 datasheet: Table 7*/
		mdelay(110);
		Si4709_dev.state.power_state = RADIO_ON;

		/* volume down wakeup enable */
		omap_writel(omap_readl(ADDRESS_GPIO2_VOLDN) | (OMAP_WAKEUP_EN << 16), ADDRESS_GPIO2_VOLDN);
#if defined( CONFIG_MACH_SAMSUNG_AALTO )
		enable_irq_wake(gpio_to_irq(OMAP_GPIO_KEY_AALTO_VOLDN));
#elif defined( CONFIG_MACH_SAMSUNG_HUGO )
		enable_irq_wake(gpio_to_irq(OMAP_GPIO_KEY_HUGO_VOLDN));
#endif

		/* volume up wakeup enable */
		omap_writel(omap_readl(ADDRESS_GPIO2_VOLUP) | OMAP_WAKEUP_EN, ADDRESS_GPIO2_VOLUP);
#if defined( CONFIG_MACH_SAMSUNG_AALTO )
		enable_irq_wake(gpio_to_irq(OMAP_GPIO_KEY_AALTO_VOLUP));
#elif defined( CONFIG_MACH_SAMSUNG_HUGO )
		enable_irq_wake(gpio_to_irq(OMAP_GPIO_KEY_HUGO_VOLUP));
#endif
	}
	//i2c_read(BOOTCONFIG);
	return ret;
}

static int powerdown(void)
{
    int ret = 0;
    u16 test1 = Si4709_dev.registers[TEST1], 
    sysconfig1 = Si4709_dev.registers[SYSCONFIG1],
    powercfg = Si4709_dev.registers[POWERCFG];

	if(!(RADIO_POWERDOWN==Si4709_dev.state.power_state))
	{
		//TEST1_BITSET_AHIZEN_HIGH( &Si4709_dev.registers[TEST1] );
		//TEST1_BITSET_RESERVED( &Si4709_dev.registers[TEST1] );

#ifdef FMRADIO_TSP_DELAY_CONTROL
		debug("Si4709_dev_tsp_speed_set: TSP speed = 0");
		//zinitix_change_scan_delay(0);
#endif
 
		SYSCONFIG1_BITSET_GPIO_LOW(&Si4709_dev.registers[SYSCONFIG1]);
		SYSCONFIG1_BITSET_RESERVED( &Si4709_dev.registers[SYSCONFIG1] );
		/*VNVS: 13-OCT'09---- During Powerdown of the device RDS should be disabled 
							according to the Si4708/09 datasheet*/
		SYSCONFIG1_BITSET_RDS_LOW(&Si4709_dev.registers[SYSCONFIG1]);
 
		POWERCFG_BITSET_DMUTE_LOW( &Si4709_dev.registers[POWERCFG] );
		POWERCFG_BITSET_ENABLE_HIGH( &Si4709_dev.registers[POWERCFG] );
		POWERCFG_BITSET_DISABLE_HIGH( &Si4709_dev.registers[POWERCFG] );
		POWERCFG_BITSET_RESERVED( &Si4709_dev.registers[POWERCFG] );

		/*this will write all the above registers*/
		if( (ret = i2c_write( TEST1 )) < 0 )
		{
			error("powerdown->i2c_write failed");
			Si4709_dev.registers[SYSCONFIG1] = sysconfig1;
			Si4709_dev.registers[POWERCFG] = powercfg;
			Si4709_dev.registers[TEST1] = test1;
		}
		else
		{
			Si4709_dev.state.power_state = RADIO_POWERDOWN;
		}

		 /****Resetting the device****/	
		gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_LOW);
		gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_HIGH);
		gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_LOW);
		Si4709_dev.state.power_state = RADIO_POWERDOWN;	// Jinokang 2011.07.30

		/* volume down wakeup disable */
#if defined( CONFIG_MACH_SAMSUNG_AALTO )
		disable_irq_wake(gpio_to_irq(OMAP_GPIO_KEY_AALTO_VOLDN));
#elif defined( CONFIG_MACH_SAMSUNG_HUGO )
		disable_irq_wake(gpio_to_irq(OMAP_GPIO_KEY_HUGO_VOLDN));
#endif
		omap_writel(omap_readl(ADDRESS_GPIO2_VOLDN) & ~(OMAP_WAKEUP_EN<<16), ADDRESS_GPIO2_VOLDN);

		/* volume up wakeup disable */
#if defined( CONFIG_MACH_SAMSUNG_AALTO )
		disable_irq_wake(gpio_to_irq(OMAP_GPIO_KEY_AALTO_VOLUP));
#elif defined( CONFIG_MACH_SAMSUNG_HUGO )
		disable_irq_wake(gpio_to_irq(OMAP_GPIO_KEY_HUGO_VOLUP));
#endif
	
		omap_writel(omap_readl(ADDRESS_GPIO2_VOLUP) & ~(OMAP_WAKEUP_EN<<16), ADDRESS_GPIO2_VOLUP);
	}
	else
		debug("Device already Powered-OFF");

    return ret;
}

static int seek(u32 *frequency, int up)
{
    int ret = 0;
    u16 powercfg = Si4709_dev.registers[POWERCFG];
    u16 channel = 0;
    int valid_station_found = 0;
 
    if( up ) 
    {
        POWERCFG_BITSET_SEEKUP_HIGH(&Si4709_dev.registers[POWERCFG]);
    }
    else
    {
        POWERCFG_BITSET_SEEKUP_LOW(&Si4709_dev.registers[POWERCFG]);
    }

    POWERCFG_BITSET_SKMODE_HIGH(&Si4709_dev.registers[POWERCFG]);
    POWERCFG_BITSET_SEEK_HIGH(&Si4709_dev.registers[POWERCFG]);
    POWERCFG_BITSET_RESERVED(&Si4709_dev.registers[POWERCFG]);

    if( (ret = i2c_write(POWERCFG)) < 0 )
    {	
        error("seek i2c_write 1 failed");
        Si4709_dev.registers[POWERCFG] = powercfg;
    }
    else
    {
        Si4709_dev_wait_flag = SEEK_WAITING;

        wait();
		
		if ( Si4709_dev_wait_flag == SEEK_CANCEL ) 
		{
            powercfg = Si4709_dev.registers[POWERCFG];
            POWERCFG_BITSET_SEEK_LOW(&Si4709_dev.registers[POWERCFG]);
            POWERCFG_BITSET_RESERVED(&Si4709_dev.registers[POWERCFG]);
 
            if( (ret = i2c_write(POWERCFG)) < 0 )
            {
                error("seek i2c_write 2 failed");
                Si4709_dev.registers[POWERCFG] = powercfg;
            }

			if( (ret = i2c_read( READCHAN )) < 0 )
			{
				error("seek i2c_read 1 failed");
			}
			else
			{
				channel = READCHAN_GET_CHAN(Si4709_dev.registers[READCHAN]);
				*frequency = channel_to_freq(channel);
			}
			*frequency = 0;
		}

        Si4709_dev_wait_flag = NO_WAIT;
		            
        if( (ret = i2c_read(STATUSRSSI)) < 0 )
        {
            error("seek i2c_read 2 failed");
        }
        else
        {
			/*VNVS:START 13-OCT'09---- Checking the status of Seek/Tune Bit*/
#ifdef TEST_FM
			if(STATUSRSSI_SEEK_TUNE_STATUS(Si4709_dev.registers[STATUSRSSI]) == COMPLETE)								
			{
				debug("Seek/Tune Status is set to 1 by device");
				if(STATUSRSSI_SF_BL_STATUS(Si4709_dev.registers[STATUSRSSI]) == SEEK_SUCCESSFUL)
				{
					debug("Seek Fail/Band Limit Status is set to 0 by device ---SeekUp Operation Completed");
					valid_station_found = 1;
				}
				else
					debug("Seek Fail/Band Limit Status is set to 1 by device ---SeekUp Operation Not Completed");
			}
			else
				debug("Seek/Tune Status is set to 0 by device ---SeekUp Operation Not Completed");		 			
			
#endif
			/*VNVS:END*/

            powercfg = Si4709_dev.registers[POWERCFG];
                    
            POWERCFG_BITSET_SEEK_LOW(&Si4709_dev.registers[POWERCFG]);
            POWERCFG_BITSET_RESERVED(&Si4709_dev.registers[POWERCFG]);
 
            if( (ret = i2c_write(POWERCFG)) < 0 )
            {
                error("seek i2c_write 2 failed");
                Si4709_dev.registers[POWERCFG] = powercfg;
            }
            else
            {
                do
                {
                    if( (ret = i2c_read(STATUSRSSI)) < 0 )
                    {
                        error("seek i2c_read 3 failed"); 
                        break;
                    }
                }while( STATUSRSSI_SEEK_TUNE_STATUS(Si4709_dev.registers[STATUSRSSI]) != CLEAR );

                if( ret == 0 && valid_station_found == 1 )
                {
                    if( (ret = i2c_read( READCHAN )) < 0 )
                    {
                        error("seek i2c_read 4 failed");
                    }
                    else
                    {
                        channel = READCHAN_GET_CHAN(Si4709_dev.registers[READCHAN]);
                        *frequency = channel_to_freq(channel);
						debug("Frequency after seek-up is %d \n",*frequency);					
                    }
                }
                else
                {	
					debug("Valid station not found \n");										
					*frequency = 0;
				}
            }
        }    
    }

    return ret;
}

static int tune_freq(u32 frequency)
{
    int ret = 0;
    u16 channel = Si4709_dev.registers[CHANNEL];
#ifdef TEST_FM
	u16 read_channel;														
#endif
    debug("tune_freq called");

    Si4709_dev.registers[CHANNEL] = freq_to_channel(frequency);
#ifdef TEST_FM	
	read_channel = Si4709_dev.registers[CHANNEL];									
	debug(" Input read_channel =%x",read_channel);											
#endif
    CHANNEL_BITSET_TUNE_HIGH(&Si4709_dev.registers[CHANNEL]);
    CHANNEL_BITSET_RESERVED(&Si4709_dev.registers[CHANNEL]);

    if( (ret = i2c_write(CHANNEL)) < 0 )
    {
        error("tune_freq i2c_write 1 failed");
        Si4709_dev.registers[CHANNEL] = channel;
    }
    else
    {	
			        
		Si4709_dev_wait_flag = TUNE_WAITING;
		debug("Si4709_dev_wait_flag = TUNE_WAITING");				
#ifdef TEST_FM	
		if( (ret = i2c_read(READCHAN)) < 0 )															
		{
			error("tune_freq i2c_read 1 failed");
		}
		else
		{
			read_channel=READCHAN_GET_CHAN(Si4709_dev.registers[READCHAN]);											
			debug("curr_channel before tuning = %x",read_channel);														
		}																								
#endif																								

		//i2c_read(0x04);
        wait();
		
		Si4709_dev_wait_flag = NO_WAIT;
		
		/*VNVS:START 13-OCT'09---- Checking the status of Seek/Tune Bit*/
#ifdef TEST_FM
		if( (ret = i2c_read(STATUSRSSI)) < 0 )																
        {
            error("tune_freq i2c_read 2 failed"); 
           
        }
		else if(STATUSRSSI_SEEK_TUNE_STATUS(Si4709_dev.registers[STATUSRSSI]) == COMPLETE)							
			debug("Seek/Tune Status is set to 1 by device ---Tuning Operation Completed");
		else
			debug("Seek/Tune Status is set to 0 by device ---Tuning Operation Not Completed");	
#endif		
		/*VNVS:END*/

        channel = Si4709_dev.registers[CHANNEL];
    
        CHANNEL_BITSET_TUNE_LOW(&Si4709_dev.registers[CHANNEL]);
        CHANNEL_BITSET_RESERVED(&Si4709_dev.registers[CHANNEL]);
        
        if( (ret = i2c_write(CHANNEL)) < 0 )
        {
            error("tune_freq i2c_write 2 failed");
            Si4709_dev.registers[CHANNEL] = channel;
        }
        else
        {
            do
            {
                if( (ret = i2c_read(STATUSRSSI)) < 0 )
                {
                     error("tune_freq i2c_read 3 failed"); 
                     break;
                }
            }while( STATUSRSSI_SEEK_TUNE_STATUS(Si4709_dev.registers[STATUSRSSI]) != CLEAR );  
           
        }
		
		/*VNVS:START 13-OCT'09---- Reading the READCHAN register after tuning operation*/	
#ifdef TEST_FM
		if( (ret = i2c_read(READCHAN)) < 0 )																
		{
			error("tune_freq i2c_read 2 failed");
		}
		else
		{
			read_channel=READCHAN_GET_CHAN(Si4709_dev.registers[READCHAN]);											
			debug("curr_channel after tuning= %x",read_channel);															
		}
#endif
		/*VNVS:END*/
    }

    return ret;
}

static void get_cur_chan_freq(u32 *frequency, u16 readchan)
{

    u16 channel = 0;
    //debug("get_cur_chan_freq called"); 
	
	channel = READCHAN_GET_CHAN(readchan);
	//debug("read_channel=%x",channel);											

    *frequency = channel_to_freq(channel);

    //debug("frequency-> %u",*frequency);  
}

static u16 freq_to_channel(u32 frequency)
{
    u16 channel;

    if( frequency < Si4709_dev.settings.bottom_of_band )
    {
        frequency = Si4709_dev.settings.bottom_of_band;
    }

    channel = (frequency - Si4709_dev.settings.bottom_of_band) 
    	         / Si4709_dev.settings.channel_spacing;

    return channel;
}

static u32 channel_to_freq(u16 channel)
{
    u32 frequency;

    frequency = Si4709_dev.settings.bottom_of_band +
    	           Si4709_dev.settings.channel_spacing * channel;

    return frequency;
}
extern int twl_i2c_write_u8(u8 mod_no, u8 value, u8 reg);
static void force_reset(void)
{
	int err = 0;

	debug("+++ force_reset start +++\n");

	debug("%s: RST disable\n", __func__);
	gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_LOW);
	gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_HIGH);
	gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_LOW);
	mdelay(1);

#if defined(CONFIG_FMRADIO_USE_GPIO_I2C)
	debug("%s: I2C disable\n", __func__);
	omap_gpio_i2c_deinit(Si4709_i2c_client);
	Si4709_i2c_client = NULL;
#endif

	debug("%s: Turn off VDAC for FM VIO\n", __func__);
	err = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0x00, TWL4030_VDAC_DEV_GRP);	
	if(err)
		error("TWL4030 failed to write TWL4030_VDAC_DEV_GRP register!.\n");
	mdelay(1);

	debug("%s: Turn off VMMC2 for FM VDD\n", __func__);
	err = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0x00, TWL4030_VMMC2_DEV_GRP);
	if(err)
		error("TWL4030 failed to write TWL4030_VMMC2_DEV_GRP register!.\n");
	mdelay(1);

	debug("%s: Turn on VMMC2 for FM VDD\n", __func__);
	err = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0x9, TWL4030_VMMC2_DEDICATED);
	if(err)
	{
		error("TWL4030 failed to write TWL4030_VMMC2_DEDICATED!.\n");
	}
	else
	{
		err = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0xe0, TWL4030_VMMC2_DEV_GRP);
		if(err)
			error("TWL4030 failed to write TWL4030_VMMC2_DEV_GRP register.\n");
	}
	mdelay(1);

	debug("%s: Turn on VDAC for FM VIO\n", __func__);
	err = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0x3, TWL4030_VDAC_DEDICATED);
	if(err)
	{
		error("TWL4030 failed to write TWL4030_VDAC_DEDICATED register!.\n");
	}
	else
	{
		err = twl_i2c_write_u8(TWL4030_MODULE_PM_RECEIVER, 0xe0, TWL4030_VDAC_DEV_GRP);
		if(err)
			error("TWL4030 failed to write TWL4030_VDAC_DEV_GRP register.\n");
	}
	mdelay(1);

#if defined(CONFIG_FMRADIO_USE_GPIO_I2C)
	debug("%s: I2C enable\n", __func__);
	Si4709_i2c_client = omap_gpio_i2c_init(OMAP_GPIO_FM_SDA,
						  OMAP_GPIO_FM_SCL,
						  0x10,
						  200);
#endif

	debug("%s: RST enable\n", __func__);
	gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_LOW);
	mdelay(10);	
	gpio_set_value(OMAP_GPIO_FM_nRST, GPIO_LEVEL_HIGH);
	mdelay(30);

	debug("%s: Si4709 enable\n", __func__);
	POWERCFG_BITSET_DMUTE_HIGH( &Si4709_dev.registers[POWERCFG] );
	POWERCFG_BITSET_ENABLE_HIGH( &Si4709_dev.registers[POWERCFG] );
	POWERCFG_BITSET_DISABLE_LOW( &Si4709_dev.registers[POWERCFG] );
	POWERCFG_BITSET_RESERVED( &Si4709_dev.registers[POWERCFG] );

	if(i2c_write(POWERCFG) < 0)
		error("%s: write POWERCFG fail\n", __func__);

	mdelay(110);

	debug("%s: Si4709 recovery to tune or seek\n", __func__);
	if(i2c_write(SYSCONFIG3) < 0)
		error("%s: write SYSCONFIG3 fail\n", __func__);

	if(i2c_write(SYSCONFIG1) < 0)
		error("%s: write SYSCONFIG1 fail\n", __func__); /* Si4709 recovery for RDS status. 2012.02.03. jw47.hwang */

	debug("--- force_reset end ---\n");
}

/*Only one thread will be able to call this, since this function call is 
   protected by a mutex, so no race conditions can arise*/
static void wait(void)
{
#ifdef FMRADIO_WAIT_TIMEOUT	/* workaround for FM ANR by miscatching interrupt. 2012.01.11. kh76.kim */
	u32 timeout;
	u32 ret;
	u32 check_cnt = 0;

FM_RECOVER:
	debug("fmradio : wait_event_interruptible_timeout\n");

	if(Si4709_dev_wait_flag == TUNE_WAITING)
		timeout = msecs_to_jiffies(FMRADIO_WAIT_TIME_TUNE);
	else		// Si4709_dev_wait_flag == SEEK_WAITING
		timeout = msecs_to_jiffies(FMRADIO_WAIT_TIME_SEEK);
	ret = wait_event_interruptible_timeout(Si4709_waitq, 
	    	(Si4709_dev_wait_flag == WAIT_OVER) || (Si4709_dev_wait_flag == SEEK_CANCEL), timeout);
	if(ret == 0)
	{
		error("[ERROR] WAIT TIME OUT\n");
		if(check_cnt++ == 0)
		{
			force_reset();
			goto FM_RECOVER;
		}
		Si4709_dev_wait_flag = WAIT_OVER;
	}
#else	/* old code */
	debug("fmradio : wait_event_interruptible\n");
	//mutex_unlock(&(Si4709_dev.lock)); //changoh.heo 2010.11.12
	wait_event_interruptible(Si4709_waitq, 
    		(Si4709_dev_wait_flag == WAIT_OVER) || (Si4709_dev_wait_flag == SEEK_CANCEL));
	//mutex_lock(&(Si4709_dev.lock));   //changoh.heo 2010.11.12
#endif
}

#ifndef RDS_INTERRUPT_ON_ALWAYS
static void wait_RDS(void)
{
	debug("fmradio : wait_RDS_event_interruptible\n");
	wait_event_interruptible_timeout(Si4709_waitq, 
		(Si4709_dev_wait_flag == WAIT_OVER),Si4709_dev.settings.timeout_RDS);
}
#endif

/*i2c read function*/
/*Si4709_dev.client should be set before calling this function.
   If Si4709_dev.valid = eTRUE then Si4709_dev.client will b valid
   This function should be called from the functions in this file. The 
   callers should check if Si4709_dev.valid = eTRUE before
   calling this function. If it is eFALSE then this function should not
   be called*/
static int i2c_read( u8 reg )
{
	u8 idx, reading_reg = STATUSRSSI;
	u8 data[NUM_OF_REGISTERS * 2], data_high, data_low;
	int msglen = 0, ret = 0;
#if defined(CONFIG_FMRADIO_USE_GPIO_I2C)
	OMAP_GPIO_I2C_RD_DATA i2c_rd_param;
	u8 reg_addr_0 = 0x0;
	int retry_cnt = 0;
#endif

	for(idx = 0; idx < NUM_OF_REGISTERS * 2; idx++)
	{
		data[idx] = 0x00;
	} 

	msglen = reg - reading_reg + 1;	
	  
	if(msglen > 0)
	{
		msglen = msglen * 2;
	} 
	else
	{
		msglen = (msglen + NUM_OF_REGISTERS) * 2;
	} 

#if !defined(CONFIG_FMRADIO_USE_GPIO_I2C)
	ret = i2c_master_recv(Si4709_dev.client, data, msglen);
#else
	i2c_rd_param.reg_len = 0;
	i2c_rd_param.reg_addr = &data;
	i2c_rd_param.rdata_len = msglen;
	i2c_rd_param.rdata = data;

	do {
		ret = omap_gpio_i2c_read(Si4709_i2c_client, &i2c_rd_param);
		if(ret)
			printk(KERN_ERR "[FM Radio] i2c_read failed![%d]\n", ret);
	} while(ret != 0 && retry_cnt++ < 10);

	/* this code is need for compatibility with linux i2c function */
	ret = (ret == 0)? msglen : -1;
#endif

	  if(ret == msglen) 
	  {
	  	idx = 0;
		do 
		{
			data_high	= data[idx];
			data_low	= data[idx+1];

			Si4709_dev.registers[reading_reg] = 0x0000;
			Si4709_dev.registers[reading_reg] = (data_high << 8) + data_low;

			//printk("[FM Radio] read register 0x%x, 0x%x\n", reading_reg, Si4709_dev.registers[reading_reg]);
			
			reading_reg = (reading_reg + 1) & RDSD;
			idx = idx + 2;
		} while(reading_reg != ((reg +1) & RDSD));

		ret = 0;
	  }

	  return ret;
}    

/*i2c write function*/
/*Si4709_dev.client should be set before calling this function.
   If Si4709_dev.valid = eTRUE then Si4709_dev.client will b valid
   This function should be called from the functions in this file. The 
   callers should check if Si4709_dev.valid = eTRUE before
   calling this function. If it is eFALSE then this function should not
   be called*/
   
#if defined(CONFIG_FMRADIO_USE_GPIO_I2C)
static int GP_i2c_write( unsigned char *buf, u8 len )
{
	OMAP_GPIO_I2C_WR_DATA i2c_wr_param;
	int ret = 0;
	int retry_cnt = 0;

	i2c_wr_param.reg_len = 0;
	i2c_wr_param.reg_addr = NULL;
	i2c_wr_param.wdata_len = len;
	i2c_wr_param.wdata = buf;

	/* add retry code for i2c failure */
	do {
		ret = omap_gpio_i2c_write(Si4709_i2c_client, &i2c_wr_param);
		if(ret)
			printk(KERN_ERR "[FM Radio] i2c_write failed![%d]\n", ret);
	} while(ret != 0 && retry_cnt++ < 10);

	/* this code is need for compatibility with linux i2c function */
	return (ret == 0)? len : -1;
}
#endif

static int i2c_write( u8 reg )
{
	   u8 writing_reg = POWERCFG;
	   u8 data[NUM_OF_REGISTERS * 2];
	   int i, msglen = 0, ret = 0;

	   for(i = 0; i < NUM_OF_REGISTERS * 2; i++)
	   	{
		      data[i] = 0x00;
	   	}
	   
	   do 
	   	{
		     data[msglen++] = (u8)(Si4709_dev.registers[writing_reg] >> 8);
		     data[msglen++] = (u8)(Si4709_dev.registers[writing_reg] & 0xFF);
			//printk("[FM Radio] write register 0x%x, 0x%x\n", writing_reg, Si4709_dev.registers[writing_reg]);
	      	writing_reg = (writing_reg +1) & RDSD;
	    } while(writing_reg != ((reg + 1) & RDSD));

#if !defined(CONFIG_FMRADIO_USE_GPIO_I2C)
	ret = i2c_master_send(Si4709_dev.client, ( const char *)data, msglen);
#else
	ret = GP_i2c_write( ( const char *)data, msglen);
#endif

	return (ret == msglen)? 0 : -1;
}    

static int  insert_preset(u32 frequency,u8 rssi,u8 *seek_preset_rssi)
{
    u8 i;
	  u8 min_rssi = 0xff;
   	u8 min_rssi_preset=0;
   	int ret = 0;
		   
	/* first find the minimum rssi and its location
	   this will always stop at the first location with a zero rssi */
	   
	   debug("si4709 autoseek : insert preset\n");
	
	   for (i=0; i<NUM_SEEK_PRESETS; i++) 
		 {
		     if (seek_preset_rssi[i] < min_rssi)
		     	 {
			         min_rssi = seek_preset_rssi[i];
			         min_rssi_preset = i;
		      }
	   }

	  if (rssi < min_rssi)
		 ret = -1;
	   
	/***Delete the preset with the minimum rssi, and clear the last preset
	       since it would only be a copy of the second to last preset after
	       the deletion ***/
	 for (i=min_rssi_preset; i<NUM_SEEK_PRESETS-1; i++)
		{
		     Si4709_dev.settings.seek_preset[i]= Si4709_dev.settings.seek_preset[i+1];
		     seek_preset_rssi[i] = seek_preset_rssi[i+1];
  	 }
	
	 Si4709_dev.settings.seek_preset[i] = 0;
	 seek_preset_rssi[i] = 0;

	/*** Fill the first preset with a zero for the frequency.  This will
	        always overwrite the last preset once all presets have been filled. ***/
	 for (i=min_rssi_preset; i<NUM_SEEK_PRESETS; i++)
		{
		     if(Si4709_dev.settings.seek_preset[i] == 0) 
		     	 {
			         Si4709_dev.settings.seek_preset[i]= frequency;
			         seek_preset_rssi[i] = rssi;
			         break;
		       }
	   }
	 return ret;
}	


