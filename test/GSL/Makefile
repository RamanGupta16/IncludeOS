#################################################
#          IncludeOS SERVICE makefile           #
#################################################

# The name of your service
SERVICE = test_GSL
SERVICE_NAME = C++ Core Guideline Support Library Tests

# Your service parts
FILES = service.cpp

LOCAL_INCLUDES=-I$(PWD)/../../mod/GSL/include -I$(PWD)/../lest/include/lest

# Your disk image
DISK=

# IncludeOS location
ifndef INCLUDEOS_INSTALL
INCLUDEOS_INSTALL=$(HOME)/IncludeOS_install
endif

include $(INCLUDEOS_INSTALL)/Makeseed
