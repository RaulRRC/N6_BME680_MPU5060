
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/linker/sections.h>
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

static lv_chart_series_t *gx_series;
static lv_chart_series_t *gy_series;
static lv_chart_series_t *gz_series;

static lv_chart_series_t *ax_series;
static lv_chart_series_t *ay_series;
static lv_chart_series_t *az_series;



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
		k_sleep(K_MSEC(2000));
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
	char Preassure[50], Temp[50], Humidity[50];

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


	
	gx_series = lv_chart_add_series(objects.chart_gx, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
	gy_series  = lv_chart_add_series(objects.chart_gy, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
	gz_series = lv_chart_add_series(objects.chart_gz, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);

	ax_series = lv_chart_add_series(objects.chart_ax, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
	ay_series  = lv_chart_add_series(objects.chart_ay, lv_palette_main(LV_PALETTE_BLUE), LV_CHART_AXIS_PRIMARY_Y);
	az_series = lv_chart_add_series(objects.chart_az, lv_palette_main(LV_PALETTE_GREEN), LV_CHART_AXIS_PRIMARY_Y);


	lv_chart_set_point_count(objects.chart_gx, 25); // e.g. 50 samples on screen
	lv_chart_set_point_count(objects.chart_gy, 25);
	lv_chart_set_point_count(objects.chart_gz, 25);

	
	lv_chart_set_point_count(objects.chart_ax, 25); // e.g. 50 samples on screen
	lv_chart_set_point_count(objects.chart_ay, 25);
	lv_chart_set_point_count(objects.chart_az, 25);

	lv_chart_set_range(objects.chart_gx, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
	lv_chart_set_range(objects.chart_gx, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);
	lv_chart_set_range(objects.chart_gx, LV_CHART_AXIS_PRIMARY_Y, -1000, 1000);

	
	lv_chart_set_range(objects.chart_ax, LV_CHART_AXIS_PRIMARY_Y, -4, 4);
	lv_chart_set_range(objects.chart_ay, LV_CHART_AXIS_PRIMARY_Y, -4, 4);
	lv_chart_set_range(objects.chart_az, LV_CHART_AXIS_PRIMARY_Y, -4, 4);


	struct BME680_msg data;
	struct MPU6050_msg data2;
	

	while (1) {
		int ret = k_msgq_get(&sensor_msgq, &data, K_MSEC(50));
		if(ret == 0){

			snprintf(Preassure, sizeof(Preassure), "%f", data.pressure);
			snprintf(Temp, sizeof(Temp), "%f",data.temperature);
			snprintf(Humidity, sizeof(Humidity), "%f", data.humidity);
			
			lv_label_set_text(objects.humidity_label, Humidity);
			lv_label_set_text(objects.pressure_label, Preassure);
			lv_label_set_text(objects.temperature_label, Temp);


		
		
			lv_slider_set_value(objects.preasure_bar, (int32_t)data.pressure, LV_ANIM_ON);
			lv_slider_set_value(objects.humid_bar, (int32_t)data.humidity, LV_ANIM_ON);
			lv_scale_set_line_needle_value(objects.temp_gauge, objects.indicator_line, 42, (int32_t)data.temperature );
		
		}
		int ret2 = k_msgq_get(&MPU5060_msg, &data2, K_MSEC(50));
		if(ret2 == 0){
			lv_chart_set_next_value(objects.chart_gx, gx_series, (int32_t)data2.GX);
			lv_chart_set_next_value(objects.chart_gy, gy_series, (int32_t)data2.GY);
			lv_chart_set_next_value(objects.chart_gz, gz_series, (int32_t)data2.GZ);

			lv_chart_set_next_value(objects.chart_ax, ax_series, (int32_t)data2.AX);
			lv_chart_set_next_value(objects.chart_ay, ay_series, (int32_t)data2.AY);
			lv_chart_set_next_value(objects.chart_az, az_series, (int32_t)data2.AZ);




		}
		lv_timer_handler();

	}

	return 0;
}
