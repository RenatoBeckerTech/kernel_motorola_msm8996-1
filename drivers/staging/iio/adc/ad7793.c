/*
 * AD7785/AD7792/AD7793/AD7794/AD7795 SPI ADC driver
 *
 * Copyright 2011-2012 Analog Devices Inc.
 *
 * Licensed under the GPL-2.
 */

#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/spi/spi.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/module.h>

#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/adc/ad_sigma_delta.h>

#include "ad7793.h"

/* NOTE:
 * The AD7792/AD7793 features a dual use data out ready DOUT/RDY output.
 * In order to avoid contentions on the SPI bus, it's therefore necessary
 * to use spi bus locking.
 *
 * The DOUT/RDY output must also be wired to an interrupt capable GPIO.
 */

struct ad7793_chip_info {
	const struct iio_chan_spec *channels;
	unsigned int num_channels;
};

struct ad7793_state {
	const struct ad7793_chip_info	*chip_info;
	struct regulator		*reg;
	u16				int_vref_mv;
	u16				mode;
	u16				conf;
	u32				scale_avail[8][2];

	struct ad_sigma_delta		sd;

};

enum ad7793_supported_device_ids {
	ID_AD7785,
	ID_AD7792,
	ID_AD7793,
	ID_AD7794,
	ID_AD7795,
};

static struct ad7793_state *ad_sigma_delta_to_ad7793(struct ad_sigma_delta *sd)
{
	return container_of(sd, struct ad7793_state, sd);
}

static int ad7793_set_channel(struct ad_sigma_delta *sd, unsigned int channel)
{
	struct ad7793_state *st = ad_sigma_delta_to_ad7793(sd);

	st->conf &= ~AD7793_CONF_CHAN_MASK;
	st->conf |= AD7793_CONF_CHAN(channel);

	return ad_sd_write_reg(&st->sd, AD7793_REG_CONF, 2, st->conf);
}

static int ad7793_set_mode(struct ad_sigma_delta *sd,
			   enum ad_sigma_delta_mode mode)
{
	struct ad7793_state *st = ad_sigma_delta_to_ad7793(sd);

	st->mode &= ~AD7793_MODE_SEL_MASK;
	st->mode |= AD7793_MODE_SEL(mode);

	return ad_sd_write_reg(&st->sd, AD7793_REG_MODE, 2, st->mode);
}

static const struct ad_sigma_delta_info ad7793_sigma_delta_info = {
	.set_channel = ad7793_set_channel,
	.set_mode = ad7793_set_mode,
	.has_registers = true,
	.addr_shift = 3,
	.read_mask = BIT(6),
};

static const struct ad_sd_calib_data ad7793_calib_arr[6] = {
	{AD7793_MODE_CAL_INT_ZERO, AD7793_CH_AIN1P_AIN1M},
	{AD7793_MODE_CAL_INT_FULL, AD7793_CH_AIN1P_AIN1M},
	{AD7793_MODE_CAL_INT_ZERO, AD7793_CH_AIN2P_AIN2M},
	{AD7793_MODE_CAL_INT_FULL, AD7793_CH_AIN2P_AIN2M},
	{AD7793_MODE_CAL_INT_ZERO, AD7793_CH_AIN3P_AIN3M},
	{AD7793_MODE_CAL_INT_FULL, AD7793_CH_AIN3P_AIN3M}
};

static int ad7793_calibrate_all(struct ad7793_state *st)
{
	return ad_sd_calibrate_all(&st->sd, ad7793_calib_arr,
				   ARRAY_SIZE(ad7793_calib_arr));
}

