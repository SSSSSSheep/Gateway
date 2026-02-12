log:=thirdparty/log/log.h thirdparty/log/log.c
json:=thirdparty/cJSON/cJSON.h thirdparty/cJSON/cJSON.c

log_test: test/log_test.c $(log)
	-gcc $^ -o $@ -I thirdparty
	-./$@
	-rm $@

json_test: test/json_test.c $(json) $(log)
	-gcc $^ -o $@ -I thirdparty
	-./$@
	-rm $@

# $^: 依赖列表
# $@: 目标文件
# -I: 给gcc配置包含查看路径