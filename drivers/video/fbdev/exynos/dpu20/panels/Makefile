obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HA2K)	+= s6e3ha2k_mipi_lcd.o s6e3ha2k_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HF4)	+= s6e3hf4_mipi_lcd.o s6e3hf4_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HA6)	+= s6e3ha6_mipi_lcd.o s6e3ha6_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3HA8)	+= s6e3ha8_mipi_lcd.o s6e3ha8_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3AA2)	+= s6e3aa2_mipi_lcd.o s6e3aa2_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3FA0)	+= s6e3fa0_mipi_lcd.o s6e3fa0_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3FA7)	+= s6e3fa7_mipi_lcd.o s6e3fa7_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_EA8076)	+= ea8076_mipi_lcd.o ea8076_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_EMUL_DISP) += emul_disp_mipi_lcd.o emul_disp_lcd_ctrl.o

obj-y += dsim_panel.o ../decon_board.o ../decon_notify.o ../decon_abd.o
obj-$(CONFIG_DISPLAY_USE_INFO) += dpui.o
obj-$(CONFIG_EXYNOS_DECON_MDNIE) += mdnie.o

obj-$(and $(CONFIG_EXYNOS_DECON_MDNIE),$(CONFIG_SEC_GPIO_DVS),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_mdnie.o
obj-$(and $(CONFIG_SEC_GPIO_DVS),$(subst undefined,y,$(origin CONFIG_SAMSUNG_PRODUCT_SHIP))) += dd_lcd.o dd_backlight.o dd_dpu.o dd_param.o

obj-$(CONFIG_EXYNOS_DECON_LCD_S6E3FA9_A51)	+= s6e3fa9_a51_lcd_ctrl.o ea8076_a51_lcd_ctrl.o timenval.o
obj-$(and $(CONFIG_EXYNOS_DECON_LCD_S6E3FA9_A51),$(CONFIG_SUPPORT_POC_FLASH)) += ea8076_a51_poc.o
obj-$(CONFIG_EXYNOS_DECON_LCD_NT36672A_XCOVERPRO)	+= nt36672a_xcoverpro_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_HX83102E_GTA4XL)	+= hx83102e_gta4xl_lcd_ctrl.o
obj-$(CONFIG_EXYNOS_DECON_LCD_EA8076_M31)	+= ea8076_m31_lcd_ctrl.o timenval.o
obj-$(CONFIG_EXYNOS_DECON_LCD_EA8076_M31S)	+= ea8076_m31s_lcd_ctrl.o timenval.o