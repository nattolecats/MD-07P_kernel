#include "jadard_platform.h"
#include "jadard_common.h"

DEFINE_MUTEX(jd_wr_mux);

struct of_device_id jadard_match_table[] = {
    {.compatible = "mediatek,cap_touch" },
    {.compatible = "jadard,jdcommon" },
    {},
};
MODULE_DEVICE_TABLE(of, jadard_match_table);

u8 *gpDMABuf_va = NULL;
u8 *gpDMABuf_pa = NULL;
struct i2c_client *i2c_client_point = NULL;
#ifdef JD_ENABLE_PROXIMITY_TP_SCREEN_OFF 
extern struct alsps_init_info ps_jadard_init_info;
extern int alsps_driver_add(struct alsps_init_info *obj);
extern int jd_tpd_enable_ps(int enable);
#endif

extern struct jadard_ts_data *pjadard_ts_data;
#ifdef JD_ENABLE_PROXIMITY_TP_SCREEN_OFF 
extern struct alsps_init_info ps_jadard_init_info;
extern int alsps_driver_add(struct alsps_init_info *obj);
extern int jd_tpd_enable_ps(int enable);
#endif
extern struct jadard_ic_data *pjadard_ic_data;

int jadard_dev_set(struct jadard_ts_data *ts)
{
    ts->input_dev = tpd->dev;

    if (pjadard_ic_data->JD_STYLUS_EN) {
        ts->stylus_dev = input_allocate_device();

        if (ts->stylus_dev == NULL) {
            JD_E("%s: Failed to allocate input stylus_dev\n", __func__);
            input_free_device(ts->input_dev);
            return -ENOMEM;
        }
        ts->stylus_dev->name = "jadard-stylus";
    }

    return JD_NO_ERR;
}

int jadard_input_register_device(struct input_dev *input_dev)
{
    return JD_NO_ERR;
}

