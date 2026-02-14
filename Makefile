CC:=gcc
CFLAGS:=-g -Wall -O0

log:=thirdparty/log/log.h thirdparty/log/log.c
json:=thirdparty/cJSON/cJSON.h thirdparty/cJSON/cJSON.c

app_common:=app/app_common.h app/app_common.c

app_message:=app/app_message.h app/app_message.c

app_mqtt:=app/app_mqtt.h app/app_mqtt.c

app_pool:=app/app_pool.h app/app_pool.c

app_buffer:=app/app_buffer.h app/app_buffer.c

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
	-$(CC) $^ -o $@ -Iapp -Ithirdparty
	-./$@
	-rm $@

app_buffer_test:test/app_buffer_test.c $(app_buffer) $(log)
	-$(CC) $^ -o $@ -Iapp -Ithirdparty
	-./$@
	-rm $@


# $^: 依赖列表
# $@: 目标文件
# -I: 给gcc配置包含查看路径
# -L: 指定用到的下载的库