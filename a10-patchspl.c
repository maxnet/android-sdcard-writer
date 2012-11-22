/*
 * A10-patchspl
 * Dumps DRAM controller settings and patches u-boot SPL
 *
 * Author: Floris Bos
 * License: GPL
 *
 * Compile with: arm-linux-gnueabihf-gcc -static -o a10-patchspl a10-patchspl.c
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <linux/fs.h>
#include <sys/ioctl.h>

#define SUNXI_DRAMC_BASE    0x01c01000
#define SUNXI_CCM_BASE      0x01C20000

typedef uint32_t u32;
typedef uint8_t u8;

/*
 * Memory header definition copied from u-boot arch/arm/include/asm/arch-sunxi/dram.h
 * (C) Copyright 2007-2012 Allwinner Technology Co., Ltd. <www.allwinnertech.com>)
 */
struct sunxi_dram_reg {
	u32 ccr;		/* 0x00 controller configuration register */
	u32 dcr;		/* 0x04 dram configuration register */
	u32 iocr;		/* 0x08 i/o configuration register */
	u32 csr;		/* 0x0c controller status register */
	u32 drr;		/* 0x10 dram refresh register */
	u32 tpr0;		/* 0x14 dram timing parameters register 0 */
	u32 tpr1;		/* 0x18 dram timing parameters register 1 */
	u32 tpr2;		/* 0x1c dram timing parameters register 2 */
	u32 gdllcr;		/* 0x20 global dll control register */
	u8 res0[0x28];
	u32 rslr0;		/* 0x4c rank system latency register */
	u32 rslr1;		/* 0x50 rank system latency register */
	u8 res1[0x8];
	u32 rdgr0;		/* 0x5c rank dqs gating register */
	u32 rdgr1;		/* 0x60 rank dqs gating register */
	u8 res2[0x34];
	u32 odtcr;		/* 0x98 odt configuration register */
	u32 dtr0;		/* 0x9c data training register 0 */
	u32 dtr1;		/* 0xa0 data training register 1 */
	u32 dtar;		/* 0xa4 data training address register */
	u32 zqcr0;		/* 0xa8 zq control register 0 */
	u32 zqcr1;		/* 0xac zq control register 1 */
	u32 zqsr;		/* 0xb0 zq status register */
	u32 idcr;		/* 0xb4 initializaton delay configure reg */
	u8 res3[0x138];
	u32 mr;			/* 0x1f0 mode register */
	u32 emr;		/* 0x1f4 extended mode register */
	u32 emr2;		/* 0x1f8 extended mode register */
	u32 emr3;		/* 0x1fc extended mode register */
	u32 dllctr;		/* 0x200 dll control register */
	u32 dllcr[5];	/* 0x204 dll control register 0(byte 0) */
	/* 0x208 dll control register 1(byte 1) */
	/* 0x20c dll control register 2(byte 2) */
	/* 0x210 dll control register 3(byte 3) */
	/* 0x214 dll control register 4(byte 4) */
	u32 dqtr0;		/* 0x218 dq timing register */
	u32 dqtr1;		/* 0x21c dq timing register */
	u32 dqtr2;		/* 0x220 dq timing register */
	u32 dqtr3;		/* 0x224 dq timing register */
	u32 dqstr;		/* 0x228 dqs timing register */
	u32 dqsbtr;		/* 0x22c dqsb timing register */
	u32 mcr;		/* 0x230 mode configure register */
	u8 res[0x8];
	u32 reg_23c;	/* 0x23c register description unknown!!! */
	u32 apr;		/* 0x240 arbiter period register */
	u32 pldtr;		/* 0x244 priority level data threshold reg */
	u8 res5[0x8];
	u32 hpcr[32];	/* 0x250 host port configure register */
	u8 res6[0x10];
	u32 csel;		/* 0x2e0 controller select register */
};

struct dram_para {
//	u32 baseaddr;
	u32 clock;
	u32 type;
	u32 rank_num;
	u32 density;
	u32 io_width;
	u32 bus_width;
	u32 cas;
	u32 zq;
	u32 odt_en;
	u32 size;
	u32 tpr0;
	u32 tpr1;
	u32 tpr2;
	u32 tpr3;
	u32 tpr4;
	u32 tpr5;
	u32 emr1;
	u32 emr2;
	u32 emr3;
};