static int ad7793_setup(struct iio_dev *indio_dev,
	const struct ad7793_platform_data *pdata)
{
	struct ad7793_state *st = iio_priv(indio_dev);
	int i, ret = -1;
	unsigned long long scale_uv;
	u32 id;

	if ((pdata->current_source_direction == AD7793_IEXEC1_IEXEC2_IOUT1 ||
		pdata->current_source_direction == AD7793_IEXEC1_IEXEC2_IOUT2) &&
		((pdata->exitation_current != AD7793_IX_10uA) &&
		(pdata->exitation_current != AD7793_IX_210uA)))
		return -EINVAL;

	/* reset the serial interface */
	ret = spi_write(st->sd.spi, (u8 *)&ret, sizeof(ret));
	if (ret < 0)
		goto out;
	usleep_range(500, 2000); /* Wait for at least 500us */

	/* write/read test for device presence */
	ret = ad_sd_read_reg(&st->sd, AD7793_REG_ID, 1, &id);
	if (ret)
		goto out;

	id &= AD7793_ID_MASK;

	if (!((id == AD7792_ID) || (id == AD7793_ID) || (id == AD7795_ID))) {
		dev_err(&st->sd.spi->dev, "device ID query failed\n");
		goto out;
	}

	st->mode = AD7793_MODE_RATE(1);
	st->mode |= AD7793_MODE_CLKSRC(pdata->clock_src);
	st->conf = AD7793_CONF_REFSEL(pdata->refsel);
	st->conf |= AD7793_CONF_VBIAS(pdata->bias_voltage);
	if (pdata->buffered)
		st->conf |= AD7793_CONF_BUF;
	if (pdata->boost_enable)
		st->conf |= AD7793_CONF_BOOST;
	if (pdata->burnout_current)
		st->conf |= AD7793_CONF_BO_EN;
	if (pdata->unipolar)
		st->conf |= AD7793_CONF_UNIPOLAR;

	ret = ad7793_set_mode(&st->sd, AD_SD_MODE_IDLE);
	if (ret)
		goto out;

	ret = ad7793_set_channel(&st->sd, 0);
	if (ret)
		goto out;

	ret = ad_sd_write_reg(&st->sd, AD7793_REG_IO, 1,
				   pdata->exitation_current |
			       (pdata->current_source_direction << 2));
	if (ret)
		goto out;

	ret = ad7793_calibrate_all(st);
	if (ret)
		goto out;

	/* Populate available ADC input ranges */
	for (i = 0; i < ARRAY_SIZE(st->scale_avail); i++) {
		scale_uv = ((u64)st->int_vref_mv * 100000000)
			>> (st->chip_info->channels[0].scan_type.realbits -
			(!!(st->conf & AD7793_CONF_UNIPOLAR) ? 0 : 1));
		scale_uv >>= i;

		st->scale_avail[i][1] = do_div(scale_uv, 100000000) * 10;
		st->scale_avail[i][0] = scale_uv;
	}

	return 0;
out:
	dev_err(&st->sd.spi->dev, "setup failed\n");
	return ret;
}

static const u16 sample_freq_avail[16] = {0, 470, 242, 123, 62, 50, 39, 33, 19,
					  17, 16, 12, 10, 8, 6, 4};

static ssize_t ad7793_read_frequency(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ad7793_state *st = iio_priv(indio_dev);

	return sprintf(buf, "%d\n",
		       sample_freq_avail[AD7793_MODE_RATE(st->mode)]);
}

static ssize_t ad7793_write_frequency(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t len)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ad7793_state *st = iio_priv(indio_dev);
	long lval;
	int i, ret;

	mutex_lock(&indio_dev->mlock);
	if (iio_buffer_enabled(indio_dev)) {
		mutex_unlock(&indio_dev->mlock);
		return -EBUSY;
	}
	mutex_unlock(&indio_dev->mlock);

	ret = kstrtol(buf, 10, &lval);
	if (ret)
		return ret;

	ret = -EINVAL;

	for (i = 0; i < ARRAY_SIZE(sample_freq_avail); i++)
		if (lval == sample_freq_avail[i]) {
			mutex_lock(&indio_dev->mlock);
			st->mode &= ~AD7793_MODE_RATE(-1);
			st->mode |= AD7793_MODE_RATE(i);
			ad_sd_write_reg(&st->sd, AD7793_REG_MODE,
					 sizeof(st->mode), st->mode);
			mutex_unlock(&indio_dev->mlock);
			ret = 0;
		}

	return ret ? ret : len;
}

static IIO_DEV_ATTR_SAMP_FREQ(S_IWUSR | S_IRUGO,
		ad7793_read_frequency,
		ad7793_write_frequency);