int jadard_parse_dt(struct jadard_ts_data *ts, struct jadard_platform_data *pdata)
{
    int coords_size = 0;
    uint32_t coords[4] = {0};
    uint32_t ret, data;
    struct property *prop = NULL;
    struct device_node *dt = ts->dev->of_node;

    ret = of_property_read_u32(dt, "jadard,panel-max-points", &data);
    pjadard_ic_data->JD_MAX_PT = (!ret ? data : 10);
    ret = of_property_read_u32(dt, "jadard,int-is-edge", &data);
    pjadard_ic_data->JD_INT_EDGE = (!ret ? (data > 0 ? true : false) : true);
    ret = of_property_read_u32(dt, "jadard,stylus", &data);
    pjadard_ic_data->JD_STYLUS_EN = (!ret ? (data > 0 ? true : false) : false);

    JD_I("DT:MAX_PT = %d, INT_IS_EDGE = %d, STYLUS_EN = %d\n", pjadard_ic_data->JD_MAX_PT,
        pjadard_ic_data->JD_INT_EDGE, pjadard_ic_data->JD_STYLUS_EN);

    prop = of_find_property(dt, "jadard,panel-sense-nums", NULL);
    if (prop) {
        coords_size = prop->length / sizeof(uint32_t);

        if (coords_size != 2) {
            JD_E("%s:Invalid panel sense number size %d\n", __func__, coords_size);
            return -EINVAL;
        }
    }else{
		printk("%s %d err!\n",__func__,__LINE__);
	}

    if (of_property_read_u32_array(dt, "jadard,panel-sense-nums", coords, coords_size) == 0) {
        pjadard_ic_data->JD_X_NUM = coords[0];
        pjadard_ic_data->JD_Y_NUM = coords[1];
        JD_I("DT:panel-sense-num = %d, %d\n",
            pjadard_ic_data->JD_X_NUM, pjadard_ic_data->JD_Y_NUM);
    }
        pjadard_ic_data->JD_X_NUM = 18;
        pjadard_ic_data->JD_Y_NUM = 32;
    prop = of_find_property(dt, "jadard,panel-coords", NULL);
    if (prop) {
        coords_size = prop->length / sizeof(uint32_t);

        if (coords_size != 4) {
            JD_E("%s:Invalid panel coords size %d\n", __func__, coords_size);
            return -EINVAL;
        }
    }else{
		printk("%s %d err!\n",__func__,__LINE__);
	}

    if (of_property_read_u32_array(dt, "jadard,panel-coords", coords, coords_size) == 0) {
        pdata->abs_x_min = coords[0];
        pdata->abs_x_max = coords[1];
        pdata->abs_y_min = coords[2];
        pdata->abs_y_max = coords[3];
        pjadard_ic_data->JD_X_RES = pdata->abs_x_max;
        pjadard_ic_data->JD_Y_RES = pdata->abs_y_max;

        JD_I("DT:panel-coords = %d, %d, %d, %d\n", pdata->abs_x_min,
            pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
    } else {
        /* Config resolution by tpd, if devicetree was`t set up */
        pdata->abs_x_min = 0;
        pdata->abs_x_max = 720;
        pdata->abs_y_min = 0;
        pdata->abs_y_max = 1612;
        pjadard_ic_data->JD_X_RES = pdata->abs_x_max;
        pjadard_ic_data->JD_Y_RES = pdata->abs_y_max;

        JD_I("TPD:panel-coords = %d, %d, %d, %d\n", pdata->abs_x_min,
            pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
    }

    pdata->gpio_irq = of_get_named_gpio(dt, "jadard,irq-gpio", 0);
    if (!gpio_is_valid(pdata->gpio_irq)) {
        /* Config irq gpio by tpd, if devicetree wasn`t set up */
        pdata->gpio_irq = GTP_INT_PORT;
        JD_I("TPD:gpio_irq value is GTP_INT_PORT\n");
    }

    pdata->gpio_reset = of_get_named_gpio(dt, "jadard,rst-gpio", 0);
    if (!gpio_is_valid(pdata->gpio_reset)) {
        /* Config tp reset gpio by tpd, if devicetree wasn`t set up */
        pdata->gpio_reset = GTP_RST_PORT;
        JD_I("TPD:gpio_rst value is GTP_RST_PORT\n");
    }

    JD_I("DT:gpio_irq = %d, gpio_rst = %d\n", pdata->gpio_irq, pdata->gpio_reset);

    return 0;
}

#ifdef MTK_I2C_DMA
int jadard_bus_read(uint8_t *cmd, uint8_t cmd_len, uint8_t *data, uint32_t data_len, uint8_t toRetry)
{
    int ret = 0;
    s32 retry = 0;
    uint8_t cmd_buf[cmd_len];
    struct i2c_client *client = pjadard_ts_data->client;
    struct i2c_msg msg[] = {
        {
            .addr = (client->addr & I2C_MASK_FLAG),
            .flags = 0,
            .buf = cmd_buf,
            .len = cmd_len,
            .timing = 400
        },
        {
            .addr = (client->addr & I2C_MASK_FLAG),
            .ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
            .flags = I2C_M_RD,
            .buf = gpDMABuf_pa,
            .len = data_len,
            .timing = 400
        },
    };

    mutex_lock(&jd_wr_mux);

    memcpy(cmd_buf, cmd, cmd_len);

    if (data == NULL) {
        mutex_unlock(&jd_wr_mux);
        return -EFAULT;
    }

    for (retry = 0; retry < toRetry; ++retry) {
        ret = i2c_transfer(client->adapter, &msg[0], 2);

        if (ret < 0) {
            continue;
        }

        memcpy(data, gpDMABuf_va, data_len);
        mutex_unlock(&jd_wr_mux);
        return 0;
    }

    JD_E("Dma I2C Read Error: %d byte(s), err-code: %d\n", data_len, ret);

    mutex_unlock(&jd_wr_mux);

    return ret;
}

int jadard_bus_write(uint8_t *cmd, uint8_t cmd_len, uint8_t *data, uint32_t data_len, uint8_t toRetry)
{
    int rc = 0, retry = 0;
    u8 *pWriteData = gpDMABuf_va;
    struct i2c_client *client = pjadard_ts_data->client;
    struct i2c_msg msg[] = {
        {
            .addr = (client->addr & I2C_MASK_FLAG),
            .ext_flag = (client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG),
            .flags = 0,
            .buf = gpDMABuf_pa,
            .len = cmd_len + data_len,
            .timing = 400
        },
    };

    mutex_lock(&jd_wr_mux);

    if (!pWriteData) {
        JD_E("dma_alloc_coherent failed!\n");
        mutex_unlock(&jd_wr_mux);
        return -EFAULT;
    }

    memcpy(pWriteData, cmd, cmd_len);
    memcpy(pWriteData + cmd_len, data, data_len);

    for (retry = 0; retry < toRetry; ++retry) {
        rc = i2c_transfer(client->adapter, &msg[0], 1);

        if (rc < 0) {
            continue;
        }

        mutex_unlock(&jd_wr_mux);
        return 0;
    }

    JD_E("Dma I2C master write Error: %d byte(s), err-code: %d\n", cmd_len + data_len, rc);

    mutex_unlock(&jd_wr_mux);

    return rc;
}

#else
int jadard_bus_read(uint8_t *cmd, uint8_t cmd_len, uint8_t *data, uint32_t data_len, uint8_t toRetry)
{
    int retry;
    struct i2c_client *client = pjadard_ts_data->client;
    struct i2c_msg msg[] = {
        {
            .addr = client->addr,
            .flags = 0,
            .len = cmd_len,
            .buf = cmd,
        },
        {
            .addr = client->addr,
            .flags = I2C_M_RD,
            .len = data_len,
            .buf = data,
        }
    };

    for (retry = 0; retry < toRetry; retry++) {
        if (i2c_transfer(client->adapter, msg, 2) == 2)
            break;

        msleep(20);
    }

    if (retry == toRetry) {
        JD_E("%s: i2c_read_block retry over %d\n",
          __func__, toRetry);
        return -EIO;
    }

    return 0;
}

int jadard_bus_write(uint8_t *cmd, uint8_t cmd_len, uint8_t *data, uint32_t data_len, uint8_t toRetry)
{
    int retry;
    uint8_t *buf = NULL;
    struct i2c_client *client = pjadard_ts_data->client;
    struct i2c_msg msg;


    buf = kzalloc((cmd_len + data_len) * sizeof(uint8_t), GFP_KERNEL);
    if (buf == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        return -ENOMEM;
    }

    memset(&msg, 0, sizeof(struct i2c_msg));
    /* Set I2C packet info. */
    msg.addr = client->addr;
    msg.flags = 0;
    msg.len = cmd_len + data_len;
    msg.buf = buf;

    memcpy(buf, cmd, cmd_len);
    memcpy(buf + cmd_len, data, data_len);

    for (retry = 0; retry < toRetry; retry++) {
        if (i2c_transfer(client->adapter, &msg, 1) == 1)
            break;

        msleep(20);
    }

    if (retry == toRetry) {
        JD_E("%s: i2c_write_block retry over %d\n",
          __func__, toRetry);
		kfree(buf);
        return -EIO;
    }

	kfree(buf);
    return 0;
}
#endif

void jadard_int_enable(bool enable)
{
    int irqnum = pjadard_ts_data->client->irq;
    unsigned long irqflags = 0;

    spin_lock_irqsave(&pjadard_ts_data->irq_active, irqflags);

    if (enable && (pjadard_ts_data->irq_enabled == 0)) {
        enable_irq(irqnum);
        pjadard_ts_data->irq_enabled = 1;
    } else if ((!enable) && (pjadard_ts_data->irq_enabled == 1)) {
        disable_irq_nosync(irqnum);
        pjadard_ts_data->irq_enabled = 0;
    }

    JD_I("irq_enable = %d\n", pjadard_ts_data->irq_enabled);
    spin_unlock_irqrestore(&pjadard_ts_data->irq_active, irqflags);
}

#ifdef JD_RST_PIN_FUNC
void jadard_gpio_set_value(int pin_num, uint8_t value)
{
    tpd_gpio_output(pin_num, value);
}
#endif

int jadard_gpio_power_config(struct jadard_platform_data *pdata)
{
    int error = 0;

    error = regulator_enable(tpd->reg);
    if (error != 0)
        JD_E("Failed to enable reg-vgp6: %d\n", error);

    msleep(100);
#ifdef JD_RST_PIN_FUNC
    tpd_gpio_output(pdata->gpio_reset, 1);
    msleep(20);
    tpd_gpio_output(pdata->gpio_reset, 0);
    msleep(20);
    tpd_gpio_output(pdata->gpio_reset, 1);
    msleep(100);
#endif
    JD_I("mtk_tpd: jadard reset over \n");
    /* set INT mode */
    tpd_gpio_as_int(pdata->gpio_irq);
    return 0;
}

void jadard_gpio_power_deconfig(struct jadard_platform_data *pdata)
{
    int error = 0;

    error = regulator_disable(tpd->reg);

    if (error != 0)
        JD_I("Failed to disable reg-vgp6: %d\n", error);

    regulator_put(tpd->reg);
    JD_I("%s: regulator put, completed.\n", __func__);
}

irqreturn_t jadard_ts_isr_func(int irq, void *ptr)
{
#ifdef JD_ZERO_FLASH
    if (pjadard_ts_data->fw_ready == true) {
        jadard_ts_work((struct jadard_ts_data *)ptr);
    }
#else
    jadard_ts_work((struct jadard_ts_data *)ptr);
#endif

    return IRQ_HANDLED;
}

int jadard_int_register_trigger(void)
{
    int ret = JD_NO_ERR;
    struct jadard_ts_data *ts = pjadard_ts_data;
    struct i2c_client *client = pjadard_ts_data->client;

    if (pjadard_ic_data->JD_INT_EDGE) {
        ret = request_threaded_irq(client->irq, NULL, jadard_ts_isr_func,
                                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT, client->name, ts);
    } else {
        ret = request_threaded_irq(client->irq, NULL, jadard_ts_isr_func,
                                    IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->name, ts);
    }

    return ret;
}

void jadard_int_en_set(bool enable)
{
    struct jadard_ts_data *ts = pjadard_ts_data;

    if (enable) {
        if (jadard_int_register_trigger() == 0) {
            ts->irq_enabled = 1;
        }
    } else {
        jadard_int_enable(false);
        free_irq(ts->client->irq, ts);
    }
}

int jadard_ts_register_interrupt(void)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    struct i2c_client *client = pjadard_ts_data->client;
    struct device_node *node = NULL;
   // u32 ints[2] = {0, 0};
    int ret = 0;
    node = of_find_matching_node(node, touch_of_match);

    if (node) {
       // of_property_read_u32_array(node, "debounce", ints, ARRAY_SIZE(ints));
       // gpio_set_debounce(ints[0], ints[1]);
        client->irq = irq_of_parse_and_map(node, 0);
        ts->jd_irq = (int)client->irq;
        JD_I("Jadard_touch_irq=%d\n", ts->jd_irq);
    } else {
        JD_I("[%s] tpd request_irq can not find touch eint device node!\n", __func__);
    }

    ts->irq_enabled = 0;

    /* Work functon */
    if (client->irq) {
        ret = jadard_int_register_trigger();

        if (ret == 0) {
            ts->irq_enabled = 1;
            JD_I("%s: irq enabled at qpio: %d\n", __func__, client->irq);
        } else {
            JD_E("%s: request_irq failed\n", __func__);
        }
    } else {
        JD_I("%s: client->irq is empty.\n", __func__);
    }

    tpd_load_status = 1;

    return ret;
}

void jadard_ts_free_interrupt(void)
{
    struct jadard_ts_data *ts = pjadard_ts_data;

    free_irq(ts->client->irq, ts);
}

int jadard_common_probe(struct i2c_client *client)
{
    struct jadard_ts_data *ts;
    int ret = 0;
	printk("jadard_common_probe start\n");
#if defined(MTK_I2C_DMA)
    client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
    gpDMABuf_va = (u8 *)dma_alloc_coherent(&client->dev, 4096, (dma_addr_t *)&gpDMABuf_pa, GFP_KERNEL);

    if (!gpDMABuf_va) {
        JD_E("Allocate DMA I2C Buffer failed\n");
        ret = -ENODEV;
        goto err_alloc_MTK_DMA_failed;
    }

    memset(gpDMABuf_va, 0, 4096);
#endif
    /* Check I2C functionality */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        JD_E("%s: i2c check functionality error\n", __func__);
        ret = -ENODEV;
        goto err_check_functionality_failed;
    }

    ts = kzalloc(sizeof(struct jadard_ts_data), GFP_KERNEL);
    if (ts == NULL) {
        JD_E("%s: allocate jadard_ts_data failed\n", __func__);
        ret = -ENOMEM;
        goto err_alloc_data_failed;
    }

    i2c_set_clientdata(client, ts);
    i2c_client_point = client;
    ts->client = client;
    ts->dev = &client->dev;
    ts->spi = NULL;
    pjadard_ts_data = ts;
    spin_lock_init(&ts->irq_active);

    ret = jadard_chip_common_init();

err_alloc_data_failed:
err_check_functionality_failed:
#if defined(MTK_I2C_DMA)

    if (ret) {
        if (gpDMABuf_va) {
            dma_free_coherent(&client->dev, 4096, gpDMABuf_va, (dma_addr_t)gpDMABuf_pa);
            gpDMABuf_va = NULL;
            gpDMABuf_pa = NULL;
        }
    }

err_alloc_MTK_DMA_failed:
#endif
    return ret;
}