/* Clock control header copied from include/asm/arch-sunxi/clock.h */
struct sunxi_ccm_reg {
	u32 pll1_cfg;		/* 0x00 pll1 control */
	u32 pll1_tun;		/* 0x04 pll1 tuning */
	u32 pll2_cfg;		/* 0x08 pll2 control */
	u32 pll2_tun;		/* 0x0c pll2 tuning */
	u32 pll3_cfg;		/* 0x10 pll3 control */
	u8 res0[0x4];
	u32 pll4_cfg;		/* 0x18 pll4 control */
	u8 res1[0x4];
	u32 pll5_cfg;		/* 0x20 pll5 control */
	u32 pll5_tun;		/* 0x24 pll5 tuning */
	u32 pll6_cfg;		/* 0x28 pll6 control */
	u32 pll6_tun;		/* 0x2c pll6 tuning */
	u32 pll7_cfg;		/* 0x30 pll7 control */
	u32 pll1_tun2;		/* 0x34 pll5 tuning2 */
	u8 res2[0x4];
	u32 pll5_tun2;		/* 0x3c pll5 tuning2 */
	u8 res3[0xc];
	u32 pll_lock_dbg;	/* 0x4c pll lock time debug */
	u32 osc24m_cfg;		/* 0x50 osc24m control */
	u32 cpu_ahb_apb0_cfg;	/* 0x54 cpu,ahb and apb0 divide ratio */
	u32 apb1_clk_div_cfg;	/* 0x58 apb1 clock dividor */
	u32 axi_gate;		/* 0x5c axi module clock gating */
	u32 ahb_gate0;		/* 0x60 ahb module clock gating 0 */
	u32 ahb_gate1;		/* 0x64 ahb module clock gating 1 */
	u32 apb0_gate;		/* 0x68 apb0 module clock gating */
	u32 apb1_gate;		/* 0x6c apb1 module clock gating */
	u8 res4[0x10];
	u32 nand_sclk_cfg;	/* 0x80 nand sub clock control */
	u32 ms_sclk_cfg;	/* 0x84 memory stick sub clock control */
	u32 sd0_clk_cfg;	/* 0x88 sd0 clock control */
	u32 sd1_clk_cfg;	/* 0x8c sd1 clock control */
	u32 sd2_clk_cfg;	/* 0x90 sd2 clock control */
	u32 sd3_clk_cfg;	/* 0x94 sd3 clock control */
	u32 ts_clk_cfg;		/* 0x98 transport stream clock control */
	u32 ss_clk_cfg;		/* 0x9c */
	u32 spi0_clk_cfg;	/* 0xa0 */
	u32 spi1_clk_cfg;	/* 0xa4 */
	u32 spi2_clk_cfg;	/* 0xa8 */
	u32 pata_clk_cfg;	/* 0xac */
	u32 ir0_clk_cfg;	/* 0xb0 */
	u32 ir1_clk_cfg;	/* 0xb4 */
	u32 iis_clk_cfg;	/* 0xb8 */
	u32 ac97_clk_cfg;	/* 0xbc */
	u32 spdif_clk_cfg;	/* 0xc0 */
	u32 keypad_clk_cfg;	/* 0xc4 */
	u32 sata_clk_cfg;	/* 0xc8 */
	u32 usb_clk_cfg;	/* 0xcc */
	u32 gps_clk_cfg;	/* 0xd0 */
	u32 spi3_clk_cfg;	/* 0xd4 */
	u8 res5[0x28];
	u32 dram_clk_cfg;	/* 0x100 */
	u32 be0_clk_cfg;	/* 0x104 */
	u32 be1_clk_cfg;	/* 0x108 */
	u32 fe0_clk_cfg;	/* 0x10c */
	u32 fe1_clk_cfg;	/* 0x110 */
	u32 mp_clk_cfg;		/* 0x114 */
	u32 lcd0_ch0_clk_cfg;	/* 0x118 */
	u32 lcd1_ch0_clk_cfg;	/* 0x11c */
	u32 csi_isp_clk_cfg;	/* 0x120 */
	u8 res6[0x4];
	u32 tvd_clk_reg;	/* 0x128 */
	u32 lcd0_ch1_clk_cfg;	/* 0x12c */
	u32 lcd1_ch1_clk_cfg;	/* 0x130 */
	u32 csi0_clk_cfg;	/* 0x134 */
	u32 csi1_clk_cfg;	/* 0x138 */
	u32 ve_clk_cfg;		/* 0x13c */
	u32 audio_codec_clk_cfg;	/* 0x140 */
	u32 avs_clk_cfg;	/* 0x144 */
	u32 ace_clk_cfg;	/* 0x148 */
	u32 lvds_clk_cfg;	/* 0x14c */
	u32 hdmi_clk_cfg;	/* 0x150 */
	u32 mali_clk_cfg;	/* 0x154 */
	u8 res7[0x4];
	u32 mbus_clk_cfg;	/* 0x15c */
};

