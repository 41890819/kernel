
#define HIMAX7097_I2C_ADDR  0x48

#define CONFIG_HIMAX7097_I2C0_GPIO_SCL     GPIO_PC(18)
#define CONFIG_HIMAX7097_I2C0_GPIO_SDA     GPIO_PC(19)

# ifndef I2C_ACTIVE
#  define I2C_ACTIVE do { } while (0)
# endif

# ifndef I2C_TRISTATE
#  define I2C_TRISTATE do { } while (0)
# endif

# ifndef I2C_READ
#  define I2C_READ gpio_get_value(CONFIG_HIMAX7097_I2C0_GPIO_SDA)
# endif

# ifndef I2C_SDA
#  define I2C_SDA(bit)							\
	do {								\
	        gpio_direction_output(CONFIG_HIMAX7097_I2C0_GPIO_SDA, bit); \
	} while (0)
# endif

# ifndef I2C_SCL
#  define I2C_SCL(bit)							\
	do {								\
		gpio_direction_output(CONFIG_HIMAX7097_I2C0_GPIO_SCL, bit); \
	} while (0)
# endif

# ifndef I2C_DELAY
#  define I2C_DELAY udelay(5)	/* 1/4 I2C clock duration */
# endif

/*-----------------------------------------------------------------------
 * Definitions
 */

#define I2C_ACK		0		/* PD_SDA level to ack a byte */
#define I2C_NOACK	1		/* PD_SDA level to noack a byte */

static void  send_start	(void);
static void  send_stop	(void);
static void  send_ack	(int);
static int   write_byte	(unsigned char byte);
static unsigned char read_byte	(int);

/*-----------------------------------------------------------------------
 * START: High -> Low on SDA while SCL is High
 */
static void send_start(void)
{
	I2C_DELAY;
	I2C_SDA(1);
	I2C_ACTIVE;
	I2C_DELAY;
	I2C_SCL(1);
	I2C_DELAY;
	I2C_SDA(0);
	I2C_DELAY;
}

/*-----------------------------------------------------------------------
 * STOP: Low -> High on SDA while SCL is High
 */
static void send_stop(void)
{
	I2C_SCL(0);
	I2C_DELAY;
	I2C_SDA(0);
	I2C_ACTIVE;
	I2C_DELAY;
	I2C_SCL(1);
	I2C_DELAY;
	I2C_SDA(1);
	I2C_DELAY;
	I2C_TRISTATE;
}

/*-----------------------------------------------------------------------
 * ack should be I2C_ACK or I2C_NOACK
 */
static void send_ack(int ack)
{
	I2C_SCL(0);
	I2C_DELAY;
	I2C_ACTIVE;
	I2C_SDA(ack);
	I2C_DELAY;
	I2C_SCL(1);
	I2C_DELAY;
	I2C_DELAY;
	I2C_SCL(0);
	I2C_DELAY;
}

/*-----------------------------------------------------------------------
 * Send 8 bits and look for an acknowledgement.
 */
static int write_byte(unsigned char data)
{

	int j;
	int nack;

	I2C_ACTIVE;
	for(j = 0; j < 8; j++) {
		I2C_SCL(0);
		I2C_DELAY;
		I2C_SDA(data & 0x80);
		I2C_DELAY;
		I2C_SCL(1);
		I2C_DELAY;
		I2C_DELAY;

		data <<= 1;
	}

	/*
	 * Look for an <ACK>(negative logic) and return it.
	 */
	I2C_SCL(0);
	I2C_DELAY;
	I2C_SDA(1);
	I2C_TRISTATE;
	I2C_DELAY;
	I2C_SCL(1);
	I2C_DELAY;
	I2C_DELAY;
	nack = I2C_READ;
	I2C_SCL(0);
	I2C_DELAY;
	I2C_ACTIVE;

	return(nack);	/* not a nack is an ack */
}

/*-----------------------------------------------------------------------
 * if ack == I2C_ACK, ACK the byte so can continue reading, else
 * send I2C_NOACK to end the read.
 */
static unsigned char read_byte(int ack)
{

	int  data;
	int  j;

	/*
	 * Read 8 bits, MSB first.
	 */
	I2C_TRISTATE;
	I2C_SDA(1);
	data = 0;
	for(j = 0; j < 8; j++) {
		I2C_SCL(0);
		I2C_DELAY;
		I2C_SCL(1);
		I2C_DELAY;
		data <<= 1;
		data |= I2C_READ;
		I2C_DELAY;
	}
	send_ack(ack);

	return(data);
}

static int  himax7097_i2c_read(unsigned char chip, uint addr, int alen, unsigned char *buffer, int len)
{
	int shift;
	send_start();
	if(alen > 0) {
		if(write_byte(chip << 1)) {	/* write cycle */
			send_stop();
			printk("i2c_read, no chip responded %02X\n", chip);
                        return 1;
		}
		shift = (alen-1) * 8;
		while(alen-- > 0) {
			if(write_byte(addr >> shift)) {
				printk("i2c_read, address not <ACK>ed\n");
				return 1;
			}
			shift -= 8;
		}

	       
		//send_stop();
		send_start();
	}
      
	write_byte((chip << 1) | 1);	/* read cycle */
	while(len-- > 0) {
		*buffer++ = read_byte(len==0);
	}
	send_stop();
	return(0);
}

/*-----------------------------------------------------------------------
 * Write bytes
 */
static int  himax7097_i2c_write(unsigned char chip, uint addr, int alen, unsigned char *buffer, int len)
{
	int shift, failures = 0;

	send_start();
	if(write_byte(chip << 1)) {	/* write cycle */
		send_stop();
		printk("i2c_write, no chip responded %02X\n", chip);
                return 1;
	}
	shift = (alen-1) * 8;
	while(alen-- > 0) {
		if(write_byte(addr >> shift)) {
			printk("i2c_write, address not <ACK>ed\n");
                        return 1;
		}
		shift -= 8;
	}

	while(len-- > 0) {
		if(write_byte(*buffer++))
			failures++;
	}
	send_stop();
	return(failures);
}

static unsigned char himax7097_read_reg(struct i2c_client *client, unsigned char reg)
{
	unsigned char data;

	if ( himax7097_i2c_read(HIMAX7097_I2C_ADDR, reg, 1, &data, 1) ) {
		dev_err(&client->dev, "read 0x%02x register failed\n", reg);
		return -1;
	} else {
		dev_err(&client->dev, "read 0x%02x register 0x%02x\n", reg, data);
		return data;
	}
}

static int himax7097_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	if ( himax7097_i2c_write(HIMAX7097_I2C_ADDR, reg, 1, &val, 1) ) {
		dev_err(&client->dev, "write fail\n");
		return -1;
	} else
		return 0;
}
