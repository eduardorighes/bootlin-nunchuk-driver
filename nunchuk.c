// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/delay.h>

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

static int nunchuk_probe(struct i2c_client *client)
{
	int ret;
	u8 buf[6];
	u8 cpressed;
	u8 zpressed;

	dev_info(&client->dev, "nunchuk_probe\n");

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

	ret = nunchuk_read_registers(client, buf, sizeof(buf));
	if (ret < 0) {
		dev_err(&client->dev, "failed initial read\n");
		return -EIO;
	}
	ret = nunchuk_read_registers(client, buf, sizeof(buf));
	if (ret < 0) {
		dev_err(&client->dev, "failed to read\n");
		return -EIO;
	}

	zpressed = buf[5] & BIT(0) ? 0 : 1;
	cpressed = buf[5] & BIT(1) ? 0 : 1;

	dev_info(&client->dev, "button state -> C = %d, Z = %d\n", cpressed, zpressed);

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