static IIO_CONST_ATTR_SAMP_FREQ_AVAIL(
	"470 242 123 62 50 39 33 19 17 16 12 10 8 6 4");

static ssize_t ad7793_show_scale_available(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct iio_dev *indio_dev = dev_to_iio_dev(dev);
	struct ad7793_state *st = iio_priv(indio_dev);
	int i, len = 0;

	for (i = 0; i < ARRAY_SIZE(st->scale_avail); i++)
		len += sprintf(buf + len, "%d.%09u ", st->scale_avail[i][0],
			       st->scale_avail[i][1]);

	len += sprintf(buf + len, "\n");

	return len;
}

static IIO_DEVICE_ATTR_NAMED(in_m_in_scale_available,
		in_voltage-voltage_scale_available, S_IRUGO,
		ad7793_show_scale_available, NULL, 0);

static struct attribute *ad7793_attributes[] = {
	&iio_dev_attr_sampling_frequency.dev_attr.attr,
	&iio_const_attr_sampling_frequency_available.dev_attr.attr,
	&iio_dev_attr_in_m_in_scale_available.dev_attr.attr,
	NULL
};

static const struct attribute_group ad7793_attribute_group = {
	.attrs = ad7793_attributes,
};

static int ad7793_read_raw(struct iio_dev *indio_dev,
			   struct iio_chan_spec const *chan,
			   int *val,
			   int *val2,
			   long m)
{
	struct ad7793_state *st = iio_priv(indio_dev);
	int ret;
	unsigned long long scale_uv;
	bool unipolar = !!(st->conf & AD7793_CONF_UNIPOLAR);

	switch (m) {
	case IIO_CHAN_INFO_RAW:
		ret = ad_sigma_delta_single_conversion(indio_dev, chan, val);
		if (ret < 0)
			return ret;

		return IIO_VAL_INT;

	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_VOLTAGE:
			if (chan->differential) {
				*val = st->
					scale_avail[(st->conf >> 8) & 0x7][0];
				*val2 = st->
					scale_avail[(st->conf >> 8) & 0x7][1];
				return IIO_VAL_INT_PLUS_NANO;
			} else {
				/* 1170mV / 2^23 * 6 */
				scale_uv = (1170ULL * 1000000000ULL * 6ULL);
			}
			break;
		case IIO_TEMP:
				/* 1170mV / 0.81 mV/C / 2^23 */
				scale_uv = 1444444444444444ULL;
			break;
		default:
			return -EINVAL;
		}

		scale_uv >>= (chan->scan_type.realbits - (unipolar ? 0 : 1));
		*val = 0;
		*val2 = scale_uv;
		return IIO_VAL_INT_PLUS_NANO;
	case IIO_CHAN_INFO_OFFSET:
		if (!unipolar)
			*val = -(1 << (chan->scan_type.realbits - 1));
		else
			*val = 0;

		/* Kelvin to Celsius */
		if (chan->type == IIO_TEMP) {
			unsigned long long offset;
			unsigned int shift;

			shift = chan->scan_type.realbits - (unipolar ? 0 : 1);
			offset = 273ULL << shift;
			do_div(offset, 1444);
			*val -= offset;
		}
		return IIO_VAL_INT;
	}
	return -EINVAL;
}

static int ad7793_write_raw(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       int val,
			       int val2,
			       long mask)
{
	struct ad7793_state *st = iio_priv(indio_dev);
	int ret, i;
	unsigned int tmp;

	mutex_lock(&indio_dev->mlock);
	if (iio_buffer_enabled(indio_dev)) {
		mutex_unlock(&indio_dev->mlock);
		return -EBUSY;
	}

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		ret = -EINVAL;
		for (i = 0; i < ARRAY_SIZE(st->scale_avail); i++)
			if (val2 == st->scale_avail[i][1]) {
				ret = 0;
				tmp = st->conf;
				st->conf &= ~AD7793_CONF_GAIN(-1);
				st->conf |= AD7793_CONF_GAIN(i);

				if (tmp == st->conf)
					break;

				ad_sd_write_reg(&st->sd, AD7793_REG_CONF,
						sizeof(st->conf), st->conf);
				ad7793_calibrate_all(st);
				break;
			}
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&indio_dev->mlock);
	return ret;
}

