// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include "kd_imgsensor.h"

#ifdef MTK_IMGSENSOR_ISP4_T
#include "../../isp4_t/camera_hw/imgsensor_cfg_table.h"
#elif defined(MTK_IMGSENSOR_ISP4_C)
#include "../../isp4_c/camera_hw/imgsensor_cfg_table.h"
#elif defined(MTK_IMGSENSOR_ISP3_M)
#include "../../isp3_m/camera_hw/imgsensor_cfg_table.h"
#elif defined(MTK_IMGSENSOR_ISP3_Z)
#include "../../isp3_z/camera_hw/imgsensor_cfg_table.h"
#else
#include "../../isp4_c/camera_hw/imgsensor_cfg_table.h"	
#endif


/* Legacy design */
struct IMGSENSOR_HW_POWER_SEQ sensor_power_sequence[] = {
#if defined(S5K3L8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3L8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(GC5035_SUB_MIPI_RAW)
	{
	   SENSOR_DRVNAME_GC5035_SUB_MIPI_RAW,
	   {
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 1},
			{RST, Vol_Low, 10},
	//      {AFVDD, Vol_2800, 5},
			{DOVDD, Vol_1800, 5},
			{DVDD, Vol_1200, 5},
			{AVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
	   },
	},
#endif

#if defined(TMS_MIPI_RAW)
	{
		SENSOR_DRVNAME_TMS_MIPI_RAW,
		{
			{AVDD, Vol_High, 0}, 
			{DVDD, Vol_High, 0}, 
			{DOVDD, Vol_High, 30},
			{RST, Vol_High, 30}
		},
	},
#endif
#if defined(CM60_MIPI_RAW)
	{
		SENSOR_DRVNAME_CM60_MIPI_RAW,
		{
			{AVDD, Vol_High, 0},
			{DVDD, Vol_High, 0},
			{DOVDD, Vol_High, 30},
			{RST, Vol_High, 30}
		},
	},
#endif
#if defined(OV13855_MIPI_RAW)
{
       SENSOR_DRVNAME_OV13855_MIPI_RAW,
       {
               {SensorMCLK, Vol_High, 0},
               {PDN, Vol_Low, 0},
               {RST, Vol_Low, 0},
               {DOVDD, Vol_1800, 0},
               {AVDD, Vol_2800, 0},
               {DVDD, Vol_1200, 0},
               {AFVDD, Vol_2800, 1},
               {PDN, Vol_High, 0},
               {RST, Vol_High, 5}
       },
},
#endif
#if defined(IMX219_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX219_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1000, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV50D40_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV50D40_MIPI_RAW,
		{	
            {PDN, Vol_Low, 0},
            {RST, Vol_Low, 0},
            {AVDD, Vol_2800, 0},
            {AFVDD, Vol_2800, 0},
            {DVDD, Vol_1200, 0},
            {DOVDD, Vol_1800, 1},
            {SensorMCLK, Vol_High, 1},
            {PDN, Vol_High, 0},
            {RST, Vol_High, 8}
		},
	},
#endif	
#if defined(OV64B40_MIPI_RAW)
    {
        SENSOR_DRVNAME_OV64B40_MIPI_RAW,
        {
            {PDN, Vol_Low, 0},
            {RST, Vol_Low, 0},
            {AVDD, Vol_2800, 0},
            {AFVDD, Vol_2800, 0},
            {DVDD, Vol_1200, 0},
            {DOVDD, Vol_1800, 1},
            {SensorMCLK, Vol_High, 1},
            {PDN, Vol_High, 0},
            {RST, Vol_High, 8}
        },
    },
#endif
#if defined(S5K3P8SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P8SP_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
		//	{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif

#if defined(OV8858_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8858_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			//{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
	
#if defined(SC201CS_MIPI_RAW)
       {
               SENSOR_DRVNAME_SC201CS_MIPI_RAW,
               {
					{SensorMCLK, Vol_High, 0},
					{DOVDD, Vol_1800, 0},
					{AVDD, Vol_2800, 0},
					{DVDD, Vol_1200, 0},
				//	{AFVDD, Vol_2800, 5},
					{PDN, Vol_Low, 4},
					{PDN, Vol_High, 0},
					{RST, Vol_Low, 1},
					{RST, Vol_High, 0}
               },
       },
#endif