void jadard_common_remove(struct i2c_client *client)
{
   // int ret = 0;
    jadard_chip_common_deinit();

    if (gpDMABuf_va) {
        dma_free_coherent(&client->dev, 4096, gpDMABuf_va, (dma_addr_t)gpDMABuf_pa);
        gpDMABuf_va = NULL;
        gpDMABuf_pa = NULL;
    }

    return;
}

static void jadard_common_suspend(struct device *dev)
{
    struct jadard_ts_data *ts = dev_get_drvdata(&i2c_client_point->dev);
    JD_I("%s: enter \n", __func__);
    jadard_chip_common_suspend(ts);
    JD_I("%s: end \n", __func__);
    return ;
}
static void jadard_common_resume(struct device *dev)
{
    struct jadard_ts_data *ts = dev_get_drvdata(&i2c_client_point->dev);
    JD_I("%s: enter \n", __func__);
    jadard_chip_common_resume(ts);
    JD_I("%s: end \n", __func__);
    return ;
}

#if defined(JD_CONFIG_FB)
#ifdef JD_CONFIG_DRM
int jadard_drm_check_dt(struct jadard_ts_data *ts)
{
    struct device_node *dt = ts->dev->of_node;
    struct device_node *node = NULL;
    struct drm_panel *panel = NULL;
    int i = 0;
    int count = 0;

    count = of_count_phandle_with_args(dt, "panel", NULL);
    if (count <= 0) {
        JD_I("%s: find drm_panel count(%d) fail\n", __func__, count);
        return 0;
    }
    JD_I("%s: find drm_panel count(%d)\n", __func__, count);
    for (i = 0; i < count; i++) {
        node = of_parse_phandle(dt, "panel", i);
        JD_I("DRM:node = %p\n", node);
        panel = of_drm_find_panel(node);
        JD_I("DRM:panel = %p\n", panel);

        of_node_put(node);
        if (!IS_ERR(panel)) {
            JD_I("%s: find drm_panel successfully\n", __func__);
            ts->active_panel = panel;
            return 0;
        }
    }

    JD_E("%s: no find drm_panel\n", __func__);

    return 0;
}