static int ad7793_write_raw_get_fmt(struct iio_dev *indio_dev,
			       struct iio_chan_spec const *chan,
			       long mask)
{
	return IIO_VAL_INT_PLUS_NANO;
}

static const struct iio_info ad7793_info = {
	.read_raw = &ad7793_read_raw,
	.write_raw = &ad7793_write_raw,
	.write_raw_get_fmt = &ad7793_write_raw_get_fmt,
	.attrs = &ad7793_attribute_group,
	.validate_trigger = ad_sd_validate_trigger,
	.driver_module = THIS_MODULE,
};

#define DECLARE_AD7793_CHANNELS(_name, _b, _sb, _s) \
const struct iio_chan_spec _name##_channels[] = { \
	AD_SD_DIFF_CHANNEL(0, 0, 0, AD7793_CH_AIN1P_AIN1M, (_b), (_sb), (_s)), \
	AD_SD_DIFF_CHANNEL(1, 1, 1, AD7793_CH_AIN2P_AIN2M, (_b), (_sb), (_s)), \
	AD_SD_DIFF_CHANNEL(2, 2, 2, AD7793_CH_AIN3P_AIN3M, (_b), (_sb), (_s)), \
	AD_SD_SHORTED_CHANNEL(3, 0, AD7793_CH_AIN1M_AIN1M, (_b), (_sb), (_s)), \
	AD_SD_TEMP_CHANNEL(4, AD7793_CH_TEMP, (_b), (_sb), (_s)), \
	AD_SD_SUPPLY_CHANNEL(5, 3, AD7793_CH_AVDD_MONITOR, (_b), (_sb), (_s)), \
	IIO_CHAN_SOFT_TIMESTAMP(6), \
}

#define DECLARE_AD7795_CHANNELS(_name, _b, _sb) \
const struct iio_chan_spec _name##_channels[] = { \
	AD_SD_DIFF_CHANNEL(0, 0, 0, AD7793_CH_AIN1P_AIN1M, (_b), (_sb), 0), \
	AD_SD_DIFF_CHANNEL(1, 1, 1, AD7793_CH_AIN2P_AIN2M, (_b), (_sb), 0), \
	AD_SD_DIFF_CHANNEL(2, 2, 2, AD7793_CH_AIN3P_AIN3M, (_b), (_sb), 0), \
	AD_SD_DIFF_CHANNEL(3, 3, 3, AD7795_CH_AIN4P_AIN4M, (_b), (_sb), 0), \
	AD_SD_DIFF_CHANNEL(4, 4, 4, AD7795_CH_AIN5P_AIN5M, (_b), (_sb), 0), \
	AD_SD_DIFF_CHANNEL(5, 5, 5, AD7795_CH_AIN6P_AIN6M, (_b), (_sb), 0), \
	AD_SD_SHORTED_CHANNEL(6, 0, AD7795_CH_AIN1M_AIN1M, (_b), (_sb), 0), \
	AD_SD_TEMP_CHANNEL(7, AD7793_CH_TEMP, (_b), (_sb), 0), \
	AD_SD_SUPPLY_CHANNEL(8, 3, AD7793_CH_AVDD_MONITOR, (_b), (_sb), 0), \
	IIO_CHAN_SOFT_TIMESTAMP(9), \
}

static DECLARE_AD7793_CHANNELS(ad7785, 20, 32, 4);
static DECLARE_AD7793_CHANNELS(ad7792, 16, 32, 0);
static DECLARE_AD7793_CHANNELS(ad7793, 24, 32, 0);
static DECLARE_AD7795_CHANNELS(ad7794, 16, 32);
static DECLARE_AD7795_CHANNELS(ad7795, 24, 32);

static const struct ad7793_chip_info ad7793_chip_info_tbl[] = {
	[ID_AD7785] = {
		.channels = ad7785_channels,
		.num_channels = ARRAY_SIZE(ad7785_channels),
	},
	[ID_AD7792] = {
		.channels = ad7792_channels,
		.num_channels = ARRAY_SIZE(ad7792_channels),
	},
	[ID_AD7793] = {
		.channels = ad7793_channels,
		.num_channels = ARRAY_SIZE(ad7793_channels),
	},
	[ID_AD7794] = {
		.channels = ad7794_channels,
		.num_channels = ARRAY_SIZE(ad7794_channels),
	},
	[ID_AD7795] = {
		.channels = ad7795_channels,
		.num_channels = ARRAY_SIZE(ad7795_channels),
	},
};

