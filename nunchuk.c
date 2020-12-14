// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>

struct nunchuk_dev {
	struct i2c_client *i2c_client;
	struct input_dev *input;
};

static int nunchuk_init(struct i2c_client *client)
{
	int ret;
	u8 buf[6];
	
	buf[0] = 0xf0;
	buf[1] = 0x55;
	ret = i2c_master_send(client, buf, 2);
	if (ret != 2) {
		dev_err(&client->dev, "failed to start handshake\n");
		return -EIO;
	}
	msleep(1);

	buf[0] = 0xfb;
	buf[1] = 0x00;
	ret = i2c_master_send(client, buf, 2);
	if (ret != 2) {
		dev_err(&client->dev, "failed to complete handshake\n");
		return -EIO;
	}
	msleep(1);

	return 0;
}

static int nunchuk_read_registers(struct i2c_client *client, u8 *buf, size_t bufsize)
{
	int ret;
	u8 cmd[1];

	usleep_range(10000, 20000);

	cmd[0] = 0x00;
	ret = i2c_master_send(client, cmd, 1);
	if (ret < 1) {
		dev_err(&client->dev, "failed to start register read\n");
		return -EIO;
	}
	msleep(10);

	ret = i2c_master_recv(client, buf, bufsize);
	if (ret < bufsize) {
		dev_err(&client->dev, "failed to receive data\n");
		return -EIO;
	}
	msleep(10);

	return 0;
}

static void nunchuk_poll(struct input_dev *input)
{
	int ret, zpressed, cpressed;
	u8 buf[6];
	struct nunchuk_dev *nunchuk = input_get_drvdata(input);
	struct i2c_client *client = nunchuk->i2c_client;
	struct device *dev = &client->dev;

	ret = nunchuk_read_registers(client, buf, sizeof(buf));
	if (ret < 0) {
		dev_err(dev, "failed to read nunchuk registers\n");
		return;
	}

	zpressed = buf[5] & BIT(0) ? 0 : 1;
	cpressed = buf[5] & BIT(1) ? 0 : 1;

	input_event(input, EV_KEY, BTN_Z, zpressed);
	input_event(input, EV_KEY, BTN_C, cpressed);
	input_sync(input);
}

static int nunchuk_probe(struct i2c_client *client)
{
	int ret;
	struct nunchuk_dev *nunchuk;
	struct input_dev *input;
	struct device *dev = &client->dev;

	dev_info(dev, "nunchuk_probe\n");

	ret = nunchuk_init(client);
	if (ret) {
		dev_err(&client->dev, "nunchuk initialization failed\n");
		return ret;
	}

	nunchuk = devm_kzalloc(dev, sizeof(*nunchuk), GFP_KERNEL);
	if (!nunchuk)
		return -ENOMEM;

	input = devm_input_allocate_device(dev);
	if (!input)
		return -ENOMEM;

	nunchuk->i2c_client = client;
	nunchuk->input = input;

	input_set_drvdata(input, nunchuk);

	ret = input_setup_polling(input, nunchuk_poll);
	if (ret) {
		dev_err(dev, "failed to setup polling\n");
		return ret;
	}
	input_set_poll_interval(input, 50);

	input->name = "nunchuk";
	input->id.bustype = BUS_I2C;
	set_bit(EV_KEY, input->evbit);
	set_bit(BTN_C, input->keybit);
	set_bit(BTN_Z, input->keybit);

	ret = input_register_device(input);
	if (ret) {
		dev_err(dev, "failed to register input device\n");
		return ret;
	}

	return 0;
}

static int nunchuk_remove(struct i2c_client *client)
{
	dev_info(&client->dev, "nunchuk_remove\n");
	return 0;
}

static const struct of_device_id nunchuk_of_match[] = {
	{ .compatible = "nintendo,nunchuk" },
	{ },
};
MODULE_DEVICE_TABLE(of, nunchuk_of_match);

static struct i2c_driver nunchuk_driver = {
	.driver = {
		.name = "nunchuk",
		.of_match_table = nunchuk_of_match,
	},
	.probe_new = nunchuk_probe,
	.remove = nunchuk_remove,
};

module_i2c_driver(nunchuk_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Eduardo Maestri Righes");
MODULE_DESCRIPTION("Nintendo Nunchuk driver");