int jadard_fb_notifier_callback(struct notifier_block *self,
                            unsigned long event, void *data)
{
    struct drm_panel_notifier *evdata = data;
    int *blank;
    struct jadard_ts_data *ts =
        container_of(self, struct jadard_ts_data, fb_notif);
    JD_I("DRM: %s\n", __func__);

    if (evdata && evdata->data &&
        ((event == DRM_PANEL_EARLY_EVENT_BLANK) || (event == DRM_PANEL_EVENT_BLANK)) &&
        ts != NULL &&
        ts->dev != NULL)
    {
        blank = evdata->data;
        JD_I("DRM event:%lu, blank:%d\n", event, *blank);

        switch (*blank) {
        case DRM_PANEL_BLANK_UNBLANK:
            if (DRM_PANEL_EARLY_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, Skipped\n", event);
            } else if (DRM_PANEL_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, TP_RESUME\n", event);
                jadard_common_resume(ts->dev);
            }
            break;

        case DRM_PANEL_BLANK_POWERDOWN:
            if (DRM_PANEL_EARLY_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, TP_SUSPEND\n", event);
                jadard_common_suspend(ts->dev);
            } else if (DRM_PANEL_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, Skipped\n", event);
            }
            break;
        }
    }

    return 0;
}
#else
#ifdef JD_CONFIG_DRM_MSM
int jadard_fb_notifier_callback(struct notifier_block *self,
                            unsigned long event, void *data)
{
    struct fb_event *evdata = data;
    int *blank;
    struct jadard_ts_data *ts =
        container_of(self, struct jadard_ts_data, fb_notif);
    JD_I("MSM_DRM: %s\n", __func__);

    if (evdata && evdata->data &&
        ((event == MSM_DRM_EARLY_EVENT_BLANK) || (event == MSM_DRM_EVENT_BLANK)) &&
        ts != NULL &&
        ts->dev != NULL)
    {
        blank = evdata->data;
        JD_I("MSM_DRM event:%lu, blank:%d\n", event, *blank);

        switch (*blank) {
        case MSM_DRM_BLANK_UNBLANK:
            if (MSM_DRM_EARLY_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, Skipped\n", event);
            } else if (MSM_DRM_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, TP_RESUME\n", event);
                jadard_common_resume(ts->dev);
            }
            break;

        case MSM_DRM_BLANK_POWERDOWN:
            if (MSM_DRM_EARLY_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, TP_SUSPEND\n", event);
                jadard_common_suspend(ts->dev);
            } else if (MSM_DRM_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, Skipped\n", event);
            }
            break;
        }
    }

    return 0;
}
#else
int jadard_fb_notifier_callback(struct notifier_block *self,
                            unsigned long event, void *data)
{
    struct fb_event *evdata = data;
    int *blank;
    struct jadard_ts_data *ts =
        container_of(self, struct jadard_ts_data, fb_notif);
    JD_I("FB: %s\n", __func__);

    if (evdata && evdata->data &&
        ((event == FB_EARLY_EVENT_BLANK) || (event == FB_EVENT_BLANK)) &&
        ts != NULL &&
        ts->dev != NULL)
    {
        blank = evdata->data;
        JD_I("FB event:%lu, blank:%d\n", event, *blank);

        switch (*blank) {
        case FB_BLANK_UNBLANK:
            if (FB_EARLY_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, Skipped\n", event);
            } else if (FB_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, TP_RESUME\n", event);
                jadard_common_resume(ts->dev);
            }
            break;

        case FB_BLANK_POWERDOWN:
        case FB_BLANK_HSYNC_SUSPEND:
        case FB_BLANK_VSYNC_SUSPEND:
        case FB_BLANK_NORMAL:
            if (FB_EARLY_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, TP_SUSPEND\n", event);
                jadard_common_suspend(ts->dev);
            } else if (FB_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, Skipped\n", event);
            }
            break;
        }
    }

    return 0;
}
#endif /* JD_CONFIG_DRM_MSM */
#endif /* JD_CONFIG_DRM */
#endif /* defined(JD_CONFIG_FB) */