#if defined(C2599_MIPI_RAW)
       {
               SENSOR_DRVNAME_C2599_MIPI_RAW,
               {         
					{SensorMCLK, Vol_High, 0},			
					{PDN, Vol_High, 1,Vol_Low,1},
					{RST, Vol_Low, 1,Vol_Low, 1},		
					{DVDD, Vol_1800, 1},					
					{DOVDD, Vol_1800, 1,Vol_Low,1},	
					{AVDD, Vol_2800, 1,Vol_Low,1},
					{PDN, Vol_Low, 1,Vol_High,1},
					{RST, Vol_High, 1}

               },
       },
#endif
#if defined(BF2553L_MAIN_MIPI_RAW)
       {
               SENSOR_DRVNAME_BF2553L_MAIN_MIPI_RAW,
               {
                       {PDN, Vol_Low, 0},
                       {RST, Vol_Low, 0},
                       {DOVDD, Vol_1800, 1},
                       {DVDD, Vol_1200, 1},
                       {AFVDD, Vol_1200, 1},
                       {AVDD, Vol_2800, 1},
                       {SensorMCLK, Vol_High, 1},
                       {PDN, Vol_High, 0},
                       {RST, Vol_High, 5}
               },
       },
#endif
#if defined(BF2553L_MIPI_RAW)
       {
               SENSOR_DRVNAME_BF2553L_MIPI_RAW,
               {
                       {PDN, Vol_Low, 0},
                       {RST, Vol_Low, 0},
                       {DOVDD, Vol_1800, 1},
                       {DVDD, Vol_1200, 1},
                       //{AFVDD, Vol_1200, 1},
                       {AVDD, Vol_2800, 1},
                       {SensorMCLK, Vol_High, 1},
                       {PDN, Vol_High, 0},
                       {RST, Vol_High, 5}
               },
       },
#endif
#if defined(S5K3L6_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3L6_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			//{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3L6_MAIN_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3L6_MAIN_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			//{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(HI1336_MIPI_RAW)
       {
               SENSOR_DRVNAME_HI1336_MIPI_RAW,
               {
                       {SensorMCLK, Vol_High, 0},
                       {PDN, Vol_Low, 0},
                       {RST, Vol_Low, 0},
                       {DOVDD, Vol_1800, 0},
                       {AVDD, Vol_2800, 0},
                       {DVDD, Vol_1200, 0},
                       {AFVDD, Vol_2800, 1},
                       {PDN, Vol_High, 0},
                       {RST, Vol_High, 5}
               },
       },
#endif
#if defined(HI843_MIPI_RAW)
        {
                SENSOR_DRVNAME_HI843_MIPI_RAW,
                {
                        {SensorMCLK, Vol_High, 1},
                        {PDN, Vol_Low, 5},
                        {RST, Vol_Low, 5}, 
                        {DOVDD, Vol_1800, 5},   
                        {AVDD, Vol_2800, 5},  
                        {DVDD, Vol_1200, 5},    
                        //{AFVDD, Vol_2800, 5},
            {PDN, Vol_High, 5},
            {RST, Vol_High, 5}
                },
        },
#endif
#if defined(HI556_MIPI_RAW)
       {
               SENSOR_DRVNAME_HI556_MIPI_RAW,
               {
                       {PDN, Vol_Low, 1},
                       {RST, Vol_Low, 1},
                       {DOVDD, Vol_1800, 5},
                       {AVDD, Vol_2800, 5},
                       {DVDD, Vol_1200, 5},  
                       {SensorMCLK, Vol_High, 3},					   
                       {PDN, Vol_High, 5},
                       {RST, Vol_High, 5}
               },
       },
#endif
#if defined(GC8034MAIN_MIPI_RAW)
        {
                SENSOR_DRVNAME_GC8034MAIN_MIPI_RAW,
                {
                        {PDN, Vol_Low, 0},
                        {RST, Vol_Low, 0},
                        {DOVDD, Vol_1800, 1},
                        {DVDD, Vol_1200, 1},
                        {AFVDD, Vol_1200, 1},
                        {AVDD, Vol_2800, 1},
                        {SensorMCLK, Vol_High, 1},
                        {PDN, Vol_High, 0},
                        {RST, Vol_High, 5}
                },
        },
#endif
#if defined(GC8034_MIPI_RAW)
        {
                SENSOR_DRVNAME_GC8034_MIPI_RAW,
                {
                        {PDN, Vol_Low, 0},
                        {RST, Vol_Low, 0},
                        {DOVDD, Vol_1800, 1},
                        {DVDD, Vol_1200, 1},
                        //{AFVDD, Vol_1200, 1},
                        {AVDD, Vol_2800, 1},
                        {SensorMCLK, Vol_High, 1},
                        {PDN, Vol_High, 0},
                        {RST, Vol_High, 5}
                },
        },
