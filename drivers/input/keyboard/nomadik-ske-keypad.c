/*
 * Copyright (C) ST-Ericsson SA 2010
 *
 * Author: Naveen Kumar G <naveen.gaddipati@stericsson.com> for ST-Ericsson
 * Author: Sundar Iyer <sundar.iyer@stericsson.com> for ST-Ericsson
 *
 * License terms:GNU General Public License (GPL) version 2
 *
 * Keypad controller driver for the SKE (Scroll Key Encoder) module used in
 * the Nomadik 8815 and Ux500 platforms.
 */

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/module.h>

#include <plat/ske.h>

/* SKE_CR bits */
#define SKE_KPMLT	(0x1 << 6)
#define SKE_KPCN	(0x7 << 3)
#define SKE_KPASEN	(0x1 << 2)
#define SKE_KPASON	(0x1 << 7)

/* SKE_IMSC bits */
#define SKE_KPIMA	(0x1 << 2)

/* SKE_ICR bits */
#define SKE_KPICS	(0x1 << 3)
#define SKE_KPICA	(0x1 << 2)

/* SKE_RIS bits */
#define SKE_KPRISA	(0x1 << 2)

#define SKE_KEYPAD_ROW_SHIFT	3
#define SKE_KPD_KEYMAP_SIZE	(8 * 8)

/* keypad auto scan registers */
#define SKE_ASR0	0x20
#define SKE_ASR1	0x24
#define SKE_ASR2	0x28
#define SKE_ASR3	0x2C

#define SKE_NUM_ASRX_REGISTERS	(4)

/**
 * struct ske_keypad  - data structure used by keypad driver
 * @irq:	irq no
 * @reg_base:	ske regsiters base address
 * @input:	pointer to input device object
 * @board:	keypad platform device
 * @keymap:	matrix scan code table for keycodes
 * @clk:	clock structure pointer
 */
struct ske_keypad {
	int irq;
	void __iomem *reg_base;
	struct input_dev *input;
	const struct ske_keypad_platform_data *board;
	unsigned short keymap[SKE_KPD_KEYMAP_SIZE];
	struct clk *clk;
	spinlock_t ske_keypad_lock;
};

static void ske_keypad_set_bits(struct ske_keypad *keypad, u16 addr,
		u8 mask, u8 data)
{
	u32 ret;

	spin_lock(&keypad->ske_keypad_lock);

	ret = readl(keypad->reg_base + addr);
	ret &= ~mask;
	ret |= data;
	writel(ret, keypad->reg_base + addr);

	spin_unlock(&keypad->ske_keypad_lock);
}

/*
 * ske_keypad_chip_init: init keypad controller configuration
 *
 * Enable Multi key press detection, auto scan mode
 */
static int __devinit ske_keypad_chip_init(struct ske_keypad *keypad)
{
	u32 value;
	int timeout = 50;

	/* check SKE_RIS to be 0 */
	while ((readl(keypad->reg_base + SKE_RIS) != 0x00000000) && timeout--)
		cpu_relax();

	if (!timeout)
		return -EINVAL;

	/*
	 * set debounce value
	 * keypad dbounce is configured in DBCR[15:8]
	 * dbounce value in steps of 32/32.768 ms
	 */
	spin_lock(&keypad->ske_keypad_lock);
	value = readl(keypad->reg_base + SKE_DBCR);
	value = value & 0xff;
	value |= ((keypad->board->debounce_ms * 32000)/32768) << 8;
	writel(value, keypad->reg_base + SKE_DBCR);
	spin_unlock(&keypad->ske_keypad_lock);

	/* enable multi key detection */
	ske_keypad_set_bits(keypad, SKE_CR, 0x0, SKE_KPMLT);

	/*
	 * set up the number of columns
	 * KPCN[5:3] defines no. of keypad columns to be auto scanned
	 */
	value = (keypad->board->kcol - 1) << 3;
	ske_keypad_set_bits(keypad, SKE_CR, SKE_KPCN, value);

	/* clear keypad interrupt for auto(and pending SW) scans */
	ske_keypad_set_bits(keypad, SKE_ICR, 0x0, SKE_KPICA | SKE_KPICS);

	/* un-mask keypad interrupts */
	ske_keypad_set_bits(keypad, SKE_IMSC, 0x0, SKE_KPIMA);

	/* enable automatic scan */
	ske_keypad_set_bits(keypad, SKE_CR, 0x0, SKE_KPASEN);

	return 0;
}