/* Code to patch checksum copied from mksunxiboot.c */

/* boot head definition from sun4i boot code */
typedef struct boot_file_head
{
	u32  jump_instruction;   // one intruction jumping to real code
	u8   magic[8];           // ="eGON.BT0" or "eGON.BT1",  not C-style string.
	u32  check_sum;          // generated by PC
	u32  length;             // generated by PC
	u32  pub_head_size;      // the size of boot_file_head_t
	u8   pub_head_vsn[4];    // the version of boot_file_head_t
	u8   file_head_vsn[4];   // the version of boot0_file_head_t or boot1_file_head_t
	u8   Boot_vsn[4];        // Boot version
	u8   eGON_vsn[4];        // eGON version
	u8   platform[8];        // platform information
}boot_file_head_t;

#define BOOT0_MAGIC                     "eGON.BT0"
#define STAMP_VALUE                     0x5F0A6C39

/* check sum functon from sun4i boot code */
int gen_check_sum( void *boot_buf )
{
	boot_file_head_t  *head_p;
	u32 length;
	u32 *buf;
	u32 loop;
	u32 i;
	u32 sum;

	head_p = (boot_file_head_t *)boot_buf;
	length = head_p->length;
	if( ( length & 0x3 ) != 0 )                   // must 4-byte-aligned
		return -1;
	buf = (u32 *)boot_buf;
	head_p->check_sum = STAMP_VALUE;              // fill stamp
	loop = length >> 2;
	/* calculate the sum */
	for( i = 0, sum = 0;  i < loop;  i++ )
		sum += buf[i];

	/* write back check sum */
	head_p->check_sum = sum;

	return 0;
}



int mem_fd = -1;