#endif
#if defined(S5K4H7_MIPI_RAW)
        {
                SENSOR_DRVNAME_S5K4H7_MIPI_RAW,
                {
                        {SensorMCLK, Vol_High, 0},
                        {PDN, Vol_Low, 0},
                        {RST, Vol_Low, 0},
                        {DOVDD, Vol_1800, 1},
                        {AVDD, Vol_2800, 1},
                        {DVDD, Vol_1200, 1},
                        {PDN, Vol_High, 1},
                        {RST, Vol_High, 0}
                },
        },
#endif	
#if defined(S5K3H7_MIPI_RAW)
       {
               SENSOR_DRVNAME_S5K3H7_MIPI_RAW,
               {
                       {SensorMCLK, Vol_High, 0},
                       {PDN, Vol_Low, 1},
                       {RST, Vol_Low, 1},
                       {DOVDD, Vol_1800, 5},
                       {AVDD, Vol_2800, 5},
                       {DVDD, Vol_1200, 5},
                       {AFVDD, Vol_2800, 5},
                       {PDN, Vol_High, 1},
                       {RST, Vol_High, 1}
               },
       },
#endif
#if defined(GC5035_MIPI_RAW)
        {
                SENSOR_DRVNAME_GC5035_MIPI_RAW,
                {
                        {SensorMCLK, Vol_High, 0},
                        {PDN, Vol_Low, 1},
                        {RST, Vol_Low, 10},
//                      {AFVDD, Vol_2800, 5},
                        {DOVDD, Vol_1800, 5},
                        {DVDD, Vol_1200, 5},
                        {AVDD, Vol_2800, 5},
                        {PDN, Vol_High, 5},
                        {RST, Vol_High, 5}
                },
        },
#endif	
#if defined(GC02M1_MIPI_RAW)
        {
                SENSOR_DRVNAME_GC02M1_MIPI_RAW,
                {
                        {SensorMCLK, Vol_High, 0},
                        {PDN, Vol_Low, 0},
                        {RST, Vol_Low, 0},
                        {DOVDD, Vol_1800, 0},
                        {DVDD, Vol_1200, 0},
                        {AVDD, Vol_2800, 1},
                        //{AFVDD, Vol_2800, 1},
                        {PDN, Vol_High, 0},
                        {RST, Vol_High, 5}
                },
        },
#endif
#if defined(GC030A_MIPI_RAW)
        {
                SENSOR_DRVNAME_GC030A_MIPI_RAW,
                {
                        {SensorMCLK, Vol_High, 0},
                        {PDN, Vol_High, 0},
                        {RST, Vol_High, 0},
                        {DOVDD, Vol_1800, 0},
                        {DVDD, Vol_1800, 5},
                        {AVDD, Vol_2800, 5},
                        // {AFVDD, Vol_2800, 1},
                        {PDN, Vol_Low, 10},
                        {RST, Vol_Low, 10},
                        {PDN, Vol_High, 5},
                        {RST, Vol_High, 5},
                        {PDN, Vol_Low, 10},
                        {RST, Vol_Low, 10}                                     
                },
        },
#endif
#if defined(OV13850_MIPI_RAW)
{
	SENSOR_DRVNAME_OV13850_MIPI_RAW,
	{
		{PDN, Vol_Low, 0},
		{RST, Vol_Low, 1},
		{AVDD, Vol_2800, 0},
		{DOVDD, Vol_1800, 0},
		{DVDD, Vol_1200, 0},
		{AFVDD, Vol_2800, 0},
		{SensorMCLK, Vol_High, 0},
		{PDN, Vol_High, 0},
		{RST, Vol_High, 2}
	},
},
#endif
#if defined(GC05A2_MIPI_RAW)
{
	SENSOR_DRVNAME_GC05A2_MIPI_RAW,
	{
		{SensorMCLK, Vol_High, 0},
		{DOVDD, Vol_1800, 0},
		{DVDD, Vol_1200, 0},
		{AVDD, Vol_2800, 0},
		{PDN, Vol_Low, 4},
		{PDN, Vol_High, 0},
		{RST, Vol_Low, 1},
		{RST, Vol_High, 0}
	},
},
#endif
#if defined(OV5648_MIPI_RAW)
        {
                SENSOR_DRVNAME_OV5648_MIPI_RAW,
                {
                        {SensorMCLK, Vol_High, 0},
                        {PDN, Vol_Low, 5},
                        {RST, Vol_Low, 5},
                        {DOVDD, Vol_1800, 5},
                        {AVDD, Vol_2800, 5},
                        {DVDD, Vol_1200, 5},
                        {PDN, Vol_High, 5},
                        {RST, Vol_High, 5}
                },
        },
