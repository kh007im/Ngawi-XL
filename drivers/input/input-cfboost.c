/*
 * drivers/input/input-cfboost.c
 *
 * Copyright (C) 2012 NVIDIA Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/printk.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/pm_qos.h>

/* This module listens to input events and sets a temporary frequency
 * floor upon input event detection. This is based on changes to
 * cpufreq ondemand governor by:
 *
 * Tero Kristo <tero.kristo@xxxxxxxxx>
 * Brian Steuer <bsteuer@xxxxxxxxxxxxxx>
 * David Ng <dave@xxxxxxxxxxxxxx>
 *
 * at git://codeaurora.org/kernel/msm.git tree, commits:
 *
 * 2a6181bc76c6ce46ca0fa8e547be42acd534cf0e
 * 1cca8861d8fda4e05f6b0c59c60003345c15454d
 * 96a9aeb02bf5b3fbbef47e44460750eb275e9f1b
 * b600449501cf15928440f87eff86b1f32d14214e
 * 88a65c7ae04632ffee11f9fc628d7ab017c06b83
 */

MODULE_AUTHOR("Antti P Miettinen <amiettinen@xxxxxxxxxx>");
MODULE_DESCRIPTION("Input event CPU frequency booster");
MODULE_LICENSE("GPL v2");


static struct pm_qos_request qos_req;
static struct work_struct boost;
static struct delayed_work unboost;
static unsigned int boost_freq = 1026000; /* kHz */
module_param(boost_freq, uint, 0644);
static unsigned long boost_time = 500; /* ms */
module_param(boost_time, ulong, 0644);
static struct workqueue_struct *cfb_wq;

static void cfb_boost(struct work_struct *w)
{
	cancel_delayed_work_sync(&unboost);
	pm_qos_update_request(&qos_req, boost_freq);
	printk("CFBoost: Input detected. Boosting for %ld msec",boost_time);
	queue_delayed_work(cfb_wq, &unboost, msecs_to_jiffies(boost_time));
}

static void cfb_unboost(struct work_struct *w)
{
	printk("CFBoost: Unboosting now");
	pm_qos_update_request(&qos_req, PM_QOS_DEFAULT_VALUE);
}

static void cfb_input_event(struct input_handle *handle, unsigned int type,
			    unsigned int code, int value)
{
	if (!work_pending(&boost))
		queue_work(cfb_wq, &boost);
}

static int cfb_input_connect(struct input_handler *handler,
			     struct input_dev *dev,
			     const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "icfboost";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void cfb_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

/* XXX make configurable */
static const struct input_device_id cfb_ids[] = {
	{ /* touch screen */
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.keybit = {[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH) },
	},
	{ /* mouse */
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
				INPUT_DEVICE_ID_MATCH_KEYBIT,
		.evbit = { BIT_MASK(EV_REL) },
		.keybit = {[BIT_WORD(BTN_MOUSE)] = BIT_MASK(BTN_MOUSE) },
	},
	{ },
};

static struct input_handler cfb_input_handler = {
	.event		= cfb_input_event,
	.connect	= cfb_input_connect,
	.disconnect	= cfb_input_disconnect,
	.name		= "icfboost",
	.id_table	= cfb_ids,
};

static int __init cfboost_init(void)
{
	int ret;

	cfb_wq = create_workqueue("icfb-wq");
	if (!cfb_wq)
		return -ENOMEM;
	INIT_WORK(&boost, cfb_boost);
	INIT_DELAYED_WORK(&unboost, cfb_unboost);
	ret = input_register_handler(&cfb_input_handler);
	if (ret) {
		destroy_workqueue(cfb_wq);
		return ret;
	}
	pm_qos_add_request(&qos_req, PM_QOS_CPU_FREQ_MIN,
			   PM_QOS_DEFAULT_VALUE);
	return 0;
}

static void __exit cfboost_exit(void)
{
	/* stop input events */
	input_unregister_handler(&cfb_input_handler);
	/* cancel pending work requests */
	cancel_work_sync(&boost);
	cancel_delayed_work_sync(&unboost);
	/* clean up */
	destroy_workqueue(cfb_wq);
	pm_qos_remove_request(&qos_req);
}

module_init(cfboost_init);
module_exit(cfboost_exit);
