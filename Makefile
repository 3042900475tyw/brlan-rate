include $(TOPDIR)/rules.mk

PKG_NAME:=brlan-rate
PKG_VERSION:=1.0.0
PKG_RELEASE:=1
PKG_LICENSE:=GPL-2.0

PKG_BUILD_DIR := $(BUILD_DIR)/$(PKG_NAME)

include $(INCLUDE_DIR)/package.mk

define Package/brlan-rate
  SECTION:=utils
  CATEGORY:=Utilities
  TITLE:=Realtime br-lan RX/TX rate monitor
endef

define Package/brlan-rate/description
  Samples br-lan RX/TX byte counters via netlink RTM_GETLINK (IFLA_STATS64)
  and prints realtime throughput in Mbps.
endef

define Build/Prepare
	mkdir -p $(PKG_BUILD_DIR)
	$(CP) ./src/* $(PKG_BUILD_DIR)/
endef

define Build/Compile
	$(TARGET_CC) $(TARGET_CFLAGS) -o $(PKG_BUILD_DIR)/brlan-rate $(PKG_BUILD_DIR)/brlan-rate.c
endef

define Package/brlan-rate/install
	$(INSTALL_DIR) $(1)/usr/bin
	$(INSTALL_BIN) $(PKG_BUILD_DIR)/brlan-rate $(1)/usr/bin/
endef

$(eval $(call BuildPackage,brlan-rate))