static int jadard_common_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    strlcpy(info->type, TPD_DEVICE, sizeof(info->type));
    return 0;
}

static const struct i2c_device_id jadard_common_ts_id[] = {
    {JADARD_common_NAME, 0 },
    {}
};

static struct i2c_driver tpd_i2c_driver = {
    .probe = jadard_common_probe,
    .remove = jadard_common_remove,
    .detect = jadard_common_detect,
    .driver = {
        .name = JADARD_common_NAME,
        .of_match_table = of_match_ptr(jadard_match_table),
    },
    .id_table = jadard_common_ts_id,
    .address_list = (const unsigned short *) forces,
};

static int jadard_common_local_init(void)
{
    int retval;

    JD_I("I2C Touchscreen Driver local init\n");
    tpd->reg = regulator_get(tpd->tpd_dev, "vtouch");
    retval = regulator_set_voltage(tpd->reg, 2800000, 2800000);

    if (retval != 0) {
        JD_E("Failed to set voltage 2V8: %d\n", retval);
    }

    if (i2c_add_driver(&tpd_i2c_driver) != 0) {
        JD_E("unable to add i2c driver\n");
        return -EFAULT;
    }

    JD_I("end %s, %d\n", __FUNCTION__, __LINE__);
    tpd_type_cap = 1;
    return 0;
}

