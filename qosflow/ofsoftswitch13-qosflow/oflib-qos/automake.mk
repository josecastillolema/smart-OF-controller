noinst_LIBRARIES += oflib-qos/liboflib_qos.a

oflib_qos_liboflib_qos_a_SOURCES = \
	oflib-qos/ofl-qos.c \
	oflib-qos/ofl-qos.h 

AM_CPPFLAGS += -DOFL_LOG_VLOG
