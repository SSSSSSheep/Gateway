CC:=gcc
CFLAGS:=-g -Wall -O0

log:=thirdparty/log/log.h thirdparty/log/log.c
json:=thirdparty/cJSON/cJSON.h thirdparty/cJSON/cJSON.c

app_common:=app/app_common.h app/app_common.c

app_message:=app/app_message.h app/app_message.c

app_mqtt:=app/app_mqtt.h app/app_mqtt.c

app_pool:=app/app_pool.h app/app_pool.c

app_buffer:=app/app_buffer.h app/app_buffer.c

app_device:=app/app_device.h app/app_device.c

app_bt:=app/app_bt.h app/app_bt.c

app_serial:= app/app_serial.c app/app_serial.h

ota_http:= ota/ota_http.h ota/ota_http.c

ota_version:= ota/ota_version.h ota/ota_version.c

log_test: test/log_test.c $(log)
	-$(CC) $(CFLAGS) $^ -o $@ -I thirdparty
	-./$@
	-rm $@

json_test: test/json_test.c $(json) $(log)
	-$(CC) $(CFLAGS) $^ -o $@ -I thirdparty
#	-./$@
#	-rm $@

app_common_test:test/common_test.c $(app_common) $(log) $(json)
	-$(CC) $^ -o $@ -Iapp -Ithirdparty
	-./$@
	-rm $@

app_message_test:test/message_test.c $(app_message) $(app_common) $(log) $(json)
	-$(CC) $(CFLAGS) $^ -o $@ -Iapp -Ithirdparty
	-./$@
	-rm $@

mqtt_test: test/mqtt_test.c
	-$(CC) $^ -o $@ -lpaho-mqtt3c
	-./$@
	-rm $@

app_mqtt_test:test/app_mqtt_test.c $(app_mqtt) $(log) 
	-$(CC) $^ -o $@ -Iapp -Ithirdparty -lpaho-mqtt3c
	-./$@
	-rm $@

app_pool_test:test/app_pool_test.c $(app_pool) $(log)
	-$(CC) $^ -o $@ -Iapp -Ithirdparty -lpthread -lrt
	-./$@
	-rm $@

app_buffer_test:test/app_buffer_test.c $(app_buffer) $(log)
	-$(CC) $^ -o $@ -Iapp -Ithirdparty
	-./$@
	-rm $@

app_device_test: test/app_device_test.c $(app_device) $(app_bt) $(log) $(app_buffer) $(app_message) $(app_common) $(json) $(app_pool) $(app_mqtt) $(app_message) 
	-$(CC) -o $@ $^ -Ithirdparty -Iapp -lpaho-mqtt3c
	-./app_device_test
	-rm app_device_test

ota_http_test: test/ota_http_test.c $(ota_http) $(log)
	-$(CC) -o $@ $^ -Ithirdparty -Iapp -Iota -lcurl
	-./$@
	-rm $@

ota_version_test: test/ota_version_test.c $(ota_version) $(log) $(json) $(ota_http)
	-$(CC) -o $@ $^ -Ithirdparty -Iapp -Iota -lcurl -lcrypto
	-./$@
	-rm $@

app_serial:= app/app_serial.c app/app_serial.h
app_runner:= app/app_runner.c app/app_runner.h
daemon_sub_process:= daemon/daemon_sub_process.c daemon/daemon_sub_process.h
daemon_runner:= daemon/daemon_runner.c daemon/daemon_runner.h
IPATHS := -Ithirdparty -Iapp -Iota -Idaemon
LLIBS := -lpaho-mqtt3c -lcurl -lcrypto
OBJS := $(app_common) $(log) $(json) $(app_message) $(app_mqtt) $(app_buffer) \
		$(app_pool) $(app_device) $(app_bt) $(app_serial) $(app_runner) \
		$(ota_http) $(ota_version) $(daemon_sub_process) $(daemon_runner) 
gateway_test: test/gateway_test.c $(OBJS)
	-$(CC)  $^ -o $@ $(IPATHS) $(LLIBS)
	./$@ daemon

app_bt_test: test/app_bt_test.c $(app_bt) $(app_serial) $(log)
	-$(CC) $^ -o $@ -Iapp -Ithirdparty
	-./$@
	-rm $@

simple_test: test/simple_bt_test.c $(app_bt) $(app_common) $(log) $(json) $(app_serial)
	-$(CC) $^ -o $@ -Iapp -Ithirdparty
	-./$@
	-rm $@

test_at_ack: test/test_at_ack.c $(app_bt) $(app_common) $(log) $(json) $(app_serial) $(app_message) $(app_pool) $(app_mqtt)
	-$(CC) $^ -o $@ -Iapp -Ithirdparty -lpaho-mqtt3c -lpthread -lrt
	-./$@
	-rm $@

test_packet_timeout_retry: test/test_packet_timeout_retry.c $(app_bt) $(app_common) $(log) $(json) $(app_serial) $(app_message) $(app_pool) $(app_mqtt)
	-$(CC) $^ -o $@ -Iapp -Ithirdparty -lpaho-mqtt3c -lpthread -lrt
	-./$@
	-rm $@

test_fsm_parsing: test/test_fsm_parsing.c $(app_bt) $(app_common) $(log) $(json) $(app_serial) $(app_message) $(app_pool) $(app_mqtt)
	-$(CC) $^ -o $@ -Iapp -Ithirdparty -lpaho-mqtt3c -lpthread -lrt
	-./$@
	-rm $@

#	-rm $@
# $^: ?????§Ò?
# $@: ??????
# -I: ??gcc?????????¡¤??
# -L: ??????????????