static void ske_keypad_read_data(struct ske_keypad *keypad)
{
	struct input_dev *input = keypad->input;
	u16 status;
	int col = 0, row = 0, code;
	int ske_asr, ske_ris, key_pressed, i;

	/*
	 * Read the auto scan registers
	 *
	 * Each SKE_ASRx (x=0 to x=3) contains two row values.
	 * lower byte contains row value for column 2*x,
	 * upper byte contains row value for column 2*x + 1
	 */
	for (i = 0; i < SKE_NUM_ASRX_REGISTERS; i++) {
		ske_asr = readl(keypad->reg_base + SKE_ASR0 + (4 * i));
		if (!ske_asr)
			continue;

		/* now that ASRx is zero, find out the column x and row y*/
		if (ske_asr & 0xff) {
			col = i * 2;
			status = ske_asr & 0xff;
		} else {
			col = (i * 2) + 1;
			status = (ske_asr & 0xff00) >> 8;
		}

		/* find out the row */
		row = __ffs(status);

		code = MATRIX_SCAN_CODE(row, col, SKE_KEYPAD_ROW_SHIFT);
		ske_ris = readl(keypad->reg_base + SKE_RIS);
		key_pressed = ske_ris & SKE_KPRISA;

		input_event(input, EV_MSC, MSC_SCAN, code);
		input_report_key(input, keypad->keymap[code], key_pressed);
		input_sync(input);
	}
}

static irqreturn_t ske_keypad_irq(int irq, void *dev_id)
{
	struct ske_keypad *keypad = dev_id;
	int retries = 20;

	/* disable auto scan interrupt; mask the interrupt generated */
	ske_keypad_set_bits(keypad, SKE_IMSC, ~SKE_KPIMA, 0x0);
	ske_keypad_set_bits(keypad, SKE_ICR, 0x0, SKE_KPICA);

	while ((readl(keypad->reg_base + SKE_CR) & SKE_KPASON) && --retries)
		msleep(5);

	if (retries) {
		/* SKEx registers are stable and can be read */
		ske_keypad_read_data(keypad);
	}

	/* enable auto scan interrupts */
	ske_keypad_set_bits(keypad, SKE_IMSC, 0x0, SKE_KPIMA);

	return IRQ_HANDLED;
}

