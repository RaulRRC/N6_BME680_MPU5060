/*
 * Copyright (c) 2018 Jan Van Winkel <jan.van_winkel@dxplore.eu>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <lvgl_input_device.h>

#include <ui.h>

#define LOG_LEVEL CONFIG_LOG_DEFAULT_LEVEL
#define STACKSIZE 1024
#define PRIORITY_PRODUCER 5
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app);

/* Semaphore for signaling between threads */
K_SEM_DEFINE(data_ready_sem, 0, 1);

static lv_chart_series_t *temp_series;
static lv_chart_series_t *hum_series;
static lv_chart_series_t *pres_series;


struct BME680_msg {
	float temperature;
	float humidity;
	float pressure;
	uint32_t timestamp;
};

struct MPU6050_msg {
	double GX;
	double GY;
	double GZ;
	double AY;
	double AX;
	double AZ;
	uint32_t timestamp;
};

K_MSGQ_DEFINE(sensor_msgq, sizeof(struct BME680_msg), 1, 4);
K_MSGQ_DEFINE(MPU5060_msg, sizeof(struct MPU6050_msg), 1, 4);

// static int process_mpu6050(const struct device *dev)
// {
//
//
// 	return rc;
// }

void BME680_task(void *p1, void *p2, void *p3)
{
	const struct device *const dev = DEVICE_DT_GET_ANY(bosch_bme680);
	if (!device_is_ready(dev)) {
		printf("BME680 device not ready\n");
		return;
	}

	struct BME680_msg  msg;
	struct sensor_value temp, press, humidity, gas_res;
	while (1) {
		if (sensor_sample_fetch(dev) < 0) {
			printf("Failed to fetch sample\n");
		} else {
			sensor_channel_get(dev, SENSOR_CHAN_AMBIENT_TEMP, &temp);
			sensor_channel_get(dev, SENSOR_CHAN_PRESS, &press);
			sensor_channel_get(dev, SENSOR_CHAN_HUMIDITY, &humidity);
			sensor_channel_get(dev, SENSOR_CHAN_GAS_RES, &gas_res);

			msg.timestamp = k_uptime_get_32();
			msg.humidity = humidity.val1;
			msg.temperature = temp.val1;
			msg.pressure = press.val1;

			/* Try to put message on the queue, don't block if full */
			int ret = k_msgq_put(&sensor_msgq, &msg, K_NO_WAIT);
			if (ret != 0) {
				printk("Producer: queue full, dropping message\n");
			} else {
				printk("Temp: %d.%06d C, Press: %d.%06d kPa, Humidity: %d.%06d %%, Gas: %d.%06d Ohms\n",
			   temp.val1, temp.val2,
			   press.val1, press.val2,
			   humidity.val1, humidity.val2,
			   gas_res.val1, gas_res.val2);
			}
		}
		k_sleep(K_MSEC(1000));
	}
}

void MPU6050_task(void *p1, void *p2, void *p3)
{
	// k_sleep(K_MSEC(150));
	const struct device *const mpu6050 = DEVICE_DT_GET_ONE(invensense_mpu6050);

	// k_sleep(K_MSEC(150));

	if (!device_is_ready(mpu6050)) {
		printf("Device %s is not ready\n", mpu6050->name);
		return;
	}

#ifdef CONFIG_MPU6050_TRIGGER
	trigger = (struct sensor_trigger) {
		.type = SENSOR_TRIG_DATA_READY,
		.chan = SENSOR_CHAN_ALL,
	};
	if (sensor_trigger_set(mpu6050, &trigger,
				   handle_mpu6050_drdy) < 0) {
		printf("Cannot configure trigger\n");
		return 0;
				   }
	printk("Configured for triggered sampling.\n");
#endif
	struct MPU6050_msg  msg = {};

	while (!IS_ENABLED(CONFIG_MPU6050_TRIGGER)) {
		struct MPU6050_msg Data;
		struct sensor_value temperature;
		struct sensor_value accel[3];
		struct sensor_value gyro[3];
		int rc = sensor_sample_fetch(mpu6050);

		if (rc == 0) {
			rc = sensor_channel_get(mpu6050, SENSOR_CHAN_ACCEL_XYZ,
						accel);
		}
		if (rc == 0) {
			rc = sensor_channel_get(mpu6050, SENSOR_CHAN_GYRO_XYZ,
						gyro);
		}
		if (rc == 0) {
			rc = sensor_channel_get(mpu6050, SENSOR_CHAN_DIE_TEMP,
						&temperature);
		}

		if (rc == 0) {
			msg.AY = sensor_value_to_double(&accel[0]);
			msg.AX = sensor_value_to_double(&accel[1]);
			msg.AZ = sensor_value_to_double(&accel[2]);
			msg.GX = sensor_value_to_double(&gyro[0]);
			msg.GY = sensor_value_to_double(&gyro[1]);
			msg.GZ = sensor_value_to_double(&gyro[2]);
		} else {
			printf("sample fetch/get failed: %d\n", rc);
		}


		if (rc != 0) {
			break;
		}

		/* Try to put message on the queue, don't block if full */
		int ret = k_msgq_put(&MPU5060_msg, &msg, K_NO_WAIT);

		if (ret != 0) {
			printk("Producer: queue full, dropping message\n");
		} else {
			printk("data_sent");
		}

		k_sleep(K_MSEC(250));
	}
}