static struct tpd_driver_t tpd_device_driver = {
    .tpd_device_name = JADARD_common_NAME,
    .tpd_local_init = jadard_common_local_init,
    .suspend = jadard_common_suspend,
    .resume = jadard_common_resume,
#ifdef TPD_HAVE_BUTTON
    .tpd_have_button = 1,
#else
    .tpd_have_button = 0,
#endif
};

#if defined(__JADARD_KMODULE__)
int jadard_common_init(void)
{
    JD_I("I2C Jadard kmodule common touch panel driver init\n");
    tpd_get_dts_info();

    if (tpd_driver_add(&tpd_device_driver) < 0)
        JD_I("Failed to add Driver!\n");
	tpd_device_init();
    return 0;
}

void jadard_common_exit(void)
{
    tpd_driver_remove(&tpd_device_driver);
	tpd_device_exit();
}

#else
static int __init jadard_common_init(void)
{
    JD_I("I2C Jadard common touch panel driver init\n");
    tpd_get_dts_info();

    if (tpd_driver_add(&tpd_device_driver) < 0)
        JD_I("Failed to add Driver!\n");

    #ifdef JD_ENABLE_PROXIMITY_TP_SCREEN_OFF 
	alsps_driver_add(&ps_jadard_init_info);
	#endif
    return 0;
}

static void __exit jadard_common_exit(void)
{
    tpd_driver_remove(&tpd_device_driver);
	
}

module_init(jadard_common_init);
module_exit(jadard_common_exit);

MODULE_DESCRIPTION("Jadard_common driver");
MODULE_LICENSE("GPL");

#endif
