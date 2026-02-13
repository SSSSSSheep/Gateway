CC:=gcc
CFLAGS:=-g -Wall -O0

log:=thirdparty/log/log.h thirdparty/log/log.c
json:=thirdparty/cJSON/cJSON.h thirdparty/cJSON/cJSON.c

app_common:=app/app_common.h app/app_common.c

app_message:=app/app_message.h app/app_message.c

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


# $^: 依赖列表
# $@: 目标文件
# -I: 给gcc配置包含查看路径