#endif

#if defined(GC2385_MIPI_RAW)
       {
               SENSOR_DRVNAME_GC2385_MIPI_RAW,
               {
                       {SensorMCLK, Vol_High, 0},
                       {PDN, Vol_Low, 0},
                       {RST, Vol_Low, 0},
                       {DOVDD, Vol_1800, 0},
                       {AVDD, Vol_2800, 0},
                       {DVDD, Vol_1200, 0},
                       {PDN, Vol_High, 0},
                       {RST, Vol_High, 5}
               },
       },
#endif

#if 0
#if defined(IMX398_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX398_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1},
		},
	},
#endif
#if defined(OV23850_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV23850_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 2},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(IMX386_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX386_MIPI_RAW,
		{
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(IMX386_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX386_MIPI_MONO,
		{
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1100, 0},
			{DOVDD, Vol_1800, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif

#if defined(IMX338_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX338_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2500, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(S5K4E6_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K4E6_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 1},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2900, 0},
			{DVDD, Vol_1200, 2},
			{AFVDD, Vol_2800, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3P8SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P8SP_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(S5K2T7SP_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2T7SP_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(IMX230_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX230_MIPI_RAW,
		{
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{AVDD, Vol_2500, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1100, 0},
			{AFVDD, Vol_2800, 1},
			{SensorMCLK, Vol_High, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 10}
		},
	},
#endif
#if defined(S5K3M2_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3M2_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3P3SX_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3P3SX_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K5E2YA_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K5E2YA_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K4ECGX_MIPI_YUV)
	{
		SENSOR_DRVNAME_S5K4ECGX_MIPI_YUV,
		{
			{DVDD, Vol_1200, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 0},
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV16880_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV16880_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(S5K2P7_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2P7_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1000, 1},
			{DOVDD, Vol_1800, 1},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0},
		},
	},
#endif
#if defined(S5K2P8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2P8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 4},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 1},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX258_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX258_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX258_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX258_MIPI_MONO,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX377_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX377_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV8858_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8858_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 1},
			{AVDD, Vol_2800, 1},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 1},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV8856_MIPI_RAW)
	{SENSOR_DRVNAME_OV8856_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 2},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 5},
		},
	},
#endif
#if defined(S5K2X8_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2X8_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1200, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(IMX214_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX214_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX214_MIPI_MONO)
	{
		SENSOR_DRVNAME_IMX214_MIPI_MONO,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 0},
			{DOVDD, Vol_1800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 1}
		},
	},
#endif
#if defined(IMX362_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX362_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1200, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K2L7_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K2L7_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 3},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX318_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX318_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1200, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(OV8865_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV8865_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(IMX219_MIPI_RAW)
	{
		SENSOR_DRVNAME_IMX219_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{AVDD, Vol_2800, 10},
			{DOVDD, Vol_1800, 10},
			{DVDD, Vol_1000, 10},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_Low, 0},
			{PDN, Vol_High, 0},
			{RST, Vol_Low, 0},
			{RST, Vol_High, 0}
		},
	},
#endif
#if defined(S5K3M3_MIPI_RAW)
	{
		SENSOR_DRVNAME_S5K3M3_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 0},
			{RST, Vol_Low, 0},
			{DOVDD, Vol_1800, 0},
			{AVDD, Vol_2800, 0},
			{DVDD, Vol_1000, 0},
			{AFVDD, Vol_2800, 1},
			{PDN, Vol_High, 0},
			{RST, Vol_High, 2}
		},
	},
#endif
#if defined(OV5670_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV5670_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV5670_MIPI_RAW_2)
	{
		SENSOR_DRVNAME_OV5670_MIPI_RAW_2,
		{
			{SensorMCLK, Vol_High, 0},
			{PDN, Vol_Low, 5},
			{RST, Vol_Low, 5},
			{DOVDD, Vol_1800, 5},
			{AVDD, Vol_2800, 5},
			{DVDD, Vol_1200, 5},
			{AFVDD, Vol_2800, 5},
			{PDN, Vol_High, 5},
			{RST, Vol_High, 5}
		},
	},
#endif
#if defined(OV20880_MIPI_RAW)
	{
		SENSOR_DRVNAME_OV20880_MIPI_RAW,
		{
			{SensorMCLK, Vol_High, 0},
			{RST, Vol_Low, 1},
			{AVDD, Vol_2800, 1},
			{DOVDD, Vol_1800, 1},
			{DVDD, Vol_1100, 1},
			{RST, Vol_High, 5}
		},
	},
#endif
#endif
	/* add new sensor before this line */
	{NULL,},
};

