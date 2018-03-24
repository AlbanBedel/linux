#
# Makefile for the Atheros AR71xx built-in ethernet macs
#

ag71xx-y				+= ag71xx_main.o
ag71xx-y				+= ag71xx_ethtool.o
ag71xx-$(CONFIG_AG71XX_DEBUG_FS)	+= ag71xx_debugfs.o

obj-$(CONFIG_AG71XX)			+= ag71xx.o
obj-$(CONFIG_ATH79_MII_CTRL)		+= ath79-mii-ctrl.o
