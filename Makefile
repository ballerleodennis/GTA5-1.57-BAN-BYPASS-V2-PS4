PS4SDK      ?= /opt/ps4-payload-sdk
GOLDHEN_SDK ?= ../GoldHEN_Plugins_SDK
TARGET      := gta_ban_bypass

CFLAGS  := -O2 -Wall -fno-builtin -fPIC -I$(GOLDHEN_SDK)/include -I$(PS4SDK)/include -Isource
LDFLAGS := -nostdlib -Wl,-soname,$(TARGET).prx -L$(PS4SDK)/lib
LIBS    := -lkernel

# Das SDK bringt die Regeln + Linkerscript fuer .prx mit
include $(PS4SDK)/lib/plugin.mk