static int ad7793_probe(struct spi_device *spi)
{
	const struct ad7793_platform_data *pdata = spi->dev.platform_data;
	struct ad7793_state *st;
	struct iio_dev *indio_dev;
	int ret, voltage_uv = 0;

	if (!pdata) {
		dev_err(&spi->dev, "no platform data?\n");
		return -ENODEV;
	}

	if (!spi->irq) {
		dev_err(&spi->dev, "no IRQ?\n");
		return -ENODEV;
	}

	indio_dev = iio_device_alloc(sizeof(*st));
	if (indio_dev == NULL)
		return -ENOMEM;

	st = iio_priv(indio_dev);

	ad_sd_init(&st->sd, indio_dev, spi, &ad7793_sigma_delta_info);

	st->reg = regulator_get(&spi->dev, "vcc");
	if (!IS_ERR(st->reg)) {
		ret = regulator_enable(st->reg);
		if (ret)
			goto error_put_reg;

		voltage_uv = regulator_get_voltage(st->reg);
	}

	st->chip_info =
		&ad7793_chip_info_tbl[spi_get_device_id(spi)->driver_data];

	if (pdata && pdata->vref_mv)
		st->int_vref_mv = pdata->vref_mv;
	else if (voltage_uv)
		st->int_vref_mv = voltage_uv / 1000;
	else
		st->int_vref_mv = 1170; /* Build-in ref */

	spi_set_drvdata(spi, indio_dev);

	indio_dev->dev.parent = &spi->dev;
	indio_dev->name = spi_get_device_id(spi)->name;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = st->chip_info->channels;
	indio_dev->num_channels = st->chip_info->num_channels;
	indio_dev->info = &ad7793_info;

	ret = ad_sd_setup_buffer_and_trigger(indio_dev);
	if (ret)
		goto error_disable_reg;

	ret = ad7793_setup(indio_dev, pdata);
	if (ret)
		goto error_remove_trigger;

	ret = iio_device_register(indio_dev);
	if (ret)
		goto error_remove_trigger;

	return 0;

error_remove_trigger:
	ad_sd_cleanup_buffer_and_trigger(indio_dev);
error_disable_reg:
	if (!IS_ERR(st->reg))
		regulator_disable(st->reg);
error_put_reg:
	if (!IS_ERR(st->reg))
		regulator_put(st->reg);

	iio_device_free(indio_dev);

	return ret;
}

static int ad7793_remove(struct spi_device *spi)
{
	struct iio_dev *indio_dev = spi_get_drvdata(spi);
	struct ad7793_state *st = iio_priv(indio_dev);

	iio_device_unregister(indio_dev);
	ad_sd_cleanup_buffer_and_trigger(indio_dev);

	if (!IS_ERR(st->reg)) {
		regulator_disable(st->reg);
		regulator_put(st->reg);
	}

	iio_device_free(indio_dev);

	return 0;
}

static const struct spi_device_id ad7793_id[] = {
	{"ad7785", ID_AD7785},
	{"ad7792", ID_AD7792},
	{"ad7793", ID_AD7793},
	{"ad7794", ID_AD7794},
	{"ad7795", ID_AD7795},
	{}
};
MODULE_DEVICE_TABLE(spi, ad7793_id);

static struct spi_driver ad7793_driver = {
	.driver = {
		.name	= "ad7793",
		.owner	= THIS_MODULE,
	},
	.probe		= ad7793_probe,
	.remove		= ad7793_remove,
	.id_table	= ad7793_id,
};
module_spi_driver(ad7793_driver);

MODULE_AUTHOR("Michael Hennerich <hennerich@blackfin.uclinux.org>");
MODULE_DESCRIPTION("Analog Devices AD7793 and simialr ADCs");
MODULE_LICENSE("GPL v2");
