/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log_backend_ble.h>

#include <zephyr/drivers/sensor.h>

const struct device *const accel = DEVICE_DT_GET(DT_NODELABEL(lis3dh));

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, LOGGER_BACKEND_BLE_ADV_UUID_DATA)
};

static const struct bt_data sd[] = {
	BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, sizeof(CONFIG_BT_DEVICE_NAME) - 1),
};

static void start_adv(void)
{
	int err;

	err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		LOG_ERR("Advertising failed to start (err %d)", err);
		return;
	}

	LOG_INF("Advertising successfully started");
}

static void connected(struct bt_conn *conn, uint8_t err)
{
	if (err) {
		LOG_ERR("Connection failed (err 0x%02x)", err);
	} else {
		LOG_INF("Connected");
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	if (conn) {
		bt_conn_unref(conn);
		conn = NULL;
	}
	LOG_INF("Disconnected (reason 0x%02x)", reason);
	start_adv();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
};

static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("Pairing cancelled: %s", addr);
}

static struct bt_conn_auth_cb auth_cb_display = {
	.cancel = auth_cancel,
};

void backend_ble_hook(bool status, void *ctx)
{
	ARG_UNUSED(ctx);

	if (status) {
		LOG_INF("BLE Logger Backend enabled.");
	} else {
		LOG_INF("BLE Logger Backend disabled.");
	}
}

static void trigger_handler(const struct device *dev,
							const struct sensor_trigger *trig) {
	int err;
		
	struct sensor_value accel_v[3];

	err = sensor_channel_get(dev, SENSOR_CHAN_ACCEL_XYZ, accel_v);
	if (err < 0) {
		LOG_ERR("sensor_channel_get failed: %d", err);
		return;
	}

	LOG_INF("X: %f, Y: %f, Z: %f",
		sensor_value_to_double(&accel_v[0]),
		sensor_value_to_double(&accel_v[1]),
		sensor_value_to_double(&accel_v[2]));
}

int main(void)
{
	int err;

	if (!device_is_ready(accel)) {
		LOG_ERR("Accelerometer device not ready");
		return -1;
	}

	logger_backend_ble_set_hook(backend_ble_hook, NULL);

	err = bt_enable(NULL);
	if (err < 0) {
		LOG_ERR("Bluetooth init failed (err %d)", err);
		return -1;
	}

	bt_conn_auth_cb_register(&auth_cb_display);

	start_adv();

	struct sensor_trigger trig;

	trig.type = SENSOR_TRIG_DATA_READY;
	trig.chan = SENSOR_CHAN_ACCEL_XYZ;

	if (IS_ENABLED(CONFIG_LIS2DH_ODR_RUNTIME)) {
		struct sensor_value odr = {
			.val1 = 1,
		};

		err = sensor_attr_set(accel, trig.chan,
					 SENSOR_ATTR_SAMPLING_FREQUENCY,
					 &odr);
		if (err != 0) {
			printf("Failed to set odr: %d\n", err);
			return 0;
		}
		printf("Sampling at %u Hz\n", odr.val1);
	}

	err = sensor_trigger_set(accel, &trig, trigger_handler);
	if (err != 0) {
		printf("Failed to set trigger: %d\n", err);
		return 0;
	}

	while (true) {
		k_sleep(K_MSEC(2000));
	}

	return 0;
}