K_THREAD_DEFINE(task_a_id, STACKSIZE, BME680_task,
				NULL, NULL, NULL, PRIORITY_PRODUCER, 0, 0);
K_THREAD_DEFINE(task_b_id, 2048, MPU6050_task,
				NULL, NULL, NULL, PRIORITY_PRODUCER, 0, 0);


int main(void)
{
	const struct device *display_dev;

	display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
	if (!device_is_ready(display_dev)) {
		printk("Display device not ready\n");
		return -1;
	}

	/* Initialize the EEZ Studio generated UI (creates screens, styles, vars) */
	ui_init();

	/* Force LVGL to render the first frame BEFORE turning the screen on */
	lv_timer_handler();
	k_msleep(50); // give it a moment to flush to the display driver

	/* Turn on the display now that LVGL has something to draw */
	display_blanking_off(display_dev);


	temp_series = lv_chart_add_series(objects.chart_temp, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
	hum_series  = lv_chart_add_series(objects.chart_humidity, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
	pres_series = lv_chart_add_series(objects.chart_pressure, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

	lv_chart_set_point_count(objects.chart_temp, 50); // e.g. 50 samples on screen
	lv_chart_set_point_count(objects.chart_humidity, 50);
	lv_chart_set_point_count(objects.chart_pressure, 50);

	lv_chart_set_range(objects.chart_temp, LV_CHART_AXIS_PRIMARY_Y, 0, 50);
	lv_chart_set_range(objects.chart_humidity, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
	lv_chart_set_range(objects.chart_pressure, LV_CHART_AXIS_PRIMARY_Y, 900, 1100);



	struct BME680_msg data;
	struct MPU6050_msg data2;
	char AX[10],AY[10],AZ[10],GX[10],GY[10],GZ[10];

	while (1) {
		int ret = k_msgq_get(&sensor_msgq, &data, K_MSEC(50));
		if(ret == 0){
			lv_chart_set_next_value(objects.chart_temp, temp_series, (int32_t)data.temperature);
			lv_chart_set_next_value(objects.chart_humidity, hum_series, (int32_t)data.humidity);
			lv_chart_set_next_value(objects.chart_pressure, pres_series, (int32_t)data.pressure);
		}
		int ret2 = k_msgq_get(&MPU5060_msg, &data2, K_MSEC(50));
		if(ret2 == 0){
			snprintf(AX, sizeof(data2.AX), "%f", data2.AX);
			snprintf(AY, sizeof(data2.AY), "%f", data2.AY);
			snprintf(AZ, sizeof(data2.AZ), "%f", data2.AZ);
			snprintf(GX, sizeof(data2.GX), "%f", data2.GX);
			snprintf(GY, sizeof(data2.GY), "%f", data2.GY);
			snprintf(GZ, sizeof(data2.GZ), "%f", data2.GZ);

			lv_label_set_text(objects.ax,AX);
			lv_label_set_text(objects.ay,AY);
			lv_label_set_text(objects.az,AZ);
			lv_label_set_text(objects.gx,GX);
			lv_label_set_text(objects.gy,GY);
			lv_label_set_text(objects.gz,GZ);

		}
		lv_timer_handler();

	}

	return 0;
}
