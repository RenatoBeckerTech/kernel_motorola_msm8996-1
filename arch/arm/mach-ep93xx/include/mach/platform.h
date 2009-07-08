/*
 * arch/arm/mach-ep93xx/include/mach/platform.h
 */

#ifndef __ASSEMBLY__

struct i2c_board_info;

struct ep93xx_eth_data
{
	unsigned char	dev_addr[6];
	unsigned char	phy_id;
};

void ep93xx_map_io(void);
void ep93xx_init_irq(void);
void ep93xx_init_time(unsigned long);

/* EP93xx System Controller software locked register write */
void ep93xx_syscon_swlocked_write(unsigned int val, unsigned int reg);
void ep93xx_devcfg_set_clear(unsigned int set_bits, unsigned int clear_bits);

static inline void ep93xx_devcfg_set_bits(unsigned int bits)
{
	ep93xx_devcfg_set_clear(bits, 0x00);
}

static inline void ep93xx_devcfg_clear_bits(unsigned int bits)
{
	ep93xx_devcfg_set_clear(0x00, bits);
}

void ep93xx_register_eth(struct ep93xx_eth_data *data, int copy_addr);
void ep93xx_register_i2c(struct i2c_board_info *devices, int num);

void ep93xx_init_devices(void);
extern struct sys_timer ep93xx_timer;

#endif