volatile unsigned *map_physical_memory(uint32_t addr, size_t len)
{
    volatile unsigned *mem;

    if (mem_fd == -1 && (mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0)
    {
        perror("opening /dev/mem");
        exit(1);
    }

    mem = (volatile unsigned *) mmap(NULL, len, PROT_READ, MAP_SHARED, mem_fd, (off_t) addr);
    
    if (mem == MAP_FAILED)
    {
        perror("mmap");
        exit (1);
    }

    return mem;
}


void print_dram_para(struct dram_para p)
{
    // Print dram_para struct
    printf("dram_clk          = %d\n", p.clock);
    printf("dram_type         = %d\n", p.type);
    printf("dram_rank_num     = %d\n", p.rank_num);
    printf("dram_chip_density = %d\n", p.density);
    printf("dram_io_width     = %d\n", p.io_width);
    printf("dram_bus_width    = %d\n", p.bus_width);
    printf("dram_cas          = %d\n", p.cas);
    printf("dram_zq           = 0x%x\n", p.zq);
    printf("dram_odt_en       = %d\n", p.odt_en);
    //printf("dram_size         = %d\n", p.size);
    printf("dram_tpr0         = 0x%x\n", p.tpr0);
    printf("dram_tpr1         = 0x%x\n", p.tpr1);
    printf("dram_tpr2         = 0x%x\n", p.tpr2);
    printf("dram_tpr3         = 0x%x\n", p.tpr3);
    printf("dram_emr1         = 0x%x\n", p.emr1);
    printf("dram_emr2         = 0x%x\n", p.emr2);
    printf("dram_emr3         = 0x%x\n", p.emr3);
}


void patch_spl(const char *spl_image, const char *sd_device, struct dram_para p)
{
	/* We assume the original u-boot SPL was build for model cubieboard */
	struct dram_para old_dram_para = {
		.clock = 480,
		.type = 3,
		.rank_num = 1,
		.density = 4096,
		.io_width = 16,
		.bus_width = 32,
		.cas = 6,
		.zq = 123,
		.odt_en = 0,
		.size = 1024,
		.tpr0 = 0x30926692,
		.tpr1 = 0x1090,
		.tpr2 = 0x1a0c8,
		.tpr3 = 0,
		.tpr4 = 0,
		.tpr5 = 0,
		.emr1 = 0,
		.emr2 = 0,
		.emr3 = 0,
	};
	unsigned char spl[24*1024];
	unsigned int i;
	size_t len;
	unsigned int offset = 0;
		
	int fd = open(spl_image, O_RDONLY);
	if (fd < 0)
	{
		perror("opening SPL input file");
		exit(1);
	}
	len = read(fd, spl, sizeof(spl));
	if (len < 1)
	{
		perror("reading from SPL file");
		exit(1);
	}
	close(fd);
	
	/* Search for offset of old information */
	for (i=0; i < len-sizeof(old_dram_para); i++)
	{
		if (memcmp(spl+i, &old_dram_para, sizeof(old_dram_para)) == 0)
		{
			offset = i;
			break;
		}
	}
	if (!offset)
	{
		printf("Error: cannot locate dram_para inside SPL file. Make sure it was build for model cubieboard\n");
		exit(1);
	}
	
	printf("Patching dram_para struct at offset: %d size: %ud\n", offset, sizeof(p));
	memcpy(spl+offset, &p, sizeof(p));
	
	/* Calulate new checksum */
	gen_check_sum(spl);
	
	/* Write to SD device at offset 8kb */
	fd = open(sd_device, O_RDWR);
	if (fd < 0)
	{
		perror("opening SD card device");
		exit(1);
	}
	lseek(fd, 8192, SEEK_SET);
	
	if ( write(fd, spl, len) != len || fsync(fd) )
	{
		perror("writing to SD card device");
		exit(1);
	}
	
	/* We are done. Tell Linux it can reread the partition table, so that we can mount the SD card
	   to add script.bin afterwards */
	ioctl(fd, BLKRRPART);

	if ( close(fd) )
	{
		perror("error closing SD card device");
		exit(1);
	}
}


int main(int argc, char **argv)
{
    volatile struct sunxi_dram_reg *r  = (volatile struct sunxi_dram_reg *) map_physical_memory(SUNXI_DRAMC_BASE, 4096);
    volatile struct sunxi_ccm_reg *ccm = (volatile struct sunxi_ccm_reg *) map_physical_memory(SUNXI_CCM_BASE, 4096);
    struct dram_para p = {0};
    
    /* Convert information found inside registers back to dram_para struct */
    p.tpr0   = r->tpr0;
    p.tpr1   = r->tpr1;
    p.tpr2   = r->tpr2;
    p.emr1   = r->emr;
    p.emr2   = r->emr2;
    p.emr3   = r->emr3;
    p.type   = (r->dcr & 0x1 ? 3 : 2);
    p.odt_en = (r->iocr & 0x3);
    p.zq     = (r->zqcr0 & 0xf0000000)+(r->zqcr0 >> 20 & 0xff)+((r->zqcr0 & 0xfffff) << 8);
    p.cas    = (r->mr >> 4 & 15);
    if (p.type == 3)
        p.cas += 4;
    p.density  = 1 << 8+(r->dcr >> 3 & 7);
    p.rank_num = (r->dcr >> 10 & 3)+1;
    p.io_width = (r->dcr >> 1 & 3) << 3;
    p.bus_width = ((r->dcr >> 6 & 3)+1) << 3;
    p.clock   = (ccm->pll5_cfg >> 8 & 0x1f) * 24;
    
    if (argc == 2 && strcmp(argv[1], "-dump") == 0)
	{
		print_dram_para(p);
	}
	else if (argc == 3)
	{
		patch_spl(argv[1], argv[2], p);
	}
	else
	{
		printf("Usage: a10-patchspl <sunxi-boot.bin> <sd card device>\n");
		printf("-or- : a10-patchspl -dump\n");
	}	
	
    /* Clean up */
    munmap((void *)r, 4096);
    munmap((void *)ccm, 4096);
    close(mem_fd);
    return 0;
}

