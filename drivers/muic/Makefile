#
# Makefile for muic devices
#

obj-y		+= muic-core.o
obj-$(CONFIG_MUIC_CORE)	+= muic_core.o
obj-$(CONFIG_MUIC_NOTIFIER)		+= muic_notifier.o
obj-$(CONFIG_MUIC_MANAGER)		+= muic_manager.o
obj-$(CONFIG_MUIC_SYSFS)		+= muic_sysfs.o
obj-$(CONFIG_MUIC_S2MU106)		+= s2mu106-muic.o
obj-$(CONFIG_HV_MUIC_S2MU106_AFC)	+= s2mu106-muic-afc.o
obj-$(CONFIG_MUIC_SM5713)	+= sm5713-muic.o sm5713-muic-afc.o
ifeq ($(CONFIG_MUIC_SUPPORT_CCIC), y)
obj-$(CONFIG_MUIC_SM5713)	+= sm5713-muic-ccic.o
endif