static int __devinit ske_keypad_probe(struct platform_device *pdev)
{
	const struct ske_keypad_platform_data *plat = pdev->dev.platform_data;
	struct ske_keypad *keypad;
	struct input_dev *input;
	struct resource *res;
	int irq;
	int error;

	if (!plat) {
		dev_err(&pdev->dev, "invalid keypad platform data\n");
		return -EINVAL;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "failed to get keypad irq\n");
		return -EINVAL;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "missing platform resources\n");
		return -EINVAL;
	}

	keypad = kzalloc(sizeof(struct ske_keypad), GFP_KERNEL);
	input = input_allocate_device();
	if (!keypad || !input) {
		dev_err(&pdev->dev, "failed to allocate keypad memory\n");
		error = -ENOMEM;
		goto err_free_mem;
	}

	keypad->irq = irq;
	keypad->board = plat;
	keypad->input = input;
	spin_lock_init(&keypad->ske_keypad_lock);

	if (!request_mem_region(res->start, resource_size(res), pdev->name)) {
		dev_err(&pdev->dev, "failed to request I/O memory\n");
		error = -EBUSY;
		goto err_free_mem;
	}

	keypad->reg_base = ioremap(res->start, resource_size(res));
	if (!keypad->reg_base) {
		dev_err(&pdev->dev, "failed to remap I/O memory\n");
		error = -ENXIO;
		goto err_free_mem_region;
	}

	keypad->clk = clk_get(&pdev->dev, NULL);
	if (IS_ERR(keypad->clk)) {
		dev_err(&pdev->dev, "failed to get clk\n");
		error = PTR_ERR(keypad->clk);
		goto err_iounmap;
	}

	input->id.bustype = BUS_HOST;
	input->name = "ux500-ske-keypad";
	input->dev.parent = &pdev->dev;

	input->keycode = keypad->keymap;
	input->keycodesize = sizeof(keypad->keymap[0]);
	input->keycodemax = ARRAY_SIZE(keypad->keymap);

	input_set_capability(input, EV_MSC, MSC_SCAN);

	__set_bit(EV_KEY, input->evbit);
	if (!plat->no_autorepeat)
		__set_bit(EV_REP, input->evbit);

	matrix_keypad_build_keymap(plat->keymap_data, SKE_KEYPAD_ROW_SHIFT,
			input->keycode, input->keybit);

	clk_enable(keypad->clk);

	/* go through board initialization helpers */
	if (keypad->board->init)
		keypad->board->init();

	error = ske_keypad_chip_init(keypad);
	if (error) {
		dev_err(&pdev->dev, "unable to init keypad hardware\n");
		goto err_clk_disable;
	}

	error = request_threaded_irq(keypad->irq, NULL, ske_keypad_irq,
				     IRQF_ONESHOT, "ske-keypad", keypad);
	if (error) {
		dev_err(&pdev->dev, "allocate irq %d failed\n", keypad->irq);
		goto err_clk_disable;
	}

	error = input_register_device(input);
	if (error) {
		dev_err(&pdev->dev,
				"unable to register input device: %d\n", error);
		goto err_free_irq;
	}

	if (plat->wakeup_enable)
		device_init_wakeup(&pdev->dev, true);

	platform_set_drvdata(pdev, keypad);

	return 0;

err_free_irq:
	free_irq(keypad->irq, keypad);
err_clk_disable:
	clk_disable(keypad->clk);
	clk_put(keypad->clk);
err_iounmap:
	iounmap(keypad->reg_base);
err_free_mem_region:
	release_mem_region(res->start, resource_size(res));
err_free_mem:
	input_free_device(input);
	kfree(keypad);
	return error;
}

static int __devexit ske_keypad_remove(struct platform_device *pdev)
{
	struct ske_keypad *keypad = platform_get_drvdata(pdev);
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	free_irq(keypad->irq, keypad);

	input_unregister_device(keypad->input);

	clk_disable(keypad->clk);
	clk_put(keypad->clk);

	if (keypad->board->exit)
		keypad->board->exit();

	iounmap(keypad->reg_base);
	release_mem_region(res->start, resource_size(res));
	kfree(keypad);

	return 0;
}

#ifdef CONFIG_PM
static int ske_keypad_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ske_keypad *keypad = platform_get_drvdata(pdev);
	int irq = platform_get_irq(pdev, 0);

	if (device_may_wakeup(dev))
		enable_irq_wake(irq);
	else
		ske_keypad_set_bits(keypad, SKE_IMSC, ~SKE_KPIMA, 0x0);

	return 0;
}

static int ske_keypad_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct ske_keypad *keypad = platform_get_drvdata(pdev);
	int irq = platform_get_irq(pdev, 0);

	if (device_may_wakeup(dev))
		disable_irq_wake(irq);
	else
		ske_keypad_set_bits(keypad, SKE_IMSC, 0x0, SKE_KPIMA);

	return 0;
}

static const struct dev_pm_ops ske_keypad_dev_pm_ops = {
	.suspend = ske_keypad_suspend,
	.resume = ske_keypad_resume,
};
#endif

struct platform_driver ske_keypad_driver = {
	.driver = {
		.name = "nmk-ske-keypad",
		.owner  = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &ske_keypad_dev_pm_ops,
#endif
	},
	.probe = ske_keypad_probe,
	.remove = __devexit_p(ske_keypad_remove),
};
module_platform_driver(ske_keypad_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Naveen Kumar <naveen.gaddipati@stericsson.com> / Sundar Iyer <sundar.iyer@stericsson.com>");
MODULE_DESCRIPTION("Nomadik Scroll-Key-Encoder Keypad Driver");
MODULE_ALIAS("platform:nomadik-ske-keypad");
